/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#include "Note.h"
#include "ChordSets.h"
#include <string.h>
#include <stdlib.h>

const char* root_names[17] = {
    "C", "C#", "Db", "D", "D#", "Eb", "E", "F", "F#",
    "Gb", "G", "G#", "Ab", "A", "A#", "Bb", "B",
};

const char* sharp_names[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};
const char* flat_names[12] = {
    "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
};

uint8_t scale_intervals[SCALE_COUNT][SCALE_LEN] =
{
    { 0, 0, 0, 0, 0, 0, 0 }, // Null
    { 0, 2, 4, 5, 7, 9, 11 }, // Maj
    { 0, 2, 3, 5, 7, 8, 10 }, // Min
    { 0, 2, 3, 5, 7, 9, 10 }, // Dorian
    { 0, 1, 3, 5, 7, 8, 10 }, // Phrygian
    { 0, 2, 4, 6, 7, 9, 11 }, // Lydian
    { 0, 2, 4, 5, 7, 9, 10 }, // Mixolydian
    { 0, 1, 3, 5, 6, 8, 10 }, // Locrian
    { 0, 1, 2, 3, 4, 5, 6 }, // C0
    { 0, 1, 2, 3, 4, 5, 6 }, // C1
    { 0, 1, 2, 3, 4, 5, 6 }, // C2
    { 0, 1, 2, 3, 4, 5, 6 }, // C3
    { 0, 2, 3, 5, 7, 8, 11 }, // Harmonic minor
    { 0, 2, 3, 5, 7, 9, 11 }, // Melodic minor
    { 0, 2, 4, 5, 7, 8, 11 }, // Harmonic major
    { 0, 1, 4, 5, 7, 8, 10 }, // Phrygian dominant
    { 0, 2, 4, 6, 7, 9, 10 }, // Lydian dominant
    { 0, 2, 4, 5, 7, 8, 10 }, // Mixolydian b6
    { 0, 2, 4, 7, 9, 12, 14 }, // Major pentatonic, last two keys go up an octave
    { 0, 3, 5, 7, 10, 12, 15 }, // Minor pentatonic, last two keys go up an octave
    { 0, 5, 7, 9, 2, 4, 7 }, // Chord set, filled in by load_chord_set()
};

ChordDegree scale_chords[SCALE_COUNT][SCALE_LEN] =
{
    {CHORD_MAJOR, CHORD_MAJOR, CHORD_MAJOR, CHORD_MAJOR, CHORD_MAJOR, CHORD_MAJOR, CHORD_MAJOR},
    {CHORD_MAJOR, CHORD_MINOR, CHORD_MINOR, CHORD_MAJOR, CHORD_DOMINANT, CHORD_MINOR, CHORD_DIM}, // Maj
    {CHORD_MINOR, CHORD_DIM, CHORD_MAJOR, CHORD_MINOR, CHORD_MINOR, CHORD_MAJOR, CHORD_MAJOR}, // Min
    {CHORD_MINOR, CHORD_MINOR, CHORD_MAJOR, CHORD_DOMINANT, CHORD_MINOR, CHORD_DIM, CHORD_MAJOR}, // Dorian
    {CHORD_MINOR, CHORD_MAJOR, CHORD_DOMINANT, CHORD_MINOR, CHORD_DIM, CHORD_MAJOR, CHORD_MINOR}, // Phrygian
    {CHORD_MAJOR, CHORD_DOMINANT, CHORD_MINOR, CHORD_DIM, CHORD_MAJOR, CHORD_MINOR, CHORD_MINOR}, // Lydian
    {CHORD_DOMINANT, CHORD_MINOR, CHORD_DIM, CHORD_MAJOR, CHORD_MINOR, CHORD_MINOR, CHORD_MAJOR}, // Mixolydian
    {CHORD_DIM, CHORD_MAJOR, CHORD_MINOR, CHORD_MINOR, CHORD_MAJOR, CHORD_DOMINANT, CHORD_MINOR}, // Locrian
    {CHORD_MAJOR, CHORD_MAJOR, CHORD_MAJOR, CHORD_MAJOR, CHORD_MAJOR, CHORD_MAJOR, CHORD_MAJOR}, // Custom0
    {CHORD_MINOR, CHORD_MINOR, CHORD_MINOR, CHORD_MINOR, CHORD_MINOR, CHORD_MINOR, CHORD_MINOR},
    {CHORD_DOMINANT, CHORD_DOMINANT, CHORD_DOMINANT, CHORD_DOMINANT, CHORD_DOMINANT, CHORD_DOMINANT, CHORD_DOMINANT},
    {CHORD_DIM, CHORD_DIM, CHORD_DIM, CHORD_DIM, CHORD_DIM, CHORD_DIM, CHORD_DIM}, // Custom3
    {CHORD_MINOR, CHORD_DIM, CHORD_AUG, CHORD_MINOR, CHORD_DOMINANT, CHORD_MAJOR, CHORD_DIM}, // Harmonic minor
    {CHORD_MINOR, CHORD_MINOR, CHORD_AUG, CHORD_DOMINANT, CHORD_DOMINANT, CHORD_DIM, CHORD_DIM}, // Melodic minor
    {CHORD_MAJOR, CHORD_DIM, CHORD_MINOR, CHORD_MINOR, CHORD_DOMINANT, CHORD_AUG, CHORD_DIM}, // Harmonic major
    {CHORD_DOMINANT, CHORD_MAJOR, CHORD_DIM, CHORD_MINOR, CHORD_DIM, CHORD_AUG, CHORD_MINOR}, // Phrygian dominant
    {CHORD_DOMINANT, CHORD_DOMINANT, CHORD_DIM, CHORD_DIM, CHORD_MINOR, CHORD_MINOR, CHORD_AUG}, // Lydian dominant
    {CHORD_DOMINANT, CHORD_DIM, CHORD_DIM, CHORD_MINOR, CHORD_MINOR, CHORD_MAJOR, CHORD_DOMINANT}, // Mixolydian b6
    {CHORD_MAJOR, CHORD_MINOR, CHORD_MINOR, CHORD_DOMINANT, CHORD_MINOR, CHORD_MAJOR, CHORD_MINOR}, // Major pentatonic
    {CHORD_MINOR, CHORD_MAJOR, CHORD_MINOR, CHORD_MINOR, CHORD_MAJOR, CHORD_MINOR, CHORD_MAJOR}, // Minor pentatonic
    {CHORD_MAJOR, CHORD_MAJOR, CHORD_DOMINANT, CHORD_MINOR, CHORD_MINOR, CHORD_MINOR, CHORD_SUS4}, // Chord set
};

