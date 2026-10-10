# M5Stack Tab5 Port — Changes and Gotchas

[English](NOTES.en.md) | [简体中文](NOTES.md)

This document records the changes made while porting [retro-go](https://github.com/ducalex/retro-go)
to the **M5Stack Tab5** (ESP32-P4), and the reasoning behind each decision.

**Many source comments that say "we hit a trap here once" have a full write-up in this document** —
written down so that whoever comes next (including future me) does not have to walk the same ground
again.

---

## Contents

- [Port overview](#port-overview)
- [Hardware constraints (read this first)](#hardware-constraints-read-this-first)
- [Change list](#change-list)
- [Gotchas](#gotchas)
  - [1. Landscape: why PPA hardware rotation is unusable](#1-landscape-why-ppa-hardware-rotation-is-unusable)
  - [2. Landscape orientation: why it is runtime-switchable](#2-landscape-orientation-why-it-is-runtime-switchable)
  - [3. Touch mapping: silently masked by `#ifndef`](#3-touch-mapping-silently-masked-by-ifndef)
  - [4. USB gamepad: three real traps](#4-usb-gamepad-three-real-traps)
  - [5. ESP-IDF's USB Host component has two hidden traps](#5-esp-idfs-usb-host-component-has-two-hidden-traps)
  - [6. GBA crashes: not the system's fault](#6-gba-crashes-not-the-systems-fault)
- [Open items](#open-items)
- [Upstream and credits](#upstream-and-credits)

---

## Port overview

| Item | Value |
|---|---|
| Target device | M5Stack Tab5 |
| SoC | ESP32-P4 (rev v1.3, esp32p4-eco2) |
| Display | ST7123 (720×1280 native portrait panel) |
| Audio | ES8388 codec + I2S |
| Host port | USB-A (USB Host) |
| Wireless | On-board ESP32-C6, **BLE only** |
| Firmware size | 2,424,832 B |
| ESP-IDF | 5.5.2 |
| CPU clock | 360 MHz (**400MHz does not work**) |
| Optimisation | `-O2` |

Main features: landscape mode, touch virtual gamepad, wired USB gamepad, HIDE button toggle, live
frame rate display.

---

## Hardware constraints (read this first)

These three are **physical limitations, not configuration problems**. They determined the shape of
the entire port.

### 1. The panel is natively portrait, so landscape requires software transposition

The Tab5's ST7123 panel is natively 720×1280 portrait, and its MADCTL register reads back `0x00`
(no hardware rotation available). Landscape 1280×720 can therefore only be achieved by having the
driver transpose 90° before writing.

### 2. The Tab5 supports BLE but not Classic Bluetooth

**Confirmed with M5Stack directly.** The on-board ESP32-C6 is a BLE-only chip.

Consequence: the **wireless** link on Xbox / Switch–family controllers runs over Classic Bluetooth
(BR/EDR), which is **physically impossible** on this device. Wired USB-A is the only path.

(As an aside: even with `esp_hosted` plus hand-wired SDIO pins, you would only get BLE, which is
equally useless for these controllers.)

### 3. USB Host runs through the USB-A port, and the `hid_host` component is unusable for the target gamepad

The target controller (IINE L1031 / IINE GAMEPAD) enumerates over USB as a
**Nintendo Switch Pro Controller** (VID `057e`, PID `2009`).
The `usb_host_hid` component only recognises HID-class interfaces and **provides no interrupt OUT
endpoint API**, while the Switch Pro protocol requires interrupt OUT. There was no option but to
implement it directly on top of the `usb_host` library.

---

## Change list

13 files modified, plus 1 new directory. The complete diff is in
`patches/tab5-port-changes.patch`.

### New files

| File | Purpose |
|---|---|
| `components/retro-go/drivers/usbhid/rg_usbhid.c/.h` | USB gamepad driver (XInput + Nintendo dual protocol) |
| `components/retro-go/CMakeLists.txt` | Adds `usb` / `usb_host_hid` dependencies + header paths |
| `targets/tab5/touch_layout.h` | Geometry for the new HIDE button |
| `rg_tool.py` | Windows / Git Bash build environment fixes |

### Landscape display

| File | Change |
|---|---|
| `drivers/display/mipi_dsi_tab5.h` | 90° transposed writes, dual CW90/CCW90 orientations, FPS compositing region |
| `rg_display.c/.h` | `rg_display_get_flip180()` — the single source of truth for landscape orientation (NVS-persisted) |
| `rg_gui.c` | `Screen flip` menu entry |

### Touch virtual gamepad

| File | Change |
|---|---|
| `targets/tab5/config.h` | Fixed the touch mapping that had been wrongly set to identity; enabled the USB HID path |
| `rg_input.c` | Wired up the USB HID input path; HIDE button hit-testing |
| `rg_touch_overlay.c/.h` | Persistent HIDE button; X/Y↔L/R swap synchronised to the USB path; FPS drawing |
| `mipi_dsi_tab5.h` | Removed a 48KB no-op SRAM→SRAM pre-read copy |

### Audio

| File | Change |
|---|---|
| `vendor/m5stack_tab5/m5stack_tab5.c` | Stop creating the RX microphone channel (it contended for the I2S clock domain with TX → noise); deepened the TX DMA buffer |

### GBA core

| File | Change |
|---|---|
| `gbsp/components/gbsp-libretro/cpu_threaded.c` | Removed temporary diagnostic probes |
| `launcher/sdkconfig`, `gbsp/sdkconfig` | `-Os` → `-O2` |

---

## Gotchas

### 1. Landscape: why PPA hardware rotation is unusable

**Conclusion: on this board, PPA's landing spot is inherently worse than the CPU's — not worth doing.**

PPA hardware rotation was tried historically. Measured at ~25ms per tile, while the CPU transposition
of the same tile takes ~1.2ms — 20× slower, which is what dropped the measured frame rate to 1–2fps
at the time.

The root cause is **structural**: PPA's destination `tab5_fb` is the DPI framebuffer at
1280×720×2 = **1.84MB**, which does not even fit in internal SRAM, so it must live in PSRAM — and the
panel is continuously scanning and reading that PSRAM at roughly 88MB/s. The moment PPA writes there it
contends with the peripherals for bandwidth; worse, the operation is blocking, so the CPU just waits.

The CPU path actually has the advantage: the linker map shows the staging buffer `tab5_scratch` lives
in **internal SRAM** (`0x4ff40000`), and only the final step DMA-copies into PSRAM.

This also killed a false optimisation: there was once an "E3 pre-read" mechanism, justified by "the
source data is in PSRAM, so it has to be read into SRAM first" — but `tab5_line_buffer` **already is**
in internal SRAM (`0x4ff54000`). It was an SRAM→SRAM copy of equal size: **zero benefit, pure
overhead**. Worse, it had a threshold (only triggered for tiles ≤48KB), which meant the same code path
had two different costs and profiling data flickered between fast and slow. It has been removed, which
also frees 48KB of internal SRAM.

> **Lesson**: confirm the premise before optimising. The premise "the source is in PSRAM" was wrong,
> and an optimisation built on a false premise disguised itself as "sometimes fast, sometimes slow",
> which is very hard to trace back to it.

---

### 2. Landscape orientation: why it is runtime-switchable

**Conclusion: hard-coding the orientation at compile time is wrong. The correct orientation depends on
how the panel module is physically mounted in the case plus how the user holds the device — the code
cannot derive it.**

How it evolved:

1. Initially written as "90° clockwise" (`logical (lx,ly) → physical (px,py) = (719-ly, lx)`)
2. User reported "upside down" → changed to 90° counter-clockwise with an X flip (`= (ly, 1279-lx)`)
3. User still reported "upside down"

The key reasoning: these two mappings are **180° rotations of each other**. For any single grip
orientation, exactly one of them must be correct — if both report being upside down, then "hard-code
one orientation at compile time" is itself the wrong approach.

There was also a detour along the way: using the D-pad arrow directions to infer screen orientation.
**The D-pad is 180° self-symmetric** (up/down/left/right arrows, together with their positions, look
identical after a 180° flip) so it provides **zero orientation information**. Only text can tell.

Final approach: `rg_display_get_flip180()` as the single source of truth (NVS-persisted), with the
display mapping, the inverse touch mapping, and the virtual button overlay all deriving from it — which
guarantees they stay mutually consistent.

The mapping algorithm itself was verified pixel-by-pixel with a Python simulation (12/12 cases passing,
including non-integer multiples of 32), confirming `CCW90 == rot180(CW90)` with no dropped, duplicated
or out-of-bounds pixels.

> **Lesson**: with no usable accelerometer on the device, screen orientation **can only be confirmed by
> eye**. Rather than guessing, give the user a switch.

---

### 3. Touch mapping: silently masked by `#ifndef`

`targets/tab5/config.h` defined the touch mapping as an **identity mapping**, with a comment labeling
it "【portrait branch】":

```c
#define RG_TOUCH_LOGICAL_FROM_PHYS(px, py, lx, ly) \
    do { (lx) = (px); (ly) = (py); } while (0)
```

Meanwhile the 90° inverse mapping in `rg_input.c` is wrapped in
`#ifndef RG_TOUCH_LOGICAL_FROM_PHYS` — and since **config.h defines it first, the version in
rg_input.c never takes effect.**

Result: the display side rotated 90°, the touch side did not — the two differed by 90°. This also
explains why "changing rg_input.c had no effect whatsoever".

> **Lesson (this class of bug is extremely hard to find)**: a macro's `defined` test only checks
> *whether* it is defined, not what it is defined as. A related instance: while diagnosing, I tried to
> disable a feature with `#if defined(RG_USB_HID_GAMEPAD)` and changed the value to `0` — **which is
> not the same as disabling it.** Both times, wrong conclusions were reached because "the change did
> not actually take effect."
> **Any diagnosis of the form "disable feature X by changing a variable/macro" must verify that the
> change really took effect** (image size change / feature-string count / log lines) — reading the
> edited source is not enough.

---

### 4. USB gamepad: three real traps

Getting from "completely unresponsive" to "usable" spanned 5 versions. Three were real traps.

#### Trap A: `set_report` goes over control transfer, which Switch Pro ignores

The first attempt sent Nintendo subcommands via `hid_class_request_set_report()`
(0x80/04 disable-timeout, 0x01/03/30, 0x01/10/02, etc.). All returned
`Control transfer timeout`.

**Decisive evidence**: after the format was corrected (v5, proper 64-byte frames), the log was
*identical* to when the format was wrong (v3, 3 bytes of garbage) — 0x81×2, 0x21×0, 0x30×0. Changing
the format from wrong to correct necessarily changes behaviour; identical output proves **the
controller was not processing these commands at all.**

Cause: Switch Pro receives commands through the **interrupt OUT endpoint**, whereas
`hid_class_request_set_report()` uses **control transfers** (the USB spec permits devices to merely ACK
a SET_REPORT control transfer without acting on it). Corroboration: Linux `hid-nintendo` and Chromium
both use `hid_write()` — interrupt OUT. Nobody uses control transfers.

#### Trap B: a device handle leak creating "ghost entries"

After the handshake was fixed the device still would not connect, and the log repeatedly reported:

```
E (3695) USB HOST: usbh_devs_open error: ESP_ERR_INVALID_STATE
[warn] USBHID: XI open addr=1 failed
```

Reading the ESP-IDF source revealed the **exact** meaning of `ESP_ERR_INVALID_STATE (0x103)`:

```c
// components/usb/usbh.c  usbh_devs_open()
if (dev_obj->dynamic.flags.is_gone ||           // the device has already been unplugged
        dev_obj->dynamic.flags.waiting_free) { // the device is waiting to be freed
    ret = ESP_ERR_INVALID_STATE;
}
```

**It has nothing to do with "enumeration never completed"** (my initial judgement was wrong). And
`usb_host_device_addr_list_fill()` only lists devices that have finished configuring. Combining the
two:

> The `addr=1` entry on the bus had once enumerated successfully, was later flagged as "unplugged",
> but the object was never reclaimed — a ghost entry.

The real culprit was **my own bug**: `xi_probe()` discovered "this is not XInput" and returned
directly, **never calling `usb_host_device_close()`**. IDF's reclamation rule is "free is only allowed
when `open_count == 0`":

```c
dev_obj->dynamic.flags.is_gone = 1;
if (open_count == 0)  DEV_ACTION_FREE            // reclaim immediately
else                  DEV_ACTION_PROP_GONE_EVT   // wait for the other side to close before reclaiming
```

One leaked handle → `open_count` is never 0 → the device is never reclaimed → every subsequent `open`
returns 0x103.

**So every time the user plugged in the controller, it had in fact enumerated successfully** — my code
locked it dead on the spot, and then I went on hammering that already-dead entry.

> **Lesson**: any "probe → obtain object → assume it is there" flow must ask
> "**does the entity this object represents still exist?**" Handle exists ≠ entity exists.
> This was the third time this project made the same family of mistake (see §6 and the general
> lessons below).

#### Trap C: the IN transfer omitted `num_bytes` — a mandatory field

This one was the most unfair. The symptom was "the controller does not respond at all", which looks
like a hardware or protocol problem; it cost 5 versions before it was found.

Even with the handshake fixed and the interface claimed, the interrupt IN still **received not a
single report**, and the controller kept self-resetting every 2.5 seconds.

The `usb_transfer_t` struct has a mandatory field `num_bytes` (how many bytes to read/write) — I had
omitted it. The moment it was added:

| Metric | Before | After |
|---|---|---|
| Interrupt IN reports | **0** | Streaming continuously |
| `0x30` input reports | **0** | Flowing |
| Controller self-resets | **80** | **1** |
| UI load | BUSY 100% / FPS 4 | BUSY 7% / FPS 103 |

> **Lesson**: when debugging "the device is completely unresponsive", **first verify that every API
> call is complete and correct** (claim? num_bytes? alignment? endpoint direction?), and only then
> suspect hardware and protocol. A missing mandatory field produces symptoms that mimic a hardware
> failure exactly.

#### Aside: the button byte layout must be measured, not copied from the "standard layout"

The first two bytes of each report **are not buttons at all**: `p[0]` is a per-frame incrementing
packet counter and `p[1]` is a constant `0x91` status byte. The "standard Pro layout" I copied
(buttons starting at `p[0]`) was therefore **misaligned by 2 bytes**, so I was reading the counter as
buttons and decoding a set of constant phantom buttons.

**This kind of whole-structure misalignment is invisible when eyeballing a dump** — only differencing
actual button presses pins down the true offset.

Final layout (confirmed by pressing every button on real hardware; byte values correspond 1:1 to the
button order):

| Button | Byte |
|---|---|
| A / B / X / Y | `p2` = `08` / `04` / `02` / `01` |
| R / ZR | `p2` = `40` / `80` |
| L / ZL | `p4` = `40` / `80` |
| D-pad up/down/left/right | `p4` = `02` / `01` / `08` / `04` |
| + / − / Home | `p3` = `82` / `81` / `90` |
| Left stick rest/left/right | LX = `2048` / `≈0` / `≈4092` |

> **Lesson**: neither protocol documentation nor "generic layouts" substitute for real measurement.
> Sampling logic needs care too: the first version printed on a time throttle, and **the sampling
> budget was burned entirely on the counter changing during idle frames**. Switching to "print only
> when the payload changes" nailed it in one go.

---

### 5. ESP-IDF's USB Host component has two hidden traps

#### Trap A: `hid_host` silently claims exclusive access to the device

After `hid_host_install()`, it automatically opens any HID interface it finds.
A device can **only be opened by one client** — so every other access path to the same device is
blocked, and `hid_host` **does not tell you**.

To access non-HID interfaces at the same time (such as XInput's vendor class 0xFF), `hid_host` has to
be removed entirely.

#### Trap B: a registered callback never hears about devices that existed before registration

After `usb_host_client_register()`, only **newly connected** devices generate a `NEW_DEV` event.
If the device was already plugged in before registration, the event never arrives.

You must **enumerate proactively**:

```c
uint8_t addrs[8] = {0};
int n = 0;
usb_host_device_addr_list_fill(8, addrs, &n);
```

#### Aside: header paths are not propagated through REQUIRES

Header paths for `usb` (bundled with IDF) and `usb_host_hid` (vendor) are **not** passed to consumers
via `COMPONENT_REQUIRES`. You must add this explicitly in
`components/retro-go/CMakeLists.txt`:

```cmake
target_include_directories(${COMPONENT_LIB} PRIVATE
    "$ENV{IDF_PATH}/components/usb/include"
    "${CMAKE_CURRENT_LIST_DIR}/../../../vendor/managed_components/espressif__usb_host_hid/include"
)
```

> Watch the path depth: `CMakeLists.txt` lives in `retro-go-p4/components/retro-go/`, so reaching
> `retro-go-tab5-main/vendor/` needs `../../../`.

---

### 6. GBA crashes: not the system's fault

**Symptom**: some GBA games crash on launch (`Instruction access fault`, `MEPC = MTVAL = 0xfffffffe`),
every single launch, 1:1 correspondence.

Investigation (each step verified as a single variable):

| Suspect | How it was verified | Result |
|---|---|---|
| USB gamepad | Unplug the controller | Still crashes |
| USB host stack consuming memory | **Stripped entirely** from the image (image −64KB, feature strings → zero) | Still crashes |
| Boot-time save-loading path | Added a switch to skip `.srm` + save-slot loading | Still crashes |
| JIT infrastructure | Added probes and measured | `ret` normal, `C2M`/`M2C` both `ESP_OK`, 5 consecutive blocks fine |

**Swapping in a non-hacked ROM → zero crashes** (`FPS:60 (0+60+0)`, `skipped=0`, panic count 0).

**Conclusion**: fan-translated / hack ROMs trigger instructions or special mappers that the dynarec
does not implement. This is a known weakness of the upstream gpsp core and unrelated to this port.

Fixing it properly would require adding an "unimplemented instruction → fall back to the interpreter
for that instruction" path to the dynarec, rather than letting it jump to the wild address `-2`. That is
a separate project.

> **Lesson**: the next time a particular game crashes every time, **the first thing to do is swap in
> another ROM as a control**, rather than modifying system code. A checklist-style investigation with
> single-variable verification can rule out every wrong direction — but only once you have a
> **known-good sample** to use as the baseline.

---

### General lessons

These recurred several times in this project and deserve their own list:

1. **After a change, verify the change actually took effect** — image size change / feature string
   count / log lines. Two "false successes" (an `#if defined()` masking a value changed to 0; a 6-second
   build whose artifact never changed) both led to wrong conclusions.

2. **For "register a callback and wait for events" frameworks, ask "will objects that existed before
   I registered ever notify me?"** For USB Host the answer is no — enumerate proactively.

3. **"Probe → fail → give up" must first distinguish transient failure from permanent failure.**
   Transient failures must be retried.

4. **Handle / entry existing ≠ entity existing.** This was the third time this project made that
   family of mistake.

5. **Keep "configuration-style static variables" at the top of the file.** Declaring one after its use
   point fails to compile.

6. **Verification scripts for multi-branch code must exercise each branch with its own input
   boundaries** — otherwise the "failure" comes from the verifier, not the code under test.

7. **Coordinate-transform changes must be verified offline, pixel by pixel.** The human eye cannot
   catch these — this project has now fallen into it twice ("landscape/portrait" and "FPS position").

8. **Inferring orientation from "button order" is unreliable.** The stick's Y axis is the only axis
   that cannot prove itself; have the user confirm by hand rather than leaving a "change this if it's
   backwards" comment and calling it done.

---

## Open items

| Item | Status |
|---|---|
| Crashes on fan-translated / hack GBA ROMs | Unfixed (needs an interpreter fallback in the dynarec) |
| Frame drops under heavy load | Unfixed (the GBA dynarec uses ~90% CPU; landscape transposition needs another 25–30%. Static scenes hold full speed) |
| ZL/ZR → Turbo | Not implemented (currently merged with L/R; GBA has no dedicated trigger bits) |
| Purpose of X/Y | Undecided (currently turbo rapid-fire A/B) |
| Bluetooth gamepads | **Not possible** (hardware is BLE only — see [Hardware constraints](#hardware-constraints-read-this-first)) |

---

## Upstream and credits

This port exists entirely because of **[ducalex/retro-go](https://github.com/ducalex/retro-go)**
(GPLv2). Without Alex (ducalex) and the retro-go contributors, there would be no such firmware.

Upstream also integrates the following open-source projects (see the Acknowledgements section of
retro-go's `README.md`):

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

This port additionally references:

- [Irak4t0n/HowBoyAdvance](https://github.com/Irak4t0n/HowBoyAdvance) (GPLv2) — its P4 JIT cache MMU executable mapping scheme is the basis of this port's `map_jit_block()`
- [espressif/usb_host_hid](https://github.com/espressif/usb_host_hid) — USB HID host component
- [M5Stack Tab5 BSP](https://github.com/m5stack/M5StackTab5) — peripheral drivers

Thanks to everyone who has contributed to retro-go and to these open-source projects.