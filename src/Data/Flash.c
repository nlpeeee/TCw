/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#include "Flash.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/multicore.h"
#include <string.h>

// Last 2 flash sectors hold one preset per page (16 presets/sector, 32 total)
#define FLASH_STORAGE_SECTORS 2
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - (FLASH_SECTOR_SIZE * FLASH_STORAGE_SECTORS))

#if TOUCHORD_BLE
// BTstack keeps Bluetooth pairings in its own flash bank; it must sit below the presets.
#include "pico/btstack_flash_bank.h"
_Static_assert(PICO_FLASH_BANK_STORAGE_OFFSET + PICO_FLASH_BANK_TOTAL_SIZE <= FLASH_TARGET_OFFSET,
               "Bluetooth pairing storage overlaps the preset sectors");
#endif

// One more sector, below the Bluetooth bank, keeps the last setup. Each save
// takes the next free page, so the sector is only erased every 16 saves and
// the sectors holding the saved presets are never touched.
#define FLASH_LAST_OFFSET (PICO_FLASH_SIZE_BYTES - (FLASH_SECTOR_SIZE * 5))
#define FLASH_LAST_PAGES  (FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE)
#if TOUCHORD_BLE
_Static_assert(FLASH_LAST_OFFSET + FLASH_SECTOR_SIZE <= PICO_FLASH_BANK_STORAGE_OFFSET,
               "Last-setup sector overlaps the Bluetooth pairing storage");
#endif

static const uint8_t* flash_preset_base(void)
{
    return (const uint8_t*)(XIP_BASE + FLASH_TARGET_OFFSET);
}

_Static_assert(FLASH_PRESET_COUNT * FLASH_PAGE_SIZE <= FLASH_SECTOR_SIZE * FLASH_STORAGE_SECTORS,
               "More preset slots than the preset sectors hold");

_Static_assert(sizeof(TouchordSettings) <= FLASH_PAGE_SIZE,
               "TouchordSettings exceeds flash page size");

bool flash_preset_exists(uint8_t slot)
{
    if (slot >= FLASH_PRESET_COUNT) return false;

    const TouchordSettings* preset =
        (const TouchordSettings*)(flash_preset_base() + slot * FLASH_PAGE_SIZE);
    return preset->magic == SETTINGS_MAGIC;
}

bool flash_load_preset(uint8_t slot, TouchordSettings* settings)
{
    if (slot >= FLASH_PRESET_COUNT) return false;

    const TouchordSettings* preset =
        (const TouchordSettings*)(flash_preset_base() + slot * FLASH_PAGE_SIZE);

    if (preset->magic != SETTINGS_MAGIC) return false;
    // Skip a preset with wrong layout
    if (preset->schema_version > SETTINGS_SCHEMA_VERSION) return false;

    uint16_t stored_size = preset->struct_size;
    if (stored_size == 0 || stored_size > sizeof(TouchordSettings))
        stored_size = sizeof(TouchordSettings);

    *settings = tc_app_default;
    memcpy(settings, preset, stored_size);

    settings->schema_version = SETTINGS_SCHEMA_VERSION;
    settings->struct_size    = sizeof(TouchordSettings);
    return true;
}

void flash_clear_all_presets(void)
{
    uint32_t ints = save_and_disable_interrupts();
    multicore_lockout_start_blocking();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE * FLASH_STORAGE_SECTORS);
    multicore_lockout_end_blocking();
    restore_interrupts(ints);
}