const char* scale_names[SCALE_COUNT] = {
    "NULL", "Major", "Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian", "Custom 0", "Custom 1", "Custom 2", "Custom 3",
    "Harm Minor", "Mel Minor", "Harm Major", "Phryg Dom", "Lydian Dom", "Mixo b6", "Major Pent", "Minor Pent", "Chord Set"
};

const uint8_t chord_intervals[8][6] =
{
    {0, 0, 0, 0,  0,  0},
    {0, 4, 7, 11, 14, 18}, // Major
    {0, 3, 7, 10, 14, 17}, // Minor
    {0, 4, 7, 10, 14, 17}, // Dominant
    {0, 3, 6, 10, 14, 17}, // Dim
    {0, 4, 8, 0, 0, 0}, // Aug
    {0, 2, 7, 0, 0, 0}, // Sus2
    {0, 5, 7, 0, 0, 0}  // Sus4
};

const char* interval_name_from_semitones(int semitones) {
    switch (semitones % 12) 
    {
        case 1:  return "b9";
        case 2:  return "9";
        case 3:  return "#9";
        case 5:  return "11";
        case 6:  return "#11";
        case 8:  return "b13";
        case 9:  return "13";
        case 10: return "b7";
        case 11: return "7";
        case 0:  return "R";    // Root
        case 4:  return "3";
        case 7:  return "5";
        default: return "?";
    }
}

const char* deg_name(ChordDegree deg) {
    switch (deg)
    {
        case CHORD_DEFAULT: return "";
        case CHORD_MAJOR: return "";
        case CHORD_MINOR: return "m";
        case CHORD_DOMINANT: return "dom";
        case CHORD_DIM: return "dim";
        case CHORD_AUG: return "aug";
        case CHORD_SUS2: return "sus2";
        case CHORD_SUS4: return "sus4";
        case CHORD_PARALLEL: return "";
    }
    return "";
}

uint8_t note_name_to_midi(const char* note, int octave, bool* flat) {
    static const char* names[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
        "Cb", "Db", "Eb", "Fb", "Gb", "Ab", "Bb", "E#", "B#"
    };
    static const int values[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
        11, 1, 3, 4, 6, 8, 10, 5, 0
    };
    for (int i = 0; i < 21; ++i) {
        if (strcmp(note, names[i]) == 0) {
            *flat = i > 11;
            return values[i] + 12 * (octave + 1);
        }
    }
    return -1;
}

uint8_t note_to_midi(Note note, int octave, bool* flat) {
    static const int values[] = { 0, 1, 1, 2, 3, 3, 4, 5, 6, 6, 7, 8, 8, 9, 10, 10, 11 };
    static const bool flats[] = { 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0 };

    *flat = flats[note];
    return values[note] + 12 * (octave + 1);
}

// The key's root as a MIDI note, kept between F# below the octave's C and
// the F above it, so changing key moves the chords a little, not most of
// an octave.
static uint8_t key_root_midi(Note note, int octave, bool* flat)
{
    uint8_t root = note_to_midi(note, octave, flat);
    if (root % 12 >= 6 && root >= 12) root -= 12;
    return root;
}

