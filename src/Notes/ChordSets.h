/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#ifndef TOUCHORD_CHORD_SETS_H
#define TOUCHORD_CHORD_SETS_H

#include <stdint.h>
#include "Defines.h"
#include "Types.h"

// Factory chord sets: seven chords each, relative to the key root, ordered
// from darkest to brightest. A key slot plays one when its scale is
// SCALE_CHORD_SET; tc_app.chord_set picks which.
#define CHORD_SET_COUNT   10
#define CHORD_SET_DEFAULT 8     // Bright 1

typedef struct
{
    uint8_t offsets[SCALE_LEN];     // semitones above the key root
    ChordDegree chords[SCALE_LEN];
} ChordSet;

extern const ChordSet chord_sets[CHORD_SET_COUNT];
extern const char* const chord_set_names[CHORD_SET_COUNT];

// Copies the selected set into the SCALE_CHORD_SET rows of the scale tables.
void load_chord_set(void);

// Name of a key slot's scale, or of the chord set when it plays one.
const char* key_scale_name(Scale key);

#endif
