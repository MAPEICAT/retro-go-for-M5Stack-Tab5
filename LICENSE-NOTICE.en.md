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

---

## 2. Upstream project

**retro-go** — https://github.com/ducalex/retro-go
Author: **Alex (ducalex)**
License: **GNU GPL v2**

```
Copyright (C) 2020-2024 ducalex
```

retro-go is the **sole foundation** of this port. Every emulator, the launcher, the menus,
save states and the settings system are implemented by Alex and the retro-go contributors.

retro-go itself integrates many open-source projects (emulators, PNG libraries, cover art, etc.),
each keeping its own license. For the complete list, see the upstream `README.md` and `CREDITS.md`.

**Important note**: retro-go contains both GPL and non-GPL (mostly BSD/LGPL) third-party components,
so the whole project is distributed under **GPLv2**. When using, modifying or redistributing this
firmware, comply with GPLv2 — most notably: retain the copyright notices, keep the source available,
and give attribution.

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

**This port exists entirely because of Alex (ducalex) and retro-go.**

retro-go fits a dozen-plus emulators onto an ESP32 — a chip with extremely limited resources — and
deeply optimizes CPU, memory and flash while retaining compatibility. That is remarkable work.

This port only addresses one hardware class retro-go does not natively support (landscape ESP32-P4
devices) plus a few peripheral issues. All core functionality comes from upstream.

**Thanks to Alex, to all retro-go contributors, and to the authors of every open-source project
integrated into retro-go.**