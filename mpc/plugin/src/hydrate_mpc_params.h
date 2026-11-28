/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
/* HYDRATE on the MPC: its parameters (shared by the plugin and the skin's renderer). Ids never change; new ones only
 * at the end:
 *   0             VOLUME
 *   1 .. NPARAM   the engine's parameters in dsp/Params.h's order (id: the engine's id)
 *   then          VOICES (8 / 16), QUALITY (ECO / HIGH), INIT (a press: the INIT patch)
 * An engine parameter's MPC kind: a choice (its names), on/off, a whole number (its own range), or continuous
 * (0..1, the VST3's curve: toNorm / fromNorm). */
#pragma once
#include "Params.h"
#include <cstring>
#include <string>

namespace hmpc
{
constexpr int kVolume = 0, kP = 1, kVoices = kP + hy::NPARAM, kQuality = kVoices + 1, kInit = kQuality + 1,
              kNumParams = kInit + 1;
inline int M(int p) { return kP + p; }            /* an engine parameter's MPC index */
inline int E(int i) { return i >= kP && i < kVoices ? i - kP : -1; }

enum Type { FLOAT, CHOICE, BOOL, INT };
inline Type typeOf(int p)
{
    const hy::ParamDef &d = hy::pdef(p);
    switch (d.kind) {
    case hy::K_ONOFF: return BOOL;
    case hy::K_CHOICE: case hy::K_WAVE: case hy::K_SRC: case hy::K_DST: case hy::K_SYNC: return CHOICE;
    default: break;
    }
    if (d.step >= 1.0f && d.hi - d.lo <= 128.0f)
        return INT;
    return FLOAT;
}
/* the MPC's plain value <> the engine's value */
inline float toEngine(int p, float plain)
{
    switch (typeOf(p)) {
    case CHOICE: return hy::pdef(p).lo + plain;
    case FLOAT: return hy::fromNorm(p, plain);
    default: return plain;
    }
}
inline float toPlain(int p, float v)
{
    switch (typeOf(p)) {
    case CHOICE: return v - hy::pdef(p).lo;
    case FLOAT: return hy::toNorm(p, v);
    default: return v;
    }
}
inline int steps(int p)                  /* a stepped parameter's number of values (0: continuous) */
{
    const hy::ParamDef &d = hy::pdef(p);
    return typeOf(p) == FLOAT ? 0 : typeOf(p) == BOOL ? 2 : (int)(d.hi - d.lo) + 1;
}
/* the MPC's name of an engine parameter: unique, 12 characters at most (the module abbreviated) */
inline std::string mpcName(int p)
{
  std::string s=hy::pdef(p).name;
  const char* R[][2]={{"Destination","Dst"},{"Source","Src"},{"Oscillator","Osc"},{"Osc ","O"},{"Mutant ","Mu"},{"Filter ","F"},{"Env ","E"},{"LFO ","L"},{"Mixer ","Mx "},{"Macro ","Mc"},{"Pre-FX ","PreFX "},{"Post-FX ","PostFX "},{"Reverb ","Rv "},{"Delay ","Dl "},{"Voice ","Vc "},{"Ring Mod ","Ring "},{"Resonance","Res"},{"Keytrack","KeyTrk"},{"Velocity","Vel"},{"Amount","Amt"},{"Feedback","Fdbk"},{"Depth","Dpth"},{"Param ","P"},{"Button","Btn"},{"Attack","Atk"},{"Release","Rel"},{"Sustain","Sus"},{"Decay","Dec"},{"Curve","Crv"},{"Trigger","Trig"},{"Window","Win"},{"Dry/Wet","Wet"},{"Trig Src ","TrSrc"},{"Dest ","D"},{" Btn Value"," BV"},{"Unison ","Uni "},{"Pitch Bend","PB"},{"Vibrato ","Vib "},{"Octave","Oct"},{"Offset","Ofs"},{"Clock","Clk"},{"Drive Route","DrvRt"},{"Vowel Order","VowOrd"},{"PostFX ","PstFX "},{"Density","Dens"},{"Detune","Detn"}};
  for(auto&r:R){ size_t i; size_t from=0; while((i=s.find(r[0],from))!=std::string::npos){ s.replace(i,strlen(r[0]),r[1]); from=i+strlen(r[1]); } }
  if(s.size()>12){ std::string t; for(char c: s) if(c!=' ') t+=c; s=t; }
  if(s.size()>12) s=s.substr(0,12);
  return s;
}
} // namespace hmpc
