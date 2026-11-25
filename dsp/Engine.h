/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
/* HYDRATE's engine: the Hydrasynth's voice, as its manual describes it.
 *
 *   voices      8 or 16 (the Keyboard's / Desktop's and the Deluxe's), Poly / Mono (Lo, Hi) / Unison (Lo, Hi) with
 *               Density 1-8 and Detune, Stereo Mode (Rotate, Alter, Random) and Width, Analog Feel, Random Phase,
 *               Warm Mode, Snap, Glide (Glide, Glissando; curve, legato), mod-wheel Vibrato, scales and Key Lock,
 *               Sustain / Sostenuto / Mod Only pedal, VoiceMod (a value per voice)
 *   oscillators three, 219 waves each (osc 1 and 2: WaveScan through a list of eight), BitRedux, Keytrack
 *   mutants     two in series after osc 1 and after osc 2: FM-Lin, WavStack, OSC Sync, PW-Orig, PW-Sqeez, PW-ASM
 *               (eight Warp points), Harmonic, PhazDiff. They work on the wave itself (its phase): a Mutant reads the
 *               stage before it at the phase it wants, so FM, sync and the PWMs stay clean (and Dry/Wet blends them)
 *   ring-noise  a ring modulator (two sources of eight, Depth), noise in seven colours
 *   mixer       five sources, each with level, pan and its share of Filter 1 / Filter 2; Series or Parallel
 *   filter 1    16 types (ladders 12/24 uncompensated and 'Fat' compensated, LP Gate, MS-20 LP/HP, 3-Ler LP/BP/HP,
 *               Stn12 LP/BP/HP, 1 pole, 8 pole, Vowel), Drive (pre or post), stereo
 *   filter 2    the 12 dB state-variable that morphs LP-BP-HP or LP-Notch-HP
 *   amp         ENV 2 (fixed), LFO 2, velocity, level (the ceiling)
 *   modulation  five DAHDSR envelopes (curves, loop, BPM, quantize, legato, reset, freerun, four triggers), five LFOs
 *               (eleven waves, 64-step, delay, fade, phase, smooth, quantize, one-shot / step, poly / single / free),
 *               32 mod-matrix slots of the manual's sources and destinations, eight macros of eight destinations
 *   arp         Up, Down, Up/Dn, Up & Dn, Order, Random, Chord, Phrase (64); octave modes, ratchet, chance, swing,
 *               gate, length, offset, latch
 *   effects     Pre-FX, Delay, Reverb, Post-FX (Fx.h)
 *
 * The voices run at twice the sample rate (Quality HIGH; ECO: once), the modulation every 16 samples (0.36 ms),
 * glided across. All calls from the audio thread. */
#pragma once
#include "Dsp.h"
#include "Fx.h"
#include "Params.h"
#include "Waves.h"
#include <atomic>

namespace hy
{
constexpr int MAXV = 16, CH = 16, NKEYS = 32, SCOPE = 1024;

class Engine
{
public:
    Engine();
    void prepare(double sampleRate);
    void setPatch(const Patch &p);
    const Patch &patch() const { return pat; }
    void setParam(int p, float v);
    void setStep(int lfo, int step, float v);
    void setVoices(int n);
    void setQuality(int q) { os = q ? 2 : 1; }
    int voiceCount() const { return nVoices; }

    void noteOn(int note, float vel);
    void noteOff(int note, float vel = 0.5f);
    void sustain(bool on);
    void bend(float v);              /* -1..1 */
    void modWheel(float v);          /* 0..1 */
    void aftertouch(float v);
    void polyAftertouch(int note, float v);
    void cc(int n, float v);
    void allOff();
    void panic();
    void host(bool playing, double ppq, double bpm, bool sync);
    void tapTrigger(int env);        /* an envelope's TAP TRIGGER */

    void render(float *L, float *R, int n);

