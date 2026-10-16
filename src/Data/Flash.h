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

#ifndef TOUCHORD_FLASH_STORAGE_H
#define TOUCHORD_FLASH_STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "Types.h"
#include "Globals.h"
#include "Timing/Looper.h"

#define FLASH_PRESET_COUNT 32

// Load preset from slot
bool flash_load_preset(uint8_t slot, TouchordSettings* settings);

// Save settings to preset slot
bool flash_save_preset(uint8_t slot, const TouchordSettings* settings);

// Check if preset slot has a valid preset
bool flash_preset_exists(uint8_t slot);

// Erase every preset slot.
void flash_clear_all_presets(void);

// The setup as it was last played, saved automatically (see Data/Memories.c).
bool flash_load_last(TouchordSettings* settings);
void flash_save_last(const TouchordSettings* settings);
void flash_clear_last(void);

// Loops: slots 0-6 are the song memories, LOOP_SLOT_LIVE the setup being played.
// A saved empty loop (length 0) still counts as saved.
bool flash_load_loop(uint8_t slot, LoopData* out);
void flash_save_loop(uint8_t slot, const LoopData* loop);
void flash_clear_loops(void);

#endif
