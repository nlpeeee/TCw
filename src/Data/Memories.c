/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#include "Memories.h"
#include "Flash.h"
#include "Globals.h"
#include "Sync.h"
#include "Notes/Note.h"
#include "Notes/ChordSets.h"
#include "Modes/Arp.h"
#include "Rendering/Graphics.h"
#include "pico/critical_section.h"
#include <stdio.h>
#include <string.h>

#define AUTOSAVE_QUIET_MS 5000

// Dark 1..Bright 2 in chord_sets[] order.
enum { DARK_2 = 1, DARK_3 = 2, WARM_1 = 4, WARM_2 = 5, WARM_3 = 6, BRIGHT_1 = 8, BRIGHT_2 = 9 };

// The factory starter setups for memories 1-7, each showing off a mode.
static void starter(uint8_t slot, TouchordSettings* s)
{
    *s = tc_app_default;
    s->key[0].root = NOTE_C;
    s->key[0].quality = SCALE_CHORD_SET;
    switch (slot) {
        case 0:     // the default: the four-chord pop loop
            s->mode = TOUCHORD_COMPOSE; s->chord_set = BRIGHT_1; s->extension_count = 3;
            break;
        case 1:     // ninth chords, rolled
            s->mode = TOUCHORD_COMPOSE; s->chord_set = WARM_3; s->extension_count = 5;
            s->compose_roll = 3;
            break;
        case 2:     // latched arp
            s->mode = TOUCHORD_ARP; s->chord_set = DARK_2; s->extension_count = 4;
            s->arp_pattern = 0; s->arp_rate = 3; s->arp_octaves = 0;
            break;
        case 3:     // strumming the bar
            s->mode = TOUCHORD_STRUM; s->chord_set = BRIGHT_2; s->extension_count = 4;
            s->octave_count = 2;
            break;
        case 4:     // omnichord-style strum
            s->mode = TOUCHORD_OMNI; s->chord_set = WARM_2; s->extension_count = 4;
            break;
        case 5:     // a slower arp on the R&B loop
            s->mode = TOUCHORD_ARP; s->chord_set = WARM_1; s->extension_count = 4;
            s->arp_pattern = 2; s->arp_rate = 1; s->arp_octaves = 1;
            break;
        default:    // chords with pitch bend from the bar
            s->mode = TOUCHORD_PERFORM; s->chord_set = DARK_3; s->extension_count = 4;
            s->perform_pos_type = PERFORM_BEND;
            break;
    }
}

bool memory_load(uint8_t slot, TouchordSettings* out)
{
    if (flash_load_preset(slot, out)) {
        use_single_key_slot(out);
        return true;
    }
    if (slot >= MEMORY_COUNT) return false;
    starter(slot, out);
    return true;
}

void memory_describe(const TouchordSettings* s, char* out, int len)
{
    TouchordMode m = s->mode < TOUCHORD_SETTINGS ? s->mode : TOUCHORD_COMPOSE;
    const Scale* k = &s->key[0];
    const char* chords = k->quality == SCALE_CHORD_SET
        ? chord_set_names[s->chord_set < CHORD_SET_COUNT ? s->chord_set : CHORD_SET_DEFAULT]
        : (k->quality < SCALE_COUNT ? scale_names[k->quality] : "");
    snprintf(out, len, "%s %s", tc_mode_names[m], chords);
}

void memory_recall(uint8_t slot)
{
    TouchordSettings next;
    char msg[24], what[20];
    if (!memory_load(slot, &next)) return;

    critical_section_enter_blocking(&tc_app_lock);
    // A memory is a song, not a setup of the rig: MIDI and Bluetooth stay.
    next.channel      = tc_app.channel;
    next.midi_type    = tc_app.midi_type;
    next.bluetooth    = tc_app.bluetooth;
    next.clock_in     = tc_app.clock_in;
    next.bass_channel = tc_app.bass_channel;
    // The sounding chord stays too, so the old mode can stop its notes.
    memcpy(next.chord, tc_app.chord, sizeof next.chord);
    memcpy(next.chord_name, tc_app.chord_name, sizeof next.chord_name);
    if (next.mode >= TOUCHORD_SETTINGS) next.mode = TOUCHORD_COMPOSE;
    tc_app = next;
    critical_section_exit(&tc_app_lock);

    reload_custom_scales();
    tc_mode_restart = true;
    memory_describe(&next, what, sizeof what);
    snprintf(msg, sizeof msg, "%u %s", (unsigned)slot + 1, what);
    trigger_overlay(msg, 1200);
}

void memory_store(uint8_t slot)
{
    char msg[24];
    TouchordSettings copy;
    critical_section_enter_blocking(&tc_app_lock);
    copy = tc_app;
    critical_section_exit(&tc_app_lock);
    if (flash_save_preset(slot, &copy))
        snprintf(msg, sizeof msg, "Saved to %u", (unsigned)slot + 1);
    else
        snprintf(msg, sizeof msg, "Save Failed");
    trigger_overlay(msg, 1200);
}

void memories_load_startup(TouchordSettings* out)
{
    *out = tc_app_default;
    if (!flash_load_last(out) && !flash_load_preset(0, out))
        starter(0, out);
    use_single_key_slot(out);
}

// The parts of the settings that make up the setup, without what is
// sounding right now.
static void setup_only(TouchordSettings* s)
{
    memset(s->chord, 0, sizeof s->chord);
    memset(s->chord_name, 0, sizeof s->chord_name);
    s->mode = TOUCHORD_COMPOSE;
    s->degree = CHORD_DEFAULT;
    s->inversion = 0;
    s->prev_extension = 0;
}

// Flash writes pause the screen core and interrupts briefly, so they wait
// until nothing is being played.
static bool idle(void)
{
    if (tc_touch_state || arp_is_playing()) return false;
    for (int i = 0; i < NUM_KEYS; i++)     if (!tc_key_states[i])     return false;
    for (int i = 0; i < NUM_CONTROLS; i++) if (!tc_control_states[i]) return false;
    return tc_app_get_mode() != TOUCHORD_SETTINGS;
}

void autosave_task(void)
{
    static TouchordSettings saved, seen;
    static bool primed = false;
    static uint32_t changed_at = 0, checked_at = 0;

    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - checked_at < 250) return;
    checked_at = now;

    TouchordSettings cur;
    critical_section_enter_blocking(&tc_app_lock);
    cur = tc_app;
    critical_section_exit(&tc_app_lock);
    setup_only(&cur);

    if (!primed) {
        saved = seen = cur;
        primed = true;
        return;
    }
    if (memcmp(&cur, &seen, sizeof cur) != 0) {
        seen = cur;
        changed_at = now;
    }
    if (memcmp(&seen, &saved, sizeof seen) != 0
        && now - changed_at >= AUTOSAVE_QUIET_MS && idle()) {
        flash_save_last(&seen);
        saved = seen;
    }
}
