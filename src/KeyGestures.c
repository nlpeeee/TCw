/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#include "KeyGestures.h"
#include "Globals.h"
#include "Sync.h"
#include "IO/Midi.h"
#include "Timing/Scheduler.h"
#include "Notes/Note.h"
#include "Notes/ChordSets.h"
#include "Rendering/Graphics.h"
#include "ssd1306.h"
#include "font.h"
#include <stdio.h>
#include <string.h>
#include "pico/time.h"

typedef enum { GESTURE_NONE = 0, GESTURE_SIZE, GESTURE_CHORDS, GESTURE_KEY } Gesture;

#define KEY_ZONES  12
#define SIZE_ZONES 3
#define SIZE_MIN   3        // Simple = 3 notes, Rich = 4, Lush = 5
#define WARM_MIN_SIZE 4     // Warm sets are written as sevenths and ninths
#define PREVIEW_US 250000

static const Note key_roots[KEY_ZONES] = {
    NOTE_C, NOTE_D_FLAT, NOTE_D, NOTE_E_FLAT, NOTE_E, NOTE_F,
    NOTE_F_SHARP, NOTE_G, NOTE_A_FLAT, NOTE_A, NOTE_B_FLAT, NOTE_B,
};
static const char* const size_names[SIZE_ZONES] = { "Simple", "Rich", "Lush" };

static Gesture held_gesture(void)
{
    TouchordMode mode = tc_app_get_mode();
    if (mode == TOUCHORD_SETTINGS || mode == TOUCHORD_DRUM) return GESTURE_NONE;
    if (!tc_control_states[5]) return GESTURE_KEY;
    if (!tc_control_states[4]) return GESTURE_CHORDS;
    if (!tc_control_states[3]) return GESTURE_SIZE;
    return GESTURE_NONE;
}

static Scale* current_scale(void)
{
    uint8_t k = tc_app.current_key < 3 ? tc_app.current_key : 0;
    return &tc_app.key[k];
}

static int key_zone(void)
{
    bool flat;
    return note_to_midi(current_scale()->root, -1, &flat) % 12;
}

static int size_zone(void)
{
    int z = (int)tc_app.extension_count - SIZE_MIN;
    if (z < 0) z = 0;
    if (z >= SIZE_ZONES) z = SIZE_ZONES - 1;
    return z;
}

static int zone_count(Gesture g)
{
    switch (g) {
        case GESTURE_KEY:    return KEY_ZONES;
        case GESTURE_CHORDS: return CHORD_SET_COUNT;
        case GESTURE_SIZE:   return SIZE_ZONES;
        default:             return 0;
    }
}

static int current_zone(Gesture g)
{
    switch (g) {
        case GESTURE_KEY:    return key_zone();
        case GESTURE_CHORDS: return tc_app.chord_set < CHORD_SET_COUNT ? tc_app.chord_set : CHORD_SET_DEFAULT;
        case GESTURE_SIZE:   return size_zone();
        default:             return 0;
    }
}

// Zone under the finger, kept on the current zone until the finger is a
// quarter of a zone past its edge, so a resting finger doesn't flicker.
// Like Strum and Perform, the far end of the bar (position 0, the top of the
// screen strip) is "more": B, the brightest set, the biggest size.
static int zone_at(float pos, int n, int cur)
{
    pos = 1.0f - pos;
    int z = (int)(pos * n);
    if (z < 0) z = 0;
    if (z >= n) z = n - 1;
    if (z != cur && cur >= 0 && cur < n) {
        float w = 1.0f / n;
        if (pos > cur * w - w * 0.25f && pos < (cur + 1) * w + w * 0.25f) z = cur;
    }
    return z;
}

// Plays the home chord briefly so the change can be heard. Skipped while a
// key is held, since that chord is already sounding.
static void preview_home_chord(void)
{
    for (int i = 0; i < NUM_KEYS; i++)
        if (!tc_key_states[i]) return;

    uint8_t notes[MAX_CHORD];
    char name[CHORD_NAME_LEN];
    build_chord(*current_scale(), tc_app.octave, 0, CHORD_DEFAULT,
                tc_app.extension_count, 0, notes, name);

    sched_cancel(SCHED_TAG_PREVIEW);
    for (int i = 0; i < MAX_CHORD; i++) {
        if (!notes[i]) continue;
        sched_note(0, tc_app.channel, NOTE_ON, notes[i], tc_app.velocity, SCHED_TAG_PREVIEW);
        sched_note(PREVIEW_US, tc_app.channel, NOTE_OFF, notes[i], tc_app.velocity, SCHED_TAG_PREVIEW);
    }
}

static void apply_zone(Gesture g, int z)
{
    switch (g) {
        case GESTURE_KEY:
            current_scale()->root = key_roots[z];
            break;
        case GESTURE_CHORDS:
            tc_app.chord_set = (uint8_t)z;
            current_scale()->quality = SCALE_CHORD_SET;
            if (z >= CHORD_SET_WARM_FIRST && z <= CHORD_SET_WARM_LAST &&
                tc_app.extension_count < WARM_MIN_SIZE)
                tc_app.extension_count = WARM_MIN_SIZE;
            load_chord_set();
            break;
        case GESTURE_SIZE:
            tc_app.extension_count = (uint8_t)(SIZE_MIN + z);
            break;
        default:
            break;
    }
}

