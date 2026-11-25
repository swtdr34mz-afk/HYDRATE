/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
#include "Presets.h"
#include "Fx.h"
#include "Waves.h"
#include <cstdio>
#include <cstring>
#include <functional>
#include <vector>

namespace hy
{
int findDest(const char *g, const char *n)
{
    const auto &D = dests();
    for (size_t i = 1; i < D.size(); i++)
        if (!std::strcmp(D[i].group, g) && !std::strcmp(D[i].name, n))
            return (int)i;
    return 0;
}
static int W(const char *name)
{
    for (int i = 0; i < NWAVE; i++)
        if (!std::strcmp(waveName(i), name))
            return i;
    return 3;
}

namespace
{
struct Maker {
    Patch p;
    int slot = 0;
    Maker(const char *name, const char *cat)
    {
        patchInit(p);
        std::snprintf(p.name, NAME_LEN, "%s", name);
        std::snprintf(p.category, sizeof p.category, "%s", cat);
    }
    Maker &set(int prm, float v) { p.v[prm] = v; return *this; }
    Maker &wave(int o, const char *w) { p.v[pOsc(o, OS_WAVE)] = (float)W(w); return *this; }
    Maker &scan(int o, std::initializer_list<const char *> ws, float pos)
    {
        p.v[pOsc(o, OS_MODE)] = OM_SCAN;
        int i = 0;
        for (auto w : ws)
            p.v[pOsc(o, OS_WAV1 + i++)] = std::strcmp(w, "-") ? (float)W(w) : -1.0f;
        for (; i < 8; i++)
            p.v[pOsc(o, OS_WAV1 + i)] = -2.0f;
        p.v[pOsc(o, OS_SCAN)] = pos;
        return *this;
    }
    Maker &mut(int m, int mode, float ratio, float depth, float wet, float fb = 0.0f)
    {
        p.v[pMut(m, MT_MODE)] = (float)mode;
        p.v[pMut(m, MT_RATIO)] = ratio;
        p.v[pMut(m, MT_DEPTH)] = depth;
        p.v[pMut(m, MT_WET)] = wet;
        p.v[pMut(m, MT_FB)] = fb;
        return *this;
    }
    Maker &mix(int s, float vol, float pan = 0.0f, float filt = 0.0f)
    {
        p.v[P_MX_VOL1 + s] = vol;
        p.v[P_MX_PAN1 + s] = pan;
        p.v[P_MX_FR1 + s] = filt;
        return *this;
    }
    Maker &f1(int type, float cut, float res, float env = 0.0f, float kt = 0.0f)
    {
        p.v[P_F1_TYPE] = (float)type;
        p.v[P_F1_CUT] = cut;
        p.v[P_F1_RES] = res;
        p.v[P_F1_ENV] = env;
        p.v[P_F1_KT] = kt;
        return *this;
    }
    Maker &env(int e, float a, float d, float s, float r)
    {
        p.v[pEnv(e, EN_ATK)] = a;
        p.v[pEnv(e, EN_DEC)] = d;
        p.v[pEnv(e, EN_SUS)] = s;
        p.v[pEnv(e, EN_REL)] = r;
        return *this;
    }
    Maker &lfo(int l, int wave, float rate, float level = 128.0f, int trig = TS_POLY)
    {
        p.v[pLfo(l, LF_WAVE)] = (float)wave;
        p.v[pLfo(l, LF_RATE)] = rate;
        p.v[pLfo(l, LF_LEVEL)] = level;
        p.v[pLfo(l, LF_TRIG)] = (float)trig;
        return *this;
    }
    Maker &mod(int src, const char *g, const char *n, float depth)
    {
        if (slot < NMOD) {
            p.v[pMod(slot, MM_SRC)] = (float)src;
            p.v[pMod(slot, MM_DST)] = (float)findDest(g, n);
            p.v[pMod(slot, MM_DEPTH)] = depth;
            slot++;
        }
        return *this;
    }
    Maker &macro(int m, int name, std::initializer_list<std::pair<int, float>> d)
    {
        p.v[P_MC_NAME1 + m] = (float)name;
        int j = 0;
        for (auto &x : d) {
            p.v[pMacroDst(m, j, MR_DST)] = (float)x.first;
            p.v[pMacroDst(m, j, MR_DEPTH)] = x.second;
            p.v[pMacroDst(m, j, MR_BTN)] = x.second;
            j++;
        }
        return *this;
    }
    Maker &fx(int post, int type, int preset, float wet = -1.0f)
    {
        float p6[6], w;
        fxPreset(type, preset, p6, w);
        p.v[pFx(post, FX_TYPE)] = (float)type;
        p.v[pFx(post, FX_PRESET)] = (float)preset;
        for (int k = 0; k < 6; k++)
            p.v[pFx(post, FX_P1 + k)] = p6[k];
        p.v[pFx(post, FX_WET)] = wet >= 0.0f ? wet : (w >= 0.0f ? w : 50.0f);
        return *this;
    }
    Maker &delay(int type, float ms, float fb, float wet, int sync = -1)
    {
        p.v[P_DL_TYPE] = (float)type;
        p.v[P_DL_TIME] = ms;
        p.v[P_DL_FB] = fb;
        p.v[P_DL_WET] = wet;
        if (sync >= 0) {
            p.v[P_DL_BPM] = 1;
            p.v[P_DL_SYNC] = (float)sync;
        }
        return *this;
    }
    Maker &reverb(int type, float time, float wet, float pre = 12.0f, float hd = 48.0f)
    {
        p.v[P_RV_TYPE] = (float)type;
        p.v[P_RV_TIME] = time;
        p.v[P_RV_WET] = wet;
        p.v[P_RV_PRE] = pre;
        p.v[P_RV_HIDAMP] = hd;
        return *this;
    }
    Maker &steps(int l, std::initializer_list<float> s)
    {
        int i = 0;
        for (float x : s)
            p.step[l][i++] = x;
        p.v[pLfo(l, LF_STEPS)] = (float)i;
        p.v[pLfo(l, LF_WAVE)] = LW_STEP;
        return *this;
    }
};
int D(const char *g, const char *n) { return findDest(g, n); }

std::vector<Patch> build()
{
    std::vector<Patch> v;
    auto add = [&](Maker &m) { v.push_back(m.p); };
    /* macro names (MACRO_NAME): 1 Cutoff 2 Reso 8 Wavescan 9 WavStack 10 FM 12 Warp 13 Harmonic 18 Drive 21 Filter
     * 26 Width 33 Chorus 41 Delay 42 Reverb 44 Freeze 56 Wobble 58 Shimmer */
    {
        Maker m("Liquid Glass", "Pad");
        m.scan(0, {"Horizon 2", "Spect A 3", "Glefan 4", "Horizon 6", "Vokz 3"}, 1.0f)
            .wave(1, "Saw").set(pOsc(1, OS_CENT), 7).mut(0, MU_STACK, 1, 30, 55).mut(2, MU_PWSQZ, 1, 34, 70)
            .mix(0, 112, -20).mix(1, 84, 20).f1(F1_FAT24, 72, 30, 18, 40)
            .env(0, 1.4f, 2.5f, 60, 2.2f).env(1, 1.2f, 2.0f, 110, 2.6f)
            .lfo(2, LW_TRI, 0.11f).mod(SRC_LFO1 + 2, "Osc 1", "WaveScan", 128).lfo(3, LW_SINE, 0.23f)
            .mod(SRC_LFO1 + 3, "Mutant 3", "Depth", 40).mod(SRC_MODWHL, "Filter 1", "Cutoff", 40)
            .set(P_VO_WIDTH, 100).set(P_VO_AFEEL, 18).set(P_VO_WARM, 1).fx(0, FX_CHORUS, 1, 40).reverb(RT_HALL, 6.5f, 38, 30)
            .delay(DT_PAN, 420, 52, 18)
            .macro(0, 8, {{D("Osc 1", "WaveScan"), 128}}).macro(1, 1, {{D("Filter 1", "Cutoff"), 64}})
            .macro(2, 9, {{D("Mutant 1", "Depth"), 90}}).macro(3, 42, {{D("Reverb", "Dry/Wet"), 70}});
        add(m);
    }
    {
        Maker m("Seven Heads", "Lead");
        m.wave(0, "Saw").wave(1, "Saw").set(pOsc(1, OS_SEMI), 12).mut(0, MU_STACK, 1, 54, 100).mut(2, MU_STACK, 1, 42, 100)
            .mix(0, 120).mix(1, 70).f1(F1_LDR24, 92, 22, 18, 50).env(0, 0, 0.9f, 40, 0.4f).env(1, 0.005f, 0.4f, 118, 0.45f)
            .set(P_VO_POLY, PO_MONO).set(P_VO_GLIDE, GL_GLIDE).set(P_VO_GTIME, 30).set(P_VO_GLEGATO, 1)
            .set(P_VO_VIBAMT, 0.5f).delay(DT_STEREO, 380, 50, 24, 12).reverb(RT_PLATE, 2.2f, 22)
            .macro(0, 9, {{D("Mutant 1", "Depth"), 70}, {D("Mutant 3", "Depth"), 70}}).macro(1, 1, {{D("Filter 1", "Cutoff"), 50}});
        add(m);
    }
    {
        Maker m("Glass Clockwork", "Bell");
        m.wave(0, "Sine").mut(0, MU_FM, 3.5f, 64, 100).wave(1, "Sine").set(pOsc(1, OS_SEMI), 12)
            .mut(2, MU_FM, 7.0f, 30, 100).mix(0, 110).mix(1, 46).f1(F1_1POLE, 128, 0)
            .env(1, 0.0f, 3.5f, 0, 2.8f).env(2, 0.0f, 1.6f, 10, 1.6f)
            .mod(SRC_ENV1 + 2, "Mutant 1", "Depth", 70).mod(SRC_ENV1 + 2, "Mutant 3", "Depth", 50).mod(SRC_VELON, "Mutant 1", "Depth", 30)
            .reverb(RT_PLATE, 4.0f, 34, 20, 30).delay(DT_PAN, 330, 40, 16)
            .macro(0, 10, {{D("Mutant 1", "Depth"), 80}}).macro(1, 11, {{D("Mutant 1", "Ratio"), 40}});
        add(m);
    }
    {
        Maker m("Sync Serpent", "Lead");
        m.wave(0, "Saw").mut(0, MU_SYNC, 1, 30, 100).mut(1, MU_STACK, 1, 25, 50).wave(1, "Square").set(pOsc(1, OS_SEMI), -12)
            .mix(0, 120).mix(1, 50).f1(F1_FAT24, 100, 20, 10, 40).env(2, 0.0f, 0.7f, 20, 0.5f).env(1, 0.002f, 0.3f, 120, 0.3f)
            .mod(SRC_ENV1 + 2, "Mutant 1", "Depth", 90).mod(SRC_MODWHL, "Mutant 1", "Window", 80)
            .set(P_VO_POLY, PO_MONO).set(P_VO_GLIDE, GL_GLIDE).set(P_VO_GTIME, 26).set(P_VO_VIBAMT, 0.4f)
            .delay(DT_STEREO, 300, 45, 22, 12).reverb(RT_ROOM, 1.4f, 16)
            .macro(0, 15, {{D("Mutant 1", "Depth"), 128}}).macro(1, 1, {{D("Filter 1", "Cutoff"), 60}});
        add(m);
    }
    {
        Maker m("Velvet Squeeze", "Strings");
        m.wave(0, "Square").mut(0, MU_PWSQZ, 1, 40, 100).wave(1, "Saw").mut(2, MU_PWORIG, 1, 30, 60).set(pOsc(1, OS_CENT), -9)
            .mix(0, 104, -30).mix(1, 90, 30).f1(F1_LDR12, 74, 10, 16, 40).env(0, 0.6f, 1.5f, 70, 1.2f)
            .env(1, 0.45f, 1.0f, 112, 1.4f).lfo(2, LW_TRI, 0.37f).lfo(3, LW_SINE, 0.29f)
            .mod(SRC_LFO1 + 2, "Mutant 1", "Depth", 50).mod(SRC_LFO1 + 3, "Mutant 3", "Depth", 40)
            .set(P_VO_WIDTH, 110).set(P_VO_AFEEL, 30).set(P_VO_WARM, 1).fx(0, FX_CHORUS, 0, 46).reverb(RT_HALL, 3.8f, 28)
            .macro(0, 14, {{D("Mutant 1", "Depth"), 70}, {D("Mutant 3", "Depth"), 70}});
        add(m);
    }
    {
        Maker m("Harmonic Rain", "Pad");
        m.wave(0, "Saw").mut(0, MU_HARM, 1, 20, 100).wave(1, "Saw").set(pOsc(1, OS_CENT), 6).mut(2, MU_HARM, 2, 30, 100)
            .mix(0, 110, -40).mix(1, 110, 40).f1(F1_FAT12, 128, 0).env(1, 1.6f, 2.0f, 110, 3.0f)
            .lfo(2, LW_RANDOM, 0.5f).lfo(3, LW_TRI, 0.07f).mod(SRC_LFO1 + 2, "Mutant 1", "Depth", 28)
            .mod(SRC_LFO1 + 3, "Mutant 3", "Depth", 40).mod(SRC_KEY, "Mutant 1", "Depth", -10)
            .set(P_VO_WIDTH, 127).reverb(RT_CLOUD, 9.0f, 45, 40, 30).delay(DT_PAN, 500, 60, 25)
            .macro(0, 13, {{D("Mutant 1", "Depth"), 60}, {D("Mutant 3", "Depth"), 60}}).macro(1, 58, {{D("Reverb", "Dry/Wet"), 60}});
        add(m);
    }
    {
        Maker m("Phase Pluck", "Pluck");
        m.wave(0, "Saw").mut(0, MU_PHAZ, 1, 40, 100).wave(1, "Triangle").set(pOsc(1, OS_SEMI), 12).mix(0, 118).mix(1, 40)
            .f1(F1_LDR24, 40, 36, 44, 60).env(0, 0, 0.32f, 0, 0.3f).env(1, 0, 0.7f, 0, 0.5f).env(2, 0, 0.25f, 0, 0.3f)
            .mod(SRC_ENV1 + 2, "Mutant 1", "Depth", 70).delay(DT_PAN, 375, 46, 26, 12).reverb(RT_PLATE, 1.8f, 20)
            .macro(0, 1, {{D("Filter 1", "Cutoff"), 60}}).macro(1, 3, {{D("Filter 1", "ENV1amt"), 40}});
        add(m);
    }
    {
        Maker m("Warp Engine", "Lead");
        m.wave(0, "Sine").mut(0, MU_PWASM, 2, 96, 100, 20).wave(1, "Saw").set(pOsc(1, OS_CENT), -8)
            .mix(0, 116).mix(1, 50).f1(F1_MS20LP, 90, 50, 20, 40).env(0, 0, 0.6f, 40, 0.3f).env(1, 0.003f, 0.5f, 110, 0.3f)
            .set(pMut(0, MT_WARP1 + 1), 120).set(pMut(0, MT_WARP1 + 4), 10).set(pMut(0, MT_WARP1 + 6), 110)
            .lfo(2, LW_TRI, 3.0f).lfo(3, LW_SAWDN, 0.5f).mod(SRC_LFO1 + 2, "Mutant 1", "Warp 2", 50).mod(SRC_LFO1 + 3, "Mutant 1", "Warp 5", 60)
            .mod(SRC_MODWHL, "Mutant 1", "Ratio", 40).set(P_VO_POLY, PO_MONO).set(P_VO_GLIDE, GL_GLIDE).set(P_VO_GTIME, 20)
            .delay(DT_STEREO, 250, 40, 20).macro(0, 12, {{D("Mutant 1", "Depth"), -60}}).macro(1, 11, {{D("Mutant 1", "Ratio"), 30}});
        add(m);
    }
    {
        Maker m("Mutant Choir", "Vox");
        m.scan(0, {"Vokz 1", "Vokz 2", "Vokz 3", "Vokz 4", "Vokz 5"}, 1.0f).wave(1, "Glefan 2").set(pOsc(1, OS_CENT), 8)
            .mut(0, MU_STACK, 1, 26, 60).mix(0, 112).mix(1, 64).f1(F1_VOWEL, 50, 40).set(P_F1_CTRL, 60)
            .env(1, 0.7f, 1.0f, 110, 1.8f).lfo(2, LW_TRI, 0.18f).lfo(3, LW_SINE, 0.09f)
            .mod(SRC_LFO1 + 2, "Filter 1", "Cutoff", 60).mod(SRC_LFO1 + 3, "Osc 1", "WaveScan", 110)
            .set(P_VO_WIDTH, 100).set(P_VO_AFEEL, 25).fx(0, FX_CHORUS, 1, 35).reverb(RT_HALL, 5.0f, 40, 25)
            .macro(0, 20, {{D("Filter 1", "Cutoff"), 128}}).macro(1, 8, {{D("Osc 1", "WaveScan"), 128}});
        add(m);
    }
    {
        Maker m("Hydra Run", "Arp");
        m.wave(0, "Saw").mut(0, MU_PWORIG, 2, 30, 60).wave(1, "Square").set(pOsc(1, OS_SEMI), 12).mix(0, 110).mix(1, 50)
            .f1(F1_FAT24, 52, 40, 36, 50).env(0, 0, 0.22f, 10, 0.2f).env(1, 0, 0.35f, 30, 0.25f)
            .set(P_AR_ON, 1).set(P_AR_MODE, AM_UPANDDN).set(P_AR_DIV, 7).set(P_AR_OCT, 2).set(P_AR_GATE, 60)
            .set(P_AR_LATCH, 1).lfo(2, LW_TRI, 0.12f).mod(SRC_LFO1 + 2, "Filter 1", "Cutoff", 30)
            .delay(DT_PAN, 375, 52, 30, 12).reverb(RT_ROOM, 1.6f, 18)
            .macro(0, 1, {{D("Filter 1", "Cutoff"), 70}}).macro(1, 53, {{D("Arp", "Chance"), 100}, {D("Arp", "Ratchet"), 128}});
        add(m);
    }
    {
        Maker m("Phrase Machine", "Arp");
        m.wave(0, "Duotone 2").mut(0, MU_FM, 2, 22, 100).wave(1, "Pulse 3").set(pOsc(1, OS_SEMI), -12).mix(0, 110).mix(1, 60)
            .f1(F1_3LER_LP, 58, 50, 30, 40).env(0, 0, 0.18f, 0, 0.2f).env(1, 0, 0.3f, 0, 0.2f)
            .set(P_AR_ON, 1).set(P_AR_MODE, AM_PHRASE).set(P_AR_PHRASE, 12).set(P_AR_DIV, 7).set(P_AR_OCT, 1).set(P_AR_LATCH, 1)
            .set(P_AR_SWING, 58).delay(DT_LRC, 375, 40, 22, 10).reverb(RT_ROOM, 1.2f, 14)
            .macro(0, 1, {{D("Filter 1", "Cutoff"), 60}}).macro(1, 51, {{D("Arp", "Swing"), 128}});
        add(m);
    }
    {
        Maker m("Step Garden", "Seq");
        m.wave(0, "Triangle").mut(0, MU_FM, 1, 18, 100).wave(2, "Sine").set(pOsc(2, OS_SEMI), -12).mix(0, 110).mix(2, 70)
            .f1(F1_LDR24, 60, 40, 30).env(0, 0, 0.2f, 20, 0.3f).env(1, 0.002f, 1.0f, 100, 0.6f)
            .steps(0, {0, 15, 35, 0, 50, 35, 15, 60}).set(pLfo(0, LF_BPM), 1).set(pLfo(0, LF_SYNC), 19).set(pLfo(0, LF_SEMILOCK), 1)
            .mod(SRC_LFO1, "Osc 1", "Pitch", 128).set(P_F1_LFO, 0).lfo(1, LW_SQUARE, 1.0f, 0).set(pLfo(1, LF_BPM), 1)
            .set(pLfo(1, LF_SYNC), 7).set(pEnv(0, EN_TRIG1 + 1), TR_LFO2).set(pLfo(1, LF_LEVEL), 128)
            .lfo(2, LW_TRI, 0.05f).mod(SRC_LFO1 + 2, "Filter 1", "Cutoff", 20).delay(DT_PAN, 375, 50, 28, 12).reverb(RT_PLATE, 2.5f, 24)
            .macro(0, 1, {{D("Filter 1", "Cutoff"), 60}});
        add(m);
    }
    {
        Maker m("Deep Water", "Drone");
        m.scan(0, {"Particl 2", "Horizon 1", "Chendom 3", "Glefan 6"}, 1.0f).wave(1, "Klangor 1").set(pOsc(1, OS_SEMI), 7)
            .wave(2, "Sine").set(pOsc(2, OS_SEMI), -12).mut(2, MU_STACK, 1, 50, 70).mix(0, 100).mix(1, 50).mix(2, 90).mix(4, 18)
            .set(P_RN_NOISE, NT_BROWN).f1(F1_8POLE, 64, 30).env(1, 4.0f, 2.0f, 128, 8.0f).lfo(2, LW_RANDOM, 0.06f)
            .lfo(3, LW_TRI, 0.03f).mod(SRC_LFO1 + 2, "Osc 1", "WaveScan", 128).mod(SRC_LFO1 + 3, "Filter 1", "Cutoff", 30)
            .set(P_VO_WIDTH, 127).set(P_VO_AFEEL, 40).reverb(RT_CLOUD, 30.0f, 55, 60, 40).delay(DT_REVERSE, 900, 50, 20)
            .macro(0, 44, {{D("Reverb", "Time"), 128}}).macro(1, 1, {{D("Filter 1", "Cutoff"), 50}});
        add(m);
    }
    {
        Maker m("Bronze Section", "Brass");
        m.wave(0, "Saw").wave(1, "Saw").set(pOsc(1, OS_CENT), 9).mut(0, MU_STACK, 1, 16, 50).mix(0, 110).mix(1, 100)
            .f1(F1_MS20LP, 52, 30, 40, 50).set(P_F1_VEL, 30).env(0, 0.08f, 0.6f, 70, 0.4f).env(1, 0.05f, 0.5f, 118, 0.5f)
            .set(P_VO_AFEEL, 22).set(P_VO_WIDTH, 70).fx(0, FX_CHORUS, 2, 25).reverb(RT_HALL, 2.4f, 22)
            .macro(0, 1, {{D("Filter 1", "Cutoff"), 50}}).macro(1, 3, {{D("Filter 1", "ENV1amt"), 40}});
        add(m);
    }
    {
        Maker m("Electric Tines", "Keys");
        m.wave(0, "Sine").mut(0, MU_FM, 14, 24, 100).wave(1, "Sine").mut(2, MU_FM, 1, 30, 100).mix(0, 70).mix(1, 110)
            .f1(F1_1POLE, 110, 0).env(1, 0, 2.8f, 0, 0.6f).env(2, 0, 0.18f, 0, 0.2f).env(3, 0, 1.2f, 20, 0.6f)
            .mod(SRC_ENV1 + 2, "Mutant 1", "Depth", 80).mod(SRC_ENV1 + 3, "Mutant 3", "Depth", 40).mod(SRC_VELON, "Mutant 1", "Depth", 40)
            .fx(0, FX_TREMOLO, 0, 60).reverb(RT_ROOM, 1.4f, 16).macro(0, 10, {{D("Mutant 3", "Depth"), 70}})
            .macro(1, 37, {{D("Pre-FX", "Dry/Wet"), 100}});
        add(m);
    }
    {
        Maker m("Orland Drawbars", "Organ");
        m.wave(0, "Orland 2").wave(1, "Orland 7").set(pOsc(1, OS_SEMI), 12).mix(0, 110).mix(1, 50).f1(F1_1POLE, 128, 0)
            .env(1, 0.004f, 0.1f, 128, 0.06f).fx(1, FX_ROTARY, 0, 100).reverb(RT_ROOM, 1.5f, 14).set(P_VO_AFEEL, 8)
            .macro(0, 36, {{D("Post-FX", "Param1"), 128}, {D("Post-FX", "Param2"), 128}});
        add(m);
    }
    {
        Maker m("Cassette Keys", "Keys");
        m.wave(0, "Tronic 4").set(pOsc(0, OS_BITS), 7).wave(1, "Melotic 2").mix(0, 90).mix(1, 90).f1(F1_LDR12, 80, 10, 10)
            .env(1, 0.01f, 1.8f, 50, 0.8f).lfo(2, LW_SINE, 0.6f).mod(SRC_LFO1 + 2, "All Osc", "Pitch", 1.2f)
            .fx(0, FX_LOFI, 0, 80).fx(1, FX_EQ, 6, 100).reverb(RT_ROOM, 1.3f, 18).set(P_VO_WARM, 1)
            .macro(0, 39, {{D("Pre-FX", "Param1"), -100}}).macro(1, 56, {{D("ModMtrx", "Depth 1"), 8}});
        add(m);
    }
    {
        Maker m("Ring Foundry", "FX");
        m.wave(0, "Square").wave(1, "Belview 2").set(pOsc(1, OS_SEMI), 19).mix(0, 30).mix(1, 30).mix(3, 120)
            .set(P_RN_SRC1, RS_OSC1).set(P_RN_SRC2, RS_OSC2).f1(F1_STN12BP, 80, 60, 20).env(1, 0, 1.2f, 30, 1.0f)
            .lfo(2, LW_SH, 6.0f).mod(SRC_LFO1 + 2, "Osc 2", "Pitch", 6).delay(DT_PAN, 200, 60, 26).reverb(RT_PLATE, 3.0f, 30)
            .macro(0, 16, {{D("Ring Mod", "Depth"), -80}});
        add(m);
    }
    {
        Maker m("Wooden Gate", "Pluck");
        m.wave(0, "Triangle").mut(0, MU_FM, 2, 30, 100).wave(1, "Sine").set(pOsc(1, OS_SEMI), 12).mix(0, 110).mix(1, 40)
            .f1(F1_GATE, 30, 40, 64, 40).env(0, 0, 0.35f, 0, 0.35f).env(1, 0, 1.0f, 0, 0.6f).env(2, 0, 0.12f, 0, 0.1f)
            .mod(SRC_ENV1 + 2, "Mutant 1", "Depth", 60).delay(DT_STEREO, 333, 35, 20).reverb(RT_ROOM, 1.5f, 20)
            .macro(0, 1, {{D("Filter 1", "Cutoff"), 50}});
        add(m);
    }
    {
        Maker m("Boutique Bass", "Bass");
        m.wave(0, "Saw").mut(0, MU_PWORIG, 1, 20, 50).wave(2, "Square").set(pOsc(2, OS_SEMI), -12).mix(0, 112).mix(2, 80)
            .f1(F1_3LER_LP, 34, 60, 48, 30).env(0, 0, 0.28f, 10, 0.2f).env(1, 0.001f, 0.6f, 100, 0.12f)
            .set(P_VO_POLY, PO_MONO).set(P_VO_GLIDE, GL_GLIDE).set(P_VO_GTIME, 18).set(P_VO_GLEGATO, 1).set(P_VO_SNAP, 1)
            .macro(0, 1, {{D("Filter 1", "Cutoff"), 60}}).macro(1, 2, {{D("Filter 1", "Resonance"), 60}});
        add(m);
    }
    {
        Maker m("Steiner Scream", "Lead");
        m.wave(0, "Saw").wave(1, "Pulse 2").set(pOsc(1, OS_CENT), 12).mix(0, 110).mix(1, 90).f1(F1_STN12LP, 60, 100, 30, 40)
            .set(P_F1_DRIVE, 50).env(0, 0, 0.8f, 30, 0.5f).env(1, 0.003f, 0.5f, 110, 0.4f).set(P_VO_POLY, PO_MONO)
            .set(P_VO_VIBAMT, 0.5f).delay(DT_STEREO, 300, 40, 20).fx(1, FX_DISTORT, 0, 35)
            .macro(0, 1, {{D("Filter 1", "Cutoff"), 60}}).macro(1, 18, {{D("Filter 1", "Drive"), 90}});
        add(m);
    }
    {
        Maker m("Eight Pole Dusk", "Pad");
        m.scan(0, {"Esquire 2", "ChriMey 3", "Spect X 2"}, 1.0f).wave(1, "Sano 3").set(pOsc(1, OS_SEMI), 12).set(pOsc(1, OS_CENT), 5)
            .mut(0, MU_STACK, 1, 36, 70).mix(0, 110).mix(1, 60).f1(F1_8POLE, 58, 40, 30, 40).env(0, 2.0f, 3.0f, 50, 3.0f)
            .env(1, 1.5f, 2.0f, 110, 3.0f).lfo(2, LW_TRI, 0.08f).mod(SRC_LFO1 + 2, "Osc 1", "WaveScan", 128)
            .set(P_VO_WIDTH, 120).set(P_VO_WARM, 1).fx(0, FX_PHASER, 1, 40).reverb(RT_HALL, 7.0f, 40, 40)
            .macro(0, 1, {{D("Filter 1", "Cutoff"), 60}}).macro(1, 35, {{D("Pre-FX", "Dry/Wet"), 100}});
        add(m);
    }
    {
        Maker m("Bit Hydra", "Bass");
        m.wave(0, "Alweg 3").set(pOsc(0, OS_BITS), 9).mut(0, MU_FM, 0.5f, 40, 100, 30).wave(2, "Sine").set(pOsc(2, OS_SEMI), -12)
            .mix(0, 100).mix(2, 90).f1(F1_FAT24, 46, 50, 40, 30).env(0, 0, 0.3f, 20, 0.2f).env(1, 0, 0.5f, 100, 0.15f)
            .env(2, 0, 0.25f, 0, 0.2f).mod(SRC_ENV1 + 2, "Mutant 1", "Depth", 50).set(P_VO_POLY, PO_MONO).set(P_VO_SNAP, 1)
            .fx(1, FX_DISTORT, 1, 40).macro(0, 1, {{D("Filter 1", "Cutoff"), 60}}).macro(1, 39, {{D("Mutant 1", "Feedback"), 90}});
        add(m);
    }
    {
        Maker m("Unison Hydra", "Lead");
        m.wave(0, "Saw").wave(1, "Square").set(pOsc(1, OS_SEMI), -12).mix(0, 110).mix(1, 60).f1(F1_FAT24, 90, 20, 16, 40)
            .env(1, 0.004f, 0.6f, 110, 0.5f).set(P_VO_POLY, PO_UNI).set(P_VO_DENSITY, 6).set(P_VO_DETUNE, 40)
            .set(P_VO_WIDTH, 127).set(P_VO_GLIDE, GL_GLIDE).set(P_VO_GTIME, 24).delay(DT_PAN, 300, 45, 22, 12)
            .reverb(RT_PLATE, 2.0f, 20).macro(0, 27, {{D("Voice", "Detune"), 80}});
        add(m);
    }
    {
        Maker m("Surf Static", "FX");
        m.mix(0, 0).mix(4, 120).set(P_RN_NOISE, NT_PINK).f1(F1_STN12BP, 72, 30).set(P_MX_ROUTE, 0).set(P_F2_MORPH, 20)
            .set(P_F2_CUT, 96).set(P_F2_RES, 30).env(1, 2.0f, 1.0f, 128, 4.0f).lfo(2, LW_SINE, 0.09f).lfo(3, LW_SINE, 0.13f)
            .mod(SRC_LFO1 + 2, "Filter 1", "Cutoff", 40).mod(SRC_LFO1 + 3, "Filter 2", "Cutoff", 40).mod(SRC_LFO1 + 3, "Mixer", "NoisePan", 64)
            .reverb(RT_CLOUD, 12.0f, 50, 50).macro(0, 1, {{D("Filter 1", "Cutoff"), 60}, {D("Filter 2", "Cutoff"), 60}});
        add(m);
    }
    {
        Maker m("Squeezed Saw Keys", "Keys");
        m.wave(0, "Saw").mut(0, MU_PWSQZ, 1, 20, 100).wave(1, "Saw").set(pOsc(1, OS_CENT), 5).mut(2, MU_PWSQZ, 1, 20, 100)
            .mix(0, 104).mix(1, 90).f1(F1_LDR24, 50, 20, 40, 60).env(0, 0, 1.1f, 30, 0.7f).env(1, 0, 1.4f, 70, 0.7f)
            .lfo(2, LW_TRI, 0.8f).mod(SRC_LFO1 + 2, "Mutant 1", "Depth", 18).mod(SRC_LFO1 + 2, "Mutant 3", "Depth", -18)
            .set(P_VO_WIDTH, 90).fx(0, FX_CHORUS, 0, 30).reverb(RT_ROOM, 1.8f, 20)
            .macro(0, 1, {{D("Filter 1", "Cutoff"), 60}});
        add(m);
    }
    {
        Maker m("Sub Mutant", "Bass");
        m.wave(0, "Saw").mut(0, MU_SYNC, 2, 10, 60).wave(2, "Sine").set(pOsc(2, OS_SEMI), -12).mix(0, 100).mix(2, 110)
            .f1(F1_LDR24, 38, 44, 46, 20).env(0, 0, 0.25f, 10, 0.2f).env(1, 0, 0.4f, 110, 0.12f).env(2, 0, 0.3f, 0, 0.2f)
            .mod(SRC_ENV1 + 2, "Mutant 1", "Depth", 50).set(P_VO_POLY, PO_MONOLO).set(P_VO_GLIDE, GL_GLIDE).set(P_VO_GTIME, 14)
            .set(P_VO_WARM, 1).macro(0, 1, {{D("Filter 1", "Cutoff"), 60}}).macro(1, 15, {{D("Mutant 1", "Depth"), 100}});
        add(m);
    }
    {
        Maker m("Cluster Bells", "Bell");
        m.wave(0, "Cluster 3").mut(0, MU_FM, 4, 30, 60).wave(1, "Belview 4").set(pOsc(1, OS_SEMI), 12).mix(0, 90).mix(1, 80)
            .f1(F1_FAT12, 110, 20).env(1, 0, 4.0f, 0, 3.0f).env(2, 0, 2.0f, 0, 2.0f).mod(SRC_ENV1 + 2, "Mutant 1", "Depth", 50)
            .set(P_VO_WIDTH, 127).delay(DT_PAN, 450, 55, 30).reverb(RT_HALL, 6.0f, 40, 30, 30)
            .macro(0, 10, {{D("Mutant 1", "Depth"), 80}});
        add(m);
    }
    {
        Maker m("Hydra Strings", "Strings");
        m.wave(0, "Saw").mut(0, MU_STACK, 1, 46, 100).wave(1, "Resyn 2").set(pOsc(1, OS_SEMI), 12).mix(0, 110).mix(1, 50)
            .f1(F1_LDR12, 84, 10, 10, 40).env(0, 0.8f, 1.0f, 80, 1.5f).env(1, 0.7f, 1.0f, 118, 1.6f)
            .set(P_VO_WIDTH, 110).set(P_VO_AFEEL, 20).set(P_VO_WARM, 1).fx(0, FX_CHORUS, 1, 30).reverb(RT_HALL, 4.5f, 34)
            .macro(0, 9, {{D("Mutant 1", "Depth"), 70}}).macro(1, 1, {{D("Filter 1", "Cutoff"), 50}});
        add(m);
    }
    {
        Maker m("Flux Capacitor", "FX");
        m.scan(0, {"Flux 1", "Flux 3", "Flux 5", "-", "Induct 2"}, 1.0f).mut(0, MU_SYNC, 1.5f, 40, 70).mix(0, 110)
            .f1(F1_3LER_BP, 70, 70, 20).lfo(2, LW_SAWUP, 0.25f).lfo(3, LW_SH, 8.0f).set(pLfo(3, LF_SMOOTH), 60)
            .mod(SRC_LFO1 + 2, "Osc 1", "WaveScan", 128).mod(SRC_LFO1 + 3, "Mutant 1", "Ratio", 30).env(1, 0.2f, 1.0f, 110, 2.0f)
            .delay(DT_LRC, 250, 60, 30, 10).reverb(RT_CLOUD, 8.0f, 40).macro(0, 1, {{D("Filter 1", "Cutoff"), 60}});
        add(m);
    }
    {
        Maker m("Init Saw", "Init");
        add(m);
    }
    /* level trim: every factory sound comes out at about the same loudness.
       Amp Level takes the first part; if it would need more than full scale,
       the mixer levels make up the rest. */
    static const struct { const char *n; float g; } T[] = {
        {"Liquid Glass", 0.95f},
        {"Seven Heads", 1.51f},
        {"Glass Clockwork", 0.59f},
        {"Sync Serpent", 1.05f},
        {"Velvet Squeeze", 0.62f},
        {"Harmonic Rain", 1.29f},
        {"Phase Pluck", 1.43f},
        {"Warp Engine", 0.82f},
        {"Mutant Choir", 4.00f},
        {"Hydra Run", 0.98f},
        {"Phrase Machine", 3.33f},
        {"Step Garden", 1.20f},
        {"Deep Water", 0.73f},
        {"Bronze Section", 0.72f},
        {"Electric Tines", 0.71f},
        {"Orland Drawbars", 0.55f},
        {"Cassette Keys", 0.74f},
        {"Ring Foundry", 3.14f},
        {"Wooden Gate", 1.09f},
        {"Boutique Bass", 2.49f},
        {"Steiner Scream", 0.33f},
        {"Eight Pole Dusk", 1.42f},
        {"Bit Hydra", 0.74f},
        {"Unison Hydra", 0.34f},
        {"Surf Static", 4.00f},
        {"Squeezed Saw Keys", 0.87f},
        {"Sub Mutant", 1.39f},
        {"Cluster Bells", 1.47f},
        {"Hydra Strings", 1.15f},
        {"Flux Capacitor", 0.69f},
    };
    for (auto &p : v)
        for (auto &t : T)
            if (std::strcmp(p.name, t.n) == 0)
            {
                float want = p.v[P_AMP_LEVEL] * t.g;
                float lv = want > 128.0f ? 128.0f : want;
                float rest = want / lv;
                p.v[P_AMP_LEVEL] = std::round(lv);
                if (rest > 1.01f)
                    for (int k = 0; k < 6; ++k)
                        p.v[P_MX_VOL1 + k] = std::fmin(128.0f, std::round(p.v[P_MX_VOL1 + k] * rest));
            }
    return v;
}
} // namespace

static const std::vector<Patch> &all()
{
    static const std::vector<Patch> P = build();
    return P;
}
int factoryCount() { return (int)all().size(); }
Patch factory(int i)
{
    const auto &P = all();
    return P[(size_t)(i < 0 ? 0 : i >= (int)P.size() ? (int)P.size() - 1 : i)];
}
} // namespace hy