const char* get_note_name(int midi, bool flat) {
    return flat ? flat_names[midi % 12] : sharp_names[midi % 12];
}

void reload_custom_scales()
{
    size_t size_chords = sizeof(tc_app.custom_scale_chords[0]) * 4;
    size_t size_intervals = sizeof(tc_app.custom_scale_intervals[0]) * 4;
    memcpy(scale_chords[SCALE_CUSTOM0], tc_app.custom_scale_chords[0], size_chords);
    memcpy(scale_intervals[SCALE_CUSTOM0], tc_app.custom_scale_intervals[0], size_intervals);
    load_chord_set();
}

// Drop notes above the MIDI range by octaves. Data bytes >127 would be
// read as status bytes by every receiver.
static void fold_into_midi_range(uint8_t *notes, int len)
{
    for (int i = 0; i < len; i++)
        while (notes[i] > 127) notes[i] -= 12;
}

void sort_chord_notes(uint8_t *arr, int len)
{
    for (int n = len; n > 1; --n)
        for (int i = 0; i < n - 1; ++i)
            if (arr[i] > arr[i + 1]) {
                uint8_t t = arr[i]; arr[i] = arr[i + 1]; arr[i + 1] = t;
            }
}


// Parallel key for the added scales: minor-flavoured scales borrow from
// major, the rest borrow from minor.
static ScaleType parallel_scale(ScaleType quality)
{
    for (int i = 1; i < 3; i++)
        if (scale_intervals[quality][i] == 3) return SCALE_MAJOR;
    return SCALE_MINOR;
}

// Chord symbol suffix for the chord_intervals[] stacks, by note count (3 to 6).
static const char* chord_suffix(ChordDegree type, int notes)
{
    static const char* const stacks[][4] = {
        [CHORD_MAJOR]    = {"",    "maj7", "maj9", "maj9#11"},
        [CHORD_MINOR]    = {"m",   "m7",   "m9",   "m11"},
        [CHORD_DOMINANT] = {"",    "7",    "9",    "11"},
        [CHORD_DIM]      = {"dim", "m7b5", "m9b5", "m11b5"},
    };
    switch (type) {
        case CHORD_AUG:  return "aug";
        case CHORD_SUS2: return "sus2";
        case CHORD_SUS4: return "sus4";
        case CHORD_MAJOR: case CHORD_MINOR: case CHORD_DOMINANT: case CHORD_DIM:
            if (notes < 3) notes = 3;
            if (notes > 6) notes = 6;
            return stacks[type][notes - 3];
        default: return "";
    }
}

void build_chord(
    Scale key, int octave, int degree, ChordDegree chord_type, int extensions, int inversion,
    uint8_t* midi_out, char* chord_name) 
{
    for(int i = 0; i < MAX_CHORD; i++)
    {
        midi_out[i] = 0;
    }
    bool is_flat;
    uint8_t root = key_root_midi(key.root, octave, &is_flat);
    if(key.quality == SCALE_CHORD_SET)
    {
        // Settings can change the set without a reload, so read it fresh.
        load_chord_set();
        uint8_t offset = scale_intervals[SCALE_CHORD_SET][degree];
        root += offset;
        // Chords on the flat degrees (bII, bIII, bVI, bVII) never need a sharp.
        if (offset == 1 || offset == 3 || offset == 8 || offset == 10) is_flat = true;
    }
    else if(key.quality == SCALE_MAJOR)
    {
        if(chord_type != CHORD_PARALLEL)
            root += scale_intervals[SCALE_MAJOR][degree];
        else root += scale_intervals[SCALE_MINOR][degree];
    }
    else if(key.quality == SCALE_MINOR)
    {
        if(chord_type != CHORD_PARALLEL)
        {
            root += scale_intervals[SCALE_MINOR][degree];
            if(key.root == NOTE_C)
                is_flat = true;
        }
        else 
            root += scale_intervals[SCALE_MAJOR][degree];
    }
    else if(key.quality >= SCALE_HARMONIC_MINOR && chord_type == CHORD_PARALLEL)
        root += scale_intervals[parallel_scale(key.quality)][degree];
    else root += scale_intervals[key.quality][degree];

    // Like C minor above, the added scales with flat degrees read better spelled with flats in C.
    if(key.quality >= SCALE_HARMONIC_MINOR && key.quality != SCALE_CHORD_SET && key.root == NOTE_C)
    {
        for (int i = 0; i < SCALE_LEN; i++)
        {
            uint8_t iv = scale_intervals[key.quality][i] % 12;
            if (iv == 1 || iv == 3 || iv == 8 || iv == 10) is_flat = true;
        }
    }

    if((chord_type == CHORD_AUG || chord_type == CHORD_SUS2 || chord_type == CHORD_SUS4))
    {
        extensions = 3;
    }

    if (key.quality == SCALE_CHORD_SET)
    {
        // A chord set has no parallel key, so parallel swaps major and minor on the same root.
        ChordDegree set_type = scale_chords[SCALE_CHORD_SET][degree];
        if (chord_type == CHORD_PARALLEL)
        {
            if (set_type == CHORD_MAJOR || set_type == CHORD_DOMINANT) set_type = CHORD_MINOR;
            else if (set_type == CHORD_MINOR) set_type = CHORD_MAJOR;
        }
        chord_type = set_type;
    }
    else if (chord_type == CHORD_PARALLEL)
    {
        if(key.quality == SCALE_MAJOR)
        {
            chord_type = scale_chords[SCALE_MINOR][degree];
        }
        else if(key.quality >= SCALE_HARMONIC_MINOR)
        {
            chord_type = scale_chords[parallel_scale(key.quality)][degree];
        }
        else
        {
            chord_type = scale_chords[key.quality-1][degree];
        }
    }
    else
    {
        chord_type = scale_chords[key.quality][degree];
    }

    // Augmented and sus chords only have three notes, whichever table they came from.
    if (chord_type == CHORD_AUG || chord_type == CHORD_SUS2 || chord_type == CHORD_SUS4)
    {
        if (extensions > 3) extensions = 3;
    }

    for (int i = 0; i < extensions; i++)
    {
        midi_out[i] = root + chord_intervals[chord_type][i];
    }

    // Inversion
    for (int i = 0; i < inversion && i < extensions; ++i)
        midi_out[i] += 12;

    fold_into_midi_range(midi_out, extensions);
    sort_chord_notes(midi_out, extensions);

    snprintf(chord_name, CHORD_NAME_LEN, "%s%s", get_note_name(root, is_flat),
             chord_suffix(chord_type, extensions));
}

