/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
/* A picture of the editor (a PNG), without a window: for checking the panel's drawing.
 *   HydrateShot out.png [scale] [patch] [module] [page] */
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
