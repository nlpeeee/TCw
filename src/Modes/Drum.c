/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#include "Drum.h"
#include "Globals.h"
#include "Timing/Quantize.h"
#include "Sync.h"
#include "IO/Midi.h"
#include "Rendering/Graphics.h"

#define DRUM_NO_NOTE 0xFF

uint8_t drum_velocity = DEFAULT_VELOCITY;

// Note sounding on each pad, so its note-off matches even if the octave
// changed while it was held. DRUM_NO_NOTE when the pad is up.
static uint8_t pad_note[NUM_KEYS] = {
    DRUM_NO_NOTE, DRUM_NO_NOTE, DRUM_NO_NOTE, DRUM_NO_NOTE,
    DRUM_NO_NOTE, DRUM_NO_NOTE, DRUM_NO_NOTE,
};
_Static_assert(NUM_KEYS == 7, "pad_note initialiser");

const char* const drum_layout_names[DRUM_LAYOUT_COUNT] = { "Kit", "Notes" };

// General MIDI drums on the keys, left to right.
static const uint8_t kit_notes[NUM_KEYS] = { 36, 38, 39, 42, 46, 45, 49 };
static const char* const kit_labels[NUM_KEYS] = { "Kik", "Snr", "Clp", "Hat", "Opn", "Tom", "Cym" };

static bool kit(void) { return tc_app.drum_layout != DRUM_NOTES; }

static const ModeHandlers drum_handlers = TC_MODE_HANDLERS(drum);

void drum_start()
{
    tc_bind_handlers(&drum_handlers);
    drum_velocity = tc_app.velocity;
    tc_trill_segs = 0;
    tc_trill_show = true;
}

void drum_end()
{
    for (int i = 0; i < NUM_KEYS; i++) {
        if (pad_note[i] == DRUM_NO_NOTE) continue;
        send_midi_note(tc_app.channel, NOTE_OFF, pad_note[i], 0);
        pad_note[i] = DRUM_NO_NOTE;
    }
}

void drum_draw()
{
    draw_mode_label("Drum");
    quant_draw_marker();
    if (!kit()) draw_octave_dots(128 - 32, 2, tc_app.octave, 1, 8);

    const int pad_w = 12, pad_h = 14, gap = 3;
    const int total_w = NUM_KEYS * pad_w + (NUM_KEYS - 1) * gap;
    const int start_x = (128 - total_w) / 2;
    const int pad_y   = 14;
    for (int i = 0; i < NUM_KEYS; i++) {
        int px = start_x + i * (pad_w + gap);
        if (!tc_key_states[i]) ssd1306_draw_square(&tc_disp, px, pad_y, pad_w, pad_h);
        else                   ssd1306_draw_empty_square(&tc_disp, px, pad_y, pad_w, pad_h);
        if (kit())
            ssd1306_draw_string_with_font(&tc_disp, px, pad_y + pad_h + 2, 1, font_3x6, kit_labels[i]);
    }

    draw_string_int_centered("Velocity: ", drum_velocity, 40);
}

void drum_update() {}

void drum_key_down(uint8_t key)
{
    uint8_t note = kit() ? kit_notes[key] : 12 * (tc_app.octave + 1) + key;
    send_midi_note(tc_app.channel, NOTE_ON, note, drum_velocity);
    pad_note[key] = note;
}

void drum_key_up(uint8_t key) {}

void drum_key_up_independent(uint8_t key)
{
    if (pad_note[key] == DRUM_NO_NOTE) return;
    send_midi_note(tc_app.channel, NOTE_OFF, pad_note[key], 0);
    pad_note[key] = DRUM_NO_NOTE;
}

void drum_button_down(uint8_t button)
{
    switch (button) {
        case 0: tc_app_set_mode(TOUCHORD_COMPOSE); break;   // hold B0 for Settings
        // Octave only moves the notes layout; the kit's drums stay put.
        case 1: if (!kit() && tc_app.octave > 0) tc_app.octave--; break;
        case 2: if (!kit() && tc_app.octave < 7) tc_app.octave++; break;
    }
}

void drum_button_up(uint8_t button) {}

void drum_trill_down(float pos, float size)
{
    drum_velocity = 127 - (uint8_t)(pos * 127);
}

void drum_trill_up() {}
