/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
#include "PluginEditor.h"
#include "Fx.h"
#include "Waves.h"
#include <map>

using namespace hyui;
using namespace hy;
using Panel = HydratePanel;

namespace
{
/* the panel's places (W x H = 1960 x 620) */
constexpr float DX = -404;                      /* (MAIN SYSTEMS and MASTER CONTROL: from their first place) */
const juce::Rectangle<float> R_ARP(1004, 14, 506, 592), R_MAIN(666 + DX, 14, 240, 592), R_MC(918 + DX, 14, 478, 592),
    R_FC(14, 620, 486, 426), R_MS(512, 620, 998, 426), R_SETUP(22, 336, 226, 270), R_DISP(930 + DX, 210, 404, 158),
    R_SMALL(282, 204, 200, 112);
const float ARPX[4] = {1094, 1208, 1322, 1432};
const float MCX[4] = {976 + DX, 1078 + DX, 1180 + DX, 1282 + DX};
const float FCX[5] = {104, 254, 404, 154, 304};
const float FCY[5] = {738, 738, 738, 906, 906};
constexpr float FCS = 92;                       /* the filter knobs' size */
/* module select: the columns and rows of the diagram */
const float MSC[9] = {562, 670, 778, 896, 1010, 1122, 1232, 1344, 1452};
constexpr float MBW = 80, MBH = 48;
/* its rows: the envelopes, osc 1, filter 1, the mixer's line, filter 2, osc 3, the LFOs */
constexpr float Y_ENV = 690, Y_O1 = 760, Y_F1 = 788, Y_MID = 830, Y_F2 = 872, Y_O3 = 900, Y_LFO = 1000;
struct MsPlace {
    int module;
    float cx, cy;
    const char *text;
};
const MsPlace MSP[] = {
    {Panel::M_OSC1, MSC[0], Y_O1, "OSC\n1"},      {Panel::M_MUT1, MSC[1], Y_O1, "MUTANT\n1"},  {Panel::M_MUT2, MSC[2], Y_O1, "MUTANT\n2"},
    {Panel::M_OSC2, MSC[0], Y_MID, "OSC\n2"},      {Panel::M_MUT3, MSC[1], Y_MID, "MUTANT\n3"},  {Panel::M_MUT4, MSC[2], Y_MID, "MUTANT\n4"},
    {Panel::M_OSC3, MSC[0], Y_O3, "OSC\n3"},      {Panel::M_RING, MSC[2], Y_O3, "RING-\nNOISE"}, {Panel::M_MIXER, MSC[3], Y_MID, "MIXER"},
    {Panel::M_F1, MSC[4], Y_F1, "FILTER\n1"},     {Panel::M_F2, MSC[4], Y_F2, "FILTER\n2"},    {Panel::M_AMP, MSC[5], Y_MID, "AMP"},
    {Panel::M_PREFX, MSC[6], Y_MID, "PRE-FX"},     {Panel::M_DELAY, MSC[7], Y_F1, "DELAY"},     {Panel::M_REVERB, MSC[7], Y_F2, "REVERB"},
    {Panel::M_POSTFX, MSC[8], Y_MID, "POST-FX"},
    {Panel::M_ENV1, MSC[4], Y_ENV, "ENV\n1"},      {Panel::M_ENV1 + 1, MSC[5], Y_ENV, "ENV\n2"}, {Panel::M_ENV1 + 2, MSC[6], Y_ENV, "ENV\n3"},
    {Panel::M_ENV1 + 3, MSC[7], Y_ENV, "ENV\n4"},  {Panel::M_ENV1 + 4, MSC[8], Y_ENV, "ENV\n5"},
    {Panel::M_LFO1, MSC[4], Y_LFO, "LFO\n1"},      {Panel::M_LFO1 + 1, MSC[5], Y_LFO, "LFO\n2"}, {Panel::M_LFO1 + 2, MSC[6], Y_LFO, "LFO\n3"},
    {Panel::M_LFO1 + 3, MSC[7], Y_LFO, "LFO\n4"},  {Panel::M_LFO1 + 4, MSC[8], Y_LFO, "LFO\n5"}};
juce::Rectangle<float> msRect(const MsPlace &m) { return {m.cx - MBW / 2, m.cy - MBH / 2, MBW, MBH}; }

bool bipolarKind(const ParamDef &d)
{
    return d.kind == K_BIP || d.kind == K_PAN || d.kind == K_SEMI || d.kind == K_CENT || (d.lo < 0 && d.hi > 0 && d.kind != K_WAVE);
}
} // namespace

/* =========================================================================== entries === */
Panel::Entry Panel::param(int p, const juce::String &label, bool bpmText)
{
    Entry e;
    const ParamDef &d = pdef(p);
    e.param = p;
    e.label = label.isNotEmpty() ? label : juce::String(d.shortName);
    /* a time shown as a division while its module's BPM Sync is on */
    int bpmP = -1;
    if (bpmText) {
        if (p >= P_ENV1 && p < P_ENV1 + 5 * EN_N)
            bpmP = pEnv((p - P_ENV1) / EN_N, EN_BPM);
        else if (p >= P_LFO1 && p < P_LFO1 + 5 * LF_N)
            bpmP = pLfo((p - P_LFO1) / LF_N, LF_BPM);
    }
    e.text = [this, p, bpmP] {
        char t[48];
        paramText(p, proc.value(p), t, sizeof t, bpmP >= 0 && proc.value(bpmP) > 0.5f);
        return juce::String(t);
    };
    e.getN = [this, p] { return toNorm(p, proc.value(p)); };
    e.setN = [this, p](float n) { proc.setValueInGesture(p, fromNorm(p, n)); };
    e.begin = [this, p] { proc.beginEdit(p); };
    e.end = [this, p] { proc.endEdit(p); };
    e.stepN = d.step > 0.0f && d.hi > d.lo ? d.step / (d.hi - d.lo) : 0.0f;
    e.defN = toNorm(p, d.def);
    e.bip = bipolarKind(d);
    if (d.kind == K_ONOFF) {
        e.press = [this, p] { proc.setValue(p, proc.value(p) > 0.5f ? 0.0f : 1.0f); };
        e.lit = [this, p] { return proc.value(p) > 0.5f; };
    }
    return e;
}
Panel::Entry Panel::fxEntry(int post, int k)
{
    int type = (int)proc.value(pFx(post, FX_TYPE));
    Entry e;
    const char *nm = fxParamName(type, k);
    if (!nm || !nm[0])
        return e;
    int p = pFx(post, FX_P1 + k);
    e = param(p, nm);
    e.text = [this, p, post, k] {
        char t[48];
        fxParamText((int)proc.value(pFx(post, FX_TYPE)), k, proc.value(p), t, sizeof t);
        return juce::String(t);
    };
    e.bip = false;
    return e;
}
Panel::Entry Panel::stepEntry(int l, int s)
{
    Entry e;
    e.label = "Step" + juce::String(s + 1);
    bool semi = proc.value(pLfo(l, LF_SEMILOCK)) > 0.5f;
    e.text = [this, l, s, semi] {
        float v = proc.stepValue(l, s);
        if (semi) {
            int st = (int)std::lround(v / 5.0f);
            bool exact = std::fabs(v - (float)st * 5.0f) < 0.05f;
            return juce::String(st > 0 ? "+" : "") + juce::String(st) + "semi" + (exact ? "" : "*");
        }
        return juce::String(v, 1);
    };
    e.getN = [this, l, s] { return (proc.stepValue(l, s) + 64.0f) / 128.0f; };
    e.setN = [this, l, s, semi](float n) {
        float v = n * 128.0f - 64.0f;
        if (semi)
            v = std::round(v / 5.0f) * 5.0f;
        proc.setStep(l, s, v);
    };
    e.stepN = semi ? 5.0f / 128.0f : 1.0f / 128.0f;
    e.defN = 0.5f;
    e.bip = true;
    return e;
}

int Panel::pages()
{
    auto V = [&](int p) { return proc.value(p); };
    switch (module) {
    case M_OSC1: case M_OSC2: return V(pOsc(module - M_OSC1, OS_MODE)) > 0.5f ? 2 : 1;
    case M_MUT1: case M_MUT2: case M_MUT3: case M_MUT4: return (int)V(pMut(module - M_MUT1, MT_MODE)) == MU_PWASM ? 2 : 1;
    case M_MIXER: return 3;
    case M_F1: return 2;
    case M_PREFX: case M_POSTFX: return (int)V(pFx(module == M_POSTFX, FX_TYPE)) == FX_COMP ? 2 : 1;
    case M_VOICE: return 4;
    case M_ARP: return 2;
    case M_MACRO: return macroSel < 0 ? 1 : 4;
    case M_MOD: return NMOD / 4;
    default: break;
    }
    if (module >= M_ENV1 && module < M_ENV1 + 5)
        return 3;
    if (module >= M_LFO1 && module < M_LFO1 + 5) {
        int l = module - M_LFO1;
        if ((int)V(pLfo(l, LF_WAVE)) == LW_STEP)
            return 2 + ((int)V(pLfo(l, LF_STEPS)) + 7) / 8;
        return 2;
    }
    return 1;
}

