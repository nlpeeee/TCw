/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#include "Settings.h"
#include "IO/Midi.h"
#include "Notes/Note.h"
#include "Notes/ChordSets.h"
#include "Globals.h"
#include "Helper.h"
#include "Sync.h"
#include "Data/Flash.h"
#include "IO/BleMidi.h"
#include "Timing/MidiClock.h"
#include "Timing/Quantize.h"
#include "Compose.h"
#include "Arp.h"
#include "Rendering/Graphics.h"
#include "pico/critical_section.h"
#include <stdio.h>

uint8_t stack[MAX_UI_DEPTH], depth=0, sel=0, cur=0;

const UINode* curr_node(void) { return &tree[cur]; }

const UINode* sel_child(void) {
    const UINode* n = curr_node();
    if (sel >= n->n_child) return NULL;
    return &tree[n->first_child + sel];
}

const UINode* get_node(uint8_t idx) {
    return &tree[idx];
}


void push(uint8_t idx) {
    if (depth < MAX_UI_DEPTH) {
        stack[depth++] = cur;
        cur = idx;
        sel = 0;
    }
}

uint8_t pop(void) {
    if (depth) cur = stack[--depth];
    sel = 0;
    return cur;
}

void trigger_boot_sel(const DataU* data)
{
    tc_trigger_bootsel = true;
}

void trigger_factory_reset(const DataU* data)
{
    critical_section_enter_blocking(&tc_app_lock);
    tc_app         = tc_app_default;
    tc_app_working = tc_app_default;
    critical_section_exit(&tc_app_lock);

    reload_custom_scales();
    flash_clear_all_presets();
    trigger_overlay("Factory Reset", 1000);
}

void trigger_midi_type(const DataU* data)
{
    switch_midi_trs(tc_app.midi_type);
}

void trigger_custom_scale(const DataU* data)
{
    reload_custom_scales();
}

// Picking a set in Settings also switches the key to it, as hold B4 does.
void trigger_chord_set(const DataU* data)
{
    tc_app.key[tc_app.current_key].quality = SCALE_CHORD_SET;
    reload_custom_scales();
}

uint8_t preset_slot_load = 0;
uint8_t preset_slot_save = 0;

void trigger_load_preset(const DataU* data)
{
    TouchordSettings temp;
    char msg[24];
    if (flash_load_preset(preset_slot_load, &temp)) {
        tc_app = temp;
        tc_app.mode = TOUCHORD_COMPOSE;
        use_single_key_slot(&tc_app);
        reload_custom_scales();
        snprintf(msg, sizeof msg, "Preset %u Loaded", (unsigned)preset_slot_load);
        trigger_overlay(msg, 800);
    } else {
        snprintf(msg, sizeof msg, "Slot %u Empty", (unsigned)preset_slot_load);
        trigger_overlay(msg, 1000);
    }
}

void trigger_save_preset(const DataU* data)
{
    char msg[24];
    if (flash_save_preset(preset_slot_save, &tc_app))
        snprintf(msg, sizeof msg, "Preset %u Saved", (unsigned)preset_slot_save);
    else
        snprintf(msg, sizeof msg, "Save Failed");
    trigger_overlay(msg, 800);
}

const char* trs_midi_sel[2] = {"Type A", "Type B"};
const char* on_off_sel[2] = {"Off", "On"};
const char* compose_sel[3] = {"Degree", "Inversion", "Jazz"};
const char* degree_sel[9] = {"ERR", "Major", "Minor", "Dominant", "Diminished", "Augmented", "Sus2", "Sus4", "ERR"};
const char* roll_sel[COMPOSE_ROLL_COUNT] = {"Off", "10 ms", "20 ms", "30 ms", "50 ms", "80 ms"};
const char* perform_type_sel[PERFORM_TARGET_COUNT] = {"CC", "Bend", "Press"};
const char* bass_sel[17] = {"Off", "Ch 1", "Ch 2", "Ch 3", "Ch 4", "Ch 5", "Ch 6", "Ch 7", "Ch 8",
                            "Ch 9", "Ch 10", "Ch 11", "Ch 12", "Ch 13", "Ch 14", "Ch 15", "Ch 16"};
const char* scale_sel[SCALE_COUNT] = {"ERR", "Major", "Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian", "Custom 0", "Custom 1", "Custom 2", "Custom 3",
                                       "Harm Minor", "Mel Minor", "Harm Major", "Phryg Dom", "Lydian Dom", "Mixo b6", "Major Pent", "Minor Pent", "Chord Set"};

