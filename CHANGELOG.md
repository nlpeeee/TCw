# Changelog

All notable changes to Touchord are recorded here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

The single source of truth for the firmware version is `src/version.h`. CMake
parses it so build artifacts and `pico_set_program_version()` stay in lockstep
with what the firmware reports at runtime. Bump it together with this file
when cutting a release.

## [Unreleased]

### Added

- Looper: tap B4 to record a progression of green-key presses and loop it, tap to stop/play, hold 2 s to clear. Plays through the current mode, follows host clock or Arp Tempo, saved with song memories and autosave; starter memories include demo loops.
- A "Connect USB or Bluetooth" screen until a host connects.
- Reset Factory and Firmware Update ask for a second OK press.
- The setup is saved automatically and restored at power-on, including the mode (Touchord used to always start in Compose).
- Song memories: hold B0 + a green key to recall memory 1-7, keep the key held 2 s to save. Empty memories hold 7 starter setups.
- A row of the 7 keys' chords on the play screen, with the held key lit.
- Hold B0 and slide to pick a mode, B1 for the mode's playing style, B2 for velocity; hold B1 + B2 for a help screen.

### Changed

- B0 + B2 + B4 must be held for 3 seconds, with a countdown, to enter BOOTSEL.
- Preset slots are shown numbered from 1.
- B1/B2 no longer double-click: a tap changes octave on release, without the 300 ms wait. Arp pattern, Strum size and Omni span moved to hold-and-slide (B1, B3, B1).
- Compose's bar labels read "6 notes", "5 notes", "4 notes", "Flip 4", "Flip 3" instead of "Ext 6" ... "Ext 3 Par".
- Compose voice leading is on by default.
- Chord roots stay between F# below and F above the octave's C, so a key change doesn't jump most of an octave.
- Chord modes keep to octaves 2-6.
- B0 steps through Compose, Arp, Strum, Omni, Perform, Drum.
- Drum mode defaults to a General MIDI kit; hold B1 and slide for the old notes-in-a-row layout.
- Arp's bar rates run fastest at the far end, like every other bar control.
- Perform's "Pos Keep" / "Size Keep" are renamed "Pos Resets" / "Size Resets", which is what they do.
- `Touchord-single.uf2` (TCw wiring) reads the white buttons in reverse pin order, so B0 is the leftmost button on the TCw as on the Touchord.

### Fixed

### Removed

## [1.0.0] — 2026-05-31

### Added

- Compose Jazz mode (tritone sub, secondary dominant, parallel maj↔min, vii°7).
- Compose voice-leading toggle.
- Compose trill tooltip.
- Drum mode and visual 7-pad grid.
- Trill bar position indicator.
- Transitions between modes.
- Boot Intro.
- Octave / range visuals.
- Settings page.
- Dual-I2C and single-I2C firmware.
- Persistent presets.
- Custom scales: four user-configurable scale slots.
- Compose sustain toggle, Perform CC mappings/defaults/reset-on-lift toggles.
- Versioning system: `src/version.h`.
- `CONTRIBUTING.md` developer guide (architecture, concurrency, mode/setting how-tos, pitfalls, PR + release flow).
- SPDX copyright header (`Copyright (C) 2025-2026 MB Daugdara`, `GPL-3.0-or-later`, `info@daugdara.com`) on every non-vendored source file.
- DMA-based screen send.
- Other features.

### Changed

- `compose_key_down` snapshots and emits set-difference (no transient natural chord when pressing a key with the bar held in JAZZ).
- `compose_trill_down` updates trill state and tooltip outside the chord-name guard so the bar's effect is in place when a key press arrives.
- `io_task` polls the trill 5× per frame so latency drops from ~30 ms to a few ms.
- Screen I2C bumped to 1 MHz.
- `tc_app_default` now uses designated initialisers — adding fields can't silently misalign the list.
- Trill 4 % deadzone.
- `io_task` calls `multicore_lockout_victim_init()` so `flash_save_preset` can pause it without hanging.
- `trill_init` now requests 16-bit resolution to match the `TRILL_MAX_SIZE` normalisation.
- Other changes.

### Fixed

- Too many to track for initial release.

### Removed

- Legacy single-frame `main()` splash (replaced by `anim_boot_intro`).
- USB CDC settings protocol.

[Unreleased]: https://github.com/dovker/Touchord/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/dovker/Touchord/releases/tag/v1.0.0
