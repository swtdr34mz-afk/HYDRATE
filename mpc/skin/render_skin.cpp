/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
/* HYDRATE's MPC skin, drawn with the VST3's own art (src/ui/Look.cpp: the satin steel, the sanded-plastic wells with
 * their orange titles, the aluminium knobs on knurled skirts, the rubber buttons, the OLEDs), large for a finger on
 * the MPC's screen (Live III, XL: 1280 x 628 for a plugin page). Six pages on the six function keys:
 *
 *   OSC     OSC 1, 2, 3 (the wave in an OLED: drag it) and the MIXER
 *   MUTANT  MUTANT 1-4 (the mode in an OLED)
 *   FILTER  FILTER 1 (CUTOFF large) and FILTER 2
 *   ENV     ENV 1 (the filter's) and ENV 2 (the amp's), eight knobs each
 *   MOD     LFO 1, LFO 2, the eight MACROS; VOICES, QUALITY, INIT
 *   FX/ARP  PRE-FX, DELAY, REVERB, the ARPEGGIATOR
 *
 * Each page is cut up: its background, and every control at every value (a knob's film strip with its value under it,
 * an OLED's values, a button's looks). manifest.json says what each picture is, its MPC handle and its Q-Link, for
 * make_skin.py. With --preview: each page as the MPC shows it (the first factory patch).
 *
 *   HydrateSkin <out folder> [--preview]                                                                       */
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/Look.h"
#include "Presets.h"
#include "../plugin/src/hydrate_mpc_params.h"
#include <cstdio>
#include <functional>
#include <vector>

using juce::Colour;
using juce::Graphics;
using juce::Image;
using juce::Rectangle;
using juce::String;
using juce::var;
using namespace hyui;
using namespace hmpc;

