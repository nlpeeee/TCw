/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#include "Globals.h"
#include "Notes/ChordSets.h"

const TouchordSettings tc_app_default = {
    .magic           = SETTINGS_MAGIC,
    .schema_version  = SETTINGS_SCHEMA_VERSION,
    .struct_size     = sizeof(TouchordSettings),

    .key             = { {NOTE_C, SCALE_CHORD_SET},
                         {NOTE_E, SCALE_MINOR},
                         {NOTE_B_FLAT, SCALE_CUSTOM0} },
    .current_key     = 0,
    .octave          = DEFAULT_OCTAVE,
    .extension_count = DEFAULT_EXTENSIONS,
    .inversion       = DEFAULT_INVERSION,
    .velocity        = DEFAULT_VELOCITY,

    .mode            = TOUCHORD_COMPOSE,
    .octave_count    = DEFAULT_OCTAVE_COUNT,
    .cutoff          = 0,

    .chord           = {0, 0, 0, 0, 0, 0},
    .chord_name      = {'\0'},
    .degree          = CHORD_DEFAULT,

    .prev_extension  = DEFAULT_EXTENSIONS,
    .channel         = MIDI_CHANNEL,
    .midi_type       = MIDI_TRS_A,

    .custom_scale_chords = {
        {CHORD_MAJOR,    CHORD_MAJOR,    CHORD_MAJOR,    CHORD_MAJOR,    CHORD_MAJOR,    CHORD_MAJOR,    CHORD_MAJOR},
        {CHORD_MINOR,    CHORD_MINOR,    CHORD_MINOR,    CHORD_MINOR,    CHORD_MINOR,    CHORD_MINOR,    CHORD_MINOR},
        {CHORD_DOMINANT, CHORD_DOMINANT, CHORD_DOMINANT, CHORD_DOMINANT, CHORD_DOMINANT, CHORD_DOMINANT, CHORD_DOMINANT},
        {CHORD_DIM,      CHORD_DIM,      CHORD_DIM,      CHORD_DIM,      CHORD_DIM,      CHORD_DIM,      CHORD_DIM},
    },
    .custom_scale_intervals = {
        {0, 1, 2, 3, 4, 5, 6},
        {0, 1, 2, 3, 4, 5, 6},
        {0, 1, 2, 3, 4, 5, 6},
        {0, 1, 2, 3, 4, 5, 6},
    },

    .compose_type       = COMPOSE_DEGREE,
    .compose_sustain    = false,
    .compose_voice_lead = false,

    .perform_pos_cc              = MIDI_CUTOFF,
    .perform_size_cc             = MIDI_MOD,
    .perform_pos_default         = DEFAULT_CUTOFF,
    .perform_size_default        = 0,
    .perform_reset_pos_on_lift   = false,
    .perform_reset_size_on_lift  = true,

    .bluetooth                   = true,

    .clock_in                    = true,
    .compose_roll                = 0,

    .arp_pattern                 = 0,   // Up
    .arp_rate                    = 3,   // 1/16
    .arp_octaves                 = 0,   // 1 octave
    .arp_gate                    = 1,   // 50%
    .arp_bpm                     = 120,
    .arp_latch                   = true,

    .bass_channel                = 0,

    .perform_pos_type            = PERFORM_CC,
    .perform_size_type           = PERFORM_CC,

    .chord_set                   = CHORD_SET_DEFAULT,
};

TouchordSettings tc_app;
TouchordSettings tc_app_working;

void (*tc_draw)() = NULL;
void (*tc_update)() = NULL;
void (*tc_key_down)(uint8_t) = NULL;
void (*tc_key_up)(uint8_t) = NULL;
void (*tc_key_up_independent)(uint8_t) = NULL;
void (*tc_button_down)(uint8_t) = NULL;
void (*tc_button_up)(uint8_t) = NULL;
void (*tc_trill_down)(float, float) = NULL;
void (*tc_trill_up)() = NULL;

void tc_bind_handlers(const ModeHandlers *h)
{
    tc_draw               = h->draw;
    tc_update             = h->update;
    tc_key_down           = h->key_down;
    tc_key_up             = h->key_up;
    tc_key_up_independent = h->key_up_independent;
    tc_button_down        = h->button_down;
    tc_button_up          = h->button_up;
    tc_trill_down         = h->trill_down;
    tc_trill_up           = h->trill_up;
}
TouchordMode tc_prev_mode = TOUCHORD_COMPOSE;

bool tc_running = true;
volatile bool tc_trigger_bootsel = false;
volatile bool tc_connected = false;

TrillBar tc_bar;
ssd1306_t tc_disp;

bool tc_key_states[NUM_KEYS];
bool tc_control_states[NUM_CONTROLS];
bool tc_touch_state = false;
volatile bool tc_button_slid[NUM_CONTROLS];

uint8_t tc_last_key = 0;

float   tc_trill_pos = 0.0f;
float   tc_trill_strength = 0.0f;
uint8_t tc_trill_segs = 0;
bool    tc_trill_show = true;

char     tc_overlay_msg[24]   = {'\0'};
uint32_t tc_overlay_start_ms  = 0;
uint32_t tc_overlay_dur_ms    = 0;
void use_single_key_slot(TouchordSettings* s)
{
    if (s->current_key < 3) s->key[0] = s->key[s->current_key];
    s->current_key = 0;
}
