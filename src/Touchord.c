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

#include <stdio.h>
#include <string.h>
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "Timing/Quantize.h"
#include "pico/bootrom.h"
#include "pico/time.h"

#include "Defines.h"
#include "Types.h"
#include "Helper.h"
#include "Globals.h"
#include "IO/Midi.h"
#include "IO/BleMidi.h"
#include "Timing/Scheduler.h"
#include "Timing/MidiClock.h"
#include "Timing/Looper.h"
#include "Data/Flash.h"
#include "Data/Memories.h"
#include "Notes/Note.h"
#include "KeyGestures.h"
#include "Sync.h"
#include "Rendering/Graphics.h"
#include "Rendering/ScreenDma.h"

#include "bsp/board.h"
#include "tusb.h"

#include "Modes/Compose.h"
#include "Modes/Arp.h"
#include "Modes/Perform.h"
#include "Modes/Strum.h"
#include "Modes/Omni.h"
#include "Modes/Drum.h"
#include "Modes/Settings.h"

void led_blinking_task(void);
static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

#define BUTTON_POLL_MS 10

// Holding B0 this long jumps to Settings; a shorter press fires on release.
#define B0_HOLD_MS 600

// B0 + B2 + B4 held this long drops into BOOTSEL, so a stray press can't.
#define BOOTSEL_HOLD_MS 3000

// Set by core 1 once it can be paused for flash writes (see io_task).
static volatile bool core1_lockout_ready = false;

// B0 cycles modes on a short press and opens Settings when held. Its press
// is held back until release (or the hold time) so the two don't collide,
// and a hold that slides on the bar to pick a mode does neither.
static uint64_t b0_down_time = 0;
static bool b0_held = false;

static void poll_b0(bool curr_state, uint64_t now)
{
    if (tc_control_states[0] && !curr_state) {
        b0_down_time = now;
        b0_held = false;
        tc_button_slid[0] = false;
    } else if (!tc_control_states[0] && curr_state) {
        if (!b0_held && !tc_button_slid[0]) tc_button_down(0);
        tc_button_up(0);
    } else if (!curr_state && !b0_held && !tc_button_slid[0]
               && now - b0_down_time >= B0_HOLD_MS
               && tc_control_states[2] && tc_control_states[4]) {
        // Not while B2 and B4 are down, so the BOOTSEL hold stays where it is.
        b0_held = true;
        if (tc_app_get_mode() != TOUCHORD_SETTINGS)
            tc_app_set_mode(TOUCHORD_SETTINGS);
    }
}

// Hold B0 + B2 + B4 to drop into BOOTSEL, with a countdown on screen.
static void poll_bootsel_combo(uint64_t now)
{
    static uint64_t combo_start = 0;
    static int shown = -1;

    if (tc_control_states[0] || tc_control_states[2] || tc_control_states[4]) {
        if (shown >= 0) trigger_overlay("", 0);
        combo_start = 0;
        shown = -1;
        return;
    }
    if (combo_start == 0) combo_start = now;
    // Letting go afterwards neither changes mode nor octave.
    b0_held = true;
    tc_button_slid[2] = true;
    uint64_t held = now - combo_start;
    if (held >= BOOTSEL_HOLD_MS) {
        tc_trigger_bootsel = true;
        return;
    }
    int left = (int)((BOOTSEL_HOLD_MS - held + 999) / 1000);
    if (left != shown) {
        char msg[24];
        snprintf(msg, sizeof msg, "Firmware mode in %d", left);
        trigger_overlay(msg, 1100);
        shown = left;
    }
}

// Hold B0 and tap a green key to recall that song memory; keep the key
// held to save into it instead. These keys never reach the mode.
static int8_t memory_key = -1;
static uint64_t memory_key_time = 0;
static bool memory_saved = false;

static bool memory_key_down(int key, uint64_t now)
{
    if (memory_key >= 0) return true;   // a second key during one is ignored
    if (tc_control_states[0] || tc_app_get_mode() == TOUCHORD_SETTINGS) return false;
    tc_button_slid[0] = true;           // B0's tap and Settings hold are spent
    memory_key = (int8_t)key;
    memory_key_time = now;
    memory_saved = false;
    trigger_overlay("Hold to save", MEMORY_SAVE_HOLD_MS);
    return true;
}

