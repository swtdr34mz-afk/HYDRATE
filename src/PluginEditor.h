/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
/* The editor: a Hydrasynth's top panel (no keys), drawn at 1524 x 1060 and scaled to the window (drag the corner):
 *   SETUP (HYDRATE's own: VOICES 8 / 16, QUALITY, HOST SYNC, PANIC, the voices' LEDs) under the name and MASTER VOLUME,
 *   ARPEGGIATOR (Tempo, Ratchet, Chance, Gate, Mode, Octave, Division, Swing; ON / EDIT, LATCH, TRIPLET, TAP TEMPO),
 *   MAIN SYSTEMS (the patch encoder, its display, BROWSE, SAVE, INIT, RANDOM, -1 / +1, HOME),
 *   MASTER CONTROL (eight encoders with LED rings, eight buttons, the display: the module's page; EXIT, VOICE, PAGE,
 *   MACRO ASSIGN, MOD MATRIX), FILTER CONTROLS (Cutoff, Resonance, Drive / Morph, ENV 1, LFO 1; FILTER 1 / 2) and
 *   MODULE SELECT (the signal flow: each module's button opens its pages, as on the instrument).
 * With the mouse: drag a knob up and down (Shift: fine), the wheel turns it, a double-click sets it to its default;
 * a click on a display field opens its list (waves, sources, destinations, types). */
#pragma once
#include "PluginProcessor.h"
#include "ui/Look.h"

class HydratePanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1524, H = 1060;
    explicit HydratePanel(HydrateProcessor &);
    ~HydratePanel() override;
    void paint(juce::Graphics &) override;
    void mouseDown(const juce::MouseEvent &) override;
    void mouseDrag(const juce::MouseEvent &) override;
    void mouseUp(const juce::MouseEvent &) override;
    void mouseDoubleClick(const juce::MouseEvent &) override;
    void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &) override;

    enum Module {
        M_HOME, M_OSC1, M_OSC2, M_OSC3, M_MUT1, M_MUT2, M_MUT3, M_MUT4, M_RING, M_MIXER, M_F1, M_F2, M_AMP, M_PREFX,
        M_DELAY, M_REVERB, M_POSTFX, M_ENV1, M_LFO1 = M_ENV1 + 5, M_VOICE = M_LFO1 + 5, M_ARP, M_MACRO, M_MOD, NMODULE
    };
    /* a field of the display: its label, its value, how a knob turns it, what its button does */
    struct Entry {
        juce::String label;
        std::function<juce::String()> text;
        std::function<juce::String()> text2;            /* a second line (the mod matrix: the destination) */
        std::function<float()> getN;                     /* 0..1 */
        std::function<void(float)> setN;
        std::function<void()> begin, end;
        std::function<void()> press;                     /* its Control button */
        std::function<bool()> lit;
        int param = -1;
        float stepN = 0.0f, defN = 0.0f;
        bool bip = false, editSecond = false;
        bool valid() const { return (bool)getN || (bool)press; }
    };
    void open(Module m, int page = 0);
    std::vector<Entry> entries();                        /* the page shown: eight */
    int pages();

    Module module = M_HOME;
    int page = 0, macroSel = -1, touched = -1, filterSel = 0;
    bool modDst[hy::NMOD] = {};

private:
    HydrateProcessor &proc;
    hyui::Look look;
    struct Ctl {
        juce::Rectangle<float> r;
        enum Kind { KNOB, MKNOB, BUTTON, CELL, ENCODER } kind = KNOB;
        int index = -1;                                  /* MKNOB, CELL: the field */
        std::function<float()> getN;
        std::function<void(float)> setN;
        std::function<void()> begin, end, click;
        std::function<bool()> on;
        float stepN = 0.0f, defN = 0.0f;
        bool bip = false, ring = false;
        juce::String label;
        hyui::Lit lit = hyui::Lit::white;
        float textSize = 10.0f;
        juce::AudioProcessorParameter *hp = nullptr;     /* its host parameter (right click: the host's menu) */
    };
    std::vector<Ctl> ctls;
    juce::AudioProcessorParameter *hostParamFor(const Ctl &c);
    bool hostMenu(const juce::MouseEvent &ev);
    juce::PopupMenu::Options menuOptions(juce::Rectangle<int> screen);
    void randomModule();
    int active = -1;
    float dragStart = 0.0f, dragAcc = 0.0f;
    bool pressed = false;
    juce::Image background;
    float bgScale = 0.0f;
    double sig = 0.0;
    std::vector<double> taps;
    int encAcc = 0;

    void build();
    void timerCallback() override;
    void paintBackground(juce::Graphics &, float density);
    void paintDisplay(juce::Graphics &);
    void paintMainDisplay(juce::Graphics &);
    Ctl &knob(int param, float cx, float cy, float size, const juce::String &label);
    Ctl &button(juce::Rectangle<float> r, const juce::String &text, std::function<void()> click, std::function<bool()> on,
                hyui::Lit lit = hyui::Lit::white, float textSize = 10.0f);
    Entry param(int p, const juce::String &label = {}, bool bpm = false);
    Entry fxEntry(int post, int k);
    Entry stepEntry(int lfo, int s);
    void menuFor(const Entry &e, juce::Rectangle<int> screen);
    void paramMenu(int p, juce::Rectangle<int> screen);
    void programMenu(juce::Rectangle<int> screen);
    void stepProgram(int d);
    void saveDialog();
    void initMenu(juce::Rectangle<int> screen, bool random);
    int ctlAt(juce::Point<float> p) const;
    juce::String moduleName(Module m) const;
    juce::Component::SafePointer<juce::AlertWindow> dialog;
};

class HydrateEditor : public juce::AudioProcessorEditor
{
public:
    explicit HydrateEditor(HydrateProcessor &);
    void resized() override;

private:
    HydrateProcessor &proc;
    juce::SharedResourcePointer<hyui::UiResources> resources;
    HydratePanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HydrateEditor)
};
