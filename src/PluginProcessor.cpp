/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace hy;

/* ------------------------------------------------------------------------------- parameters --- */
juce::AudioProcessorValueTreeState::ParameterLayout HydrateProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int p = 0; p < NPARAM; p++) {
        const ParamDef &d = pdef(p);
        juce::NormalisableRange<float> range(d.lo, d.hi, d.step, d.skew);
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{d.id, 1}, d.name, range, d.def,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction([p](float v, int) {
                    char t[48];
                    paramText(p, v, t, sizeof t);
                    return juce::String(t);
                })
                .withValueFromStringFunction([p](const juce::String &s) {
                    const ParamDef &q = pdef(p);
                    if (q.choice)
                        for (int i = (int)q.lo; i <= (int)q.hi; i++)
                            if (s.trim().equalsIgnoreCase(q.choice[i]))
                                return (float)i;
                    return s.getFloatValue();
                })));
    }
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"voices", 1}, "Voices",
                                                            juce::StringArray{"8 Voices", "16 Voices"}, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"quality", 1}, "Quality",
                                                            juce::StringArray{"ECO (1x)", "HIGH (2x)"}, 1));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"host_sync", 1}, "Host Sync", true));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"low_split", 1}, "Low Split", false));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"split_note", 1}, "Split Note", 1, 127, 48,
        juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) {
            return juce::MidiMessage::getMidiNoteName(v, true, true, 3);
        })));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"spread", 1}, "Voice Spread", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) {
            return juce::String(juce::roundToInt(v * 100.0f)) + " %";
        })));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"volume", 1}, "Master Volume", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) {
            return v <= 0.0001f ? juce::String("-inf") : juce::String(20.0f * std::log10(v * v * 2.0f), 1) + " dB";
        })));
    return layout;
}

HydrateProcessor::HydrateProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "HYDRATE", createLayout())
{
    for (int p = 0; p < NPARAM; p++) {
        prm[p] = apvts.getParameter(pdef(p).id);
        raw[p] = apvts.getRawParameterValue(pdef(p).id);
    }
    for (auto &s : scope)
        s.store(0.0f);
    loadUserPatches();
    live = factory(0);
    applyPatch(live, true);
}

float HydrateProcessor::value(int p) const { return raw[p]->load(std::memory_order_relaxed); }
void HydrateProcessor::setValue(int p, float v)
{
    auto *q = prm[p];
    q->beginChangeGesture();
    q->setValueNotifyingHost(q->convertTo0to1(juce::jlimit(pdef(p).lo, pdef(p).hi, v)));
    q->endChangeGesture();
}
void HydrateProcessor::setValueInGesture(int p, float v)
{
    auto *q = prm[p];
    q->setValueNotifyingHost(q->convertTo0to1(juce::jlimit(pdef(p).lo, pdef(p).hi, v)));
}
float HydrateProcessor::stepValue(int l, int s) const
{
    juce::SpinLock::ScopedLockType lock(patchLock);
    return live.step[l][s];
}
void HydrateProcessor::setStep(int l, int s, float v)
{
    {
        juce::SpinLock::ScopedLockType lock(patchLock);
        live.step[l][s] = juce::jlimit(-64.0f, 64.0f, v);
    }
    stepsDirty.store(true);
}

/* ------------------------------------------------------------------------------- patches --- */
void HydrateProcessor::applyPatch(const Patch &p, bool toHost)
{
    {
        juce::SpinLock::ScopedLockType lock(patchLock);
        pending = p;
        live = p;
    }
    pendingSet.store(true);
    if (toHost)
        for (int i = 0; i < NPARAM; i++) {
            auto *q = prm[i];
            q->setValueNotifyingHost(q->convertTo0to1(juce::jlimit(pdef(i).lo, pdef(i).hi, p.v[i])));
        }
}
void HydrateProcessor::loadPatch(const Patch &p) { applyPatch(p, true); }

void HydrateProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    program = index;
    applyPatch(index < factoryCount() ? factory(index) : userPatches[(size_t)(index - factoryCount())], true);
    updateHostDisplay(ChangeDetails().withProgramChanged(true));
}
const juce::String HydrateProcessor::getProgramName(int index)
{
    if (index >= 0 && index < factoryCount())
        return juce::String(factory(index).name);
    if (index >= factoryCount() && index < getNumPrograms())
        return juce::String(userPatches[(size_t)(index - factoryCount())].name);
    return {};
}
juce::String HydrateProcessor::patchName() const
{
    juce::SpinLock::ScopedLockType lock(patchLock);
    return juce::String(live.name);
}
juce::String HydrateProcessor::patchCategory() const
{
    juce::SpinLock::ScopedLockType lock(patchLock);
    return juce::String(live.category);
}
void HydrateProcessor::setPatchName(const juce::String &n)
{
    juce::SpinLock::ScopedLockType lock(patchLock);
    std::snprintf(live.name, NAME_LEN, "%s", n.substring(0, NAME_LEN - 1).toRawUTF8());
}
Patch HydrateProcessor::currentPatch() const
{
    Patch p;
    {
        juce::SpinLock::ScopedLockType lock(patchLock);
        p = live;
    }
    for (int i = 0; i < NPARAM; i++)
        p.v[i] = value(i);
    return p;
}

juce::File HydrateProcessor::patchFolder()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("HYDRATE").getChildFile("Patches");
}
void HydrateProcessor::patchToXml(const Patch &p, juce::XmlElement &x)
{
    x.setAttribute("name", juce::String(p.name));
    x.setAttribute("category", juce::String(p.category));
    x.setAttribute("version", 1);
    for (int i = 0; i < NPARAM; i++)
        x.setAttribute(pdef(i).id, (double)p.v[i]);
    juce::MemoryBlock mb(p.step, sizeof p.step);
    x.setAttribute("steps", mb.toBase64Encoding());
}
bool HydrateProcessor::patchFromXml(const juce::XmlElement &x, Patch &p)
{
    patchInit(p);
    std::snprintf(p.name, NAME_LEN, "%s", x.getStringAttribute("name", "Patch").substring(0, NAME_LEN - 1).toRawUTF8());
    std::snprintf(p.category, sizeof p.category, "%s", x.getStringAttribute("category", "User").substring(0, 11).toRawUTF8());
    for (int i = 0; i < NPARAM; i++)
        p.v[i] = juce::jlimit(pdef(i).lo, pdef(i).hi, (float)x.getDoubleAttribute(pdef(i).id, pdef(i).def));
    juce::MemoryBlock mb;
    if (mb.fromBase64Encoding(x.getStringAttribute("steps")) && mb.getSize() == sizeof p.step)
        std::memcpy(p.step, mb.getData(), sizeof p.step);
    return true;
}
void HydrateProcessor::loadUserPatches()
{
    userPatches.clear();
    auto files = patchFolder().findChildFiles(juce::File::findFiles, false, "*.hydrate");
    files.sort();
    for (auto &f : files)
        if (auto xml = juce::XmlDocument::parse(f))
            if (xml->hasTagName("HYDRATEPATCH")) {
                Patch p;
                if (patchFromXml(*xml, p))
                    userPatches.push_back(p);
            }
}
int HydrateProcessor::writePatch(const juce::String &name)
{
    auto clean = name.trim().substring(0, NAME_LEN - 1);
    if (clean.isEmpty())
        return -1;
    Patch p = currentPatch();
    std::snprintf(p.name, NAME_LEN, "%s", clean.toRawUTF8());
    if (juce::String(p.category).isEmpty() || juce::String(p.category) == "Init")
        std::snprintf(p.category, sizeof p.category, "User");
    juce::XmlElement xml("HYDRATEPATCH");
    patchToXml(p, xml);
    auto folder = patchFolder();
    folder.createDirectory();
    if (!xml.writeTo(folder.getChildFile(juce::File::createLegalFileName(clean) + ".hydrate")))
        return -1;
    setPatchName(clean);
    loadUserPatches();
    for (size_t i = 0; i < userPatches.size(); i++)
        if (juce::String(userPatches[i].name) == clean) {
            program = factoryCount() + (int)i;
            updateHostDisplay(ChangeDetails().withProgramChanged(true));
            return program;
        }
    return -1;
}

