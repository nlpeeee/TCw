/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#include "Looper.h"
#include "Globals.h"
#include "Sync.h"
#include "Timing/MidiClock.h"
#include "Modes/Arp.h"
#include "Rendering/Graphics.h"
#include "pico/time.h"
#include <stdio.h>
#include <string.h>

// The loop is a list of key presses and releases, not notes. Playback hands
// them to the mode the way the key scan does, so the mode's own roll, voice
// leading, arp and bass play the loop, and changing key, chord set or size
// changes what the loop plays.
//
// Positions count 1/16 of a MIDI clock tick. While a host sends clock and is
// playing they follow its ticks, so bars line up with the host and a Start
// takes the loop back to its top; otherwise they run at the Arp Tempo.

#define SUB          16
#define BEAT_SUBS    (MIDI_CLOCK_PPQN * SUB)
#define BAR_SUBS     (BEAT_SUBS * 4)
#define MAX_BARS     8
#define MAX_SUBS     (BAR_SUBS * MAX_BARS)      // fits LoopEvent.at
#define END_GAP_SUBS (SUB * 3)                  // a key still down at the end lets go just before the top

typedef enum { LOOP_EMPTY = 0, LOOP_ARMED, LOOP_RECORDING, LOOP_PLAYING, LOOP_STOPPED } LoopState;

static volatile LoopState state = LOOP_EMPTY;
static LoopData loop;

// Core 0 only, apart from the volatile ones the screen reads.
static uint32_t now_pos;            // positions since boot
static uint64_t last_us, acc_us;
static bool     was_running;
static uint32_t last_ticks;
static uint64_t last_tick_at;
static uint32_t clock_pos;          // host position: its ticks since Start, in positions
static bool     host_started;

static uint32_t origin;             // now_pos at the loop's top
static uint32_t last_play;          // events before this position have been sent
static volatile uint32_t shown_pos; // play or record position, for the screen

static uint32_t live_mask;          // keys the player holds, as the mode sees them
static uint32_t rec_mask;           // keys down in the recording so far
static volatile uint32_t loop_mask; // keys the loop holds down for the mode
static volatile bool injecting;
static void (*seen_key_down)(uint8_t);

static bool     dirty;
static uint32_t changed_at_ms;

static uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }

static void changed(void)
{
    dirty = true;
    changed_at_ms = now_ms();
}

static uint32_t tick_us(void)
{
    uint32_t t = midi_clock_tick_us();
    if (t) return t;
    uint32_t bpm = tc_app.arp_bpm ? tc_app.arp_bpm : 120;
    return 60000000u / (MIDI_CLOCK_PPQN * bpm);
}

// Moves now_pos on to the present.
static void advance(void)
{
    uint64_t now = time_us_64();
    if (last_us == 0) last_us = now;

    if (midi_clock_running()) {
        uint32_t t = midi_clock_ticks();
        if (!was_running || t < last_ticks) {
            // Start sets the host's ticks back to 0; Continue carries on.
            if (t < last_ticks || t < MIDI_CLOCK_PPQN / 4) host_started = true;
            was_running  = true;
            last_ticks   = t;
            last_tick_at = now;
            clock_pos    = t * SUB;
            acc_us       = 0;
            last_us      = now;
            return;
        }
        if (t != last_ticks) {
            last_ticks   = t;
            last_tick_at = now;
        }
        // Between ticks, guess how far into the next one we are.
        uint32_t frac = (uint32_t)((now - last_tick_at) * SUB / tick_us());
        if (frac >= SUB) frac = SUB - 1;
        uint32_t p = t * SUB + frac;
        if (p > clock_pos) {
            now_pos  += p - clock_pos;
            clock_pos = p;
        }
    } else {
        was_running = false;
        uint32_t step = tick_us() / SUB;
        if (!step) step = 1;
        acc_us += now - last_us;
        uint32_t d = (uint32_t)(acc_us / step);
        now_pos += d;
        acc_us  -= (uint64_t)d * step;
    }
    last_us = now;
}

// How far the current host bar has got, or 0 with no host playing.
static uint32_t bar_phase(void)
{
    return midi_clock_running() ? clock_pos % BAR_SUBS : 0;
}

static int32_t since_origin(void) { return (int32_t)(now_pos - origin); }

static uint32_t pos_in_loop(void)
{
    int32_t d = since_origin();
    if (d < 0) d = 0;
    return (uint32_t)d % loop.length;
}