UINode tree[MAX_UI_NODES] = {
    // 0: ROOT "Settings"
    {"Settings", UI_SUBMENU, 7, 1, NULL, {0}, NULL},

    // 1-7: First Page
    {"Modes", UI_SUBMENU, 3, 8,  NULL, {0}, NULL},
    {"MIDI", UI_SUBMENU, 7, 29,  NULL, {0}, NULL},
    {"Chords", UI_ENUM, CHORD_SET_COUNT, 0, trigger_chord_set, {.en_val = &tc_app.chord_set}, chord_set_names},
    {"Load Preset", UI_INT, FLASH_PRESET_COUNT, 0, trigger_load_preset, {.i_val = &preset_slot_load}, NULL},
    {"Save Preset", UI_INT, FLASH_PRESET_COUNT, 0, trigger_save_preset, {.i_val = &preset_slot_save}, NULL},
    {"Advanced", UI_SUBMENU, 6, 36, NULL, {0}, NULL},
    {"Firmware", UI_SUBMENU, 2, 50, NULL, {0}, NULL},

    // 8-10: Modes Settings
    {"Compose", UI_SUBMENU, 4, 11, NULL, {0}, NULL},
    {"Perform", UI_SUBMENU, 8, 15, NULL, {0}, NULL},
    {"Arp",     UI_SUBMENU, 6, 23, NULL, {0}, NULL},

    // 11-14: Modes.Compose Settings
    {"Mode",       UI_ENUM,   3, 0, NULL, {.en_val=&tc_app.compose_type}, compose_sel},
    {"Sustain",    UI_TOGGLE, 2, 0, NULL, {.toggle=&tc_app.compose_sustain},    on_off_sel},
    {"Voice Lead", UI_TOGGLE, 2, 0, NULL, {.toggle=&tc_app.compose_voice_lead}, on_off_sel},
    {"Roll",       UI_ENUM,   COMPOSE_ROLL_COUNT, 0, NULL, {.en_val=&tc_app.compose_roll}, roll_sel},

    // 15-22: Modes.Perform Settings
    {"Pos Sends", UI_ENUM, PERFORM_TARGET_COUNT, 0, NULL, {.en_val=&tc_app.perform_pos_type}, perform_type_sel},
    {"Pos CC", UI_INT, 128, 0, NULL, {.i_val=&tc_app.perform_pos_cc}, NULL},
    {"Size Sends", UI_ENUM, PERFORM_TARGET_COUNT, 0, NULL, {.en_val=&tc_app.perform_size_type}, perform_type_sel},
    {"Size CC", UI_INT, 128, 0, NULL, {.i_val=&tc_app.perform_size_cc}, NULL},
    {"Pos Default", UI_INT, 128, 0, NULL, {.i_val=&tc_app.perform_pos_default}, NULL},
    {"Size Default", UI_INT, 128, 0, NULL, {.i_val=&tc_app.perform_size_default}, NULL},
    {"Pos Keep", UI_TOGGLE, 2, 0, NULL, {.toggle=&tc_app.perform_reset_pos_on_lift}, on_off_sel},
    {"Size Keep", UI_TOGGLE, 2, 0, NULL, {.toggle=&tc_app.perform_reset_size_on_lift}, on_off_sel},

    // 23-28: Modes.Arp Settings
    {"Pattern", UI_ENUM,   ARP_PATTERN_COUNT, 0, NULL, {.en_val=&tc_app.arp_pattern}, arp_pattern_names},
    {"Rate",    UI_ENUM,   ARP_RATE_COUNT,    0, NULL, {.en_val=&tc_app.arp_rate},    arp_rate_names},
    {"Range",   UI_ENUM,   ARP_OCTAVES_MAX,   0, NULL, {.en_val=&tc_app.arp_octaves}, arp_octave_names},
    {"Gate",    UI_ENUM,   ARP_GATE_COUNT,    0, NULL, {.en_val=&tc_app.arp_gate},    arp_gate_names},
    {"Tempo",   UI_INT,    241,              40, NULL, {.i_val=&tc_app.arp_bpm},      NULL},
    {"Latch",   UI_TOGGLE, 2,                 0, NULL, {.toggle=&tc_app.arp_latch},   on_off_sel},

    // 29-35: MIDI Settings
    {"Channel", UI_INT, 16, 0, NULL,  {.i_val=&tc_app.channel}, NULL},
    {"TRS Midi", UI_ENUM, 2, 0, trigger_midi_type, {.en_val=&tc_app.midi_type}, trs_midi_sel},
    {"Velocity", UI_INT, 128, 0, NULL, {.i_val=&tc_app.velocity}, NULL},
    {"Bluetooth", UI_TOGGLE, 2, 0, NULL, {.toggle=&tc_app.bluetooth}, on_off_sel},
    {"Clock In", UI_TOGGLE, 2, 0, NULL, {.toggle=&tc_app.clock_in}, on_off_sel},
    {"Quantize", UI_ENUM, QUANT_GRID_COUNT, 0, NULL, {.en_val=&tc_app.quantize}, quant_grid_names},
    {"Bass", UI_ENUM, 17, 0, NULL, {.en_val=&tc_app.bass_channel}, bass_sel},

    // 36-41 Advanced: the key's root and scale, and the custom scales
    {"Root", UI_ENUM, 17, 0, NULL, {.en_val = &tc_app.key[0].root}, root_names},
    {"Scale", UI_ENUM, SCALE_COUNT, 1, NULL, {.en_val = &tc_app.key[0].quality}, scale_sel},
    {"Custom 0", UI_SUBMENU, 2, 42, NULL, {0}, NULL},
    {"Custom 1", UI_SUBMENU, 2, 44, NULL, {0}, NULL},
    {"Custom 2", UI_SUBMENU, 2, 46, NULL, {0}, NULL},
    {"Custom 3", UI_SUBMENU, 2, 48, NULL, {0}, NULL},

    // 42-43 Advanced.Custom 0
    {"Intervals", UI_PER_BUTTON_INT,  12, 0, trigger_custom_scale, {.i_val  = tc_app.custom_scale_intervals[0]}, NULL},
    {"Degrees",   UI_PER_BUTTON_ENUM, 7,  1, trigger_custom_scale, {.en_val = tc_app.custom_scale_chords[0]},    degree_sel},
    // 44-45 Advanced.Custom 1
    {"Intervals", UI_PER_BUTTON_INT,  12, 0, trigger_custom_scale, {.i_val  = tc_app.custom_scale_intervals[1]}, NULL},
    {"Degrees",   UI_PER_BUTTON_ENUM, 7,  1, trigger_custom_scale, {.en_val = tc_app.custom_scale_chords[1]},    degree_sel},
    // 46-47 Advanced.Custom 2
    {"Intervals", UI_PER_BUTTON_INT,  12, 0, trigger_custom_scale, {.i_val  = tc_app.custom_scale_intervals[2]}, NULL},
    {"Degrees",   UI_PER_BUTTON_ENUM, 7,  1, trigger_custom_scale, {.en_val = tc_app.custom_scale_chords[2]},    degree_sel},
    // 48-49 Advanced.Custom 3
    {"Intervals", UI_PER_BUTTON_INT,  12, 0, trigger_custom_scale, {.i_val  = tc_app.custom_scale_intervals[3]}, NULL},
    {"Degrees",   UI_PER_BUTTON_ENUM, 7,  1, trigger_custom_scale, {.en_val = tc_app.custom_scale_chords[3]},    degree_sel},

    // 50-51: Firmware
    {"Reset Factory",   UI_TRIGGER, 0, 0, trigger_factory_reset, {0}, NULL},
    {"Firmware Update", UI_TRIGGER, 0, 0, trigger_boot_sel,      {0}, NULL},
};


