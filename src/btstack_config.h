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

// BTstack configuration for the BLE MIDI peripheral (TOUCHORD_BLE builds).
// Based on rppicomidi/pico-w-ble-midi-server-demo.

#ifndef TOUCHORD_BTSTACK_CONFIG_H
#define TOUCHORD_BTSTACK_CONFIG_H

#ifndef ENABLE_BLE
#error Please link to pico_btstack_ble
#endif

// Peripheral only: Touchord advertises, the host connects.
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LOG_ERROR
#define ENABLE_PRINTF_HEXDUMP

#define BLE_MIDI_SERVER_MAX_CONNECTIONS 1
#define MAX_NR_GATT_CLIENTS 0

// Buffers
#define HCI_OUTGOING_PRE_BUFFER_SIZE 4
#define HCI_ACL_PAYLOAD_SIZE (255 + 4)
#define HCI_ACL_CHUNK_SIZE_ALIGNMENT 4
#define MAX_NR_HCI_CONNECTIONS 1
#define MAX_NR_SM_LOOKUP_ENTRIES 3
#define MAX_NR_WHITELIST_ENTRIES 16
#define MAX_NR_LE_DEVICE_DB_ENTRIES 16

// Limit ACL/SCO buffers so the CYW43 shared bus doesn't overrun
#define MAX_NR_CONTROLLER_ACL_BUFFERS 3
#define MAX_NR_CONTROLLER_SCO_PACKETS 3

// Controller-to-host flow control, also to protect the CYW43 shared bus
#define ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL
#define HCI_HOST_ACL_PACKET_LEN (255 + 4)
#define HCI_HOST_ACL_PACKET_NUM 3
#define HCI_HOST_SCO_PACKET_LEN 120
#define HCI_HOST_SCO_PACKET_NUM 3

// Pairings (bonds) live in flash via the TLV store; see PICO_FLASH_BANK_STORAGE_OFFSET
#define NVM_NUM_DEVICE_DB_ENTRIES 16
#define NVM_NUM_LINK_KEYS 16

// No malloc, so a fixed-size ATT DB
#define MAX_ATT_DB_SIZE 512

// HAL
#define HAVE_EMBEDDED_TIME_MS
#define HAVE_ASSERT
#define HCI_RESET_RESEND_TIMEOUT_MS 1000
#define ENABLE_SOFTWARE_AES128
#define ENABLE_MICRO_ECC_FOR_LE_SECURE_CONNECTIONS

#endif
