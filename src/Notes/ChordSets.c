/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 *
 * Modified for TCw (https://github.com/nlpeeee/TCw); changes are listed in
 * CHANGELOG.md and the git history.
 */

#include "ChordSets.h"
#include "Note.h"
#include "Globals.h"
#include <string.h>

#define M  CHORD_MAJOR
#define m  CHORD_MINOR
#define D7 CHORD_DOMINANT
#define HD CHORD_DIM
#define S4 CHORD_SUS4

// Shown in C. Size decides how much of each chord plays: M is a triad,
// maj7 then maj9; m is m, m7, m9; D7 is a triad, 7, 9; HD is m7b5.
const ChordSet chord_sets[CHORD_SET_COUNT] =
{
    // Dark 1: Cm Ab Db Fm Gm Eb Bb (trap and drill minor)
    {{0, 8, 1, 5, 7, 3, 10}, {m, M, M, m, m, M, M}},
    // Dark 2: Cm Ab Bb Eb Fm Gm G (minor pop and EDM)
    {{0, 8, 10, 3, 5, 7, 7}, {m, M, M, M, m, m, D7}},
    // Dark 3: Cm Fm Ab Gm Eb Bb7 Dm7b5 (moody R&B and synthwave)
    {{0, 5, 8, 7, 3, 10, 2}, {m, m, M, m, M, D7, HD}},
    // Dark 4: Cm F7 Bb Gm Eb Dm Am7b5 (dorian funk and disco house)
    {{0, 5, 10, 7, 3, 2, 9}, {m, D7, M, m, M, m, HD}},
    // Warm 1: Cm Ab G7 Bbm Eb7 Fm Db ("Just the Two of Us" loop)
    {{0, 8, 7, 10, 3, 5, 1}, {m, M, D7, m, D7, m, M}},
    // Warm 2: C F G7 Em Am Dm E7 (the J-pop "royal road")
    {{0, 5, 7, 4, 9, 2, 4}, {M, M, D7, m, m, m, D7}},
    // Warm 3: C F Em Dm Am G Bb (neo-soul)
    {{0, 5, 4, 2, 9, 7, 10}, {M, M, m, m, m, D7, M}},
    // Warm 4: C F Fm Am G Em Ab (bittersweet pop, borrowed minor IV)
    {{0, 5, 5, 9, 7, 4, 8}, {M, M, m, m, D7, m, M}},
    // Bright 1: C F G Am Dm Em Gsus4 (four-chord pop)
    {{0, 5, 7, 9, 2, 4, 7}, {M, M, D7, m, m, m, S4}},
    // Bright 2: C Bb F G Am Dm Ab (pop-rock anthems)
    {{0, 10, 5, 7, 9, 2, 8}, {M, M, M, D7, m, m, M}},
};

#undef M
#undef m
#undef D7
#undef HD
#undef S4

const char* const chord_set_names[CHORD_SET_COUNT] = {
    "Dark 1", "Dark 2", "Dark 3", "Dark 4",
    "Warm 1", "Warm 2", "Warm 3", "Warm 4",
    "Bright 1", "Bright 2",
};

const char* const chord_set_loops[CHORD_SET_COUNT] = {
    "1 3 1 2", "1 2 4 3", "1 2 3 4", "1 2 1 2",
    "2 3 1 4 5", "2 3 4 5", "2 3 4 1", "1 2 3 1",
    "2 3 4 1", "1 2 3 1",
};

static uint8_t current_set(void)
{
    return tc_app.chord_set < CHORD_SET_COUNT ? tc_app.chord_set : CHORD_SET_DEFAULT;
}

void load_chord_set(void)
{
    const ChordSet* set = &chord_sets[current_set()];
    memcpy(scale_intervals[SCALE_CHORD_SET], set->offsets, sizeof set->offsets);
    memcpy(scale_chords[SCALE_CHORD_SET], set->chords, sizeof set->chords);
}

const char* key_scale_name(Scale key)
{
    if (key.quality == SCALE_CHORD_SET) return chord_set_names[current_set()];
    return key.quality < SCALE_COUNT ? scale_names[key.quality] : "?";
}