static const ModeHandlers settings_handlers = TC_MODE_HANDLERS(settings);

void settings_start()
{
    tc_bind_handlers(&settings_handlers);
    depth = 0; cur = 0; sel = 0;
    tc_trill_segs = 10;
    tc_trill_show = false;
}

void settings_end()
{
    reload_custom_scales();
}

int settings_current_key = 0;
void settings_draw()
{
    ssd1306_draw_line(&tc_disp, 0, 7, 124, 7);
    const UINode* cn = curr_node();
    draw_string_top(cn->title);
    switch (cn->type)
    {
        case UI_SUBMENU:
        {
            uint8_t visible = cn->n_child < 5 ? cn->n_child : 5;
            uint8_t scroll_off = 0;
            if (sel >= visible)
                scroll_off = sel - visible + 1;

            for (uint8_t i = 0; i < visible; ++i) {
                uint8_t item = scroll_off + i;
                uint8_t y = 9 + i * 9;
                const UINode* child = get_node(cn->first_child + item);

                if (item == sel) {
                    draw_string_inverted(0, y, child->title);
                } else {
                    ssd1306_draw_string(&tc_disp, 2, y + 1, 1, child->title);
                }
            }

            draw_scroll_indicator(sel, cn->n_child);
        }
        break;
        case UI_ENUM:
            if(cn->data.en_val)
                draw_string_center(cn->opts[*cn->data.en_val]);
        break;
        case UI_INT:
            if(cn->data.i_val)
                draw_int_center(*cn->data.i_val);
            if (cn->data.i_val == &preset_slot_load || cn->data.i_val == &preset_slot_save)
            {
                const char* status = flash_preset_exists(*cn->data.i_val) ? "Saved" : "Empty";
                ssd1306_draw_string(&tc_disp, 64 - (strlen(status) * 6) / 2, 44, 1, status);
            }
        break;
        case UI_TOGGLE:
            if (cn->data.toggle)
                draw_string_center(cn->opts[*cn->data.toggle ? 1 : 0]);
            {
                char status[22];
                status[0] = '\0';
#if TOUCHORD_BLE
                if (cn->data.toggle == &tc_app.bluetooth)
                    ble_midi_status(status, sizeof status);
#endif
                if (cn->data.toggle == &tc_app.clock_in)
                    midi_clock_status(status, sizeof status);
                if (status[0])
                    ssd1306_draw_string(&tc_disp, 64 - (strlen(status) * 6) / 2, 44, 1, status);
            }
        break;
        case UI_PER_BUTTON_ENUM:
            draw_string_int_centered("Selected Key: ", settings_current_key, 7);
            if(cn->data.en_val)
                draw_string_center(cn->opts[cn->data.en_val[settings_current_key]]);
        break;
        case UI_PER_BUTTON_INT:
            draw_string_int_centered("Selected Key: ", settings_current_key, 7);
            if(cn->data.i_val)
                draw_int_center(cn->data.i_val[settings_current_key]);
        break;
    }
    draw_settings_footer();
}

