# TCw

DIY version of the great Touchord, using a Pico 2W, Redragon low profile switches, 0.96" oled and a 3d printed case. still uses the Trill bar,

It's just a prototype. Thanks

![External](https://github.com/nlpeeee/TCw/blob/main/img/external.jpeg)


![Internal](https://github.com/nlpeeee/TCw/blob/main/img/internal.jpeg)

## What TCw adds to Touchord

TCw tracks [dovker/Touchord](https://github.com/dovker/Touchord) (currently v1.0.0) and adds:

- **Raspberry Pi Pico 2 W support.** The build targets `pico2_w` by default. GPIO25 is the radio's chip select on the W boards, so the status LED is driven through the radio instead.
- **Bluetooth LE MIDI.** Touchord shows up as a Bluetooth MIDI device called **Touchord** on macOS, iOS, Windows (MIDI apps with BLE support) and Android. It pairs with "Just Works" pairing and remembers the host. USB MIDI and TRS MIDI keep working alongside it.
  - Keys play whenever USB *or* Bluetooth is connected, so it can run from a battery or USB power bank.
  - **Settings > MIDI > Bluetooth** turns Bluetooth MIDI on or off. The LED blinks slowly while USB or Bluetooth is connected.
  - "BT Connected" / "BT Disconnected" pops up on screen when a host connects or drops.
- **Arp mode** (B0 from Compose). Hold a key and its chord plays one note at a time.
  - Touch bar, left to right in 8 parts: six rates (1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32), then the last two parts together: each tap steps the range through 1-4 octaves.
  - Latch is on by default: the arp keeps going after you let go, another key changes chord, and the same key again stops it. Turn it off under Settings > Modes > Arp.
  - Double-click B1/B2 steps the pattern: Up, Down, Up-Down, Random, Chord.
  - Pattern, rate, range, gate, tempo and latch are also under **Settings > Modes > Arp**.
- **MIDI clock in** over USB or Bluetooth. The arp locks to the host's beat while it plays and uses its tempo while it is stopped; with no clock it uses the Arp Tempo setting. **Settings > MIDI > Clock In** shows the incoming tempo.
- **Bass channel** (**Settings > MIDI > Bass**, Off by default). Compose and Arp send the chord root an octave down on that channel; Omni sends its root and fifth there.
- **Pitch bend and pressure from the touch bar** in Perform: set **Settings > Modes > Perform > Pos Sends** or **Size Sends** to **Bend** or **Press** (channel pressure) instead of CC. Both return to rest when you lift.
- **Compose Roll** (**Settings > Modes > Compose > Roll**) plays chords from the lowest note up, 10 to 80 ms apart.

## Building

Same as upstream (Pico SDK 2.2.0, VS Code Pico extension or plain CMake). Two firmware files are produced:

| File | Wiring |
| --- | --- |
| `Touchord-single.uf2` | Trill bar and screen share I2C0 on GPIO16/17. **This is the TCw prototype wiring.** |
| `Touchord-dual.uf2` | Trill bar on I2C0 (GPIO16/17), screen on I2C1 (GPIO14/15), as on the Touchord PCB. |

Hold the first control button while plugging in, or press buttons 1 + 3 + 5 together, to get into BOOTSEL, then copy the `.uf2` across.

To build for a plain Pico 2 without Bluetooth: `cmake -DPICO_BOARD=pico2 ...`.

Bluetooth pairings are stored in flash just below the preset sectors, so saving presets and pairing never overwrite each other.

---

*The rest of this README is upstream Touchord's.*

# Touchord (upstream)
<p align="center"> <img src="./img/logo.png" width="200"> </p>

Touchord is an open source chord-generating MIDI controller featuring a touch bar interface. It is designed for musicians seeking versatile chordal playing and performance options and for makers looking for a highly modifyable product with a realistic and fun use.

## [TOUCHORD DIY KIT PRE-ORDERS](https://daugdara.com/products/touchord-diy-kit)

## Motivation

Making music without formal training is hard. Learning theory takes time, and when the goal is to create and express ideas quickly, it can feel tedious. Chord progressions and harmony are crucial yet difficult to master; exploring by ear works, but entering complex chords on guitar or piano is slow. That is where chord‑generating synths and MIDI controllers help.

**Why not buy an existing similar device?** Many are closed‑source and built by unestablished companies with limited support histories. When projects are abandoned or parts become unavailable, devices turn into e‑waste.

Touchord addresses these problems. Components are common, 3D‑printable, or manufacturable at a reasonable cost, thus repairs and replacements remain feasible. Open development also streamlines bug fixes and issue tracking because contributors can help accelerate software improvements. And if we for some reason stop developing new updates or cannot contribute anymore, community still remains along with the source code and all the production files, keeping the project alive.

## Licensing and Legal Notice

**Copyright © 2026 MB Daugdara. All rights reserved.**

**Trademarks:**
- Touchord™ is a trademark of MB Daugdara
- Touchord™ logo is a trademark of MB Daugdara

**Intellectual Property:**
- The Touchord name and logo are the exclusive property of MB Daugdara
- Firmware is licensed under GNU General Public License v3.0
- The 3D model design files and PCB Files are licensed under CERN-OHL-S-2.0
- The license does not grant rights to use the company name or logo

**Restrictions:**
- You may not use the company name or logo to endorse derivative commercial works
- You must include this notice when distributing modified versions
- See LICENSE file for full CERN-OHL-S-2.0 terms


## Features

- **Three Operating Modes:**
  - **Strum Mode:** Harp style playing for plucked synths.
  - **Omni Mode:** An omni-chord style play mode mimicking the strumming of an omnichord.
  - **Composer Mode:** Allows adjustment of chord qualities directly via the touch bar for chord customization.
  - **Perform Mode:** Uses the touch bar’s pressure sensitivity for expressive live performances.
  - **Drum Mode:** A simple mode with chromatically mapped keys and the touch bar controlling velocity.

- **Chord Layout:**
  - Each key corresponds to a chord degree in a musical key.
  - Supports custom musical keys and customizable chords on each key.

- **Hardware:**
  - Uses the RP2350 microcontroller chip.
  - Programmed with the PicoSDK in C.
  - Cherry MX style mechanical switches.
  - 6 control buttons.
  - 7 chord keys sized 3u, equipped with 3u plate-mounted stabilizers for enhanced stability.
  - Touch bar: [Trill Bar by Bella](https://bela.io/products/trill/) ([GitHub](https://github.com/BelaPlatform/Trill)) providing multi-touch and pressure sensing.
  - Display: Standard 0.96" 128x64 OLED SSD1306 screen.

- **Enclosure:**
  - Combination of 3D printed carbon fiber reinforced PETG base and CNC machined aluminum top.
  - Optionally, the top can also be fabricated via 3D printing.

- **Connectivity:**
  - USB Type-C MIDI support.
  - TRS MIDI support (User switchable in software):
    - **TRS Type A (Standard MIDI TRS)**
    - **TRS Type B (Roland TRS MIDI)**

---

## Contribution

Check `CONTRIBUTING.md` for more info.

### AI Usage

AI usage on Bug Fixes and Code Auditing is welcome, if done with proper testing, cleanup and no blind trust. Please disclose AI usage in Pull Requests.
Features are better left for humans to write.

---

## Showcase

[![Showcase video](https://img.youtube.com/vi/dFS27xT5Rgg/0.jpg)](https://www.youtube.com/watch?v=dFS27xT5Rgg)
![Touchord On a grander scale](./img/DSC_5172.jpg)
![Touchord PCB](./img/Touchord%20Board%20Visual.png)

## User Showcase
![User Made Touchord with cool colors](./img/UserMade1.jpeg)

## Projects based on Touchord

### "Ghetto" Touchord made using Pico 2W by [nlpeeee](https://github.com/nlpeeee/TCw)
!["Ghetto Touchord"](./img/ghettotouchard.jpeg)

## Special Thanks

I wanted to share my experience with [PCBWay](https://www.pcbway.com/), as I used it for prototyping and although they have helped me produce one of the batches at their cost, this opinion is my own. My choice to go with PCB Way was first, and only then did I ask for their help.

They have been an amazing experience, especially comparing to their biggest competitors, as for the same price I was able to manufacture boards of way better quality. I couldn't have made this project without PCBA services, as soldering small footprint components are too time consuming and expensive to setup.

What I have found is that the quality of thru-holes, soldering are way better and feel like an actual professional product instead of a draft board. Also, the fact that components can be sourced from anywhere, it doesn't have to be a specific selection of components and other components costing more than a standard price, unlike their competitors. Also, their care of the customer surprised me as I was asked multiple times, whether the components I chose are the right ones (There have been issues with the BOM), and one of the more suprising questions I got was whether the color of soldermask could be changes, as properties of black soldermask wouldn'tve played well with my design. That was amazing, as other service providers just apply blind trust and manufacture the board.

Overall, an amazing experience, especially for anyone looking to prototype PCB's that don't feel like toys, guaranteeing that what you designed will be what you get.

---

*Find the Trill Bar by Bella:* [https://bela.io/products/trill/](https://bela.io/products/trill/)
*Trill Bar GitHub repository:* [https://github.com/bela-platform/trill](https://github.com/BelaPlatform/Trill)
