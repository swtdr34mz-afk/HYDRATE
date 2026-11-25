/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HYDRATE) */
#include "Phrases.h"

namespace hy
{
static Phrase P[64];
static bool made = false;

/* sixteen motifs; each phrase is a motif, varied (inverted, displaced, with rests and ties, stretched to 32) */
static const char *const MOTIF[16] = {
    "0123", "0212", "0313", "0120", "0242", "1020", "0r1r", "02t1", "0-102", "032-1", "0123321", "00r2", "01t2t3",
    "0r02r1", "2101", "0-1-20"};

static void build()
{
    for (int i = 0; i < 64; i++) {
        Phrase &p = P[i];
        const char *m = MOTIF[i % 16];
        int var = i / 16;                       /* 0 plain, 1 inverted, 2 displaced up, 3 long */
        int8_t base[16];
        int n = 0;
        for (const char *c = m; *c && n < 16; c++) {
            if (*c == 'r')
                base[n++] = PH_REST;
            else if (*c == 't')
                base[n++] = PH_TIE;
            else if (*c == '-') {
                c++;
                base[n++] = (int8_t)(-(*c - '0'));
            } else
                base[n++] = (int8_t)(*c - '0');
        }
        int len = var == 3 ? 32 : 16;
        for (int s = 0; s < len; s++) {
            int8_t x = base[s % n];
            int rep = s / n;
            if (x != PH_REST && x != PH_TIE) {
                if (var == 1)
                    x = (int8_t)(3 - x);
                else if (var == 2)
                    x = (int8_t)(x + (rep & 1));
                else if (var == 3)
                    x = (int8_t)(x + (rep % 4 == 3 ? 4 : rep % 2));
            }
            /* a breath at the end of each half */
            if (len == 16 && s == 15 && (i & 1))
                x = PH_REST;
            p.step[s] = x;
        }
        p.len = len;
    }
    made = true;
}

const Phrase &phrase(int i)
{
    if (!made)
        build();
    return P[i < 0 ? 0 : i > 63 ? 63 : i];
}
} // namespace hy
