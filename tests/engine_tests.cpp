/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
/* HYDRATE's engine tests, without JUCE: every filter type, every Mutant mode, every effect, the arp's modes, the
 * voice modes, 8 and 16 voices, random patches (nothing may go NaN, blow up or stay silent where it should sound),
 * the factory patches at three rates, and the CPU of a heavy patch. */
#include "Engine.h"
#include "Presets.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace hy;
static int fails = 0;
#define CHECK(c, ...)                                                                                                   \
    do {                                                                                                                \
        if (!(c)) {                                                                                                     \
            std::printf("FAIL: " __VA_ARGS__);                                                                          \
            std::printf("\n");                                                                                          \
            fails++;                                                                                                    \
        }                                                                                                               \
    } while (0)

struct Res {
    float peak = 0, rms = 0;
    bool finite = true;
};
static Res run(Engine &e, int frames, int block = 256)
{
    std::vector<float> L((size_t)block), R((size_t)block);
    Res r;
    double acc = 0;
    long n = 0;
    for (int done = 0; done < frames; done += block) {
        int b = std::min(block, frames - done);
        e.render(L.data(), R.data(), b);
        for (int i = 0; i < b; i++) {
            if (!std::isfinite(L[(size_t)i]) || !std::isfinite(R[(size_t)i]))
                r.finite = false;
            r.peak = std::max(r.peak, std::max(std::fabs(L[(size_t)i]), std::fabs(R[(size_t)i])));
            acc += (double)L[(size_t)i] * L[(size_t)i];
            n++;
        }
    }
    r.rms = (float)std::sqrt(acc / (double)std::max(1L, n));
    return r;
}
static Res chord(Patch &p, float sr = 48000.0f, int voices = 8, float secs = 0.8f)
{
    Engine e;
    e.prepare(sr);
    e.setVoices(voices);
    e.setPatch(p);
    e.noteOn(48, 0.9f);
    e.noteOn(55, 0.8f);
    e.noteOn(64, 0.7f);
    Res r = run(e, (int)(sr * secs));
    e.noteOff(48);
    e.noteOff(55);
    e.noteOff(64);
    Res t = run(e, (int)(sr * 0.5f));
    r.finite = r.finite && t.finite;
    r.peak = std::max(r.peak, t.peak);
    return r;
}