bool key_gesture_poll(float pos, bool touching)
{
    static bool active = false;

    Gesture g = held_gesture();
    if (g == GESTURE_NONE || !touching) {
        active = false;
        return false;
    }

    // Only a touch that starts while the button is held counts, so pressing
    // a button mid-strum leaves the strum alone.
    if (!active && tc_touch_state) return false;
    active = true;

    int n = zone_count(g);
    int cur = current_zone(g);
    int z = zone_at(pos, n, cur);
    // Picking a chord set always applies it, even the one already selected,
    // in case this key slot is still on a scale.
    bool changed = z != cur ||
                   (g == GESTURE_CHORDS && current_scale()->quality != SCALE_CHORD_SET);
    if (changed) {
        apply_zone(g, z);
        preview_home_chord();
    }
    return true;
}

#define PANEL_X 10      // right of the zone strip

static void draw_small(int x, int y, const char* str)
{
    ssd1306_draw_string_with_font(&tc_disp, x, y, 1, font_3x6, str);
}

static int small_width(const char* str)
{
    return (int)strlen(str) * (font_3x6[1] + font_3x6[2]) - font_3x6[2];
}

static void draw_small_centered(int y, const char* str)
{
    int x = PANEL_X + (128 - PANEL_X - small_width(str)) / 2;
    draw_small(x < PANEL_X ? PANEL_X : x, y, str);
}

// The zones as a strip on the left, lined up with the bar: the far end of the
// bar, which holds the last zone, is at the top.
static void draw_zones(int n, int cur)
{
    const int x = 1, w = 5, top = 0, h = 63, gap = 1;
    int block_h = (h - (n - 1) * gap) / n;
    int total = n * block_h + (n - 1) * gap;
    int y0 = top + (h - total) / 2;
    for (int i = 0; i < n; i++) {
        int zone = n - 1 - i;
        int y = y0 + i * (block_h + gap);
        if (zone == cur) ssd1306_draw_square(&tc_disp, x, y, w, block_h);
        else             ssd1306_draw_empty_square(&tc_disp, x, y, w - 1, block_h - 1);
    }
}

// The seven chords the keys play at the current size, on up to two lines.
static void draw_chord_lines(int y)
{
    int max_w = 128 - PANEL_X;
    char lines[2][40] = {{0}};
    int line = 0;
    for (int d = 0; d < SCALE_LEN && line < 2; d++) {
        uint8_t notes[MAX_CHORD];
        char name[CHORD_NAME_LEN];
        build_chord(*current_scale(), tc_app.octave, d, CHORD_DEFAULT,
                    tc_app.extension_count, 0, notes, name);
        for (;;) {
            char trial[40];
            snprintf(trial, sizeof trial, "%s%s%s", lines[line], lines[line][0] ? " " : "", name);
            if (small_width(trial) <= max_w || !lines[line][0]) {
                snprintf(lines[line], sizeof lines[line], "%s", trial);
                break;
            }
            if (++line >= 2) break;
        }
    }
    draw_small_centered(y, lines[0]);
    draw_small_centered(y + 8, lines[1]);
}

void key_gesture_draw(void)
{
    Gesture g = held_gesture();
    if (g == GESTURE_NONE) return;

    Scale* key = current_scale();
    int n = zone_count(g);
    int cur = current_zone(g);
    const char* title = "";
    const char* value = "";
    char buf[24];

    switch (g) {
        case GESTURE_KEY:    title = "KEY";    value = root_names[key->root]; break;
        case GESTURE_CHORDS: title = "CHORDS"; value = key_scale_name(*key);  break;
        case GESTURE_SIZE:   title = "SIZE";   value = size_names[cur];       break;
        default: break;
    }

    ssd1306_clear(&tc_disp);
    draw_zones(n, cur);
    ssd1306_draw_string(&tc_disp, PANEL_X, 0, 1, title);
    snprintf(buf, sizeof buf, "%d/%d", cur + 1, n);
    ssd1306_draw_string(&tc_disp, 128 - (int)strlen(buf) * 6, 0, 1, buf);
    ssd1306_draw_line(&tc_disp, PANEL_X, 9, 127, 9);

    int vw = (int)strlen(value) * 12 - 2;
    ssd1306_draw_string(&tc_disp, PANEL_X + (128 - PANEL_X - vw) / 2, 13, 2, value);

    if (g == GESTURE_SIZE) {
        snprintf(buf, sizeof buf, "%d notes", tc_app.extension_count);
        draw_small_centered(36, buf);
    } else {
        draw_chord_lines(34);
    }

    if (g == GESTURE_CHORDS && key->quality == SCALE_CHORD_SET) {
        snprintf(buf, sizeof buf, "Try %s", chord_set_loops[cur]);
        draw_small_centered(55, buf);
    } else if (g != GESTURE_SIZE) {
        draw_small_centered(55, "Slide to change");
    }
}

#define HINT_MS 3000

void key_gesture_draw_hint(void)
{
    static uint32_t start_ms = 0;
    static bool done = false;
    if (done || !tc_connected) return;

    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (start_ms == 0) start_ms = now ? now : 1;
    // Gone once its time is up or the player holds one of the buttons.
    if (now - start_ms >= HINT_MS || held_gesture() != GESTURE_NONE) {
        done = true;
        return;
    }

    static const char* const lines[3] = { "Hold + slide:", "B5 Key  B4 Chords", "B3 Size" };
    const int box_x = 8, box_y = 12, box_w = 112, box_h = 38;
    ssd1306_clear_square(&tc_disp, box_x - 1, box_y - 1, box_w + 2, box_h + 2);
    ssd1306_draw_empty_square(&tc_disp, box_x, box_y, box_w, box_h);
    for (int i = 0; i < 3; i++) {
        int w = (int)strlen(lines[i]) * 6 - 1;
        ssd1306_draw_string(&tc_disp, (128 - w) / 2, box_y + 5 + i * 10, 1, lines[i]);
    }
}
