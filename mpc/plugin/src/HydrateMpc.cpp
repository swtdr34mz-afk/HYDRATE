// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 swtdr34mz (HYDRATE)
//
// HYDRATE for the MPC: the same engine as the VST3 / AU / LV2 (dsp/: Engine, Params, Presets, Waves, Fx, Phrases),
// hosted by the MPC Plugin SDK's framework (mpc_plugin.h). mpc/build.py copies the engine beside this file.
//
// 8 voices (16 if you dare), the filters at ECO (1x) to start with: the Hydrasynth's engine is a heavy one for the
// MPC's CPU. Parameters: hydrate_mpc_params.h. The step LFOs' steps ride in the state (saveExtraState); a factory
// program brings its own.
//
// MIDI in: notes and velocity, pitch bend, channel and poly pressure, every CC (the mod matrix's CC sources), CC 1 mod
// wheel, CC 64 sustain, CC 120 / 123 all off, CC 121 reset controllers. Tempo and position: the MPC's.

#include "mpc_plugin.h"
#include "Engine.h"
#include "Presets.h"
#include "hydrate_mpc_params.h"

#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>

namespace {
using namespace hy;
using namespace hmpc;

class HydrateMpc : public mpc::Plugin
{
public:
    HydrateMpc() : mpc::Plugin(makeInfo())
    {
        addParam(mpc::floatParam("vol", "Volume", 0.0f, 1.0f, 0.7f, "", 2));
        Patch init;
        patchInit(init);
        for (int p = 0; p < NPARAM; p++) {
            const ParamDef &d = pdef(p);
            const std::string nm = mpcName(p);
            float def = toPlain(p, init.v[p]);
            switch (typeOf(p)) {
            case CHOICE: {
                std::vector<std::string> ch;
                for (int k = (int)d.lo; k <= (int)d.hi; k++) {
                    char t[48];
                    paramText(p, (float)k, t, sizeof t, false);
                    ch.push_back(t);
                }
                addParam(mpc::choiceParam(d.id, nm.c_str(), ch, (int)std::lround(def)));
                break;
            }
            case BOOL: addParam(mpc::boolParam(d.id, nm.c_str(), def > 0.5f)); break;
            case INT: addParam(mpc::intParam(d.id, nm.c_str(), (int)d.lo, (int)d.hi, (int)std::lround(def))); break;
            default: addParam(mpc::floatParam(d.id, nm.c_str(), 0.0f, 1.0f, def, "", 3)); break;
            }
        }
        addParam(mpc::choiceParam("voices", "Voices", {"8 Voices", "16 Voices"}, 0));
        addParam(mpc::choiceParam("quality", "Quality", {"ECO", "HIGH"}, 0));
        addParam(mpc::boolParam("init", "Init", false));

        /* the factory patches as the MPC's programs (VOLUME, VOICES and QUALITY as they are at the start) */
        for (int k = 0; k < factoryCount(); k++) {
            Patch f = factory(k);
            std::vector<float> v((size_t)kNumParams, 0.0f);
            v[kVolume] = 0.7f;
            for (int p = 0; p < NPARAM; p++)
                v[(size_t)M(p)] = toPlain(p, f.v[p]);
            v[kVoices] = 0.0f;
            v[kQuality] = 0.0f;
            addProgram(f.name, v);
        }
        engine.prepare(44100.0);         /* (made here, on the UI thread, never in process) */
        rate = 44100.0;
        engine.setVoices(8);
        engine.setQuality(0);
        Patch f0 = factory(0);
        engine.setPatch(f0);
        std::memcpy(steps, f0.step, sizeof steps);
        for (auto &s : sent) s = -1e9f;
    }

    void parameterChanged(int index, float plain) override
    {
        if (index == kInit && plain > 0.5f && !busy)
            initRequest.store(true);
    }

    void prepare(double sampleRate, int) override
    {
        if (sampleRate != rate) {
            engine.prepare(sampleRate);
            rate = sampleRate;
        }
        for (auto &s : sent) s = -1e9f;
    }

    void reset() override
    {
        engine.allOff();
        if (stagedReady.exchange(false))
            std::memcpy(steps, staged, sizeof steps);
        stepsDirty = true;
        for (auto &s : sent) s = -1e9f;
    }

