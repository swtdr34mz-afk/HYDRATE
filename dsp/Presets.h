/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
/* HYDRATE's factory patches: its own sounds, made to show what the engine does (WaveScan, the Mutants, the filters,
 * the step LFOs, the arp, the effects). */
#pragma once
#include <cstdint>
#include "Params.h"

namespace hy
{
int factoryCount();
Patch factory(int i);
/* RANDOM: a new patch from the seed - musical ranges, checked to be audible and levelled like the factory sounds */
Patch randomPatch(uint32_t seed);
#define NFACTORY factoryCount()
int findDest(const char *group, const char *name);   /* a destination by its group and name (0: none) */
} // namespace hy