bool flash_save_preset(uint8_t slot, const TouchordSettings* settings)
{
    if (slot >= FLASH_PRESET_COUNT) return false;

    uint32_t sector = slot / (FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE);
    uint32_t sector_offset = FLASH_TARGET_OFFSET + (sector * FLASH_SECTOR_SIZE);

    // static (BSS) — 4KB on the 2KB per-core stack would hard-fault
    static uint8_t sector_buffer[FLASH_SECTOR_SIZE];
    memcpy(sector_buffer, (const uint8_t*)(XIP_BASE + sector_offset), FLASH_SECTOR_SIZE);

    uint32_t slot_in_sector = slot - (sector * (FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE));
    uint8_t *slot_dst = sector_buffer + (slot_in_sector * FLASH_PAGE_SIZE);
    memcpy(slot_dst, settings, sizeof(TouchordSettings));

    // Stamp current schema/size in case the caller passed a mutated copy
    TouchordSettings *stamped = (TouchordSettings *)slot_dst;
    stamped->magic          = SETTINGS_MAGIC;
    stamped->schema_version = SETTINGS_SCHEMA_VERSION;
    stamped->struct_size    = sizeof(TouchordSettings);

    uint32_t ints = save_and_disable_interrupts();
    multicore_lockout_start_blocking();
    flash_range_erase(sector_offset, FLASH_SECTOR_SIZE);
    flash_range_program(sector_offset, sector_buffer, FLASH_SECTOR_SIZE);
    multicore_lockout_end_blocking();
    restore_interrupts(ints);

    return true;
}

static const TouchordSettings* last_page(int page)
{
    return (const TouchordSettings*)(XIP_BASE + FLASH_LAST_OFFSET + page * FLASH_PAGE_SIZE);
}

// Newest written page in the last-setup sector, or -1 when it is empty.
static int last_newest(void)
{
    int newest = -1;
    for (int p = 0; p < FLASH_LAST_PAGES; p++)
        if (last_page(p)->magic == SETTINGS_MAGIC) newest = p;
    return newest;
}

bool flash_load_last(TouchordSettings* settings)
{
    int p = last_newest();
    if (p < 0) return false;
    const TouchordSettings* stored = last_page(p);
    if (stored->schema_version > SETTINGS_SCHEMA_VERSION) return false;

    uint16_t stored_size = stored->struct_size;
    if (stored_size == 0 || stored_size > sizeof(TouchordSettings))
        stored_size = sizeof(TouchordSettings);
    *settings = tc_app_default;
    memcpy(settings, stored, stored_size);
    settings->schema_version = SETTINGS_SCHEMA_VERSION;
    settings->struct_size    = sizeof(TouchordSettings);
    return true;
}

void flash_save_last(const TouchordSettings* settings)
{
    static uint8_t page_buffer[FLASH_PAGE_SIZE];
    memset(page_buffer, 0xFF, sizeof page_buffer);
    memcpy(page_buffer, settings, sizeof(TouchordSettings));
    TouchordSettings *stamped = (TouchordSettings *)page_buffer;
    stamped->magic          = SETTINGS_MAGIC;
    stamped->schema_version = SETTINGS_SCHEMA_VERSION;
    stamped->struct_size    = sizeof(TouchordSettings);

    int next = last_newest() + 1;
    // A page left half-written by a power cut can't be programmed over.
    if (next < FLASH_LAST_PAGES) {
        const uint8_t* page = (const uint8_t*)last_page(next);
        for (int i = 0; i < FLASH_PAGE_SIZE; i++)
            if (page[i] != 0xFF) { next = FLASH_LAST_PAGES; break; }
    }
    uint32_t ints = save_and_disable_interrupts();
    multicore_lockout_start_blocking();
    if (next >= FLASH_LAST_PAGES) {
        flash_range_erase(FLASH_LAST_OFFSET, FLASH_SECTOR_SIZE);
        next = 0;
    }
    flash_range_program(FLASH_LAST_OFFSET + next * FLASH_PAGE_SIZE, page_buffer, FLASH_PAGE_SIZE);
    multicore_lockout_end_blocking();
    restore_interrupts(ints);
}

void flash_clear_last(void)
{
    uint32_t ints = save_and_disable_interrupts();
    multicore_lockout_start_blocking();
    flash_range_erase(FLASH_LAST_OFFSET, FLASH_SECTOR_SIZE);
    multicore_lockout_end_blocking();
    restore_interrupts(ints);
}

