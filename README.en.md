# retro-go for M5Stack Tab5

A **parallel branch** putting [retro-go](https://github.com/ducalex/retro-go) on the
**M5Stack Tab5** (ESP32-P4), focused on **wired USB gamepad support** and an **audio noise fix**.

The baseline is the Tab5 port in
[andjiang0083/retro-go-tab5](https://github.com/andjiang0083/retro-go-tab5).

[简体中文](README.md) | English

> **This project is not retro-go itself.** It is a device port of it.
> See [Credits](#credits) for upstream and original authors.

---

## What this is

**A parallel branch built on the Tab5 baseline of
[andjiang0083/retro-go-tab5](https://github.com/andjiang0083/retro-go-tab5), adding USB gamepad
support and an audio noise fix.**

What is genuinely new in this port:

| Increment | Description |
|---|---|
| **Wired USB gamepad driver** | New `drivers/usbhid/`, supports Nintendo Switch Pro protocol controllers (including IINE L1031 / IINE GAMEPAD). Every button mapping confirmed by hand on hardware. **Upstream's `drivers/` contains only `audio` and `display` — this layer does not exist there.** |
| **Audio noise fix** | Removes the hiss on the ES8388. Root cause: the BSP unconditionally enabled a microphone channel that shared the I2S clock domain with the output. |
| **E3 pre-read copy removal** | It was an SRAM→SRAM copy with zero benefit, occupying 48KB of internal SRAM for nothing. Removing it also stopped the profiling data from flickering between fast and slow. |
| **Touch mapping fix** | An identity mapping in `config.h` was silently masking the correct 90° inverse mapping in `rg_input.c` via `#ifndef`, so the display rotated while touch did not. |
| **Live FPS display** | Real output frame rate in the top-right corner, toggleable in settings. |
| **Build environment fixes** | `rg_tool.py` handles `MSYSTEM` pollution under Windows/Git Bash, Python venv switching, and toolchain PATH. |

Verified: GBA *Pokémon Emerald* runs at full speed (`FPS:60`, 0 frameskip), all gamepad buttons working.

### Overlap with upstream v0.4.9, stated plainly

`retro-go-tab5` shipped **v0.4.9** on 2026-10-10, which includes
"one image supports both orientations (runtime orientation switching)".

**Runtime orientation switching overlaps with this port's original landscape approach; upstream got
there first.** This port narrows the landscape part down to "runtime switching consistent with
upstream" (see the `Screen flip` setting in [`docs/FLASHING.en.md`](docs/FLASHING.en.md)), and puts
its differentiation on the USB gamepad and audio fixes listed above.

The two projects are **parallel branches**, not a parent/child relationship, and can evolve
independently.

---

## Quick start

### Flashing

Connect the Tab5 over USB-C and follow [`docs/FLASHING.en.md`](docs/FLASHING.en.md):

```bash
python -m esptool --chip esp32p4 --port COM16 --baud 921600 \
    write_flash --flash_size detect 0x0 firmware/retro-go_v0.4.7-tab5.img
```

You are done when you see `Hash of data verified.` Then **press the reset button** to boot
(the Tab5 does not wire DTR/RTS to EN, so a software reset does nothing).

### SD card layout

Standard retro-go layout on the SD card root:

```
/sd/retro-go/
├── bios/          BIOS files (needed by some cores)
├── roms/          Game ROMs, one directory per system
│   ├── gba/
│   ├── gb/  gbc/  nes/  sms/
│   └── ...
└── saves/          Save states (created automatically)
```

See the upstream [retro-go README](https://github.com/ducalex/retro-go#usage) for the full description.

### First launch

1. Plug in the gamepad (USB-A port; its power LED should light up)
2. Open the game list, press **HOME** to bring up the menu → **Emulator options** → pick a core
3. Play

---

## Gamepads

**⚠ Wired only.**

The ESP32-C6 on the M5Stack Tab5 is **BLE-only** — it does not support Classic Bluetooth (BR/EDR)
(confirmed with M5Stack). Wireless links on Xbox / Nintendo Switch–family controllers all run over
Classic Bluetooth, so on this device they are **physically impossible**. The USB-A port is currently
the only option.

Supported: any controller that enumerates over USB as a **Nintendo Switch Pro Controller**
(VID `057e`), including but not limited to the IINE L1031 / IINE GAMEPAD.

See [`docs/FLASHING.en.md`](docs/FLASHING.en.md#3-gamepads) for the full button map.

On-screen virtual buttons and the USB gamepad **work simultaneously**. The button between L and R
swaps X/Y ↔ L/R, and the swap applies to **both input sources at once**.

---

## Known limitations

| Symptom | Cause |
|---|---|
| **Some fan-translated / hack GBA games crash on launch** | The ROM hits an instruction or a special mapper the dynarec does not implement. Use a non-hacked ROM and it plays fine. This is a known weakness of the upstream core. |
| Occasional frame drops under heavy load | The GBA core alone occupies ~90% CPU; the landscape transposition needs another 25–30%. Static scenes hold full speed. |
| Bluetooth gamepads do not work | Hardware limitation, see above. |
| **Do not raise the CPU clock to 400MHz** | On `esp32p4-eco2` the bootloader asserts and reboot-loops. Pinned to 360MHz. |

---

## Building from source

See [`docs/BUILDING.en.md`](docs/BUILDING.en.md).

```bash
# ⚠ The patch baseline is andjiang0083/retro-go-tab5, NOT ducalex/retro-go
git clone https://github.com/andjiang0083/retro-go-tab5.git
cd retro-go-tab5
git apply /path/to/patches/tab5-port-changes.patch
mkdir -p retro-go-p4/components/retro-go/drivers/usbhid
cp source/drivers/usbhid/rg_usbhid.{c,h} retro-go-p4/components/retro-go/drivers/usbhid/
cp source/*.py ./

cd retro-go-p4
python rg_tool.py --target tab5 build-img launcher gbsp --no-networking
```

Baseline commit `bee1ef0`. Requires ESP-IDF **5.5.2**.

---

## Repository contents

```
firmware/retro-go_v0.4.7-tab5.img     flashable firmware image
docs/FLASHING.en.md                    flashing & usage guide
docs/BUILDING.en.md                    build guide
docs/NOTES.en.md                       what changed and why ← the most valuable one
patches/tab5-port-changes.patch         complete diff against upstream
source/drivers/usbhid/                  the new USB gamepad driver
source/verify_fps.py                    FPS coordinate-transform verification script
source/test_orientation.py              display mapping verification script
source/serial_tail.py                   serial log capture (with auto-reconnect)
```

Chinese versions of all documents are kept alongside in the same directories.

---

## Documentation

| Document | Contents |
|---|---|
| [`docs/FLASHING.en.md`](docs/FLASHING.en.md) | Flashing steps, gamepad details, settings, known limitations |
| [`docs/BUILDING.en.md`](docs/BUILDING.en.md) | Build environment, flags, verification tools, common build failures |
| [`docs/NOTES.en.md`](docs/NOTES.en.md) | **The reasoning behind every technical decision, and the traps along the way** |

`NOTES.en.md` covers six classes of problem — landscape orientation, touch mapping, USB handshake,
ESP-IDF pitfalls, and a few that "looked like a hardware failure but were actually a missing
mandatory field". Read it before changing anything.

---

## License

This port follows upstream retro-go's **GPLv2** license.

Source derived from [ducalex/retro-go](https://github.com/ducalex/retro-go) (GPLv2);
the Tab5 baseline comes from
[andjiang0083/retro-go-tab5](https://github.com/andjiang0083/retro-go-tab5) (GPLv2);
the JIT cache MMU mapping scheme is referenced from
[Irak4t0n/HowBoyAdvance](https://github.com/Irak4t0n/HowBoyAdvance) (GPLv2);
USB HID host components come from Espressif (Apache-2.0).

See [`LICENSE-NOTICE.en.md`](LICENSE-NOTICE.en.md) for the full attribution table.

---

## Credits

**This project stands on top of [ducalex/retro-go](https://github.com/ducalex/retro-go),
whose Tab5 port comes from
[andjiang0083](https://github.com/andjiang0083/retro-go-tab5).**

### Primary credit

**[Alex (ducalex)](https://github.com/ducalex) — the author of retro-go.**

retro-go is an extraordinary project: it fits a dozen-plus emulators onto a chip as resource-starved
as an ESP32, deeply optimizing CPU, memory and flash while keeping compatibility. The author poured
enormous effort into it, turning devices like the ODROID-GO and MyRetroGameCase G32 into usable
retro gaming machines.

This port only solves one class of hardware retro-go does not natively support
(M5Stack Tab5 / ESP32-P4 landscape screens), plus a handful of peripheral issues.
**Every core feature — emulators, launcher, menus, save system — comes from upstream.**
Without Alex's work, this port would not exist.

**Thank you again, Alex, and to all retro-go contributors.**

### The Tab5 baseline: andjiang0083

**Tab5 support in this repository builds on an existing Tab5 port by
[andjiang0083](https://github.com/andjiang0083/retro-go-tab5) (GPLv2).
This repository is a **parallel branch** based on it, not a downstream successor.**

What comes from that baseline work:

| Contribution | Description |
|---|---|
| Tab5 target definition | Board-level config and partition layout in `targets/tab5/` |
| ST7123 display driver | MIPI-DSI panel init, framebuffer and DMA paths |
| ES8388 audio driver | I2S audio output path |
| SD card mounting | SDMMC init and on-chip LDO power handling |
| Dual-app partition layout | launcher / gbsp partition table and switching logic |
| **GBA dynarec cycle accounting** | Fixed "buttons unresponsive" — aligning the dynamic recompiler's wait-cycle accounting with the interpreter, item by item |
| **GBA double buffering** | Fixed "horizontal tearing" during fast scrolling — two surfaces, rotated after submit |
| **Save-slot resume fix** | Added the missing `RG_BOOT_RESUME` handling so "Continue + save slot" works |
| Zero-tab crash guard | Fixed the null-pointer crash when the core partition is absent |
| **Runtime orientation (v0.4.9)** | One image supporting both orientations |
| GBA zip support, additional systems, etc. | See that repository's commit history |

> This port's patch is based on `bee1ef0` of `retro-go-tab5`. Their later **v0.4.9**
> (runtime orientation) already covers orientation switching, so this port narrows its landscape
> work to an implementation consistent with theirs and focuses its differentiation on the USB gamepad
> and audio fixes.

**The new work here (USB gamepad driver, virtual buttons and the HIDE toggle, live FPS display, audio
noise fix, E3 pre-read removal, touch mapping fix) was completed on top of that foundation.**

Without andjiang0083 bringing retro-go to the Tab5, none of this would have a starting point.
**Explicit thanks are due.**

### Upstream projects integrated into retro-go

(see the `README.md` → Acknowledgements section of the upstream repository)

- [crashoverride/go-play](https://github.com/othercrashoverride/go-play) (Triforce fork) — NES/GBC/SMS emulators and base libraries
- [pelle7/odroid-go-emu-launcher](https://github.com/pelle7/odroid-go-emu-launcher) — inspiration for the launcher design
- [kallisti5/huexpress](https://github.com/kallisti5/huexpress) — source of PCE-GO
- [libretro/libretro-handy](https://github.com/libretro/libretro-handy) — Lynx emulator
- [libretro/snes9x2005](https://github.com/libretro/snes9x2005) — SNES emulator
- [PrBoom 2.5.0](http://prboom.sourceforge.net/) — DOOM engine
- [bzhxx/gwenesis](https://github.com/bzhxx/gwenesis/) — Genesis emulator
- [bzhxx/lcd-game-emulator](https://github.com/bzhxx/lcd-game-emulator/) — Game & Watch emulator
- [Marat Fayzullin's fMSX](https://fms.komkon.org/fMSX/) — MSX emulator
- [lvandeve/lodepng](https://github.com/lvandeve/lodepng/) — PNG support
- [Christian_Haitian](https://github.com/christian_haitian/christian-haitian) — PCE cover art

Thanks to the authors of all of these.

### Projects this port references

- [Irak4t0n/HowBoyAdvance](https://github.com/Irak4t0n/HowBoyAdvance) (GPLv2) — its **JIT cache MMU executable mapping scheme** for ESP32-P4 is the direct origin of `map_jit_block()`
- [espressif/usb_host_hid](https://github.com/espressif/usb_host_hid) — USB HID host component
- [M5Stack Tab5 BSP](https://github.com/m5stack/M5StackTab5) — display / audio / IO expander drivers
- [Espressif ESP-IDF](https://github.com/espressif/esp-idf) — build framework

---

## Reporting issues

Please attach the serial log (115200 baud) — everything important is in there:

```bash
python source/serial_tail.py COM16 115200 debug.log
```

`serial_tail.py` reconnects automatically: when the device reboots and the port disappears, it waits
and reconnects, so you never silently lose the crash output (which is usually exactly the
information you need).