static bool memory_key_up(int key)
{
    if (memory_key < 0) return false;
    if (key != memory_key) return true;
    if (!memory_saved) memory_recall((uint8_t)key);
    memory_key = -1;
    return true;
}

static void memory_key_poll(uint64_t now)
{
    if (memory_key < 0 || memory_saved) return;
    if (now - memory_key_time >= MEMORY_SAVE_HOLD_MS) {
        memory_store((uint8_t)memory_key);
        memory_saved = true;
    }
}

// Tap B4 to record, loop, stop and play again; hold it to clear the loop.
// A hold that touches the bar picks a chord set instead and does neither,
// and so does a press held past the tap time.
#define B4_TAP_MS        600
#define LOOP_CLEAR_HOLD_MS 2000
static uint64_t b4_down_time = 0;
static bool b4_spent = false;
static int  b4_shown = -1;

static void poll_b4(bool was_up, bool is_up, uint64_t now)
{
    if (was_up && !is_up) {
        b4_down_time = now;
        b4_shown = -1;
        // Not as part of another button's hold, or in Settings.
        b4_spent = tc_app_get_mode() == TOUCHORD_SETTINGS;
        for (int i = 0; i < NUM_CONTROLS; i++)
            if (i != 4 && !tc_control_states[i]) b4_spent = true;
        return;
    }
    if (!was_up && is_up) {
        if (b4_shown >= 0) trigger_overlay("", 0);
        if (!b4_spent && !tc_button_slid[4] && now - b4_down_time < B4_TAP_MS)
            looper_tap();
        return;
    }
    if (is_up || b4_spent) return;
    if (tc_button_slid[4]) {
        // Picking a chord set after all: drop the countdown.
        if (b4_shown >= 0) trigger_overlay("", 0);
        b4_shown = -1;
        return;
    }
    for (int i = 0; i < NUM_CONTROLS; i++)
        if (i != 4 && !tc_control_states[i]) { b4_spent = true; return; }   // e.g. the BOOTSEL hold
    if (!looper_has_loop()) return;

    uint64_t held = now - b4_down_time;
    if (held < B4_TAP_MS) return;
    if (held >= LOOP_CLEAR_HOLD_MS) {
        looper_clear();
        b4_spent = true;
        trigger_overlay("Loop cleared", 1000);
        return;
    }
    int left = (int)((LOOP_CLEAR_HOLD_MS - held + 999) / 1000);
    if (left != b4_shown) {
        char msg[24];
        snprintf(msg, sizeof msg, "Clear loop in %d", left);
        trigger_overlay(msg, 1100);
        b4_shown = left;
    }
}

void poll_buttons()
{
    uint64_t now = to_ms_since_boot(get_absolute_time());

    for (int i = 0; i < NUM_CONTROLS; i++) {
        bool curr_state = gpio_get(CONTROL_PIN(i));
        if (i == 4) poll_b4(tc_control_states[4], curr_state, now);
        if (i == 0) {
            poll_b0(curr_state, now);
        } else if (tc_control_states[i] && !curr_state) {
            tc_button_slid[i] = false;
            // B1 and B2 also hold and slide, so their tap fires on release.
            if (i > 2) tc_button_down(i);
        } else if (!tc_control_states[i] && curr_state) {
            if (i <= 2 && !tc_button_slid[i]) tc_button_down(i);
            tc_button_up(i);
        }
        tc_control_states[i] = curr_state;
    }

    poll_bootsel_combo(now);

    for (int i = 0; i < NUM_KEYS; i++) {
        bool curr_state = gpio_get(i + KEY_0);
        if (tc_key_states[i] && !curr_state) {
            if (!memory_key_down(i, now)) {
                tc_last_key = i;
                quant_key_down(i);
            }
        } else if (!tc_key_states[i] && curr_state) {
            if (!memory_key_up(i))
                quant_key_up(i);
        }
        tc_key_states[i] = curr_state;
    }
    memory_key_poll(now);
}