std::vector<Panel::Entry> Panel::entries()
{
    std::vector<Entry> e(8);
    auto V = [&](int p) { return proc.value(p); };
    int pg = juce::jlimit(0, juce::jmax(0, pages() - 1), page);
    switch (module) {
    case M_HOME:
        for (int m = 0; m < NMACRO; m++) {
            Entry x = param(P_MC_VAL1 + m);
            x.label = MACRO_NAME[juce::jlimit(0, NMACRONAME - 1, (int)V(P_MC_NAME1 + m))];
            bool any = false;
            for (int j = 0; j < NMACRODST; j++)
                any = any || V(pMacroDst(m, j, MR_DST)) > 0.5f;
            if (!any)
                x.text = [] { return juce::String("-"); };
            x.press = [this, m] { proc.setValue(P_MC_BTN1 + m, proc.value(P_MC_BTN1 + m) > 0.5f ? 0.0f : 1.0f); };
            x.lit = [this, m] { return proc.value(P_MC_BTN1 + m) > 0.5f; };
            e[(size_t)m] = x;
        }
        break;
    case M_OSC1: case M_OSC2: case M_OSC3: {
        int o = module - M_OSC1;
        bool scan = o < 2 && V(pOsc(o, OS_MODE)) > 0.5f;
        if (pg == 0) {
            if (o < 2)
                e[0] = param(pOsc(o, OS_MODE));
            e[1] = param(scan ? pOsc(o, OS_SCAN) : pOsc(o, OS_WAVE));
            e[2] = param(pOsc(o, OS_SEMI));
            e[3] = param(pOsc(o, OS_CENT));
            if (scan) {
                Entry w;
                w.label = "Wavelist";
                w.text = [] { return juce::String("Edit..."); };
                w.press = [this] { page = 1; };
                e[5] = w;
            }
            e[6] = param(pOsc(o, OS_BITS));
            e[7] = param(pOsc(o, OS_KT));
        } else
            for (int w = 0; w < 8; w++)
                e[(size_t)w] = param(pOsc(o, OS_WAV1 + w));
        break;
    }
    case M_MUT1: case M_MUT2: case M_MUT3: case M_MUT4: {
        int m = module - M_MUT1, mode = (int)V(pMut(m, MT_MODE));
        if (pg == 0) {
            e[0] = param(pMut(m, MT_MODE));
            if (mode == MU_FM)
                e[1] = param(pMut(m, MT_FMSRC));
            if (mode == MU_SYNC)
                e[1] = param(pMut(m, MT_SYNCSRC));
            bool ratio = mode != MU_STACK && mode != MU_PHAZ && !(mode == MU_FM && (int)V(pMut(m, MT_FMSRC)) > FS_TRI);
            if (ratio)
                e[2] = param(pMut(m, MT_RATIO));
            e[3] = param(pMut(m, MT_DEPTH));
            if (mode == MU_SYNC)
                e[4] = param(pMut(m, MT_WINDOW));
            if (mode != MU_STACK)
                e[5] = param(pMut(m, MT_FB));
            if (mode == MU_PWASM) {
                Entry w;
                w.label = "Custom";
                w.text = [] { return juce::String("Edit..."); };
                w.press = [this] { page = 1; };
                e[6] = w;
            }
            e[7] = param(pMut(m, MT_WET));
        } else
            for (int w = 0; w < 8; w++)
                e[(size_t)w] = param(pMut(m, MT_WARP1 + w));
        break;
    }
    case M_RING:
        e[0] = param(P_RN_SRC1);
        e[1] = param(P_RN_SRC2);
        e[2] = param(P_RN_DEPTH);
        e[3] = param(P_MX_VOL1 + 3, "Ring Vol");
        e[4] = param(P_RN_NOISE);
        e[5] = param(P_MX_VOL1 + 4, "Noise Vol");
        break;
    case M_MIXER:
        if (pg == 0) {
            e[0] = param(P_MX_VOL1); e[1] = param(P_MX_VOL1 + 1); e[2] = param(P_MX_VOL1 + 2);
            e[4] = param(P_MX_VOL1 + 3); e[5] = param(P_MX_VOL1 + 4);
        } else if (pg == 1) {
            e[0] = param(P_MX_PAN1); e[1] = param(P_MX_PAN1 + 1); e[2] = param(P_MX_PAN1 + 2);
            e[4] = param(P_MX_FR1); e[5] = param(P_MX_FR1 + 1); e[6] = param(P_MX_FR1 + 2);
        } else {
            e[0] = param(P_MX_PAN1 + 3); e[1] = param(P_MX_PAN1 + 4); e[3] = param(P_MX_ROUTE);
            e[4] = param(P_MX_FR1 + 3); e[5] = param(P_MX_FR1 + 4);
        }
        break;
    case M_F1: {
        bool vow = (int)V(P_F1_TYPE) == F1_VOWEL;
        if (pg == 0) {
            e[0] = param(P_F1_TYPE);
            if (vow)
                e[1] = param(P_F1_CTRL);
            e[2] = param(P_F1_CUT, vow ? "Vowel" : "Cutoff");
            e[3] = param(P_F1_RES);
            e[4] = param(P_F1_ENV); e[5] = param(P_F1_LFO); e[6] = param(P_F1_VEL); e[7] = param(P_F1_KT);
        } else {
            e[0] = param(P_MX_ROUTE, "Route");
            e[2] = param(P_F1_DRIVE);
            e[3] = param(P_F1_DRVPOST);
            if (vow)
                e[4] = param(P_F1_VOWORD);
        }
        break;
    }
    case M_F2:
        e[0] = param(P_F2_TYPE); e[1] = param(P_F2_MORPH); e[2] = param(P_F2_CUT); e[3] = param(P_F2_RES);
        e[4] = param(P_F2_ENV); e[5] = param(P_F2_LFO); e[6] = param(P_F2_VEL); e[7] = param(P_F2_KT);
        break;
    case M_AMP:
        e[5] = param(P_AMP_LFO2); e[6] = param(P_AMP_VEL); e[7] = param(P_AMP_LEVEL);
        break;
    case M_PREFX: case M_POSTFX: {
        int post = module == M_POSTFX;
        int type = (int)V(pFx(post, FX_TYPE));
        if (pg == 0) {
            e[0] = param(pFx(post, FX_TYPE));
            if (type != FX_BYPASS && fxPresetCount(type) > 1) {
                int pp = pFx(post, FX_PRESET);
                Entry x = param(pp, "Preset");
                int n = fxPresetCount(type);
                x.text = [this, pp, post] {
                    int t = (int)proc.value(pFx(post, FX_TYPE));
                    return juce::String(fxPresetName(t, juce::jlimit(0, fxPresetCount(t) - 1, (int)proc.value(pp))));
                };
                x.getN = [this, pp, n] { return juce::jlimit(0.0f, 1.0f, proc.value(pp) / (float)juce::jmax(1, n - 1)); };
                x.setN = [this, pp, n, post](float v) {
                    int i = juce::jlimit(0, n - 1, (int)std::lround(v * (float)(n - 1)));
                    if (i == (int)proc.value(pp))
                        return;
                    proc.setValueInGesture(pp, (float)i);
                    /* a preset: its template into the parameters */
                    float p6[6], wet;
                    fxPreset((int)proc.value(pFx(post, FX_TYPE)), i, p6, wet);
                    for (int k = 0; k < 6; k++)
                        proc.setValue(pFx(post, FX_P1 + k), p6[k]);
                };
                x.stepN = 1.0f / (float)juce::jmax(1, n - 1);
                e[1] = x;
            }
            for (int k = 0; k < 5; k++)
                e[(size_t)(2 + k)] = fxEntry(post, k);
            if (type != FX_BYPASS)
                e[7] = param(pFx(post, FX_WET));
        } else
            e[0] = fxEntry(post, 5);
        break;
    }
    case M_DELAY: {
        bool bpm = V(P_DL_BPM) > 0.5f;
        e[0] = param(P_DL_TYPE); e[1] = param(bpm ? P_DL_SYNC : P_DL_TIME, "Time"); e[2] = param(P_DL_FB);
        e[3] = param(P_DL_WTONE); e[5] = param(P_DL_BPM); e[6] = param(P_DL_FTONE); e[7] = param(P_DL_WET);
        break;
    }
    case M_REVERB:
        e[0] = param(P_RV_TYPE); e[1] = param(P_RV_PRE); e[2] = param(P_RV_TIME); e[3] = param(P_RV_TONE);
        e[4] = param(P_RV_FREEZE); e[5] = param(P_RV_HIDAMP); e[6] = param(P_RV_LODAMP); e[7] = param(P_RV_WET);
        break;
    case M_VOICE:
        if (pg == 0) {
            e[0] = param(P_VO_POLY); e[1] = param(P_VO_DENSITY); e[2] = param(P_VO_DETUNE); e[3] = param(P_VO_AFEEL);
            e[4] = param(P_VO_RPHASE); e[5] = param(P_VO_STMODE); e[6] = param(P_VO_WIDTH); e[7] = param(P_VO_WARM);
        } else if (pg == 1) {
            e[0] = param(P_VO_PB); e[1] = param(P_VO_VIBAMT); e[2] = param(P_VO_VIBBPM);
            e[3] = param(V(P_VO_VIBBPM) > 0.5f ? P_VO_VIBSYNC : P_VO_VIBRATE, "Vib Rate");
            e[4] = param(P_VO_GLIDE);
            if (V(P_VO_GLIDE) > 0.5f) {
                e[5] = param(P_VO_GTIME); e[6] = param(P_VO_GCURVE); e[7] = param(P_VO_GLEGATO);
            }
        } else if (pg == 2) {
            e[0] = param(P_VO_KEY); e[1] = param(P_VO_SCALE); e[4] = param(P_VO_SUSPED);
            Entry w;
            w.label = "VoiceMod";
            w.text = [] { return juce::String("Edit..."); };
            w.press = [this] { page = 3; };
            e[6] = w;
            e[7] = param(P_VO_SNAP);
        } else
            for (int v = 0; v < NVMOD; v++)
                e[(size_t)v] = param(P_VO_VMOD1 + v);
        break;
    case M_ARP:
        if (pg == 0) {
            e[0] = param(P_AR_TEMPO); e[1] = param(P_AR_DIV); e[2] = param(P_AR_SWING); e[3] = param(P_AR_GATE);
            e[4] = param(P_AR_OCTMODE); e[5] = param(P_AR_OCT); e[6] = param(P_AR_MODE); e[7] = param(P_AR_LEN);
            e[7].text = [this] { int l = (int)proc.value(P_AR_LEN); return l <= 0 ? juce::String("Default") : juce::String(l); };
        } else {
            e[1] = param(P_AR_PHRASE); e[2] = param(P_AR_RATCHET); e[3] = param(P_AR_CHANCE);
            e[4] = param(P_AR_CLKLOCK); e[5] = param(P_AR_OFFSET); e[6] = param(P_AR_LATCH); e[7] = param(P_AR_ON, "Arp");
        }
        break;
    case M_MACRO:
        if (macroSel < 0) {
            for (int m = 0; m < NMACRO; m++) {
                Entry x;
                x.label = MACRO_NAME[juce::jlimit(0, NMACRONAME - 1, (int)V(P_MC_NAME1 + m))] + juce::String(" ") + juce::String(m + 1);
                x.text = [] { return juce::String("Edit..."); };
                x.press = [this, m] {
                    macroSel = m;
                    page = 0;
                };
                e[(size_t)m] = x;
            }
        } else {
            int m = macroSel;
            if (pg <= 1)
                for (int i = 0; i < 4; i++) {
                    int d = pg * 4 + i;
                    e[(size_t)i] = param(pMacroDst(m, d, MR_DST), "Des " + juce::String(d + 1));
                    e[(size_t)(4 + i)] = param(pMacroDst(m, d, MR_DEPTH), "Depth " + juce::String(d + 1));
                }
            else if (pg == 2)
                for (int d = 0; d < 8; d++)
                    e[(size_t)d] = param(pMacroDst(m, d, MR_BTN), "BtnVal " + juce::String(d + 1));
            else {
                e[0] = param(P_MC_NAME1 + m, "Name");
                e[1] = param(P_MC_VAL1 + m, "Knob");
                e[2] = param(P_MC_BTN1 + m, "Button");
            }
        }
        break;
    case M_MOD:
        for (int i = 0; i < 4; i++) {
            int s = pg * 4 + i;
            int ps = pMod(s, MM_SRC), pd = pMod(s, MM_DST);
            Entry x;
            x.label = "Mod " + juce::String(s + 1);
            x.text = [this, ps] { return juce::String(srcName((int)proc.value(ps))); };
            x.text2 = [this, pd] {
                char t[48];
                paramText(pd, proc.value(pd), t, sizeof t);
                return juce::String(t);
            };
            x.editSecond = modDst[s];
            int p = modDst[s] ? pd : ps;
            x.param = p;
            x.getN = [this, p] { return toNorm(p, proc.value(p)); };
            x.setN = [this, p](float n) { proc.setValueInGesture(p, fromNorm(p, n)); };
            x.begin = [this, p] { proc.beginEdit(p); };
            x.end = [this, p] { proc.endEdit(p); };
            x.stepN = 1.0f / juce::jmax(1.0f, pdef(p).hi - pdef(p).lo);
            x.press = [this, s] { modDst[s] = !modDst[s]; };
            x.lit = [this, s] { return modDst[s]; };
            e[(size_t)i] = x;
            e[(size_t)(4 + i)] = param(pMod(s, MM_DEPTH), "Depth " + juce::String(s + 1));
        }
        break;
    default:
        if (module >= M_ENV1 && module < M_ENV1 + 5) {
            int en = module - M_ENV1;
            if (pg == 0) {
                e[0] = param(pEnv(en, EN_ATK), {}, true); e[1] = param(pEnv(en, EN_DEC), {}, true);
                e[2] = param(pEnv(en, EN_SUS)); e[3] = param(pEnv(en, EN_REL), {}, true);
                e[4] = param(pEnv(en, EN_DLY), {}, true); e[5] = param(pEnv(en, EN_HLD), {}, true);
                e[6] = param(pEnv(en, EN_QUANT)); e[7] = param(pEnv(en, EN_BPM));
                e[6].text = [this, en] { int q = (int)proc.value(pEnv(en, EN_QUANT)); return q < 2 ? juce::String("Off") : juce::String(q); };
            } else if (pg == 1) {
                e[0] = param(pEnv(en, EN_ACRV)); e[1] = param(pEnv(en, EN_DCRV)); e[3] = param(pEnv(en, EN_RCRV));
                e[4] = param(pEnv(en, EN_LEGATO)); e[5] = param(pEnv(en, EN_RESET)); e[6] = param(pEnv(en, EN_FREE));
                e[7] = param(pEnv(en, EN_LOOP));
                e[7].text = [this, en] {
                    int l = (int)proc.value(pEnv(en, EN_LOOP));
                    return l <= 0 ? juce::String("Off") : l >= 50 ? juce::String("Infinite") : juce::String(l + 1);
                };
            } else {
                for (int t = 0; t < 4; t++)
                    if (!(en == 1 && t == 0))
                        e[(size_t)t] = param(pEnv(en, EN_TRIG1 + t));
                if (en == 1) {
                    Entry x;
                    x.label = "TrigSrc1";
                    x.text = [] { return juce::String("Note On"); };
                    e[0] = x;
                    e[0].getN = [] { return 0.0f; };
                }
                Entry tap;
                tap.label = "Tap Trig";
                tap.text = [] { return juce::String("Tap"); };
                tap.press = [this, en] { proc.tapEnvelope(en); };
                e[4] = tap;
            }
        } else if (module >= M_LFO1 && module < M_LFO1 + 5) {
            int l = module - M_LFO1;
            bool bpm = V(pLfo(l, LF_BPM)) > 0.5f, step = (int)V(pLfo(l, LF_WAVE)) == LW_STEP;
            if (pg == 0) {
                e[0] = param(pLfo(l, LF_WAVE)); e[1] = param(bpm ? pLfo(l, LF_SYNC) : pLfo(l, LF_RATE), "Rate");
                e[2] = param(pLfo(l, LF_BPM)); e[3] = param(pLfo(l, LF_TRIG)); e[4] = param(pLfo(l, LF_DLY), {}, true);
                e[5] = param(pLfo(l, LF_FADE), {}, true); e[6] = param(pLfo(l, LF_PHASE)); e[7] = param(pLfo(l, LF_LEVEL));
            } else if (pg == 1) {
                if (step)
                    e[0] = param(pLfo(l, LF_STEPS));
                e[1] = param(pLfo(l, LF_SMOOTH)); e[2] = param(pLfo(l, LF_QUANT)); e[3] = param(pLfo(l, LF_ONESHOT));
                e[2].text = [this, l] { int q = (int)proc.value(pLfo(l, LF_QUANT)); return q < 2 ? juce::String("Off") : juce::String(q); };
                if (step) {
                    e[6] = param(pLfo(l, LF_SEMILOCK));
                    Entry x;
                    x.label = "Step Edit";
                    x.text = [] { return juce::String("Edit..."); };
                    x.press = [this] { page = 2; };
                    e[7] = x;
                }
            } else {
                int n = (int)V(pLfo(l, LF_STEPS));
                for (int i = 0; i < 8; i++) {
                    int s = (pg - 2) * 8 + i;
                    if (s < n)
                        e[(size_t)i] = stepEntry(l, s);
                }
            }
        }
        break;
    }
    return e;
}

