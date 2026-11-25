/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
/* HYDRATE's small building blocks: fast math, noise, a polyphase half-band for the voices' 2x, one-poles and a
 * state-variable section (Cytomic's trapezoidal form: no delay-free loop, stable under fast modulation). */
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

namespace hy
{
constexpr float PI = 3.14159265358979f, TAU = 6.28318530717959f;

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
inline float minf(float a, float b) { return a < b ? a : b; }
inline float maxf(float a, float b) { return a > b ? a : b; }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float flushDn(float x) { return std::fabs(x) < 1e-15f ? 0.0f : x; }

/* 2^x, fast (a polynomial, 2e-7 off) */
inline float fexp2(float x)
{
    if (x < -126.0f)
        return 0.0f;
    if (x > 127.0f)
        x = 127.0f;
    float fl = std::floor(x), f = x - fl;
    float p = 1.534580891e-04f;
    p = p * f + 1.339993158e-03f;
    p = p * f + 9.618488972e-03f;
    p = p * f + 5.550328776e-02f;
    p = p * f + 2.402264689e-01f;
    p = p * f + 6.931472057e-01f;
    p = p * f + 1.0f;
    return std::ldexp(p, (int)fl);
}
inline float hzOf(float midi) { return 440.0f * fexp2((midi - 69.0f) * (1.0f / 12.0f)); }

/* tanh, Pade (flat at +-3) */
inline float softT(float x)
{
    if (x > 3.0f)
        return 1.0f;
    if (x < -3.0f)
        return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
/* a cubic soft clip, flat at +-1.5 */
inline float sat(float x)
{
    x = clampf(x, -1.5f, 1.5f);
    return x - 0.148148f * x * x * x;
}
/* sin(2 pi x) for any x, fast (a parabola pair, 0.1% off) */
inline float fsin(float x)
{
    x -= std::floor(x);
    float y = x < 0.5f ? 8.0f * x * (1.0f - 2.0f * x) : -8.0f * (x - 0.5f) * (1.0f - 2.0f * (x - 0.5f));
    return y * (0.775f + 0.225f * std::fabs(y));
}
inline float tanW(float w)                         /* tan(w), w < ~1.4 */
{
    if (w > 1.45f)
        w = 1.45f;
    return std::tan(w);
}
inline uint32_t rndU(uint32_t &s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}
inline float rndF(uint32_t &s) { return (float)(int32_t)rndU(s) * (1.0f / 2147483648.0f); }   /* -1..1 */
inline float rnd01(uint32_t &s) { return (float)(rndU(s) >> 8) * (1.0f / 16777216.0f); }

/* ----------------------------------------------------------- the half-band --- */
/* a polyphase IIR half-band (two paths of four first-order allpasses; de Soras' elliptic design: -91 dB from 24.7 kHz
 * at 88.2, flat to 19.4 kHz) */
static const float HB_A0[4] = {0.0472898531f, 0.3385179585f, 0.6552730688f, 0.8745126132f};
static const float HB_A1[4] = {0.1727806110f, 0.5069776792f, 0.7764899192f, 0.9589457184f};
inline float hbAp4(float x, float *t, const float *a)
{
    float y0 = a[0] * (x - t[1]) + t[0], y1 = a[1] * (y0 - t[2]) + t[1], y2 = a[2] * (y1 - t[3]) + t[2],
          y3 = a[3] * (y2 - t[4]) + t[3];
    t[0] = x;
    t[1] = y0;
    t[2] = y1;
    t[3] = y2;
    t[4] = y3;
    return y3;
}
struct HalfBand {                                   /* two in, one out */
    float d0[5] = {}, d1[5] = {}, dp = 0.0f;
    inline float down(float a, float b)
    {
        float y = 0.5f * (hbAp4(a, d0, HB_A0) + hbAp4(dp, d1, HB_A1));
        dp = b;
        return y;
    }
    void clear() { std::memset(this, 0, sizeof *this); }
    void flush()
    {
        for (int i = 0; i < 5; i++) {
            d0[i] = flushDn(d0[i]);
            d1[i] = flushDn(d1[i]);
        }
        dp = flushDn(dp);
    }
};

/* --------------------------------------------------------- filter sections --- */
struct OnePole {                                    /* TPT one-pole: lp, and hp = x - lp */
    float s = 0.0f;
    inline float lp(float x, float g)               /* g = G / (1 + G), G = tan(pi fc / fs) */
    {
        float v = (x - s) * g, y = v + s;
        s = y + v;
        return y;
    }
};
/* the state-variable filter (trapezoidal): one step gives low, band and high */
struct Svf {
    float ic1 = 0.0f, ic2 = 0.0f;
    float lo = 0, bp = 0, hp = 0;
    inline void step(float x, float g, float k)     /* g = tan(pi fc / fs), k = 1/Q (2 none .. 0 oscillating) */
    {
        float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lo = v2;
        bp = v1;
        hp = x - k * v1 - v2;
    }
    void clear() { ic1 = ic2 = lo = bp = hp = 0.0f; }
    void flush()
    {
        ic1 = flushDn(ic1);
        ic2 = flushDn(ic2);
    }
};
/* RBJ biquad (the effects' EQ and tone stages) */
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline float run(float x)
    {
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void clear() { z1 = z2 = 0.0f; }
    void flush()
    {
        z1 = flushDn(z1);
        z2 = flushDn(z2);
    }
    void set(int type, float fs, float f0, float q, float gainDb)
    {
        f0 = clampf(f0, 10.0f, fs * 0.45f);
        float A = std::pow(10.0f, gainDb / 40.0f), w = TAU * f0 / fs, c = std::cos(w), s = std::sin(w),
              al = s / (2.0f * q), B0, B1, B2, A0, A1, A2;
        switch (type) {
        case 0: B0 = (1 - c) / 2; B1 = 1 - c; B2 = (1 - c) / 2; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; break;  /* lp */
        case 1: B0 = (1 + c) / 2; B1 = -(1 + c); B2 = (1 + c) / 2; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; break; /* hp */
        case 2: B0 = al; B1 = 0; B2 = -al; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; break;                       /* bp */
        case 3:                                                                                                  /* peak */
            B0 = 1 + al * A; B1 = -2 * c; B2 = 1 - al * A; A0 = 1 + al / A; A1 = -2 * c; A2 = 1 - al / A; break;
        case 4: {                                                                                                /* low shelf */
            float sq = 2.0f * std::sqrt(A) * al;
            B0 = A * ((A + 1) - (A - 1) * c + sq); B1 = 2 * A * ((A - 1) - (A + 1) * c); B2 = A * ((A + 1) - (A - 1) * c - sq);
            A0 = (A + 1) + (A - 1) * c + sq; A1 = -2 * ((A - 1) + (A + 1) * c); A2 = (A + 1) + (A - 1) * c - sq;
            break;
        }
        default: {                                                                                               /* high shelf */
            float sq = 2.0f * std::sqrt(A) * al;
            B0 = A * ((A + 1) + (A - 1) * c + sq); B1 = -2 * A * ((A - 1) + (A + 1) * c); B2 = A * ((A + 1) + (A - 1) * c - sq);
            A0 = (A + 1) - (A - 1) * c + sq; A1 = 2 * ((A - 1) - (A + 1) * c); A2 = (A + 1) - (A - 1) * c - sq;
            break;
        }
        }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
};
} // namespace hy
