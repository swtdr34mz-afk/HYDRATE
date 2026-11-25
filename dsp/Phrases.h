/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
/* The arpeggiator's 64 phrases (Mode = Phrase): HYDRATE's own little patterns, each a run of up to 32 steps. A step
 * is a degree of the held chord (0 the lowest note, 1 the next ...; past the chord's size it climbs an octave,
 * below 0 it goes down), a rest or a tie. Each phrase repeats in each octave of the arp's range. */
#pragma once
#include <cstdint>

namespace hy
{
constexpr int8_t PH_ZERO = 0, PH_REST = 100, PH_TIE = 101;
struct Phrase {
    int len;
    int8_t step[32];
};
const Phrase &phrase(int i);         /* 0..63 */
} // namespace hy
