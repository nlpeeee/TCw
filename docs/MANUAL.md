# Touchord manual

Touchord turns one finger into a whole chord: press a green key to play a chord, and use the white buttons and the touch bar to change how it sounds. This manual matches the firmware on main as of 15 October 2026 (TCw single build).

## Getting started

Touchord is a MIDI controller, so it makes no sound itself. Plug it into a computer, phone or synth over USB, or pair it over Bluetooth as "Touchord", or use the TRS MIDI socket.

1. Power it on. Until USB or Bluetooth connects, the screen says "Connect USB or Bluetooth" and the keys stay silent.
2. Once connected, a hint shows for 3 seconds: hold B5, B4 or B3 and slide on the bar. Hold B1 and B2 together any time for a help screen.
3. Press any green key. You hear a chord, its name shows large on screen, and the row of 7 chord names under it lights the key you hold.

Out of the box you are in **Compose** mode, key of **C**, chord set **Bright 1** (C F G Am Dm Em Gsus4). The "Try" hint on the chord set screen suggests a loop of keys that sounds good, for example keys 2 3 4 1.

**The parts.** Seven green keys play chords, numbered 1 to 7 from left to right. Six white buttons, B0 to B5 from left to right, change things. The touch bar runs front to back on the left. Its **far end** (away from you, the top of the screen strip) always means more: higher, faster, brighter, harder.

## Playing chords

Three things decide which chords the green keys play: the **key**, the **chord set** and the **size**. Each has its own white button. Hold the button, then touch the bar and slide; the screen shows a strip of zones on the left, the choice in large letters, and the 7 chords you will get. Each change plays the first chord for a quarter second so you can hear it.

| Hold | Then slide to choose | Choices, near end to far end |
| --- | --- | --- |
| B5 | Key | C, Db, D, Eb, E, F, F#, G, Ab, A, Bb, B (12 zones) |
| B4 | Chord set | Dark 1-4, Warm 1-4, Bright 1-2 (10 zones) |
| B3 | Size | Simple (3 notes), Rich (4 notes), Lush (5 notes) |

**Chord sets** are seven chords that work well together. The names describe the sound of the chords themselves, from minor-heavy (Dark) to major-heavy (Bright).

| Set | Chords in C, keys 1 to 7 | Try |
| --- | --- | --- |
| Dark 1 | Cm Ab Db Fm Gm Eb Bb | 1 3 1 2 |
| Dark 2 | Cm Ab Bb Eb Fm Gm G | 1 2 4 3 |
| Dark 3 | Cm Fm Ab Gm Eb Bb7 Dm7b5 | 1 2 3 4 |
| Dark 4 | Cm F7 Bb Gm Eb Dm Am7b5 | 1 2 1 2 |
| Warm 1 | Cm Ab G7 Bbm Eb7 Fm Db | 2 3 1 4 5 |
| Warm 2 | C F G7 Em Am Dm E7 | 2 3 4 5 |
| Warm 3 | C F Em Dm Am G Bb | 2 3 4 1 |
| Warm 4 | C F Fm Am G Em Ab | 1 2 3 1 |
| Bright 1 | C F G Am Dm Em Gsus4 | 2 3 4 1 |
| Bright 2 | C Bb F G Am Dm Ab | 1 2 3 1 |

Choosing a Warm set raises the size to at least Rich, because those chords are written as sevenths. Changing the key moves every chord in the set together. Chords stay in one comfortable range whatever the key, so they don't jump up or down when you change it.

A slide only counts when your finger lands on the bar after you are already holding the button. Pressing a button in the middle of a strum leaves the strum alone.

## The white buttons

Every white button follows one rule: **tap** it for a quick action, or **hold it and slide** on the bar to choose. B3 and B5 have no tap action.

| Button | Tap | Hold + slide on the bar |
| --- | --- | --- |
| B0 | Next mode | Mode (switches when you let go) |
| B1 | Octave down | Play style for this mode |
| B2 | Octave up | Touch: how hard notes play (near soft, far hard, 40 to 127) |
| B3 | nothing | Size |
| B4 | Looper: record, loop, stop, play (see Looper) | Chord set |
| B5 | nothing | Key |

Other combinations:

- **Hold B0 alone** for just over half a second: open Settings.
- **Hold B1 + B2** together: help screen listing what every button does in this mode.
- **Hold B0, tap a green key**: switch to song memory 1-7. Keep the key held for 2 seconds to save into it instead (see Song memories).
- **Hold B0 + B2 + B4** for 3 seconds: firmware update mode. The screen counts down, and letting go cancels it.

