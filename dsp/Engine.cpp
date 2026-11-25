/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
#include "Engine.h"
#include "Phrases.h"
#include <algorithm>

namespace hy
{
/* the filters' cutoff: 0..128 over ten octaves from 20 Hz */
static inline float cutHz(float c) { return 20.0f * fexp2(clampf(c, -24.0f, 140.0f) * (10.0f / 128.0f)); }
/* a segment's curve: 0 linear, + fast start, - slow start */
static inline float curveShape(float t, float c)
{
    float k = c * (5.0f / 64.0f);
    if (t <= 0.0f)
        return 0.0f;
    if (t >= 1.0f)
        return 1.0f;
    if (std::fabs(k) < 0.05f)
        return t;
    return (1.0f - std::exp(-k * t)) / (1.0f - std::exp(-k));
}
static inline float frac(float x) { return x - std::floor(x); }
static const float STACK_C[5] = {-1.0f, -0.45f, 0.0f, 0.55f, 1.0f};
static const float FMI = 2.0f;                       /* FM-Lin at Depth 128: two cycles of deviation */

Engine::Engine()
{
    patchInit(pat);
    for (int i = 0; i < MAXV; i++) {
        V[i].idx = i;
        V[i].seed = 0x1234567u + (uint32_t)i * 7919u;
        std::memcpy(V[i].val, pat.v, sizeof pat.v);
    }
    std::memcpy(ghost.val, pat.v, sizeof pat.v);
    gval = ghost.val;
    for (int l = 0; l < 5; l++)
        glfo[l].seed = 0xABCDEFu + (uint32_t)l * 31u;
}

void Engine::prepare(double s)
{
    sr = (float)s;
    WaveBank::get();
    pre.prepare(sr);
    post.prepare(sr);
    dly.prepare(sr);
    rev.prepare(sr);
    for (int c = 0; c < 2; c++) {
        warmLs[c].set(4, sr, 140.0f, 0.7f, 2.5f);
        warmLp[c].set(0, sr, 9000.0f, 0.6f, 0.0f);
        warmLs[c].clear();
        warmLp[c].clear();
    }
    for (auto &v : V) {
        v.active = v.gate = false;
        v.amp = v.ampT = 0.0f;
    }
    gpos = CH;
    nKeys = arpN = 0;
    arpRunning = false;
}

void Engine::setPatch(const Patch &p)
{
    pat = p;
    for (auto &v : V) {
        std::memcpy(v.val, pat.v, sizeof pat.v);
        v.nDirty = 0;
    }
    std::memcpy(ghost.val, pat.v, sizeof pat.v);
    ghost.nDirty = 0;
    pre.clear();
    post.clear();
}
void Engine::setParam(int p, float x)
{
    if (p < 0 || p >= NPARAM)
        return;
    pat.v[p] = x;
    for (auto &v : V)
        v.val[p] = x;
    ghost.val[p] = x;
}
void Engine::setStep(int l, int s, float x)
{
    if (l >= 0 && l < 5 && s >= 0 && s < NSTEP)
        pat.step[l][s] = clampf(x, -64.0f, 64.0f);
}
void Engine::setVoices(int n)
{
    n = n > 8 ? 16 : 8;
    if (n == nVoices)
        return;
    for (int i = n; i < MAXV; i++)
        V[i].active = V[i].gate = false;
    if (splitOn && V[nVoices - 1].active) {
        V[nVoices - 1].active = V[nVoices - 1].gate = false;
        bassN = 0;
    }
    nVoices = n;
    rotNext = 0;
}

void Engine::setSplit(bool on, int note)
{
    splitNote = note < 1 ? 1 : note > 127 ? 127 : note;
    if (on == splitOn)
        return;
    /* the voice that changes hands lets go first */
    Voice &b = V[bassIdx()];
    if (b.active && b.gate)
        releaseVoice(b);
    bassN = 0;
    splitOn = on;
    rotNext = 0;
}
void Engine::bassDown(int note, float vel)
{
    for (int i = 0; i < bassN; i++)
        if (bassKeys[i] == note) {
            for (int j = i; j < bassN - 1; j++) { bassKeys[j] = bassKeys[j + 1]; bassVel[j] = bassVel[j + 1]; }
            bassN--;
            break;
        }
    if (bassN < NKEYS) { bassKeys[bassN] = (int8_t)note; bassVel[bassN] = vel; bassN++; }
    Voice &v = V[bassIdx()];
    bool legato = v.active && v.gate;
    v.panPos = 0.0f;
    startVoice(v, note, note, vel, 0.0f, v.active, legato);
}
void Engine::bassUp(int note)
{
    for (int i = 0; i < bassN; i++)
        if (bassKeys[i] == note) {
            for (int j = i; j < bassN - 1; j++) { bassKeys[j] = bassKeys[j + 1]; bassVel[j] = bassVel[j + 1]; }
            bassN--;
            break;
        }
    Voice &v = V[bassIdx()];
    if (!(v.active && v.gate))
        return;
    if (bassN > 0) {                                /* back to the key still held, legato */
        int n = bassKeys[bassN - 1];
        if (v.note != n)
            startVoice(v, n, n, bassVel[bassN - 1], 0.0f, true, true);
        return;
    }
    if (susOn && (int)pat.v[P_VO_SUSPED] == SP_SUSTAIN)
        v.sus = true;
    else
        releaseVoice(v);
}

/* ===================================================================== keys === */
int Engine::quantizeNote(int n) const
{
    int sc = (int)pat.v[P_VO_SCALE];
    if (sc <= 0 || sc >= NSCALE)
        return n;
    int key = (int)pat.v[P_VO_KEY];
    uint16_t m = SCALE_MASK[sc];
    for (int d = 0; d < 12; d++) {                   /* the nearest scale note, down first */
        int a = n - d, b = n + d;
        if (a >= 0 && (m >> (((a - key) % 12 + 12) % 12) & 1))
            return a;
        if (b < 128 && (m >> (((b - key) % 12 + 12) % 12) & 1))
            return b;
    }
    return n;
}

static int8_t qmap[128];
void Engine::noteOn(int note, float vel)
{
    if (note < 0 || note > 127)
        return;
    if (vel <= 0.0f) {
        noteOff(note);
        return;
    }
    int q = quantizeNote(note);
    qmap[note] = (int8_t)q;
    if (splitOn && note < splitNote) {
        splitHeld[note] = true;
        bassDown(q, vel);
        return;
    }
    if (pat.v[P_AR_ON] > 0.5f) {
        bool latch = pat.v[P_AR_LATCH] > 0.5f;
        /* latched: a new chord (keys pressed after all were let go) replaces the old */
        bool anyPhysical = false;
        for (int i = 0; i < 128; i++)
            if (sostHeld[i]) { anyPhysical = true; break; }
        if (latch && !anyPhysical)
            arpN = 0;
        sostHeld[note] = true;
        for (int i = 0; i < arpN; i++)
            if (arpHeld[i] == q)
                return;
        if (arpN < NKEYS) {
            arpHeld[arpN] = (int8_t)q;
            arpVel[arpN] = vel;
            arpN++;
        }
        arpRebuild();
        return;
    }
    sostHeld[note] = true;
    keyDown(q, vel);
}
void Engine::noteOff(int note, float vel)
{
    if (note < 0 || note > 127)
        return;
    sostHeld[note] = false;
    int q = qmap[note];
    if (splitHeld[note]) {
        splitHeld[note] = false;
        bassUp(q);
        return;
    }
    if (pat.v[P_AR_ON] > 0.5f || arpN > 0) {
        bool latch = pat.v[P_AR_LATCH] > 0.5f;
        if (!latch) {
            for (int i = 0; i < arpN; i++)
                if (arpHeld[i] == q) {
                    for (int j = i; j < arpN - 1; j++) {
                        arpHeld[j] = arpHeld[j + 1];
                        arpVel[j] = arpVel[j + 1];
                    }
                    arpN--;
                    break;
                }
            arpRebuild();
        }
        if (pat.v[P_AR_ON] > 0.5f)
            return;
    }
    for (auto &v : V)
        if (v.active && v.gate && v.note == q)
            v.offVel = vel;
    keyUp(q);
}

void Engine::keyDown(int note, float vel)
{
    /* the stack of sounding keys (mono, unison, the legato tests) */
    for (int i = 0; i < nKeys; i++)
        if (keys[i] == note) {
            for (int j = i; j < nKeys - 1; j++) {
                keys[j] = keys[j + 1];
                keyVel[j] = keyVel[j + 1];
            }
            nKeys--;
            break;
        }
    if (nKeys < NKEYS) {
        keys[nKeys] = (int8_t)note;
        keyVel[nKeys] = vel;
        nKeys++;
    }
    /* TrigSync Single: every new note restarts the shared LFO */
    for (int l = 0; l < 5; l++)
        if ((int)pat.v[pLfo(l, LF_TRIG)] == TS_SINGLE)
            lfoTrig(glfo[l], pat.v, l);
    int poly = (int)pat.v[P_VO_POLY];
    if (poly == PO_POLY)
        playNote(note, note, vel);
    else
        monoUpdate();
    lastNote = note;
}
void Engine::keyUp(int note)
{
    for (int i = 0; i < nKeys; i++)
        if (keys[i] == note) {
            for (int j = i; j < nKeys - 1; j++) {
                keys[j] = keys[j + 1];
                keyVel[j] = keyVel[j + 1];
            }
            nKeys--;
            break;
        }
    if ((int)pat.v[P_VO_POLY] == PO_POLY)
        stopNote(note);
    else
        monoUpdate();
}

int Engine::findVoice()
{
    int mode = (int)pat.v[P_VO_STMODE];
    auto freeV = [&](int i) { return !V[i].active || !V[i].gate; };
    const int nP = polyN();
    int pick = -1;
    if (mode == SM_ROTATE) {
        for (int k = 0; k < nP; k++) {
            int i = (rotNext + k) % nP;
            if (freeV(i)) {
                pick = i;
                break;
            }
        }
    } else if (mode == SM_ALTER) {
        for (int i = 0; i < nP; i++)
            if (freeV(i)) {
                pick = i;
                break;
            }
    } else {
        int n = 0, cand[MAXV];
        for (int i = 0; i < nP; i++)
            if (freeV(i))
                cand[n++] = i;
        if (n)
            pick = cand[rndU(rseed) % (uint32_t)n];
    }
    if (pick < 0) {                                 /* all held: the oldest */
        uint32_t best = 0xffffffffu;
        for (int i = 0; i < nP; i++)
            if (V[i].age < best) {
                best = V[i].age;
                pick = i;
            }
    }
    rotNext = (pick + 1) % nP;
    return pick;
}

void Engine::playNote(int key, int note, float vel)
{
    int i = findVoice();
    Voice &v = V[i];
    /* its place in the stereo field: Rotate and Random left to right, Alter from the centre out */
    int n = polyN();
    if ((int)pat.v[P_VO_STMODE] == SM_ALTER)
        v.panPos = (i & 1 ? 1.0f : -1.0f) * ((float)(i / 2) + 0.5f) / ((float)n * 0.5f);
    else
        v.panPos = -1.0f + (2.0f * (float)i + 1.0f) / (float)n;
    bool held = v.active && v.gate;
    /* VOICE SPREAD: up to +-7 cents and a touch of cutoff, different for every note */
    float sDet = spread > 0.0f ? rndF(rseed) * spread * 0.07f : 0.0f;
    startVoice(v, key, note, vel, sDet, v.active, held);
    if (spread > 0.0f)
        v.cutOff += rndF(rseed) * spread * 3.0f;
}
void Engine::stopNote(int key)
{
    int pedal = (int)pat.v[P_VO_SUSPED];
    for (auto &v : V)
        if (v.active && v.gate && v.key == key) {
            if (v.sost)
                v.sus = true;
            else if (susOn && pedal == SP_SUSTAIN)
                v.sus = true;
            else
                releaseVoice(v);
        }
}

void Engine::monoUpdate()
{
    int poly = (int)pat.v[P_VO_POLY];
    bool uni = poly >= PO_UNI;
    int pri = uni ? poly - PO_UNI : poly - PO_MONO;       /* 0 last, 1 low, 2 high */
    int n = uni ? std::min((int)pat.v[P_VO_DENSITY], polyN()) : 1;
    n = std::max(n, 1);
    if (nKeys == 0) {
        int pedal = (int)pat.v[P_VO_SUSPED];
        for (int i = 0; i < MAXV; i++)
            if (V[i].active && V[i].gate && !(splitOn && i == bassIdx())) {
                if (susOn && pedal == SP_SUSTAIN)
                    V[i].sus = true;
                else
                    releaseVoice(V[i]);
            }
        return;
    }
    int note = keys[nKeys - 1];
    float vel = keyVel[nKeys - 1];
    for (int i = 0; i < nKeys; i++) {
        if (pri == 1 && keys[i] < note) { note = keys[i]; vel = keyVel[i]; }
        if (pri == 2 && keys[i] > note) { note = keys[i]; vel = keyVel[i]; }
    }
    if (pri == 1)
        for (int i = 0; i < nKeys; i++) if (keys[i] < note) note = keys[i];
    bool legato = V[0].active && V[0].gate;
    if (legato && V[0].note == note && V[0].key == note)
        return;
    float detS = std::pow((float)pat.v[P_VO_DETUNE] / 127.0f, 1.5f) * 0.6f;
    for (int j = 0; j < n; j++) {
        Voice &v = V[j];
        float pos = n > 1 ? -1.0f + 2.0f * (float)j / (float)(n - 1) : 0.0f;
        v.panPos = uni ? pos : 0.0f;
        float det = uni && n > 1 ? pos * detS : 0.0f;
        startVoice(v, note, note, vel, det, v.active, legato);
    }
    for (int j = n; j < MAXV; j++)
        if (V[j].active && V[j].gate && !(splitOn && j == bassIdx()))
            releaseVoice(V[j]);
}

void Engine::startVoice(Voice &v, int key, int note, float vel, float det, bool retrig, bool legato)
{
    const float *P = pat.v;
    bool was = v.active;
    float af = P[P_VO_AFEEL] / 127.0f;
    v.key = key;
    v.note = note;
    v.vel = vel;
    v.gate = true;
    v.sus = false;
    v.sost = false;
    v.age = ++ageN;
    v.uniDet = det;
    v.press = 0.0f;
    lastVoice = (int)(&v - V);
    if (!was) {
        v.active = true;
        bool rp = P[P_VO_RPHASE] > 0.5f;
        for (int o = 0; o < 3; o++)
            v.ph[o] = rp ? rnd01(v.seed) : 0.0f;
        for (auto &m : v.mu) {
            m.modPh = rp ? rnd01(v.seed) : 0.0f;
            m.y1 = m.y2 = m.harmY = m.harmN = 0.0f;
            m.hs.clear();
            for (int j = 0; j < 5; j++)
                m.stack[j] = j == 2 ? 0.0f : rnd01(v.seed);
            m.r = 1.0f; m.d = 0.0f; m.m = 0.0f;
        }
        for (int c = 0; c < 2; c++) {
            v.f1[c].clear();
            v.f2[c].clear();
            v.hb[c].clear();
        }
        v.monoMix = true;
        std::memset(v.pink, 0, sizeof v.pink);
        v.brown = v.red = v.prevW = v.prevP = 0.0f;
        std::memset(v.mo, 0, sizeof v.mo);
        std::memset(v.oo, 0, sizeof v.oo);
        v.ring = v.noise = 0.0f;
        for (auto &e : v.env)
            e = Env();
        v.amp = v.ampT = 0.0f;
        v.fadeIn = 0.0f;
        std::memset(v.mg, 0, sizeof v.mg);
        v.inc[0] = v.inc[1] = v.inc[2] = 0.0f;
        for (int o = 0; o < 3; o++) {
            v.driftOff[o] = rndF(v.seed) * af * 0.10f;
            v.drift[o] = v.driftT[o] = 0.0f;
        }
    }
    v.cutOff = rndF(v.seed) * af * 4.0f;
    v.envScale = 1.0f + rndF(v.seed) * af * 0.2f;
    v.fadeK = 1.0f / ((P[P_VO_SNAP] > 0.5f ? 0.00005f : 0.0015f) * sr);
    if (!was)
        v.fadeIn = 0.0f;
    /* glide */
    int gl = (int)P[P_VO_GLIDE];
    bool mono = (int)P[P_VO_POLY] != PO_POLY;
    float from = mono ? lastPitch : v.pitch;
    if (gl != GL_OFF && (was || mono) && (P[P_VO_GLEGATO] < 0.5f || legato)) {
        v.gFrom = was ? v.pitch : from;
        v.gTo = (float)note;
        v.gU = 0.0f;
        float gt = P[P_VO_GTIME];
        v.gDur = gt <= 0.0f ? 0.0f : 0.002f * std::pow(1.0645f, gt);
        if (v.gDur <= 0.0f)
            v.gU = 1.0f;
    } else {
        v.gU = 1.0f;
        v.gTo = (float)note;
    }
    if (v.gU >= 1.0f)
        v.pitch = (float)note;
    lastPitch = (float)note;
    /* envelopes: those triggered by notes */
    for (int e = 0; e < 5; e++) {
        bool byNote = e == 1;
        for (int t = 0; t < 4; t++)
            if ((int)P[pEnv(e, EN_TRIG1 + t)] == TR_NOTE)
                byNote = true;
        if (!byNote)
            continue;
        if (legato && P[pEnv(e, EN_LEGATO)] > 0.5f && v.env[e].st != ES_IDLE && v.env[e].st != ES_REL)
            continue;
        envGate(v, e, retrig);
    }
    /* LFOs per voice */
    for (int l = 0; l < 5; l++)
        if ((int)P[pLfo(l, LF_TRIG)] == TS_POLY && !(legato && mono))
            lfoTrig(v.lfo[l], P, l);
}

void Engine::releaseVoice(Voice &v)
{
    v.gate = false;
    v.sus = false;
    for (int e = 0; e < 5; e++)
        envRelease(v, e);
}

/* ============================================================== envelopes === */
float Engine::envTime(const float *P, int e, int k) const
{
    int p = pEnv(e, k);
    if (P[pEnv(e, EN_BPM)] > 0.5f) {
        int i = (int)std::lround(toNorm(p, P[p]) * NSYNC);
        return i <= 0 ? 0.0f : SYNC_BEATS[i - 1] * 60.0f / bpm;
    }
    return P[p];
}
void Engine::envGate(Voice &v, int e, bool retrig)
{
    Env &en = v.env[e];
    const float *P = v.val;
    bool reset = P[pEnv(e, EN_RESET)] > 0.5f;
    en.from = (retrig && !reset) ? en.v : 0.0f;
    if (!retrig)
        en.v = 0.0f;
    en.st = ES_DELAY;
    en.t = 0.0f;
    en.armed = false;
    int lp = (int)P[pEnv(e, EN_LOOP)];
    en.loops = lp <= 0 ? 0 : lp >= 50 ? -1 : lp;
}
void Engine::envRelease(Voice &v, int e)
{
    Env &en = v.env[e];
    if (en.st == ES_IDLE || en.st == ES_REL)
        return;
    if (v.val[pEnv(e, EN_FREE)] > 0.5f && en.st < ES_SUS) {
        en.armed = true;                             /* freerun: on to the sustain, then the release */
        return;
    }
    en.st = ES_REL;
    en.from = en.v;
    en.t = 0.0f;
}
void Engine::envStep(Voice &v, int e, float dt)
{
    Env &en = v.env[e];
    const float *P = v.val;
    float sus = clampf(P[pEnv(e, EN_SUS)] / 128.0f, 0.0f, 1.0f);
    auto T = [&](int k) { return envTime(P, e, k) * v.envScale; };
    for (int guard = 0; guard < 6; guard++) {
        switch (en.st) {
        case ES_IDLE: en.v = 0.0f; return;
        case ES_DELAY: {
            float t = T(EN_DLY);
            if (t <= 0.0f || (en.t += dt / t) >= 1.0f) {
                en.st = ES_ATT;
                en.t = 0.0f;
                continue;
            }
            return;
        }
        case ES_ATT: {
            float t = T(EN_ATK);
            en.t = t <= 0.0003f ? 1.0f : en.t + dt / t;
            if (en.t >= 1.0f) {
                en.v = 1.0f;
                en.st = ES_HOLD;
                en.t = 0.0f;
                continue;
            }
            en.v = en.from + (1.0f - en.from) * curveShape(en.t, P[pEnv(e, EN_ACRV)]);
            return;
        }
        case ES_HOLD: {
            float t = T(EN_HLD);
            if (t <= 0.0f || (en.t += dt / t) >= 1.0f) {
                en.st = ES_DEC;
                en.t = 0.0f;
                en.from = 1.0f;
                continue;
            }
            return;
        }
        case ES_DEC: {
            float t = T(EN_DEC);
            en.t = t <= 0.0003f ? 1.0f : en.t + dt / t;
            if (en.t >= 1.0f) {
                en.v = sus;
                if (en.loops != 0) {
                    if (en.loops > 0)
                        en.loops--;
                    en.st = ES_ATT;
                    en.from = en.v;
                    en.t = 0.0f;
                    return;
                }
                en.st = ES_SUS;
                continue;
            }
            en.v = 1.0f + (sus - 1.0f) * curveShape(en.t, P[pEnv(e, EN_DCRV)]);
            return;
        }
        case ES_SUS:
            en.v = sus;
            if (en.armed || !v.gate) {
                if (!v.gate && !v.sus) {
                    en.st = ES_REL;
                    en.from = en.v;
                    en.t = 0.0f;
                    en.armed = false;
                    continue;
                }
            }
            return;
        case ES_REL: {
            float t = T(EN_REL);
            en.t = t <= 0.0003f ? 1.0f : en.t + dt / t;
            if (en.t >= 1.0f) {
                en.v = 0.0f;
                en.st = ES_IDLE;
                return;
            }
            en.v = en.from * (1.0f - curveShape(en.t, P[pEnv(e, EN_RCRV)]));
            return;
        }
        default: return;
        }
    }
}

/* =================================================================== LFOs === */
float Engine::lfoRate(const float *P, int k) const
{
    if (P[pLfo(k, LF_BPM)] > 0.5f) {
        int i = (int)P[pLfo(k, LF_SYNC)];
        i = i < 0 ? 0 : i >= NSYNC ? NSYNC - 1 : i;
        return bpm / 60.0f / SYNC_BEATS[i];
    }
    return P[pLfo(k, LF_RATE)];
}
void Engine::lfoTrig(Lfo &l, const float *P, int k)
{
    int os1 = (int)P[pLfo(k, LF_ONESHOT)];
    int wv = (int)P[pLfo(k, LF_WAVE)];
    if (os1 == OS_STEP && wv == LW_STEP) {
        if (l.started) {
            int n = std::max(2, (int)P[pLfo(k, LF_STEPS)]);
            l.step = (l.step + 1) % n;
        } else
            l.step = 0;
        l.started = true;
        l.dly = 0.0f;
        return;
    }
    l.ph = P[pLfo(k, LF_PHASE)] / 360.0f;
    l.dly = 0.0f;
    l.done = false;
    l.started = true;
    l.step = 0;
    l.sh = rndF(l.seed);
    l.r0 = l.r1;
    l.r1 = rndF(l.seed);
}
void Engine::lfoStep(Lfo &l, const float *P, int k, float dt, Voice *)
{
    int wv = (int)P[pLfo(k, LF_WAVE)];
    int os1 = (int)P[pLfo(k, LF_ONESHOT)];
    float rate = lfoRate(P, k);
    l.wrap = false;
    /* delay, fade in */
    float dT, fT;
    if (P[pLfo(k, LF_BPM)] > 0.5f) {
        int a = (int)std::lround(toNorm(pLfo(k, LF_DLY), P[pLfo(k, LF_DLY)]) * NSYNC);
        int b = (int)std::lround(toNorm(pLfo(k, LF_FADE), P[pLfo(k, LF_FADE)]) * NSYNC);
        dT = a <= 0 ? 0.0f : SYNC_BEATS[a - 1] * 60.0f / bpm;
        fT = b <= 0 ? 0.0f : SYNC_BEATS[b - 1] * 60.0f / bpm;
    } else {
        dT = P[pLfo(k, LF_DLY)];
        fT = P[pLfo(k, LF_FADE)];
    }
    l.dly += dt;
    if (l.dly < dT) {
        l.out = l.raw = 0.0f;
        return;
    }
    float amp = fT > 0.0f ? minf(1.0f, (l.dly - dT) / fT) : 1.0f;
    bool stepMode = os1 == OS_STEP && wv == LW_STEP;
    if (!stepMode && !(os1 == OS_ON && l.done)) {
        l.ph += rate * dt;
        if (l.ph >= 1.0f) {
            l.ph -= std::floor(l.ph);
            l.wrap = true;
            l.sh = rndF(l.seed);
            l.r0 = l.r1;
            l.r1 = rndF(l.seed);
            if (os1 == OS_ON) {
                l.done = true;
                l.ph = 0.9999f;
            }
        }
    }
    float p = l.ph, y;
    switch (wv) {
    case LW_SINE: y = fsin(p); break;
    case LW_TRI: y = p < 0.25f ? 4.0f * p : p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f; break;
    case LW_SAWUP: y = 2.0f * p - 1.0f; break;
    case LW_SAWDN: y = 1.0f - 2.0f * p; break;
    case LW_SQUARE: y = p < 0.5f ? 1.0f : -1.0f; break;
    case LW_PULSE27: y = p < 0.27f ? 1.0f : -1.0f; break;
    case LW_PULSE13: y = p < 0.13f ? 1.0f : -1.0f; break;
    case LW_SH: y = l.sh; break;
    case LW_NOISE: y = rndF(l.seed); break;
    case LW_RANDOM: {
        float s = 0.5f - 0.5f * std::cos(PI * p);
        y = l.r0 + (l.r1 - l.r0) * s;
        break;
    }
    default: {
        int n = std::max(2, std::min(NSTEP, (int)P[pLfo(k, LF_STEPS)]));
        int i = stepMode ? l.step % n : std::min(n - 1, (int)(p * (float)n));
        y = pat.step[k][i] / 64.0f;
        break;
    }
    }
    float smooth = P[pLfo(k, LF_SMOOTH)];
    if (smooth > 0.0f && wv != LW_TRI && !stepMode) {
        float tau = (smooth / 127.0f) * (smooth / 127.0f) * 0.5f / maxf(rate, 0.02f);
        l.sm += (y - l.sm) * (1.0f - std::exp(-dt / maxf(tau, 1e-5f)));
        y = l.sm;
    } else
        l.sm = y;
    int q = (int)P[pLfo(k, LF_QUANT)];
    if (q >= 2)
        y = std::round(y * (float)q) / (float)q;
    l.raw = y * amp;
    l.out = y * amp * P[pLfo(k, LF_LEVEL)] / 128.0f;
}

/* ============================================================= modulation === */
void Engine::sources(Voice &v, bool isGhost)
{
    float *s = v.src;
    const float *P = v.val;
    for (int e = 0; e < 5; e++) {
        float x = isGhost ? 0.0f : v.env[e].v;
        int q = (int)P[pEnv(e, EN_QUANT)];
        if (q >= 2)
            x = std::round(x * (float)q) / (float)q;
        s[SRC_ENV1 + e] = x;
    }
    for (int l = 0; l < 5; l++) {
        bool poly = (int)P[pLfo(l, LF_TRIG)] == TS_POLY;
        const Lfo &lf = poly ? v.lfo[l] : glfo[l];
        float lvl = P[pLfo(l, LF_LEVEL)] / 128.0f;
        bool on = !(poly && isGhost);
        s[SRC_LFO1 + l] = on ? lf.out : 0.0f;
        s[SRC_LFO1U + l] = on ? (lf.raw + 1.0f) * 0.5f * lvl : 0.0f;
    }
    s[SRC_MONOAT] = atV;
    s[SRC_POLYAT] = isGhost ? 0.0f : v.press;
    s[SRC_KEY] = isGhost ? 0.0f : ((float)v.note - 60.0f) / 60.0f;
    s[SRC_VELON] = isGhost ? 0.0f : v.vel;
    s[SRC_VELOFF] = isGhost ? 0.0f : v.offVel;
    s[SRC_PITCHWHL] = bendV;
    s[SRC_MODWHL] = mwV;
    s[SRC_EXP] = expV;
    s[SRC_SUSPED] = susV;
    s[SRC_BREATH] = brV;
    float vm = P[P_VO_VMOD1 + (v.idx % NVMOD)] / 128.0f;
    s[SRC_VMOD] = isGhost ? 0.0f : vm;
    s[SRC_VMODU] = isGhost ? 0.0f : (vm + 1.0f) * 0.5f;
}

void Engine::modulate(Voice &v)
{
    float *val = v.val;
    for (int i = 0; i < v.nDirty; i++)
        val[v.dirty[i]] = pat.v[v.dirty[i]];
    v.nDirty = 0;
    v.pmod[0] = v.pmod[1] = v.pmod[2] = v.pmod[3] = 0.0f;
    const auto &D = dests();
    const int ND = (int)D.size();
    float d[256];
    int touched[256], nT = 0;
    auto addD = [&](int dst, float x) {
        if (dst <= 0 || dst >= ND || x == 0.0f)
            return;
        bool seen = false;
        for (int i = 0; i < nT; i++)
            if (touched[i] == dst) {
                seen = true;
                break;
            }
        if (!seen) {
            touched[nT++] = dst;
            d[dst] = 0.0f;
        }
        d[dst] += x;
    };
    auto apply = [&](int first) {
        for (int i = first; i < nT; i++) {
            int dst = touched[i];
            const DstDef &dd = D[(size_t)dst];
            if (dd.param < 0) {
                float semis = d[dst] * 12.8f;
                if (dd.special == DS_PITCHALL)
                    v.pmod[3] += semis;
                else if (dd.special >= DS_PITCH1 && dd.special <= DS_PITCH3)
                    v.pmod[dd.special - DS_PITCH1] += semis;
                continue;
            }
            int p = dd.param;
            float n = toNorm(p, val[p]) + d[dst];
            val[p] = fromNorm(p, n);
            bool known = false;
            for (int k = 0; k < v.nDirty; k++)
                if (v.dirty[k] == p) {
                    known = true;
                    break;
                }
            if (!known && v.nDirty < 400)
                v.dirty[v.nDirty++] = (uint16_t)p;
        }
    };
    auto srcVal = [&](int s) -> float {
        if (s <= 0 || s >= NSRC)
            return 0.0f;
        return s >= SRC_CC0 ? ccV[s - SRC_CC0] : v.src[s];
    };
    /* first the routes that move other routes' depths and the macro knobs */
    int firstMod = D[(size_t)dstOfParam(pMod(0, MM_DEPTH))].param >= 0 ? dstOfParam(pMod(0, MM_DEPTH)) : 9999;
    int firstMacro = dstOfParam(P_MC_VAL1);
    auto isMeta = [&](int dst) { return (dst >= firstMod && dst < firstMod + NMOD) || (dst >= firstMacro && dst < firstMacro + NMACRO); };
    for (int s = 0; s < NMOD; s++) {
        int sv = (int)val[pMod(s, MM_SRC)], dv = (int)val[pMod(s, MM_DST)];
        if (sv > 0 && dv > 0 && isMeta(dv))
            addD(dv, srcVal(sv) * val[pMod(s, MM_DEPTH)] / 128.0f);
    }
    apply(0);
    int mark = nT;
    /* the macros */
    for (int m = 0; m < NMACRO; m++) {
        float knob = val[P_MC_VAL1 + m] / 128.0f;
        bool btn = pat.v[P_MC_BTN1 + m] > 0.5f;
        for (int j = 0; j < NMACRODST; j++) {
            int dv = (int)val[pMacroDst(m, j, MR_DST)];
            if (dv <= 0)
                continue;
            float x = btn ? val[pMacroDst(m, j, MR_BTN)] / 128.0f : val[pMacroDst(m, j, MR_DEPTH)] / 128.0f * knob;
            addD(dv, x);
        }
    }
    /* then every other route */
    for (int s = 0; s < NMOD; s++) {
        int sv = (int)val[pMod(s, MM_SRC)], dv = (int)val[pMod(s, MM_DST)];
        if (sv > 0 && dv > 0 && !isMeta(dv))
            addD(dv, srcVal(sv) * val[pMod(s, MM_DEPTH)] / 128.0f);
    }
    /* (the first pass's destinations are done: start after them) */
    apply(mark);
}

/* ============================================================== the voice === */
float Engine::wave(const Voice &v, int o, float x) const
{
    float a = v.scanA[o];
    float y = 0.0f;
    if (v.tA[o] && a < 0.9999f)
        y = readTab(v.tA[o], x) * (1.0f - a);
    if (v.tB[o] && a > 0.0001f)
        y += readTab(v.tB[o], x) * a;
    if (v.bitsQ[o] > 0.0f)
        y = std::round(y * v.bitsQ[o]) / v.bitsQ[o];
    return y;
}

float Engine::evalChain(Voice &v, int o, int k, float x, bool main)
{
    if (k == 0)
        return wave(v, o, x);
    Mut &M = v.mu[o * 2 + k - 1];
    if (!M.on)
        return evalChain(v, o, k - 1, x, false);
    float m = M.m;
    float dry = m < 0.999f ? evalChain(v, o, k - 1, x, false) : 0.0f;
    float wet = 0.0f;
    switch (M.mode) {
    case MU_FM:
        wet = evalChain(v, o, k - 1, x + minf(M.d, M.dMax) * FMI * M.sVal + M.fbT, false);
        break;
    case MU_STACK: {
        float s = 0.0f;
        for (int j = 0; j < 5; j++)
            s += evalChain(v, o, k - 1, x + M.stack[j], false);
        wet = s * 0.42f;
        break;
    }
    case MU_SYNC: {
        float xm = M.syncSrc == o ? x : v.ph[M.syncSrc] + (x - v.ph[o]);
        float u = frac(xm * M.r);
        float sp = u * minf(1.0f + 15.0f * M.d, M.sMax) + M.fbT;
        float win = 1.0f - M.w + M.w * (0.5f - 0.5f * fsin(u + 0.25f)) * 1.5f;
        /* the reset's edge rounded over a few samples (else it aliases at high notes): invisible low down */
        float del = minf(0.5f, 2.5f * v.inc[o] * M.r * minf(1.0f + 15.0f * M.d, M.sMax));
        float ta = clampf(u / del, 0.0f, 1.0f), tb = clampf((1.0f - u) / del, 0.0f, 1.0f);
        ta = ta * ta * (3.0f - 2.0f * ta);
        tb = tb * tb * (3.0f - 2.0f * tb);
        float base = evalChain(v, o, k - 1, sp, false);
        float t = minf(ta, tb);
        if (t < 1.0f) {
            float y0 = evalChain(v, o, k - 1, 0.0f, false);    /* (where the reset lands) */
            base = y0 + (base - y0) * t;
        }
        wet = base * win;
        break;
    }
    case MU_PWORIG:
    case MU_PWSQZ: {
        float t = x * M.r, fl = std::floor(t), u = t - fl, u2;
        if (M.mode == MU_PWORIG) {
            float p = clampf(0.5f - 0.48f * M.d + M.fbT, 0.02f, 0.98f);
            u2 = u < p ? 0.5f * u / p : 0.5f + 0.5f * (u - p) / (1.0f - p);
        } else {
            float g = maxf(0.2f, 1.0f + 6.0f * M.d + M.fbT * 4.0f);
            u2 = u <= 0.0f ? 0.0f : fexp2(g * std::log2(u));
        }
        wet = evalChain(v, o, k - 1, (fl + u2) / M.r, false);
        break;
    }
    case MU_PWASM: {
        float t = frac(x * M.r) * 8.0f;
        int i = (int)t;
        if (i > 7)
            i = 7;
        float mv = M.warp[i] + (M.warp[i + 1] - M.warp[i]) * (t - (float)i);
        wet = evalChain(v, o, k - 1, x + M.d * 0.5f * mv + M.fbT, false);
        break;
    }
    case MU_HARM: {
        if (main) {
            float in = m < 0.999f ? dry : evalChain(v, o, k - 1, x, false);
            float kq = M.hk;
            M.hs.step(in - M.fbT, M.hg, kq);
            M.harmN = M.hs.bp * kq;                          /* (peak 1: what the feedback takes) */
            M.harmY = softT(M.harmN * 2.2f * M.hMake * 0.5f) * 2.0f;
        }
        wet = M.harmY;
        break;
    }
    case MU_PHAZ: {
        float a = m < 0.999f ? dry : evalChain(v, o, k - 1, x, false);
        wet = 0.7f * (a - evalChain(v, o, k - 1, x + 0.5f * M.d + M.fbT, false));
        break;
    }
    default: wet = dry; break;
    }
    return dry * (1.0f - m) + wet * m;
}

void Engine::voiceSetup(Voice &v)
{
    const float *P = v.val;
    const float dtc = (float)CH / sr;
    const int N = CH * os;
    const float fs = sr * (float)os;
    /* pitch: glide, bend, vibrato, analog feel */
    if (v.gU < 1.0f) {
        v.gU = minf(1.0f, v.gU + dtc / maxf(v.gDur, 1e-4f));
        float s = curveShape(v.gU, -P[P_VO_GCURVE]);
        v.pitch = v.gFrom + (v.gTo - v.gFrom) * s;
        if ((int)P[P_VO_GLIDE] == GL_GLISS)
            v.pitch = std::round(v.pitch);
    }
    float af = P[P_VO_AFEEL] / 127.0f;
    if (af > 0.0f) {
        if (--v.driftN <= 0) {
            v.driftN = 300 + (int)(rnd01(v.seed) * 900.0f);
            for (int o = 0; o < 3; o++)
                v.driftT[o] = rndF(v.seed) * af * 0.25f;
        }
        for (int o = 0; o < 3; o++)
            v.drift[o] += (v.driftT[o] - v.drift[o]) * 0.004f;
    }
    float vib = mwV * P[P_VO_VIBAMT] * fsin(vibPh);
    float common = bendV * P[P_VO_PB] + vib + v.uniDet;
    float warpF[2] = {1.0f, 1.0f};
    /* the mutants */
    for (int mi = 0; mi < 4; mi++) {
        Mut &M = v.mu[mi];
        int b = pMut(mi, 0);
        M.mode = (int)P[b + MT_MODE];
        M.src = (int)P[b + MT_FMSRC];
        M.syncSrc = (int)P[b + MT_SYNCSRC];
        float rT = clampf(P[b + MT_RATIO], 0.25f, 64.0f), dT = clampf(P[b + MT_DEPTH] / 128.0f, 0.0f, 1.0f),
              mT = clampf(P[b + MT_WET] / 100.0f, 0.0f, 1.0f);
        M.w = clampf(P[b + MT_WINDOW] / 128.0f, 0.0f, 1.0f);
        M.f = clampf(P[b + MT_FB] / 100.0f, 0.0f, 1.5f);
        for (int w = 0; w < 8; w++)
            M.warp[w] = (P[b + MT_WARP1 + w] - 64.0f) / 64.0f;
        M.warp[8] = M.warp[0];
        M.rI = (rT - M.r) / (float)N;
        M.dI = (dT - M.d) / (float)N;
        M.mI = (mT - M.m) / (float)N;
        M.on = mT > 0.0005f || M.m > 0.0005f;
        /* a Mutant that changes nothing (FM or a PWM at Depth 0, no feedback) is skipped */
        bool ident = (M.mode == MU_FM || M.mode == MU_PWORIG || M.mode == MU_PWSQZ || M.mode == MU_PWASM) && dT <= 0.0f &&
                     M.d <= 1e-6f && M.f <= 0.0f;
        if (ident) {
            M.on = false;
            M.d = 0.0f;
            M.dI = 0.0f;
        }
        M.spread = std::pow(dT, 1.5f) * 0.05f;

        if (M.on) {
            float wf = 1.0f;
            switch (M.mode) {
            case MU_FM: wf = 1.0f + (M.src <= FS_TRI ? maxf(rT, 1.0f) : 3.0f) * (1.0f + 12.6f * dT) * 0.5f + M.f * 2.0f; break;
            case MU_SYNC: wf = rT * (1.0f + 15.0f * dT); break;
            case MU_PWORIG: case MU_PWSQZ: wf = rT * (1.0f + 4.0f * dT); break;
            case MU_PWASM: wf = 1.0f + dT * rT * 3.0f; break;
            default: break;
            }
            warpF[mi / 2] *= minf(wf, 64.0f);
        }
    }
    /* the oscillators */
    for (int o = 0; o < 3; o++) {
        int b = pOsc(o, 0);
        float kt = P[b + OS_KT] / 100.0f;
        float pitch = 60.0f + (v.pitch - 60.0f) * kt + P[b + OS_SEMI] + P[b + OS_CENT] * 0.01f + v.pmod[o] + v.pmod[3] + common +
                      v.drift[o] + v.driftOff[o];
        float inc = clampf(hzOf(pitch) / fs, 1e-7f, 0.45f);
        v.incT[o] = inc;
        if (v.inc[o] <= 0.0f)
            v.inc[o] = inc;
        int mip = mipFor(inc * (o < 2 ? warpF[o] : 1.0f));
        const WaveBank &W = WaveBank::get();
        if (o < 2 && (int)P[b + OS_MODE] == OM_SCAN) {
            int list[8], n = 0;
            for (int w = 0; w < 8; w++) {
                int wv = (int)P[b + OS_WAV1 + w];
                if (w == 0 && wv < 0)
                    wv = 0;
                if (wv >= -1)
                    list[n++] = wv;
            }
            float pos = clampf(P[b + OS_SCAN], 1.0f, (float)n) - 1.0f;
            int i = std::min((int)pos, n - 1), j = std::min(i + 1, n - 1);
            float a = pos - (float)i;
            v.tA[o] = list[i] >= 0 ? W.table(list[i], mip) : nullptr;
            v.tB[o] = list[j] >= 0 ? W.table(list[j], mip) : nullptr;
            v.scanA[o] = j == i ? 0.0f : a;
        } else {
            int wv = std::max(0, std::min(NWAVE - 1, (int)P[b + OS_WAVE]));
            v.tA[o] = W.table(wv, mip);
            v.tB[o] = nullptr;
            v.scanA[o] = 0.0f;
        }
        int bits = BITS_VAL[std::max(0, std::min(12, (int)P[b + OS_BITS]))];
        v.bitsQ[o] = bits ? (float)(1 << (bits - 1)) : 0.0f;
    }
    /* FM: the deviation kept where its sidebands still fit under the rate (high notes stay clean, as on the
     * instrument); the low and middle of the keyboard never reach it */
    for (int mi = 0; mi < 4; mi++) {
        Mut &M = v.mu[mi];
        float r = M.src <= FS_TRI ? clampf(P[pMut(mi, MT_RATIO)], 0.25f, 64.0f) : 2.0f;
        float room = 0.45f / maxf(v.incT[mi / 2] * r, 1e-6f);
        M.dMax = maxf(0.0f, (room - 1.0f - 1.0f / r) / 12.6f);
        /* (sync: the slave's speed kept under the rate the same way) */
        M.sMax = maxf(1.0f, 0.45f / maxf(v.incT[mi / 2] * clampf(P[pMut(mi, MT_RATIO)], 0.25f, 64.0f), 1e-6f));
    }
    /* Harmonic: a narrow band-pass at a harmonic of the oscillator; Depth sweeps it up the series */
    for (int mi = 0; mi < 4; mi++) {
        Mut &M = v.mu[mi];
        if (M.mode != MU_HARM)
            continue;
        float rT = clampf(P[pMut(mi, MT_RATIO)], 0.25f, 64.0f), dT = clampf(P[pMut(mi, MT_DEPTH)] / 128.0f, 0.0f, 1.0f);
        float f0 = v.incT[mi / 2] * fs, h = rT * dT * 32.0f;
        float fc = clampf(f0 * maxf(h, 0.02f), 5.0f, fs * 0.45f);
        M.hk = clampf(0.35f / maxf(h, 0.35f), 0.015f, 1.5f);
        M.hg = tanW(PI * fc / fs);
        M.hMake = clampf(std::sqrt(maxf(h, 1.0f)), 1.0f, 5.0f);   /* (a harmonic high up is weaker: brought up) */
    }
    /* the mixer */
    bool series = (int)P[P_MX_ROUTE] == 0;
    v.series = series;
    bool any1 = false, any2 = false;
    for (int s = 0; s < 5; s++) {
        float vol = clampf(P[P_MX_VOL1 + s] / 128.0f, 0.0f, 1.0f), g = vol * std::sqrt(vol) * 0.55f;
        float pan = P[P_MX_PAN1 + s], fr = clampf(P[P_MX_FR1 + s] / 128.0f, 0.0f, 1.0f);
        float lg = minf(1.0f, (64.0f - pan) / 64.0f), rg = minf(1.0f, (64.0f + pan) / 64.0f);
        v.mgT[s][0] = g * lg * (1.0f - fr);
        v.mgT[s][1] = g * rg * (1.0f - fr);
        v.mgT[s][2] = g * lg * fr;
        v.mgT[s][3] = g * rg * fr;
        if (g > 0.0f && fr < 1.0f) any1 = true;
        if (g > 0.0f && fr > 0.0f) any2 = true;
        for (int c = 0; c < 4; c++)
            if (v.mg[s][c] > 0.0f) {
                if (c < 2) any1 = true; else any2 = true;
            }
    }
    /* every source in the centre: the two channels are one (the filters run once) */
    bool mono = true;
    for (int s = 0; s < 5; s++)
        if (v.mgT[s][0] != v.mgT[s][1] || v.mgT[s][2] != v.mgT[s][3] || v.mg[s][0] != v.mg[s][1] || v.mg[s][2] != v.mg[s][3])
            mono = false;
    if (mono && !v.monoMix) {
        /* (from stereo to one: the second channel's state follows the first's) */
    } else if (!mono && v.monoMix) {
        v.f1[1] = v.f1[0];
        v.f2[1] = v.f2[0];
        v.hb[1] = v.hb[0];
    }
    v.monoMix = mono;
    v.useF1 = series || any1;
    v.useF2 = series || any2;
    /* the filters */
    float env1 = v.src[SRC_ENV1], lfo1 = v.src[SRC_LFO1];
    auto velScale = [&](float ve) { return ve >= 0.0f ? 1.0f - ve / 64.0f * (1.0f - v.vel) : 1.0f + ve / 64.0f * v.vel; };
    float keyC = ((float)v.note - 36.0f) * (128.0f / 120.0f);
    v.f1Type = (int)P[P_F1_TYPE];
    float c1 = P[P_F1_CUT] + keyC * P[P_F1_KT] / 100.0f + env1 * P[P_F1_ENV] / 64.0f * 128.0f * velScale(P[P_F1_VEL]) +
               lfo1 * P[P_F1_LFO] / 64.0f * 64.0f + v.cutOff;
    v.c1T = c1;
    /* resonance: comes in by the knob's middle, as on the instrument (not only in the last quarter) */
    auto resCurve = [](float r) { return 1.0f - std::pow(1.0f - r, 1.6f); };
    v.r1 = resCurve(clampf(P[P_F1_RES] / 128.0f, 0.0f, 1.0f));
    v.drive = clampf(P[P_F1_DRIVE] / 128.0f, 0.0f, 1.0f);
    v.drvPost = P[P_F1_DRVPOST] > 0.5f;
    v.gateAmp = std::pow(clampf((c1 - 6.0f) / 96.0f, 0.0f, 1.0f), 1.6f);
    if (v.f1Type == F1_VOWEL) {
        static const float VOW[5][3] = {{730, 1090, 2440}, {530, 1840, 2480}, {270, 2290, 3010}, {570, 840, 2410}, {300, 870, 2240}};
        static const char *const ORD[8] = {"AEIOU", "AIUEO", "EAIOU", "IOUAE", "OUAEI", "UOIEA", "EIOUA", "UIEAO"};
        const char *ord = ORD[std::max(0, std::min(7, (int)P[P_F1_VOWORD]))];
        auto vi = [](char ch) { return ch == 'A' ? 0 : ch == 'E' ? 1 : ch == 'I' ? 2 : ch == 'O' ? 3 : 4; };
        float pos = clampf(c1 / 128.0f, 0.0f, 1.0f) * 4.0f;
        int i = std::min(3, (int)pos);
        float a = pos - (float)i;
        float shift = fexp2((P[P_F1_CTRL] - 64.0f) / 64.0f);
        for (int f = 0; f < 3; f++) {
            float hz = (VOW[vi(ord[i])][f] * (1.0f - a) + VOW[vi(ord[i + 1])][f] * a) * shift;
            v.vow[f][0] = tanW(PI * clampf(hz, 50.0f, fs * 0.45f) / fs);
        }
        v.vowQ = 1.0f / (3.0f + v.r1 * 22.0f);
    }
    v.f2Type = (int)P[P_F2_TYPE];
    v.c2T = P[P_F2_CUT] + keyC * P[P_F2_KT] / 100.0f + env1 * P[P_F2_ENV] / 64.0f * 128.0f * velScale(P[P_F2_VEL]) +
            lfo1 * P[P_F2_LFO] / 64.0f * 64.0f + v.cutOff;
    v.r2 = resCurve(clampf(P[P_F2_RES] / 128.0f, 0.0f, 1.0f));
    v.morph = clampf(P[P_F2_MORPH] / 128.0f, 0.0f, 1.0f);
    /* the amp: Amp Level is the ceiling; LFO 2 and velocity only take away */
    float lvl = clampf(P[P_AMP_LEVEL] / 128.0f, 0.0f, 1.0f);
    float lfo2 = v.src[SRC_LFO1 + 1], la = P[P_AMP_LFO2] / 64.0f, va = P[P_AMP_VEL] / 64.0f;
    float lf = 1.0f - std::fabs(la) * 0.5f * (1.0f - (la >= 0.0f ? lfo2 : -lfo2));
    float vf = va >= 0.0f ? 1.0f - va * (1.0f - v.vel) : 1.0f + va * v.vel;
    float e2 = v.env[1].v;
    v.ampT = e2 * lvl * clampf(lf, 0.0f, 1.0f) * clampf(vf, 0.0f, 1.0f) * 0.8f;
}

float Engine::filter1(Voice &v, F1 &f, float x, float g, float res, float gateAmp)
{
    const int t = v.f1Type;
    float drv = v.drive;
    if (drv > 0.0f && !v.drvPost)
        x = softT(x * (1.0f + 9.0f * drv));
    float y;
    switch (t) {
    case F1_LDR12:
    case F1_LDR24:
    case F1_FAT12:
    case F1_FAT24:
    case F1_8POLE: {
        float G = g / (1.0f + g), G2 = G * G, G4 = G2 * G2, oG = 1.0f - G;
        float k = res * 4.15f;
        bool fat = t == F1_FAT12 || t == F1_FAT24;
        float xi = fat ? x * (1.0f + k) : x;
        float S = G * G2 * oG * f.s[0] + G2 * oG * f.s[1] + G * oG * f.s[2] + oG * f.s[3];
        float y4 = (G4 * xi + S) / (1.0f + k * G4);
        float u = softT(xi - k * y4);
        float st[4];
        float in = u;
        for (int i = 0; i < 4; i++) {
            float vv = (in - f.s[i]) * G;
            st[i] = vv + f.s[i];
            f.s[i] = st[i] + vv;
            in = st[i];
        }
        if (t == F1_8POLE) {
            for (int i = 4; i < 8; i++) {
                float vv = (in - f.s[i]) * G;
                float o = vv + f.s[i];
                f.s[i] = o + vv;
                in = o;
            }
            y = in * (1.0f + k * 0.3f);
        } else
            y = (t == F1_LDR12 || t == F1_FAT12) ? st[1] : st[3];
        if (!fat)
            y *= 1.0f + 0.2f * k;
        break;
    }
    case F1_3LER_LP:
    case F1_3LER_BP:
    case F1_3LER_HP: {
        /* (three poles peak where each turns 60 degrees: at sqrt(3) x their corner. The corner moved down so the
         * peak, and the self-oscillation, sit on the cutoff) */
        float g3 = g * 0.57735f;
        float G = g3 / (1.0f + g3), oG = 1.0f - G, G3 = G * G * G;
        float k = res * 8.6f;
        float S = G * G * oG * f.s[0] + G * oG * f.s[1] + oG * f.s[2];
        float y3 = (G3 * x + S) / (1.0f + k * G3);
        float u = softT(x - k * y3);
        float st[3], in = u;
        for (int i = 0; i < 3; i++) {
            float vv = (in - f.s[i]) * G;
            st[i] = vv + f.s[i];
            f.s[i] = st[i] + vv;
            in = st[i];
        }
        if (t == F1_3LER_LP)
            y = st[2] * (1.0f + 0.15f * k);
        else if (t == F1_3LER_BP)
            y = (st[0] - 2.0f * st[1] + st[2]) * 1.6f;
        else
            y = u - 3.0f * st[0] + 3.0f * st[1] - st[2];
        break;
    }
    case F1_GATE: {
        f.a.step(x, g, 2.0f - res * 1.5f);
        y = f.a.lo * gateAmp;
        break;
    }
    case F1_MS20LP:
    case F1_MS20HP: {
        float k = 2.0f - res * 2.08f;
        f.a.step(x, g, k);
        f.a.ic1 = 1.3f * softT(f.a.ic1 * (1.0f / 1.3f));
        y = t == F1_MS20LP ? f.a.lo : f.a.hp;
        break;
    }
    case F1_STN12LP:
    case F1_STN12HP:
    case F1_STN12BP: {
        float k = 2.0f - res * 2.03f;
        f.a.step(sat(x * 0.9f) * 1.1f, g, k);
        f.a.ic1 = 1.6f * sat(f.a.ic1 * (1.0f / 1.6f));
        f.a.ic2 = 1.8f * sat(f.a.ic2 * (1.0f / 1.8f));
        y = t == F1_STN12LP ? f.a.lo : t == F1_STN12HP ? f.a.hp : f.a.bp * maxf(k, 0.3f) * 1.4f;
        break;
    }
    case F1_1POLE:
        y = f.one.lp(x, g / (1.0f + g));
        break;
    default: {                                       /* Vowel */
        float s = 0.0f;
        static const float GA[3] = {1.0f, 0.7f, 0.45f};
        for (int i = 0; i < 3; i++) {
            f.form[i].step(x, v.vow[i][0], v.vowQ * 2.0f);
            s += f.form[i].bp * v.vowQ * 2.0f * GA[i];
        }
        y = s * 1.6f;
        break;
    }
    }
    if (drv > 0.0f && v.drvPost)
        y = softT(y * (1.0f + 9.0f * drv));
    return y;
}

void Engine::voiceRender(Voice &v, float *L, float *R)
{
    const int N = CH * os;
    const float fs = sr * (float)os, invN = 1.0f / (float)N;
    const float *P = v.val;
    float incI[3];
    for (int o = 0; o < 3; o++)
        incI[o] = (v.incT[o] - v.inc[o]) * invN;
    float mgI[5][4];
    for (int s = 0; s < 5; s++)
        for (int c = 0; c < 4; c++)
            mgI[s][c] = (v.mgT[s][c] - v.mg[s][c]) * invN;
    /* the filters' coefficients, at the chunk's two ends and glided between (no tan() a sample) */
    auto gOf = [&](float c) { return tanW(PI * minf(cutHz(c), fs * 0.42f) / fs); };
    float g1 = gOf(v.c1), g2 = gOf(v.c2);
    const float g1I = (gOf(v.c1T) - g1) * invN, g2I = (gOf(v.c2T) - g2) * invN;
    v.c1 = v.c1T;
    v.c2 = v.c2T;
    const int ntype = (int)P[P_RN_NOISE];
    const int rs1 = (int)P[P_RN_SRC1], rs2 = (int)P[P_RN_SRC2];
    const float rdep = clampf(P[P_RN_DEPTH] / 128.0f, 0.0f, 1.0f);
    bool needNoise = v.mgT[4][0] + v.mgT[4][1] + v.mgT[4][2] + v.mgT[4][3] > 0.0f || rs1 == RS_NOISE || rs2 == RS_NOISE;
    for (int mi = 0; mi < 4; mi++)
        if (v.mu[mi].on && v.mu[mi].mode == MU_FM && v.mu[mi].src == FS_NOISE)
            needNoise = true;
    bool needRing = v.mgT[3][0] + v.mgT[3][1] + v.mgT[3][2] + v.mgT[3][3] > 0.0f;
    for (int mi = 0; mi < 4; mi++)
        if (v.mu[mi].on && v.mu[mi].mode == MU_FM && v.mu[mi].src == FS_RING)
            needRing = true;
    bool need3 = v.mgT[2][0] + v.mgT[2][1] + v.mgT[2][2] + v.mgT[2][3] > 0.0f || v.mg[2][0] + v.mg[2][1] > 0.0f || rs1 == RS_OSC3 ||
                 rs2 == RS_OSC3 || (needRing && (rs1 == RS_OSC3 || rs2 == RS_OSC3));
    for (int mi = 0; mi < 4; mi++)
        if (v.mu[mi].on && ((v.mu[mi].mode == MU_FM && v.mu[mi].src == FS_OSC3) || (v.mu[mi].mode == MU_SYNC && v.mu[mi].syncSrc == 2)))
            need3 = true;
    float out[2][2 * CH];
    for (int n = 0; n < N; n++) {
        for (int o = 0; o < 3; o++)
            v.inc[o] += incI[o];
        /* osc 1 and 2, each through its two mutants */
        for (int o = 0; o < 2; o++) {
            for (int s = 0; s < 2; s++) {
                Mut &M = v.mu[o * 2 + s];
                M.r += M.rI;
                M.d += M.dI;
                M.m += M.mI;
                if (!M.on)
                    continue;
                float y1 = M.y1, yAvg = 0.5f * (M.y1 + M.y2);
                switch (M.mode) {
                case MU_FM: {
                    switch (M.src) {
                    case FS_SINE: M.sVal = fsin(M.modPh); break;
                    case FS_TRI: M.sVal = M.modPh < 0.25f ? 4.0f * M.modPh : M.modPh < 0.75f ? 2.0f - 4.0f * M.modPh : 4.0f * M.modPh - 4.0f; break;
                    case FS_OSC1: case FS_OSC2: case FS_OSC3: M.sVal = v.oo[M.src - FS_OSC1]; break;
                    case FS_RING: M.sVal = v.ring; break;
                    case FS_NOISE: M.sVal = v.noise; break;
                    default: M.sVal = v.mo[M.src - FS_MUT1]; break;
                    }
                    M.modPh += v.inc[o] * M.r;
                    M.modPh -= std::floor(M.modPh);
                    M.fbT = M.f * 0.35f * yAvg;
                    break;
                }
                case MU_STACK:
                    for (int j = 0; j < 5; j++) {
                        M.stack[j] += STACK_C[j] * M.spread * v.inc[o];
                        M.stack[j] -= std::floor(M.stack[j]);
                    }
                    break;
                case MU_SYNC: M.fbT = M.f * 0.5f * y1; break;
                case MU_PWORIG: M.fbT = M.f * 0.25f * y1; break;
                case MU_PWSQZ: M.fbT = M.f * 0.5f * y1; break;
                case MU_PWASM: M.fbT = M.f * 0.3f * yAvg; break;
                case MU_HARM: M.fbT = M.f * 0.6f * M.harmN; break;
                case MU_PHAZ: M.fbT = M.f * 0.2f * y1; break;
                default: break;
                }
            }
            float x = v.ph[o];
            Mut &A = v.mu[o * 2], &B = v.mu[o * 2 + 1];
            float y1 = A.on ? evalChain(v, o, 1, x, true) : wave(v, o, x);
            if (A.on) {
                A.y2 = A.y1;
                A.y1 = y1;
            }
            float y2 = B.on ? evalChain(v, o, 2, x, true) : y1;
            if (B.on) {
                B.y2 = B.y1;
                B.y1 = y2;
            }
            v.mo[o * 2] = y1;
            v.mo[o * 2 + 1] = y2;
            v.oo[o] = y2;
        }
        v.oo[2] = need3 ? wave(v, 2, v.ph[2]) : 0.0f;
        for (int o = 0; o < 3; o++) {
            v.ph[o] += v.inc[o];
            v.ph[o] -= std::floor(v.ph[o]);
        }
        /* noise */
        if (needNoise) {
            float w = rndF(v.seed);
            float *b = v.pink;
            b[0] = 0.99886f * b[0] + w * 0.0555179f;
            b[1] = 0.99332f * b[1] + w * 0.0750759f;
            b[2] = 0.96900f * b[2] + w * 0.1538520f;
            b[3] = 0.86650f * b[3] + w * 0.3104856f;
            b[4] = 0.55000f * b[4] + w * 0.5329522f;
            b[5] = -0.7616f * b[5] - w * 0.0168980f;
            float pink = (b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + w * 0.5362f) * 0.18f;
            b[6] = w * 0.115926f;
            v.brown = v.brown * 0.996f + w * 0.07f;
            v.red = v.red * 0.985f + v.brown * 0.015f;
            float nz;
            switch (ntype) {
            case NT_PINK: nz = pink; break;
            case NT_BROWN: nz = v.brown * 0.75f; break;
            case NT_RED: nz = v.red * 0.85f; break;
            case NT_BLUE: nz = (pink - v.prevP) * 2.2f; break;
            case NT_VIOLET: nz = (w - v.prevW) * 0.5f; break;
            case NT_GREY: nz = v.brown * 0.6f + (w - v.prevW) * 0.12f + w * 0.1f; break;
            default: nz = w * 0.6f; break;
            }
            v.prevW = w;
            v.prevP = pink;
            v.noise = nz;
        } else
            v.noise = 0.0f;
        /* the ring modulator */
        if (needRing) {
            auto rsv = [&](int s) {
                switch (s) {
                case RS_OSC1: return v.oo[0];
                case RS_OSC2: return v.oo[1];
                case RS_OSC3: return v.oo[2];
                case RS_NOISE: return v.noise;
                default: return v.mo[s - RS_MUT1];
                }
            };
            float a = rsv(rs1), b2 = rsv(rs2);
            v.ring = a * (1.0f - rdep + rdep * b2);
        } else
            v.ring = 0.0f;
        /* the mixer */
        const float srcs[5] = {v.oo[0], v.oo[1], v.oo[2], v.ring, v.noise};
        float in1[2] = {0, 0}, in2[2] = {0, 0};
        for (int s = 0; s < 5; s++) {
            for (int c = 0; c < 4; c++)
                v.mg[s][c] += mgI[s][c];
            in1[0] += srcs[s] * v.mg[s][0];
            in1[1] += srcs[s] * v.mg[s][1];
            in2[0] += srcs[s] * v.mg[s][2];
            in2[1] += srcs[s] * v.mg[s][3];
        }
        /* the filters */
        g1 += g1I;
        g2 += g2I;
        float k2 = 2.0f - v.r2 * 1.97f;
        for (int c = 0; c < (v.monoMix ? 1 : 2); c++) {
            float y1 = v.useF1 ? filter1(v, v.f1[c], in1[c], g1, v.r1, v.gateAmp) : 0.0f;
            float y2 = 0.0f;
            if (v.useF2) {
                float x2 = v.series ? y1 + in2[c] : in2[c];
                Svf &f = v.f2[c];
                f.step(softT(x2 * 0.8f) * 1.25f, g2, k2);
                float bpn = f.bp * maxf(k2, 0.25f), m = v.morph;
                if (v.f2Type == 0)
                    y2 = m < 0.5f ? lerpf(f.lo, bpn, m * 2.0f) : lerpf(bpn, f.hp, m * 2.0f - 1.0f);
                else {
                    float nt = f.lo + f.hp;
                    y2 = m < 0.5f ? lerpf(f.lo, nt, m * 2.0f) : lerpf(nt, f.hp, m * 2.0f - 1.0f);
                }
            }
            out[c][n] = v.series ? y2 : y1 + y2;
        }
        if (v.monoMix)
            out[1][n] = out[0][n];
    }
    /* down to the rate, the amp */
    float ampI = (v.ampT - v.amp) / (float)CH;
    float width = (int)P[P_VO_POLY] >= PO_MONO && (int)P[P_VO_POLY] <= PO_MONOHI ? 0.0f : P[P_VO_WIDTH] / 127.0f;
    width += (1.0f - width) * spread;              /* VOICE SPREAD opens the field further */
    float pl = minf(1.0f, 1.0f - v.panPos * width), pr = minf(1.0f, 1.0f + v.panPos * width);
    float pk = 0.0f;
    for (int i = 0; i < CH; i++) {
        float l, r;
        if (os == 2) {
            l = v.hb[0].down(out[0][2 * i], out[0][2 * i + 1]);
            r = v.monoMix ? l : v.hb[1].down(out[1][2 * i], out[1][2 * i + 1]);
        } else {
            l = out[0][i];
            r = out[1][i];
        }
        v.amp += ampI;
        v.fadeIn = minf(1.0f, v.fadeIn + v.fadeK);
        float a = v.amp * v.fadeIn;
        L[i] += l * a * pl;
        R[i] += r * a * pr;
        pk = maxf(pk, std::fabs(l * a));
    }
    v.shown = v.shown * 0.9f + pk * 0.1f;
    for (int c = 0; c < 2; c++) {
        for (int i = 0; i < 8; i++)
            v.f1[c].s[i] = flushDn(v.f1[c].s[i]);
        v.f1[c].a.flush();
        v.f2[c].flush();
        for (auto &f : v.f1[c].form)
            f.flush();
        v.hb[c].flush();
    }
    for (auto &m : v.mu)
        m.hs.flush();
}

/* ================================================================== clock === */
void Engine::host(bool playing, double ppq, double b, bool sync)
{
    hostSync = sync;
    if (b > 10.0)
        hostBpm = (float)b;
    bool wasPlaying = hostPlaying;
    hostPlaying = playing;
    hostPpq = ppq;
    if (sync && playing) {
        if (!wasPlaying || std::fabs(beatPos - ppq) > 0.05)
            beatPos = ppq;
        if (!wasPlaying)
            arpRunning = false;
    }
}
void Engine::clock()
{
    bpm = hostSync && hostBpm > 10.0f ? hostBpm : gval[P_AR_TEMPO];
    beatPos += (double)(bpm / 60.0f) * (double)CH / (double)sr;
}

/* ===================================================================== arp === */
void Engine::arpRebuild()
{
    arpLen = 0;
    if (arpN == 0)
        return;
    const float *P = gval;
    int mode = (int)P[P_AR_MODE], octMode = (int)P[P_AR_OCTMODE], R = std::max(1, std::min(6, (int)P[P_AR_OCT]));
    int8_t base[NKEYS * 2];
    int nb = 0;
    int8_t sorted[NKEYS];
    std::copy(arpHeld, arpHeld + arpN, sorted);
    std::sort(sorted, sorted + arpN);
    switch (mode) {
    case AM_DOWN: for (int i = arpN - 1; i >= 0; i--) base[nb++] = sorted[i]; break;
    case AM_UPDN:
        for (int i = 0; i < arpN; i++) base[nb++] = sorted[i];
        for (int i = arpN - 1; i >= 0; i--) base[nb++] = sorted[i];
        break;
    case AM_UPANDDN:
        for (int i = 0; i < arpN; i++) base[nb++] = sorted[i];
        for (int i = arpN - 2; i >= 1; i--) base[nb++] = sorted[i];
        break;
    case AM_ORDER: for (int i = 0; i < arpN; i++) base[nb++] = arpHeld[i]; break;
    case AM_CHORD: base[nb++] = -100; break;                     /* every held note (a chord) */
    case AM_PHRASE: {
        const Phrase &ph = phrase((int)P[P_AR_PHRASE] - 1);
        for (int i = 0; i < ph.len && nb < NKEYS * 2; i++) {
            int8_t d = ph.step[i];
            if (d == PH_REST || d == PH_TIE)
                base[nb++] = d == PH_REST ? -1 : -2;
            else {
                int deg = (int)d - PH_ZERO, oc = 0;
                while (deg < 0) { deg += arpN; oc--; }
                oc += deg / arpN;
                base[nb++] = (int8_t)std::max(0, std::min(127, sorted[deg % arpN] + 12 * oc));
            }
        }
        break;
    }
    default: for (int i = 0; i < arpN; i++) base[nb++] = sorted[i]; break;
    }
    /* the octaves */
    int oct[16], no = 0;
    switch (octMode) {
    case AO_DN: for (int i = 0; i < R; i++) oct[no++] = -i; break;
    case AO_UPDN:
        for (int i = 0; i < R; i++) oct[no++] = i;
        for (int i = R - 2; i >= 1; i--) oct[no++] = i;
        break;
    default: for (int i = 0; i < R; i++) oct[no++] = i; break;
    }
    int16_t seq[256];
    int n = 0;
    for (int i = 0; i < no && n < 256; i++)
        for (int j = 0; j < nb && n < 256; j++) {
            int8_t x = base[j];
            seq[n++] = x == -100 ? (int16_t)(1000 + oct[i]) : x < 0 ? (int16_t)x : (int16_t)std::max(0, std::min(127, (int)x + 12 * oct[i]));
        }
    if ((octMode == AO_ALT || octMode == AO_ALT2) && n > 1) {
        /* Alt: the whole pattern forward, then backward (Alt 2: the ends not twice) */
        int fwd = n;
        int from = octMode == AO_ALT ? fwd - 1 : fwd - 2, to = octMode == AO_ALT ? 0 : 1;
        for (int i = from; i >= to && n < 256; i--)
            seq[n++] = seq[i];
    }
    /* Length: the pattern cut short, or carried on, to that many steps */
    int len = (int)P[P_AR_LEN];
    if (len > 0 && n > 0) {
        int16_t tmp[256];
        int m = std::min(len, 256);
        for (int i = 0; i < m; i++)
            tmp[i] = seq[i % n];
        n = m;
        std::copy(tmp, tmp + n, seq);
    }
    std::copy(seq, seq + n, arpSeq);
    arpLen = n;
}

void Engine::arpTick()
{
    const float *P = gval;
    bool on = P[P_AR_ON] > 0.5f;
    auto releaseAll = [&]() {
        for (int i = 0; i < arpNPlaying; i++)
            keyUp(arpPlaying[i]);
        arpNPlaying = 0;
    };
    if (!on || arpN == 0 || arpLen == 0) {
        if (arpRunning) {
            releaseAll();
            arpRunning = false;
            arpShown = -1;
        }
        if (!on && arpN > 0 && P[P_AR_LATCH] < 0.5f) {
            /* the arp turned off with keys down: they sound as keys again */
            bool any = false;
            for (int i = 0; i < 128; i++)
                any = any || sostHeld[i];
            if (!any)
                arpN = 0;
        }
        return;
    }
    const float div = ARPDIV_BEATS[std::max(0, std::min(NARPDIV - 1, (int)P[P_AR_DIV]))];
    if (!arpRunning) {
        arpRunning = true;
        arpCount = 0;
        bool lock = P[P_AR_CLKLOCK] > 0.5f && hostSync && hostPlaying;
        arpNext = lock ? std::ceil(beatPos / (double)div - 1e-6) * (double)div : beatPos;
        arpOff = -1.0;
        arpRatchetLeft = 0;
    }
    if (arpOff >= 0.0 && beatPos >= arpOff) {
        releaseAll();
        arpOff = -1.0;
    }
    if (arpRatchetLeft > 0 && beatPos >= arpRatchetNext) {
        /* a ratchet: the same notes again */
        int notes[8], nn = arpNPlaying;
        std::copy(arpPlaying, arpPlaying + nn, notes);
        releaseAll();
        for (int i = 0; i < nn; i++) {
            keyDown(notes[i], arpVel[0]);
            arpPlaying[arpNPlaying++] = notes[i];
        }
        arpRatchetLeft--;
        arpOff = arpRatchetNext + arpRatchetStep * (double)(P[P_AR_GATE] / 100.0f);
        arpRatchetNext += arpRatchetStep;
    }
    if (beatPos < arpNext)
        return;
    /* a step */
    float swing = clampf(P[P_AR_SWING] / 100.0f, 0.5f, 0.75f);
    double len = (double)div * 2.0 * (double)((arpCount & 1) ? 1.0f - swing : swing);
    int off = (int)P[P_AR_OFFSET];
    int idx = (int)(((arpCount + off) % arpLen + arpLen) % arpLen);
    arpShown = idx;
    int16_t e = arpSeq[idx];
    int mode = (int)P[P_AR_MODE];
    if (mode == AM_RANDOM) {
        int R = std::max(1, std::min(6, (int)P[P_AR_OCT]));
        e = (int16_t)std::min(127, arpHeld[rndU(rseed) % (uint32_t)arpN] + 12 * (int)(rndU(rseed) % (uint32_t)R));
    }
    double start = arpNext;
    arpNext += len;
    arpCount++;
    if (e == -2) {                                   /* a tie: the notes go on */
        arpOff = arpNext + len * 0.0;
        return;
    }
    releaseAll();
    if (e == -1)
        return;
    float gate = clampf(P[P_AR_GATE] / 100.0f, 0.05f, 1.0f);
    float vel = arpVel[0];
    if (e >= 900) {
        int o = e - 1000;
        for (int i = 0; i < arpN && arpNPlaying < 8; i++) {
            int nt = std::max(0, std::min(127, arpHeld[i] + 12 * o));
            keyDown(nt, arpVel[i]);
            arpPlaying[arpNPlaying++] = nt;
        }
    } else {
        for (int i = 0; i < arpN; i++)
            if (arpHeld[i] == e % 12 + 12 * (e / 12)) vel = arpVel[i];
        keyDown(e, vel);
        arpPlaying[arpNPlaying++] = e;
    }
    static const int RAT[4] = {1, 2, 4, 8};
    int rat = RAT[std::max(0, std::min(3, (int)P[P_AR_RATCHET]))];
    float chance = P[P_AR_CHANCE] / 100.0f;
    if (rat > 1 && rnd01(rseed) < chance) {
        arpRatchetStep = len / (double)rat;
        arpRatchetLeft = rat - 1;
        arpRatchetNext = start + arpRatchetStep;
        arpOff = start + arpRatchetStep * (double)gate;
    } else {
        arpRatchetLeft = 0;
        arpOff = start + len * (double)gate;
        if (gate >= 0.999f)
            arpOff = arpNext - 1e-6;
    }
}

/* ============================================================ performance === */
void Engine::sustain(bool on)
{
    int pedal = (int)pat.v[P_VO_SUSPED];
    susV = on ? 1.0f : 0.0f;
    if (on && !susOn)
        for (auto &v : V) {
            if (pedal == SP_SOSTENUTO && v.active && v.gate)
                v.sost = true;
            /* SusPed On: an envelope trigger */
            if (v.active)
                for (int e = 0; e < 5; e++)
                    for (int t = 0; t < 4; t++)
                        if ((int)v.val[pEnv(e, EN_TRIG1 + t)] == TR_SUSPED)
                            envGate(v, e, true);
        }
    susOn = on;
    if (!on)
        for (auto &v : V) {
            v.sost = false;
            if (v.active && v.gate && v.sus) {
                bool held = false;
                for (int i = 0; i < nKeys; i++)
                    if (keys[i] == v.note)
                        held = true;
                if (!held)
                    releaseVoice(v);
                else
                    v.sus = false;
            }
        }
}
void Engine::bend(float x) { bendV = clampf(x, -1.0f, 1.0f); }
void Engine::modWheel(float x) { mwV = clampf(x, 0.0f, 1.0f); }
void Engine::aftertouch(float x) { atV = clampf(x, 0.0f, 1.0f); }
void Engine::polyAftertouch(int note, float x)
{
    for (auto &v : V)
        if (v.active && v.note == note)
            v.press = clampf(x, 0.0f, 1.0f);
}
void Engine::cc(int n, float x)
{
    if (n < 0 || n > 127)
        return;
    ccV[n] = clampf(x, 0.0f, 1.0f);
    if (n == 2)
        brV = ccV[n];
    if (n == 11)
        expV = ccV[n];
}
void Engine::allOff()
{
    for (auto &v : V)
        if (v.active && v.gate)
            releaseVoice(v);
    nKeys = 0;
    arpN = 0;
    bassN = 0;
    std::memset(sostHeld, 0, sizeof sostHeld);
    std::memset(splitHeld, 0, sizeof splitHeld);
}
void Engine::panic()
{
    for (auto &v : V) {
        v.active = v.gate = false;
        v.amp = v.ampT = 0.0f;
    }
    nKeys = arpN = arpNPlaying = bassN = 0;
    std::memset(splitHeld, 0, sizeof splitHeld);
    arpRunning = false;
    std::memset(sostHeld, 0, sizeof sostHeld);
    susOn = false;
    pre.clear();
    post.clear();
    dly.clear();
    rev.clear();
}
void Engine::tapTrigger(int e)
{
    if (e < 0 || e > 4)
        return;
    for (auto &v : V)
        if (v.active)
            envGate(v, e, true);
}
int Engine::voicesSounding() const
{
    int n = 0;
    for (int i = 0; i < nVoices; i++)
        if (V[i].active)
            n++;
    return n;
}

/* ================================================================== chunk === */
void Engine::chunk()
{
    const float dt = (float)CH / sr;
    std::memset(bufL, 0, sizeof bufL);
    std::memset(bufR, 0, sizeof bufR);
    clock();
    /* the shared LFOs, the vibrato */
    for (int l = 0; l < 5; l++)
        if ((int)gval[pLfo(l, LF_TRIG)] != TS_POLY) {
            if (!glfo[l].started)
                lfoTrig(glfo[l], gval, l);
            lfoStep(glfo[l], gval, l, dt, nullptr);
        }
    float vr = gval[P_VO_VIBBPM] > 0.5f ? bpm / 60.0f / SYNC_BEATS[std::max(0, std::min(NSYNC - 1, (int)gval[P_VO_VIBSYNC]))]
                                        : gval[P_VO_VIBRATE];
    vibPh += vr * dt;
    vibPh -= std::floor(vibPh);
    /* the arp */
    arpTick();
    /* the voices */
    int newest = -1;
    uint32_t best = 0;
    for (int i = 0; i < nVoices; i++) {
        Voice &v = V[i];
        if (!v.active)
            continue;
        /* LFOs, then the envelopes (an LFO's wrap can trigger one) */
        for (int l = 0; l < 5; l++)
            if ((int)v.val[pLfo(l, LF_TRIG)] == TS_POLY)
                lfoStep(v.lfo[l], v.val, l, dt, &v);
        for (int e = 0; e < 5; e++) {
            for (int t = 0; t < 4; t++) {
                int ts = (int)v.val[pEnv(e, EN_TRIG1 + t)];
                if (ts >= TR_LFO1 && ts <= TR_LFO5) {
                    int l = ts - TR_LFO1;
                    bool poly = (int)v.val[pLfo(l, LF_TRIG)] == TS_POLY;
                    if ((poly ? v.lfo[l].wrap : glfo[l].wrap) && v.gate) {
                        envGate(v, e, true);
                        break;
                    }
                }
            }
            envStep(v, e, dt);
        }
        sources(v, false);
        modulate(v);
        voiceSetup(v);
        voiceRender(v, bufL, bufR);
        if (v.env[1].st == ES_IDLE && v.amp < 1e-5f) {
            v.active = false;
            v.gate = false;
        }
        if (v.age > best) {
            best = v.age;
            newest = i;
        }
    }
    /* the global parameters: as the newest voice has them, or with no voice the shared sources' */
    if (newest >= 0)
        gval = V[newest].val;
    else {
        sources(ghost, true);
        modulate(ghost);
        gval = ghost.val;
    }
    for (int l = 0; l < 5; l++)
        lfoShow[l] = (int)gval[pLfo(l, LF_TRIG)] == TS_POLY ? (newest >= 0 ? V[newest].lfo[l].out : 0.0f) : glfo[l].out;
    for (int e = 0; e < 5; e++)
        envShow[e] = newest >= 0 ? V[newest].env[e].v : 0.0f;
    for (int o = 0; o < 3; o++)
        oscShow[o] = newest >= 0 ? V[newest].oo[o] : 0.0f;
    /* warm mode, the effects */
    const float *P = gval;
    if (P[P_VO_WARM] > 0.5f)
        for (int i = 0; i < CH; i++) {
            bufL[i] = warmLp[0].run(warmLs[0].run(bufL[i]));
            bufR[i] = warmLp[1].run(warmLs[1].run(bufR[i]));
        }
    for (int f = 0; f < 2; f++) {
        float p6[6];
        for (int k = 0; k < 6; k++)
            p6[k] = P[pFx(f, FX_P1 + k)];
        (f ? post : pre).set((int)P[pFx(f, FX_TYPE)], (int)P[pFx(f, FX_PRESET)], p6, P[pFx(f, FX_WET)], bpm, beatPos);
    }
    pre.process(bufL, bufR, CH);
    float dTime = P[P_DL_BPM] > 0.5f ? SYNC_BEATS[std::max(0, std::min(20, (int)P[P_DL_SYNC]))] * 60.0f / bpm : P[P_DL_TIME] * 0.001f;
    dly.set((int)P[P_DL_TYPE], dTime, P[P_DL_FB] / 128.0f * 1.02f, P[P_DL_WTONE], P[P_DL_FTONE], P[P_DL_WET]);
    if (P[P_DL_WET] > 0.0f || dly.tail() > 1e-5f)
        dly.process(bufL, bufR, CH);
    rev.set((int)P[P_RV_TYPE], P[P_RV_PRE], P[P_RV_TIME], P[P_RV_FREEZE] > 0.5f, P[P_RV_TONE], P[P_RV_HIDAMP], P[P_RV_LODAMP], P[P_RV_WET]);
    if (!rev.idle())
        rev.process(bufL, bufR, CH);
    post.process(bufL, bufR, CH);
    for (int i = 0; i < CH; i++) {
        scope[scopeW] = 0.5f * (bufL[i] + bufR[i]);
        scopeW = (scopeW + 1) & (SCOPE - 1);
    }
}

void Engine::render(float *L, float *R, int n)
{
    for (int i = 0; i < n; i++) {
        if (gpos >= CH) {
            chunk();
            gpos = 0;
        }
        L[i] = bufL[gpos];
        R[i] = bufR[gpos];
        gpos++;
    }
}
} // namespace hy
