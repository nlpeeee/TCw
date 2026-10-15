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

#ifndef TOUCHORD_DRUM_H
#define TOUCHORD_DRUM_H

#include <stdio.h>
#include "pico/stdlib.h"
#include "Defines.h"
#include "Types.h"

extern const char* const drum_layout_names[DRUM_LAYOUT_COUNT];

void drum_start();
void drum_end();
void drum_draw();
void drum_update();
void drum_key_down(uint8_t key);
void drum_key_up(uint8_t key);
void drum_key_up_independent(uint8_t key);
void drum_button_down(uint8_t button);
void drum_button_up(uint8_t button);
void drum_trill_down(float pos, float size);
void drum_trill_up();

#endif