Octave runs from 2 to 6. The dots at the top right of the screen show where you are.

**Play style (hold B1)** means something different in each mode:

| Mode | Hold B1 + slide chooses |
| --- | --- |
| Compose | Roll: gap between the notes of a chord (Off, 10, 20, 30, 50, 80 ms) |
| Arp | Pattern: Up, Down, Up-Down, Random, Chord |
| Strum, Omni | How far the bar reaches: 1 to 4 octaves |
| Perform | What the bar sends: CC, Bend or Press |
| Drum | Kit (drum sounds) or Notes (a row of notes) |

## Modes and the touch bar

Tap B0 to step through the six modes in this order: Compose, Arp, Strum, Omni, Perform, Drum, then back to Compose. The mode name shows on screen. What the bar does depends on the mode.

| Mode | Green keys | Touch bar, near end to far end |
| --- | --- | --- |
| Compose | Hold a key to play its chord; it stops when you let go | While a chord sounds: Flip 3, Flip 4, 4 notes, 5 notes, 6 notes. Flip swaps major and minor. Lift to return to the plain chord |
| Arp | Press a key and its notes play one at a time; press another to change chord, the same one again to stop | Nearest two parts: each tap steps the range 1, 2, 3, 4 octaves. Other six parts: speed, from 1/4 up to 1/32 at the far end |
| Strum | Choose the chord (nothing sounds yet) | Slide to pluck the chord's notes one at a time, low near you, high far away |
| Omni | Plays a low root and fifth while held | Slide to strum pairs of chord notes, like an Omnichord |
| Perform | Hold a key to play its chord | Sends a control as you slide: filter (CC 74) by default, or pitch bend or pressure. Bend starts from where your finger lands |
| Drum | Kick, snare, clap, closed hat, open hat, tom, crash (left to right) | Sets how hard the next hit plays, hard at the far end |

Notes for each mode:

- **Arp** follows MIDI clock from your computer when it is playing; otherwise it uses the Tempo in Settings (120 BPM out of the box). Latch is on, so the arp keeps going after you let go.
- **Strum and Omni**: hold B1 and slide to set how many octaves the bar covers.
- **Drum**: holding B3, B4, B5 or B2 does nothing (a tap of B4 still runs the looper), since drums have no key or chords. Hold B1 and slide to switch to Notes, where the keys play a row of notes and B1/B2 change octave.

## Looper

Tap **B4** to record a chord progression, and tap it again to loop it. The loop plays the green keys for you while you strum, arp, bend or switch modes on top.

1. **Tap B4.** REC blinks at the bottom right. Nothing records yet, so take your time; tap B4 again to cancel.
2. **Play your chords.** Recording starts on your first green key. The screen shows REC and the bar you are on.
3. **Tap B4** to close the loop. It carries straight on playing from the top, and the screen shows which bar of the loop is playing (for example >2/4).
4. **Tap B4** to stop (LOOP stays on screen), and tap again to play it from the top.
5. **Hold B4** for 2 seconds, without touching the bar, to clear the loop. The screen counts down.

How it behaves:

- The loop records **which green keys you pressed and when**, not notes. Changing key (hold B5), chord set (hold B4 + slide), size (hold B3) or mode (B0) while it plays changes what it plays. Record in Compose, then switch to Arp, Strum or Omni and play over your own progression.
- The bar is never recorded; it stays live, so strumming, arp speed and bend are played fresh every time round.
- Pressing a green key while the loop plays takes over until you let go; the loop then picks up at its next chord.
- Length: with Quantize on, or while your computer sends clock, the loop rounds to whole bars (1 to 8) and lines up with the beat; a Start from your computer takes it back to its top. With Quantize off and no clock, the loop is exactly as long as you played it. Up to 8 bars and about 30 chord changes.
- With no clock coming in, the loop keeps time with the Arp Tempo, so changing the tempo speeds it up or slows it down.
- B4 only counts as a tap when you let go quickly without touching the bar. Holding B4 to pick a chord set never starts the looper.

## Song memories and autosave

Touchord remembers your setup by itself. About 5 seconds after your last change, once nothing is playing, it saves the mode, key, chord set, size and everything else. Next time you power on, you start right where you left off.

The seven green keys also hold seven **song memories**, so you can jump between setups:

- **Recall:** hold B0 and tap a green key. The screen shows the memory number, mode and chord set.
- **Save:** hold B0 and keep the green key held for 2 seconds, until "Saved to" appears.

The loop is saved and recalled with the memory, and the autosave keeps it too. A recalled loop waits stopped: tap B4 to hear it. Each starter memory comes with a four-bar demo loop of its chord set.

