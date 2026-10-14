/*
 * Touchord — MIDI chord controller firmware.
 * Copyright (C) 2025-2026 MB Daugdara
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * For more info, email info@daugdara.com
 */

#include "Scheduler.h"
#include "Defines.h"
#include "IO/Midi.h"
#include "pico/time.h"
#include "pico/critical_section.h"

// Polled from the core 0 loop rather than fired from a timer IRQ: the loop
// never sleeps, and a due note has to wait for that loop to reach the USB,
// BLE and TRS senders anyway, so an alarm would add no precision.

#define SCHED_MAX_EVENTS 64

typedef struct {
    uint64_t due_us;
    uint8_t  channel, status, note, velocity;
    uint8_t  tag;
    bool     used;
} SchedEvent;

// Taken inside tc_app_lock by callers, never the other way round.
static critical_section_t sched_lock;
static SchedEvent events[SCHED_MAX_EVENTS];
static volatile uint32_t pending = 0;

void sched_init(void)
{
    critical_section_init(&sched_lock);
}

void sched_note(uint32_t delay_us, uint8_t channel, uint8_t status,
                uint8_t note, uint8_t velocity, uint8_t tag)
{
    if (delay_us > 0) {
        uint64_t due = time_us_64() + delay_us;
        critical_section_enter_blocking(&sched_lock);
        for (int i = 0; i < SCHED_MAX_EVENTS; i++) {
            if (events[i].used) continue;
            events[i] = (SchedEvent){ due, channel, status, note, velocity, tag, true };
            pending++;
            critical_section_exit(&sched_lock);
            return;
        }
        critical_section_exit(&sched_lock);
    }
    // No delay, or the table is full: late is better than lost.
    send_midi_note(channel, status, note, velocity);
}

// Remove matching events under the lock, then send the ones to keep in due
// order. keep_on decides whether note-ons are sent or dropped.
static void sched_take(bool all_tags, uint8_t tag, bool due_only, bool keep_on)
{
    SchedEvent out[SCHED_MAX_EVENTS];
    int n = 0;
    uint64_t now = time_us_64();

    critical_section_enter_blocking(&sched_lock);
    for (int i = 0; i < SCHED_MAX_EVENTS && pending; i++) {
        SchedEvent *e = &events[i];
        if (!e->used) continue;
        if (!all_tags && e->tag != tag) continue;
        if (due_only && e->due_us > now) continue;
        e->used = false;
        pending--;
        if (e->status == NOTE_ON && e->velocity > 0 && !keep_on) continue;
        // Insertion sort by due time
        int j = n++;
        while (j > 0 && out[j - 1].due_us > e->due_us) { out[j] = out[j - 1]; j--; }
        out[j] = *e;
    }
    critical_section_exit(&sched_lock);

    for (int i = 0; i < n; i++)
        send_midi_note(out[i].channel, out[i].status, out[i].note, out[i].velocity);
}

void sched_cancel(uint8_t tag) { sched_take(false, tag, false, false); }
void sched_flush(uint8_t tag)  { sched_take(false, tag, false, true);  }

void sched_task(void)
{
    if (pending) sched_take(true, 0, true, true);
}
