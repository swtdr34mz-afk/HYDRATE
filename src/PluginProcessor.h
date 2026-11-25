/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
/* HYDRATE as a plugin (VST3) and a standalone app: the engine (dsp/Engine.h), every patch parameter for the host
 * (automation, saved with the project), the factory patches and the user's own as the host's programs, MIDI in
 * (notes, velocity, poly and channel aftertouch, pitch bend, mod wheel, breath, expression, sustain, any CC for the
 * mod matrix), the host's tempo and position for the arp, the synced LFOs, envelopes and delay. */
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include "Engine.h"
#include "Presets.h"

class HydrateProcessor : public juce::AudioProcessor
{
public:
    HydrateProcessor();
    ~HydrateProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "HYDRATE"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return hy::factoryCount() + (int)userPatches.size(); }
    int getCurrentProgram() override { return program; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    /* the patch's parameters */
    juce::AudioProcessorValueTreeState apvts;
    juce::RangedAudioParameter *param(int p) const { return prm[p]; }
    float value(int p) const;                  /* in its unit */
    void setValue(int p, float v);             /* (from the editor: a whole gesture) */
    void beginEdit(int p) { prm[p]->beginChangeGesture(); }
    void endEdit(int p) { prm[p]->endChangeGesture(); }
    void setValueInGesture(int p, float v);
    /* the step LFOs' steps (in the patch, not host parameters) */
    float stepValue(int lfo, int s) const;
    void setStep(int lfo, int s, float v);

    /* the editor's buttons that act on the engine */
    void tapEnvelope(int e) { tapEnv.store(e); }
    void panicNow() { panicPending.store(true); }

    /* what the engine shows */
    std::atomic<float> voiceLevel[hy::MAXV] = {};
    std::atomic<float> lfoNow[5] = {}, envNow[5] = {};
    std::atomic<float> bpmNow{120.0f};
    std::atomic<bool> beatNow{false};
    std::atomic<int> arpStep{-1};
    std::atomic<int> voicesNow{0};
    float scopeCopy(int i) const { return scope[(size_t)(i & 511)].load(std::memory_order_relaxed); }
    juce::MidiKeyboardState keyboardState;
    juce::MidiMessageCollector uiMidi;
    std::atomic<float> uiScale{0.8f};
    std::atomic<bool> uiScaleSet{false};

    /* patches */
    juce::String patchName() const;
    juce::String patchCategory() const;
    void setPatchName(const juce::String &);
    hy::Patch currentPatch() const;
    void loadPatch(const hy::Patch &p);        /* (the editor's INIT, RANDOM: a whole patch) */
    std::vector<hy::Patch> userPatches;
    static juce::File patchFolder();
    void loadUserPatches();
    int writePatch(const juce::String &name);

private:
    hy::Engine engine;
    juce::RangedAudioParameter *prm[hy::NPARAM] = {};
    std::atomic<float> *raw[hy::NPARAM] = {};
    float sent[hy::NPARAM] = {};
    int sentVoices = -1;
    int program = 0;
    float lastGain = -1.0f;
    std::atomic<bool> panicPending{false};
    std::atomic<int> tapEnv{-1};
    std::array<std::atomic<float>, 512> scope{};

    mutable juce::SpinLock patchLock;
    hy::Patch pending, live;                   /* live: the name, the steps (the editor's and the saves') */
    std::atomic<bool> pendingSet{false}, stepsDirty{false};

    void handleMidi(const juce::MidiMessage &m);
    void applyPatch(const hy::Patch &p, bool toHost);
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static void patchToXml(const hy::Patch &p, juce::XmlElement &x);
    static bool patchFromXml(const juce::XmlElement &x, hy::Patch &p);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HydrateProcessor)
};
