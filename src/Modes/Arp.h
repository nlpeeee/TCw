/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#ifndef TOUCHORD_ARP_H
#define TOUCHORD_ARP_H

#include <stdio.h>
#include "pico/stdlib.h"
#include "Defines.h"

#define ARP_PATTERN_COUNT 5
#define ARP_RATE_COUNT    6
#define ARP_OCTAVES_MAX   4
#define ARP_GATE_COUNT    4

extern const char *arp_pattern_names[ARP_PATTERN_COUNT];
extern const char *arp_rate_names[ARP_RATE_COUNT];
extern const char *arp_octave_names[ARP_OCTAVES_MAX];
extern const char *arp_gate_names[ARP_GATE_COUNT];

void arp_start();
void arp_end();
void arp_draw();
void arp_update();
void arp_key_down(uint8_t key);
void arp_key_up(uint8_t key);
void arp_key_up_independent(uint8_t key);
void arp_button_down(uint8_t button);
void arp_button_double_down(uint8_t button);
void arp_button_up(uint8_t button);
void arp_trill_down(float pos, float size);
void arp_trill_up();

// Core 0 main loop: play the next step when it is due.
void arp_task(void);

#endif
