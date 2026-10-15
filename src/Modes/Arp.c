/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#include "Arp.h"
#include "Globals.h"
#include "Timing/Quantize.h"
#include "Helper.h"
#include "Sync.h"
#include "IO/Midi.h"
#include "Notes/Note.h"
#include "Rendering/Graphics.h"
#include "Timing/Scheduler.h"
#include "Timing/MidiClock.h"
#include "Timing/Looper.h"
#include "pico/critical_section.h"
#include <string.h>
#include <stdio.h>

// Hold a key and its chord is played one note at a time. Steps follow
// incoming MIDI clock while the host is playing, otherwise the Tempo setting
// (or the incoming clock's tempo, while the host is stopped).
//
// Touch bar, 8 segments: 6 rates, then a two-segment Range zone where each
// tap steps 1-4 octaves. Latch is in Settings. Double-click B1/B2 steps the
// pattern.

enum { ARP_UP = 0, ARP_DOWN, ARP_UPDOWN, ARP_RANDOM, ARP_CHORD };

const char *arp_pattern_names[ARP_PATTERN_COUNT] = { "Up", "Down", "Up-Down", "Random", "Chord" };
const char *arp_rate_names[ARP_RATE_COUNT]       = { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
const char *arp_octave_names[ARP_OCTAVES_MAX]    = { "1 Oct", "2 Oct", "3 Oct", "4 Oct" };
const char *arp_gate_names[ARP_GATE_COUNT]       = { "25%", "50%", "75%", "95%" };

static const uint8_t rate_ticks[ARP_RATE_COUNT] = { 24, 12, 8, 6, 4, 3 };   // MIDI clock ticks per step
static const uint8_t gate_pct[ARP_GATE_COUNT]   = { 25, 50, 75, 95 };

#define ARP_TRILL_SEGS  8
#define ARP_NO_KEY      0xFF

// Shared between core 0 (keys, arp_task) and core 1 (touch bar, mode
// changes), so guarded by tc_app_lock.
static bool     active   = false;
static uint8_t  held_key = ARP_NO_KEY;
static uint8_t  base[MAX_CHORD];        // chord notes, lowest first
static uint8_t  base_len = 0;
static uint32_t step     = 0;
static uint64_t next_step_us = 0;
static uint32_t last_tick_step = 0;
static bool     synced   = false;       // stepping on external clock ticks
static uint32_t rng      = 0x1234567u;

static int  trill_seg_prev = -1;
static char arp_tooltip[16] = {'\0'};

static uint32_t clamp_setting(uint8_t v, uint32_t count) { return v < count ? v : 0; }

static void load_chord(uint8_t key)
{
    build_chord(tc_app.key[tc_app.current_key], tc_app.octave, key, tc_app.degree,
                tc_app.extension_count, tc_app.inversion,
                tc_app.chord, tc_app.chord_name);
    base_len = 0;
    for (int i = 0; i < MAX_CHORD; i++) {
        uint8_t c = tc_app.chord[i];
        if (!c) continue;
        int j = base_len++;
        while (j > 0 && base[j - 1] > c) { base[j] = base[j - 1]; j--; }
        base[j] = c;
    }

    // Bass channel gets the root, in root position
    uint8_t root_pos[MAX_CHORD];
    char    name[CHORD_NAME_LEN];
    build_chord(tc_app.key[tc_app.current_key], tc_app.octave, key, tc_app.degree,
                tc_app.extension_count, 0, root_pos, name);
    bass_play(root_pos[0]);
}

static void stop_locked(void)
{
    active   = false;
    held_key = ARP_NO_KEY;
    sched_cancel(SCHED_TAG_ARP);
    bass_stop();
    tc_app.chord_name[0] = '\0';
}

static uint32_t tick_us_now(void)
{
    uint32_t t = midi_clock_present() ? midi_clock_tick_us() : 0;
    if (t) return t;
    uint32_t bpm = tc_app.arp_bpm ? tc_app.arp_bpm : 120;
    return 60000000u / (MIDI_CLOCK_PPQN * bpm);
}

static uint32_t next_random(void)
{
    rng = rng * 1664525u + 1013904223u;
    return rng >> 16;
}

// Send one step's note(s); note-offs go through the scheduler.
static void play_step_locked(uint32_t step_us)
{
    if (base_len == 0) return;

    uint32_t octaves = clamp_setting(tc_app.arp_octaves, ARP_OCTAVES_MAX) + 1;
    uint32_t len     = base_len * octaves;
    uint32_t gate_us = step_us * gate_pct[clamp_setting(tc_app.arp_gate, ARP_GATE_COUNT)] / 100;
    if (gate_us < 2000) gate_us = 2000;

    uint8_t  ch  = tc_app.channel;
    uint8_t  vel = tc_app.velocity;
    uint32_t idx = 0;

    switch (clamp_setting(tc_app.arp_pattern, ARP_PATTERN_COUNT)) {
        case ARP_UP:   idx = step % len; break;
        case ARP_DOWN: idx = len - 1 - step % len; break;
        case ARP_UPDOWN:
            if (len < 2) { idx = 0; break; }
            idx = step % (2 * len - 2);
            if (idx >= len) idx = 2 * len - 2 - idx;
            break;
        case ARP_RANDOM: idx = next_random() % len; break;
        case ARP_CHORD: {
            uint32_t oct = step % octaves;
            for (int i = 0; i < base_len; i++) {
                uint8_t n = clamp_midi(base[i] + 12 * oct);
                send_midi_note(ch, NOTE_ON, n, vel);
                sched_note(gate_us, ch, NOTE_OFF, n, vel, SCHED_TAG_ARP);
            }
            step++;
            return;
        }
    }

    uint8_t n = clamp_midi(base[idx % base_len] + 12 * (idx / base_len));
    send_midi_note(ch, NOTE_ON, n, vel);
    sched_note(gate_us, ch, NOTE_OFF, n, vel, SCHED_TAG_ARP);
    step++;
}

void arp_task(void)
{
    if (!active) return;

    critical_section_enter_blocking(&tc_app_lock);
    if (!active) { critical_section_exit(&tc_app_lock); return; }
    if (!tc_app.arp_latch && tc_key_states[held_key] && !looper_holds(held_key)) {
        // Latch was turned off while no key was held.
        stop_locked();
        critical_section_exit(&tc_app_lock);
        return;
    }

    uint32_t tps = rate_ticks[clamp_setting(tc_app.arp_rate, ARP_RATE_COUNT)];

    if (midi_clock_running()) {
        // Lock to the host's grid: a step on every tps-th tick since Start.
        uint32_t s = midi_clock_ticks() / tps;
        if (!synced) { synced = true; last_tick_step = s; play_step_locked(tick_us_now() * tps); }
        else if (s != last_tick_step) { last_tick_step = s; play_step_locked(tick_us_now() * tps); }
    } else {
        uint64_t now = time_us_64();
        uint32_t step_us = tick_us_now() * tps;
        if (synced) { synced = false; next_step_us = now + step_us; }
        if (now >= next_step_us) {
            play_step_locked(step_us);
            next_step_us += step_us;
            if (next_step_us <= now) next_step_us = now + step_us;   // fell behind: don't burst
        }
    }
    critical_section_exit(&tc_app_lock);
}

static const ModeHandlers arp_handlers = TC_MODE_HANDLERS(arp);

void arp_start()
{
    tc_bind_handlers(&arp_handlers);
    tc_trill_segs  = ARP_TRILL_SEGS;
    tc_trill_show  = true;
    trill_seg_prev = -1;
    arp_tooltip[0] = '\0';
}

void arp_end()
{
    critical_section_enter_blocking(&tc_app_lock);
    stop_locked();
    critical_section_exit(&tc_app_lock);
    arp_tooltip[0] = '\0';
}

void arp_draw()
{
    draw_current_chord();

    char info[32];
    if (arp_tooltip[0]) {
        snprintf(info, sizeof info, "%s", arp_tooltip);
    } else {
        uint32_t bpm = midi_clock_bpm();
        char tempo[12];
        if (bpm) snprintf(tempo, sizeof tempo, "%luExt", (unsigned long)bpm);
        else     snprintf(tempo, sizeof tempo, "%u", tc_app.arp_bpm);
        snprintf(info, sizeof info, "%s %s %s %s%s",
                 arp_pattern_names[clamp_setting(tc_app.arp_pattern, ARP_PATTERN_COUNT)],
                 arp_rate_names[clamp_setting(tc_app.arp_rate, ARP_RATE_COUNT)],
                 arp_octave_names[clamp_setting(tc_app.arp_octaves, ARP_OCTAVES_MAX)],
                 tempo, tc_app.arp_latch ? " Latch" : "");
    }
    int adv = font_3x6[1] + font_3x6[2];
    int w   = (int)strlen(info) * adv - font_3x6[2];
    ssd1306_draw_string_with_font(&tc_disp, (128 - w) / 2, 47, 1, font_3x6, info);
    draw_mode_label("Arp");
    quant_draw_marker();
}

void arp_update() {}

void arp_key_down(uint8_t key)
{
    critical_section_enter_blocking(&tc_app_lock);
    if (tc_app.arp_latch && active && held_key == key && !looper_injecting()) {
        // Pressing the latched chord again stops it. A loop that plays the
        // same chord twice in a row keeps it going.
        stop_locked();
    } else {
        load_chord(key);
        held_key = key;
        if (!active) {
            active = true;
            synced = false;
            step   = 0;
            next_step_us = time_us_64();
        }
    }
    critical_section_exit(&tc_app_lock);
}

void arp_key_up(uint8_t key)
{
    critical_section_enter_blocking(&tc_app_lock);
    if (held_key == key && !tc_app.arp_latch) stop_locked();
    critical_section_exit(&tc_app_lock);
}

void arp_key_up_independent(uint8_t key) {}

void arp_button_down(uint8_t button)
{
    switch (button) {
        case 0: tc_app_set_mode(TOUCHORD_STRUM); break;
        case 1: if (tc_app.octave > OCTAVE_MIN) tc_app.octave--; break;
        case 2: if (tc_app.octave < OCTAVE_MAX) tc_app.octave++; break;
    }
}


void arp_button_up(uint8_t button) {}

bool arp_is_playing(void) { return active; }

void arp_stop(void)
{
    critical_section_enter_blocking(&tc_app_lock);
    if (active) stop_locked();
    critical_section_exit(&tc_app_lock);
}

void arp_trill_down(float pos, float size)
{
    int seg = segments(pos, ARP_TRILL_SEGS);
    if (seg < 0) seg = 0;
    if (seg >= ARP_TRILL_SEGS) seg = ARP_TRILL_SEGS - 1;
    if (seg == trill_seg_prev) return;
    bool fresh = trill_seg_prev < 0;
    trill_seg_prev = seg;
    // The Range zone acts once per tap, not when a rate slide runs into it.
    if (seg >= ARP_RATE_COUNT && !fresh) return;

    critical_section_enter_blocking(&tc_app_lock);
    if (seg < ARP_RATE_COUNT) {
        // The far end of the bar (position 0) is the fastest rate.
        int rate = ARP_RATE_COUNT - 1 - seg;
        tc_app.arp_rate = rate;
        snprintf(arp_tooltip, sizeof arp_tooltip, "Rate %s", arp_rate_names[rate]);
    } else {
        tc_app.arp_octaves = (clamp_setting(tc_app.arp_octaves, ARP_OCTAVES_MAX) + 1) % ARP_OCTAVES_MAX;
        snprintf(arp_tooltip, sizeof arp_tooltip, "Range %s", arp_octave_names[tc_app.arp_octaves]);
    }
    critical_section_exit(&tc_app_lock);
}

void arp_trill_up()
{
    trill_seg_prev = -1;
    arp_tooltip[0] = '\0';
}
