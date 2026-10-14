/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#include "Midi.h"
#include "BleMidi.h"
#include "Timing/MidiClock.h"
#include "tusb.h"
#include "hardware/uart.h"
#include "pico/util/queue.h"
#include <string.h>

// Every outgoing message goes through this queue and is sent by core 0 in
// midi_out_task(). Modes send from both cores (keys on core 0, trill on
// core 1), but TinyUSB, the TRS UART and BTstack must not be driven from
// two cores at once.
typedef struct {
    uint8_t len;
    uint8_t data[3];
} MidiMsg;

#define MIDI_OUT_QUEUE_LEN    256
#define MIDI_OUT_CORE1_WAIT_US 50000

static queue_t midi_out_queue;

void midi_out_init(void)
{
    queue_init(&midi_out_queue, sizeof(MidiMsg), MIDI_OUT_QUEUE_LEN);
}

static void midi_out_emit(const MidiMsg *m)
{
    tud_midi_stream_write(0, m->data, m->len);
    send_trs_midi((uint8_t *)m->data, m->len);
    ble_midi_send(m->data, m->len);
}

void midi_out_task(void)
{
    MidiMsg m;
    while (queue_try_remove(&midi_out_queue, &m))
        midi_out_emit(&m);
}

void midi_in_task(void)
{
    uint8_t pkt[4];
    while (tud_midi_available() && tud_midi_packet_read(pkt)) {
        // Real-time messages are one byte, so they never share a packet.
        if (pkt[1] >= 0xF8) midi_clock_rx(CLOCK_SRC_USB, pkt[1], time_us_64());
    }
}

static void midi_out_send(const uint8_t *data, uint8_t len)
{
    MidiMsg m = { .len = len };
    memcpy(m.data, data, len);
    if (queue_try_add(&midi_out_queue, &m)) return;

    if (get_core_num() == 0) {
        // Core 0 is the consumer, so make room rather than wait on itself.
        midi_out_task();
        queue_add_blocking(&midi_out_queue, &m);
        return;
    }
    // Core 1 may hold tc_app_lock while core 0 waits on it, so never block
    // forever here; drop the message if core 0 can't drain in time.
    absolute_time_t until = make_timeout_time_us(MIDI_OUT_CORE1_WAIT_US);
    while (!queue_try_add(&midi_out_queue, &m))
        if (time_reached(until)) return;
}

void switch_midi_trs(MidiType type)
{
    //Toggles the TS5A23157DGSR
    switch(type)
    {
        case MIDI_TRS_A:
        gpio_put(MIDI_PIN_IN_1, 0);
        gpio_put(MIDI_PIN_IN_2, 0);
        break;
        case MIDI_TRS_B:
        gpio_put(MIDI_PIN_IN_1, 1);
        gpio_put(MIDI_PIN_IN_2, 1);
        break;
    }
}
void setup_midi_trs(MidiType type)
{
    gpio_init(MIDI_PIN_IN_1);
    gpio_init(MIDI_PIN_IN_2);
    gpio_set_dir(MIDI_PIN_IN_1, GPIO_OUT);
    gpio_set_dir(MIDI_PIN_IN_2, GPIO_OUT);

    switch_midi_trs(type);

    uart_init(uart1, MIDI_BAUD_RATE);
    gpio_set_function(MIDI_PIN_DAT, GPIO_FUNC_UART); //MIDI_PIN_DAT is TX pin
    uart_set_hw_flow(uart1, false, false);
    uart_set_format(uart1, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(uart1, true); // 32-byte FIFO: a chord no longer blocks core 0 byte by byte
}

void send_trs_midi(uint8_t* data, uint8_t len)
{
    for(int i = 0; i < len; i++)
    {
        uart_putc_raw(uart1, data[i]);
    }
}

void send_midi_chord(uint8_t channel, uint8_t status, uint8_t* notes, uint8_t length, uint8_t velocity)
{
    for(int i = 0; i < length; i++)
    {
        send_midi_note(channel, status, notes[i], velocity);
    }
}

void send_midi_chord_diff(const uint8_t *prev, const uint8_t *curr,
                          uint8_t channel, uint8_t velocity, bool retrigger)
{
    if (retrigger) {
        for (int i = 0; i < MAX_CHORD; i++)
            if (prev[i]) send_midi_note(channel, NOTE_OFF, prev[i], velocity);
        for (int i = 0; i < MAX_CHORD; i++)
            if (curr[i]) send_midi_note(channel, NOTE_ON,  curr[i], velocity);
        return;
    }
    for (int i = 0; i < MAX_CHORD; i++) {
        uint8_t p = prev[i];
        if (p == 0) continue;
        bool kept = false;
        for (int j = 0; j < MAX_CHORD; j++)
            if (curr[j] == p) { kept = true; break; }
        if (!kept) send_midi_note(channel, NOTE_OFF, p, velocity);
    }
    for (int i = 0; i < MAX_CHORD; i++) {
        uint8_t c = curr[i];
        if (c == 0) continue;
        bool was = false;
        for (int j = 0; j < MAX_CHORD; j++)
            if (prev[j] == c) { was = true; break; }
        if (!was) send_midi_note(channel, NOTE_ON, c, velocity);
    }
}

void send_midi_note(uint8_t channel, uint8_t status, uint8_t note, uint8_t velocity) {
    uint8_t packet[3] = {
        status | (channel & 0x0F),
        note,
        velocity
    };
    midi_out_send(packet, 3);
}

void send_midi_cc(uint8_t channel, uint8_t cc_num, uint8_t cc_value) {
    uint8_t status = 0xB0 | (channel & 0x0F); 
    uint8_t packet[3] = {
        status,
        cc_num,
        cc_value
    };
    midi_out_send(packet, 3);
}

void send_poly_aftertouch(uint8_t channel, uint8_t note, uint8_t pressure) {
    uint8_t status = 0xA0 | (channel & 0x0F);

    uint8_t packet[3] = {
        status,
        note,
        pressure
    };
    midi_out_send(packet, 3);
}

void send_aftertouch(uint8_t channel, uint8_t pressure)
{
    uint8_t status = 0xD0 | (channel & 0x0F);

    uint8_t packet[2] = {
        status,
        pressure
    };
    midi_out_send(packet, 2);
}