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

#ifndef TOUCHORD_MIDI_H
#define TOUCHORD_MIDI_H

#include <stdio.h>
#include "pico/stdlib.h"
#include "Defines.h"
#include "Types.h"

// Outgoing MIDI is queued from either core and sent from core 0.
void midi_out_init(void);
void midi_out_task(void);

// Core 0: read incoming USB MIDI. Only clock and transport are used so far.
void midi_in_task(void);

void switch_midi_trs(MidiType type);
void setup_midi_trs(MidiType type);
void send_trs_midi(uint8_t* data, uint8_t len);

void send_midi_note(uint8_t channel, uint8_t status, uint8_t note, uint8_t velocity);

void send_poly_aftertouch(uint8_t channel, uint8_t note, uint8_t pressure);
void send_aftertouch(uint8_t channel, uint8_t pressure);

void send_midi_cc(uint8_t channel, uint8_t cc_num, uint8_t cc_value);

// value is 0..16383, 8192 = centre
void send_pitch_bend(uint8_t channel, uint16_t value);

// Bass on its own channel (Settings > MIDI > Bass): the chord root an octave
// down. Off when tc_app.bass_channel is 0. Call with tc_app_lock held.
void bass_play(uint8_t chord_root);
void bass_stop(void);

void send_midi_chord(uint8_t channel, uint8_t status, uint8_t* notes, uint8_t length, uint8_t velocity);

void send_midi_chord_diff(const uint8_t* prev, const uint8_t* curr,
                          uint8_t channel, uint8_t velocity, bool retrigger);

#endif