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

#ifndef TOUCHORD_KEY_GESTURES_H
#define TOUCHORD_KEY_GESTURES_H

#include <stdbool.h>

// Hold a button and slide on the bar:
//   B5 sets the key, B4 picks the chord set, B3 sets the chord size,
//   B0 picks a mode, B1 the mode's playing style, B2 the touch (velocity).
// Holding B1 and B2 together shows what every button does.
// Returns true while the bar is taken over, so the mode never sees the touch.
bool key_gesture_poll(float pos, bool touching);

// Draws the panel for the held button over the mode's screen.
void key_gesture_draw(void);

// Shows how to use the held buttons for a few seconds once the device connects.
void key_gesture_draw_hint(void);

#endif
