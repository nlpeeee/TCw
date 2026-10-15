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

#ifndef TOUCHORD_MIDI_CLOCK_H
#define TOUCHORD_MIDI_CLOCK_H

#include <stdint.h>
#include <stdbool.h>

#define MIDI_CLOCK_PPQN 24

typedef enum {
    CLOCK_SRC_USB = 0,
    CLOCK_SRC_BLE,
    CLOCK_SRC_COUNT
} ClockSource;

// Core 0: feed one incoming real-time byte (F8 tick, FA start, FB continue,
// FC stop). t_us is when it was sent, on that source's own timeline: local
// time for USB, the sender's timestamps for BLE. Only intervals are used.
void midi_clock_rx(ClockSource src, uint8_t status, uint64_t t_us);

// Core 0 main loop: notice when the clock goes away.
void midi_clock_task(void);

// Safe from either core.
bool     midi_clock_present(void);    // ticks arrived recently
bool     midi_clock_running(void);    // between Start/Continue and Stop
uint32_t midi_clock_bpm(void);        // 0 when no clock
uint32_t midi_clock_tick_us(void);    // average tick length, 0 when no clock
uint32_t midi_clock_ticks(void);      // ticks since Start, 24 per beat
void     midi_clock_status(char *buf, uint32_t len);

#endif
