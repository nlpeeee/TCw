/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#ifndef TOUCHORD_QUANTIZE_H
#define TOUCHORD_QUANTIZE_H

#include <stdint.h>
#include <stdbool.h>

// Settings > MIDI > Quantize: a key press in Compose, Drum or Arp waits for
// the next grid point before the mode sees it.
#define QUANT_GRID_COUNT 5
extern const char *quant_grid_names[QUANT_GRID_COUNT];

// Core 0 key scan: use these instead of calling the mode's key handlers.
void quant_key_down(uint8_t key);
void quant_key_up(uint8_t key);

// Core 0 main loop: hand over presses whose grid point has come.
void quant_task(void);

// Core 1 draw: a small "Q 1/8" mark, blinking while a press waits.
void quant_draw_marker(void);

#endif
