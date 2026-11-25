/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
/* HYDRATE's effects, in the Hydrasynth's order: PRE-FX -> DELAY -> REVERB -> POST-FX, all stereo.
 *   PRE-/POST-FX  Chorus, Flanger, Rotary, Phaser, Lo-Fi, Tremolo, EQ, Compressor, Distort (each with its presets and
 *                 the manual's parameters; Param1/Param2 and Dry/Wet are the mod destinations)
 *   DELAY         Basic Mono, Basic Stereo, Pan Delay, LRC Delay, Reverse; 1 ms .. 3 s or synced; Wet Tone (LP/HP),
 *                 Feed Tone (the repeats' darkening)
 *   REVERB        Hall, Room, Plate, Cloud: an eight-line feedback delay network (Householder mix) with a diffusing
 *                 allpass in each line, input diffusion, modulated lines, three-band decay (Lo/Hi Damp), Freeze */
#pragma once
#include "Dsp.h"
#include "Params.h"
#include <vector>

namespace hy
{
/* what a Pre/Post FX parameter means for each type: its label, and its value from the 0..1 the patch keeps */
const char *fxParamName(int type, int k);           /* "" when the type has no such parameter */
float fxValue(int type, int k, float n);            /* the value in its unit (Hz, dB, ...) */
void fxParamText(int type, int k, float n, char *out, int len);
int fxPresetCount(int type);
const char *fxPresetName(int type, int preset);
/* a preset's template: the six parameters and the dry/wet (-1: keep) */
void fxPreset(int type, int preset, float *p6, float &wet);

/* ------------------------------------------------------------ Pre/Post FX --- */
class FxUnit
{
public:
    void prepare(float sampleRate);
    void clear();
    void set(int type, int preset, const float *p6, float wet, float bpm, double beatPos);
    void process(float *L, float *R, int n);

private:
    float sr = 44100.0f;
    int type = FX_BYPASS, wantType = FX_BYPASS, preset = 0;
    float tp[6] = {}, sp[6] = {}, tWet = 0, sWet = 0, xf = 0;
    float bpm = 120.0f;
    double beat = 0.0;
    bool init = false;
    /* the modulated delay (chorus, flanger, rotary's doppler, tremolo's pitch) */
    std::vector<float> dl[2];
    int dlN = 1, dlW = 0;
    float lfoPh = 0, lfo2Ph = 0, fbL = 0, fbR = 0;
    /* phaser */
    float apS[2][12] = {}, phFb[2] = {};
    /* lo-fi */
    Biquad lf[2][2];
    float holdL = 0, holdR = 0, holdPh = 0;
    int lofiType = -1;
    float lofiCut = -1, lofiRes = -1;
    /* eq */
    Biquad eq[2][3];
    float eqSet[5] = {-99, -99, -99, -99, -99};
    /* compressor */
    float env = 0.0f, gr = 1.0f;
    /* distort */
    Biquad dTone[2];
    float dToneSet = -999.0f;
    /* rotary */
    OnePole rotSplit[2];
    float rotLo = 0, rotHi = 0;

    void clearState();
    void run(float &l, float &r);
    float readDl(int c, float d) const;
};

/* ------------------------------------------------------------------ Delay --- */
class Delay
{
public:
    void prepare(float sampleRate);
    void clear();
    /* time in seconds (already the synced one) */
    void set(int type, float timeS, float fb01, float wetTone, float feedTone, float wet);
    void process(float *L, float *R, int n);
    float tail() const { return tailLvl; }

private:
    float sr = 44100.0f;
    std::vector<float> buf[2];
    int N = 1, w = 0;
    int type = DT_STEREO;
    float tTime = 0.3f, sTime = 0.3f, fb = 0, wet = 0, sWet = 0;
    OnePole ftL, ftR;
    float ftG = 0.5f;
    Svf wtL, wtR;
    float wtG = 0.5f, wtK = 1.4f;
    int wtMode = 0;
    float panPos = 0.0f;
    int lrcStep = 0;
    long lrcCount = 0;
    /* reverse: two read heads crossfading */
    float revPh = 0.0f;
    float tailLvl = 0.0f;
    float rd(int c, float d) const;
};

/* ----------------------------------------------------------------- Reverb --- */
class Reverb
{
public:
    void prepare(float sampleRate);
    void clear();
    void set(int type, float preDelayMs, float timeS, bool freeze, float tone, float hiDamp, float loDamp, float wet);
    void process(float *L, float *R, int n);
    bool idle() const { return wet <= 0.0f && sWet < 1e-4f; }

private:
    static constexpr int NL = 8, NAP = 6;
    float sr = 44100.0f;
    int type = -1;
    std::vector<float> line[NL], lineAp[NL], inAp[NAP], pre;
    int lineLen[NL] = {}, apLen[NL] = {}, inApLen[NAP] = {};
    int lw[NL] = {}, law[NL] = {}, iaw[NAP] = {}, pw = 0, preN = 1;
    float modPh[NL] = {}, modRate[NL] = {}, modDepth = 0.0f;
    float gLo[NL] = {}, gMid[NL] = {}, gHi[NL] = {};
    OnePole sLo[NL], sHi[NL];
    float kLo = 0.1f, kHi = 0.5f;
    float inApG = 0.6f, lineApG = 0.5f, size = 1.0f;
    float preS = 0.0f, tPre = 0.0f;
    float wet = 0.0f, sWet = 0.0f;
    float freezeX = 0.0f;
    bool freeze = false;
    Svf toneL, toneR;
    float toneG = 0.5f;
    int toneMode = 0;
    float last[3] = {-1, -1, -1};
    void setType(int t);
    void gains(float timeS, float hiDamp, float loDamp);
};
} // namespace hy
