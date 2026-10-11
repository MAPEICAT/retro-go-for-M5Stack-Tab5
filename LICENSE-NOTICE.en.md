# retro-go for M5Stack Tab5 — Upstream License and Attribution

[English](LICENSE-NOTICE.en.md) | [简体中文](LICENSE-NOTICE.md)

This repository is a **device port**. It does not redistribute upstream emulator code.
Please understand the origins and licensing before use.

---

## 1. What this repository contains

| Content | Origin | License |
|---|---|---|
| `firmware/retro-go_v0.4.7-tab5.img` | **Build artifact** (binary image) | GPLv2 (because it links GPL components) |
| `docs/*` | Documentation original to this port | Included with this repository |
| `patches/tab5-port-changes.patch` | This port's diff against upstream (**contains no upstream source**) | GPLv2 |
| `source/drivers/usbhid/*` | USB gamepad driver, new in this port | GPLv2 |
| `source/*.py` | Verification / logging scripts, new in this port | GPLv2 |

**This repository does not contain** the upstream implementation under `components/retro-go/`,
nor does it contain `vendor/`. For the full source, clone
[ducalex/retro-go](https://github.com/ducalex/retro-go) yourself.

### 1.1 Derivation chain

This repository did not start from zero. It is a **parallel branch** continuing from an existing
Tab5 port:

```
ducalex/retro-go  (Alex, GPLv2)
        └── andjiang0083/retro-go-tab5  (Tab5 baseline, GPLv2)
                ├── andjiang0083/retro-go-tab5 continues (v0.4.8 / v0.4.9 …)
                └── this repository  (USB gamepad + audio fix, GPLv2)   ← parallel branch
```

The **baseline of `patches/tab5-port-changes.patch` is `bee1ef0` of `retro-go-tab5`**, not
`ducalex/retro-go`. See [`patches/BASE.en.md`](patches/BASE.en.md).

⚠ The two are **parallel**. This repository is not a downstream successor to `retro-go-tab5` and
does not replace it. **`retro-go-tab5`'s v0.4.9 (runtime orientation switching)** overlaps with the
landscape part of this port; this port has narrowed its landscape work to an implementation
consistent with it, and focuses its differentiation on the USB gamepad driver and the audio noise fix.

---

## 2. Upstream project

### 2.1 retro-go

**retro-go** — https://github.com/ducalex/retro-go
Author: **Alex (ducalex)**
License: **GNU GPL v2**

```
Copyright (C) 2020-2024 ducalex
```

retro-go is the **topmost upstream foundation** of this port. Every emulator, the launcher, the menus,
save states and the settings system are implemented by Alex and the retro-go contributors.

retro-go itself integrates many open-source projects (emulators, PNG libraries, cover art, etc.),
each keeping its own license. For the complete list, see the upstream `README.md` and `CREDITS.md`.

**Important note**: retro-go contains both GPL and non-GPL (mostly BSD/LGPL) third-party components,
so the whole project is distributed under **GPLv2**. When using, modifying or redistributing this
firmware, comply with GPLv2 — most notably: retain the copyright notices, keep the source available,
and give attribution.

### 2.2 Tab5 baseline port

**retro-go-tab5** — https://github.com/andjiang0083/retro-go-tab5
Author: **andjiang0083**
License: **GNU GPL v2** (inherited from retro-go)

This is this repository's direct upstream baseline — the first step of bringing retro-go to the
M5Stack Tab5 was his work, published across 7 releases from v0.4 to v0.4.7.

**Work that comes from that repository** (carried over here, not re-implemented):

| Contribution | Description |
|---|---|
| Tab5 target definition | Board config and partition layout in `targets/tab5/` |
| ST7123 display driver | MIPI-DSI panel init, framebuffer and DMA paths |
| ES8388 audio driver | I2S audio output path |
| SD card mounting | SDMMC init and on-chip LDO power handling |
| Dual-app partition layout | launcher / gbsp partition table and switching logic |
| GBA dynarec cycle accounting | Fixed the dynamic recompiler's wait-cycle accounting, resolving "buttons unresponsive" |
| GBA double buffering | Fixed horizontal tearing during fast scrolling |
| Save-slot resume fix | Added the missing `RG_BOOT_RESUME` handling |
| Zero-tab crash guard | Fixed the null-pointer crash when the core partition is absent |

New in this repository, on top of that: landscape transposition, the USB gamepad driver
(Nintendo Switch Pro protocol), the virtual gamepad and its HIDE toggle, the live FPS display, and
the ES8388 noise fix.

> GPLv2 requires retaining copyright notices and attribution. Both contributions are credited above.

---

## 3. Other projects referenced by this port

| Project | Use | License |
|---|---|---|
| [Irak4t0n/HowBoyAdvance](https://github.com/Irak4t0n/HowBoyAdvance) | Its ESP32-P4 **JIT cache MMU executable mapping scheme** — `memmap.c::map_jit_block()` in this port is directly adapted from it | GPLv2 |
| [espressif/usb_host_hid](https://github.com/espressif/usb_host_hid) | USB HID host component (in the vendor directory, not distributed here) | Apache-2.0 |
| [M5StackTab5](https://github.com/m5stack/M5StackTab5) | Official Tab5 BSP: ST7123 display / ES8388 audio / PI4IOE expander drivers | See upstream repository |
| [Espressif ESP-IDF](https://github.com/espressif/esp-idf) | Build framework, ESP32-P4 SDK | Apache-2.0 |

> ⚠ **HowBoyAdvance is GPLv2.** The parts of this port adapted from it are equally bound by GPLv2.
> Attribution is noted in the source comments.

---

## 4. Trademarks

- **M5Stack / Tab5** are products and trademarks of M5Stack Inc. This project is an **unofficial
  third-party port**, is not affiliated with, and is not endorsed by, M5Stack.
- Console trademarks belong to their respective rights holders. This repository contains no game ROMs.

---

## 5. Author credits

**This port exists because of the work of two predecessors.**

**Alex (ducalex)** — the author of retro-go. retro-go fits a dozen-plus emulators onto an ESP32 — a
chip with extremely limited resources — and deeply optimizes CPU, memory and flash while retaining
compatibility. That is remarkable work. Without it, this project would not exist.

**andjiang0083** — the author of the Tab5 baseline port. He was the one who brought
retro-go to the M5Stack Tab5: board bring-up, display and audio drivers, SD mounting, partition
layout, and several key GBA core fixes (dynarec cycle accounting, double buffering, save-slot
resume). This landscape port was completed on top of his results.

This port only addresses the landscape form factor retro-go does not natively support, plus a few
peripheral issues. All core functionality comes from upstream.

**Thanks to Alex and all retro-go contributors, to andjiang0083, and to the authors of every
open-source project integrated into retro-go.**