// ---- Pressing keys for the mode --------------------------------------------

static void send_key(uint8_t key, bool down)
{
    uint32_t bit = 1u << key;
    if (down) loop_mask |= bit;
    else      loop_mask &= ~bit;
    // While the player holds a key the loop waits, and picks up at its next
    // press once they let go.
    if (live_mask) return;
    if (tc_app_get_mode() == TOUCHORD_SETTINGS) return;
    injecting = true;
    if (down) {
        tc_key_down(key);
    } else {
        tc_key_up(key);
        tc_key_up_independent(key);
    }
    injecting = false;
}

static void release_all(void)
{
    for (uint8_t k = 0; k < NUM_KEYS; k++)
        if (loop_mask & (1u << k)) send_key(k, false);
    loop_mask = 0;
}

// Events at positions from <= at < to.
static void fire(uint32_t from, uint32_t to)
{
    for (int i = 0; i < loop.count; i++) {
        const LoopEvent* e = &loop.ev[i];
        if (e->at >= from && e->at < to) send_key(e->key, e->down);
    }
}

// Starting part-way through: press the key the loop would be holding there.
static void chase(uint32_t p)
{
    uint32_t want = 0;
    int last_down = -1;
    for (int i = 0; i < loop.count; i++) {
        const LoopEvent* e = &loop.ev[i];
        if (e->at >= p) break;
        if (e->down) { want |= 1u << e->key; last_down = e->key; }
        else         want &= ~(1u << e->key);
    }
    for (uint8_t k = 0; k < NUM_KEYS; k++)
        if ((loop_mask & (1u << k)) && !(want & (1u << k))) send_key(k, false);
    if (last_down >= 0 && (want & (1u << last_down)) && !(loop_mask & (1u << last_down)))
        send_key((uint8_t)last_down, true);
}

// The arp keeps going on its own when latched; the loop stopping stops it.
static void stop_latched_arp(void)
{
    if (tc_app_get_mode() != TOUCHORD_ARP || live_mask) return;
    for (int k = 0; k < NUM_KEYS; k++) if (!tc_key_states[k]) return;
    arp_stop();
}

// ---- States -----------------------------------------------------------------

static void start_playing(void)
{
    // With a host playing, join it at the same point in the bar.
    origin    = now_pos - bar_phase();
    last_play = pos_in_loop();
    shown_pos = last_play;
    seen_key_down = tc_key_down;
    loop_mask = 0;
    chase(last_play);
    state = LOOP_PLAYING;
}

static void stop_playing(void)
{
    release_all();
    stop_latched_arp();
    state = LOOP_STOPPED;
}

static void sort_events(void)
{
    for (int i = 1; i < loop.count; i++) {
        LoopEvent e = loop.ev[i];
        int j = i - 1;
        while (j >= 0 && loop.ev[j].at > e.at) { loop.ev[j + 1] = loop.ev[j]; j--; }
        loop.ev[j + 1] = e;
    }
}

static void close_recording(void)
{
    int32_t d = since_origin();
    uint32_t len = d > 0 ? (uint32_t)d : 1;
    if (len > MAX_SUBS) len = MAX_SUBS;
    // Same rule as Quantize: whole bars when there is a beat to follow.
    if (midi_clock_running() || tc_app.quantize) {
        uint32_t bars = (len + BAR_SUBS / 2) / BAR_SUBS;
        if (bars < 1) bars = 1;
        if (bars > MAX_BARS) bars = MAX_BARS;
        len = bars * BAR_SUBS;
    } else if (len < BEAT_SUBS) {
        len = BEAT_SUBS;
    }

    // Drop what falls past the end, then let go of keys still down just
    // before the top so every press has its release.
    uint8_t n = 0;
    uint32_t held = 0;
    uint16_t down_at[NUM_KEYS] = {0};
    for (int i = 0; i < loop.count; i++) {
        LoopEvent e = loop.ev[i];
        if (e.at >= len) continue;
        uint32_t bit = 1u << e.key;
        if (e.down) { held |= bit; down_at[e.key] = e.at; }
        else if (held & bit) held &= ~bit;
        else continue;
        loop.ev[n++] = e;
    }
    for (uint8_t k = 0; k < NUM_KEYS && n < LOOP_MAX_EVENTS; k++) {
        if (!(held & (1u << k))) continue;
        uint32_t at = len > END_GAP_SUBS ? len - END_GAP_SUBS : len - 1;
        if (at <= down_at[k]) at = len - 1;
        loop.ev[n++] = (LoopEvent){ (uint16_t)at, k, 0 };
    }
    loop.count  = n;
    loop.length = (uint16_t)len;
    sort_events();
    changed();

    // Carry straight on into the loop from where recording got to.
    last_play = pos_in_loop();
    shown_pos = last_play;
    seen_key_down = tc_key_down;
    loop_mask = 0;
    chase(last_play);
    state = LOOP_PLAYING;
    trigger_overlay("Looping", 800);
}

