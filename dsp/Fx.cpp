/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
#include "Fx.h"
#include <algorithm>
#include <cstdio>

namespace hy
{
/* ------------------------------------------------------- the FX parameters --- */
namespace
{
enum FK { FK_NUM, FK_HZ, FK_DB, FK_DEG, FK_CHOICE, FK_MS, FK_RATIO, FK_INT, FK_NONE };
struct FxP {
    const char *name;
    float lo, hi, skew;
    FK kind;
    const char *const *ch;
};
const char *const MONOST[] = {"Mono", "Stereo"};
const char *const LOFIT[] = {"Thru", "PWBass", "Radio", "Tele", "Clean", "Low"};
const char *const TREMS[] = {"Sine", "Square"};
const char *const SIDEC[] = {"Off", "BPM Duck"};
const FxP NONE = {"", 0, 1, 1, FK_NONE, nullptr};
const FxP RATE = {"Rate", 0.02f, 10.0f, 0.4f, FK_HZ, nullptr};
const FxP FXT[NFXTYPE][6] = {
    {NONE, NONE, NONE, NONE, NONE, NONE},
    {RATE, {"Depth", 0, 128, 1, FK_NUM, nullptr}, {"Offset", -180, 180, 1, FK_DEG, nullptr}, {"Feedback", -63, 63, 1, FK_INT, nullptr}, {"Mono/St", 0, 1, 1, FK_CHOICE, MONOST}, NONE},
    {RATE, {"Depth", 0, 128, 1, FK_NUM, nullptr}, {"Offset", -180, 180, 1, FK_DEG, nullptr}, {"Feedback", -63, 63, 1, FK_INT, nullptr}, {"Mono/St", 0, 1, 1, FK_CHOICE, MONOST}, NONE},
    {{"Lo-Speed", 0.02f, 10.0f, 0.4f, FK_HZ, nullptr}, {"Hi-Speed", 0.02f, 10.0f, 0.4f, FK_HZ, nullptr}, {"Lo-Depth", 0, 127, 1, FK_INT, nullptr}, {"Hi-Depth", 0, 127, 1, FK_INT, nullptr}, {"Low/High", -63, 63, 1, FK_INT, nullptr}, NONE},
    {RATE, {"Feedback", -63, 63, 1, FK_NUM, nullptr}, {"Depth", 0, 127, 1, FK_INT, nullptr}, {"Phase", 0, 127, 1, FK_INT, nullptr}, {"Offset", -180, 180, 1, FK_DEG, nullptr}, NONE},
    {{"Cutoff", 160, 20000, 0.3f, FK_HZ, nullptr}, {"Resonance", 1, 12, 1, FK_NUM, nullptr}, {"FiltType", 0, 5, 1, FK_CHOICE, LOFIT}, {"Output", -6, 36, 1, FK_DB, nullptr}, {"Sampling", 2756, 44100, 0.4f, FK_HZ, nullptr}, NONE},
    {RATE, {"Depth", 0, 128, 1, FK_NUM, nullptr}, {"LFOShape", 0, 1, 1, FK_CHOICE, TREMS}, {"Phase", -180, 180, 1, FK_DEG, nullptr}, {"PitchMod", 0, 127, 1, FK_INT, nullptr}, NONE},
    {{"LowGain", -36, 24, 1, FK_DB, nullptr}, {"HighGain", -36, 24, 1, FK_DB, nullptr}, {"MidGain", -36, 24, 1, FK_DB, nullptr}, {"Xover Lo", 32, 2000, 0.35f, FK_HZ, nullptr}, {"Xover Hi", 512, 16000, 0.35f, FK_HZ, nullptr}, NONE},
    {{"Ratio", 1, 20, 0.5f, FK_RATIO, nullptr}, {"Threshld", -64, 0, 1, FK_DB, nullptr}, {"Attack", 1, 400, 0.4f, FK_MS, nullptr}, {"Release", 5, 560, 0.4f, FK_MS, nullptr}, {"Output", 0, 512, 1, FK_INT, nullptr}, {"SideChn", 0, 1, 1, FK_CHOICE, SIDEC}},
    {{"Drive", 0, 128, 1, FK_NUM, nullptr}, {"Tone", -64, 64, 1, FK_NUM, nullptr}, {"Asym", 0, 128, 1, FK_NUM, nullptr}, {"Curve", 0, 128, 1, FK_NUM, nullptr}, {"Output", -36, 24, 1, FK_DB, nullptr}, NONE},
};
float fxNorm(int t, int k, float v)
{
    const FxP &p = FXT[t][k];
    if (p.hi <= p.lo)
        return 0.0f;
    float n = clampf((v - p.lo) / (p.hi - p.lo), 0.0f, 1.0f);
    return p.skew != 1.0f ? std::pow(n, p.skew) : n;
}
} // namespace

const char *fxParamName(int t, int k) { return t >= 0 && t < NFXTYPE && k >= 0 && k < 6 ? FXT[t][k].name : ""; }
float fxValue(int t, int k, float n)
{
    const FxP &p = FXT[t][k];
    n = clampf(n, 0.0f, 1.0f);
    if (p.skew != 1.0f && n > 0.0f)
        n = std::pow(n, 1.0f / p.skew);
    float v = p.lo + (p.hi - p.lo) * n;
    if (p.kind == FK_CHOICE || p.kind == FK_INT)
        v = std::round(v);
    return v;
}
void fxParamText(int t, int k, float n, char *o, int len)
{
    if (t <= 0 || t >= NFXTYPE || FXT[t][k].kind == FK_NONE) {
        std::snprintf(o, (size_t)len, "-");
        return;
    }
    const FxP &p = FXT[t][k];
    float v = fxValue(t, k, n);
    switch (p.kind) {
    case FK_HZ: std::snprintf(o, (size_t)len, v >= 1000.0f ? "%.0f Hz" : v >= 10.0f ? "%.1f Hz" : "%.2f Hz", v); break;
    case FK_DB: std::snprintf(o, (size_t)len, "%+.1f dB", v); break;
    case FK_DEG: std::snprintf(o, (size_t)len, "%+.0f deg", v); break;
    case FK_CHOICE: std::snprintf(o, (size_t)len, "%s", p.ch[(int)v]); break;
    case FK_MS: std::snprintf(o, (size_t)len, "%.0f ms", v); break;
    case FK_RATIO: std::snprintf(o, (size_t)len, "%.1f:1", v); break;
    case FK_INT: std::snprintf(o, (size_t)len, "%+d", (int)v); if (k == 4 && t == FX_COMP) std::snprintf(o, (size_t)len, "%d", (int)v); if ((t == FX_ROTARY && k < 4 && k > 1) || (t == FX_PHASER && k > 1) || t == FX_TREMOLO) std::snprintf(o, (size_t)len, "%d", (int)v); break;
    default: std::snprintf(o, (size_t)len, "%.1f", v); break;
    }
}
int fxPresetCount(int t)
{
    static const int N[NFXTYPE] = {1, 3, 3, 3, 3, 2, 3, 7, 1, 3};
    return t >= 0 && t < NFXTYPE ? N[t] : 1;
}
const char *fxPresetName(int t, int p)
{
    static const char *const EQ[7] = {"Flat", "LowBoost", "Bass Cut", "High Cut", "Smile", "Lo-Fi", "Warm"};
    static const char *const DR[3] = {"Drive 1", "Drive 2", "Drive 3"};
    static const char *const NUM[3] = {"1", "2", "3"};
    if (t == FX_EQ)
        return EQ[clampf((float)p, 0, 6) == p ? p : 0];
    if (t == FX_DISTORT)
        return DR[p < 0 ? 0 : p > 2 ? 2 : p];
    return NUM[p < 0 ? 0 : p > 2 ? 2 : p];
}
void fxPreset(int t, int pr, float *o, float &wet)
{
    /* the templates, in each parameter's unit */
    float v[6] = {0, 0, 0, 0, 0, 0};
    float w = -1.0f;
    switch (t) {
    case FX_CHORUS: {
        static const float T[3][5] = {{0.6f, 48, 90, 0, 1}, {0.35f, 80, 120, 10, 1}, {1.2f, 26, 180, -8, 1}};
        std::copy(T[pr % 3], T[pr % 3] + 5, v); w = 50; break;
    }
    case FX_FLANGER: {
        static const float T[3][5] = {{0.25f, 90, 90, 40, 1}, {0.08f, 128, 180, 56, 1}, {1.5f, 50, 0, -45, 0}};
        std::copy(T[pr % 3], T[pr % 3] + 5, v); w = 50; break;
    }
    case FX_ROTARY: {
        static const float T[3][5] = {{0.8f, 0.9f, 60, 90, 0}, {5.7f, 6.6f, 80, 110, 0}, {0.3f, 0.4f, 40, 70, -10}};
        std::copy(T[pr % 3], T[pr % 3] + 5, v); w = 100; break;
    }
    case FX_PHASER: {
        static const float T[3][5] = {{0.3f, 30, 90, 30, 90}, {0.12f, 50, 110, 20, 180}, {1.8f, -40, 70, 50, 0}};
        std::copy(T[pr % 3], T[pr % 3] + 5, v); w = 50; break;
    }
    case FX_LOFI: {
        static const float T[2][5] = {{6000, 1.5f, 4, 0, 11025}, {2400, 3.0f, 2, 6, 8000}};
        std::copy(T[pr % 2], T[pr % 2] + 5, v); w = 100; break;
    }
    case FX_TREMOLO: {
        static const float T[3][5] = {{4.5f, 64, 0, 0, 0}, {6.0f, 100, 1, 180, 0}, {3.2f, 50, 0, 90, 30}};
        std::copy(T[pr % 3], T[pr % 3] + 5, v); w = 100; break;
    }
    case FX_EQ: {
        static const float T[7][5] = {{0, 0, 0, 200, 4000}, {8, 0, 0, 150, 4000}, {-24, 0, 0, 220, 4000}, {0, -24, 0, 200, 3500},
                                      {6, 6, -4, 300, 3000}, {-20, -24, 6, 500, 2500}, {4, -6, 1, 250, 5000}};
        std::copy(T[pr % 7], T[pr % 7] + 5, v); w = 100; break;
    }
    case FX_COMP: {
        static const float T[6] = {4, -18, 10, 120, 160, 0};
        std::copy(T, T + 6, v); w = 100; break;
    }
    case FX_DISTORT: {
        static const float T[3][5] = {{40, -10, 0, 20, -6}, {80, 0, 30, 60, -10}, {120, 12, 70, 110, -14}};
        std::copy(T[pr % 3], T[pr % 3] + 5, v); w = 100; break;
    }
    default: break;
    }
    for (int k = 0; k < 6; k++)
        o[k] = t > 0 && t < NFXTYPE && FXT[t][k].kind != FK_NONE ? fxNorm(t, k, v[k]) : 0.5f;
    wet = w;
}

/* ================================================================ FxUnit === */
void FxUnit::prepare(float s)
{
    sr = s;
    dlN = 1;
    while (dlN < (int)(sr * 0.06f))
        dlN <<= 1;
    dl[0].assign((size_t)dlN, 0.0f);
    dl[1].assign((size_t)dlN, 0.0f);
    clear();
}
void FxUnit::clear()
{
    clearState();
    init = false;
}
void FxUnit::clearState()
{
    for (auto &b : dl)
        std::fill(b.begin(), b.end(), 0.0f);
    dlW = 0;
    fbL = fbR = 0;
    std::memset(apS, 0, sizeof apS);
    phFb[0] = phFb[1] = 0;
    for (auto &a : lf)
        for (auto &b : a)
            b.clear();
    for (auto &a : eq)
        for (auto &b : a)
            b.clear();
    dTone[0].clear();
    dTone[1].clear();
    env = 0;
    gr = 1;
    holdL = holdR = holdPh = 0;
    rotSplit[0].s = rotSplit[1].s = 0;
}
void FxUnit::set(int t, int pr, const float *p6, float w, float b, double bp)
{
    wantType = t;
    preset = pr;
    for (int k = 0; k < 6; k++)
        tp[k] = p6[k];
    tWet = clampf(w * 0.01f, 0.0f, 1.0f);
    bpm = b < 20.0f ? 120.0f : b;
    beat = bp;
    if (!init) {
        type = wantType;
        for (int k = 0; k < 6; k++)
            sp[k] = tp[k];
        sWet = tWet;
        xf = type != FX_BYPASS ? 1.0f : 0.0f;
        init = true;
    }
}
float FxUnit::readDl(int c, float d) const
{
    float rp = (float)dlW - d;
    while (rp < 0.0f)
        rp += (float)dlN;
    int i = (int)rp;
    float f = rp - (float)i;
    const std::vector<float> &b = dl[c];
    float a = b[(size_t)(i & (dlN - 1))], bb = b[(size_t)((i + 1) & (dlN - 1))];
    return a + (bb - a) * f;
}
void FxUnit::process(float *L, float *R, int n)
{
    const float kp = 1.0f - std::exp(-1.0f / (0.02f * sr)), kx = 1.0f / (0.015f * sr);
    for (int i = 0; i < n; i++) {
        if (wantType != type) {
            xf -= kx;
            if (xf <= 0.0f) {
                xf = 0.0f;
                type = wantType;
                clearState();
            }
        } else if (type != FX_BYPASS && xf < 1.0f)
            xf = minf(1.0f, xf + kx);
        for (int k = 0; k < 6; k++)
            sp[k] += (tp[k] - sp[k]) * kp;
        sWet += (tWet - sWet) * kp;
        if (type == FX_BYPASS || xf <= 0.0f)
            continue;
        float l = L[i], r = R[i], wl = l, wr = r;
        run(wl, wr);
        float m = sWet * xf;
        L[i] = l + (wl - l) * m;
        R[i] = r + (wr - r) * m;
    }
    if (type == FX_LOFI || type == FX_EQ || type == FX_DISTORT)
        for (int c = 0; c < 2; c++) {
            for (auto &b : lf[c]) b.flush();
            for (auto &b : eq[c]) b.flush();
            dTone[c].flush();
        }
}

void FxUnit::run(float &l, float &r)
{
    auto V = [&](int k) { return fxValue(type, k, sp[k]); };
    auto raw = [&](int k) { return sp[k]; };
    switch (type) {
    case FX_CHORUS:
    case FX_FLANGER: {
        bool fl = type == FX_FLANGER;
        float rate = V(0), depth = V(1) / 128.0f, off = V(2) / 360.0f, fb = V(3) / 63.0f * (fl ? 0.92f : 0.55f);
        bool stereo = V(4) > 0.5f;
        lfoPh += rate / sr;
        lfoPh -= std::floor(lfoPh);
        int voices = fl ? 1 : 2 + (preset % 3);
        float base = fl ? (preset == 1 ? 0.0003f : 0.0012f) : (preset == 1 ? 0.011f : preset == 2 ? 0.005f : 0.0075f);
        float dep = fl ? 0.0045f * depth : 0.0055f * depth;
        float inL = l + fbL * fb, inR = r + fbR * fb;
        dl[0][(size_t)dlW] = inL;
        dl[1][(size_t)dlW] = stereo ? inR : 0.5f * (inL + inR);
        float oL = 0, oR = 0;
        for (int v = 0; v < voices; v++) {
            float ph = lfoPh + (float)v / (float)voices;
            float dL = (base + dep * 0.5f * (1.0f + fsin(ph))) * sr, dR = (base + dep * 0.5f * (1.0f + fsin(ph + off))) * sr;
            oL += readDl(0, maxf(dL, 1.0f));
            oR += readDl(stereo ? 1 : 0, maxf(dR, 1.0f));
        }
        oL /= (float)voices;
        oR /= (float)voices;
        fbL = oL;
        fbR = oR;
        dlW = (dlW + 1) & (dlN - 1);
        if (!stereo)
            oL = oR = 0.5f * (oL + oR);
        l = oL;
        r = oR;
        break;
    }
    case FX_ROTARY: {
        float lo = rotSplit[0].lp(l + r, 1.0f - std::exp(-TAU * 800.0f / sr)) * 0.5f, hi = 0.5f * (l + r) - lo;
        float sLo = V(0), sHi = V(1), dLo = V(2) / 127.0f, dHi = V(3) / 127.0f, bal = V(4) / 63.0f;
        /* the horn and the drum come up to speed gradually */
        rotLo += (sLo - rotLo) * (1.0f / (1.8f * sr));
        rotHi += (sHi - rotHi) * (1.0f / (0.6f * sr));
        lfoPh += rotLo / sr;
        lfo2Ph += rotHi / sr;
        lfoPh -= std::floor(lfoPh);
        lfo2Ph -= std::floor(lfo2Ph);
        dl[0][(size_t)dlW] = hi;
        float hs = fsin(lfo2Ph), hc = fsin(lfo2Ph + 0.25f);
        float hL = readDl(0, (0.0012f + 0.0007f * dHi * hs) * sr) * (1.0f - 0.45f * dHi * (0.5f + 0.5f * hc));
        float hR = readDl(0, (0.0012f - 0.0007f * dHi * hs) * sr) * (1.0f - 0.45f * dHi * (0.5f - 0.5f * hc));
        dlW = (dlW + 1) & (dlN - 1);
        float ls = fsin(lfoPh);
        float lL = lo * (1.0f - 0.35f * dLo * (0.5f + 0.5f * ls)), lR = lo * (1.0f - 0.35f * dLo * (0.5f - 0.5f * ls));
        float gl = bal < 0 ? 1.0f : 1.0f - bal, gh = bal > 0 ? 1.0f : 1.0f + bal;
        l = (lL * gl + hL * gh) * 1.3f;
        r = (lR * gl + hR * gh) * 1.3f;
        break;
    }
    case FX_PHASER: {
        float rate = V(0), fb = V(1) / 63.0f * 0.85f, depth = V(2) / 127.0f, ph0 = V(3) / 127.0f, off = V(4) / 360.0f;
        lfoPh += rate / sr;
        lfoPh -= std::floor(lfoPh);
        int stages = preset == 1 ? 8 : preset == 2 ? 12 : 4;
        float in[2] = {l, r}, out[2];
        for (int c = 0; c < 2; c++) {
            float s = 0.5f + 0.5f * fsin(lfoPh + (c ? off : 0.0f));
            float fmin = 80.0f * std::pow(2.0f, ph0 * 5.0f), f = fmin * fexp2(s * depth * 4.5f);
            float g = tanW(PI * minf(f, sr * 0.45f) / sr), a = (g - 1.0f) / (g + 1.0f);
            float x = in[c] + phFb[c] * fb;
            for (int k = 0; k < stages; k++) {
                float y = a * x + apS[c][k];
                apS[c][k] = x - a * y;
                x = y;
            }
            phFb[c] = flushDn(x);
            out[c] = x;
        }
        l = out[0];
        r = out[1];
        break;
    }
    case FX_LOFI: {
        int ft = (int)V(2);
        float cut = V(0), res = V(1), outDb = V(3), smp = V(4);
        if (ft != lofiType || std::fabs(cut - lofiCut) > 1.0f || std::fabs(res - lofiRes) > 0.01f) {
            lofiType = ft;
            lofiCut = cut;
            lofiRes = res;
            float q = 0.5f + (res - 1.0f) * 0.6f;
            for (int c = 0; c < 2; c++) {
                switch (ft) {
                case 1: lf[c][0].set(0, sr, cut * 0.4f, q, 0); lf[c][1].set(1, sr, 60.0f, 0.7f, 0); break;     /* PWBass */
                case 2: lf[c][0].set(2, sr, clampf(cut, 400, 4000), q, 0); lf[c][1].set(1, sr, 300.0f, 0.7f, 0); break;
                case 3: lf[c][0].set(0, sr, minf(cut, 3400.0f), q, 0); lf[c][1].set(1, sr, 320.0f, 0.9f, 0); break;
                case 4: lf[c][0].set(0, sr, cut, q, 0); lf[c][1].set(1, sr, 20.0f, 0.7f, 0); break;
                case 5: lf[c][0].set(0, sr, cut, q, 0); lf[c][1].set(0, sr, cut, 0.7f, 0); break;
                default: lf[c][0].set(0, sr, 20000.0f, 0.7f, 0); lf[c][1].set(1, sr, 10.0f, 0.7f, 0); break;
                }
            }
        }
        float x0 = l, x1 = r;
        if (ft != 0) {
            x0 = lf[0][1].run(lf[0][0].run(x0));
            x1 = lf[1][1].run(lf[1][0].run(x1));
        }
        holdPh += smp / sr;
        if (holdPh >= 1.0f) {
            holdPh -= std::floor(holdPh);
            holdL = x0;
            holdR = x1;
        }
        float gn = std::pow(10.0f, outDb / 20.0f) * 0.5f;
        l = softT(holdL * gn);
        r = softT(holdR * gn);
        break;
    }
    case FX_TREMOLO: {
        float rate = V(0), depth = V(1) / 128.0f, ph = V(3) / 360.0f, pm = V(4) / 127.0f;
        bool sq = V(2) > 0.5f;
        lfoPh += rate / sr;
        lfoPh -= std::floor(lfoPh);
        auto shape = [&](float p) {
            float s = fsin(p);
            return sq ? softT(s * 6.0f) : s;
        };
        float aL = 1.0f - depth * (0.5f + 0.5f * shape(lfoPh)), aR = 1.0f - depth * (0.5f + 0.5f * shape(lfoPh + ph));
        if (pm > 0.0f) {
            dl[0][(size_t)dlW] = l;
            dl[1][(size_t)dlW] = r;
            float d0 = (0.002f + 0.0018f * pm * (0.5f + 0.5f * fsin(lfoPh))) * sr, d1 = (0.002f + 0.0018f * pm * (0.5f + 0.5f * fsin(lfoPh + ph))) * sr;
            l = readDl(0, d0);
            r = readDl(1, d1);
            dlW = (dlW + 1) & (dlN - 1);
        }
        l *= aL;
        r *= aR;
        break;
    }
    case FX_EQ: {
        float lg = V(0), hg = V(1), mg = V(2), xl = V(3), xh = maxf(V(4), V(3) * 1.5f);
        float now[5] = {lg, hg, mg, xl, xh};
        bool ch = false;
        for (int k = 0; k < 5; k++)
            if (std::fabs(now[k] - eqSet[k]) > 0.05f)
                ch = true;
        if (ch) {
            std::copy(now, now + 5, eqSet);
            for (int c = 0; c < 2; c++) {
                eq[c][0].set(4, sr, xl, 0.7f, lg);
                eq[c][1].set(5, sr, xh, 0.7f, hg);
                eq[c][2].set(3, sr, std::sqrt(xl * xh), 0.6f, mg);
            }
        }
        l = eq[0][2].run(eq[0][1].run(eq[0][0].run(l)));
        r = eq[1][2].run(eq[1][1].run(eq[1][0].run(r)));
        break;
    }
    case FX_COMP: {
        float ratio = V(0), th = V(1), at = V(2) * 0.001f, re = V(3) * 0.001f, out = V(4) / 128.0f;
        bool duck = V(5) > 0.5f;
        float g;
        if (duck) {
            double bp = beat - std::floor(beat);
            float pulse = std::exp(-(float)bp * 6.0f * (400.0f / maxf(re * 1000.0f, 20.0f)));
            float amount = (1.0f - 1.0f / ratio) * clampf(-th / 40.0f, 0.0f, 1.0f);
            g = 1.0f - amount * pulse;
            beat += (double)(bpm / 60.0f / sr);
        } else {
            float x = maxf(std::fabs(l), std::fabs(r));
            float c = x > env ? 1.0f - std::exp(-1.0f / (at * sr)) : 1.0f - std::exp(-1.0f / (re * sr));
            env += (x - env) * c;
            float db = 20.0f * std::log10(env + 1e-9f), over = db - th;
            float red = over > 0.0f ? over * (1.0f - 1.0f / ratio) : 0.0f;
            g = std::pow(10.0f, -red / 20.0f);
        }
        gr = g;
        l *= g * out;
        r *= g * out;
        break;
    }
    case FX_DISTORT: {
        float drive = V(0) / 128.0f, tone = V(1), asym = V(2) / 128.0f, curve = V(3) / 128.0f, outDb = V(4);
        if (std::fabs(tone - dToneSet) > 0.1f) {
            dToneSet = tone;
            for (int c = 0; c < 2; c++) {
                if (tone < 0.0f)
                    dTone[c].set(0, sr, 20000.0f * std::pow(0.03f, -tone / 64.0f), 0.7f, 0);
                else if (tone > 0.0f)
                    dTone[c].set(1, sr, 20.0f * std::pow(100.0f, tone / 64.0f), 0.7f, 0);
                else
                    dTone[c].set(0, sr, 20000.0f, 0.5f, 0);
            }
        }
        float pre = std::pow(10.0f, drive * 36.0f / 20.0f), og = std::pow(10.0f, outDb / 20.0f), bias = asym * 0.6f;
        auto shape = [&](float x) {
            x = x * pre + bias;
            float soft = softT(x), hard = clampf(x, -1.0f, 1.0f);
            float fold = std::sin(clampf(x, -6.0f, 6.0f) * 1.3f);
            float y = curve < 0.5f ? lerpf(soft, hard, curve * 2.0f) : lerpf(hard, fold, (curve - 0.5f) * 2.0f);
            return y - softT(bias) * (1.0f - curve * 0.5f);
        };
        l = dTone[0].run(shape(l)) * og;
        r = dTone[1].run(shape(r)) * og;
        break;
    }
    default: break;
    }
    (void)raw;
}

/* ================================================================= Delay === */
void Delay::prepare(float s)
{
    sr = s;
    N = 1;
    while (N < (int)(sr * 3.2f) + 8)
        N <<= 1;
    buf[0].assign((size_t)N, 0.0f);
    buf[1].assign((size_t)N, 0.0f);
    clear();
}
void Delay::clear()
{
    for (auto &b : buf)
        std::fill(b.begin(), b.end(), 0.0f);
    w = 0;
    ftL.s = ftR.s = 0;
    wtL.clear();
    wtR.clear();
    tailLvl = 0;
    revPh = 0;
}
void Delay::set(int t, float timeS, float fb01, float wetTone, float feedTone, float wt)
{
    type = t;
    tTime = clampf(timeS, 0.001f, 3.0f);
    fb = clampf(fb01, 0.0f, 1.02f);
    wet = clampf(wt * 0.01f, 0.0f, 1.0f);
    float fc = 600.0f * std::pow(2.0f, feedTone / 128.0f * 5.0f);
    ftG = 1.0f - std::exp(-TAU * minf(fc, sr * 0.45f) / sr);
    if (wetTone < -0.05f) {
        wtMode = 1;
        wtG = tanW(PI * minf(20000.0f * std::pow(0.015f, -wetTone / 64.0f), sr * 0.45f) / sr);
    } else if (wetTone > 0.05f) {
        wtMode = 2;
        wtG = tanW(PI * (20.0f * std::pow(150.0f, wetTone / 64.0f)) / sr);
    } else
        wtMode = 0;
}
float Delay::rd(int c, float d) const
{
    float rp = (float)w - d;
    while (rp < 0.0f)
        rp += (float)N;
    int i = (int)rp;
    float f = rp - (float)i;
    const auto &b = buf[c];
    return b[(size_t)(i & (N - 1))] + f * (b[(size_t)((i + 1) & (N - 1))] - b[(size_t)(i & (N - 1))]);
}
void Delay::process(float *L, float *R, int n)
{
    const float kt = 1.0f - std::exp(-1.0f / (0.08f * sr)), kw = 1.0f - std::exp(-1.0f / (0.02f * sr));
    float peak = 0.0f;
    for (int i = 0; i < n; i++) {
        sTime += (tTime - sTime) * kt;
        sWet += (wet - sWet) * kw;
        float d = maxf(sTime * sr, 2.0f);
        float xl = L[i], xr = R[i], ol = 0, orr = 0;
        switch (type) {
        case DT_MONO: {
            float y = rd(0, d);
            float f = ftL.lp(y, ftG);
            buf[0][(size_t)w] = softT(0.5f * (xl + xr) + f * fb);
            ol = orr = y;
            break;
        }
        case DT_PAN: {
            float yl = rd(0, d), yr = rd(1, d);
            float fl = ftL.lp(yl, ftG), fr = ftR.lp(yr, ftG);
            buf[0][(size_t)w] = softT(0.5f * (xl + xr) + fr * fb);
            buf[1][(size_t)w] = softT(fl * fb);
            ol = yl;
            orr = yr;
            break;
        }
        case DT_LRC: {
            float y1 = rd(0, d), y2 = rd(0, 2.0f * d < (float)N - 4 ? 2.0f * d : d), y3 = rd(0, 3.0f * d < (float)N - 4 ? 3.0f * d : d);
            float f = ftL.lp(y3, ftG);
            buf[0][(size_t)w] = softT(0.5f * (xl + xr) + f * fb);
            ol = y1 + 0.7f * y3;
            orr = y2 + 0.7f * y3;
            break;
        }
        case DT_REVERSE: {
            /* the input as it came, read backwards a window (the time) at a time, two heads crossfading */
            buf[0][(size_t)w] = softT(xl + ftL.s * fb);
            buf[1][(size_t)w] = softT(xr + ftR.s * fb);
            revPh += 1.0f / d;
            if (revPh >= 1.0f)
                revPh -= 1.0f;
            float p2 = revPh + 0.5f;
            if (p2 >= 1.0f)
                p2 -= 1.0f;
            float a1 = fsin(revPh * 0.5f), a2 = fsin(p2 * 0.5f);
            float r1 = (2.0f * revPh) * d, r2 = (2.0f * p2) * d;
            ol = a1 * rd(0, r1 + 1.0f) + a2 * rd(0, r2 + 1.0f);
            orr = a1 * rd(1, r1 + 1.0f) + a2 * rd(1, r2 + 1.0f);
            ftL.lp(ol, ftG);
            ftR.lp(orr, ftG);
            break;
        }
        default: {                                    /* stereo */
            float yl = rd(0, d), yr = rd(1, d);
            float fl = ftL.lp(yl, ftG), fr = ftR.lp(yr, ftG);
            buf[0][(size_t)w] = softT(xl + fl * fb);
            buf[1][(size_t)w] = softT(xr + fr * fb);
            ol = yl;
            orr = yr;
            break;
        }
        }
        w = (w + 1) & (N - 1);
        if (wtMode == 1) {
            wtL.step(ol, wtG, 1.4f);
            wtR.step(orr, wtG, 1.4f);
            ol = wtL.lo;
            orr = wtR.lo;
        } else if (wtMode == 2) {
            wtL.step(ol, wtG, 1.4f);
            wtR.step(orr, wtG, 1.4f);
            ol = wtL.hp;
            orr = wtR.hp;
        }
        float gd = minf(1.0f, 2.0f * (1.0f - sWet)), gw = minf(1.0f, 2.0f * sWet);
        L[i] = xl * gd + ol * gw;
        R[i] = xr * gd + orr * gw;
        peak = maxf(peak, std::fabs(ol) + std::fabs(orr));
    }
    ftL.s = flushDn(ftL.s);
    ftR.s = flushDn(ftR.s);
    wtL.flush();
    wtR.flush();
    tailLvl = peak;
}

/* ================================================================ Reverb === */
static const float RV_MS[NREVTYPE][8] = {
    {31.7f, 37.1f, 41.3f, 47.9f, 53.3f, 59.9f, 67.1f, 73.7f},    /* Hall */
    {9.3f, 11.7f, 13.9f, 16.1f, 18.7f, 21.1f, 23.9f, 27.3f},     /* Room */
    {13.7f, 17.1f, 19.9f, 23.3f, 27.1f, 29.9f, 33.7f, 37.3f},    /* Plate */
    {61.1f, 73.3f, 83.9f, 97.1f, 109.3f, 127.1f, 139.7f, 151.3f} /* Cloud */
};
static const float RV_INAP_MS[6] = {4.77f, 3.59f, 12.73f, 9.31f, 7.07f, 5.11f};

void Reverb::prepare(float s)
{
    sr = s;
    float k = sr / 1000.0f;
    for (int i = 0; i < NL; i++) {
        line[i].assign((size_t)(160.0f * k * 1.05f + 64.0f), 0.0f);
        lineAp[i].assign((size_t)(160.0f * k * 0.4f + 8.0f), 0.0f);
    }
    for (int i = 0; i < NAP; i++)
        inAp[i].assign((size_t)(RV_INAP_MS[i] * 2.6f * k + 4.0f), 0.0f);
    preN = 1;
    while (preN < (int)(0.26f * sr) + 4)
        preN <<= 1;
    pre.assign((size_t)preN, 0.0f);
    type = -1;
    setType(RT_HALL);
    clear();
}
void Reverb::clear()
{
    for (auto &b : line) std::fill(b.begin(), b.end(), 0.0f);
    for (auto &b : lineAp) std::fill(b.begin(), b.end(), 0.0f);
    for (auto &b : inAp) std::fill(b.begin(), b.end(), 0.0f);
    std::fill(pre.begin(), pre.end(), 0.0f);
    for (int i = 0; i < NL; i++) {
        lw[i] = law[i] = 0;
        sLo[i].s = sHi[i].s = 0;
    }
    for (int i = 0; i < NAP; i++)
        iaw[i] = 0;
    pw = 0;
    toneL.clear();
    toneR.clear();
}
void Reverb::setType(int t)
{
    if (t == type)
        return;
    type = t;
    float k = sr / 1000.0f;
    for (int i = 0; i < NL; i++) {
        lineLen[i] = std::min((int)(RV_MS[t][i] * k), (int)line[i].size() - 48);
        apLen[i] = std::min((int)(RV_MS[t][i] * 0.31f * k), (int)lineAp[i].size() - 2);
        modRate[i] = 0.07f + 0.053f * (float)i;
        lw[i] = law[i] = 0;
    }
    float apScale = t == RT_CLOUD ? 2.5f : t == RT_ROOM ? 0.6f : 1.0f;
    for (int i = 0; i < NAP; i++)
        inApLen[i] = std::min((int)(RV_INAP_MS[i] * apScale * k), (int)inAp[i].size() - 2);
    switch (t) {
    case RT_ROOM: inApG = 0.62f; lineApG = 0.45f; modDepth = 0.00012f; break;
    case RT_PLATE: inApG = 0.75f; lineApG = 0.62f; modDepth = 0.0002f; break;
    case RT_CLOUD: inApG = 0.78f; lineApG = 0.68f; modDepth = 0.0014f; break;
    default: inApG = 0.7f; lineApG = 0.55f; modDepth = 0.00035f; break;
    }
    for (auto &b : line) std::fill(b.begin(), b.end(), 0.0f);
    for (auto &b : lineAp) std::fill(b.begin(), b.end(), 0.0f);
    last[0] = last[1] = last[2] = -1.0f;
}
void Reverb::gains(float T, float hd, float ld)
{
    for (int i = 0; i < NL; i++) {
        float Ls = (float)(lineLen[i] + apLen[i]);
        if (freeze) {
            gMid[i] = 1.0f;
            gHi[i] = 1.0f - 0.004f * hd;
            gLo[i] = 1.0f - 0.004f * ld;
            continue;
        }
        float tHi = T * (1.0f - 0.94f * hd), tLo = T * (1.0f - 0.85f * ld);
        gMid[i] = std::pow(10.0f, -3.0f * Ls / (T * sr));
        gHi[i] = std::pow(10.0f, -3.0f * Ls / (maxf(tHi, 0.05f) * sr));
        gLo[i] = std::pow(10.0f, -3.0f * Ls / (maxf(tLo, 0.05f) * sr));
    }
}
void Reverb::set(int t, float preMs, float timeS, bool fr, float tone, float hiDamp, float loDamp, float w)
{
    setType(t);
    tPre = clampf(preMs * 0.001f * sr, 1.0f, (float)preN - 2.0f);
    wet = clampf(w * 0.01f, 0.0f, 1.0f);
    float hd = clampf(hiDamp / 128.0f, 0, 1), ld = clampf(loDamp / 128.0f, 0, 1);
    float T = clampf(timeS, 0.12f, 90.0f);
    if (fr != freeze || std::fabs(T - last[0]) > 1e-4f || std::fabs(hd - last[1]) > 1e-4f || std::fabs(ld - last[2]) > 1e-4f) {
        freeze = fr;
        gains(T, hd, ld);
        last[0] = T;
        last[1] = hd;
        last[2] = ld;
    }
    kLo = 1.0f - std::exp(-TAU * 220.0f / sr);
    kHi = 1.0f - std::exp(-TAU * 3200.0f / sr);
    if (tone < -0.05f) {
        toneMode = 1;
        toneG = tanW(PI * minf(20000.0f * std::pow(0.02f, -tone / 64.0f), sr * 0.45f) / sr);
    } else if (tone > 0.05f) {
        toneMode = 2;
        toneG = tanW(PI * (20.0f * std::pow(100.0f, tone / 64.0f)) / sr);
    } else
        toneMode = 0;
}
void Reverb::process(float *L, float *R, int n)
{
    const float kw = 1.0f - std::exp(-1.0f / (0.02f * sr)), kpre = 1.0f - std::exp(-1.0f / (0.05f * sr));
    const float inK = freeze ? 0.0f : 1.0f;
    for (int i = 0; i < n; i++) {
        sWet += (wet - sWet) * kw;
        freezeX += (inK - freezeX) * kw;
        preS += (tPre - preS) * kpre;
        float xl = L[i], xr = R[i];
        /* the pre-delay */
        pre[(size_t)pw] = 0.5f * (xl + xr) * freezeX;
        float rp = (float)pw - preS;
        if (rp < 0.0f)
            rp += (float)preN;
        int ri = (int)rp;
        float fr = rp - (float)ri;
        float x = pre[(size_t)(ri & (preN - 1))] * (1.0f - fr) + pre[(size_t)((ri + 1) & (preN - 1))] * fr;
        pw = (pw + 1) & (preN - 1);
        float side = 0.25f * (xl - xr) * freezeX;
        /* input diffusion */
        int nap = type == RT_ROOM ? 4 : NAP;
        for (int a = 0; a < nap; a++) {
            auto &b = inAp[a];
            int len = inApLen[a];
            int rdx = iaw[a] - len;
            if (rdx < 0)
                rdx += (int)b.size();
            float dv = b[(size_t)rdx];
            float v = x + inApG * dv;
            b[(size_t)iaw[a]] = v;
            x = dv - inApG * v;
            iaw[a] = (iaw[a] + 1) % (int)b.size();
        }
        /* the lines' outputs (modulated reads), through their in-line allpass */
        float o[NL], sum = 0.0f;
        for (int l = 0; l < NL; l++) {
            auto &b = line[l];
            int sz = (int)b.size();
            modPh[l] += modRate[l] / sr;
            if (modPh[l] >= 1.0f)
                modPh[l] -= 1.0f;
            float d = (float)lineLen[l] + (modDepth * sr) * (1.0f + fsin(modPh[l])) * 0.5f;
            float r = (float)lw[l] - d;
            while (r < 0.0f)
                r += (float)sz;
            int i0 = (int)r;
            float f = r - (float)i0;
            float y = b[(size_t)(i0 % sz)] * (1.0f - f) + b[(size_t)((i0 + 1) % sz)] * f;
            /* three bands, each its own decay */
            float lo = sLo[l].lp(y, kLo), hiLp = sHi[l].lp(y, kHi);
            y = gLo[l] * lo + gMid[l] * (hiLp - lo) + gHi[l] * (y - hiLp);
            o[l] = y;
            sum += y;
        }
        /* Householder: each line gets its own minus 2/N of all */
        float hh = sum * (2.0f / NL);
        for (int l = 0; l < NL; l++) {
            float in = o[l] - hh + ((l & 1) ? x + side : x - side) * 0.5f;
            /* the line's allpass, then into the line */
            auto &ab = lineAp[l];
            int asz = (int)ab.size();
            int ar = law[l] - apLen[l];
            if (ar < 0)
                ar += asz;
            float dv = ab[(size_t)ar];
            float v = in + lineApG * dv;
            ab[(size_t)law[l]] = flushDn(v);
            float y = dv - lineApG * v;
            law[l] = (law[l] + 1) % asz;
            line[l][(size_t)lw[l]] = flushDn(y);
            lw[l] = (lw[l] + 1) % (int)line[l].size();
        }
        float wl = (o[0] - o[2] + o[4] - o[6] + 0.6f * (o[1] - o[5])) * 0.42f;
        float wr = (o[1] - o[3] + o[5] - o[7] + 0.6f * (o[2] - o[6])) * 0.42f;
        if (toneMode == 1) {
            toneL.step(wl, toneG, 1.3f);
            toneR.step(wr, toneG, 1.3f);
            wl = toneL.lo;
            wr = toneR.lo;
        } else if (toneMode == 2) {
            toneL.step(wl, toneG, 1.3f);
            toneR.step(wr, toneG, 1.3f);
            wl = toneL.hp;
            wr = toneR.hp;
        }
        float gd = minf(1.0f, 2.0f * (1.0f - sWet)), gw = minf(1.0f, 2.0f * sWet);
        L[i] = xl * gd + wl * gw;
        R[i] = xr * gd + wr * gw;
    }
    for (int l = 0; l < NL; l++) {
        sLo[l].s = flushDn(sLo[l].s);
        sHi[l].s = flushDn(sHi[l].s);
    }
    toneL.flush();
    toneR.flush();
}
} // namespace hy