namespace {
constexpr int TW = 1280, TH = 628;
constexpr float PI = 3.14159265f;
String H(int i) { return "Parameter " + String(i); }

/* ------------------------------------------------------------------ the MPC plugin's parameters --- */
float startPlain[kNumParams];            /* what a page shows in --preview: the first factory patch */
struct Info { bool cont; int steps; juce::StringArray names; bool bipolar; };
Info info(int i)
{
    if (i == kVolume) return { true, 0, {}, false };
    if (i == kVoices) return { false, 2, { "8 VOICES", "16 VOICES" }, false };
    if (i == kQuality) return { false, 2, { "ECO", "HIGH" }, false };
    if (i == kInit) return { false, 2, { "OFF", "ON" }, false };
    int p = E(i);
    const hy::ParamDef &d = hy::pdef(p);
    Info r{ typeOf(p) == FLOAT, steps(p), {}, d.lo < 0.0f };
    if (typeOf(p) == CHOICE)
        for (int k = (int)d.lo; k <= (int)d.hi; k++) {
            char t[48];
            hy::paramText(p, (float)k, t, sizeof t, false);
            r.names.add(t);
        }
    else if (typeOf(p) == BOOL)
        r.names = { "OFF", "ON" };
    return r;
}
String valueText(int i, float v)
{
    if (i == kVolume) return String(juce::roundToInt(v * 100.0f)) + " %";
    if (E(i) < 0) { auto in = info(i); return in.names[juce::jlimit(0, in.names.size() - 1, (int)std::lround(v))]; }
    char t[48];
    hy::paramText(E(i), toEngine(E(i), v), t, sizeof t, false);
    return t;
}

/* -------------------------------------------------------------------------------------- controls --- */
struct Ctl {
    enum Kind { KNOB, BUTTON, OLED } kind;
    int p = -1;                          /* the MPC parameter */
    String label;
    Rectangle<float> r;                  /* where it is cut */
    float cx = 0, cy = 0, d = 76;        /* a knob's centre and size */
    enum Mode { TOGGLE, MOMENTARY, CYCLE } mode = TOGGLE;
    Lit lit = Lit::white;
    int q = -1;                          /* the Q-Link slot (0..15: row 0 the top row, Q-Links 13..16) */
};
struct Section { String title; Rectangle<float> r; };
struct Page {
    String name, title;
    int key;
    std::vector<Section> secs;
    std::vector<Ctl> ctls;
    std::vector<std::function<void(Graphics &)>> extra;
};

Rectangle<float> knobRect(float cx, float cy, float d)
{
    float w = std::max(d + 40.0f, 140.0f), top = d * 0.5f + 16.0f;
    return { cx - w * 0.5f, cy - top, w, top + d * 0.5f + 58.0f };
}

struct Builder {
    Page pg;
    Builder(const char *name, const char *title, int key) { pg.name = name; pg.title = title; pg.key = key; }
    void sec(const String &t, float x, float w) { pg.secs.push_back({ t, { x, 12, w, 566 } }); }
    Ctl &K(int p, const String &label, float cx, float cy, float d = 76, int q = -1)
    {
        Ctl c; c.kind = Ctl::KNOB; c.p = p; c.label = label; c.cx = cx; c.cy = cy; c.d = d; c.q = q;
        c.r = knobRect(cx, cy, d);
        pg.ctls.push_back(c);
        return pg.ctls.back();
    }
    Ctl &O(int p, const String &label, Rectangle<float> r)       /* an OLED: the value, dragged */
    {
        Ctl c; c.kind = Ctl::OLED; c.p = p; c.label = label; c.r = r;
        pg.ctls.push_back(c);
        return pg.ctls.back();
    }
    Ctl &B(int p, const String &label, float cx, float cy, Ctl::Mode m, float w = 120, Lit lit = Lit::white)
    {
        Ctl c; c.kind = Ctl::BUTTON; c.p = p; c.label = label; c.cx = cx; c.cy = cy; c.mode = m; c.lit = lit;
        c.r = { cx - w * 0.5f - 6, cy - 26.0f, w + 12, m == Ctl::CYCLE ? 86.0f : 70.0f };
        pg.ctls.push_back(c);
        return pg.ctls.back();
    }
};

/* ------------------------------------------------------------------------------------ the drawing --- */
void drawKnob(Graphics &g, const Ctl &c, float t, const String &value)
{
    float d = c.d, cx = c.cx, cy = c.cy;
    g.setColour(col::dim.withAlpha(0.75f));
    for (int k = 0; k <= 10; k++) {
        float a = PI * (-0.75f + 1.5f * (float)k / 10.0f);
        float r1 = d * 0.5f + 7.0f, r2 = d * 0.5f + (k % 5 == 0 ? 14.0f : 11.0f);
        g.drawLine(cx + std::sin(a) * r1, cy - std::cos(a) * r1, cx + std::sin(a) * r2, cy - std::cos(a) * r2, k % 5 == 0 ? 1.8f : 1.1f);
    }
    Rectangle<float> r(cx - d / 2, cy - d / 2, d, d);
    drawKnobBody(g, r);
    drawKnobPointer(g, r, t);
    drawLegend(g, c.label, { c.r.getX(), cy + d / 2 + 16, c.r.getWidth(), 19 }, 15.0f, juce::Justification::centred, col::legend);
    g.setFont(font(Face::mono, 16.0f));
    g.setColour(col::orangeLit);
    g.drawFittedText(value, Rectangle<float>(c.r.getX(), cy + d / 2 + 37, c.r.getWidth(), 19).toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

void drawOled(Graphics &g, const Ctl &c, const String &value)
{
    auto r = c.r.reduced(4);
    g.setColour(col::oledBg);
    g.fillRoundedRectangle(r, 5.0f);
    g.setColour(Colour(0xff2c2c30));
    g.drawRoundedRectangle(r, 5.0f, 1.4f);
    g.setFont(font(Face::mono, 13.5f));
    g.setColour(col::oledDim);
    g.drawText(c.label, r.reduced(10, 5).withHeight(16), juce::Justification::centredLeft);
    g.setFont(font(Face::mono, 21.0f));
    g.setColour(col::oled);
    g.drawFittedText(value, r.reduced(10, 3).withTrimmedTop(18).toNearestInt(), juce::Justification::centred, 1, 0.6f);
    juce::ColourGradient gl(juce::Colours::white.withAlpha(0.07f), r.getX(), r.getY(), juce::Colours::transparentWhite, r.getX(), r.getCentreY(), false);
    g.setGradientFill(gl);
    g.fillRoundedRectangle(r.withHeight(r.getHeight() * 0.5f), 5.0f);
}

void drawBtn(Graphics &g, const Ctl &c, bool on, const String &value)
{
    float w = c.r.getWidth() - 12;
    Rectangle<float> b(c.cx - w * 0.5f, c.cy - 20, w, 40);
    drawButton(g, b, c.mode == Ctl::CYCLE ? value : c.label, on, false, c.lit, 14.0f);
    if (c.mode == Ctl::CYCLE)
        drawLegend(g, c.label, { c.r.getX(), b.getBottom() + 8, c.r.getWidth(), 17 }, 13.0f, juce::Justification::centred, col::dim);
}

/* a control at a value: v the MPC's plain value */
void paintCtl(Graphics &g, const Ctl &c, float v, bool lit = false)
{
    auto in = info(c.p);
    switch (c.kind) {
    case Ctl::KNOB: drawKnob(g, c, in.cont ? v : v / (float)std::max(1, in.steps - 1), valueText(c.p, v)); break;
    case Ctl::OLED: drawOled(g, c, valueText(c.p, v)); break;
    case Ctl::BUTTON: drawBtn(g, c, lit, valueText(c.p, v)); break;
    }
}

/* ---------------------------------------------------------------------------------------- pages --- */
/* a well with an OLED and four knobs (2 x 2): its Q-Link column col (rows 0..3) */
void quad(Builder &b, float x, float w, const String &title, int oledP, const String &oledLabel, const int q[4],
          const char *const L[4], int col)
{
    b.sec(title, x, w);
    if (oledP >= 0)
        b.O(oledP, oledLabel, { x + 14, 50, w - 28, 66 });
    for (int k = 0; k < 4; k++)
        if (q[k] >= 0)
            b.K(q[k], L[k], x + w * (k % 2 ? 0.73f : 0.27f), 202.0f + (float)(k / 2) * 196.0f, 80, col >= 0 ? k * 4 + col : -1);
}

Page oscPage()
{
    using namespace hy;
    Builder b("OSC", "OSCILLATORS + MIXER", 0);
    for (int o = 0; o < 3; o++) {
        int q[4] = { M(pOsc(o, OS_SEMI)), M(pOsc(o, OS_CENT)), M(o < 2 ? pOsc(o, OS_SCAN) : pOsc(o, OS_KT)), M(o < 2 ? pOsc(o, OS_KT) : pOsc(o, OS_BITS)) };
        const char *const L[4] = { "SEMI", "CENTS", o < 2 ? "WAVESCAN" : "KEYTRACK", o < 2 ? "KEYTRACK" : "BITREDUX" };
        quad(b, 12.0f + (float)o * 312.0f, 300, "OSC " + String(o + 1), M(pOsc(o, OS_WAVE)), "WAVE", q, L, o);
    }
    b.sec("MIXER", 948, 320);
    static const char *const ML[5] = { "OSC 1", "OSC 2", "OSC 3", "RING MOD", "NOISE" };
    for (int k = 0; k < 4; k++)
        b.K(M(P_MX_VOL1 + k), ML[k], 948 + (k % 2 ? 0.73f : 0.27f) * 320, 110.0f + (float)(k / 2) * 178, 74, k * 4 + 3);
    b.K(M(P_MX_VOL1 + 4), ML[4], 948 + 0.27f * 320, 466, 74);
    b.B(M(P_MX_ROUTE), "FILTER ROUTE", 948 + 0.73f * 320, 452, Ctl::CYCLE, 130, Lit::orange);
    return b.pg;
}

Page mutantPage()
{
    using namespace hy;
    Builder b("MUTANT", "MUTANTS", 1);
    for (int m = 0; m < 4; m++) {
        int q[4] = { M(pMut(m, MT_RATIO)), M(pMut(m, MT_DEPTH)), M(pMut(m, MT_FB)), M(pMut(m, MT_WET)) };
        const char *const L[4] = { "RATIO", "DEPTH", "FEEDBACK", "DRY / WET" };
        quad(b, 12.0f + (float)m * 314.0f, 302, "MUTANT " + String(m + 1), M(pMut(m, MT_MODE)), m < 2 ? "MODE - AFTER OSC 1" : "MODE - AFTER OSC 2", q, L, m);
    }
    return b.pg;
}

Page filterPage()
{
    using namespace hy;
    Builder b("FILTER", "FILTERS", 2);
    b.sec("FILTER 1", 12, 700);
    b.O(M(P_F1_TYPE), "TYPE", { 26, 50, 300, 66 });
    b.K(M(P_F1_CUT), "CUTOFF", 176, 272, 150, 0);
    b.K(M(P_F1_RES), "RESONANCE", 432, 112, 76, 1);
    b.K(M(P_F1_DRIVE), "DRIVE", 602, 112, 76, 2);
    b.K(M(P_F1_ENV), "ENV 1 AMT", 432, 292, 76, 5);
    b.K(M(P_F1_LFO), "LFO 1 AMT", 602, 292, 76, 6);
    b.K(M(P_F1_KT), "KEYTRACK", 432, 466, 76, 9);
    b.K(M(P_F1_VEL), "VEL ENV", 602, 466, 76, 10);
    b.K(M(P_F1_CTRL), "FORMANT / CTRL", 176, 478, 76, 8);
    b.sec("FILTER 2", 724, 544);
    b.O(M(P_F2_TYPE), "TYPE", { 738, 50, 516, 66 });
    const int q2[6] = { P_F2_MORPH, P_F2_CUT, P_F2_RES, P_F2_ENV, P_F2_KT, P_F2_LFO };
    static const char *const L2[6] = { "MORPH", "CUTOFF", "RESONANCE", "ENV 1 AMT", "KEYTRACK", "LFO 1 AMT" };
    static const int Q2[6] = { 3, 7, 11, 15, 14, 13 };
    for (int k = 0; k < 6; k++)
        b.K(M(q2[k]), L2[k], 819.0f + (float)(k % 3) * 177, 212.0f + (float)(k / 3) * 196, 80, Q2[k]);
    return b.pg;
}

Page envPage()
{
    using namespace hy;
    Builder b("ENV", "ENVELOPES", 3);
    for (int e = 0; e < 2; e++) {
        float x = 12.0f + (float)e * 634.0f;
        b.sec(e == 0 ? "ENV 1  -  FILTER" : "ENV 2  -  AMP", x, 622);
        int q[8] = { pEnv(e, EN_ATK), pEnv(e, EN_DEC), pEnv(e, EN_SUS), pEnv(e, EN_REL), pEnv(e, EN_DLY), pEnv(e, EN_HLD),
                     e == 0 ? pEnv(e, EN_ACRV) : P_AMP_VEL, e == 0 ? pEnv(e, EN_DCRV) : P_AMP_LEVEL };
        static const char *const L1[8] = { "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "DELAY", "HOLD", "ATK CURVE", "DEC CURVE" };
        static const char *const L2[8] = { "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "DELAY", "HOLD", "VELOCITY", "LEVEL" };
        for (int k = 0; k < 8; k++) {
            /* the Q-Links: ENV 1's ADSR the top row, ENV 2's the second; their other four rows 3 and 4 */
            int row = k < 4 ? e : 2 + e, qq = k < 4 || true ? row * 4 + k % 4 : -1;
            b.K(M(q[k]), e == 0 ? L1[k] : L2[k], x + 82 + (float)(k % 4) * 152, 162.0f + (float)(k / 4) * 236, 84, qq);
        }
    }
    return b.pg;
}

Page modPage()
{
    using namespace hy;
    Builder b("MOD", "LFOS + MACROS", 4);
    for (int l = 0; l < 2; l++) {
        int q[4] = { M(pLfo(l, LF_RATE)), M(pLfo(l, LF_LEVEL)), M(pLfo(l, LF_DLY)), M(pLfo(l, LF_FADE)) };
        const char *const L[4] = { "RATE", "LEVEL", "DELAY", "FADE IN" };
        quad(b, 12.0f + (float)l * 312.0f, 300, "LFO " + String(l + 1), M(pLfo(l, LF_WAVE)), "WAVE", q, L, -1);
    }
    b.sec("MACROS", 636, 632);
    for (int k = 0; k < 8; k++)
        b.K(M(P_MC_VAL1 + k), "MACRO " + String(k + 1), 636 + 82 + (float)(k % 4) * 156, 130.0f + (float)(k / 4) * 206, 80, (k / 4) * 4 + k % 4);
    /* the LFOs on Q-Link rows 3 and 4 */
    for (auto &c : b.pg.ctls)
        if (c.kind == Ctl::KNOB && c.q < 0 && c.cx < 636) {
            int l = c.cx < 324 ? 0 : 1, k = (c.cx > (l ? 468.0f : 162.0f) ? 1 : 0) + (c.cy > 300 ? 2 : 0);
            c.q = (2 + k / 2) * 4 + l * 2 + k % 2;
        }
    b.B(kVoices, "VOICES", 636 + 120, 516, Ctl::CYCLE, 150, Lit::white);
    b.B(kQuality, "QUALITY", 636 + 316, 516, Ctl::CYCLE, 150, Lit::white);
    b.B(kInit, "INIT", 636 + 512, 516, Ctl::MOMENTARY, 150, Lit::orange);
    return b.pg;
}

Page fxPage()
{
    using namespace hy;
    Builder b("FX/ARP", "EFFECTS + ARP", 5);
    const int q1[4] = { M(P_PREFX + FX_PRESET), M(P_PREFX + FX_P1), M(P_PREFX + FX_P1 + 1), M(P_PREFX + FX_WET) };
    static const char *const L1[4] = { "PRESET", "PARAM 1", "PARAM 2", "DRY / WET" };
    quad(b, 12, 300, "PRE-FX", M(P_PREFX + FX_TYPE), "TYPE", q1, L1, 0);
    const int q2[4] = { M(P_DL_TIME), M(P_DL_FB), M(P_DL_WTONE), M(P_DL_WET) };
    static const char *const L2[4] = { "TIME", "FEEDBACK", "TONE", "DRY / WET" };
    quad(b, 324, 300, "DELAY", M(P_DL_TYPE), "TYPE", q2, L2, 1);
    const int q3[4] = { M(P_RV_TIME), M(P_RV_PRE), M(P_RV_TONE), M(P_RV_WET) };
    static const char *const L3[4] = { "TIME", "PREDELAY", "TONE", "DRY / WET" };
    quad(b, 636, 300, "REVERB", M(P_RV_TYPE), "TYPE", q3, L3, 2);
    b.sec("ARPEGGIATOR", 948, 320);
    b.O(M(P_AR_MODE), "MODE", { 962, 50, 292, 66 });
    const int q4[4] = { P_AR_DIV, P_AR_OCT, P_AR_GATE, P_AR_SWING };
    static const char *const L4[4] = { "DIVISION", "OCTAVE", "GATE", "SWING" };
    for (int k = 0; k < 4; k++)
        b.K(M(q4[k]), L4[k], 948 + (k % 2 ? 0.73f : 0.27f) * 320, 196.0f + (float)(k / 2) * 176, 72, k * 4 + 3);
    b.B(M(P_AR_ON), "ARP ON", 1030, 520, Ctl::TOGGLE, 120, Lit::blue);
    b.B(M(P_AR_LATCH), "LATCH", 1186, 520, Ctl::TOGGLE, 120, Lit::blue);
    return b.pg;
}

/* ---------------------------------------------------------------------------------- background --- */
Image background(const Page &pg)
{
    Image img(Image::RGB, TW, TH, true);
    Graphics g(img);
    drawPanel(g, { 0, 0, (float)TW, (float)TH }, 1.0f);
    drawMetal(g, { 0, 0, (float)TW, (float)TH }, 1.0f);
    for (auto &s : pg.secs)
        drawWell(g, s.r, s.title);
    for (auto &f : pg.extra) f(g);
    drawSurface(g, { 0, 0, (float)TW, (float)TH }, 1.0f);
    g.setFont(font(Face::heavy, 20.0f).withExtraKerningFactor(0.18f));
    g.setColour(col::legend);
    g.drawText("HYDRATE", Rectangle<float>(20, TH - 36, 140, 28), juce::Justification::centredLeft);
    g.setFont(font(Face::label, 14.0f).withExtraKerningFactor(0.16f));
    g.setColour(col::title);
    g.drawText(pg.title, Rectangle<float>(170, TH - 36, 400, 28), juce::Justification::centredLeft);
    g.setFont(font(Face::label, 12.0f));
    g.setColour(col::dim);
    g.drawText(String::fromUTF8("DRAG A KNOB OR AN OLED  \xc2\xb7  DOUBLE TAP: THE MPC'S EDITOR"),
               Rectangle<float>(560, TH - 36, 520, 28), juce::Justification::centred);
    drawTag(g, { (float)TW - 170, (float)TH - 34, 150, 24 });
    return img;
}

Image cut(const Image &bg, juce::Rectangle<int> r, std::function<void(Graphics &)> paint)
{
    Image out(Image::RGB, r.getWidth(), r.getHeight(), true);
    Graphics g(out);
    g.drawImageAt(bg, -r.getX(), -r.getY());
    g.addTransform(juce::AffineTransform::translation((float)-r.getX(), (float)-r.getY()));
    paint(g);
    return out;
}
void writePng(const Image &img, const juce::File &f)
{
    f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
}
var rectVar(juce::Rectangle<int> r) { return juce::Array<var>{ r.getX(), r.getY(), r.getWidth(), r.getHeight() }; }
} // namespace

int main(int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::fprintf(stderr, "usage: %s <out folder> [--preview]\n", argv[0]); return 2; }
    bool preview = argc > 2 && String(argv[2]) == "--preview";
    juce::File dir = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    dir.deleteRecursively();
    dir.createDirectory();
    /* the fonts: an editor's resources */
    HydrateProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    {
        hy::Patch f = hy::factory(0);
        startPlain[kVolume] = 0.7f;
        for (int p = 0; p < hy::NPARAM; p++) startPlain[M(p)] = toPlain(p, f.v[p]);
        startPlain[kVoices] = startPlain[kQuality] = startPlain[kInit] = 0;
    }
    std::vector<Page> pages = { oscPage(), mutantPage(), filterPage(), envPage(), modPage(), fxPage() };
    juce::Array<var> pageList;
    int pn = 0;
    for (auto &pg : pages) {
        String tag = pg.name.toLowerCase().replace("/", "");
        Image bg = background(pg);
        if (preview) {
            Graphics g(bg);
            for (auto &c : pg.ctls) {
                float v = startPlain[c.p];
                paintCtl(g, c, v, c.kind == Ctl::BUTTON && c.mode == Ctl::TOGGLE && v > 0.5f);
            }
            writePng(bg, dir.getChildFile(String(++pn) + "-" + tag + ".png"));
            continue;
        }
        writePng(bg, dir.getChildFile(tag + "_panel.png"));
        juce::Array<var> list;
        int k = 0;
        for (auto &c : pg.ctls) {
            auto *o = new juce::DynamicObject();
            String base = tag + "_" + String(k++).paddedLeft('0', 3);
            auto in = info(c.p);
            o->setProperty("handle", H(c.p));
            o->setProperty("id", "p" + String(c.p));
            o->setProperty("show", juce::Array<var>());
            o->setProperty("q", c.q);
            o->setProperty("steps", in.steps);
            juce::Array<var> names;
            for (auto &t : in.names) names.add(t);
            o->setProperty("names", names);
            auto r = c.r.getSmallestIntegerContainer();
            if (c.kind == Ctl::KNOB || c.kind == Ctl::OLED) {
                int frames = in.cont ? std::max(2, std::min(128, 16000 / (r.getHeight() + 1))) : in.steps;
                if (frames * r.getHeight() > 32000)
                    std::printf("  (%s: %d frames of %d px)\n", c.label.toRawUTF8(), frames, r.getHeight());
                Image strip(Image::RGB, r.getWidth(), r.getHeight() * frames, true);
                {
                    Graphics sg(strip);
                    for (int f = 0; f < frames; f++) {
                        float v = in.cont ? (float)f / (float)(frames - 1) : (float)f;
                        sg.drawImageAt(cut(bg, r, [&](Graphics &g) { paintCtl(g, c, v); }), 0, f * r.getHeight());
                    }
                }
                String f = base + "_knob.png";
                writePng(strip, dir.getChildFile(f));
                o->setProperty("kind", "knob");
                o->setProperty("bipolar", in.bipolar);
                o->setProperty("rect", rectVar(r));
                o->setProperty("frames", frames);
                o->setProperty("strip", f);
            } else {
                o->setProperty("kind", "button");
                o->setProperty("rect", rectVar(r));
                o->setProperty("mode", c.mode == Ctl::CYCLE ? "cycle" : c.mode == Ctl::MOMENTARY ? "momentary" : "toggle");
                o->setProperty("set", 0);
                if (c.mode == Ctl::CYCLE) {
                    juce::Array<var> imgs, nx;
                    for (int v = 0; v < in.steps; v++) {
                        String f = base + "_v" + String(v) + ".png";
                        writePng(cut(bg, r, [&](Graphics &g) { paintCtl(g, c, (float)v, true); }), dir.getChildFile(f));
                        imgs.add(f);
                        nx.add((v + 1) % in.steps);
                    }
                    o->setProperty("imgs", imgs);
                    o->setProperty("next", nx);
                } else {
                    String on = base + "_on.png", off = base + "_off.png";
                    writePng(cut(bg, r, [&](Graphics &g) { paintCtl(g, c, 1, true); }), dir.getChildFile(on));
                    writePng(cut(bg, r, [&](Graphics &g) { paintCtl(g, c, 0, false); }), dir.getChildFile(off));
                    o->setProperty("on", on);
                    o->setProperty("off", off);
                }
            }
            list.add(var(o));
        }
        auto *po = new juce::DynamicObject();
        po->setProperty("name", pg.name);
        po->setProperty("key", pg.key);
        po->setProperty("background", tag + "_panel.png");
        po->setProperty("controls", list);
        pageList.add(var(po));
        std::printf("%-8s %d controls\n", pg.name.toRawUTF8(), (int)pg.ctls.size());
    }
    if (preview) return 0;
    /* Program mode's Q-Links (bottom left Q-Link 1): 1-4 MACRO 1-4; 5-8 ENV 2's ADSR; 9-12 ENV 1's ADSR;
       13-16 CUTOFF, RESONANCE, ENV 1 AMT, DRIVE */
    using namespace hy;
    juce::Array<var> program;
    for (int p : { M(P_MC_VAL1), M(P_MC_VAL1 + 1), M(P_MC_VAL1 + 2), M(P_MC_VAL1 + 3), M(pEnv(1, EN_ATK)), M(pEnv(1, EN_DEC)),
                   M(pEnv(1, EN_SUS)), M(pEnv(1, EN_REL)), M(pEnv(0, EN_ATK)), M(pEnv(0, EN_DEC)), M(pEnv(0, EN_SUS)),
                   M(pEnv(0, EN_REL)), M(P_F1_CUT), M(P_F1_RES), M(P_F1_ENV), M(P_F1_DRIVE) })
        program.add(p);
    auto *m = new juce::DynamicObject();
    m->setProperty("pages", pageList);
    m->setProperty("program", program);
    m->setProperty("nparams", kNumParams);
    dir.getChildFile("manifest.json").replaceWithText(juce::JSON::toString(var(m)));
    std::printf("manifest.json in %s\n", dir.getFullPathName().toRawUTF8());
    return 0;
}