Switching memories keeps your MIDI, Bluetooth, clock and bass settings, since those belong to your rig, not the song. Until you save over them, the memories hold starter setups:

| Memory | Mode | Chord set | Setup |
| --- | --- | --- | --- |
| 1 | Compose | Bright 1 | Simple size (the default) |
| 2 | Compose | Warm 3 | Lush size, 30 ms roll |
| 3 | Arp | Dark 2 | Up, 1/16 |
| 4 | Strum | Bright 2 | Bar covers 2 octaves |
| 5 | Omni | Warm 2 | Rich size |
| 6 | Arp | Warm 1 | Up-Down, 1/8, 2 octaves |
| 7 | Perform | Dark 3 | Bar sends pitch bend |

All starters are in C. Memories 1-7 are the same as presets 1-7 in Settings; presets 8-32 are reached only from Settings.

## Settings

Hold B0 alone to open Settings. Inside, the buttons change meaning, as the bottom line of the screen reminds you: **B0** exits (to Compose), **B1** goes back, **B2** is OK, and sliding on the bar scrolls the list or changes a value. Most players never need Settings; the hold-and-slide buttons cover everyday changes.

| Menu | Item | What it does | Out of the box |
| --- | --- | --- | --- |
| Modes > Compose | Mode | What Compose's bar does: Degree (notes and Flip), Inversion or Jazz | Degree |
| Modes > Compose | Sustain | Chord keeps sounding after you let go | Off |
| Modes > Compose | Voice Lead | Chords move smoothly from one to the next | On |
| Modes > Compose | Roll | Same as hold B1 + slide | Off |
| Modes > Perform | Pos Sends, Pos CC | What sliding sends, and which CC | CC 74 |
| Modes > Perform | Size Sends, Size CC | What finger size sends, and which CC | CC 1 |
| Modes > Perform | Defaults, Resets | Value sent when you lift, and whether lifting resets it |  |
| Modes > Arp | Pattern, Rate, Range | Same as hold B1 and the Arp bar | Up, 1/16, 1 octave |
| Modes > Arp | Gate | How long each note lasts | 50% |
| Modes > Arp | Tempo | Speed when no clock comes in (40-240) | 120 |
| Modes > Arp | Latch | Arp keeps going after you let go | On |
| MIDI | Channel | MIDI channel (shown 0-15, where 0 is channel 1) | 0 |
| MIDI | TRS Midi | Type A or Type B socket wiring | Type A |
| MIDI | Velocity | Same as hold B2 + slide | 100 |
| MIDI | Bluetooth | Bluetooth MIDI on or off | On |
| MIDI | Clock In | Follow MIDI clock from the computer | On |
| MIDI | Quantize | Lines presses up with the beat: Off, 1/16, 1/8, 1/4, 1 Bar | Off |
| MIDI | Bass | Sends each chord's root to its own channel | Off |
| Chords |  | Same as hold B4 + slide | Bright 1 |
| Load Preset, Save Preset |  | Presets 1-32; 1-7 are the song memories |  |
| Advanced | Root, Scale, Custom 0-3 | Use a plain scale instead of a chord set, or build your own |  |
| Firmware | Reset Factory, Firmware Update | Each asks you to press OK a second time |  |

**Quantize** applies to Compose and Drum, and to Arp only while your computer is playing and sending clock. When it is on, the screen shows its grid at the bottom left (for example Q1/8), blinking while a press waits for the beat. With no clock coming in, it keeps time with the Arp Tempo.

## Troubleshooting

| Problem | Fix |
| --- | --- |
| Keys make no sound | Check the screen. "Connect USB or Bluetooth" means nothing is listening yet. Once connected, check your synth or app is set to the same MIDI channel |
| Holding a button and sliding does nothing | Hold the button first, then touch the bar. A finger already on the bar is ignored |
| The arp won't stop | Latch is on: press the same green key again |
| Strum mode is silent when I press a key | That's normal: in Strum the key picks the chord and the bar plays it |
| Presses feel late | Quantize is on (Q shows at the bottom left). Turn it Off in Settings > MIDI |
| I want the factory setup back | Settings > Firmware > Reset Factory, then OK twice. This also clears saved memories and loops |
| Tapping B4 plays a loop I didn't record | It's the memory's demo loop or your last loop. Hold B4 for 2 seconds to clear it, then tap to record |
| Updating the firmware | Hold B0 + B2 + B4 for 3 seconds, or hold B0 while plugging in. Copy the .uf2 file to the drive that appears |