// Loops live in their own sector below the last-setup one: one page per
// save, slots 0-6 for the memories and LOOP_SLOT_LIVE for the setup being
// played. The newest page for a slot wins. When the sector is full, the
// newest page of each slot is copied back after the erase.
#define FLASH_LOOP_OFFSET (PICO_FLASH_SIZE_BYTES - (FLASH_SECTOR_SIZE * 6))
#define FLASH_LOOP_PAGES  (FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE)
#define FLASH_LOOP_SLOTS  (LOOP_SLOT_LIVE + 1)
#define LOOP_MAGIC        0x4C4F4F50u   // "LOOP"
_Static_assert(FLASH_LOOP_OFFSET + FLASH_SECTOR_SIZE <= FLASH_LAST_OFFSET,
               "Loop sector overlaps the last-setup sector");

typedef struct {
    uint32_t magic;
    uint8_t  slot;
    uint8_t  reserved[3];
    LoopData loop;
} LoopPage;
_Static_assert(sizeof(LoopPage) <= FLASH_PAGE_SIZE, "LoopPage exceeds flash page size");

static const LoopPage* loop_page(int page)
{
    return (const LoopPage*)(XIP_BASE + FLASH_LOOP_OFFSET + page * FLASH_PAGE_SIZE);
}

static bool page_blank(int page)
{
    const uint8_t* p = (const uint8_t*)loop_page(page);
    for (int i = 0; i < FLASH_PAGE_SIZE; i++) if (p[i] != 0xFF) return false;
    return true;
}

// Newest page holding this slot, or -1.
static int loop_newest(uint8_t slot)
{
    int newest = -1;
    for (int p = 0; p < FLASH_LOOP_PAGES; p++) {
        const LoopPage* lp = loop_page(p);
        if (lp->magic == LOOP_MAGIC && lp->slot == slot) newest = p;
    }
    return newest;
}

bool flash_load_loop(uint8_t slot, LoopData* out)
{
    int p = loop_newest(slot);
    if (p < 0) return false;
    *out = loop_page(p)->loop;
    return true;
}

void flash_save_loop(uint8_t slot, const LoopData* loop)
{
    static uint8_t buffer[FLASH_PAGE_SIZE * FLASH_LOOP_SLOTS];
    memset(buffer, 0xFF, sizeof buffer);
    LoopPage* page = (LoopPage*)buffer;
    page->magic = LOOP_MAGIC;
    page->slot  = slot;
    memset(page->reserved, 0, sizeof page->reserved);
    page->loop  = *loop;

    // After the last page in use; a half-written page can't be programmed over.
    int next = 0;
    for (int p = 0; p < FLASH_LOOP_PAGES; p++)
        if (!page_blank(p)) next = p + 1;

    int pages = 1;
    bool erase = next >= FLASH_LOOP_PAGES;
    if (erase) {
        // Keep the newest page of every other slot.
        for (uint8_t s = 0; s < FLASH_LOOP_SLOTS; s++) {
            int p = loop_newest(s);
            if (s == slot || p < 0) continue;
            memcpy(buffer + pages * FLASH_PAGE_SIZE, loop_page(p), FLASH_PAGE_SIZE);
            pages++;
        }
        next = 0;
    }

    uint32_t ints = save_and_disable_interrupts();
    multicore_lockout_start_blocking();
    if (erase) flash_range_erase(FLASH_LOOP_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_LOOP_OFFSET + next * FLASH_PAGE_SIZE, buffer, pages * FLASH_PAGE_SIZE);
    multicore_lockout_end_blocking();
    restore_interrupts(ints);
}

void flash_clear_loops(void)
{
    uint32_t ints = save_and_disable_interrupts();
    multicore_lockout_start_blocking();
    flash_range_erase(FLASH_LOOP_OFFSET, FLASH_SECTOR_SIZE);
    multicore_lockout_end_blocking();
    restore_interrupts(ints);
}