    /* for the editor */
    int voicesSounding() const;
    float voiceLevel(int v) const { return v >= 0 && v < MAXV ? V[v].shown : 0.0f; }
    float lfoShown(int l) const { return lfoShow[l]; }
    float envShown(int e) const { return envShow[e]; }
    float bpmNow() const { return bpm; }
    bool beatLed() const { return beatPos - std::floor(beatPos) < 0.5; }
    int arpStepShown() const { return arpShown; }
    float scopeAt(int i) const { return scope[(scopeW + i) & (SCOPE - 1)]; }
    float oscOut(int o) const { return oscShow[o]; }

private:
    /* ------------------------------------------------------------ state --- */
    struct Env {
        uint8_t st = 0;              /* ES_* */
        float v = 0.0f, t = 0.0f, from = 0.0f, prev = 0.0f;
        int loops = 0;
        bool armed = false;
    };
    enum { ES_IDLE, ES_DELAY, ES_ATT, ES_HOLD, ES_DEC, ES_SUS, ES_REL };
    struct Lfo {
        float ph = 0.0f, out = 0.0f, raw = 0.0f, sm = 0.0f, sh = 0.0f, r0 = 0.0f, r1 = 0.0f;
        float dly = 0.0f, fade = 0.0f;
        int step = 0;
        bool done = false, wrap = false, started = false;
        uint32_t seed = 1;
    };
    struct Mut {
        float modPh = 0.0f, y1 = 0.0f, y2 = 0.0f, harmY = 0.0f;
        float stack[5] = {};
        Svf hs;
        /* this chunk's (ramped) */
        int mode = 0;
        float r = 1, rI = 0, d = 0, dI = 0, m = 0, mI = 0, w = 0, f = 0, warp[9] = {};
        int src = 0, syncSrc = 0;
        float sVal = 0, fbT = 0, spread = 0, hg = 0.01f, hk = 1.0f;
        bool on = false;
    };
    struct F1 {
        float s[8] = {};
        Svf a, b, form[3];
        OnePole one;
        void clear() { *this = F1(); }
    };
    struct Voice {
        int idx = 0;                 /* its place (VoiceMod, the stereo spread) */
        bool active = false, gate = false, sus = false, sost = false;
        int note = -1, key = -1;     /* key: what plays it (a key, the arp's) */
        float vel = 0.0f, offVel = 0.0f, press = 0.0f;
        uint32_t age = 0, seed = 1;
        float pitch = 60.0f, gFrom = 60.0f, gTo = 60.0f, gU = 1.0f, gDur = 0.0f;
        float uniDet = 0.0f, panPos = 0.0f;
        /* oscillators */
        float ph[3] = {}, inc[3] = {}, incT[3] = {};
        const float *tA[3] = {}, *tB[3] = {};
        float scanA[3] = {}, scanAT[3] = {};
        float bitsQ[3] = {};
        Mut mu[4];
        float mo[4] = {}, oo[3] = {}, ring = 0, noise = 0;
        float pink[7] = {}, brown = 0, red = 0, prevW = 0, prevP = 0;
        /* mixer gains: source x (F1 L, F1 R, F2 L, F2 R), ramped */
        float mg[5][4] = {}, mgT[5][4] = {};
        /* filters */
        F1 f1[2];
        Svf f2[2];
        HalfBand hb[2];
        float c1 = 64, c1T = 64, c2 = 64, c2T = 64, r1 = 0, r2 = 0, morph = 0, drive = 0, gateAmp = 1;
        int f1Type = 0, f2Type = 0;
        bool drvPost = false, series = true, useF1 = true, useF2 = true, monoMix = true;
        float vow[3][2] = {}, vowQ = 4;
        /* amp */
        float amp = 0.0f, ampT = 0.0f, fadeIn = 0.0f, fadeK = 0.0f;
        float shown = 0.0f;
        /* modulation */
        Env env[5];
        Lfo lfo[5];
        float envScale = 1.0f, drift[3] = {}, driftT[3] = {}, driftOff[3] = {}, cutOff = 0.0f;
        int driftN = 0;
        float val[NPARAM];
        uint16_t dirty[400];
        int nDirty = 0;
        float pmod[4] = {};          /* pitch: osc 1, 2, 3, all (semitones) */
        float src[SRC_CC0] = {};
    };

    Patch pat;
    Voice V[MAXV];
    Voice ghost;                     /* the modulation with no voice playing: the global parameters' */
    const float *gval = nullptr;     /* the global parameters as modulated now */
    int nVoices = 8, os = 2;
    float sr = 44100.0f;
    uint32_t ageN = 0, rseed = 0x2545F491u;
    int rotNext = 0, lastVoice = -1;

    /* performance */
    float bendV = 0, mwV = 0, atV = 0, expV = 0, brV = 0, susV = 0;
    float ccV[128] = {};
    bool susOn = false;
    int8_t keys[NKEYS];
    float keyVel[NKEYS];
    int nKeys = 0, lastNote = 60;
    float lastPitch = 60.0f;
    bool sostHeld[128] = {};

    /* global LFOs (TrigSync Single / Off), vibrato */
    Lfo glfo[5];
    float vibPh = 0.0f;
    float lfoShow[5] = {}, envShow[5] = {}, oscShow[3] = {};

    /* clock */
    bool hostPlaying = false, hostSync = true;
    double hostPpq = 0.0, beatPos = 0.0;
    float bpm = 120.0f, hostBpm = 120.0f;

    /* arpeggiator */
    int8_t arpHeld[NKEYS];
    float arpVel[NKEYS];
    int arpN = 0;
    int16_t arpSeq[256];           /* a note, -1 rest, -2 tie, 1000 + octave: the chord */
    int arpLen = 0, arpIdx = -1, arpShown = -1;
    double arpNext = 0.0, arpOff = -1.0;
    int arpPlaying[8];
    int arpNPlaying = 0, arpRatchetLeft = 0;
    double arpRatchetStep = 0.0, arpRatchetNext = 0.0;
    long arpCount = 0;
    bool arpRunning = false, arpLatched = false;

    /* the output */
    float bufL[CH] = {}, bufR[CH] = {};
    int gpos = CH;
    FxUnit pre, post;
    Delay dly;
    Reverb rev;
    Biquad warmLs[2], warmLp[2];
    bool warmSet = false;
    float scope[SCOPE] = {};
    int scopeW = 0;

    /* ------------------------------------------------------------ parts --- */
    void chunk();
    void clock();
    int quantizeNote(int n) const;
    void keyDown(int note, float vel);
    void keyUp(int note);
    void playNote(int key, int note, float vel);
    void stopNote(int key);
    void startVoice(Voice &v, int key, int note, float vel, float det, bool retrig, bool legato);
    void releaseVoice(Voice &v);
    int findVoice();
    void monoUpdate();
    void arpRebuild();
    void arpTick();

    void envGate(Voice &v, int e, bool retrig);
    void envRelease(Voice &v, int e);
    void envStep(Voice &v, int e, float dt);
    float envTime(const float *P, int e, int k) const;
    void lfoTrig(Lfo &l, const float *P, int k);
    void lfoStep(Lfo &l, const float *P, int k, float dt, Voice *v);
    float lfoRate(const float *P, int k) const;
    void sources(Voice &v, bool isGhost);
    void modulate(Voice &v);
    void voiceSetup(Voice &v);
    void voiceRender(Voice &v, float *L, float *R);
    float evalChain(Voice &v, int o, int k, float x, bool main);
    float wave(const Voice &v, int o, float x) const;
    float filter1(Voice &v, F1 &f, float x, float g, float k, float gateAmp);
};
} // namespace hy
