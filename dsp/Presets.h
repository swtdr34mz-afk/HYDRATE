/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
/* HYDRATE's factory patches: its own sounds, made to show what the engine does (WaveScan, the Mutants, the filters,
 * the step LFOs, the arp, the effects). */
#pragma once
#include "Params.h"

namespace hy
{
int factoryCount();
Patch factory(int i);
#define NFACTORY factoryCount()
int findDest(const char *group, const char *name);   /* a destination by its group and name (0: none) */
} // namespace hy