void settings_update()
{

}

void settings_key_down(uint8_t key)
{
    settings_current_key = key;
}

void settings_key_up(uint8_t key)
{

}

void settings_key_up_independent(uint8_t key)
{

}

void settings_button_down(uint8_t button)
{
    const UINode* cn = curr_node();
    switch(button)
    {
        case 0: tc_app_set_mode(TOUCHORD_COMPOSE); break;
        case 1: pop(); break;
        case 2:
            if(cn->type == UI_SUBMENU)
            {
                const UINode* sn = sel_child();
                if(sn && sn->type == UI_TRIGGER)
                {
                    if (sn->trig) sn->trig(&sn->data);
                }
                else push(cn->first_child + sel);
            }
            else
            {
                if (cn->trig) cn->trig(&cn->data);
                pop();
            }
        break;
    }
}

void settings_button_double_down(uint8_t button)
{

}

void settings_button_up(uint8_t button)
{

}

int settings_prev_seg = -1;
void settings_trill_down(float pos, float size)
{
    int seg = segments(pos, 10);
    if(settings_prev_seg != -1)
    {
        int delta = (seg - settings_prev_seg);
        if(delta != 0)
        {
            const UINode* cn = curr_node();
            switch (cn->type)
            {
                case UI_SUBMENU:
                    sel = clamp_i16(sel + delta, 0, cn->n_child-1);
                break;
                case UI_ENUM:
                    if(cn->data.en_val)
                        *cn->data.en_val = clamp_i16(*cn->data.en_val - delta, cn->first_child, cn->n_child-1);
                break;
                case UI_INT:
                    if(cn->data.i_val)
                        *cn->data.i_val = clamp_i16(*cn->data.i_val - delta, cn->first_child, cn->n_child-1);
                break;
                case UI_TOGGLE:
                    if(cn->data.toggle)
                        *cn->data.toggle = (delta > 0);
                break;
                case UI_PER_BUTTON_ENUM:
                    if(cn->data.en_val)
                        cn->data.en_val[settings_current_key] = clamp_i16(cn->data.en_val[settings_current_key] - delta, cn->first_child, cn->n_child-1);
                break;
                case UI_PER_BUTTON_INT:
                    if(cn->data.i_val)
                        cn->data.i_val[settings_current_key] = clamp_i16(cn->data.i_val[settings_current_key] - delta, cn->first_child, cn->n_child-1);
                break;
            }
        }
    }
    settings_prev_seg = seg;
}

void settings_trill_up()
{
    settings_prev_seg = -1;
}
