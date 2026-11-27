/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
/* A picture of the editor (a PNG), without a window: for checking the panel's drawing.
 *   HydrateShot out.png [scale] [patch] [module] [page]
 *   HydrateShot --clicktest      right clicks on controls must change no value; a left click must */
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main(int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::String out = argc > 1 ? argv[1] : "shot.png";
    float scale = argc > 2 ? (float)std::atof(argv[2]) : 1.0f;
    int prog = argc > 3 ? std::atoi(argv[3]) : 0;
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
