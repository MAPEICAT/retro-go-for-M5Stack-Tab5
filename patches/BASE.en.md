# Patch baseline

[English](BASE.en.md) | [简体中文](BASE.md)

---

## ⚠ Read this first: do not apply this patch to `ducalex/retro-go`

`tab5-port-changes.patch` in this directory is based on **`retro-go-tab5`**, not on
`ducalex/retro-go`. Cloning the latter and running `git apply` **will fail**.

```bash
# ✅ correct
git clone https://github.com/andjiang0083/retro-go-tab5.git
cd retro-go-tab5
git apply /path/to/tab5-port-changes.patch

# ❌ wrong: paths don't match, the patch cannot apply
git clone https://github.com/ducalex/retro-go.git
```

---

## 1. Baseline commit

| Item | Value |
|---|---|
| Repository | [`andjiang0083/retro-go-tab5`](https://github.com/andjiang0083/retro-go-tab5) |
| Branch | `main` |
| Baseline commit | **`bee1ef0`** — `license: 补 SPDX 文件头与第三方通知` |
| License | GPL-2.0 |

If `git apply` fails on a hunk, check the baseline first:

```bash
git log -1              # should be bee1ef0
git apply --check /path/to/tab5-port-changes.patch
```

---

## 2. Why ducalex/retro-go does not work

All 13 files in the patch have paths starting with **`retro-go-p4/`**, for example:

```
retro-go-p4/components/retro-go/rg_display.c
retro-go-p4/components/retro-go/drivers/display/mipi_dsi_tab5.h
retro-go-p4/gbsp/components/gbsp-libretro/cpu_threaded.c
retro-go-p4/rg_tool.py
vendor/m5stack_tab5/m5stack_tab5.c
```

**`ducalex/retro-go` has no `retro-go-p4/` layer** — its P4 port is organised differently —
so the patch cannot find the files.

### Measured data

Checking the baseline context line by line: of the **691** baseline-context lines across the
patch's 13 files,

| Metric | Value |
|---|---|
| Found verbatim in `retro-go-tab5` | **678 lines (98.1%)** |
| Files matching 100% | 10 |

The 10 files matching 100% include:

| File | Match |
|---|---|
| `rg_touch_overlay.c` | 265 / 265 |
| `vendor/m5stack_tab5.c` | 53 / 53 |
| `rg_input.c` | 47 / 47 |
| `rg_gui.c` | 40 / 40 |

This proves the baseline is **`retro-go-tab5`**, not ducalex upstream.

(This data was measured by andjiang0083 in that issue.)

---

## 3. Files that only exist in retro-go-tab5

These files are **present only in `retro-go-tab5`** and absent from `ducalex/retro-go` —
another direct reason a wrong baseline breaks the patch:

- `retro-go-p4/components/retro-go/targets/tab5/touch_layout.h`
- `retro-go-p4/components/retro-go/rg_touch_overlay.c` / `.h`
- `retro-go-p4/rg_tool.py`
- `retro-go-p4/components/retro-go/drivers/display/mipi_dsi_tab5.h`

---

## 4. What this patch contains

13 files, all changes **new in this work** (relative to the `retro-go-tab5` baseline):

| File | Change |
|---|---|
| `components/retro-go/CMakeLists.txt` | Added `usb` / `usb_host_hid` deps + header paths |
| `drivers/display/mipi_dsi_tab5.h` | 90° transposed writes, dual orientation, FPS compositing; removed the E3 pre-read copy |
| `rg_display.c` / `.h` | `rg_display_get_flip180()` — single source of truth for orientation (NVS-persisted) |
| `rg_gui.c` | Orientation menu entry |
| `rg_input.c` | Wired up the USB HID input path; HIDE hit-testing |
| `rg_touch_overlay.c` / `.h` | Persistent HIDE button; X/Y↔L/R swap synced to USB; FPS drawing |
| `targets/tab5/config.h` | Fixed the touch mapping that had been wrongly set to identity |
| `targets/tab5/touch_layout.h` | HIDE button geometry |
| `gbsp/.../cpu_threaded.c` | Removed leftover diagnostic probes |
| `rg_tool.py` | Windows / Git Bash build env fixes |
| `vendor/m5stack_tab5/m5stack_tab5.c` | Stopped creating the I2S RX microphone channel (removes noise) |

### Genuinely new files (not in the patch — copy manually)

The patch only modifies existing files. These are entirely new:

| Source (this repo) | Destination |
|---|---|
| `source/drivers/usbhid/rg_usbhid.c` | `retro-go-p4/components/retro-go/drivers/usbhid/` |
| `source/drivers/usbhid/rg_usbhid.h` | same |
| `source/verify_fps.py` | repo root (verification tool, optional) |
| `source/test_orientation.py` | repo root (verification tool, optional) |
| `source/serial_tail.py` | repo root (logging tool, optional) |

```bash
mkdir -p retro-go-p4/components/retro-go/drivers/usbhid
cp source/drivers/usbhid/rg_usbhid.{c,h} retro-go-p4/components/retro-go/drivers/usbhid/
cp source/*.py ./
```

---

## 5. Relationship to upstream v0.4.9

`retro-go-tab5` shipped **v0.4.9** on 2026-10-10, which includes
"one image supports both orientations (runtime orientation switching)".

**Runtime orientation switching overlaps with the landscape part of this port.** Stating plainly
what each side contributes:

| Feature | Owner |
|---|---|
| Runtime orientation switching | **Already in upstream v0.4.9** |
| **USB gamepad driver** (`drivers/usbhid/`, Nintendo Switch Pro protocol) | **Unique to this port** — upstream's `drivers/` contains only `audio` and `display` |
| **I2S RX noise removal** (kills the ES8388 hiss) | **Unique to this port** |
| **E3 pre-read copy removal** (frees 48KB internal SRAM) | **Unique to this port** |
| Touch mapping fix | This port |

This repository is a **parallel branch**, not a downstream successor to retro-go-tab5. Both can evolve
independently.

---

## 6. Image version banner

`firmware/retro-go_v0.4.7-tab5.img` in this repository was built from this repository's sources.
Its banner reads `RG_TARGET_TAB5-0 v0.4.7-landscape-dirty SDK:v5.5.2-dirty esp32p4`, where
`landscape-dirty` is this port's build marker.

Log strings (`Tab5 DSI ready (ST7123 path)`, `Tab5 audio: ES8388 ready at %d Hz`) are byte-identical
to `retro-go-tab5` — because the baseline is that project. The machine list
(Atari Lynx / ColecoVision / PC Engine / Game & Watch, 11 systems) likewise comes from the baseline.