static void record(uint8_t key, bool down)
{
    uint32_t bit = 1u << key;
    if (!down && !(rec_mask & bit)) return;        // pressed before recording began
    int32_t at = since_origin();
    if (at < 0) at = 0;
    if (at >= MAX_SUBS) return;
    if (down) {
        // Keep room for the release of every key still down.
        uint32_t held = rec_mask | bit;
        int ups = 0;
        for (int k = 0; k < NUM_KEYS; k++) if (held & (1u << k)) ups++;
        if (loop.count + 1 + ups > LOOP_MAX_EVENTS) return;
        rec_mask |= bit;
    } else {
        rec_mask &= ~bit;
    }
    loop.ev[loop.count++] = (LoopEvent){ (uint16_t)at, key, down ? 1 : 0 };
}

static void begin_recording(void)
{
    loop.count  = 0;
    loop.length = 0;
    rec_mask    = 0;
    // The first press is the top. With a host playing it is the top of the
    // bar it falls in, or of the next bar if it comes just before one.
    uint32_t phase = bar_phase();
    if (phase && phase >= BAR_SUBS - BEAT_SUBS / 2) origin = now_pos + (BAR_SUBS - phase);
    else                                            origin = now_pos - phase;
    shown_pos = 0;
    state = LOOP_RECORDING;
}

// ---- Public -------------------------------------------------------------------

void looper_live_down(uint8_t key)
{
    advance();
    // Keys move the cursor in Settings: never record those.
    if (tc_app_get_mode() != TOUCHORD_SETTINGS) {
        if (state == LOOP_ARMED) begin_recording();
        if (state == LOOP_RECORDING) record(key, true);
    }
    live_mask |= 1u << key;
    tc_key_down(key);
}

void looper_live_up(uint8_t key)
{
    advance();
    if (state == LOOP_RECORDING) record(key, false);
    live_mask &= ~(1u << key);
    tc_key_up(key);
    tc_key_up_independent(key);
}

void looper_tap(void)
{
    advance();
    switch (state) {
        case LOOP_EMPTY:
            state = LOOP_ARMED;
            trigger_overlay("Loop: play to record", 1200);
            break;
        case LOOP_ARMED:
            state = LOOP_EMPTY;
            trigger_overlay("Loop cancelled", 800);
            break;
        case LOOP_RECORDING:
            close_recording();
            break;
        case LOOP_PLAYING:
            stop_playing();
            break;
        case LOOP_STOPPED:
            start_playing();
            break;
    }
}

void looper_clear(void)
{
    if (state == LOOP_PLAYING) stop_playing();
    bool had = state == LOOP_PLAYING || state == LOOP_STOPPED || state == LOOP_RECORDING;
    state = LOOP_EMPTY;
    loop.count = 0;
    loop.length = 0;
    rec_mask = 0;
    if (had) changed();
}

bool looper_has_loop(void) { return state != LOOP_EMPTY; }

bool looper_is_busy(void)
{
    LoopState s = state;
    return s == LOOP_ARMED || s == LOOP_RECORDING || s == LOOP_PLAYING;
}

