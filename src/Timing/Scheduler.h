/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#ifndef TOUCHORD_SCHEDULER_H
#define TOUCHORD_SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>

// Notes sent later instead of now. Events can be added from either core;
// core 0 sends them through the MIDI out queue when they fall due.
// Tags group a feature's events so they can be cancelled together.
enum {
    SCHED_TAG_COMPOSE = 1,
};

void sched_init(void);

// Send a note message after delay_us. A zero delay sends it now.
void sched_note(uint32_t delay_us, uint8_t channel, uint8_t status,
                uint8_t note, uint8_t velocity, uint8_t tag);

// Drop pending note-ons with this tag and send its pending note-offs now,
// so cancelling never leaves a note hanging.
void sched_cancel(uint8_t tag);

// Send every pending event with this tag now.
void sched_flush(uint8_t tag);

// Core 0 main loop: send events that are due.
void sched_task(void);

#endif
