/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#include "Perform.h"
#include "Globals.h"
#include "Sync.h"
#include "IO/Midi.h"
#include "Notes/Note.h"
#include "Rendering/Graphics.h"


// Pressure from touch strength, so it doesn't depend on where the finger is.
// A light touch sums to about PRESS_MIN, a firm flat fingertip to PRESS_MAX.
#define PRESS_MIN 0.5f
#define PRESS_MAX 2.5f

static float bend_start_pos = 0.5f;

static uint8_t to_midi7(float v)
{
    if (v < 0.0f)   return 0;
    if (v > 127.0f) return 127;
    return (uint8_t)v;
}

// Each axis sends its CC, pitch bend or channel pressure. value is the
// axis's 0-127 reading; is_pos marks the position axis.
static void perform_send(uint8_t type, uint8_t cc, float value, bool is_pos, float pos)
{
    switch (type) {
        case PERFORM_BEND: {
            // Position bends relative to where the finger landed, so touching
            // down never jumps; half the bar is a full bend.
            float b = is_pos ? (bend_start_pos - pos) * 2.0f : (value / 127.0f) * 2.0f - 1.0f;
            if (b < -1.0f) b = -1.0f;
            if (b >  1.0f) b =  1.0f;
            send_pitch_bend(tc_app.channel, (uint16_t)(8192 + b * 8191));
        } break;
        case PERFORM_PRESSURE:
            send_aftertouch(tc_app.channel,
                to_midi7((tc_trill_strength - PRESS_MIN) * 127.0f / (PRESS_MAX - PRESS_MIN)));
            break;
        default:
            send_midi_cc(tc_app.channel, cc, to_midi7(value));
            break;
    }
}

// Bend and pressure always return to rest; CCs return to their default.
static void perform_reset(uint8_t type, uint8_t cc, uint8_t cc_default)
{
    switch (type) {
        case PERFORM_BEND:     send_pitch_bend(tc_app.channel, 8192); break;
        case PERFORM_PRESSURE: send_aftertouch(tc_app.channel, 0);    break;
        default:               send_midi_cc(tc_app.channel, cc, cc_default); break;
    }
}

static const ModeHandlers perform_handlers = TC_MODE_HANDLERS(perform);

void perform_start()
{
    tc_bind_handlers(&perform_handlers);
    tc_trill_segs = 0;
    tc_trill_show = true;
}

void perform_end()
{
    send_midi_chord(tc_app.channel, NOTE_OFF, tc_app.chord, tc_app.extension_count, tc_app.velocity);
    perform_reset(tc_app.perform_pos_type,  tc_app.perform_pos_cc,  tc_app.perform_pos_default);
    perform_reset(tc_app.perform_size_type, tc_app.perform_size_cc, tc_app.perform_size_default);
    tc_app.chord_name[0] = '\0';
}

void perform_draw()
{
    draw_current_chord();
    draw_mode_label("Perform");
}

void perform_update()
{

}

static int8_t perform_active_key = -1;

void perform_key_down(uint8_t key)
{
    send_midi_chord(tc_app.channel, NOTE_OFF, tc_app.chord, tc_app.extension_count, tc_app.velocity);
    build_chord(tc_app.key[tc_app.current_key], tc_app.octave, key, tc_app.degree,
                tc_app.extension_count, tc_app.inversion, tc_app.chord, tc_app.chord_name);
    send_midi_chord(tc_app.channel, NOTE_ON, tc_app.chord, tc_app.extension_count, tc_app.velocity);
    perform_active_key = (int8_t)key;
}

void perform_key_up(uint8_t key)
{
    if (perform_active_key != (int8_t)key) return;
    perform_active_key = -1;

    send_midi_chord(tc_app.channel, NOTE_OFF, tc_app.chord, tc_app.extension_count, tc_app.velocity);

    build_chord(tc_app.key[tc_app.current_key], tc_app.octave, 0, CHORD_DEFAULT,
                        0, tc_app.inversion, tc_app.chord, tc_app.chord_name);
    tc_app.chord_name[0] = '\0';
}

void perform_key_up_independent(uint8_t key)
{

}

void perform_button_down(uint8_t button)
{
    switch(button)
    {
        case 0: tc_app_set_mode(TOUCHORD_STRUM); break;
        case 1:
        if(tc_app.octave > 0) tc_app.octave--;
        break;
        case 2:
        if(tc_app.octave < 7) tc_app.octave++;
        break;
        case 3: tc_app.current_key = 0; break;
        case 4: tc_app.current_key = 1; break;
        case 5: tc_app.current_key = 2; break;
    }
}

void perform_button_double_down(uint8_t button)
{

}

void perform_button_up(uint8_t button)
{

}

void perform_trill_down(float pos, float size)
{
    if (!tc_touch_state) bend_start_pos = pos;
    perform_send(tc_app.perform_pos_type,  tc_app.perform_pos_cc,  127 - (pos * 127),          true,  pos);
    perform_send(tc_app.perform_size_type, tc_app.perform_size_cc, (127 - (pos * 127)) * size, false, pos);
}

void perform_trill_up()
{
    if(tc_app.perform_reset_pos_on_lift || tc_app.perform_pos_type != PERFORM_CC)
        perform_reset(tc_app.perform_pos_type, tc_app.perform_pos_cc, tc_app.perform_pos_default);
    if(tc_app.perform_reset_size_on_lift || tc_app.perform_size_type != PERFORM_CC)
        perform_reset(tc_app.perform_size_type, tc_app.perform_size_cc, tc_app.perform_size_default);
}
