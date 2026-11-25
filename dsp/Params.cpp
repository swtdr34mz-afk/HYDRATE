/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
#include "Params.h"
#include "Waves.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace hy
{
/* ------------------------------------------------------------------ lists --- */
static const char *const ONOFF[] = {"Off", "On"};
static const char *const OSCMODE[] = {"Single", "WaveScan"};
static const char *const MUTMODE[] = {"FM-Lin", "WavStack", "OSC Sync", "PW-Orig", "PW-Sqeez", "PW-ASM", "Harmonic", "PhazDiff"};
static const char *const FMSRC[] = {"Sine", "Triangle", "Osc 1", "Osc 2", "Osc 3", "Ring Mod", "Noise", "Mutant 1", "Mutant 2", "Mutant 3", "Mutant 4"};
static const char *const SYNCSRC[] = {"Osc 1", "Osc 2", "Osc 3"};
static const char *const RINGSRC[] = {"Osc 1", "Osc 2", "Osc 3", "Noise", "Mutant 1", "Mutant 2", "Mutant 3", "Mutant 4"};
static const char *const NOISE[] = {"White", "Pink", "Brown", "Red", "Blue", "Violet", "Grey"};
static const char *const ROUTE[] = {"Series", "Parallel"};
static const char *const F1TYPE[] = {"LP Ldr12", "LP Ldr24", "LP Fat12", "LP Fat24", "LP Gate", "LP MS20", "LP 3-Ler", "LP Stn12",
                                     "LP 1 Pole", "LP 8 Pole", "HP MS20", "HP Stn12", "HP 3-Ler", "BP 3-Ler", "BP Stn12", "Vowel"};
static const char *const DRVROUTE[] = {"Pre", "Post"};
static const char *const VOWORD[] = {"AEIOU", "AIUEO", "EAIOU", "IOUAE", "OUAEI", "UOIEA", "EIOUA", "UIEAO"};
static const char *const F2TYPE[] = {"LP-BP-HP", "LP-NO-HP"};
static const char *const LFOWAVE[] = {"Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Pulse27%", "Pulse13%", "S&H", "Noise", "Random", "Step"};
static const char *const TRIGSYNC[] = {"Poly", "Single", "Off"};
static const char *const ONESHOT[] = {"Off", "On", "Step"};
static const char *const TRIGSRC[] = {"OFF", "Note On", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "SusPed On"};
static const char *const POLY[] = {"Poly", "Mono", "MonoLo", "MonoHi", "Unison", "UnisonLo", "UnisonHi"};
static const char *const STMODE[] = {"Rotate", "Alter", "Random"};
static const char *const GLIDE[] = {"Off", "Glide", "Glissando"};
static const char *const KEYS[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const SUSPED[] = {"Sustain", "Sosten", "Mod Only"};
static const char *const FXTYPE[] = {"Bypass", "Chorus", "Flanger", "Rotary", "Phaser", "Lo-Fi", "Tremolo", "EQ", "Compressor", "Distort"};
static const char *const DLYTYPE[] = {"Basic Mono", "Basic Stereo", "Pan Delay", "LRC Delay", "Reverse"};
static const char *const REVTYPE[] = {"Hall", "Room", "Plate", "Cloud"};
static const char *const ARPMODE[] = {"Up", "Down", "Up/Dn", "Up & Dn", "Order", "Random", "Chord", "Phrase"};
static const char *const ARPOCT[] = {"Up", "Dn", "UpDn", "Alt", "Alt 2"};
static const char *const RATCHET[] = {"1", "2", "4", "8"};

const char *const BITS_NAME[13] = {"Off", "16 bit", "14 bit", "12 bit", "10 bit", "9 bit", "8 bit", "7 bit", "6 bit", "5 bit", "4 bit", "3 bit", "2 bit"};
const int BITS_VAL[13] = {0, 16, 14, 12, 10, 9, 8, 7, 6, 5, 4, 3, 2};

const char *const SYNC_NAME[NSYNC] = {"1/64T", "1/64", "1/32T", "1/64Dot", "1/32", "1/16T", "1/32Dot", "1/16", "1/8T", "1/16Dot",
                                      "1/8", "1/4T", "1/8Dot", "1/4", "1/2T", "1/4Dot", "1/2", "1/1T", "1/2Dot", "1/1",
                                      "1/1Dot", "8'", "12'", "16'", "24'", "32'", "40'", "48'", "64'"};
const float SYNC_BEATS[NSYNC] = {1.f / 24, 1.f / 16, 1.f / 12, 3.f / 32, 1.f / 8, 1.f / 6, 3.f / 16, 0.25f, 1.f / 3, 0.375f,
                                 0.5f, 2.f / 3, 0.75f, 1.0f, 4.f / 3, 1.5f, 2.0f, 8.f / 3, 3.0f, 4.0f,
                                 6.0f, 8.0f, 12.0f, 16.0f, 24.0f, 32.0f, 40.0f, 48.0f, 64.0f};
const char *const ARPDIV_NAME[NARPDIV] = {"1/1", "1/2", "1/2T", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T", "1/64"};
const float ARPDIV_BEATS[NARPDIV] = {4.0f, 2.0f, 4.f / 3, 1.0f, 2.f / 3, 0.5f, 1.f / 3, 0.25f, 1.f / 6, 0.125f, 1.f / 12, 0.0625f};

/* the preset scales (the notes from C: x in the scale) */
const char *const SCALE_NAME[NSCALE] = {
    "Chromatic", "Major", "BeBop Maj", "BeBop", "Mixolydian", "Harmonic Major", "Lydian", "Lydian Aug", "Acoustic",
    "Pentatonic Maj", "Locrian Maj", "Prometheus", "Whole Tone", "Melodic Minor", "Half Dim", "Aeolian", "Dorian",
    "Harmonic Minor", "Algerian", "Gypsy", "Hungarian", "Ukranian", "Dim Whole Tone", "Locrian", "Neapolitan Maj",
    "Neapolitan Min", "Phrygian", "Flamenco", "Persian", "Phrygian Dom", "Enigmatic", "Tritone", "In", "Insen",
    "Augmented", "Blues", "Pentatonic Min", "Hirajoshi"};
static const char *const SCALE_X[NSCALE] = {
    "xxxxxxxxxxxx", "x-x-xx-x-x-x", "x-x-xx-xxx-x", "x-x-xx-x-xxx", "x-x-xx-x-xx-", "x-x-xx-xx--x", "x-x-x-xx-x-x",
    "x-x-x-x-xx-x", "x-x-x-xx-xx-", "x-x-x--x-x--", "x-x-xxx-x-x-", "x-x-x-x--xx-", "x-x-x-x-x-x-", "x-xx-x-xxxxx",
    "x-xx-xx-x-x-", "x-xx-x-xx-x-", "x-xx-x-x-xx-", "x-xx-x-xx--x", "x-xx-xxxx--x", "x-xx--xxx-x-", "x-xx--xxx--x",
    "x-xx--xx-xx-", "xx-xx-x-x-x-", "xx-x-xx-x-x-", "xx-x-x-x-x-x", "xx-x-x-xx--x", "xx-x-x-xx-x-", "xx--xx-xx--x",
    "xx--xxx-x--x", "xx--xx-xx-x-", "xx--x-x-x-xx", "xx--x-xx--x-", "xx---x-xx---", "xx---x-x--x-", "x--xx--xx--x",
    "x--x-xxx--x-", "x--x-x-x--x-", "x---x-xx---x"};
static uint16_t maskOf(const char *s)
{
    uint16_t m = 0;
    for (int i = 0; i < 12; i++)
        if (s[i] == 'x')
            m |= (uint16_t)(1u << i);
    return m;
}
const uint16_t SCALE_MASK[NSCALE] = {
    maskOf(SCALE_X[0]),  maskOf(SCALE_X[1]),  maskOf(SCALE_X[2]),  maskOf(SCALE_X[3]),  maskOf(SCALE_X[4]),
    maskOf(SCALE_X[5]),  maskOf(SCALE_X[6]),  maskOf(SCALE_X[7]),  maskOf(SCALE_X[8]),  maskOf(SCALE_X[9]),
    maskOf(SCALE_X[10]), maskOf(SCALE_X[11]), maskOf(SCALE_X[12]), maskOf(SCALE_X[13]), maskOf(SCALE_X[14]),
    maskOf(SCALE_X[15]), maskOf(SCALE_X[16]), maskOf(SCALE_X[17]), maskOf(SCALE_X[18]), maskOf(SCALE_X[19]),
    maskOf(SCALE_X[20]), maskOf(SCALE_X[21]), maskOf(SCALE_X[22]), maskOf(SCALE_X[23]), maskOf(SCALE_X[24]),
    maskOf(SCALE_X[25]), maskOf(SCALE_X[26]), maskOf(SCALE_X[27]), maskOf(SCALE_X[28]), maskOf(SCALE_X[29]),
    maskOf(SCALE_X[30]), maskOf(SCALE_X[31]), maskOf(SCALE_X[32]), maskOf(SCALE_X[33]), maskOf(SCALE_X[34]),
    maskOf(SCALE_X[35]), maskOf(SCALE_X[36]), maskOf(SCALE_X[37])};

const char *const MACRO_NAME[NMACRONAME] = {
    "Macro", "Cutoff", "Reso", "Env Amt", "LFO Amt", "Attack", "Decay", "Release", "Wavescan", "WavStack", "FM",
    "Ratio", "Warp", "Harmonic", "PulsWdth", "Sync", "RingMod", "Noise", "Drive", "Morph", "Vowel", "Filter",
    "Brighten", "Darken", "Thicken", "Thin", "Width", "Detune", "Glide", "Vibrato", "Pitch", "Oct +", "Oct -",
    "Chorus", "Flanger", "Phaser", "Rotary", "Tremolo", "Lo-Fi", "Crush", "Distort", "Delay", "Reverb", "Space",
    "Freeze", "Dry/Wet", "Time", "Feedback", "Rate", "Depth", "Speed", "Swing", "Ratchet", "Chance", "Gate",
    "Arp", "Wobble", "Growl", "Shimmer", "Air", "Punch", "Mangle", "Bloom", "Level"};

/* -------------------------------------------------------------- sources --- */
const char *srcName(int s)
{
    static char cc[128][8];
    static bool made = false;
    static const char *const N[SRC_CC0] = {"Off",      "Env 1",    "Env 2",    "Env 3",   "Env 4",    "Env 5",
                                           "LFO 1",    "LFO 2",    "LFO 3",    "LFO 4",   "LFO 5",    "LFO 1+",
                                           "LFO 2+",   "LFO 3+",   "LFO 4+",   "LFO 5+",  "MonoAftT", "PolyAftT",
                                           "Keytrack", "Velo On",  "Velo Off", "PitchWhl", "ModWhl",  "ExpPedal",
                                           "SusPedal", "Breath",   "VoiceMod", "VoiceMd+"};
    if (!made) {
        for (int i = 0; i < 128; i++)
            std::snprintf(cc[i], 8, "CC %03d", i);
        made = true;
    }
    if (s < 0 || s >= NSRC)
        return "?";
    return s < SRC_CC0 ? N[s] : cc[s - SRC_CC0];
}

/* ----------------------------------------------------------- definitions --- */
static std::vector<ParamDef> build();
const std::vector<ParamDef> &params()
{
    static const std::vector<ParamDef> P = build();
    return P;
}

static std::vector<DstDef> buildDst();
const std::vector<DstDef> &dests()
{
    static const std::vector<DstDef> D = buildDst();
    return D;
}
int dstOfParam(int p) { return p >= 0 && p < NPARAM ? params()[(size_t)p].dst : -1; }

namespace
{
struct B {
    std::vector<ParamDef> v;
    B() { v.resize(NPARAM); for (auto &d : v) { std::memset(&d, 0, sizeof d); d.skew = 1.0f; d.dst = -1; } }
    ParamDef &at(int p, const std::string &id, const std::string &name, const char *sh, float lo, float hi, float def, Kind k,
                 float step = 0.0f, float skew = 1.0f, const char *const *ch = nullptr)
    {
        ParamDef &d = v[(size_t)p];
        std::snprintf(d.id, sizeof d.id, "%s", id.c_str());
        std::snprintf(d.name, sizeof d.name, "%s", name.c_str());
        std::snprintf(d.shortName, sizeof d.shortName, "%s", sh);
        d.lo = lo; d.hi = hi; d.def = def; d.kind = k; d.step = step; d.skew = skew; d.choice = ch; d.dst = -1;
        return d;
    }
    void ch(int p, const std::string &id, const std::string &name, const char *sh, int n, int def, const char *const *list)
    {
        at(p, id, name, sh, 0.0f, (float)(n - 1), (float)def, K_CHOICE, 1.0f, 1.0f, list);
    }
    void onoff(int p, const std::string &id, const std::string &name, const char *sh, int def)
    {
        at(p, id, name, sh, 0.0f, 1.0f, (float)def, K_ONOFF, 1.0f, 1.0f, ONOFF);
    }
    void num(int p, const std::string &id, const std::string &name, const char *sh, float def) { at(p, id, name, sh, 0, 128, def, K_NUM); }
    void bip(int p, const std::string &id, const std::string &name, const char *sh, float def) { at(p, id, name, sh, -64, 64, def, K_BIP); }
    void time(int p, const std::string &id, const std::string &name, const char *sh, float hi, float def)
    {
        at(p, id, name, sh, 0.0f, hi, def, K_TIME, 0.0f, 0.3f);
    }
};
} // namespace

static std::vector<ParamDef> build()
{
    B b;
    auto S = [](const char *pre, int n) { return std::string(pre) + std::to_string(n); };
    /* OSCILLATORS */
    for (int o = 0; o < 3; o++) {
        std::string id = S("osc", o + 1) + "_", nm = S("Osc ", o + 1) + " ";
        int p = pOsc(o, 0);
        b.ch(p + OS_MODE, id + "mode", nm + "Mode", "Mode", 2, 0, OSCMODE);
        b.at(p + OS_WAVE, id + "wave", nm + "Wave", "Wave", 0, NWAVE - 1, 3, K_WAVE, 1.0f);
        b.at(p + OS_SEMI, id + "semi", nm + "Semi", "Semi", -36, 36, 0, K_SEMI, 1.0f);
        b.at(p + OS_CENT, id + "cent", nm + "Cents", "Cents", -50, 50, 0, K_CENT, 1.0f);
        b.at(p + OS_KT, id + "keytrack", nm + "Keytrack", "Keytrack", 0, 200, 100, K_KT, 1.0f);
        b.ch(p + OS_BITS, id + "bitredux", nm + "BitRedux", "BitRedux", 13, 0, BITS_NAME);
        b.at(p + OS_SCAN, id + "wavescan", nm + "WaveScan", "WaveScan", 1.0f, 8.0f, 1.0f, K_NUM);
        for (int w = 0; w < 8; w++)
            b.at(p + OS_WAV1 + w, id + S("wav", w + 1), nm + S("WAV ", w + 1), "", -1, NWAVE - 1, w == 0 ? 3.0f : -2.0f, K_WAVE, 1.0f);
        std::snprintf(b.v[(size_t)(p + OS_WAV1)].shortName, 12, "WAV 1");
        for (int w = 1; w < 8; w++)
            std::snprintf(b.v[(size_t)(p + OS_WAV1 + w)].shortName, 12, "WAV %d", w + 1);
        /* WAV 1..8: -2 OFF, -1 Silence, 0.. the waves (WAV 1: a wave) */
        for (int w = 0; w < 8; w++)
            b.v[(size_t)(p + OS_WAV1 + w)].lo = w == 0 ? 0.0f : -2.0f;
    }
    /* MUTANTS */
    for (int m = 0; m < 4; m++) {
        std::string id = S("mut", m + 1) + "_", nm = S("Mutant ", m + 1) + " ";
        int p = pMut(m, 0);
        b.ch(p + MT_MODE, id + "mode", nm + "Mode", "Mode", NMUTMODE, 0, MUTMODE);
        b.ch(p + MT_FMSRC, id + "source", nm + "FM Source", "Source", NFMSRC, 0, FMSRC);
        b.ch(p + MT_SYNCSRC, id + "syncsrc", nm + "Sync Source", "Source", 3, m < 2 ? 0 : 1, SYNCSRC);
        b.at(p + MT_RATIO, id + "ratio", nm + "Ratio", "Ratio", 0.25f, 64.0f, 1.0f, K_RATIO, 0.0f, 0.3f);
        b.num(p + MT_DEPTH, id + "depth", nm + "Depth", "Depth", 0.0f);
        b.num(p + MT_WINDOW, id + "window", nm + "Window", "Window", 0.0f);
        b.at(p + MT_FB, id + "feedback", nm + "Feedback", "Feedback", 0, 150, 0, K_PCT);
        b.at(p + MT_WET, id + "drywet", nm + "Dry/Wet", "Dry/Wet", 0, 100, 100, K_PCT);
        for (int w = 0; w < 8; w++)
            b.num(p + MT_WARP1 + w, id + S("warp", w + 1), nm + S("Warp ", w + 1), "", 64.0f);
        for (int w = 0; w < 8; w++)
            std::snprintf(b.v[(size_t)(p + MT_WARP1 + w)].shortName, 12, "Warp%d", w + 1);
    }
    /* RING-NOISE */
    b.ch(P_RN_SRC1, "ring_src1", "Ring Source 1", "Source 1", NRINGSRC, 0, RINGSRC);
    b.ch(P_RN_SRC2, "ring_src2", "Ring Source 2", "Source 2", NRINGSRC, 1, RINGSRC);
    b.num(P_RN_DEPTH, "ring_depth", "RM Depth", "RM Depth", 128.0f);
    b.ch(P_RN_NOISE, "noise_type", "Noise Type", "NoisType", NNOISE, 0, NOISE);
    /* MIXER */
    static const char *const SRCN[5] = {"Osc 1", "Osc 2", "Osc 3", "Ring", "Noise"};
    static const char *const SRCI[5] = {"osc1", "osc2", "osc3", "ring", "noise"};
    static const char *const VSH[5] = {"Osc1Vol", "Osc2Vol", "Osc3Vol", "RingVol", "NoiseVol"};
    static const char *const PSH[5] = {"Osc1Pan", "Osc2Pan", "Osc3Pan", "RingPan", "NoizPan"};
    static const char *const FSH[5] = {"Osc1Filt", "Osc2Filt", "Osc3Filt", "RingFilt", "NoizFilt"};
    for (int i = 0; i < 5; i++) {
        b.num(P_MX_VOL1 + i, std::string("mix_") + SRCI[i] + "_vol", std::string(SRCN[i]) + " Vol", VSH[i], i == 0 ? 128.0f : 0.0f);
        b.at(P_MX_PAN1 + i, std::string("mix_") + SRCI[i] + "_pan", std::string(SRCN[i]) + " Pan", PSH[i], -64, 64, 0, K_PAN);
        b.at(P_MX_FR1 + i, std::string("mix_") + SRCI[i] + "_filt", std::string(SRCN[i]) + " Filter Ratio", FSH[i], 0, 128, 0, K_FRAT);
    }
    b.ch(P_MX_ROUTE, "filter_route", "Filter Routing", "Routing", 2, 0, ROUTE);
    /* FILTER 1 */
    b.ch(P_F1_TYPE, "f1_type", "Filter 1 Type", "Type", NF1TYPE, F1_LDR12, F1TYPE);
    b.num(P_F1_CTRL, "f1_control", "Filter 1 Control", "Control", 64.0f);
    b.num(P_F1_CUT, "f1_cutoff", "Filter 1 Cutoff", "Cutoff", 128.0f);
    b.num(P_F1_RES, "f1_reso", "Filter 1 Resonance", "Res", 0.0f);
    b.bip(P_F1_ENV, "f1_env1", "Filter 1 Env 1 Amount", "ENV1amt", 0.0f);
    b.bip(P_F1_LFO, "f1_lfo1", "Filter 1 LFO 1 Amount", "LFO1amt", 0.0f);
    b.bip(P_F1_VEL, "f1_velenv", "Filter 1 Vel Env", "Vel Env", 0.0f);
    b.at(P_F1_KT, "f1_keytrack", "Filter 1 Keytrack", "Keytrack", -200, 200, 0, K_KT, 1.0f);
    b.num(P_F1_DRIVE, "f1_drive", "Filter 1 Drive", "Drive", 0.0f);
    b.ch(P_F1_DRVPOST, "f1_driveroute", "Filter 1 Drive Route", "DrvRoute", 2, 0, DRVROUTE);
    b.ch(P_F1_VOWORD, "f1_voworder", "Filter 1 Vowel Order", "VowOrder", 8, 0, VOWORD);
    /* FILTER 2 */
    b.ch(P_F2_TYPE, "f2_type", "Filter 2 Type", "Type", 2, 0, F2TYPE);
    b.num(P_F2_MORPH, "f2_morph", "Filter 2 Morph", "Morph", 0.0f);
    b.num(P_F2_CUT, "f2_cutoff", "Filter 2 Cutoff", "Cutoff", 128.0f);
    b.num(P_F2_RES, "f2_reso", "Filter 2 Resonance", "Res", 0.0f);
    b.bip(P_F2_ENV, "f2_env1", "Filter 2 Env 1 Amount", "ENV1amt", 0.0f);
    b.bip(P_F2_LFO, "f2_lfo1", "Filter 2 LFO 1 Amount", "LFO1amt", 0.0f);
    b.bip(P_F2_VEL, "f2_velenv", "Filter 2 Vel Env", "Vel Env", 0.0f);
    b.at(P_F2_KT, "f2_keytrack", "Filter 2 Keytrack", "Keytrack", -200, 200, 0, K_KT, 1.0f);
    /* AMP */
    b.bip(P_AMP_LFO2, "amp_lfo2", "Amp LFO 2 Amount", "LFO2amt", 0.0f);
    b.bip(P_AMP_VEL, "amp_velocity", "Amp Velocity", "Velocity", 32.0f);
    b.num(P_AMP_LEVEL, "amp_level", "Amp Level", "Level", 96.0f);
    /* PRE-FX, POST-FX */
    for (int f = 0; f < 2; f++) {
        std::string id = f ? "postfx_" : "prefx_", nm = f ? "Post-FX " : "Pre-FX ";
        int p = pFx(f, 0);
        b.ch(p + FX_TYPE, id + "type", nm + "Type", "FX Type", NFXTYPE, 0, FXTYPE);
        b.at(p + FX_PRESET, id + "preset", nm + "Preset", "Preset", 0, 6, 0, K_INT, 1.0f);
        for (int k = 0; k < 6; k++)
            b.at(p + FX_P1 + k, id + S("p", k + 1), nm + S("Param ", k + 1), "", 0, 1, 0.5f, K_FXP);
        b.at(p + FX_WET, id + "drywet", nm + "Dry/Wet", "Dry/Wet", 0, 100, 50, K_PCT);
    }
    /* DELAY */
    b.ch(P_DL_TYPE, "dly_type", "Delay Type", "Type", NDLYTYPE, DT_STEREO, DLYTYPE);
    b.at(P_DL_TIME, "dly_time", "Delay Time", "Time", 1, 3000, 375, K_MS, 0.0f, 0.4f);
    b.onoff(P_DL_BPM, "dly_bpm", "Delay BPM Sync", "BPM Sync", 0);
    b.at(P_DL_SYNC, "dly_sync", "Delay Sync Time", "Time", 0, 20, 12, K_SYNC, 1.0f);
    b.num(P_DL_FB, "dly_feedback", "Delay Feedback", "Feedback", 48.0f);
    b.bip(P_DL_WTONE, "dly_wettone", "Delay Wet Tone", "Wet Tone", 0.0f);
    b.num(P_DL_FTONE, "dly_feedtone", "Delay Feed Tone", "FeedTone", 80.0f);
    b.at(P_DL_WET, "dly_drywet", "Delay Dry/Wet", "Dry/Wet", 0, 100, 0, K_PCT);
    /* REVERB */
    b.ch(P_RV_TYPE, "rev_type", "Reverb Type", "Type", NREVTYPE, RT_HALL, REVTYPE);
    b.at(P_RV_PRE, "rev_predelay", "Reverb PreDelay", "PreDelay", 0.5f, 250.0f, 12.0f, K_MS, 0.0f, 0.5f);
    b.at(P_RV_TIME, "rev_time", "Reverb Time", "Time", 0.12f, 90.0f, 3.2f, K_RVTIME, 0.0f, 0.25f);
    b.onoff(P_RV_FREEZE, "rev_freeze", "Reverb Freeze", "Freeze", 0);
    b.bip(P_RV_TONE, "rev_tone", "Reverb Tone", "Tone", 0.0f);
    b.num(P_RV_HIDAMP, "rev_hidamp", "Reverb Hi Damp", "Hi Damp", 48.0f);
    b.num(P_RV_LODAMP, "rev_lodamp", "Reverb Lo Damp", "Lo Damp", 16.0f);
    b.at(P_RV_WET, "rev_drywet", "Reverb Dry/Wet", "Dry/Wet", 0, 100, 0, K_PCT);
    /* ENVELOPES */
    for (int e = 0; e < 5; e++) {
        std::string id = S("env", e + 1) + "_", nm = S("Env ", e + 1) + " ";
        int p = pEnv(e, 0);
        bool amp = e == 1;
        b.time(p + EN_DLY, id + "delay", nm + "Delay", "Delay", 32.0f, 0.0f);
        b.time(p + EN_ATK, id + "attack", nm + "Attack", "Attack", 36.0f, 0.0f);
        b.time(p + EN_HLD, id + "hold", nm + "Hold", "Hold", 36.0f, 0.0f);
        b.time(p + EN_DEC, id + "decay", nm + "Decay", "Decay", 60.0f, amp ? 0.0f : 0.8f);
        b.num(p + EN_SUS, id + "sustain", nm + "Sustain", "Sustain", amp ? 128.0f : 0.0f);
        b.time(p + EN_REL, id + "release", nm + "Release", "Release", 60.0f, amp ? 0.12f : 0.5f);
        b.onoff(p + EN_BPM, id + "bpm", nm + "BPM Sync", "BPM Sync", 0);
        b.at(p + EN_QUANT, id + "quantize", nm + "Quantize", "Quantize", 0, 32, 0, K_INT, 1.0f);
        b.bip(p + EN_ACRV, id + "atkcurve", nm + "Attack Curve", "AtkCurve", 0.0f);
        b.bip(p + EN_DCRV, id + "deccurve", nm + "Decay Curve", "DecCurve", 32.0f);
        b.bip(p + EN_RCRV, id + "relcurve", nm + "Release Curve", "RelCurve", 32.0f);
        b.onoff(p + EN_LEGATO, id + "legato", nm + "Legato", "Legato", 0);
        b.onoff(p + EN_RESET, id + "reset", nm + "Reset", "Reset", 1);
        b.onoff(p + EN_FREE, id + "freerun", nm + "Freerun", "Freerun", 0);
        b.at(p + EN_LOOP, id + "loop", nm + "Loop", "Env Loop", 0, 50, 0, K_INT, 1.0f);
        for (int t = 0; t < 4; t++)
            b.ch(p + EN_TRIG1 + t, id + S("trig", t + 1), nm + S("Trigger Source ", t + 1), "", NTRIGSRC, t == 0 ? TR_NOTE : TR_OFF, TRIGSRC);
        for (int t = 0; t < 4; t++)
            std::snprintf(b.v[(size_t)(p + EN_TRIG1 + t)].shortName, 12, "TrigSrc%d", t + 1);
    }
    /* LFOS */
    for (int l = 0; l < 5; l++) {
        std::string id = S("lfo", l + 1) + "_", nm = S("LFO ", l + 1) + " ";
        int p = pLfo(l, 0);
        b.ch(p + LF_WAVE, id + "wave", nm + "Wave", "Wave", NLFOWAVE, LW_SINE, LFOWAVE);
        b.at(p + LF_RATE, id + "rate", nm + "Rate", "Rate", 0.02f, 150.0f, 1.0f, K_HZ, 0.0f, 0.25f);
        b.onoff(p + LF_BPM, id + "bpm", nm + "BPM Sync", "BPM Sync", 0);
        b.at(p + LF_SYNC, id + "syncrate", nm + "Sync Rate", "Rate", 0, NSYNC - 1, 13, K_SYNC, 1.0f);
        b.ch(p + LF_TRIG, id + "trigsync", nm + "TrigSync", "TrigSync", 3, TS_POLY, TRIGSYNC);
        b.time(p + LF_DLY, id + "delay", nm + "Delay", "Delay", 32.0f, 0.0f);
        b.at(p + LF_FADE, id + "fadein", nm + "Fade In", "Fade In", 0.0f, 5.943f, 0.0f, K_TIME, 0.0f, 0.4f);
        b.at(p + LF_PHASE, id + "phase", nm + "Phase", "Phase", 0, 360, 0, K_DEG, 1.0f);
        b.num(p + LF_LEVEL, id + "level", nm + "Level", "Level", 128.0f);
        b.at(p + LF_STEPS, id + "steps", nm + "Steps", "Steps", 2, 64, 8, K_INT, 1.0f);
        b.at(p + LF_SMOOTH, id + "smooth", nm + "Smooth", "Smooth", 0, 127, 0, K_INT, 1.0f);
        b.at(p + LF_QUANT, id + "quantize", nm + "Quantize", "Quantize", 0, 32, 0, K_INT, 1.0f);
        b.ch(p + LF_ONESHOT, id + "oneshot", nm + "One-Shot", "One-Shot", 3, OS_OFF, ONESHOT);
        b.onoff(p + LF_SEMILOCK, id + "semilock", nm + "SemiLock", "SemiLock", 0);
    }
    /* VOICE */
    b.ch(P_VO_POLY, "polyphony", "Polyphony", "Polyphny", NPOLY, PO_POLY, POLY);
    b.at(P_VO_DENSITY, "density", "Unison Density", "Density", 1, 8, 4, K_INT, 1.0f);
    b.at(P_VO_DETUNE, "detune", "Unison Detune", "Detune", 0, 127, 24, K_INT, 1.0f);
    b.at(P_VO_AFEEL, "analogfeel", "Analog Feel", "AnalogFL", 0, 127, 0, K_INT, 1.0f);
    b.onoff(P_VO_RPHASE, "randomphase", "Random Phase", "RandPhas", 0);
    b.ch(P_VO_STMODE, "stereomode", "Stereo Mode", "StMode", 3, SM_ROTATE, STMODE);
    b.at(P_VO_WIDTH, "stereowidth", "Stereo Width", "StWidth", 0, 127, 0, K_INT, 1.0f);
    b.onoff(P_VO_WARM, "warmmode", "Warm Mode", "WarmMode", 0);
    b.at(P_VO_PB, "pitchbend", "Pitch Bend Range", "PitchBnd", 0, 24, 2, K_INT, 1.0f);
    b.at(P_VO_VIBAMT, "vibrato_amt", "Vibrato Amount", "Vib Amt", 0, 12, 0.6f, K_NUM);
    b.onoff(P_VO_VIBBPM, "vibrato_bpm", "Vibrato BPM", "Vib BPM", 0);
    b.at(P_VO_VIBRATE, "vibrato_rate", "Vibrato Rate", "Vib Rate", 0.3f, 10.0f, 5.5f, K_HZ, 0.0f, 0.6f);
    b.at(P_VO_VIBSYNC, "vibrato_sync", "Vibrato Sync Rate", "Vib Rate", 5, 13, 10, K_SYNC, 1.0f);
    b.ch(P_VO_GLIDE, "glide", "Glide", "Glide", 3, GL_OFF, GLIDE);
    b.at(P_VO_GTIME, "glide_time", "Glide Time", "GlidTime", 0, 127, 40, K_INT, 1.0f);
    b.bip(P_VO_GCURVE, "glide_curve", "Glide Curve", "GlidCurv", 0.0f);
    b.onoff(P_VO_GLEGATO, "glide_legato", "Glide Legato", "GlidLgto", 0);
    b.ch(P_VO_KEY, "keylock", "Key Lock", "Key Lock", 12, 0, KEYS);
    b.ch(P_VO_SCALE, "scale", "Scale", "Scale", NSCALE, 0, SCALE_NAME);
    b.ch(P_VO_SUSPED, "suspedal", "Sustain Pedal", "SusPedal", 3, SP_SUSTAIN, SUSPED);
    b.onoff(P_VO_SNAP, "snap", "Snap", "Snap", 0);
    for (int v = 0; v < NVMOD; v++)
        b.at(P_VO_VMOD1 + v, S("voicemod", v + 1), S("VoiceMod ", v + 1), "", -128, 128, 0, K_NUM);
    for (int v = 0; v < NVMOD; v++)
        std::snprintf(b.v[(size_t)(P_VO_VMOD1 + v)].shortName, 12, "Voice %d", v + 1);
    /* ARP */
    b.onoff(P_AR_ON, "arp_on", "Arp On", "Arp", 0);
    b.onoff(P_AR_LATCH, "arp_latch", "Arp Latch", "Latch", 0);
    b.at(P_AR_TEMPO, "arp_tempo", "Tempo", "Tempo", 30, 300, 120, K_BPM, 0.1f);
    b.ch(P_AR_DIV, "arp_division", "Arp Division", "Division", NARPDIV, 7, ARPDIV_NAME);
    b.at(P_AR_SWING, "arp_swing", "Arp Swing", "Swing", 50, 75, 50, K_PCT, 1.0f);
    b.at(P_AR_GATE, "arp_gate", "Arp Gate", "Gate", 5, 100, 50, K_PCT, 1.0f);
    b.ch(P_AR_OCTMODE, "arp_octmode", "Arp Octave Mode", "OctMode", NARPOCT, AO_UP, ARPOCT);
    b.at(P_AR_OCT, "arp_octave", "Arp Octave", "Octave", 1, 6, 1, K_INT, 1.0f);
    b.ch(P_AR_MODE, "arp_mode", "Arp Mode", "Mode", NARPMODE, AM_UP, ARPMODE);
    b.at(P_AR_LEN, "arp_length", "Arp Length", "Length", 0, 32, 0, K_INT, 1.0f);
    b.at(P_AR_PHRASE, "arp_phrase", "Arp Phrase", "Phrase", 1, 64, 1, K_INT, 1.0f);
    b.ch(P_AR_RATCHET, "arp_ratchet", "Arp Ratchet", "Ratchet", 4, 0, RATCHET);
    b.at(P_AR_CHANCE, "arp_chance", "Arp Chance", "Chance", 0, 100, 0, K_PCT, 1.0f);
    b.at(P_AR_OFFSET, "arp_offset", "Arp Step Offset", "StpOfset", -32, 32, 0, K_INT, 1.0f);
    b.onoff(P_AR_CLKLOCK, "arp_clklock", "Arp Clock Lock", "ClkLock", 1);
    /* MACROS */
    for (int m = 0; m < NMACRO; m++) {
        b.num(P_MC_VAL1 + m, S("macro", m + 1), S("Macro ", m + 1), "", 0.0f);
        std::snprintf(b.v[(size_t)(P_MC_VAL1 + m)].shortName, 12, "Macro %d", m + 1);
        b.onoff(P_MC_BTN1 + m, S("macro", m + 1) + "_btn", S("Macro ", m + 1) + " Button", "Button", 0);
        b.at(P_MC_NAME1 + m, S("macro", m + 1) + "_name", S("Macro ", m + 1) + " Name", "Name", 0, NMACRONAME - 1, 0, K_CHOICE, 1.0f, 1.0f, MACRO_NAME);
        for (int d = 0; d < NMACRODST; d++) {
            std::string id = S("macro", m + 1) + S("_dst", d + 1), nm = S("Macro ", m + 1) + S(" Dest ", d + 1);
            b.at(pMacroDst(m, d, MR_DST), id, nm, "", 0, 0, 0, K_DST, 1.0f);
            b.at(pMacroDst(m, d, MR_DEPTH), id + "_depth", nm + " Depth", "Depth", -128, 128, 0, K_NUM);
            b.at(pMacroDst(m, d, MR_BTN), id + "_btn", nm + " Button Value", "BtnValue", -128, 128, 0, K_NUM);
            std::snprintf(b.v[(size_t)pMacroDst(m, d, MR_DST)].shortName, 12, "Des %d", d + 1);
        }
    }
    /* MOD MATRIX */
    for (int s = 0; s < NMOD; s++) {
        std::string id = S("mod", s + 1) + "_", nm = S("Mod ", s + 1) + " ";
        b.at(pMod(s, MM_SRC), id + "src", nm + "Source", "Source", 0, NSRC - 1, 0, K_SRC, 1.0f);
        b.at(pMod(s, MM_DST), id + "dst", nm + "Destination", "Dest", 0, 0, 0, K_DST, 1.0f);
        b.at(pMod(s, MM_DEPTH), id + "depth", nm + "Depth", "Depth", -128, 128, 0, K_NUM);
    }

    /* the destinations' ends, and each parameter's destination */
    const auto &D = dests();
    for (auto &d : b.v)
        if (d.kind == K_DST)
            d.hi = (float)(D.size() - 1);
    for (size_t i = 1; i < D.size(); i++)
        if (D[i].param >= 0)
            b.v[(size_t)D[i].param].dst = (int)i;
    for (auto &d : b.v)
        if (d.id[0] == 0)
            std::snprintf(d.id, sizeof d.id, "unused");
    return b.v;
}

/* the destinations, the manual's list and order */
static std::vector<DstDef> buildDst()
{
    std::vector<DstDef> D;
    auto add = [&](const char *g, const std::string &n, int p, int sp = DS_PARAM) {
        DstDef d;
        std::snprintf(d.group, sizeof d.group, "%s", g);
        std::snprintf(d.name, sizeof d.name, "%s", n.c_str());
        d.param = p;
        d.special = sp;
        D.push_back(d);
    };
    auto S = [](const char *pre, int n) { return std::string(pre) + std::to_string(n); };
    add("", "Off", -1, -1);
    add("Arp", "Mode", P_AR_MODE); add("Arp", "Division", P_AR_DIV); add("Arp", "Swing", P_AR_SWING);
    add("Arp", "Gate", P_AR_GATE); add("Arp", "Octave", P_AR_OCT); add("Arp", "OctMode", P_AR_OCTMODE);
    add("Arp", "Length", P_AR_LEN); add("Arp", "Phrase", P_AR_PHRASE); add("Arp", "Ratchet", P_AR_RATCHET);
    add("Arp", "Chance", P_AR_CHANCE);
    static const char *const OG[3] = {"Osc 1", "Osc 2", "Osc 3"};
    for (int o = 0; o < 3; o++) {
        add(OG[o], "Pitch", -1, DS_PITCH1 + o);
        add(OG[o], "Wave", pOsc(o, OS_WAVE));
        if (o < 2)
            add(OG[o], "WaveScan", pOsc(o, OS_SCAN));
    }
    add("All Osc", "Pitch", -1, DS_PITCHALL);
    static const char *const MG[4] = {"Mutant 1", "Mutant 2", "Mutant 3", "Mutant 4"};
    for (int m = 0; m < 4; m++) {
        add(MG[m], "Ratio", pMut(m, MT_RATIO)); add(MG[m], "Depth", pMut(m, MT_DEPTH));
        add(MG[m], "Window", pMut(m, MT_WINDOW)); add(MG[m], "Feedback", pMut(m, MT_FB));
        add(MG[m], "Dry/Wet", pMut(m, MT_WET));
        for (int w = 0; w < 8; w++)
            add(MG[m], S("Warp ", w + 1), pMut(m, MT_WARP1 + w));
    }
    add("Ring Mod", "Depth", P_RN_DEPTH);
    static const char *const VN[5] = {"Osc1Vol", "Osc2Vol", "Osc3Vol", "RingVol", "NoiseVol"};
    static const char *const PN[5] = {"Osc1Pan", "Osc2Pan", "Osc3Pan", "RingPan", "NoisePan"};
    static const char *const FN[5] = {"Osc1F1/2", "Osc2F1/2", "Osc3F1/2", "RingF1/2", "NoisF1/2"};
    for (int i = 0; i < 5; i++) add("Mixer", VN[i], P_MX_VOL1 + i);
    for (int i = 0; i < 5; i++) add("Mixer", PN[i], P_MX_PAN1 + i);
    for (int i = 0; i < 5; i++) add("Mixer", FN[i], P_MX_FR1 + i);
    add("Filter 1", "Cutoff", P_F1_CUT); add("Filter 1", "Resonance", P_F1_RES); add("Filter 1", "Drive", P_F1_DRIVE);
    add("Filter 1", "Control", P_F1_CTRL); add("Filter 1", "ENV1amt", P_F1_ENV); add("Filter 1", "LFO1amt", P_F1_LFO);
    add("Filter 1", "Keytrack", P_F1_KT);
    add("Filter 2", "Morph", P_F2_MORPH); add("Filter 2", "Cutoff", P_F2_CUT); add("Filter 2", "Resonance", P_F2_RES);
    add("Filter 2", "ENV1amt", P_F2_ENV); add("Filter 2", "LFO1amt", P_F2_LFO); add("Filter 2", "Keytrack", P_F2_KT);
    add("Amp", "LFO2Amt", P_AMP_LFO2); add("Amp", "Level", P_AMP_LEVEL);
    add("Pre-FX", "Param1", pFx(0, FX_P1)); add("Pre-FX", "Param2", pFx(0, FX_P1 + 1)); add("Pre-FX", "Dry/Wet", pFx(0, FX_WET));
    add("Delay", "Time", P_DL_TIME); add("Delay", "Feedback", P_DL_FB); add("Delay", "Wet Tone", P_DL_WTONE);
    add("Delay", "FeedTone", P_DL_FTONE); add("Delay", "Dry/Wet", P_DL_WET);
    add("Reverb", "Time", P_RV_TIME); add("Reverb", "Tone", P_RV_TONE); add("Reverb", "HiDamp", P_RV_HIDAMP);
    add("Reverb", "LoDamp", P_RV_LODAMP); add("Reverb", "Dry/Wet", P_RV_WET);
    add("Post-FX", "Param1", pFx(1, FX_P1)); add("Post-FX", "Param2", pFx(1, FX_P1 + 1)); add("Post-FX", "Dry/Wet", pFx(1, FX_WET));
    static const char *const EG[5] = {"Env 1", "Env 2", "Env 3", "Env 4", "Env 5"};
    for (int e = 0; e < 5; e++) {
        add(EG[e], "Attack", pEnv(e, EN_ATK)); add(EG[e], "Hold", pEnv(e, EN_HLD)); add(EG[e], "Decay", pEnv(e, EN_DEC));
        add(EG[e], "Sustain", pEnv(e, EN_SUS)); add(EG[e], "Release", pEnv(e, EN_REL));
    }
    static const char *const LG[5] = {"LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5"};
    for (int l = 0; l < 5; l++) {
        add(LG[l], "Rate", pLfo(l, LF_RATE)); add(LG[l], "Level", pLfo(l, LF_LEVEL));
    }
    for (int s = 0; s < NMOD; s++)
        add("ModMtrx", S("Depth ", s + 1), pMod(s, MM_DEPTH));
    for (int m = 0; m < NMACRO; m++)
        add("Macro", S("Macro ", m + 1), P_MC_VAL1 + m);
    add("Voice", "Detune", P_VO_DETUNE); add("Voice", "AnalogFL", P_VO_AFEEL); add("Voice", "PitchBnd", P_VO_PB);
    add("Voice", "Vib Amt", P_VO_VIBAMT); add("Voice", "Vib Rate", P_VO_VIBRATE); add("Voice", "GlidTime", P_VO_GTIME);
    return D;
}

/* -------------------------------------------------------- normalisation --- */
float toNorm(int p, float v)
{
    const ParamDef &d = pdef(p);
    if (d.hi <= d.lo)
        return 0.0f;
    float n = (v - d.lo) / (d.hi - d.lo);
    n = n < 0 ? 0 : n > 1 ? 1 : n;
    if (d.skew != 1.0f && n > 0.0f)
        n = std::pow(n, d.skew);
    return n;
}
float fromNorm(int p, float n)
{
    const ParamDef &d = pdef(p);
    n = n < 0 ? 0 : n > 1 ? 1 : n;
    if (d.skew != 1.0f && n > 0.0f)
        n = std::pow(n, 1.0f / d.skew);
    float v = d.lo + (d.hi - d.lo) * n;
    if (d.step > 0.0f)
        v = d.lo + std::round((v - d.lo) / d.step) * d.step;
    return v;
}

/* ----------------------------------------------------------------- text --- */
static void timeText(float s, char *o, int n)
{
    if (s < 0.0005f)
        std::snprintf(o, (size_t)n, "0 ms");
    else if (s < 1.0f)
        std::snprintf(o, (size_t)n, "%d ms", (int)std::lround(s * 1000.0f));
    else if (s < 10.0f)
        std::snprintf(o, (size_t)n, "%.2f s", s);
    else
        std::snprintf(o, (size_t)n, "%.1f s", s);
}
void paramText(int p, float v, char *o, int n, bool bpm)
{
    const ParamDef &d = pdef(p);
    switch (d.kind) {
    case K_CHOICE:
    case K_ONOFF: {
        int i = (int)std::lround(v);
        i = i < (int)d.lo ? (int)d.lo : i > (int)d.hi ? (int)d.hi : i;
        std::snprintf(o, (size_t)n, "%s", d.choice ? d.choice[i] : "?");
        break;
    }
    case K_NUM: std::snprintf(o, (size_t)n, "%.1f", v); break;
    case K_BIP: std::snprintf(o, (size_t)n, "%+.1f", v); break;
    case K_INT: std::snprintf(o, (size_t)n, "%d", (int)std::lround(v)); break;
    case K_PCT: std::snprintf(o, (size_t)n, "%d%%", (int)std::lround(v)); break;
    case K_SEMI: std::snprintf(o, (size_t)n, "%+d", (int)std::lround(v)); break;
    case K_CENT: std::snprintf(o, (size_t)n, "%+d ct", (int)std::lround(v)); break;
    case K_KT: std::snprintf(o, (size_t)n, "%d%%", (int)std::lround(v)); break;
    case K_RATIO: std::snprintf(o, (size_t)n, "%.3f", v); break;
    case K_TIME:
        if (bpm) {
            int i = (int)std::lround(toNorm(p, v) * NSYNC);
            std::snprintf(o, (size_t)n, "%s", i <= 0 ? "0" : SYNC_NAME[i - 1]);
        } else
            timeText(v, o, n);
        break;
    case K_HZ: std::snprintf(o, (size_t)n, v < 10.0f ? "%.2f Hz" : "%.1f Hz", v); break;
    case K_PAN: {
        int l = (int)std::lround(64.0f - v), r = (int)std::lround(64.0f + v);
        std::snprintf(o, (size_t)n, "%d:%d", l * 100 / 128, r * 100 / 128);
        if (std::fabs(v) < 0.05f)
            std::snprintf(o, (size_t)n, "Center");
        break;
    }
    case K_FRAT: {
        int a = (int)std::lround(100.0f - v * 100.0f / 128.0f);
        std::snprintf(o, (size_t)n, "%d:%d", a, 100 - a);
        break;
    }
    case K_MS:
        if (v >= 1000.0f)
            std::snprintf(o, (size_t)n, "%.2f s", v / 1000.0f);
        else
            std::snprintf(o, (size_t)n, v < 10.0f ? "%.1f ms" : "%.0f ms", (double)v);
        break;
    case K_DEG: std::snprintf(o, (size_t)n, "%d deg", (int)std::lround(v)); break;
    case K_BPM: std::snprintf(o, (size_t)n, "%.1f", v); break;
    case K_FXP: std::snprintf(o, (size_t)n, "%.1f", v * 128.0f); break;
    case K_SYNC: {
        int i = (int)std::lround(v);
        i = i < 0 ? 0 : i >= NSYNC ? NSYNC - 1 : i;
        std::snprintf(o, (size_t)n, "%s", SYNC_NAME[i]);
        break;
    }
    case K_WAVE: {
        int i = (int)std::lround(v);
        std::snprintf(o, (size_t)n, "%s", i == -2 ? "OFF" : i == -1 ? "Silence" : waveName(i));
        break;
    }
    case K_SRC: std::snprintf(o, (size_t)n, "%s", srcName((int)std::lround(v))); break;
    case K_DST: {
        int i = (int)std::lround(v);
        const auto &D = dests();
        if (i <= 0 || i >= (int)D.size())
            std::snprintf(o, (size_t)n, "Off");
        else
            std::snprintf(o, (size_t)n, "%s %s", D[(size_t)i].group, D[(size_t)i].name);
        break;
    }
    case K_RVTIME:
        if (v < 1.0f)
            std::snprintf(o, (size_t)n, "%d ms", (int)std::lround(v * 1000.0f));
        else
            std::snprintf(o, (size_t)n, "%.1f s", v);
        break;
    default: std::snprintf(o, (size_t)n, "%.2f", v); break;
    }
}

/* ------------------------------------------------------------ the INIT --- */
void patchInit(Patch &p)
{
    std::memset(&p, 0, sizeof p);
    std::snprintf(p.name, NAME_LEN, "Init");
    std::snprintf(p.category, sizeof p.category, "Init");
    for (int i = 0; i < NPARAM; i++)
        p.v[i] = pdef(i).def;
    for (int l = 0; l < 5; l++)
        for (int s = 0; s < NSTEP; s++)
            p.step[l][s] = s == 0 ? 60.0f : s == 1 ? -60.0f : 0.0f;
}
} // namespace hy