juce::String Panel::moduleName(Module m) const
{
    static const char *const N[] = {"HOME", "OSC 1", "OSC 2", "OSC 3", "MUTANT 1", "MUTANT 2", "MUTANT 3", "MUTANT 4", "RING-NOISE",
                                    "MIXER", "FILTER 1", "FILTER 2", "AMP", "PRE-FX", "DELAY", "REVERB", "POST-FX", "ENV 1",
                                    "ENV 2", "ENV 3", "ENV 4", "ENV 5", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "VOICE",
                                    "ARP EDIT", "MACRO ASSIGN", "MOD MATRIX"};
    return N[m];
}
void Panel::open(Module m, int pg)
{
    if (m == module && pg == 0 && m != M_HOME && m != M_MACRO)
        page = (page + 1) % juce::jmax(1, pages());   /* (the module's button again: its next page) */
    else {
        module = m;
        page = pg;
    }
    touched = -1;
    repaint();
}

/* ============================================================================ build === */
Panel::Ctl &Panel::knob(int p, float cx, float cy, float size, const juce::String &label)
{
    Ctl c;
    c.kind = Ctl::KNOB;
    c.r = {cx - size / 2, cy - size / 2, size, size};
    c.label = label;
    if (p >= 0) {
        c.hp = proc.param(p);
        Entry e = param(p);
        c.getN = e.getN;
        c.setN = e.setN;
        c.begin = e.begin;
        c.end = e.end;
        c.stepN = e.stepN;
        c.defN = e.defN;
        c.bip = e.bip;
    }
    ctls.push_back(c);
    return ctls.back();
}
Panel::Ctl &Panel::button(juce::Rectangle<float> r, const juce::String &text, std::function<void()> click, std::function<bool()> on,
                          Lit lit, float textSize)
{
    Ctl c;
    c.kind = Ctl::BUTTON;
    c.r = r;
    c.label = text;
    c.click = std::move(click);
    c.on = std::move(on);
    c.lit = lit;
    c.textSize = textSize;
    ctls.push_back(c);
    return ctls.back();
}

