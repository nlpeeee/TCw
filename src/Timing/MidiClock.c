/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#include "MidiClock.h"
#include <stdio.h>
#include "Globals.h"
#include "Rendering/Graphics.h"
#include "pico/time.h"

#define CLOCK_TIMEOUT_MS   500      // no tick for this long: clock is gone
#define CLOCK_MAX_TICK_US  200000   // slower than 12.5 BPM restarts the average

// Written on core 0 only. The volatiles are read from core 1 for the screen.
static volatile bool     present  = false;
static volatile bool     running  = false;
static volatile uint32_t tick_us  = 0;   // average tick interval over the last beat
static volatile uint32_t ticks    = 0;
static volatile uint32_t last_tick_ms = 0;

static ClockSource source;
static uint64_t    last_t_us[CLOCK_SRC_COUNT];
static bool        have_last[CLOCK_SRC_COUNT];

// Times of the last beat's ticks. Averaging over a whole beat cancels the
// jitter of senders whose ticks land on whole milliseconds.
static uint64_t window[MIDI_CLOCK_PPQN + 1];
static uint32_t window_len = 0, window_head = 0;

static void clock_tick(ClockSource src, uint64_t t_us)
{
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());

    // Two hosts sending clock at once would double the tempo: follow the
    // one that started first until it stops.
    if (present && src != source) return;
    source = src;

    if (!(have_last[src] && t_us > last_t_us[src]
          && t_us - last_t_us[src] < CLOCK_MAX_TICK_US)) {
        window_len = 0;
        tick_us = 0;
    }
    const uint32_t size = MIDI_CLOCK_PPQN + 1;
    window[window_head] = t_us;
    window_head = (window_head + 1) % size;
    if (window_len < size) window_len++;
    if (window_len >= 2) {
        uint64_t oldest = window[(window_head + size - window_len) % size];
        tick_us = (uint32_t)((t_us - oldest) / (window_len - 1));
    }
    last_t_us[src] = t_us;
    have_last[src] = true;

    last_tick_ms = now_ms;
    present = true;
    if (running) ticks++;
}

void midi_clock_rx(ClockSource src, uint8_t status, uint64_t t_us)
{
    if (!tc_app.clock_in || src >= CLOCK_SRC_COUNT) return;

    switch (status) {
        case 0xF8:
            clock_tick(src, t_us);
            break;
        case 0xFA:
            ticks = 0;
            running = true;
            trigger_overlay("Clock Start", 600);
            break;
        case 0xFB:
            running = true;
            trigger_overlay("Clock Continue", 600);
            break;
        case 0xFC:
            if (running) trigger_overlay("Clock Stop", 600);
            running = false;
            break;
    }
}

void midi_clock_task(void)
{
    if (!present) return;
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    if (now_ms - last_tick_ms < CLOCK_TIMEOUT_MS && tc_app.clock_in) return;

    present = false;
    running = false;
    tick_us = 0;
    window_len = 0;
    for (int i = 0; i < CLOCK_SRC_COUNT; i++) have_last[i] = false;
}

bool midi_clock_present(void) { return present; }
bool midi_clock_running(void) { return running; }
uint32_t midi_clock_ticks(void) { return ticks; }

uint32_t midi_clock_tick_us(void) { return present ? tick_us : 0; }

uint32_t midi_clock_bpm(void)
{
    uint32_t t = tick_us;
    if (!present || t == 0) return 0;
    // 60 s / (24 ticks per beat * tick length), rounded
    return (60000000u / MIDI_CLOCK_PPQN + t / 2) / t;
}

void midi_clock_status(char *buf, uint32_t len)
{
    static const char *src_names[CLOCK_SRC_COUNT] = { "USB", "BT" };
    uint32_t bpm = midi_clock_bpm();
    if (!tc_app.clock_in)  snprintf(buf, len, "Ignoring clock");
    else if (!present)     snprintf(buf, len, "No clock");
    else if (bpm == 0)     snprintf(buf, len, "%s clock", src_names[source]);
    else                   snprintf(buf, len, "%s %lu BPM %s", src_names[source],
                                    (unsigned long)bpm, running ? "Play" : "Stop");
}
