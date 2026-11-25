/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
#include "Waves.h"
#include <algorithm>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace hy
{
static constexpr double HY_PI = 3.14159265358979323846;
const WaveGroup WGROUP[NWGROUP] = {
    {"Classic", 0, 5},    {"Pulse", 5, 6},     {"Horizon", 11, 8},  {"SyncLav", 19, 5},  {"Esquire", 24, 4},
    {"ChriMey", 28, 6},   {"Spect A", 34, 7},  {"Spect X", 41, 7},  {"Klangor", 48, 5},  {"Induct", 53, 3},
    {"Scorpio", 56, 9},   {"Belview", 65, 5},  {"Chendom", 70, 8},  {"Glefan", 78, 7},   {"Sqarbel", 85, 2},
    {"Obob", 87, 3},      {"Ingvay", 90, 3},   {"Particl", 93, 3},  {"Vokz", 96, 6},     {"Flux", 102, 5},
    {"Alweg", 107, 8},    {"Tronic", 115, 6},  {"Duotone", 121, 6}, {"Bobanab", 127, 4}, {"Melotic", 131, 7},
    {"Cluster", 138, 8},  {"Micoten", 146, 5}, {"Orland", 151, 8},  {"Neuton", 159, 7},  {"Xfer", 166, 7},
    {"Resyn", 173, 4},    {"Sano", 177, 4},    {"SquRoo", 181, 15}, {"Harmon", 196, 23}};

static const char *const CLASSIC[5] = {"Sine", "Triangle", "TriSaw", "Saw", "Square"};

int waveGroupOf(int w)
{
    for (int g = NWGROUP - 1; g >= 0; g--)
        if (w >= WGROUP[g].first)
            return g;
    return 0;
}
const char *waveName(int w)
{
    static char names[NWAVE][16];
    static bool made = false;
    if (!made) {
        for (int i = 0; i < NWAVE; i++) {
            int g = waveGroupOf(i);
            if (g == 0)
                std::snprintf(names[i], 16, "%s", CLASSIC[i]);
            else
                std::snprintf(names[i], 16, "%s %d", WGROUP[g].name, i - WGROUP[g].first + 1);
        }
        made = true;
    }
    if (w < 0 || w >= NWAVE)
        return "?";
    return names[w];
}

/* ------------------------------------------------------------------ the FFT --- */
using cd = std::complex<double>;
static void fft(std::vector<cd> &a, bool inv)
{
    size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        double ang = 2.0 * HY_PI / (double)len * (inv ? 1.0 : -1.0);
        cd wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            cd w(1.0);
            for (size_t j = 0; j < len / 2; j++) {
                cd u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

/* ----------------------------------------------------------------- recipes --- */
constexpr int NH = 512;          /* harmonics kept */
constexpr int NT = 4096;         /* a time-domain recipe's resolution */
static constexpr double TWO_PI = 6.283185307179586;

struct Spec {                    /* amplitude and phase of harmonics 1..NH (index 0 unused) */
    double a[NH + 1] = {}, p[NH + 1] = {};
};
static double frac(double x) { return x - std::floor(x); }
static double gauss(double x, double c, double s) { return std::exp(-(x - c) * (x - c) / (2.0 * s * s)); }

/* a time-domain cycle to its spectrum */
static void fromTime(const std::vector<double> &y, Spec &s)
{
    std::vector<cd> a(NT);
    for (int i = 0; i < NT; i++)
        a[(size_t)i] = cd(y[(size_t)i], 0.0);
    fft(a, false);
    for (int h = 1; h <= NH; h++) {
        cd c = a[(size_t)h] * (2.0 / NT);
        s.a[h] = std::abs(c);
        s.p[h] = std::arg(c) + HY_PI / 2.0;   /* as a sine's phase */
    }
}
template <class F> static void timeWave(Spec &s, F f)
{
    std::vector<double> y(NT);
    for (int i = 0; i < NT; i++)
        y[(size_t)i] = f((double)i / NT);
    fromTime(y, s);
}
static void addH(Spec &s, int h, double amp, double ph = 0.0)
{
    if (h >= 1 && h <= NH) {
        s.a[h] += amp;
        s.p[h] = ph;
    }
}
static void sawSpec(Spec &s, double tilt = 1.0)
{
    for (int h = 1; h <= NH; h++)
        s.a[h] = 1.0 / std::pow((double)h, tilt);
}
/* formant peaks over a harmonic series (pitch f0 Hz, peaks fq Hz with widths bw Hz) */
static void formants(Spec &s, double f0, const double *fq, const double *amp, const double *bw, int n, double tilt)
{
    for (int h = 1; h <= NH; h++) {
        double f = h * f0, e = 0.0;
        for (int i = 0; i < n; i++)
            e += amp[i] / (1.0 + ((f - fq[i]) / bw[i]) * ((f - fq[i]) / bw[i]));
        s.a[h] = e / std::pow((double)h, tilt);
    }
}
static double walsh(int w, double x)
{
    int i = (int)(x * 64.0) & 63;
    int bits = 0, gray = w ^ (w >> 1);
    for (int b = 0; b < 6; b++)
        if (((gray >> b) & 1) && ((i >> (5 - b)) & 1))
            bits++;
    return (bits & 1) ? -1.0 : 1.0;
}

static void recipe(int w, Spec &s)
{
    int g = waveGroupOf(w), k = w - WGROUP[g].first;
    uint32_t seed = 0x9E3779B9u ^ (uint32_t)(w * 7919 + 17);
    auto rnd = [&]() {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        return (double)(seed >> 8) / 16777216.0;
    };
    switch (g) {
    case 0:                                                        /* Classic */
        if (k == 0)
            addH(s, 1, 1.0);
        else if (k == 1)
            timeWave(s, [](double x) { return x < 0.25 ? 4 * x : x < 0.75 ? 2 - 4 * x : 4 * x - 4; });
        else if (k == 2)
            timeWave(s, [](double x) { return x < 0.8 ? -1 + 2 * x / 0.8 : 1 - 2 * (x - 0.8) / 0.2; });
        else if (k == 3)
            timeWave(s, [](double x) { return 1.0 - 2.0 * x; });
        else
            timeWave(s, [](double x) { return x < 0.5 ? 1.0 : -1.0; });
        break;
    case 1: {                                                      /* Pulse */
        static const double W[6] = {0.4, 0.3, 0.22, 0.15, 0.09, 0.045};
        double pw = W[k];
        timeWave(s, [pw](double x) { return (x < pw ? 1.0 : -1.0) - (2 * pw - 1); });
        break;
    }
    case 2:                                                        /* Horizon: a smooth hill of harmonics */
        for (int h = 1; h <= NH; h++)
            s.a[h] = gauss(h, 1.0 + k * 1.6, 0.9 + k * 0.55) + (h == 1 ? 0.6 : 0.0) + 0.05 / h;
        break;
    case 3: {                                                      /* SyncLav: synced saws and triangles */
        double r = 1.55 + k * 0.85;
        if (k & 1)
            timeWave(s, [r](double x) {
                double u = frac(x * r);
                return (u < 0.5 ? 4 * u - 1 : 3 - 4 * u) * (1.0 - 0.3 * x);
            });
        else
            timeWave(s, [r](double x) { return 1.0 - 2.0 * frac(x * r); });
        break;
    }
    case 4: {                                                      /* Esquire: drawbar-ish */
        static const int H[6] = {1, 2, 3, 4, 6, 8};
        static const double D[4][6] = {{1, .8, .6, .4, .2, .1}, {1, 0, .7, 0, .5, .3}, {.6, 1, .3, .8, 0, .5}, {1, .5, .9, .2, .7, .6}};
        for (int i = 0; i < 6; i++)
            addH(s, H[i], D[k][i]);
        break;
    }
    case 5:                                                        /* ChriMey: shaped odd / even series with notches */
        for (int h = 1; h <= NH; h++) {
            double a = 1.0 / std::pow((double)h, 0.8 + k * 0.15) * ((h & 1) ? 1.0 : 0.15 + 0.15 * k);
            if (h > 1 && h % (k + 3) == 0)
                a *= 0.05;
            s.a[h] = a;
        }
        break;
    case 6: {                                                      /* Spect A: one band of harmonics */
        double c = std::pow(2.0, 1.0 + k * 0.75);
        for (int h = 1; h <= NH; h++)
            s.a[h] = gauss(h, c, 0.35 * c) + (h == 1 ? 0.25 : 0.0);
        break;
    }
    case 7: {                                                      /* Spect X: two bands */
        double c1 = 2.0 + k, c2 = c1 * (2.2 + 0.35 * k);
        for (int h = 1; h <= NH; h++)
            s.a[h] = gauss(h, c1, 0.3 * c1) + 0.8 * gauss(h, c2, 0.15 * c2) + (h == 1 ? 0.3 : 0.0);
        break;
    }
    case 8: {                                                      /* Klangor: two-operator clang */
        int m = k + 2;
        double I = 1.4 + k * 0.45;
        timeWave(s, [m, I](double x) { return std::sin(TWO_PI * x + I * std::sin(TWO_PI * m * x)); });
        break;
    }
    case 9: {                                                      /* Induct: folded sines */
        double gn = 1.6 + k * 1.3;
        timeWave(s, [gn](double x) { return std::sin(HY_PI * gn * std::sin(TWO_PI * x)); });
        break;
    }
    case 10: {                                                     /* Scorpio: resonant phase distortion */
        double m = 2.0 + k * 0.85;
        int shape = k % 3;
        timeWave(s, [m, shape](double x) {
            double win = shape == 0 ? 1.0 - x : shape == 1 ? (x < 0.5 ? 2 * x : 2 - 2 * x) : 0.5 - 0.5 * std::cos(TWO_PI * x);
            return win * std::sin(TWO_PI * m * x) * (shape == 0 ? 1.0 : 1.2);
        });
        break;
    }
    case 11: {                                                     /* Belview: bell partials on the harmonic grid */
        static const int P[5][6] = {{1, 3, 5, 9, 13, 18}, {1, 2, 5, 7, 11, 16}, {2, 3, 7, 10, 14, 21}, {1, 4, 6, 11, 15, 23}, {1, 3, 4, 8, 12, 19}};
        for (int i = 0; i < 6; i++)
            addH(s, P[k][i], std::pow(0.68, i) * (i == 0 ? 1.0 : 0.9), rnd() * TWO_PI);
        break;
    }
    case 12:                                                       /* Chendom: seeded random spectra */
        for (int h = 1; h <= NH; h++) {
            s.a[h] = (h == 1 ? 1.0 : rnd() * rnd()) / std::pow((double)h, 0.55 + 0.08 * k);
            s.p[h] = rnd() * TWO_PI;
        }
        break;
    case 13: {                                                     /* Glefan: two and three formants */
        static const double F[7][3] = {{400, 1700, 2600}, {650, 1100, 2500}, {300, 2300, 3000}, {500, 900, 2400},
                                       {800, 1250, 2800}, {350, 600, 2700}, {550, 1900, 2600}};
        const double amp[3] = {1.0, 0.6, 0.35}, bw[3] = {90, 120, 170};
        formants(s, 110.0, F[k], amp, bw, 3, 0.35);
        break;
    }
    case 14:                                                       /* Sqarbel: square with a bell */
        for (int h = 1; h <= NH; h += 2)
            s.a[h] = 1.0 / h;
        addH(s, k ? 11 : 7, 0.45);
        addH(s, k ? 17 : 13, 0.25);
        break;
    case 15: {                                                     /* Obob: brassy saws with a resonant bump */
        sawSpec(s);
        double c = 4.0 + 2.0 * k;
        for (int h = 1; h <= NH; h++)
            s.a[h] *= (1.0 + 2.5 * gauss(h, c, 1.2)) / (1.0 + std::pow(h / (c * 2.0), 2.0));
        break;
    }
    case 16: {                                                     /* Ingvay: clipped saw+square */
        double gn = 2.0 * std::pow(2.0, k);
        timeWave(s, [gn](double x) { return std::tanh(gn * ((1 - 2 * x) * 0.6 + (x < 0.5 ? 0.4 : -0.4))); });
        break;
    }
    case 17: {                                                     /* Particl: smoothed noise cycles */
        std::vector<double> y(NT), r(64);
        for (auto &v : r)
            v = rnd() * 2 - 1;
        int sm = 1 + k * 3;
        for (int i = 0; i < NT; i++) {
            double xp = (double)i / NT * 64.0, acc = 0;
            for (int j = -sm; j <= sm; j++) {
                int q = ((int)std::floor(xp) + j + 64) & 63;
                acc += r[(size_t)q];
            }
            y[(size_t)i] = acc / (2 * sm + 1);
        }
        fromTime(y, s);
        break;
    }
    case 18: {                                                     /* Vokz: vowels A E I O U Y */
        static const double F[6][3] = {{800, 1150, 2900}, {400, 2100, 2800}, {280, 2300, 3200}, {450, 800, 2830},
                                       {325, 700, 2530}, {300, 1750, 2400}};
        const double amp[3] = {1.0, 0.7, 0.4}, bw[3] = {80, 90, 120};
        formants(s, 130.0, F[k], amp, bw, 3, 0.2);
        break;
    }
    case 19:                                                       /* Flux: combed harmonics */
        for (int h = 1; h <= NH; h++)
            s.a[h] = (h == 1 || h % (k + 2) == 0) ? 1.0 / std::pow((double)h, 0.9) : 0.0;
        break;
    case 20: {                                                     /* Alweg: Walsh functions */
        static const int W[8] = {1, 3, 5, 6, 9, 11, 13, 22};
        int wi = W[k];
        timeWave(s, [wi](double x) { return walsh(wi, x) * 0.8 + walsh(1, x) * 0.2; });
        break;
    }
    case 21: {                                                     /* Tronic: stepped (sampled) shapes */
        static const int ST[6] = {4, 6, 8, 12, 16, 5};
        int st = ST[k];
        bool saw = k & 1;
        timeWave(s, [st, saw](double x) {
            double q = std::floor(x * st) / st;
            return saw ? 1.0 - 2.0 * q : std::sin(TWO_PI * q);
        });
        break;
    }
    case 22: {                                                     /* Duotone: fundamental + one partial */
        static const int H[6] = {2, 3, 5, 7, 9, 13};
        addH(s, 1, 1.0);
        addH(s, H[k], 0.75);
        addH(s, H[k] * 2, 0.15);
        break;
    }
    case 23: {                                                     /* Bobanab: a resonant ladder over a saw */
        sawSpec(s);
        double c = 3.0 + 3.0 * k, q = 5.0;
        for (int h = 1; h <= NH; h++) {
            double r = h / c;
            double mag = 1.0 / std::sqrt(std::pow(1 - r * r, 2.0) + std::pow(r / q, 2.0));
            s.a[h] *= std::min(mag, 6.0) / (1.0 + std::pow(r, 4.0));
        }
        break;
    }
    case 24: {                                                     /* Melotic: flute, strings, choir, clarinet, oboe, horn, vibe */
        switch (k) {
        case 0: addH(s, 1, 1); addH(s, 2, .25); addH(s, 3, .12); addH(s, 4, .04); break;
        case 1: sawSpec(s, 1.1); for (int h = 1; h <= NH; h++) s.a[h] *= 1 + 0.8 * gauss(h, 6, 2.5); break;
        case 2: { const double F[3] = {600, 1000, 2600}, A[3] = {1, .6, .2}, B[3] = {120, 150, 200}; formants(s, 220, F, A, B, 3, 0.5); break; }
        case 3: for (int h = 1; h <= NH; h += 2) s.a[h] = 1.0 / std::pow(h, 1.3); addH(s, 2, 0.05); break;
        case 4: sawSpec(s, 1.0); for (int h = 1; h <= NH; h++) s.a[h] *= 0.2 + 2.0 * gauss(h, 4, 1.5); break;
        case 5: sawSpec(s, 1.6); for (int h = 1; h <= NH; h++) s.a[h] *= 1 + gauss(h, 3, 1.0); break;
        default: addH(s, 1, 1); addH(s, 4, .45); addH(s, 10, .2); break;
        }
        break;
    }
    case 25:                                                       /* Cluster: a block of neighbours */
        addH(s, 1, 0.7);
        for (int h = 2 + k * 2; h < 2 + k * 2 + 3 + k / 2; h++)
            addH(s, h, 0.6);
        break;
    case 26: {                                                     /* Micoten: primes, squares, Fibonacci ... */
        static const int S[5][8] = {{1, 2, 3, 5, 7, 11, 13, 17}, {1, 4, 9, 16, 25, 36, 0, 0}, {1, 2, 3, 5, 8, 13, 21, 34},
                                    {1, 3, 7, 15, 31, 0, 0, 0}, {1, 6, 11, 16, 21, 26, 31, 0}};
        for (int i = 0; i < 8; i++)
            if (S[k][i])
                addH(s, S[k][i], 1.0 / std::sqrt(i + 1.0));
        break;
    }
    case 27: {                                                     /* Orland: tonewheel registrations */
        static const int H[9] = {1, 3, 2, 4, 6, 8, 10, 12, 16};
        static const int R[8][9] = {{8, 8, 8, 0, 0, 0, 0, 0, 0}, {8, 8, 8, 8, 8, 8, 8, 8, 8}, {8, 3, 8, 0, 0, 0, 0, 0, 3},
                                    {8, 0, 0, 0, 0, 0, 0, 0, 8}, {0, 0, 8, 8, 0, 0, 0, 0, 0}, {6, 8, 8, 6, 0, 0, 0, 0, 0},
                                    {8, 8, 6, 8, 0, 8, 0, 0, 8}, {4, 5, 8, 6, 4, 3, 0, 2, 0}};
        for (int i = 0; i < 9; i++)
            if (R[k][i])
                addH(s, H[i], std::pow(10.0, (R[k][i] - 8) * 3.0 / 20.0));
        break;
    }
    case 28: {                                                     /* Neuton: soft shapes */
        int kk = k;
        timeWave(s, [kk](double x) {
            double sn = std::sin(TWO_PI * x);
            switch (kk) {
            case 0: return sn * std::fabs(sn);
            case 1: return sn * sn * sn;
            case 2: return std::fabs(sn) * 2 - 1.27;
            case 3: return sn > 0 ? sn : 0.0;
            case 4: return sn + 0.3 * std::sin(2 * TWO_PI * x);
            case 5: return std::sin(TWO_PI * x + 0.8 * sn);
            default: return std::sin(TWO_PI * x * x);
            }
        });
        break;
    }
    case 29: {                                                     /* Xfer: modern shapes */
        int kk = k;
        timeWave(s, [kk](double x) {
            double sn = std::sin(TWO_PI * x), sw = 1 - 2 * x;
            switch (kk) {
            case 0: return std::tanh(4 * sn);
            case 1: return std::tanh(3 * sn + 1.5) - std::tanh(1.5);
            case 2: return sw * (x < 0.5 ? 1.0 : 0.4);
            case 3: return std::sin(TWO_PI * std::pow(x, 3.0));
            case 4: return sw + 0.5 * (frac(4 * x) < 0.5 ? 1 : -1);
            case 5: return std::sin(TWO_PI * x + 3 * std::sin(TWO_PI * 0.5 * x));
            default: return std::copysign(std::pow(std::fabs(sw), 0.3), sw);
            }
        });
        break;
    }
    case 30: {                                                     /* Resyn: piano, strings, brass, reed */
        static const double A[4][12] = {{1, .7, .45, .5, .2, .25, .1, .12, .05, .06, .03, .02},
                                        {1, .6, .55, .45, .4, .38, .3, .28, .2, .18, .15, .12},
                                        {.7, .9, 1, .8, .7, .5, .45, .3, .25, .2, .12, .1},
                                        {1, .05, .7, .05, .5, .1, .35, .1, .25, .1, .15, .05}};
        for (int h = 1; h <= 12; h++)
            addH(s, h, A[k][h - 1], (h * 1.3 + k) * 0.7);
        for (int h = 13; h <= NH; h++)
            s.a[h] = A[k][11] * 12.0 / (h * h) * 6.0;
        break;
    }
    case 31: {                                                     /* Sano: sines with a little colour */
        static const double A[4][4] = {{1, .12, 0, 0}, {1, 0, .1, 0}, {1, .2, .1, .05}, {1, .05, .2, 0}};
        for (int h = 1; h <= 4; h++)
            addH(s, h, A[k][h - 1]);
        break;
    }
    case 32: {                                                     /* SquRoo: a square relaxing into a sine (powers of the sine) */
        double p = 0.035 + k * 0.07;
        timeWave(s, [p](double x) {
            double sn = std::sin(TWO_PI * x);
            return std::copysign(std::pow(std::fabs(sn), p), sn);
        });
        break;
    }
    default:                                                       /* Harmon: one pure harmonic */
        addH(s, k + 1, 1.0);
        break;
    }
}

/* ------------------------------------------------------------------- build --- */
const WaveBank &WaveBank::get()
{
    static WaveBank bank;
    return bank;
}

WaveBank::WaveBank()
{
    static_assert(NWAVE == 219, "219 waves");
    data = new float[(size_t)NWAVE * NMIP * (TBL + 1)];
    std::vector<cd> a(TBL);
    std::vector<double> y(TBL);
    for (int w = 0; w < NWAVE; w++) {
        Spec s;
        recipe(w, s);
        double norm = 1.0;
        for (int m = 0; m < NMIP; m++) {
            int top = NH >> m;
            std::fill(a.begin(), a.end(), cd(0.0));
            for (int h = 1; h <= top && h < TBL / 2; h++) {
                /* a sine of amplitude A, phase p: X[h] = A/2 * e^{i(p - pi/2)}, X[N-h] its conjugate */
                cd c = std::polar(s.a[h] * 0.5, s.p[h] - HY_PI / 2.0);
                a[(size_t)h] = c;
                a[(size_t)(TBL - h)] = std::conj(c);
            }
            fft(a, true);
            double pk = 1e-9;
            for (int i = 0; i < TBL; i++) {
                y[(size_t)i] = a[(size_t)i].real();
                pk = std::max(pk, std::fabs(y[(size_t)i]));
            }
            if (m == 0)
                norm = 0.98 / pk;                  /* every mip scaled as the full one: the level holds across the keys */
            float *t = data + ((size_t)w * NMIP + (size_t)m) * (TBL + 1);
            for (int i = 0; i < TBL; i++)
                t[i] = (float)(y[(size_t)i] * norm);
            t[TBL] = t[0];
        }
    }
}
} // namespace hy