void poll_trill_bar(TrillBar* bar)
{
    trill_read(bar);
    float pos  = trill_calculate_touch(bar);
    float size = trill_calculate_size(bar, pos);

    bool curr_state = pos >= 0.0f;
    if (curr_state) {
        const float DEAD_HIGH = 0.04f;
        pos = pos / (1.0f - DEAD_HIGH);
        if (pos > 1.0f) pos = 1.0f;
    }

    if (key_gesture_poll(pos, curr_state)) {
        // Shown to the mode as no touch, so it picks the bar up afresh afterwards.
        tc_touch_state = false;
        return;
    }

    if (curr_state) {
        tc_trill_pos = pos;
        tc_trill_strength = trill_calculate_strength(bar);
        tc_trill_down(pos, size);
    } else if (tc_touch_state) {
        tc_trill_up();
    }
    tc_touch_state = curr_state;
}

static void init_input_bank(int base, int count, bool *states)
{
    for (int i = 0; i < count; i++) {
        gpio_init(base + i);
        gpio_set_dir(base + i, GPIO_IN);
        gpio_pull_up(base + i);
        states[i] = true;
    }
}

void init_GPIO()
{
    init_input_bank(CONTROL_0, NUM_CONTROLS, tc_control_states);
    init_input_bank(KEY_0,     NUM_KEYS,     tc_key_states);
#ifndef CYW43_WL_GPIO_LED_PIN
    // On the W boards GPIO25 is the radio's SPI chip select, not the LED.
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
#endif
}

