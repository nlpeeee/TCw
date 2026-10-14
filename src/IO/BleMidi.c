/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#include "BleMidi.h"
#include "Globals.h"
#include "Rendering/Graphics.h"

#include "pico/cyw43_arch.h"
#include "btstack.h"
#include "ble_midi_server.h"
#include "touchord_gatt.h"   // profile_data, generated from touchord_gatt.gatt

static const uint8_t scan_resp_data[] = {
    1 + BLE_DEVICE_NAME_LEN, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME,
    'T', 'o', 'u', 'c', 'h', 'o', 'r', 'd',
};
_Static_assert(sizeof scan_resp_data == 2 + BLE_DEVICE_NAME_LEN, "scan response name length");

static bool radio_ready  = false;
static bool was_connected = false;

bool ble_midi_init(void)
{
    // Also loads the BT firmware and starts BTstack (CYW43_ENABLE_BLUETOOTH)
    radio_ready = cyw43_arch_init() == 0;
    return radio_ready;
}

static void ble_midi_server_start(void)
{
    // Just Works pairing with bonding, so hosts reconnect without asking again.
    ble_midi_server_init(profile_data, scan_resp_data, sizeof scan_resp_data,
                         IO_CAPABILITY_NO_INPUT_NO_OUTPUT,
                         SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);
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

    // Touchord has no MIDI input; drain it so the receive buffer never fills.
    uint8_t  in[3];
    uint16_t ts;
    while (ble_midi_server_stream_read(sizeof in, in, &ts) > 0) {}

    bool connected = ble_midi_server_is_connected();
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
