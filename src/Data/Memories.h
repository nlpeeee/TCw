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

#ifndef TOUCHORD_MEMORIES_H
#define TOUCHORD_MEMORIES_H

#include <stdint.h>
#include <stdbool.h>
#include "Types.h"

// Song memories are preset slots 1-7 (0-6 here), one per green key: hold B0
// and tap a key to recall, keep it held to save. An empty one plays its
// factory starter setup.
#define MEMORY_COUNT 7
#define MEMORY_SAVE_HOLD_MS 2000

// A preset slot, or the starter setup for an empty memory slot.
bool memory_load(uint8_t slot, TouchordSettings* out);

// Core 0: switch to a memory, keeping MIDI and Bluetooth settings as they are.
void memory_recall(uint8_t slot);
// Core 0: save the current setup into a memory.
void memory_store(uint8_t slot);
// A short label for a setup, such as "Arp Dark 2".
void memory_describe(const TouchordSettings* s, char* out, int len);

// Boot: the setup as it was last played, else slot 1, else the defaults.
void memories_load_startup(TouchordSettings* out);
// Core 0 loop: saves the setup a few seconds after it last changed.
void autosave_task(void);

#endif