void init_i2c()
{
    // Trill bar's max I2C is 400 kHz
    i2c_init(TRILL_I2C, 400000);
    gpio_set_function(TRILL_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(TRILL_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(TRILL_PIN_SDA);
    gpio_pull_up(TRILL_PIN_SCL);

#if TOUCHORD_DUAL_I2C
    // Screen owns its own bus, so push to 1 MHz
    i2c_init(DISP_I2C, 1000000);
    gpio_set_function(DISP_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(DISP_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(DISP_PIN_SDA);
    gpio_pull_up(DISP_PIN_SCL);
#endif
}

TouchordMode prevMode = TOUCHORD_COMPOSE;

static void start_mode(TouchordMode mode);

void select_mode(TouchordMode mode)
{
    switch (prevMode) {
        case TOUCHORD_COMPOSE:  compose_end();  break;
        case TOUCHORD_ARP:      arp_end();      break;
        case TOUCHORD_STRUM:    strum_end();    break;
        case TOUCHORD_PERFORM:  perform_end();  break;
        case TOUCHORD_OMNI:     omni_end();     break;
        case TOUCHORD_DRUM:     drum_end();     break;
        case TOUCHORD_SETTINGS: settings_end(); break;
    }
    start_mode(mode);
}

static void start_mode(TouchordMode mode)
{
    switch (mode) {
        case TOUCHORD_COMPOSE:  compose_start();  break;
        case TOUCHORD_ARP:      arp_start();      break;
        case TOUCHORD_STRUM:    strum_start();    break;
        case TOUCHORD_PERFORM:  perform_start();  break;
        case TOUCHORD_OMNI:     omni_start();     break;
        case TOUCHORD_DRUM:     drum_start();     break;
        case TOUCHORD_SETTINGS: settings_start(); break;
    }
}

void io_task()
{
    multicore_lockout_victim_init();
    core1_lockout_ready = true;

    init_i2c();
    setup_midi_trs(tc_app.midi_type);

    tc_bar = trill_init(TRILL_I2C, TRILL_ADDR);
    trill_set_auto_scan(&tc_bar, 1);
    trill_set_noise_threshold(&tc_bar, 255);

    tc_disp.external_vcc = false;
    ssd1306_init(&tc_disp, 128, 64, DISP_ADDR, DISP_I2C);
    ssd1306_contrast(&tc_disp, 0xFF);
    screen_dma_init();

    anim_boot_intro();
    ssd1306_clear(&tc_disp);

    while (tc_running) {
        if (tc_trigger_bootsel) {
            tc_running = false;
            ssd1306_clear(&tc_disp);
            ssd1306_draw_string(&tc_disp, 10, 24, 2, "Firm Mode");
            ssd1306_show(&tc_disp);
            rom_reset_usb_boot(0, 0);
            break;
        }

        TouchordMode currentMode = tc_app_get_mode();
        if (tc_mode_restart) {
            // A song memory swapped the setup: stop the old mode's notes and
            // start the new one afresh, with a transition only if it changed.
            tc_mode_restart = false;
            if (currentMode == prevMode) {
                select_mode(currentMode);
                continue;
            }
        }
        if (currentMode != prevMode) {
            static uint8_t prev_fb[128 * 8];
            static uint8_t next_fb[128 * 8];
            memcpy(prev_fb, tc_disp.buffer, sizeof prev_fb);

            select_mode(currentMode);
            ssd1306_clear(&tc_disp);
            tc_draw();
            memcpy(next_fb, tc_disp.buffer, sizeof next_fb);

            int direction = (currentMode == TOUCHORD_SETTINGS || prevMode == TOUCHORD_SETTINGS)
                            ? 0
                            : (tc_mode_position(currentMode) > tc_mode_position(prevMode) ? 1 : -1);

            anim_transition(prev_fb, next_fb, direction);
            prevMode = currentMode;
            continue;
        }

        ssd1306_clear(&tc_disp);
        if (!tc_connected) {
            draw_connect_screen(tc_app.bluetooth);
            draw_overlay();
            ssd1306_show(&tc_disp);
            sleep_ms(20);
            continue;
        }
        tc_draw();
        tc_update();
        looper_draw_badge();
        draw_trill_indicator();
        key_gesture_draw();
        key_gesture_draw_hint();
        draw_overlay();

#if TOUCHORD_DUAL_I2C
        screen_show_async(&tc_disp);
        do {
            poll_trill_bar(&tc_bar);
        } while (screen_show_busy());
        screen_show_wait();
#else
        for (int i = 0; i < 5; i++) poll_trill_bar(&tc_bar);
        ssd1306_show(&tc_disp);
#endif
    }
}

int main()
{
    board_init();
    init_GPIO();
    sleep_ms(500);

    if (!gpio_get(CONTROL_PIN(0)))
        rom_reset_usb_boot(0, 0);

    tc_app = tc_app_default;
    tc_app_working = tc_app_default;
    memories_load_startup(&tc_app);
    // Boot into the mode that was saved, but never into Settings
    if (tc_app.mode >= TOUCHORD_SETTINGS) tc_app.mode = TOUCHORD_COMPOSE;
    prevMode = tc_app.mode;

    tc_sync_init();
    sched_init();
    reload_custom_scales();
    start_mode(tc_app.mode);

    midi_out_init();
    multicore_launch_core1(io_task);

    // The radio is brought up before USB so the host never waits on an
    // attached-but-silent device. BTstack may write its pairing store during
    // bring-up, and flash writes need core 1 pausable first.
    while (!core1_lockout_ready) tight_loop_contents();
    ble_midi_init();

    tud_init(0);

    absolute_time_t next_button_poll = get_absolute_time();
    while (true) {
        tud_task();
        midi_in_task();
        ble_midi_task();
        midi_clock_task();

        // Keys and buttons only play while something is listening; until
        // then the screen asks for a connection.
        tc_connected = tud_midi_mounted() || ble_midi_is_connected();
        if (tc_connected && time_reached(next_button_poll)) {
            poll_buttons();
            next_button_poll = make_timeout_time_ms(BUTTON_POLL_MS);
        }

        autosave_task();
        quant_task();
        looper_task();
        arp_task();
        sched_task();
        midi_out_task();
        led_blinking_task();
    }
}

void tud_mount_cb(void)               { blink_interval_ms = BLINK_MOUNTED; }
void tud_umount_cb(void)              { blink_interval_ms = BLINK_NOT_MOUNTED; }
void tud_suspend_cb(bool remote)      { (void)remote; blink_interval_ms = BLINK_SUSPENDED; }
void tud_resume_cb(void)              { blink_interval_ms = BLINK_MOUNTED; }

void led_blinking_task(void)
{
    static uint32_t start_ms = 0;
    static bool led_state = false;

    uint32_t interval = blink_interval_ms;
    if (interval == BLINK_NOT_MOUNTED && ble_midi_is_connected()) interval = BLINK_MOUNTED;

    if (board_millis() - start_ms < interval) return;
    start_ms += interval;

#ifdef CYW43_WL_GPIO_LED_PIN
    ble_midi_led_write(led_state);
#else
    board_led_write(led_state);
#endif
    led_state = !led_state;
}
