/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
/* A picture of the editor (a PNG), without a window: for checking the panel's drawing.
 *   HydrateShot out.png [scale] [patch] [module] [page]
 *   HydrateShot --patchtest      SAVE a patch, load it again (another instance), and through the host's state
 *   HydrateShot --clicktest      right clicks on controls must change no value; a left click must */
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include "Params.h"

int main(int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::String out = argc > 1 ? argv[1] : "shot.png";
    float scale = argc > 2 ? (float)std::atof(argv[2]) : 1.0f;
    int prog = argc > 3 ? std::atoi(argv[3]) : 0;
    if (out == "--sizetest") {               /* the window opens as it was left */
        HydrateProcessor a;
        a.uiScale = 0.9f;
        a.uiScaleSet = true;
        std::unique_ptr<juce::AudioProcessorEditor> e(a.createEditor());
        double got = (double)e->getWidth() / HydratePanel::W;
        std::printf("left at 0.90 -> opens at %.3f (%d x %d)\n", got, e->getWidth(), e->getHeight());
        e->setSize((int)(HydratePanel::W * 0.7), (int)(HydratePanel::H * 0.7));
        e.reset();
        HydrateProcessor b;                       /* a new instance: the last size used */
        std::unique_ptr<juce::AudioProcessorEditor> e2(b.createEditor());
        double got2 = (double)e2->getWidth() / HydratePanel::W;
        std::printf("new instance after 0.70 -> opens at %.3f\n", got2);
        bool ok = std::fabs(got - 0.9) < 0.01 && std::fabs(got2 - 0.7) < 0.01;
        std::printf(ok ? "size test passed\n" : "size test FAILED\n");
        return ok ? 0 : 1;
    }
    if (out == "--patchtest") {
        int bad = 0;
        HydrateProcessor a;
        a.loadPatch(hy::randomPatch(777));
        auto want = a.currentPatch();
        int prog = a.writePatch("ZZ Patch Test");
        std::printf("written as program %d (%s)\n", prog, a.getProgramName(prog).toRawUTF8());
        HydrateProcessor b;
        int found = -1;
        for (int i = 0; i < b.getNumPrograms(); i++)
            if (b.getProgramName(i) == "ZZ Patch Test") found = i;
        if (found < 0) { std::printf("saved patch not in the list\n"); bad++; }
        else {
            b.setCurrentProgram(found);
            auto got = b.currentPatch();
            int diff = 0;
            for (int i = 0; i < hy::NPARAM; i++)
                if (std::fabs(got.v[i] - want.v[i]) > 1e-3f) { if (diff < 8) std::printf("  param %d (%s): %g vs %g\n", i, hy::pdef(i).id, got.v[i], want.v[i]); diff++; }
            std::printf("loaded back: %s, %d values differ\n", b.patchName().toRawUTF8(), diff);
            if (diff || b.patchName() != "ZZ Patch Test") bad++;
        }
        /* the host's project: state out of one instance into a new one */
        juce::MemoryBlock st;
        b.getStateInformation(st);
        HydrateProcessor c;
        c.setStateInformation(st.getData(), (int)st.getSize());
        auto got = c.currentPatch();
        int diff = 0;
        for (int i = 0; i < hy::NPARAM; i++)
            if (std::fabs(got.v[i] - want.v[i]) > 1e-3f) diff++;
        std::printf("project reload: %s, %d values differ\n", c.patchName().toRawUTF8(), diff);
        if (diff || c.patchName() != "ZZ Patch Test") bad++;
        HydrateProcessor::patchFolder().getChildFile("ZZ Patch Test.hydrate").deleteFile();
        std::printf(bad ? "patch test FAILED\n" : "patch test passed\n");
        return bad ? 1 : 0;
    }
    HydrateProcessor proc;
    proc.setCurrentProgram(prog);
    proc.uiScale = 1.0f;
    {
        proc.setPlayConfigDetails(0, 2, 48000.0, 512);
        proc.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> buf(2, 512);
        float peak = 0.0f;
        for (int b = 0; b < 120; b++) {
            juce::MidiBuffer m;
            if (b == 2) {
                m.addEvent(juce::MidiMessage::noteOn(1, 48, (juce::uint8)100), 10);
                m.addEvent(juce::MidiMessage::noteOn(1, 55, (juce::uint8)100), 10);
                m.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 10);
            }
            buf.clear();
            proc.processBlock(buf, m);
            peak = juce::jmax(peak, buf.getMagnitude(0, 512));
        }
        juce::MemoryBlock st;
        proc.getStateInformation(st);
        proc.setStateInformation(st.getData(), (int)st.getSize());
        std::printf("processed: peak %.3f, state %d bytes\n", peak, (int)st.getSize());
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setSize(HydratePanel::W, HydratePanel::H);
    if (out == "--clicktest") {
        auto *panel = ed->getChildComponent(0);
        auto values = [&] {
            std::vector<float> v;
            for (auto *q : proc.getParameters())
                v.push_back(q->getValue());
            return v;
        };
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        auto ev = [&](juce::Point<float> p, juce::ModifierKeys m, int clicks, bool dragged, juce::Point<float> at) {
            auto now = juce::Time::getCurrentTime();
            return juce::MouseEvent(src, at, m, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, panel, panel, now, p, now, clicks, dragged);
        };
        /* volume, spread, 8/16 voices, ECO, HOST SYNC, arp ON, LOW SPLIT, a master control knob, the arp knobs */
        std::vector<juce::Point<float>> pts = {{134, 268}, {134, 172}, {85, 408}, {183, 408}, {85, 472}, {85, 536},
                                               {1094, 464}, {382, 556}, {1094, 118}, {1208, 330}};
        auto sc = (float)panel->getWidth() / (float)HydratePanel::W;
        int bad = 0;
        auto right = juce::ModifierKeys(juce::ModifierKeys::rightButtonModifier);
        for (auto p : pts) {
            p *= sc;
            auto before = values();
            panel->mouseDown(ev(p, right, 1, false, p));
            panel->mouseDrag(ev(p, right, 1, true, p.translated(0, -60)));
            panel->mouseUp(ev(p, right, 1, true, p.translated(0, -60)));
            panel->mouseDoubleClick(ev(p, right, 2, false, p));
            if (values() != before) {
                std::printf("right click at %.0f,%.0f changed a value\n", p.x, p.y);
                bad++;
            }
        }
        auto before = values();                  /* and a left click on 16 VOICES does change it */
        juce::Point<float> p(183 * sc, 408 * sc);
        panel->mouseDown(ev(p, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, false, p));
        panel->mouseUp(ev(p, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, false, p));
        if (values() == before) {
            std::printf("left click on 16 VOICES changed nothing\n");
            bad++;
        }
        for (int k = 0; k < 3; k++) {            /* RANDOM: every press a new, audible sound */
            auto name = proc.patchName();
            auto vals = values();
            juce::Point<float> rp(435 * sc, 456 * sc);
            panel->mouseDown(ev(rp, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, false, rp));
            panel->mouseUp(ev(rp, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, false, rp));
            juce::AudioBuffer<float> buf(2, 512);
            float pk = 0.0f;
            for (int b = 0; b < 60; b++) {
                juce::MidiBuffer m;
                if (b == 0)
                    m.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
                buf.clear();
                proc.processBlock(buf, m);
                pk = juce::jmax(pk, buf.getMagnitude(0, 512));
            }
            juce::MidiBuffer off;
            off.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
            proc.processBlock(buf, off);
            std::printf("RANDOM -> %s (peak %.3f)\n", proc.patchName().toRawUTF8(), pk);
            if (values() == vals || proc.patchName() == name || pk < 0.01f) {
                std::printf("RANDOM did not give a new audible sound\n");
                bad++;
            }
        }
        std::printf(bad ? "click test FAILED\n" : "click test passed (%d controls)\n", (int)pts.size());
        ed.reset();
        return bad ? 1 : 0;
    }
    if (argc > 4)
        if (auto *panel = dynamic_cast<HydratePanel *>(ed->getChildComponent(0)))
            panel->open((HydratePanel::Module)std::atoi(argv[4]), argc > 5 ? std::atoi(argv[5]) : 0);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, scale);
    juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile(out);
    f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    std::printf("wrote %s (%d x %d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