void Panel::build()
{
    ctls.clear();
    ctls.reserve(160);
    auto *P = &proc;
    /* MASTER VOLUME */
    {
        auto *vol = proc.apvts.getParameter("volume");
        Ctl &c = knob(-1, 134, 268, 66, "MASTER VOLUME");
        c.hp = vol;
        c.getN = [vol] { return vol->getValue(); };
        c.setN = [vol](float n) { vol->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, n)); };
        c.begin = [vol] { vol->beginChangeGesture(); };
        c.end = [vol] { vol->endChangeGesture(); };
        c.defN = 0.7f;
    }
    /* VOICE SPREAD: the voices apart in the stereo field, a little in pitch and filter */
    {
        auto *sp = proc.apvts.getParameter("spread");
        Ctl &c = knob(-1, 134, 172, 66, "VOICE SPREAD");
        c.hp = sp;
        c.getN = [sp] { return sp->getValue(); };
        c.setN = [sp](float n) { sp->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, n)); };
        c.begin = [sp] { sp->beginChangeGesture(); };
        c.end = [sp] { sp->endChangeGesture(); };
        c.defN = 0.0f;
    }
    /* SETUP */
    auto apv = [P](const char *id) { return P->apvts.getParameter(id); };
    auto setAp = [](juce::RangedAudioParameter *q, float n) {
        q->beginChangeGesture();
        q->setValueNotifyingHost(n);
        q->endChangeGesture();
    };
    auto *voices = apv("voices"), *quality = apv("quality"), *hostSync = apv("host_sync");
    button({44, 392, 82, 32}, "8 VOICES", [=] { setAp(voices, 0.0f); }, [voices] { return voices->getValue() < 0.5f; }, Lit::white, 11.5f).hp = voices;
    button({142, 392, 82, 32}, "16 VOICES", [=] { setAp(voices, 1.0f); }, [voices] { return voices->getValue() >= 0.5f; }, Lit::white, 11.5f).hp = voices;
    button({44, 456, 82, 32}, "ECO", [=] { setAp(quality, 0.0f); }, [quality] { return quality->getValue() < 0.5f; }, Lit::white, 11.5f).hp = quality;
    button({142, 456, 82, 32}, "HIGH", [=] { setAp(quality, 1.0f); }, [quality] { return quality->getValue() >= 0.5f; }, Lit::white, 11.5f).hp = quality;
    button({44, 520, 82, 32}, "HOST\nSYNC", [=] { setAp(hostSync, hostSync->getValue() > 0.5f ? 0.0f : 1.0f); },
           [hostSync] { return hostSync->getValue() > 0.5f; }, Lit::blue, 11.0f).hp = hostSync;
    button({142, 520, 82, 32}, "PANIC", [P] { P->panicNow(); }, [] { return false; }, Lit::orange, 11.5f);

    /* ARPEGGIATOR */
    knob(P_AR_TEMPO, ARPX[0], 118, 64, "TEMPO");
    knob(P_AR_RATCHET, ARPX[1], 118, 64, "RATCHET");
    knob(P_AR_CHANCE, ARPX[2], 118, 64, "CHANCE");
    knob(P_AR_GATE, ARPX[3], 118, 64, "GATE");
    knob(P_AR_MODE, ARPX[0], 330, 60, "MODE");
    knob(P_AR_OCT, ARPX[1], 330, 60, "OCTAVE");
    knob(P_AR_DIV, ARPX[2], 330, 60, "DIVISION");
    knob(P_AR_SWING, ARPX[3], 330, 60, "SWING");
    auto tog = [P](int p) { return [P, p] { P->setValue(p, P->value(p) > 0.5f ? 0.0f : 1.0f); }; };
    auto isOn = [P](int p) { return [P, p] { return P->value(p) > 0.5f; }; };
    button({ARPX[0] - 32, 448, 64, 32}, "ON", tog(P_AR_ON), isOn(P_AR_ON), Lit::blue, 12.0f).hp = P->param(P_AR_ON);
    button({ARPX[0] - 24, 490, 48, 20}, "EDIT", [this] { open(M_ARP); }, [this] { return module == M_ARP; }, Lit::orange, 10.5f);
    button({ARPX[1] - 32, 448, 64, 32}, "LATCH", tog(P_AR_LATCH), isOn(P_AR_LATCH), Lit::blue, 12.0f).hp = P->param(P_AR_LATCH);
    button({ARPX[2] - 32, 448, 64, 32}, "TRIPLET", [P] {
        static const int STRAIGHT[] = {0, 1, 1, 3, 3, 5, 5, 7, 7, 9, 9, 11}, TRIP[] = {2, 2, 2, 4, 4, 6, 6, 8, 8, 10, 10, 10};
        int d = juce::jlimit(0, NARPDIV - 1, (int)P->value(P_AR_DIV));
        bool trip = d == 2 || d == 4 || d == 6 || d == 8 || d == 10;
        P->setValue(P_AR_DIV, (float)(trip ? STRAIGHT[d] : TRIP[d]));
    }, [P] { int d = (int)P->value(P_AR_DIV); return d == 2 || d == 4 || d == 6 || d == 8 || d == 10; }, Lit::blue, 12.0f).hp = P->param(P_AR_DIV);
    button({ARPX[3] - 32, 448, 64, 32}, "TAP\nTEMPO", [this, P] {
        double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        if (!taps.empty() && now - taps.back() > 2.0)
            taps.clear();
        taps.push_back(now);
        if (taps.size() > 4)
            taps.erase(taps.begin());
        if (taps.size() >= 3) {
            double span = (taps.back() - taps.front()) / (double)(taps.size() - 1);
            P->setValue(P_AR_TEMPO, (float)juce::jlimit(30.0, 300.0, 60.0 / span));
        }
    }, [P] { return P->beatNow.load(); }, Lit::blue, 11.0f);

    /* MAIN SYSTEMS: one centre line, two equal columns below the display */
    {
        Ctl c;
        c.kind = Ctl::ENCODER;
        c.r = {382 - 52, 116 - 52, 104, 104};
        ctls.push_back(c);
    }
    constexpr float CL = 282, CR = 388, CW = 94, FW = 200;
    button({CL, 334, CW, 32}, juce::String::fromUTF8("\xe2\x97\x80"), [this] { stepProgram(-1); }, [] { return false; }, Lit::white, 13.0f);
    button({CR, 334, CW, 32}, juce::String::fromUTF8("\xe2\x96\xb6"), [this] { stepProgram(1); }, [] { return false; }, Lit::white, 13.0f);
    button({CL, 396, CW, 32}, "BROWSE", [this] {
        auto r = getScreenBounds();
        programMenu(r.withSize(10, 10).translated((int)(282.0f * getTransform().mat00), (int)(430 * getTransform().mat00)));
    }, [] { return false; }, Lit::orange, 11.5f);
    button({CR, 396, CW, 32}, "SAVE", [this] { saveDialog(); }, [] { return false; }, Lit::orange, 11.5f);
    button({CL, 440, CW, 32}, "INIT", [this] {
        auto sc = getTransform().mat00;
        auto o = getScreenBounds().getTopLeft();
        initMenu({o.x + (int)(282 * sc), o.y + (int)(472 * sc), (int)(94 * sc), 2}, false);
    }, [] { return false; }, Lit::white, 11.5f);
    button({CR, 440, CW, 32}, "RANDOM", [this] {
        /* one press, one new sound; with SHIFT on a module page: only that module */
        if (juce::ModifierKeys::currentModifiers.isShiftDown() && module != M_HOME && module != M_MACRO && module != M_MOD) {
            randomModule();
            return;
        }
        proc.loadPatch(randomPatch(juce::Random::getSystemRandom().nextInt()));
        repaint();
    }, [] { return false; }, Lit::white, 11.5f);
    button({CL, 486, FW, 40}, "HOME", [this] { open(M_HOME); }, [this] { return module == M_HOME; }, Lit::white, 15.0f);
    {
        auto *ls = apv("low_split");
        button({CL, 540, FW, 32}, "LOW SPLIT", [=] {
            bool on = ls->getValue() < 0.5f;
            setAp(ls, on ? 1.0f : 0.0f);
            P->splitLearn.store(on);              /* on: the next key you play is the split */
        }, [ls, P] {
            if (P->splitLearn.load())             /* blinking while it waits for the key */
                return (juce::Time::getMillisecondCounter() / 300) % 2 == 0;
            return ls->getValue() > 0.5f;
        }, Lit::blue, 12.0f).hp = ls;
    }

    /* MASTER CONTROL: eight encoders, eight buttons, the display's fields */
    for (int i = 0; i < 8; i++) {
        float cx = MCX[i % 4], cy = i < 4 ? 92.0f : 512.0f;
        Ctl c;
        c.kind = Ctl::MKNOB;
        c.index = i;
        c.r = {cx - 31, cy - 31, 62, 62};
        c.ring = true;
        ctls.push_back(c);
        Ctl b;
        b.kind = Ctl::BUTTON;
        b.index = i;
        b.r = {cx - 30, i < 4 ? 154.0f : 408.0f, 60, 26};
        b.lit = Lit::white;
        b.click = [this, i] {
            auto es = entries();
            if (es[(size_t)i].press) {
                es[(size_t)i].press();
                touched = i;
                repaint();
            }
        };
        b.on = [this, i] {
            auto es = entries();
            const Entry &e = es[(size_t)i];
            return e.lit ? e.lit() : e.valid();
        };
        ctls.push_back(b);
        Ctl cell;
        cell.kind = Ctl::CELL;
        cell.index = i;
        cell.r = {R_DISP.getX() + 2 + (float)(i % 4) * 100.0f, R_DISP.getY() + 18.0f + (i < 4 ? 0.0f : 70.0f), 100, 68};
        ctls.push_back(cell);
    }
    float rx = 1342 + DX;
    button({rx, 66, 46, 30}, "EXIT", [this] {
        if (module == M_MACRO && macroSel >= 0)
            macroSel = -1, page = 0;
        else if (page > 0)
            page = 0;
        else
            module = M_HOME;
        repaint();
    }, [] { return false; }, Lit::white, 11.5f);
    button({rx, 136, 46, 30}, "VOICE", [this] { open(M_VOICE); }, [this] { return module == M_VOICE; }, Lit::orange, 11.5f);
    button({rx, 222, 46, 38}, juce::String::fromUTF8("\xe2\x96\xb2"), [this] {
        page = (page + pages() - 1) % pages();
        repaint();
    }, [] { return false; }, Lit::white, 14.0f);
    button({rx, 268, 46, 38}, juce::String::fromUTF8("\xe2\x96\xbc"), [this] {
        page = (page + 1) % pages();
        repaint();
    }, [] { return false; }, Lit::white, 14.0f);
    button({rx, 404, 46, 40}, "MACRO\nASSIGN", [this] {
        macroSel = -1;
        open(M_MACRO);
    }, [this] { return module == M_MACRO; }, Lit::orange, 10.0f);
    button({rx, 486, 46, 40}, "MOD\nMATRIX", [this] { open(M_MOD); }, [this] { return module == M_MOD; }, Lit::orange, 10.0f);

    /* FILTER CONTROLS: the five knobs on filter 1 or filter 2 */
    for (int k = 0; k < 5; k++) {
        static const char *const L[5] = {"CUTOFF", "RESONANCE", "DRIVE / MORPH", "ENV 1", "LFO 1"};
        Ctl &c = knob(-1, FCX[k], FCY[k], FCS, L[k]);
        auto pick = [this, k]() -> int {
            static const int F1[5] = {P_F1_CUT, P_F1_RES, P_F1_DRIVE, P_F1_ENV, P_F1_LFO};
            static const int F2[5] = {P_F2_CUT, P_F2_RES, P_F2_MORPH, P_F2_ENV, P_F2_LFO};
            return filterSel ? F2[k] : F1[k];
        };
        c.getN = [this, pick] { int p = pick(); return toNorm(p, proc.value(p)); };
        c.setN = [this, pick](float n) { int p = pick(); proc.setValueInGesture(p, fromNorm(p, n)); };
        c.begin = [this, pick] { proc.beginEdit(pick()); };
        c.end = [this, pick] { proc.endEdit(pick()); };
        c.defN = k >= 3 ? 0.5f : k == 0 ? 1.0f : 0.0f;
        c.bip = k >= 3;
    }
    button({408, 864, 72, 44}, "FILTER\n1", [this] { filterSel = 0; repaint(); }, [this] { return filterSel == 0; }, Lit::blue, 12.5f);
    button({408, 922, 72, 44}, "FILTER\n2", [this] { filterSel = 1; repaint(); }, [this] { return filterSel == 1; }, Lit::blue, 12.5f);

    /* MODULE SELECT */
    for (const auto &m : MSP) {
        int mod = m.module;
        button(msRect(m), m.text, [this, mod] { open((Module)mod); }, [this, mod] { return module == mod; }, Lit::orange, 12.5f);
    }
}

/* ============================================================================ panel === */
HydratePanel::HydratePanel(HydrateProcessor &p) : proc(p)
{
    setLookAndFeel(&look);
    setOpaque(true);
    build();
    startTimerHz(30);
}
HydratePanel::~HydratePanel()
{
    stopTimer();
    if (saveWin != nullptr) {
        saveWin->exitModalState(0);
        saveWin.reset();
    }
    setLookAndFeel(nullptr);
}

void Panel::timerCallback()
{
    /* LOW SPLIT: the key the processor learned goes into the parameter (saved with the project) */
    if (int ln = proc.learnedSplit.load(); ln > 0) {
        auto *q = proc.apvts.getParameter("split_note");
        q->beginChangeGesture();
        q->setValueNotifyingHost(q->convertTo0to1((float)ln));
        q->endChangeGesture();
        proc.learnedSplit.store(0);
    }
    /* repaint when anything shown changed */
    double s = (juce::Time::getMillisecondCounter() < flashUntil ? 977.0 : 0.0) + (double)module * 7.0 + (double)page * 131.0 + (double)macroSel * 17.0 + (double)filterSel * 3.0 + touched * 0.37;
    int k = 0;
    for (auto &c : ctls) {
        k++;
        if (c.kind == Ctl::KNOB && c.getN)
            s += c.getN() * (double)(k * 13 % 97 + 1);
        else if (c.kind == Ctl::BUTTON && c.on)
            s += c.on() ? (double)(k % 53 + 1) * 1.7 : 0.0;
    }
    auto es = entries();
    for (size_t i = 0; i < es.size(); i++) {
        if (es[i].getN)
            s += es[i].getN() * (double)(i * 31 + 7);
        if (es[i].text)
            s += (double)es[i].text().hashCode() * 1e-9 * (double)(i + 1);
        if (es[i].text2)
            s += (double)es[i].text2().hashCode() * 1e-9 * (double)(i + 3);
    }
    for (int v = 0; v < MAXV; v++)
        s += std::floor(proc.voiceLevel[v].load() * 12.0f) * (double)(v + 1) * 0.11;
    s += (double)proc.arpStep.load() * 0.013;
    s += (double)proc.patchName().hashCode() * 1e-7;
    if (proc.voicesNow.load() > 0 || proc.scopeCopy(0) != 0.0f)
        s += proc.scopeCopy(100) * 1000.0 + proc.scopeCopy(300);
    for (int l = 0; l < 5; l++)
        s += proc.lfoNow[l].load() * 0.3;
    if (s != sig) {
        sig = s;
        repaint();
    }
}