/* ------------------------------------------------------------------------------- the block --- */
bool HydrateProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void HydrateProcessor::prepareToPlay(double sampleRate, int)
{
    uiMidi.reset(sampleRate);
    engine.prepare(sampleRate);
    Patch cur = currentPatch();
    {
        juce::SpinLock::ScopedLockType lock(patchLock);
        pending = cur;
    }
    pendingSet.store(true);
    sentVoices = -1;
    lastGain = -1.0f;
}

void HydrateProcessor::handleMidi(const juce::MidiMessage &m)
{
    if (m.isNoteOn()) {
        if (splitLearn.exchange(false)) {          /* LOW SPLIT: the first key after switching on sets the split */
            learnedSplit.store(m.getNoteNumber());
            engine.setSplit(true, m.getNoteNumber());
        }
        engine.noteOn(m.getNoteNumber(), m.getFloatVelocity());
    }
    else if (m.isNoteOff())
        engine.noteOff(m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isPitchWheel())
        engine.bend((float)(m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isChannelPressure())
        engine.aftertouch((float)m.getChannelPressureValue() / 127.0f);
    else if (m.isAftertouch())
        engine.polyAftertouch(m.getNoteNumber(), (float)m.getAfterTouchValue() / 127.0f);
    else if (m.isSustainPedalOn() || m.isSustainPedalOff())
        engine.sustain(m.isSustainPedalOn());
    else if (m.isController()) {
        int cc = m.getControllerNumber();
        float v = (float)m.getControllerValue() / 127.0f;
        engine.cc(cc, v);
        switch (cc) {
        case 1: engine.modWheel(v); break;
        case 120: engine.panic(); break;
        case 123: engine.allOff(); break;
        case 121:
            engine.bend(0.0f);
            engine.modWheel(0.0f);
            engine.aftertouch(0.0f);
            engine.sustain(false);
            break;
        default: break;
        }
    } else if (m.isAllNotesOff() || m.isAllSoundOff())
        engine.allOff();
}

void HydrateProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    float *L = buffer.getWritePointer(0), *R = buffer.getWritePointer(buffer.getNumChannels() > 1 ? 1 : 0);

    if (pendingSet.exchange(false)) {
        Patch p;
        {
            juce::SpinLock::ScopedLockType lock(patchLock);
            p = pending;
        }
        engine.setPatch(p);
        for (int i = 0; i < NPARAM; i++)
            sent[i] = p.v[i];
    }
    if (stepsDirty.load()) {
        juce::SpinLock::ScopedTryLockType lock(patchLock);
        if (lock.isLocked()) {
            for (int l = 0; l < 5; l++)
                for (int s = 0; s < NSTEP; s++)
                    engine.setStep(l, s, live.step[l][s]);
            stepsDirty.store(false);
        }
    }
    for (int i = 0; i < NPARAM; i++) {
        float v = value(i);
        if (v != sent[i]) {
            engine.setParam(i, v);
            sent[i] = v;
        }
    }
    int voices = apvts.getRawParameterValue("voices")->load() > 0.5f ? 16 : 8;
    if (voices != sentVoices) {
        engine.setVoices(voices);
        sentVoices = voices;
    }
    engine.setQuality(apvts.getRawParameterValue("quality")->load() > 0.5f ? 1 : 0);
    {
        int ln = learnedSplit.load();
        int sn = ln > 0 ? ln : (int)apvts.getRawParameterValue("split_note")->load();
        engine.setSplit(apvts.getRawParameterValue("low_split")->load() > 0.5f, sn);
        engine.setSpread(apvts.getRawParameterValue("spread")->load());
    }
    if (panicPending.exchange(false))
        engine.panic();
    int te = tapEnv.exchange(-1);
    if (te >= 0)
        engine.tapTrigger(te);

    bool sync = apvts.getRawParameterValue("host_sync")->load() > 0.5f;
    double bpm = 0.0, ppq = 0.0;
    bool playing = false;
    if (auto *ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            if (auto b = pos->getBpm())
                bpm = *b;
            playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition())
                ppq = *q;
        }
    engine.host(playing, ppq, bpm, sync && bpm > 10.0);

    uiMidi.removeNextBlockOfMessages(midi, n);
    keyboardState.processNextMidiBuffer(midi, 0, n, true);

    int done = 0;
    for (const auto meta : midi) {
        int at = juce::jlimit(0, n, meta.samplePosition);
        if (at > done) {
            engine.render(L + done, R + done, at - done);
            done = at;
        }
        handleMidi(meta.getMessage());
    }
    if (n > done)
        engine.render(L + done, R + done, n - done);

    for (int v = 0; v < MAXV; v++)
        voiceLevel[v].store(engine.voiceLevel(v), std::memory_order_relaxed);
    for (int k = 0; k < 5; k++) {
        lfoNow[k].store(engine.lfoShown(k), std::memory_order_relaxed);
        envNow[k].store(engine.envShown(k), std::memory_order_relaxed);
    }
    bpmNow.store(engine.bpmNow(), std::memory_order_relaxed);
    beatNow.store(engine.beatLed(), std::memory_order_relaxed);
    arpStep.store(engine.arpStepShown(), std::memory_order_relaxed);
    voicesNow.store(engine.voicesSounding(), std::memory_order_relaxed);
    for (int i = 0; i < 512; i++)
        scope[(size_t)i].store(engine.scopeAt(SCOPE - 512 + i), std::memory_order_relaxed);

    float vol = apvts.getRawParameterValue("volume")->load();
    float gain = vol * vol * 2.0f;
    if (lastGain < 0.0f)
        lastGain = gain;
    buffer.applyGainRamp(0, 0, n, lastGain, gain);
    if (buffer.getNumChannels() > 1)
        buffer.applyGainRamp(1, 0, n, lastGain, gain);
    lastGain = gain;
    for (int c = 2; c < buffer.getNumChannels(); c++)
        buffer.clear(c, 0, n);
}

