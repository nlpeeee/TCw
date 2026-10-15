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

#include "Quantize.h"
#include "Globals.h"
#include "Sync.h"
#include "Timing/MidiClock.h"
#include "Rendering/Graphics.h"
#include "pico/time.h"
#include <stdio.h>
#include <string.h>

// The key event is held back, not its notes, so each mode's own logic (roll,
// voice leading, bass, arp) runs unchanged, just later.
//
// While a host sends clock and is playing, the grid is its ticks. Otherwise
// it runs at the Arp Tempo, started by the first press after a rest. A press
// in the first quarter of a step plays at once rather than waiting almost a
// whole step. Releases are never held back, except that a key let go before
// its chord played gets one grid step, and a key let go while another press
// waits keeps sounding until that press plays.

const char *quant_grid_names[QUANT_GRID_COUNT] = { "Off", "1/16", "1/8", "1/4", "1 Bar" };
static const uint8_t grid_ticks[QUANT_GRID_COUNT] = { 0, 6, 12, 24, 96 };

#define QUANT_NO_KEY     0xFF
#define QUANT_REST_BEATS 8      // no keys for two bars: the next press starts a new grid

// Core 0 only, except pend_key which the screen reads.
static volatile uint8_t pend_key = QUANT_NO_KEY;
static TouchordMode pend_mode;
static bool         pend_released;   // let go before it played
static bool         pend_on_ticks;   // waiting for a clock tick count, else a time
static uint32_t     pend_tick;
static uint64_t     pend_us;
static uint32_t     pend_step_us;    // grid step when pressed, for a tap's length
static uint32_t     held_ups;        // other keys let go while a press waits

static uint8_t      tap_key = QUANT_NO_KEY;   // played tap, released at tap_us
static TouchordMode tap_mode;
static uint64_t     tap_us;

static bool     have_anchor = false;
static uint64_t anchor_us;
static uint32_t down_mask = 0;
static uint64_t last_event_us = 0;

static uint8_t grid(void)
{
    uint8_t q = tc_app.quantize;
    return q < QUANT_GRID_COUNT ? grid_ticks[q] : 0;
}

static uint32_t tick_us(void)
{
    uint32_t t = midi_clock_tick_us();
    if (t) return t;
    uint32_t bpm = tc_app.arp_bpm ? tc_app.arp_bpm : 120;
    return 60000000u / (MIDI_CLOCK_PPQN * bpm);
}

// Arp already steps on its own grid without a running clock.
static bool applies(TouchordMode mode)
{
    if (!grid()) return false;
    switch (mode) {
        case TOUCHORD_COMPOSE:
        case TOUCHORD_DRUM:    return true;
        case TOUCHORD_ARP:     return midi_clock_running();
        default:               return false;
    }
}

static void send_up(uint8_t key)
{
    tc_key_up(key);
    tc_key_up_independent(key);
}

static void release_tap(void)
{
    uint8_t key = tap_key;
    tap_key = QUANT_NO_KEY;
    if (key != QUANT_NO_KEY && tc_app_get_mode() == tap_mode) send_up(key);
}

static void send_held_ups(void)
{
    uint32_t ups = held_ups;
    held_ups = 0;
    for (uint8_t k = 0; k < NUM_KEYS; k++)
        if (ups & (1u << k)) send_up(k);
}

// Play now. Keys let go while an earlier press waited go after it.
static void play(uint8_t key)
{
    tc_key_down(key);
    send_held_ups();
}

static void release_pending(void)
{
    uint8_t key = pend_key;
    if (key == QUANT_NO_KEY) return;
    pend_key = QUANT_NO_KEY;

    TouchordMode mode = tc_app_get_mode();
    if (mode != pend_mode) {
        held_ups = 0;   // the mode changed and already let go of everything
        return;
    }

    if (tap_key != QUANT_NO_KEY) release_tap();
    play(key);
    if (pend_released) {
        tap_key  = key;
        tap_mode = mode;
        tap_us   = time_us_64() + pend_step_us;
    }
}

void quant_key_down(uint8_t key)
{
    uint64_t now = time_us_64();
    bool rested = down_mask == 0
               && now - last_event_us > (uint64_t)tick_us() * MIDI_CLOCK_PPQN * QUANT_REST_BEATS;
    down_mask |= 1u << key;
    last_event_us = now;
    held_ups &= ~(1u << key);
    if (tap_key == key) release_tap();

    TouchordMode mode = tc_app_get_mode();
    if (!applies(mode)) {
        release_pending();
        tc_key_down(key);
        return;
    }

    // Last press wins: an earlier press still waiting never sounds.
    pend_key = QUANT_NO_KEY;

    uint32_t g = grid();
    if (midi_clock_running()) {
        uint32_t t = midi_clock_ticks();
        uint32_t phase = t % g;
        if (phase * 4 < g) { play(key); return; }
        pend_on_ticks = true;
        pend_tick     = t - phase + g;
        pend_step_us  = tick_us() * g;
    } else {
        uint32_t step = tick_us() * g;
        if (!have_anchor || rested) {
            have_anchor = true;
            anchor_us   = now;
            play(key);
            return;
        }
        uint32_t phase = (uint32_t)((now - anchor_us) % step);
        if (phase * 4ull < step) { play(key); return; }
        pend_on_ticks = false;
        pend_us       = now + (step - phase);
        pend_step_us  = step;
    }
    pend_mode     = mode;
    pend_released = false;
    pend_key      = key;
}

void quant_key_up(uint8_t key)
{
    down_mask &= ~(1u << key);
    last_event_us = time_us_64();

    if (key == pend_key) { pend_released = true; return; }
    if (pend_key != QUANT_NO_KEY) { held_ups |= 1u << key; return; }
    send_up(key);
}

void quant_task(void)
{
    if (tap_key != QUANT_NO_KEY && time_us_64() >= tap_us) release_tap();
    if (pend_key == QUANT_NO_KEY) return;

    bool due;
    if (!grid())                     due = true;    // turned off while waiting
    else if (pend_on_ticks) {
        // A new Start resets the tick count: don't wait for the old target.
        uint32_t t = midi_clock_ticks();
        due = !midi_clock_running() || t >= pend_tick || t + grid() < pend_tick;
    }
    else                             due = time_us_64() >= pend_us;
    if (due) release_pending();
}

void quant_draw_marker(void)
{
    if (!applies(tc_app.mode)) return;
    // Blink while a press waits so it never looks ignored.
    if (pend_key != QUANT_NO_KEY && (to_ms_since_boot(get_absolute_time()) / 100) % 2) return;
    char buf[8];
    snprintf(buf, sizeof buf, "Q%s", quant_grid_names[tc_app.quantize]);
    ssd1306_draw_string_with_font(&tc_disp, 0, 52, 1, font_3x6, buf);
}
