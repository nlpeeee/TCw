/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

// Config for lib/ring_buffer_lib, used by the BLE MIDI library.

#ifndef TOUCHORD_RING_BUFFER_LIB_CONFIG_H
#define TOUCHORD_RING_BUFFER_LIB_CONFIG_H

#include <stdint.h>

// BLE MIDI buffers are larger than 255 bytes
#define RING_BUFFER_SIZE_TYPE uint16_t

// Only core 0 touches the BLE buffers
#define RING_BUFFER_MULTICORE_SUPPORT 0

#endif
