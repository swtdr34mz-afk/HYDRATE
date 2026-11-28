/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
/* HYDRATE's patch: every parameter of a Hydrasynth patch (owner's manual 2.2.0), in the instrument's own ranges and
 * units, grouped by module as the panel's MODULE SELECT has them:
 *   OSC 1-3, MUTANT 1-4, RING-NOISE, MIXER, FILTER 1, FILTER 2, AMP, PRE-FX, DELAY, REVERB, POST-FX, ENV 1-5, LFO 1-5,
 *   VOICE, ARP, MACROS (8, eight destinations each), the MOD MATRIX (32 slots).
 * Values are floats in the units the display shows (Cutoff 0..128, Semi +-36, Attack in seconds ...). The step LFOs'
 * steps (64 each) live in the patch beside the parameters. */
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace hy
{
/* ------------------------------------------------------------------ choices --- */
enum OscMode { OM_SINGLE, OM_SCAN };
enum MutMode { MU_FM, MU_STACK, MU_SYNC, MU_PWORIG, MU_PWSQZ, MU_PWASM, MU_HARM, MU_PHAZ, NMUTMODE };
enum FmSrc { FS_SINE, FS_TRI, FS_OSC1, FS_OSC2, FS_OSC3, FS_RING, FS_NOISE, FS_MUT1, FS_MUT2, FS_MUT3, FS_MUT4, NFMSRC };
enum RingSrc { RS_OSC1, RS_OSC2, RS_OSC3, RS_NOISE, RS_MUT1, RS_MUT2, RS_MUT3, RS_MUT4, NRINGSRC };
enum NoiseType { NT_WHITE, NT_PINK, NT_BROWN, NT_RED, NT_BLUE, NT_VIOLET, NT_GREY, NNOISE };
enum F1Type {
    F1_LDR12, F1_LDR24, F1_FAT12, F1_FAT24, F1_GATE, F1_MS20LP, F1_3LER_LP, F1_STN12LP, F1_1POLE, F1_8POLE, F1_MS20HP,
    F1_STN12HP, F1_3LER_HP, F1_3LER_BP, F1_STN12BP, F1_VOWEL, NF1TYPE
};
enum LfoWave { LW_SINE, LW_TRI, LW_SAWUP, LW_SAWDN, LW_SQUARE, LW_PULSE27, LW_PULSE13, LW_SH, LW_NOISE, LW_RANDOM, LW_STEP, NLFOWAVE };
enum TrigSync { TS_POLY, TS_SINGLE, TS_OFF };
enum Poly { PO_POLY, PO_MONO, PO_MONOLO, PO_MONOHI, PO_UNI, PO_UNILO, PO_UNIHI, NPOLY };
enum StMode { SM_ROTATE, SM_ALTER, SM_RANDOM };
enum FxType { FX_BYPASS, FX_CHORUS, FX_FLANGER, FX_ROTARY, FX_PHASER, FX_LOFI, FX_TREMOLO, FX_EQ, FX_COMP, FX_DISTORT, NFXTYPE };
enum DlyType { DT_MONO, DT_STEREO, DT_PAN, DT_LRC, DT_REVERSE, NDLYTYPE };
enum RevType { RT_HALL, RT_ROOM, RT_PLATE, RT_CLOUD, NREVTYPE };
enum ArpMode { AM_UP, AM_DOWN, AM_UPDN, AM_UPANDDN, AM_ORDER, AM_RANDOM, AM_CHORD, AM_PHRASE, NARPMODE };
enum ArpOct { AO_UP, AO_DN, AO_UPDN, AO_ALT, AO_ALT2, NARPOCT };
enum Glide { GL_OFF, GL_GLIDE, GL_GLISS };
enum TrigSrc { TR_OFF, TR_NOTE, TR_LFO1, TR_LFO2, TR_LFO3, TR_LFO4, TR_LFO5, TR_SUSPED, NTRIGSRC };
enum OneShot { OS_OFF, OS_ON, OS_STEP };
enum SusPed { SP_SUSTAIN, SP_SOSTENUTO, SP_MODONLY };

/* ------------------------------------------------------ the parameter blocks --- */
/* an oscillator (osc 3 has Single mode only: its MODE, SCAN and WAV list are unused) */
enum { OS_MODE, OS_WAVE, OS_SEMI, OS_CENT, OS_KT, OS_BITS, OS_SCAN, OS_WAV1, OS_N = OS_WAV1 + 8 };
/* a Mutant */
enum { MT_MODE, MT_FMSRC, MT_SYNCSRC, MT_RATIO, MT_DEPTH, MT_WINDOW, MT_FB, MT_WET, MT_WARP1, MT_N = MT_WARP1 + 8 };
/* an envelope */
enum {
    EN_DLY, EN_ATK, EN_HLD, EN_DEC, EN_SUS, EN_REL, EN_BPM, EN_QUANT, EN_ACRV, EN_DCRV, EN_RCRV, EN_LEGATO, EN_RESET,
    EN_FREE, EN_LOOP, EN_TRIG1, EN_N = EN_TRIG1 + 4
};
/* an LFO */
enum {
    LF_WAVE, LF_RATE, LF_BPM, LF_SYNC, LF_TRIG, LF_DLY, LF_FADE, LF_PHASE, LF_LEVEL, LF_STEPS, LF_SMOOTH, LF_QUANT,
    LF_ONESHOT, LF_SEMILOCK, LF_N
};
/* a Pre-/Post-FX: its type, the type's preset, six parameters (0..1, as the type reads them), Dry/Wet */
enum { FX_TYPE, FX_PRESET, FX_P1, FX_P6 = FX_P1 + 5, FX_WET, FX_N };
/* a macro's destination */
enum { MR_DST, MR_DEPTH, MR_BTN, MR_N };
/* a mod-matrix slot */
enum { MM_SRC, MM_DST, MM_DEPTH, MM_N };

constexpr int NMOD = 32, NMACRO = 8, NMACRODST = 8, NSTEP = 64, NVMOD = 8;

enum Param {
    P_OSC1 = 0, P_OSC2 = P_OSC1 + OS_N, P_OSC3 = P_OSC2 + OS_N,
    P_MUT1 = P_OSC3 + OS_N,
    P_RN_SRC1 = P_MUT1 + 4 * MT_N, P_RN_SRC2, P_RN_DEPTH, P_RN_NOISE,
    /* MIXER: Osc 1, 2, 3, Ring, Noise */
    P_MX_VOL1, P_MX_PAN1 = P_MX_VOL1 + 5, P_MX_FR1 = P_MX_PAN1 + 5, P_MX_ROUTE = P_MX_FR1 + 5,
    /* FILTER 1 */
    P_F1_TYPE, P_F1_CTRL, P_F1_CUT, P_F1_RES, P_F1_ENV, P_F1_LFO, P_F1_VEL, P_F1_KT, P_F1_DRIVE, P_F1_DRVPOST, P_F1_VOWORD,
    /* FILTER 2 */
    P_F2_TYPE, P_F2_MORPH, P_F2_CUT, P_F2_RES, P_F2_ENV, P_F2_LFO, P_F2_VEL, P_F2_KT,
    /* AMP */
    P_AMP_LFO2, P_AMP_VEL, P_AMP_LEVEL,
    /* PRE-FX, POST-FX */
    P_PREFX, P_POSTFX = P_PREFX + FX_N,
    /* DELAY */
    P_DL_TYPE = P_POSTFX + FX_N, P_DL_TIME, P_DL_BPM, P_DL_SYNC, P_DL_FB, P_DL_WTONE, P_DL_FTONE, P_DL_WET,
    /* REVERB */
    P_RV_TYPE, P_RV_PRE, P_RV_TIME, P_RV_FREEZE, P_RV_TONE, P_RV_HIDAMP, P_RV_LODAMP, P_RV_WET,
    /* ENV 1-5, LFO 1-5 */
    P_ENV1, P_LFO1 = P_ENV1 + 5 * EN_N,
    /* VOICE */
    P_VO_POLY = P_LFO1 + 5 * LF_N, P_VO_DENSITY, P_VO_DETUNE, P_VO_AFEEL, P_VO_RPHASE, P_VO_STMODE, P_VO_WIDTH, P_VO_WARM,
    P_VO_PB, P_VO_VIBAMT, P_VO_VIBBPM, P_VO_VIBRATE, P_VO_VIBSYNC, P_VO_GLIDE, P_VO_GTIME, P_VO_GCURVE, P_VO_GLEGATO,
    P_VO_KEY, P_VO_SCALE, P_VO_SUSPED, P_VO_SNAP, P_VO_VMOD1, P_VO_VMOD8 = P_VO_VMOD1 + 7,
    /* ARPEGGIATOR */
    P_AR_ON, P_AR_LATCH, P_AR_TEMPO, P_AR_DIV, P_AR_SWING, P_AR_GATE, P_AR_OCTMODE, P_AR_OCT, P_AR_MODE, P_AR_LEN,
    P_AR_PHRASE, P_AR_RATCHET, P_AR_CHANCE, P_AR_OFFSET, P_AR_CLKLOCK,
    /* MACROS: the eight knobs, their buttons, their names, then 8 x 8 destinations */
    P_MC_VAL1, P_MC_BTN1 = P_MC_VAL1 + NMACRO, P_MC_NAME1 = P_MC_BTN1 + NMACRO, P_MC_DST = P_MC_NAME1 + NMACRO,
    /* the MOD MATRIX */
    P_MOD1 = P_MC_DST + NMACRO * NMACRODST * MR_N,
    NPARAM = P_MOD1 + NMOD * MM_N
};
inline int pOsc(int o, int k) { return P_OSC1 + o * OS_N + k; }
inline int pMut(int m, int k) { return P_MUT1 + m * MT_N + k; }
inline int pEnv(int e, int k) { return P_ENV1 + e * EN_N + k; }
inline int pLfo(int l, int k) { return P_LFO1 + l * LF_N + k; }
inline int pFx(int post, int k) { return (post ? P_POSTFX : P_PREFX) + k; }
inline int pMacroDst(int mc, int d, int k) { return P_MC_DST + (mc * NMACRODST + d) * MR_N + k; }
inline int pMod(int s, int k) { return P_MOD1 + s * MM_N + k; }

/* --------------------------------------------------------- the definitions --- */
enum Kind : uint8_t {
    K_NUM,      /* 0.0 .. 128.0 */
    K_BIP,      /* +-64.0 */
    K_INT,      /* a whole number */
    K_CHOICE,
    K_ONOFF,
    K_PCT,      /* 0 .. 100 % */
    K_SEMI,     /* +-36 semitones */
    K_CENT,
    K_KT,       /* keytrack % */
    K_RATIO,    /* 0.250 .. 64.000 */
    K_TIME,     /* seconds (ms below 1 s), a division when its module's BPM is on */
    K_HZ,
    K_PAN,      /* +-64 */
    K_FRAT,     /* filter ratio 100:0 .. 0:100 */
    K_MS,
    K_DEG,
    K_BPM,
    K_FXP,      /* a Pre/Post FX parameter (the type reads it) */
    K_SYNC,     /* a division (the list below) */
    K_WAVE,     /* one of the 219 waves (WAV list: OFF, Silence, then the waves) */
    K_SRC,      /* a mod source */
    K_DST,      /* a mod destination */
    K_RVTIME,   /* the reverb's time */
    K_STEPSEMI  /* (unused) */
};

struct ParamDef {
    char id[24];
    char name[32];
    char shortName[12];        /* the display's label (the Hydrasynth's, up to 8 letters) */
    float lo, hi, def;
    float step;                /* 1 for whole numbers and choices, 0 continuous */
    float skew;                /* the host's and the knobs' curve (1 linear) */
    Kind kind;
    const char *const *choice;
    int dst;                   /* its mod destination (-1: none) */
};
const std::vector<ParamDef> &params();
inline const ParamDef &pdef(int p) { return params()[(size_t)p]; }

/* normalised (0..1, the host's) and back: the same curve as the host's ranges */
float toNorm(int p, float v);
float fromNorm(int p, float n);
/* the value as the display shows it (bpm: the module's BPM sync on) */
void paramText(int p, float v, char *out, int n, bool bpm = false);

/* ------------------------------------------------- sources and destinations --- */
enum ModSrc {
    SRC_OFF, SRC_ENV1, SRC_LFO1 = SRC_ENV1 + 5, SRC_LFO1U = SRC_LFO1 + 5, SRC_MONOAT = SRC_LFO1U + 5, SRC_POLYAT,
    SRC_KEY, SRC_VELON, SRC_VELOFF, SRC_PITCHWHL, SRC_MODWHL, SRC_EXP, SRC_SUSPED, SRC_BREATH, SRC_VMOD, SRC_VMODU,
    SRC_CC0, NSRC = SRC_CC0 + 128
};
const char *srcName(int s);

/* destinations: a parameter's, or one of these */
enum SpecialDst { DS_PARAM, DS_PITCH1, DS_PITCH2, DS_PITCH3, DS_PITCHALL };
struct DstDef {
    char group[10];
    char name[16];
    int param;                 /* the parameter it moves (-1: special) */
    int special;
};
const std::vector<DstDef> &dests();             /* [0] is OFF */
int dstOfParam(int p);

/* ---------------------------------------------------------------- lists --- */
constexpr int NSYNC = 29;                       /* 1/64T .. 64' */
extern const char *const SYNC_NAME[NSYNC];
extern const float SYNC_BEATS[NSYNC];           /* in quarter notes */
constexpr int NARPDIV = 12;
extern const char *const ARPDIV_NAME[NARPDIV];
extern const float ARPDIV_BEATS[NARPDIV];
constexpr int NSCALE = 38;                      /* Chromatic and the 37 preset scales */
extern const char *const SCALE_NAME[NSCALE];
extern const uint16_t SCALE_MASK[NSCALE];
constexpr int NMACRONAME = 64;
extern const char *const MACRO_NAME[NMACRONAME];
extern const char *const BITS_NAME[13];
extern const int BITS_VAL[13];

/* ------------------------------------------------------------ the patch --- */
constexpr int NAME_LEN = 24;
struct Patch {
    char name[NAME_LEN];
    char category[12];
    float v[NPARAM];
    float step[5][NSTEP];      /* the step LFOs: +-64 each step */
};
void patchInit(Patch &p);      /* the Hydrasynth's INIT: a saw through the 12 dB ladder */
} // namespace hy