int Panel::ctlAt(juce::Point<float> p) const
{
    for (int i = (int)ctls.size() - 1; i >= 0; i--) {
        const Ctl &c = ctls[(size_t)i];
        auto r = c.kind == Ctl::KNOB || c.kind == Ctl::MKNOB || c.kind == Ctl::ENCODER ? c.r.expanded(6.0f) : c.r;
        if (r.contains(p))
            return i;
    }
    return -1;
}

/* a control's host parameter: its own, or the parameter of the display field it stands for */
juce::AudioProcessorParameter *Panel::hostParamFor(const Ctl &c)
{
    if (c.hp)
        return c.hp;
    if ((c.kind == Ctl::MKNOB || c.kind == Ctl::CELL) && c.index >= 0) {
        auto es = entries();
        if ((size_t)c.index < es.size() && es[(size_t)c.index].param >= 0)
            return proc.param(es[(size_t)c.index].param);
    }
    return nullptr;
}

/* right click: the host's menu for that parameter (MIDI Learn, automation ...); never a change of value */
bool Panel::hostMenu(const juce::MouseEvent &ev)
{
    if (!ev.mods.isPopupMenu())
        return false;
    int i = ctlAt(ev.position);
    if (i >= 0)
        if (auto *prm = hostParamFor(ctls[(size_t)i]))
            if (auto *editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
                if (auto *ctx = editor->getHostContext())
                    if (auto menu = ctx->getContextMenuForParameter(prm))
                        menu->showNativeMenu(editor->getMouseXYRelative());
    return true;
}

void Panel::mouseDown(const juce::MouseEvent &ev)
{
    if (hostMenu(ev)) {
        active = -1;
        pressed = false;
        return;
    }
    active = ctlAt(ev.position);
    pressed = true;
    dragAcc = 0.0f;
    encAcc = 0;
    if (active < 0)
        return;
    Ctl &c = ctls[(size_t)active];
    switch (c.kind) {
    case Ctl::KNOB:
        if (c.getN) {
            dragStart = c.getN();
            if (c.begin)
                c.begin();
        }
        break;
    case Ctl::MKNOB: {
        auto es = entries();
        const Entry &e = es[(size_t)c.index];
        touched = c.index;
        if (e.getN) {
            dragStart = e.getN();
            if (e.begin)
                e.begin();
        }
        break;
    }
    case Ctl::BUTTON:
        if (c.click)
            c.click();
        break;
    case Ctl::CELL: {
        auto es = entries();
        const Entry &e = es[(size_t)c.index];
        touched = c.index;
        if (e.getN && e.param >= 0) {
            auto sc = getTransform().mat00;
            auto r = getScreenBounds().getTopLeft() + juce::Point<int>((int)(c.r.getX() * sc), (int)(c.r.getBottom() * sc));
            menuFor(e, juce::Rectangle<int>(r.x, r.y, (int)(c.r.getWidth() * sc), 2));
        } else if (e.press)
            e.press();
        break;
    }
    default: break;
    }
    repaint();
}

void Panel::mouseDrag(const juce::MouseEvent &ev)
{
    if (active < 0 || ev.mods.isPopupMenu())
        return;
    Ctl &c = ctls[(size_t)active];
    float dy = (float)(ev.getDistanceFromDragStartY());
    float span = ev.mods.isShiftDown() ? 1600.0f : 240.0f;
    if (c.kind == Ctl::KNOB && c.setN)
        c.setN(juce::jlimit(0.0f, 1.0f, dragStart - dy / span));
    else if (c.kind == Ctl::MKNOB) {
        auto es = entries();
        const Entry &e = es[(size_t)c.index];
        if (e.setN)
            e.setN(juce::jlimit(0.0f, 1.0f, dragStart - dy / span));
    } else if (c.kind == Ctl::ENCODER) {
        int steps = (int)(-dy / 24.0f);
        if (steps != encAcc) {
            stepProgram(steps - encAcc);
            encAcc = steps;
        }
    }
}

void Panel::mouseUp(const juce::MouseEvent &ev)
{
    if (ev.mods.isPopupMenu() && active < 0)
        return;
    if (active >= 0) {
        Ctl &c = ctls[(size_t)active];
        if (c.kind == Ctl::KNOB && c.end && c.getN)
            c.end();
        else if (c.kind == Ctl::MKNOB) {
            auto es = entries();
            if (es[(size_t)c.index].end)
                es[(size_t)c.index].end();
        }
    }
    active = -1;
    pressed = false;
    repaint();
}

void Panel::mouseDoubleClick(const juce::MouseEvent &ev)
{
    if (ev.mods.isPopupMenu())
        return;
    int i = ctlAt(ev.position);
    if (i < 0)
        return;
    Ctl &c = ctls[(size_t)i];
    if (c.kind == Ctl::KNOB && c.setN) {
        if (c.begin) c.begin();
        c.setN(c.defN);
        if (c.end) c.end();
    } else if (c.kind == Ctl::MKNOB) {
        auto es = entries();
        const Entry &e = es[(size_t)c.index];
        if (e.setN) {
            if (e.begin) e.begin();
            e.setN(e.param >= 0 ? toNorm(e.param, pdef(e.param).def) : e.defN);
            if (e.end) e.end();
        }
    }
}

void Panel::mouseWheelMove(const juce::MouseEvent &ev, const juce::MouseWheelDetails &w)
{
    int i = ctlAt(ev.position);
    if (i < 0)
        return;
    Ctl &c = ctls[(size_t)i];
    float dir = (w.deltaY != 0.0f ? w.deltaY : w.deltaX) > 0.0f ? 1.0f : -1.0f;
    if (w.isReversed)
        dir = -dir;
    auto turn = [&](const std::function<float()> &get, const std::function<void(float)> &set, const std::function<void()> &b,
                    const std::function<void()> &e, float stepN) {
        if (!get || !set)
            return;
        if (b) b();
        float st = stepN > 0.0f ? stepN : (ev.mods.isShiftDown() ? 0.002f : 0.012f);
        set(juce::jlimit(0.0f, 1.0f, get() + dir * st + (stepN > 0.0f ? dir * 0.001f : 0.0f)));
        if (e) e();
    };
    if (c.kind == Ctl::KNOB)
        turn(c.getN, c.setN, c.begin, c.end, c.stepN);
    else if (c.kind == Ctl::MKNOB || c.kind == Ctl::CELL) {
        auto es = entries();
        const Entry &e = es[(size_t)c.index];
        touched = c.index;
        turn(e.getN, e.setN, e.begin, e.end, e.stepN);
    } else if (c.kind == Ctl::ENCODER)
        stepProgram(dir > 0 ? 1 : -1);
}

/* ============================================================================ menus === */
void Panel::menuFor(const Entry &e, juce::Rectangle<int> screen) { paramMenu(e.param, screen); }

void Panel::paramMenu(int p, juce::Rectangle<int> screen)
{
    const ParamDef &d = pdef(p);
    juce::PopupMenu m;
    int cur = (int)std::lround(proc.value(p));
    auto set = [this, p](float v) { proc.setValue(p, v); };
    std::vector<float> vals;
    auto item = [&](juce::PopupMenu &menu, const juce::String &t, float v) {
        vals.push_back(v);
        menu.addItem((int)vals.size(), t, true, (int)std::lround(v) == cur);
    };
    switch (d.kind) {
    case K_WAVE: {
        if (d.lo < -1.5f)
            item(m, "OFF", -2.0f);
        if (d.lo < -0.5f)
            item(m, "Silence", -1.0f);
        for (int g = 0; g < NWGROUP; g++) {
            juce::PopupMenu sub;
            for (int w = WGROUP[g].first; w < WGROUP[g].first + WGROUP[g].count; w++)
                item(sub, waveName(w), (float)w);
            m.addSubMenu(WGROUP[g].name, sub, true, nullptr, cur >= WGROUP[g].first && cur < WGROUP[g].first + WGROUP[g].count);
        }
        break;
    }
    case K_DST: {
        const auto &D = dests();
        item(m, "Off", 0.0f);
        juce::String grp;
        juce::PopupMenu sub;
        bool tick = false;
        for (size_t i = 1; i <= D.size(); i++) {
            juce::String g = i < D.size() ? juce::String(D[i].group) : juce::String();
            if (g != grp) {
                if (grp.isNotEmpty())
                    m.addSubMenu(grp, sub, true, nullptr, tick);
                sub = juce::PopupMenu();
                tick = false;
                grp = g;
            }
            if (i < D.size()) {
                item(sub, D[i].name, (float)i);
                tick = tick || (int)i == cur;
            }
        }
        break;
    }
    case K_SRC: {
        for (int s = 0; s < SRC_CC0; s++)
            item(m, srcName(s), (float)s);
        for (int b = 0; b < 4; b++) {
            juce::PopupMenu sub;
            for (int c = b * 32; c < b * 32 + 32; c++)
                item(sub, srcName(SRC_CC0 + c), (float)(SRC_CC0 + c));
            m.addSubMenu("CC " + juce::String(b * 32) + "-" + juce::String(b * 32 + 31), sub);
        }
        break;
    }
    case K_CHOICE:
    case K_ONOFF:
        for (int i = (int)d.lo; i <= (int)d.hi; i++)
            item(m, d.choice[i], (float)i);
        break;
    case K_SYNC:
        for (int i = (int)d.lo; i <= (int)d.hi; i++)
            item(m, SYNC_NAME[i], (float)i);
        break;
    default:
        if (d.step > 0.0f && (d.hi - d.lo) / d.step <= 64.0f)
            for (float v = d.lo; v <= d.hi + 1e-3f; v += d.step) {
                char t[48];
                paramText(p, v, t, sizeof t);
                item(m, t, v);
            }
        else {
            vals.push_back(d.def);
            m.addItem(1, "Default (" + [&] { char t[48]; paramText(p, d.def, t, sizeof t); return juce::String(t); }() + ")");
        }
        break;
    }
    if (vals.empty())
        return;
    auto opts = menuOptions(screen).withMaximumNumColumns(4);
    m.showMenuAsync(opts, [set, vals](int r) {
        if (r > 0 && r <= (int)vals.size())
            set(vals[(size_t)(r - 1)]);
    });
}

void Panel::programMenu(juce::Rectangle<int> screen)
{
    juce::PopupMenu m;
    int n = proc.getNumPrograms(), cur = proc.getCurrentProgram();
    std::map<juce::String, juce::PopupMenu> cats;
    juce::PopupMenu user;
    for (int i = 0; i < n; i++) {
        if (i < factoryCount())
            cats[juce::String(factory(i).category)].addItem(i + 1, juce::String(i + 1).paddedLeft('0', 3) + "  " + proc.getProgramName(i), true, i == cur);
        else
            user.addItem(i + 1, proc.getProgramName(i), true, i == cur);
    }
    for (auto &c : cats)
        m.addSubMenu(c.first, c.second);
    if (n > factoryCount())
        m.addSubMenu("User", user);
    m.addSeparator();
    m.addItem(10000, "Reload user patches");
    m.addItem(10001, "Open patch folder");
    m.showMenuAsync(menuOptions(screen), [this](int r) {
        if (r == 10000)
            proc.loadUserPatches();
        else if (r == 10001) {
            auto f = proc.savedTo.existsAsFile() ? proc.savedTo.getParentDirectory() : HydrateProcessor::patchFolder();
            f.createDirectory();
            f.startAsProcess();
        } else if (r > 0)
            proc.setCurrentProgram(r - 1);
        repaint();
    });
}
void Panel::stepProgram(int d)
{
    int n = proc.getNumPrograms();
    if (n <= 0)
        return;
    proc.setCurrentProgram(((proc.getCurrentProgram() + d) % n + n) % n);
    repaint();
}
void Panel::saveDialog()
{
    if (saveWin != nullptr)
        return;
    /* inside the plugin's window: a window of its own can open behind the host's plugin window */
    saveWin = std::make_unique<juce::AlertWindow>("SAVE", "The patch's name:", juce::MessageBoxIconType::NoIcon);
    auto *w = saveWin.get();
    w->setLookAndFeel(&look);
    w->addTextEditor("name", proc.patchName());
    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    addAndMakeVisible(w);
    w->setCentrePosition(W / 2, H / 2);
    juce::Component::SafePointer<Panel> self(this);
    w->enterModalState(true, juce::ModalCallbackFunction::create([self](int r) {
        if (self == nullptr || self->saveWin == nullptr)
            return;
        if (r == 1) {
            auto name = self->saveWin->getTextEditorContents("name").trim();
            int prog = self->proc.writePatch(name);
            if (prog >= 0)
                self->flash("SAVED: " + name);
            else
                self->flash(name.isEmpty() ? "SAVE: NO NAME" : "SAVE FAILED");
        }
        juce::MessageManager::callAsync([self] {
            if (self != nullptr) {
                self->saveWin.reset();
                self->repaint();
            }
        });
    }), false);
    if (auto *ed = w->getTextEditor("name")) {
        ed->grabKeyboardFocus();
        ed->selectAll();
    }
}
void Panel::flash(const juce::String &t)
{
    flashText = t;
    flashUntil = juce::Time::getMillisecondCounter() + 3000;
    repaint();
}

static std::vector<int> moduleParams(int m)
{
    std::vector<int> v;
    auto range = [&](int a, int n) { for (int i = 0; i < n; i++) v.push_back(a + i); };
    switch (m) {
    case Panel::M_OSC1: case Panel::M_OSC2: case Panel::M_OSC3: range(pOsc(m - Panel::M_OSC1, 0), OS_N); break;
    case Panel::M_MUT1: case Panel::M_MUT2: case Panel::M_MUT3: case Panel::M_MUT4: range(pMut(m - Panel::M_MUT1, 0), MT_N); break;
    case Panel::M_RING: range(P_RN_SRC1, 4); v.push_back(P_MX_VOL1 + 3); v.push_back(P_MX_VOL1 + 4); break;
    case Panel::M_MIXER: range(P_MX_VOL1, P_MX_ROUTE - P_MX_VOL1 + 1); break;
    case Panel::M_F1: range(P_F1_TYPE, P_F1_VOWORD - P_F1_TYPE + 1); break;
    case Panel::M_F2: range(P_F2_TYPE, P_F2_KT - P_F2_TYPE + 1); break;
    case Panel::M_AMP: range(P_AMP_LFO2, 3); break;
    case Panel::M_PREFX: range(P_PREFX, FX_N); break;
    case Panel::M_POSTFX: range(P_POSTFX, FX_N); break;
    case Panel::M_DELAY: range(P_DL_TYPE, 8); break;
    case Panel::M_REVERB: range(P_RV_TYPE, 8); break;
    case Panel::M_VOICE: range(P_VO_POLY, P_VO_VMOD8 - P_VO_POLY + 1); break;
    case Panel::M_ARP: range(P_AR_ON, P_AR_CLKLOCK - P_AR_ON + 1); break;
    case Panel::M_MACRO: range(P_MC_VAL1, P_MOD1 - P_MC_VAL1); break;
    case Panel::M_MOD: range(P_MOD1, NPARAM - P_MOD1); break;
    default:
        if (m >= Panel::M_ENV1 && m < Panel::M_ENV1 + 5) range(pEnv(m - Panel::M_ENV1, 0), EN_N);
        else if (m >= Panel::M_LFO1 && m < Panel::M_LFO1 + 5) range(pLfo(m - Panel::M_LFO1, 0), LF_N);
        break;
    }
    return v;
}

/* a menu inside the plugin's window (a separate window can open behind an always-on-top plugin window) */
juce::PopupMenu::Options Panel::menuOptions(juce::Rectangle<int> screen)
{
    auto o = juce::PopupMenu::Options().withTargetScreenArea(screen);
    if (auto *ed = findParentComponentOfClass<juce::AudioProcessorEditor>())
        o = o.withParentComponent(ed);
    return o;
}

void Panel::randomModule()
{
    juce::Random rnd;
    for (int q : moduleParams(module))
        proc.setValue(q, fromNorm(q, rnd.nextFloat()));
    repaint();
}

void Panel::initMenu(juce::Rectangle<int> screen, bool random)
{
    juce::PopupMenu m;
    m.addItem(1, random ? "Randomize the patch" : "Init the patch");
    if (module != M_HOME)
        m.addItem(2, (random ? "Randomize " : "Init ") + moduleName(module));
    m.showMenuAsync(menuOptions(screen), [this, random](int r) {
        if (r <= 0)
            return;
        juce::Random rnd;
        auto rv = [&](int p) {
            const ParamDef &d = pdef(p);
            return fromNorm(p, rnd.nextFloat()) + 0.0f * d.lo;
        };
        if (r == 1) {
            Patch p;
            patchInit(p);
            if (random) {
                for (int mod : std::initializer_list<int>{M_OSC1, M_OSC2, M_OSC3, M_MUT1, M_MUT2, M_MUT3, M_MUT4, M_RING, M_F1, M_F2, M_ENV1, M_ENV1 + 2, M_LFO1, M_LFO1 + 1, M_LFO1 + 2})
                    for (int q : moduleParams(mod))
                        p.v[q] = rv(q);
                for (int s = 0; s < 3; s++)
                    p.v[P_MX_VOL1 + s] = 40.0f + rnd.nextFloat() * 88.0f;
                p.v[P_MX_VOL1 + 3] = rnd.nextFloat() < 0.3f ? rnd.nextFloat() * 80.0f : 0.0f;
                p.v[P_MX_VOL1 + 4] = rnd.nextFloat() < 0.2f ? rnd.nextFloat() * 40.0f : 0.0f;
                p.v[pEnv(1, EN_ATK)] = rnd.nextFloat() * rnd.nextFloat() * 2.0f;
                p.v[pEnv(1, EN_DEC)] = rnd.nextFloat() * 3.0f;
                p.v[pEnv(1, EN_SUS)] = rnd.nextFloat() * 128.0f;
                p.v[pEnv(1, EN_REL)] = rnd.nextFloat() * 2.5f;
                p.v[pEnv(1, EN_DLY)] = 0.0f;
                p.v[P_F1_CUT] = 40.0f + rnd.nextFloat() * 88.0f;
                for (int s = 0; s < 4; s++) {
                    p.v[pMod(s, MM_SRC)] = (float)(1 + rnd.nextInt(SRC_LFO1U + 4));
                    p.v[pMod(s, MM_DST)] = (float)(1 + rnd.nextInt((int)dests().size() - 1));
                    p.v[pMod(s, MM_DEPTH)] = (rnd.nextFloat() - 0.5f) * 80.0f;
                }
                p.v[P_RV_WET] = rnd.nextFloat() * 40.0f;
                p.v[P_DL_WET] = rnd.nextFloat() * 30.0f;
                std::snprintf(p.name, NAME_LEN, "Random");
            }
            proc.loadPatch(p);
        } else {
            for (int q : moduleParams(module))
                proc.setValue(q, random ? rv(q) : pdef(q).def);
        }
        repaint();
    });
}

/* ============================================================================ paint === */
void Panel::paintBackground(juce::Graphics &g, float density)
{
    using namespace juce;
    drawPanel(g, Rectangle<float>(0, 0, (float)W, (float)H), density);
    drawMetal(g, Rectangle<float>(0, 0, (float)W, (float)H), density);
    /* the name */
    {
        Path drop;
        float x = 34, y = 30;
        drop.startNewSubPath(x + 22, y);
        drop.cubicTo(x + 34, y + 18, x + 44, y + 30, x + 44, y + 42);
        drop.cubicTo(x + 44, y + 56, x + 34, y + 64, x + 22, y + 64);
        drop.cubicTo(x + 10, y + 64, x, y + 56, x, y + 42);
        drop.cubicTo(x, y + 30, x + 10, y + 18, x + 22, y);
        drop.closeSubPath();
        g.setColour(col::title);
        g.fillPath(drop);
        g.setColour(Colour(0xff17171a));
        for (int k = 0; k < 3; k++)
            g.drawLine(x + 8, y + 36 + k * 8, x + 36, y + 36 + k * 8 - 4, 2.4f);
        g.setFont(font(Face::heavy, 34.0f).withExtraKerningFactor(0.12f));
        g.setColour(col::legend);
        g.drawText("HYDRATE", Rectangle<float>(86, 34, 170, 40), Justification::centredLeft);
        g.setFont(font(Face::label, 11.5f).withExtraKerningFactor(0.18f));
        g.setColour(col::dim);
        g.drawText("WAVE MORPHING SYNTH", Rectangle<float>(88, 72, 170, 16), Justification::centredLeft);
        drawTag(g, Rectangle<float>(96, 98, 120, 24));
    }
    drawLegend(g, "VOICE SPREAD", Rectangle<float>(54, 207, 160, 16), 12.0f);
    drawLegend(g, "MASTER VOLUME", Rectangle<float>(54, 303, 160, 16), 12.0f);
    drawWell(g, R_SETUP, "SETUP");
    drawLegend(g, "VOICES", Rectangle<float>(44, 368, 180, 18), 11.0f, Justification::centred, col::dim);
    drawLegend(g, "QUALITY", Rectangle<float>(44, 432, 180, 18), 11.0f, Justification::centred, col::dim);
    drawLegend(g, "VOICE ACTIVITY", Rectangle<float>(44, 566, 180, 14), 9.5f, Justification::centred, col::dim);

    /* ARPEGGIATOR */
    drawWell(g, R_ARP, "ARPEGGIATOR");
    static const char *const AL1[4] = {"TEMPO", "RATCHET", "CHANCE", "GATE"};
    static const char *const AL2[4] = {"MODE", "OCTAVE", "DIVISION", "SWING"};
    for (int i = 0; i < 4; i++) {
        drawLegend(g, AL1[i], Rectangle<float>(ARPX[i] - 46, 162, 92, 18), 13.0f);
        drawLegend(g, AL2[i], Rectangle<float>(ARPX[i] - 46, 400, 92, 18), 13.0f);
    }
    /* the scales beside the stepped knobs, with their leader lines */
    auto scaleL = [&](float cx, float cy, std::initializer_list<const char *> left, std::initializer_list<const char *> right,
                      float wl, float wr) {
        int i = 0;
        for (auto t : left) {
            float y = cy + 24 - (float)i * 16;
            drawLegend(g, t, Rectangle<float>(cx - 34 - wl, y - 8, wl, 13), 9.5f, Justification::centredLeft);
            g.setColour(col::dim.withAlpha(0.75f));
            g.drawLine(cx - 34 - wl, y + 6, cx - 24, y + 6, 0.8f);
            i++;
        }
        i = 0;
        for (auto t : right) {
            float y = cy - 24 + (float)i * 16;
            drawLegend(g, t, Rectangle<float>(cx + 34, y - 8, wr, 13), 9.5f, Justification::centredRight);
            g.setColour(col::dim.withAlpha(0.75f));
            g.drawLine(cx + 24, y + 6, cx + 34 + wr, y + 6, 0.8f);
            i++;
        }
    };
    scaleL(ARPX[0], 330, {"UP", "DOWN", "UP/DN", "UP & DN"}, {"ORDER", "RANDOM", "CHORD", "PHRASE"}, 50, 50);
    scaleL(ARPX[1], 330, {}, {"1", "2", "3", "4"}, 0, 14);
    scaleL(ARPX[2], 330, {"1/4", "1/8", "1/16", "1/32"}, {}, 30, 0);
    scaleL(ARPX[3], 330, {"OFF", "52%", "54%", "58%"}, {"62%", "66%", "70%", "75%"}, 30, 30);
    drawLegend(g, "SUSTAIN", Rectangle<float>(ARPX[1] - 40, 488, 80, 16), 10.0f, Justification::centred, col::title.withAlpha(0.7f));
    drawLegend(g, "ARP STEP", Rectangle<float>(R_ARP.getX(), 566, R_ARP.getWidth(), 14), 9.5f, Justification::centred, col::dim);

    /* MAIN SYSTEMS */
    drawWell(g, R_MAIN, "MAIN SYSTEMS");
    drawLegend(g, "PATCH", Rectangle<float>(332, 174, 100, 14), 10.0f, Justification::centred, col::dim);
    drawLegend(g, "-1", Rectangle<float>(282, 369, 94, 13), 10.0f, Justification::centred, col::title.withAlpha(0.8f));
    drawLegend(g, "+1", Rectangle<float>(388, 369, 94, 13), 10.0f, Justification::centred, col::title.withAlpha(0.8f));

    /* MASTER CONTROL */
    drawWell(g, R_MC, "MASTER CONTROL");
    for (int i = 0; i < 8; i++) {
        float cx = MCX[i % 4];
        drawLegend(g, juce::String(i + 1), Rectangle<float>(cx - 10, i < 4 ? 182.0f : 390.0f, 20, 16), 12.0f, Justification::centred, col::dim);
    }
    g.setColour(Colour(0xff050506));
    g.fillRoundedRectangle(R_DISP.expanded(4.0f), 5.0f);
    g.setColour(Colour(0xff2c2c30));
    g.drawRoundedRectangle(R_DISP.expanded(4.0f), 5.0f, 1.2f);
    drawLegend(g, "PAGE", Rectangle<float>(1338 + DX, 310, 54, 16), 11.0f, Justification::centred, col::dim);

    /* FILTER CONTROLS */
    drawWell(g, R_FC, "FILTER CONTROLS");
    static const char *const FL[5] = {"CUTOFF", "RESONANCE", "DRIVE / MORPH", "ENV 1", "LFO 1"};
    for (int k = 0; k < 5; k++) {
        drawLegend(g, FL[k], Rectangle<float>(FCX[k] - 70, FCY[k] + FCS / 2 + 10, 140, 20), 15.0f);
        if (k >= 3) {
            drawLegend(g, "-", Rectangle<float>(FCX[k] - FCS / 2 - 8, FCY[k] + FCS / 2 - 12, 14, 16), 16.0f);
            drawLegend(g, "+", Rectangle<float>(FCX[k] + FCS / 2 - 6, FCY[k] + FCS / 2 - 12, 14, 16), 16.0f);
        }
    }

    /* MODULE SELECT: the signal flow */
    drawWell(g, R_MS, "MODULE SELECT");
    auto at = [&](int mod) -> juce::Point<float> {
        for (const auto &m : MSP)
            if (m.module == mod)
                return {m.cx, m.cy};
        return {};
    };
    g.setColour(col::line.withAlpha(0.85f));
    auto hl = [&](float x0, float x1, float y) { g.drawLine(x0, y, x1, y, 1.8f); };
    auto vl = [&](float x, float y0, float y1) { g.drawLine(x, y0, x, y1, 1.8f); };
    float busX = MSC[2] + MBW / 2 + 20;
    hl(MSC[0], busX, Y_O1);
    hl(MSC[0], busX, Y_MID);
    hl(MSC[2], busX, Y_O3);
    hl(MSC[0], MSC[0] + 30, Y_O3);
    vl(MSC[0] + 30, Y_O3, Y_O3 + 30);
    hl(MSC[0] + 30, busX, Y_O3 + 30);
    vl(busX, Y_O1, Y_O3 + 30);
    hl(busX, MSC[3], Y_MID);
    hl(MSC[3], MSC[4] - MBW / 2 - 14, Y_MID);
    vl(MSC[4] - MBW / 2 - 14, Y_F1, Y_F2);
    hl(MSC[4] - MBW / 2 - 14, MSC[4], Y_F1);
    hl(MSC[4] - MBW / 2 - 14, MSC[4], Y_F2);
    vl(MSC[4] + MBW / 2 + 12, Y_F1, Y_F2);
    hl(MSC[4], MSC[4] + MBW / 2 + 12, Y_F1);
    hl(MSC[4], MSC[4] + MBW / 2 + 12, Y_F2);
    vl(MSC[4], Y_F1, Y_F2);
    hl(MSC[4] + MBW / 2 + 12, MSC[6], Y_MID);
    hl(MSC[6], MSC[7] - MBW / 2 - 12, Y_MID);
    vl(MSC[7] - MBW / 2 - 12, Y_F1, Y_MID);
    hl(MSC[7] - MBW / 2 - 12, MSC[7], Y_F1);
    vl(MSC[7], Y_F1, Y_F2);
    hl(MSC[7], MSC[7] + MBW / 2 + 10, Y_F2);
    vl(MSC[7] + MBW / 2 + 10, Y_MID, Y_F2);
    hl(MSC[7] + MBW / 2 + 10, MSC[8], Y_MID);
    /* the envelopes' and LFOs' bars, the dotted pre-wiring (ENV 1 and LFO 1 to the filters, ENV 2 and LFO 2 to the amp) */
    g.setColour(col::title);
    g.fillRect(Rectangle<float>(MSC[4] - 60, Y_ENV - 2.5f, MSC[8] - MSC[4] + 112, 5.0f));
    g.fillRect(Rectangle<float>(MSC[4] - 60, Y_LFO - 2.5f, MSC[8] - MSC[4] + 112, 5.0f));
    g.setColour(col::line.withAlpha(0.8f));
    float dash[2] = {3.0f, 3.0f};
    auto dl = [&](float x, float y0, float y1) { g.drawDashedLine(Line<float>(x, y0, x, y1), dash, 2, 1.4f); };
    dl(MSC[4], Y_ENV + MBH / 2, Y_F1 - MBH / 2);
    dl(MSC[5], Y_ENV + MBH / 2, Y_MID - MBH / 2);
    dl(MSC[4], Y_F2 + MBH / 2, Y_LFO - MBH / 2);
    dl(MSC[5], Y_MID + MBH / 2, Y_LFO - MBH / 2);
    (void)at;
    drawSurface(g, Rectangle<float>(0, 0, (float)W, (float)H), density);
}

void Panel::paintMainDisplay(juce::Graphics &g)
{
    using namespace juce;
    auto r = R_SMALL;
    g.setColour(col::oledBg);
    g.fillRoundedRectangle(r.expanded(4), 4.0f);
    g.setColour(Colour(0xff2c2c30));
    g.drawRoundedRectangle(r.expanded(4), 4.0f, 1.2f);
    /* the top line: the patch's number and category */
    g.setFont(font(Face::mono, 11.0f));
    g.setColour(col::oledDim);
    int prog = proc.getCurrentProgram();
    g.drawText(juce::String(prog + 1).paddedLeft('0', 3) + "  " + proc.patchCategory().toUpperCase(), r.reduced(6, 4).withHeight(14), Justification::centredLeft);
    if (juce::Time::getMillisecondCounter() < flashUntil) {
        g.setColour(Colour(0xff121214));
        g.fillRect(r.reduced(4, 2).withHeight(18));
        g.setColour(col::orangeLit);
        g.drawText(flashText, r.reduced(6, 4).withHeight(14), Justification::centredLeft);
    } else if (proc.splitLearn.load()) {
        g.setColour(col::blueLit);
        g.drawText("SPLIT: PLAY KEY", r.reduced(6, 4).withHeight(14), Justification::centredRight);
    } else if (proc.apvts.getRawParameterValue("low_split")->load() > 0.5f) {
        int sn = (int)proc.apvts.getRawParameterValue("split_note")->load();
        g.setColour(col::blueLit);
        g.drawText("SPLIT " + juce::MidiMessage::getMidiNoteName(sn, true, true, 3), r.reduced(6, 4).withHeight(14), Justification::centredRight);
    }
    /* the wave: the oscillator's when an oscillator page is open, else the output's scope */
    auto wv = Rectangle<float>(r.getX() + 6, r.getY() + 22, r.getWidth() - 12, 62);
    Path p;
    bool oscView = module >= M_OSC1 && module <= M_OSC3;
    if (oscView) {
        int o = module - M_OSC1;
        const WaveBank &W = WaveBank::get();
        bool scan = o < 2 && proc.value(pOsc(o, OS_MODE)) > 0.5f;
        const float *tA = nullptr, *tB = nullptr;
        float a = 0.0f;
        if (scan) {
            int list[8], n = 0;
            for (int w = 0; w < 8; w++) {
                int v = (int)proc.value(pOsc(o, OS_WAV1 + w));
                if (w == 0 && v < 0) v = 0;
                if (v >= -1) list[n++] = v;
            }
            float pos = juce::jlimit(1.0f, (float)n, proc.value(pOsc(o, OS_SCAN))) - 1.0f;
            int i = juce::jmin((int)pos, n - 1), j = juce::jmin(i + 1, n - 1);
            a = pos - (float)i;
            tA = list[i] >= 0 ? W.table(list[i], 2) : nullptr;
            tB = list[j] >= 0 ? W.table(list[j], 2) : nullptr;
        } else
            tA = W.table(juce::jlimit(0, NWAVE - 1, (int)proc.value(pOsc(o, OS_WAVE))), 2);
        for (int i = 0; i <= 128; i++) {
            float x = (float)i / 128.0f;
            float y = (tA ? readTab(tA, x) * (1 - a) : 0.0f) + (tB ? readTab(tB, x) * a : 0.0f);
            float px = wv.getX() + x * wv.getWidth(), py = wv.getCentreY() - y * wv.getHeight() * 0.45f;
            if (i == 0) p.startNewSubPath(px, py); else p.lineTo(px, py);
        }
    } else {
        /* the scope: a cycle found at a rising zero crossing */
        int start = 0;
        for (int i = 1; i < 256; i++)
            if (proc.scopeCopy(i - 1) <= 0.0f && proc.scopeCopy(i) > 0.0f) { start = i; break; }
        float pk = 0.05f;
        for (int i = 0; i < 256; i++)
            pk = juce::jmax(pk, std::fabs(proc.scopeCopy(start + i)));
        for (int i = 0; i < 256; i++) {
            float y = proc.scopeCopy(start + i) / pk;
            float px = wv.getX() + (float)i / 255.0f * wv.getWidth(), py = wv.getCentreY() - y * wv.getHeight() * 0.45f;
            if (i == 0) p.startNewSubPath(px, py); else p.lineTo(px, py);
        }
    }
    g.setColour(col::blueLit.withAlpha(0.25f));
    g.strokePath(p, PathStrokeType(3.0f));
    g.setColour(Colour(0xffbfe4ff));
    g.strokePath(p, PathStrokeType(1.3f));
    g.setFont(font(Face::mono, 13.0f));
    g.setColour(col::oled);
    g.drawFittedText(proc.patchName(), Rectangle<int>((int)r.getX() + 4, (int)r.getBottom() - 24, (int)r.getWidth() - 8, 18), Justification::centred, 1, 0.7f);
}

void Panel::paintDisplay(juce::Graphics &g)
{
    using namespace juce;
    auto r = R_DISP;
    g.setFont(font(Face::mono, 11.0f));
    g.setColour(col::oledDim);
    juce::String head = moduleName(module);
    if (module == M_MACRO && macroSel >= 0)
        head += " - " + juce::String(MACRO_NAME[juce::jlimit(0, NMACRONAME - 1, (int)proc.value(P_MC_NAME1 + macroSel))]) + " " + juce::String(macroSel + 1);
    int np = pages();
    g.drawText(head, Rectangle<float>(r.getX() + 6, r.getY() + 2, 280, 14), Justification::centredLeft);
    if (np > 1)
        g.drawText("PAGE " + juce::String(juce::jmin(page, np - 1) + 1) + "/" + juce::String(np), Rectangle<float>(r.getRight() - 120, r.getY() + 2, 114, 14), Justification::centredRight);
    if (module == M_HOME) {
        g.drawText(juce::String(proc.voicesNow.load()) + " VOICES  " + juce::String(proc.bpmNow.load(), 1) + " BPM",
                   Rectangle<float>(r.getRight() - 200, r.getY() + 2, 194, 14), Justification::centredRight);
    }
    auto es = entries();
    for (int i = 0; i < 8; i++) {
        const Entry &e = es[(size_t)i];
        auto c = Rectangle<float>(r.getX() + 4 + (float)(i % 4) * 100.0f, r.getY() + 20.0f + (i < 4 ? 0.0f : 68.0f), 96, 62);
        if (!e.valid() || !e.text)
            continue;
        bool sel = i == touched;
        auto box = c.reduced(4, 3);
        if (sel) {
            g.setColour(Colour(0xff3b3d42));
            g.fillRoundedRectangle(box, 3.0f);
        }
        g.setFont(font(Face::mono, 13.0f));
        g.setColour(col::oled);
        if (e.text2) {
            g.setColour(e.editSecond ? col::oledDim : col::oled);
            g.drawFittedText(e.text(), box.withHeight(18).translated(0, 6).toNearestInt(), Justification::centred, 1, 0.6f);
            g.setColour(e.editSecond ? col::oled : col::oledDim);
            g.setFont(font(Face::mono, 11.0f));
            g.drawFittedText(e.text2(), box.withTrimmedTop(26).toNearestInt(), Justification::centred, 2, 0.6f);
            g.setColour(col::oledDim);
            g.setFont(font(Face::mono, 10.0f));
            g.drawText(e.label, box.withHeight(10).translated(0, -1), Justification::centred);
            continue;
        }
        g.drawFittedText(e.label, box.withHeight(22).translated(0, 4).toNearestInt(), Justification::centred, 1, 0.6f);
        g.setColour(col::oled.withAlpha(0.65f));
        g.drawLine(box.getX() + 6, box.getY() + 29, box.getRight() - 6, box.getY() + 29, 1.0f);
        g.setFont(font(Face::mono, 15.0f));
        g.setColour(col::oled);
        g.drawFittedText(e.text(), box.withTrimmedTop(30).toNearestInt(), Justification::centred, 1, 0.55f);
    }
}

void Panel::paint(juce::Graphics &g)
{
    using namespace juce;
    float density = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (background.isNull() || std::fabs(density - bgScale) > 0.01f) {
        bgScale = density;
        background = Image(Image::RGB, jmax(1, (int)std::ceil(W * density)), jmax(1, (int)std::ceil(H * density)), false);
        Graphics bg(background);
        bg.addTransform(AffineTransform::scale(density));
        paintBackground(bg, density);
    }
    g.setOpacity(1.0f);
    g.drawImage(background, Rectangle<float>(0, 0, (float)W, (float)H), RectanglePlacement::stretchToFit);
    auto es = entries();
    for (const auto &c : ctls) {
        switch (c.kind) {
        case Ctl::KNOB: {
            drawKnobBody(g, c.r);
            drawKnobPointer(g, c.r, c.getN ? c.getN() : 0.0f);
            if (c.label.isNotEmpty() && c.r.getWidth() > 70.0f) {
            }
            break;
        }
        case Ctl::MKNOB: {
            const Entry &e = es[(size_t)c.index];
            bool ok = (bool)e.getN;
            float pos = ok ? e.getN() : 0.0f;
            drawLedRing(g, c.r.getCentre(), c.r.getWidth() * 0.5f + 12.0f, pos, e.bip, ok);
            drawKnobBody(g, c.r);
            drawKnobPointer(g, c.r, pos);
            break;
        }
        case Ctl::ENCODER: {
            auto ctr = c.r.getCentre();
            float R = c.r.getWidth() * 0.5f + 8.0f;
            g.setGradientFill(ColourGradient(col::blueLit.withAlpha(0.55f), ctr.x, ctr.y, col::blueLit.withAlpha(0.0f), ctr.x + R + 14, ctr.y, true));
            g.fillEllipse(ctr.x - R - 14, ctr.y - R - 14, (R + 14) * 2, (R + 14) * 2);
            g.setColour(Colour(0xff7fc4ff));
            g.drawEllipse(ctr.x - R, ctr.y - R, R * 2, R * 2, 3.0f);
            drawKnobBody(g, c.r);
            break;
        }
        case Ctl::BUTTON: {
            bool down = pressed && active >= 0 && &ctls[(size_t)active] == &c;
            bool on = c.on ? c.on() : false;
            drawButton(g, c.r, c.label, on, down, c.lit, c.textSize, c.lit != Lit::orange);
            break;
        }
        default: break;
        }
    }
    paintDisplay(g);
    paintMainDisplay(g);
    /* the displays' glass: a faint reflection of the light */
    for (auto rr : {R_DISP.expanded(4.0f), R_SMALL.expanded(4.0f)}) {
        g.setGradientFill(ColourGradient(Colours::white.withAlpha(0.07f), rr.getX(), rr.getY(), Colours::transparentWhite,
                                         rr.getX() + rr.getWidth() * 0.45f, rr.getY() + rr.getHeight() * 0.6f, false));
        g.fillRoundedRectangle(rr, 5.0f);
    }
    /* the voices' LEDs, the arp's step */
    int nv = proc.apvts.getRawParameterValue("voices")->load() > 0.5f ? 16 : 8;
    for (int v = 0; v < 16; v++) {
        float x = 50 + (float)v * 11.2f;
        drawLed(g, {x, 588}, 3.2f, v < nv ? proc.voiceLevel[v].load() * 3.0f : 0.0f, v < nv ? col::blueLit : col::dim);
    }
    int st = proc.arpStep.load();
    for (int i = 0; i < 16; i++)
        drawLed(g, {R_ARP.getX() + 30.0f + (float)i * 29.7f, 588.0f}, 3.2f, st >= 0 && st % 16 == i ? 1.0f : 0.0f, col::orangeLit);
}

/* =========================================================================== editor === */
HydrateEditor::HydrateEditor(HydrateProcessor &p) : AudioProcessorEditor(&p), proc(p), panel(p)
{
    addAndMakeVisible(panel);
    panel.setBounds(0, 0, HydratePanel::W, HydratePanel::H);
    const double W = HydratePanel::W, H = HydratePanel::H;
    auto area = juce::Rectangle<int>(0, 0, 1920, 1080);
    if (auto *d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        area = d->userArea;
    double fit = juce::jmin((area.getWidth() - 40) / W, (area.getHeight() - 90) / H);
    fit = juce::jlimit(0.4, 1.6, fit);
    setResizable(true, true);
    setResizeLimits((int)(W * 0.4), (int)(H * 0.4), (int)(W * juce::jmax(0.5, fit)), (int)(H * juce::jmax(0.5, fit)));
    getConstrainer()->setFixedAspectRatio(W / H);
    double s = proc.uiScaleSet ? (double)proc.uiScale.load() : juce::jmin(fit, 1.0);
    s = juce::jlimit(0.4, fit, s);
    setSize((int)std::lround(W * s), (int)std::lround(H * s));
}
void HydrateEditor::resized()
{
    float s = (float)getWidth() / (float)HydratePanel::W;
    panel.setTransform(juce::AffineTransform::scale(s));
    proc.uiScale = s;
    proc.uiScaleSet = true;
}
