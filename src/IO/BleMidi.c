/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#include "BleMidi.h"
#include <stdio.h>
#include "Globals.h"
#include "Rendering/Graphics.h"
#include "Timing/MidiClock.h"

#include "pico/cyw43_arch.h"
#include "btstack.h"
#include "ble_midi_server.h"
#include "touchord_gatt.h"   // profile_data, generated from touchord_gatt.gatt

static const uint8_t scan_resp_data[] = {
    1 + BLE_DEVICE_NAME_LEN, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME,
    'T', 'o', 'u', 'c', 'h', 'o', 'r', 'd',
};
_Static_assert(sizeof scan_resp_data == 2 + BLE_DEVICE_NAME_LEN, "scan response name length");

static volatile bool radio_ready  = false;
static bool was_connected = false;

// Diagnostics shown in Settings > MIDI > Bluetooth (written on core 0, read on core 1)
static volatile int     radio_init_err = 0;
static volatile uint8_t hci_state      = HCI_STATE_OFF;
static volatile int     adv_status     = -1;   // -1: advertising never enabled yet
static volatile bool    connected_now  = false;

static btstack_packet_callback_registration_t diag_registration;

static void diag_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    (void)channel; (void)size;
    if (packet_type != HCI_EVENT_PACKET) return;
    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            hci_state = btstack_event_state_get_state(packet);
            break;
        case HCI_EVENT_COMMAND_COMPLETE:
            if (hci_event_command_complete_get_command_opcode(packet) == HCI_OPCODE_HCI_LE_SET_ADVERTISE_ENABLE)
                adv_status = hci_event_command_complete_get_return_parameters(packet)[0];
            break;
    }
}

bool ble_midi_init(void)
{
    // Also loads the BT firmware and starts BTstack (CYW43_ENABLE_BLUETOOTH)
    radio_init_err = cyw43_arch_init();
    radio_ready = radio_init_err == 0;
    if (radio_ready) {
        diag_registration.callback = &diag_packet_handler;
        hci_add_event_handler(&diag_registration);
    }
    return radio_ready;
}

void ble_midi_status(char *buf, uint32_t len)
{
    static const char *state_names[] = { "Off", "Starting", "Ready", "Stopping", "Sleeping", "Sleeping" };
    if (!radio_ready)            snprintf(buf, len, "Radio err %d", radio_init_err);
    else if (connected_now)      snprintf(buf, len, "Connected");
    else if (adv_status == 0)    snprintf(buf, len, "Advertising");
    else if (adv_status > 0)     snprintf(buf, len, "Adv err %02X", adv_status);
    else if (hci_state < 6)      snprintf(buf, len, "BT %s", state_names[hci_state]);
    else                         snprintf(buf, len, "BT state %u", hci_state);
}

static void ble_midi_server_start(void)
{
    // Just Works pairing with bonding, so hosts reconnect without asking again.
    ble_midi_server_init(profile_data, scan_resp_data, sizeof scan_resp_data,
                         IO_CAPABILITY_NO_INPUT_NO_OUTPUT,
                         SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);
}

// BLE packets bunch up per connection interval, so arrival times are too
// rough for tempo. The sender's 13-bit millisecond timestamps are unwrapped
// into a running timeline instead.
static uint64_t ble_rx_time_us(uint16_t ts)
{
    static uint64_t t_ms = 0;
    static uint16_t last_ts = 0;
    t_ms += (uint16_t)(ts - last_ts) & 0x1FFF;
    last_ts = ts;
    return t_ms * 1000;
}

void ble_midi_task(void)
{
    if (!radio_ready) return;

    cyw43_arch_poll();

    // ble_midi_server_deinit() only clears "initialized" once the controller
    // reports HCI_STATE_OFF, so toggling quickly settles on the next passes.
    bool want_on = tc_app.bluetooth;
    bool is_on   = ble_midi_server_is_initialized();
    if (want_on && !is_on)       ble_midi_server_start();
    else if (!want_on && is_on)  ble_midi_server_deinit();

    // Only clock and transport are used; everything else is drained so the
    // receive buffer never fills.
    uint8_t  in[3];
    uint16_t ts;
    while (ble_midi_server_stream_read(sizeof in, in, &ts) > 0)
        if (in[0] >= 0xF8) midi_clock_rx(CLOCK_SRC_BLE, in[0], ble_rx_time_us(ts));

    bool connected = ble_midi_server_is_connected();
    connected_now = connected;
    if (connected != was_connected) {
        trigger_overlay(connected ? "BT Connected" : "BT Disconnected", 1000);
        was_connected = connected;
    }
}

void ble_midi_send(const uint8_t *data, uint8_t len)
{
    if (radio_ready) ble_midi_server_stream_write(len, data);
}

bool ble_midi_is_connected(void)
{
    return radio_ready && ble_midi_server_is_connected();
}

void ble_midi_led_write(bool on)
{
    // On the W boards GPIO25 is the radio's chip select; the LED hangs off the CYW43.
    if (radio_ready) cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
}
