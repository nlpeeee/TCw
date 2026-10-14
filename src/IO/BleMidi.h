/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#ifndef TOUCHORD_BLE_MIDI_H
#define TOUCHORD_BLE_MIDI_H

#include <stdint.h>
#include <stdbool.h>

// Bluetooth LE MIDI on the Pico W / Pico 2 W radio (CYW43).
// Every function must be called from core 0: the CYW43 driver and BTstack
// run in a polled async context that is not safe to touch from core 1.

#if TOUCHORD_BLE

// Bring up the radio. Call once from core 0, after core 1 has called
// multicore_lockout_victim_init() (BTstack keeps pairings in flash).
bool ble_midi_init(void);

// Service the radio and BTstack, and follow tc_app.bluetooth on/off.
void ble_midi_task(void);

void ble_midi_send(const uint8_t *data, uint8_t len);
bool ble_midi_is_connected(void);

void ble_midi_led_write(bool on);

#else

static inline bool ble_midi_init(void) { return false; }
static inline void ble_midi_task(void) {}
static inline void ble_midi_send(const uint8_t *data, uint8_t len) { (void)data; (void)len; }
static inline bool ble_midi_is_connected(void) { return false; }
static inline void ble_midi_led_write(bool on) { (void)on; }

#endif

#endif