    void process(mpc::ProcessContext &ctx) override
    {
        const int n = ctx.numFrames;
        float *L = ctx.outputs[0], *R = ctx.numOutputs > 1 ? ctx.outputs[1] : ctx.outputs[0];
        /* a factory program chosen: its step LFOs */
        if (currentProgram() != lastProgram) {
            lastProgram = currentProgram();
            if (lastProgram >= 0 && lastProgram < factoryCount()) {
                Patch f = factory(lastProgram);
                std::memcpy(steps, f.step, sizeof steps);
                stepsDirty = true;
            }
        }
        if (initRequest.exchange(false)) {
            Patch init;
            patchInit(init);
            busy = true;
            for (int p = 0; p < NPARAM; p++) {
                set(M(p), toPlain(p, init.v[p]));
                notifyHostParameterChanged(M(p));
            }
            set(kInit, 0.0f);
            notifyHostParameterChanged(kInit);
            busy = false;
            std::memset(steps, 0, sizeof steps);
            stepsDirty = true;
        }
        if (stepsDirty) {
            for (int l = 0; l < 5; l++)
                for (int s = 0; s < NSTEP; s++)
                    engine.setStep(l, s, steps[l][s]);
            stepsDirty = false;
        }
        for (int p = 0; p < NPARAM; p++) {
            float v = get(M(p));
            if (v != sent[p]) {
                engine.setParam(p, toEngine(p, v));
                sent[p] = v;
            }
        }
        engine.setVoices(getInt(kVoices) > 0 ? 16 : 8);
        engine.setQuality(getInt(kQuality) > 0 ? 1 : 0);
        const auto &t = ctx.transport;
        engine.host(t.valid && t.playing, t.valid && t.ppqValid ? t.ppqPos : 0.0,
                    t.valid && t.tempoValid ? t.bpm : 120.0, true);
        float vol = get(kVolume), target = vol * vol * 2.0f;
        int done = 0;
        for (int i = 0; i < ctx.numMidiEvents; i++) {
            const mpc::MidiEvent &e = ctx.midi[i];
            int at = mpc::clamp(e.sampleOffset, 0, n);
            if (at > done) {
                render(L, R, done, at - done, target);
                done = at;
            }
            midi(e);
        }
        if (n > done)
            render(L, R, done, n - done, target);
    }

    bool formatValue(int index, float plain, std::string &out) override
    {
        int p = E(index);
        if (p < 0)
            return false;
        char t[48];
        paramText(p, toEngine(p, plain), t, sizeof t, false);
        out = t;
        return true;
    }

    void saveExtraState(std::vector<uint8_t> &out) override
    {
        out.resize(sizeof steps);
        std::memcpy(out.data(), steps, sizeof steps);
    }
    void loadExtraState(const uint8_t *data, size_t size) override
    {
        if (size != sizeof staged)
            return;
        std::memcpy(staged, data, sizeof staged);   /* (taken over in reset(), on the audio thread) */
        stagedReady.store(true);
    }

private:
    static mpc::PluginInfo makeInfo()
    {
        mpc::PluginInfo i;
        i.name = "HYDRATE";
        i.vendor = "swtdr34mz";
        i.uniqueId = MPC_FOURCC('H', 'y', 'd', 'r');
        i.version = 1000;
        i.isInstrument = true;
        i.numInputs = 0;
        i.numOutputs = 2;
        i.tailSamples = 44100 * 12;
        return i;
    }

    void render(float *L, float *R, int at, int count, float target)
    {
        while (count > 0) {
            int m = count < 256 ? count : 256;
            float tmpL[256], tmpR[256];
            engine.render(tmpL, tmpR, m);
            for (int i = 0; i < m; i++) {
                gain += (target - gain) * 0.002f;
                L[at + i] = tmpL[i] * gain;
                if (R != L)
                    R[at + i] = tmpR[i] * gain;
            }
            at += m;
            count -= m;
        }
    }

    void midi(const mpc::MidiEvent &e)
    {
        const uint8_t st = e.status();
        if (e.isNoteOn())
            engine.noteOn(e.note(), (float)e.velocity() / 127.0f);
        else if (e.isNoteOff())
            engine.noteOff(e.note());
        else if (e.isPitchBend())
            engine.bend((float)e.pitchBend() / 8192.0f);
        else if (st == 0xA0)
            engine.polyAftertouch(e.data[1], (float)e.data[2] / 127.0f);
        else if (e.isAftertouch())
            engine.aftertouch((float)e.data[1] / 127.0f);
        else if (e.isController()) {
            int cc = e.controller(), v = e.controllerValue();
            engine.cc(cc, (float)v / 127.0f);
            if (cc == 1)
                engine.modWheel((float)v / 127.0f);
            else if (cc == 64)
                engine.sustain(v >= 64);
            else if (cc == 120 || cc == 123)
                engine.allOff();
            else if (cc == 121) {
                engine.bend(0.0f);
                engine.modWheel(0.0f);
                engine.aftertouch(0.0f);
                engine.sustain(false);
            }
        }
    }

    Engine engine;
    double rate = 0.0;
    float sent[NPARAM];
    float steps[5][NSTEP] = {}, staged[5][NSTEP] = {};
    float gain = 0.0f;
    int lastProgram = 0;
    bool busy = false, stepsDirty = true;
    std::atomic<bool> initRequest{false}, stagedReady{false};
};

} // namespace

MPC_PLUGIN_ENTRY(HydrateMpc)
