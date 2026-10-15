/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#ifndef TOUCHORD_LOOPER_H
#define TOUCHORD_LOOPER_H

#include <stdint.h>
#include <stdbool.h>

// Tap B4 to record which green keys are pressed and when, tap again to loop
// them. Playback presses the keys for the mode, so whatever mode, key, chord
// set or size is in use plays the loop, and the bar stays live on top.

#define LOOP_MAX_EVENTS 60
#define LOOP_SLOT_LIVE  7       // flash slot of the setup being played; 0-6 are memories

typedef struct {
    uint16_t at;                // position in the loop, 1/16 of a MIDI clock tick
    uint8_t  key;
    uint8_t  down;
} LoopEvent;

typedef struct {
    uint16_t  length;           // 0 = no loop
    uint8_t   count;
    uint8_t   reserved;
    LoopEvent ev[LOOP_MAX_EVENTS];
} LoopData;

// Core 0 key path, after Quantize: what the player pressed reaches the mode
// through these.
void looper_live_down(uint8_t key);
void looper_live_up(uint8_t key);

// Core 0: B4 tapped, or held long enough to clear.
void looper_tap(void);
void looper_clear(void);
bool looper_has_loop(void);         // recorded, playing or stopped
bool looper_is_busy(void);          // armed, recording or playing

// Core 0 main loop: record, play and follow the clock.
void looper_task(void);

// True while the loop is holding this key down for the mode.
bool looper_holds(uint8_t key);
// True only during a key press or release the loop sends.
bool looper_injecting(void);

// Saving: memories and autosave copy the loop in and out (core 0).
void looper_get(LoopData* out);
void looper_set(const LoopData* in);  // stops playback, keeps the loop stopped
bool looper_dirty(void);
uint32_t looper_changed_ms(void);
void looper_mark_saved(void);
// The demo loop that comes with a starter memory.
void looper_demo(uint8_t slot, LoopData* out);

// Core 1 draw: REC / play position / LOOP at the bottom right.
void looper_draw_badge(void);

#endif
