/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#ifndef TOUCHORD_KEY_GESTURES_H
#define TOUCHORD_KEY_GESTURES_H

#include <stdbool.h>

// Hold a button and slide on the bar:
//   B5 sets the key, B4 picks the chord set, B3 sets the chord size.
// Returns true while the bar is taken over, so the mode never sees the touch.
bool key_gesture_poll(float pos, bool touching);

// Draws the panel for the held button over the mode's screen.
void key_gesture_draw(void);

// Shows how to use the held buttons for a few seconds once the device connects.
void key_gesture_draw_hint(void);

#endif