/* ------------------------------------------------------------------------------ state --- */
void HydrateProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state = apvts.copyState();
    state.setProperty("program", program, nullptr);
    state.setProperty("uiScale", (double)uiScale.load(), nullptr);
    state.setProperty("uiScaleSet", uiScaleSet.load(), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    auto *pg = xml->createNewChildElement("HYDRATEPATCH");
    patchToXml(currentPatch(), *pg);
    copyXmlToBinary(*xml, destData);
}

void HydrateProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (!xml || !xml->hasTagName(apvts.state.getType()))
        return;
    Patch p;
    bool hasPatch = false;
    if (auto *pg = xml->getChildByName("HYDRATEPATCH")) {
        hasPatch = patchFromXml(*pg, p);
        xml->removeChildElement(pg, true);
    }
    auto state = juce::ValueTree::fromXml(*xml);
    program = (int)state.getProperty("program", 0);
    uiScale = (float)(double)state.getProperty("uiScale", 0.8);
    uiScaleSet = (bool)state.getProperty("uiScaleSet", false);
    apvts.replaceState(state);
    if (hasPatch) {
        for (int i = 0; i < NPARAM; i++)
            p.v[i] = value(i);
        applyPatch(p, false);
    }
}

juce::AudioProcessorEditor *HydrateProcessor::createEditor() { return new HydrateEditor(*this); }

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new HydrateProcessor(); }
