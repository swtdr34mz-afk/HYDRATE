/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
/* HYDRATE's 219 single-cycle waveforms, in the Hydrasynth's 34 groups (Classic, Pulse, Horizon ... SquRoo, Harmon),
 * the group names and counts the instrument's list has. The waves themselves are HYDRATE's own: each one built from
 * a recipe (additive spectra, formants, sync and phase-distortion shapes, folds, Walsh functions ...), then stored as
 * ten band-limited copies (512 harmonics down to 1), so a wave never aliases at any pitch: the oscillator reads the
 * copy whose highest harmonic stays under Nyquist. Built once, on first use (~18 MB). */
#pragma once
#include <cmath>
#include <cstdint>

namespace hy
{
constexpr int NWAVE = 219, TBL = 2048, NMIP = 10;
constexpr int NWGROUP = 34;

struct WaveGroup {
    const char *name;
    int first, count;
};
extern const WaveGroup WGROUP[NWGROUP];
const char *waveName(int w);                       /* "Saw", "Horizon 3", "Harmon 12" */
int waveGroupOf(int w);

class WaveBank
{
public:
    static const WaveBank &get();
    /* the table of wave w at mip m (TBL + 1 values: the last is the first again) */
    const float *table(int w, int m) const { return data + ((size_t)w * NMIP + (size_t)m) * (TBL + 1); }

private:
    WaveBank();
    float *data;
};

/* the mip for a phase increment (cycles a sample): the one whose top harmonic stays under ~0.45 of the rate */
inline int mipFor(float inc)
{
    float maxH = 0.45f / (inc > 1e-6f ? inc : 1e-6f);
    int m = 0, H = 512;
    while (H > maxH && m < NMIP - 1) {
        H >>= 1;
        m++;
    }
    return m;
}
/* a table read at phase x (any real: wrapped), linear */
inline float readTab(const float *t, float x)
{
    x -= std::floor(x);
    float fi = x * (float)TBL;
    int i = (int)fi;
    if (i >= TBL)
        i = TBL - 1;
    float fr = fi - (float)i;
    return t[i] + fr * (t[i + 1] - t[i]);
}
} // namespace hy