void looper_task(void)
{
    advance();
    bool restart = host_started;
    host_started = false;

    // Core 1 rebinds the key handlers when the mode changes, and the old
    // mode lets go of everything, including keys whose release it will now
    // never be told about.
    bool new_mode = tc_key_down != seen_key_down;
    if (new_mode) {
        seen_key_down = tc_key_down;
        uint32_t down = 0;
        for (int k = 0; k < NUM_KEYS; k++) if (!tc_key_states[k]) down |= 1u << k;
        live_mask &= down;
    }

    if (state == LOOP_RECORDING) {
        int32_t d = since_origin();
        shown_pos = d > 0 ? (uint32_t)d : 0;
        if (d >= (int32_t)MAX_SUBS) close_recording();
        return;
    }
    if (state != LOOP_PLAYING) return;

    if (restart) {
        release_all();
        origin    = now_pos;
        last_play = 0;
    }
    // Press the loop's key again for the new mode.
    if (new_mode) {
        loop_mask = 0;
        last_play = pos_in_loop();
        chase(last_play);
        return;
    }

    uint32_t p = pos_in_loop();
    if (p >= last_play) {
        fire(last_play, p);
    } else {
        fire(last_play, loop.length);
        fire(0, p);
    }
    last_play = p;
    shown_pos = p;
}

bool looper_holds(uint8_t key)
{
    return state == LOOP_PLAYING && (loop_mask & (1u << key));
}

bool looper_injecting(void) { return injecting; }

void looper_get(LoopData* out)
{
    LoopState s = state;
    if (s == LOOP_PLAYING || s == LOOP_STOPPED) {
        *out = loop;
    } else {
        memset(out, 0, sizeof *out);
    }
}

void looper_set(const LoopData* in)
{
    if (state == LOOP_PLAYING) stop_playing();
    bool ok = in && in->length && in->length <= MAX_SUBS
           && in->count && in->count <= LOOP_MAX_EVENTS;
    if (ok) {
        loop = *in;
        for (int i = 0; i < loop.count; i++)
            if (loop.ev[i].key >= NUM_KEYS || loop.ev[i].at >= loop.length) { ok = false; break; }
    }
    if (ok) {
        state = LOOP_STOPPED;
    } else {
        loop.count = 0;
        loop.length = 0;
        state = LOOP_EMPTY;
    }
    rec_mask = 0;
    changed();
}

bool looper_dirty(void) { return dirty; }
uint32_t looper_changed_ms(void) { return changed_at_ms; }
void looper_mark_saved(void) { dirty = false; }

// One chord a bar for four bars, from the starter memory's chord set.
static const uint8_t demo_keys[7][4] = {
    {0, 2, 3, 1},   // Bright 1: C G7 Am F
    {0, 4, 3, 5},   // Warm 3:   C Am Dm G7
    {0, 1, 3, 2},   // Dark 2:   Cm Ab Eb Bb
    {0, 1, 2, 0},   // Bright 2: C Bb F C
    {0, 4, 1, 2},   // Warm 2:   C Am F G7
    {0, 1, 5, 2},   // Warm 1:   Cm Ab Fm G7
    {0, 2, 4, 5},   // Dark 3:   Cm Ab Eb Bb7
};

void looper_demo(uint8_t slot, LoopData* out)
{
    memset(out, 0, sizeof *out);
    if (slot >= 7) return;
    for (int b = 0; b < 4; b++) {
        uint8_t k = demo_keys[slot][b];
        out->ev[out->count++] = (LoopEvent){ (uint16_t)(b * BAR_SUBS), k, 1 };
        out->ev[out->count++] = (LoopEvent){ (uint16_t)((b + 1) * BAR_SUBS - END_GAP_SUBS), k, 0 };
    }
    out->length = 4 * BAR_SUBS;
}

void looper_draw_badge(void)
{
    LoopState s = state;
    if (s == LOOP_EMPTY || tc_app.mode == TOUCHORD_SETTINGS) return;

    char buf[12];
    bool blink = (now_ms() / 250) % 2;
    uint32_t bars = loop.length ? (loop.length + BAR_SUBS - 1) / BAR_SUBS : 0;
    switch (s) {
        case LOOP_ARMED:
            if (!blink) return;
            snprintf(buf, sizeof buf, "REC");
            break;
        case LOOP_RECORDING:
            snprintf(buf, sizeof buf, "REC %lu", (unsigned long)(shown_pos / BAR_SUBS + 1));
            break;
        case LOOP_PLAYING:
            snprintf(buf, sizeof buf, ">%lu/%lu",
                     (unsigned long)(shown_pos / BAR_SUBS + 1), (unsigned long)bars);
            break;
        default:
            snprintf(buf, sizeof buf, "LOOP");
            break;
    }
    int adv = font_3x6[1] + font_3x6[2];
    int w = (int)strlen(buf) * adv - font_3x6[2];
    ssd1306_draw_string_with_font(&tc_disp, 128 - w, 52, 1, font_3x6, buf);
}