void build_jazz_chord(Scale key, int octave, int degree, int sub_seg,
                      int extensions, uint8_t *out, char *name)
{
    bool is_flat;
    int natural_root = key_root_midi(key.root, octave, &is_flat);
    if (key.quality == SCALE_CHORD_SET) load_chord_set();
    if      (key.quality == SCALE_MAJOR) natural_root += scale_intervals[SCALE_MAJOR][degree];
    else if (key.quality == SCALE_MINOR) natural_root += scale_intervals[SCALE_MINOR][degree];
    else                                 natural_root += scale_intervals[key.quality][degree];
    ChordDegree natural_type = scale_chords[key.quality][degree];

    int sub_root = natural_root;
    ChordDegree sub_type = natural_type;
    switch (sub_seg) {
        case 0: sub_root = natural_root + 6; sub_type = CHORD_DOMINANT; break;
        case 1: sub_root = natural_root + 7; sub_type = CHORD_DOMINANT; break;
        case 2:
            if      (natural_type == CHORD_MAJOR) sub_type = CHORD_MINOR;
            else if (natural_type == CHORD_MINOR) sub_type = CHORD_MAJOR;
            break;
        case 3: sub_root = natural_root - 1; sub_type = CHORD_DIM; break;
    }
    while (sub_root < 0)   sub_root += 12;
    while (sub_root > 127) sub_root -= 12;

    if (extensions > 6) extensions = 6;
    for (int i = 0; i < MAX_CHORD; i++) out[i] = 0;
    for (int i = 0; i < extensions; i++)
        out[i] = (uint8_t)(sub_root + chord_intervals[sub_type][i]);

    fold_into_midi_range(out, extensions);
    sort_chord_notes(out, extensions);

    snprintf(name, CHORD_NAME_LEN, "%s", get_note_name(sub_root, is_flat));
}

void apply_voice_leading(uint8_t *chord, int extensions, const uint8_t *prev)
{
    bool prev_has = false;
    for (int i = 0; i < MAX_CHORD; i++) if (prev[i]) { prev_has = true; break; }
    if (!prev_has) return;

    for (int i = 0; i < extensions; i++) {
        int pc = chord[i] % 12;
        int best_octave = chord[i] / 12;
        int best_dist = 0x7fff;
        for (int oct = 0; oct < 11; oct++) {
            int candidate = pc + oct * 12;
            if (candidate < 0 || candidate > 127) continue;
            int min_to_prev = 0x7fff;
            for (int j = 0; j < MAX_CHORD; j++) {
                if (prev[j] == 0) continue;
                int d = abs(candidate - (int)prev[j]);
                if (d < min_to_prev) min_to_prev = d;
            }
            if (min_to_prev < best_dist) {
                best_dist = min_to_prev;
                best_octave = oct;
            }
        }
        chord[i] = (uint8_t)(pc + best_octave * 12);
    }

    sort_chord_notes(chord, extensions);
}