int main()
{
    Patch p;
    /* every filter 1 type, low and high resonance */
    for (int t = 0; t < NF1TYPE; t++)
        for (float res : {0.0f, 127.0f}) {
            patchInit(p);
            p.v[P_F1_TYPE] = (float)t;
            p.v[P_F1_CUT] = 70.0f;
            p.v[P_F1_RES] = res;
            p.v[P_F1_DRIVE] = 40.0f;
            Res r = chord(p);
            CHECK(r.finite && r.peak < 8.0f && r.rms > 1e-4f, "filter 1 type %d res %.0f: peak %.3f rms %.5f", t, res, r.peak, r.rms);
        }
    /* filter 2 both types across the morph, parallel routing */
    for (int t = 0; t < 2; t++)
        for (float m : {0.0f, 64.0f, 128.0f}) {
            patchInit(p);
            p.v[P_MX_ROUTE] = 1;
            p.v[P_MX_FR1] = 128;
            p.v[P_F2_TYPE] = (float)t;
            p.v[P_F2_MORPH] = m;
            p.v[P_F2_CUT] = 80;
            p.v[P_F2_RES] = 100;
            Res r = chord(p);
            CHECK(r.finite && r.peak < 8.0f && r.rms > 1e-4f, "filter 2 type %d morph %.0f: peak %.3f rms %.5f", t, m, r.peak, r.rms);
        }
    /* every mutant mode, on both of osc 1's, with feedback */
    for (int md = 0; md < NMUTMODE; md++)
        for (float fb : {0.0f, 150.0f}) {
            patchInit(p);
            for (int k = 0; k < 2; k++) {
                p.v[pMut(k, MT_MODE)] = (float)md;
                p.v[pMut(k, MT_DEPTH)] = 90.0f;
                p.v[pMut(k, MT_RATIO)] = 2.5f;
                p.v[pMut(k, MT_FB)] = fb;
                p.v[pMut(k, MT_WINDOW)] = 50.0f;
                p.v[pMut(k, MT_WARP1 + 2)] = 120.0f;
            }
            p.v[pMut(0, MT_FMSRC)] = FS_OSC3;
            p.v[P_MX_VOL1 + 2] = 0.0f;
            Res r = chord(p);
            CHECK(r.finite && r.peak < 8.0f && r.rms > 1e-4f, "mutant mode %d fb %.0f: peak %.3f rms %.5f", md, fb, r.peak, r.rms);
        }
    /* wavescan through every wave, ring and every noise */
    for (int w = 0; w < NWAVE; w += 7) {
        patchInit(p);
        p.v[pOsc(0, OS_MODE)] = OM_SCAN;
        p.v[pOsc(0, OS_WAV1)] = (float)w;
        p.v[pOsc(0, OS_WAV1 + 1)] = -1.0f;
        p.v[pOsc(0, OS_WAV1 + 2)] = (float)((w + 50) % NWAVE);
        p.v[pOsc(0, OS_SCAN)] = 2.6f;
        p.v[pOsc(0, OS_BITS)] = 8;
        Res r = chord(p, 48000, 8, 0.3f);
        CHECK(r.finite && r.peak < 8.0f && r.rms > 1e-4f, "wavescan from %d: peak %.3f rms %.5f", w, r.peak, r.rms);
    }
    for (int nt = 0; nt < NNOISE; nt++) {
        patchInit(p);
        p.v[P_MX_VOL1] = 0;
        p.v[P_MX_VOL1 + 3] = 100;
        p.v[P_MX_VOL1 + 4] = 100;
        p.v[P_MX_VOL1 + 1] = 0;
        p.v[P_RN_NOISE] = (float)nt;
        Res r = chord(p, 48000, 8, 0.3f);
        CHECK(r.finite && r.peak < 8.0f && r.rms > 1e-4f, "noise %d / ring: peak %.3f rms %.5f", nt, r.peak, r.rms);
    }
    /* every effect */
    for (int f = 1; f < NFXTYPE; f++)
        for (int post = 0; post < 2; post++) {
            patchInit(p);
            float p6[6], wet;
            fxPreset(f, 0, p6, wet);
            p.v[pFx(post, FX_TYPE)] = (float)f;
            for (int k = 0; k < 6; k++)
                p.v[pFx(post, FX_P1 + k)] = p6[k];
            p.v[pFx(post, FX_WET)] = wet < 0 ? 50 : wet;
            Res r = chord(p);
            CHECK(r.finite && r.peak < 8.0f && r.rms > 1e-4f, "fx %d (%s): peak %.3f rms %.5f", f, post ? "post" : "pre", r.peak, r.rms);
        }
    for (int d = 0; d < NDLYTYPE; d++) {
        patchInit(p);
        p.v[P_DL_TYPE] = (float)d;
        p.v[P_DL_WET] = 50;
        p.v[P_DL_FB] = 128;
        Res r = chord(p, 48000, 8, 1.5f);
        CHECK(r.finite && r.peak < 8.0f, "delay %d: peak %.3f", d, r.peak);
    }
    for (int rv = 0; rv < NREVTYPE; rv++)
        for (int fr = 0; fr < 2; fr++) {
            patchInit(p);
            p.v[P_RV_TYPE] = (float)rv;
            p.v[P_RV_WET] = 60;
            p.v[P_RV_TIME] = 90;
            p.v[P_RV_FREEZE] = (float)fr;
            p.v[P_RV_HIDAMP] = 0;
            p.v[P_RV_LODAMP] = 0;
            Res r = chord(p, 48000, 8, 2.0f);
            CHECK(r.finite && r.peak < 8.0f && r.rms > 1e-4f, "reverb %d freeze %d: peak %.3f", rv, fr, r.peak);
        }
    /* the voice modes, 8 and 16 voices */
    for (int pm = 0; pm < NPOLY; pm++)
        for (int nv : {8, 16}) {
            patchInit(p);
            p.v[P_VO_POLY] = (float)pm;
            p.v[P_VO_DENSITY] = 8;
            p.v[P_VO_WIDTH] = 127;
            p.v[P_VO_GLIDE] = GL_GLIDE;
            p.v[P_VO_AFEEL] = 127;
            p.v[P_VO_WARM] = 1;
            Res r = chord(p, 48000, nv);
            CHECK(r.finite && r.peak < 8.0f && r.rms > 1e-4f, "voice mode %d, %d voices: peak %.3f", pm, nv, r.peak);
        }
    /* the arp, every mode and octave mode */
    for (int am = 0; am < NARPMODE; am++)
        for (int om = 0; om < NARPOCT; om++) {
            patchInit(p);
            p.v[P_AR_ON] = 1;
            p.v[P_AR_MODE] = (float)am;
            p.v[P_AR_OCTMODE] = (float)om;
            p.v[P_AR_OCT] = 3;
            p.v[P_AR_RATCHET] = 2;
            p.v[P_AR_CHANCE] = 50;
            p.v[P_AR_PHRASE] = (float)(am * 9 + om + 1);
            Res r = chord(p, 48000, 8, 1.2f);
            CHECK(r.finite && r.peak < 8.0f && r.rms > 1e-4f, "arp mode %d oct %d: rms %.5f", am, om, r.rms);
        }
    /* LOW SPLIT: a bass voice of its own below the split, the chord above; VOICE SPREAD on top */
    for (int nv : {8, 16})
        for (int poly : {(int)PO_POLY, (int)PO_MONO, (int)PO_UNI}) {
            Engine e;
            e.prepare(48000);
            e.setVoices(nv);
            Patch q;
            patchInit(q);
            q.v[P_VO_POLY] = (float)poly;
            e.setPatch(q);
            e.setSplit(true, 48);
            e.setSpread(1.0f);
            std::vector<float> L(9600), R(9600);
            e.noteOn(36, 0.8f);
            e.noteOn(60, 0.8f); e.noteOn(64, 0.8f); e.noteOn(67, 0.8f);
            e.render(L.data(), R.data(), 9600);
            int s1 = e.voicesSounding();
            CHECK(e.voiceLevel(nv - 1) > 0.0f || s1 >= 2, "split %d/%d: bass voice", nv, poly);
            e.noteOn(40, 0.8f);                     /* legato in the bass: still one bass voice */
            e.render(L.data(), R.data(), 9600);
            CHECK(e.voicesSounding() == s1, "split %d/%d: bass stays mono (%d vs %d)", nv, poly, e.voicesSounding(), s1);
            e.noteOff(40); e.noteOff(36);
            e.noteOff(60); e.noteOff(64); e.noteOff(67);
            e.setSplit(false, 48);
            for (int k = 0; k < 30; k++)
                e.render(L.data(), R.data(), 9600);
            CHECK(e.voicesSounding() == 0, "split %d/%d: all quiet after release (%d)", nv, poly, e.voicesSounding());
        }
    /* RANDOM: 200 seeds, each audible, finite and at the factory level */
    {
        int quiet = 0, loud = 0;
        auto t0 = std::chrono::steady_clock::now();
        for (uint32_t sd = 1; sd <= 200; sd++) {
            Patch q = randomPatch(sd);
            Res r = chord(q);
            CHECK(r.finite, "random button seed %u: not finite", sd);
            if (r.rms < 0.03f) quiet++;
            if (r.peak > 2.5f) loud++;
        }
        std::printf("RANDOM: 200 patches, %d quiet, %d loud, %.1f ms each\n", quiet, loud, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 200.0);
        CHECK(quiet <= 10 && loud <= 4, "random button: %d quiet, %d loud", quiet, loud);
    }
    /* random patches: every parameter anywhere in its range */
    srand(1234);
    for (int k = 0; k < 120; k++) {
        patchInit(p);
        for (int i = 0; i < NPARAM; i++) {
            if (i >= P_AR_ON && i <= P_AR_CLKLOCK)
                continue;
            if (rand() % 3 == 0)
                p.v[i] = fromNorm(i, (float)rand() / (float)RAND_MAX);
        }
        p.v[pEnv(1, EN_ATK)] = 0.01f;
        p.v[pEnv(1, EN_DLY)] = 0.0f;
        p.v[P_RV_TIME] = 2.0f;
        p.v[P_RV_FREEZE] = 0;
        Res r = chord(p, 44100, 16, 0.5f);
        CHECK(r.finite && r.peak < 30.0f, "random patch %d: peak %.3f finite %d", k, r.peak, (int)r.finite);
    }
    /* the factory patches at three rates */
    for (int i = 0; i < NFACTORY; i++)
        for (float sr : {44100.0f, 48000.0f, 96000.0f}) {
            p = factory(i);
            Res r = chord(p, sr, 16, 0.8f);
            CHECK(r.finite && r.peak < 4.0f && r.rms > 1e-4f, "factory %d %s at %.0f: peak %.3f rms %.5f", i, p.name, sr, r.peak, r.rms);
        }
    /* CPU: 16 voices of a heavy patch */
    {
        patchInit(p);
        for (int k = 0; k < 4; k++) {
            p.v[pMut(k, MT_MODE)] = MU_STACK;
            p.v[pMut(k, MT_DEPTH)] = 40;
        }
        p.v[P_MX_VOL1 + 1] = 100;
        p.v[P_MX_VOL1 + 2] = 100;
        p.v[P_MX_VOL1 + 4] = 30;
        p.v[P_F1_TYPE] = F1_8POLE;
        p.v[P_MX_ROUTE] = 0;
        p.v[P_RV_WET] = 30;
        p.v[P_DL_WET] = 20;
        p.v[pFx(0, FX_TYPE)] = FX_CHORUS;
        Engine e;
        e.prepare(48000);
        e.setVoices(16);
        e.setPatch(p);
        for (int n = 0; n < 16; n++)
            e.noteOn(40 + n * 2, 0.8f);
        auto t0 = std::chrono::steady_clock::now();
        Res r = run(e, 48000 * 2);
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        std::printf("CPU: 16 voices, WavStack x4, 3 osc, 8 pole, FX: %.1f%% of one core (peak %.2f)\n", ms / 2000.0 * 100.0, r.peak);
        CHECK(r.finite, "heavy patch finite");
    }
    std::printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
