# Flashing Guide (M5Stack Tab5 / ESP32-P4)

[English](FLASHING.en.md) | [简体中文](FLASHING.md)

This guide applies to `retro-go_v0.4.7-tab5.img` as shipped in this repository.

---

## 1. Prerequisites

| Item | Requirement |
|---|---|
| Device | M5Stack Tab5 (ESP32-P4, rev v1.3, 16MB flash) |
| Host | Windows / macOS / Linux, Python 3.8+ |
| Tools | [ESP-IDF 5.5.2](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32p4/index.html) (only needed to rebuild; not needed to just flash) |
| Cable | **A USB-C data cable** (must support data, not charge-only) |

---

## 2. Flashing

### 2.1 Identify the device and port

Connect the Tab5 to the computer with a USB-C cable. On Windows it enumerates as a serial port
(for example `COM16`) — check **Device Manager → Ports (COM & LPT)**, or:

```powershell
Get-PnpDevice -Class Ports | Select-Object FriendlyName, InstanceId, Status
```

On Linux it is `/dev/ttyACM0`; on macOS, `/dev/cu.usbmodem*`.

> **Note**: the Tab5 uses the ESP32-P4's native USB-Serial/JTAG peripheral, so esptool's trailing
> *"Hard resetting via RTS pin"* has **no effect** — DTR/RTS are not wired to EN/IO0.
> After flashing you must **press the reset button manually** (or power-cycle) to boot the new firmware.

### 2.2 Flash with esptool

```bash
# Locate your esptool (three common options, pick one)
python -m esptool                 # if you pip-installed esptool
~/.espressif/python_env/*/bin/python -m esptool   # bundled with ESP-IDF

# Flash (replace PORT with your actual port)
python -m esptool --chip esp32p4 --port COM16 --baud 921600 \
    write_flash --flash_size detect 0x0 retro-go_v0.4.7-tab5.img
```

Linux / macOS:

```bash
python3 -m esptool --chip esp32p4 --port /dev/ttyACM0 --baud 921600 \
    write_flash --flash_size detect 0x0 retro-go_v0.4.7-tab5.img
```

**Success looks like:**

```
Wrote 2424832 bytes ...
Hash of data verified.
Hard resetting via RTS pin...
```

The write is complete when you see `Hash of data verified.` (the final reset line is inert on this
device — see above).

### 2.3 Verify the image (optional but recommended)

```bash
sha256sum retro-go_v0.4.7-tab5.img
# Windows PowerShell:
Get-FileHash retro-go_v0.4.7-tab5.img -Algorithm SHA256
```

The expected value is in `firmware/SHA256SUMS.txt`.

### 2.4 Boot

Press the Tab5's reset button (or power-cycle).

On first boot you should see the following in the log (115200 baud):

```
launcher v0.4.7-landscape-dirty (Oct 10 2026 23:37:34)
[info] lcd_init: Tab5 DSI ready (ST7123 path): logical 1280x720 -> physical 720x1280, orient=CCW90
[info] USBHID: ready (host + HID class driver up), plug a pad into the USB-A port
```

---

## 3. Gamepads (important)

### 3.1 Wired only

**The ESP32-C6 on the M5Stack Tab5 supports BLE (Low Energy Bluetooth) only, and does not support
Classic Bluetooth (BR/EDR)** — confirmed with M5Stack.

The **wireless** link on Xbox / Nintendo Switch–family controllers runs over Classic Bluetooth, so
such controllers **cannot connect to the Tab5 over Bluetooth**. The USB-A port is currently the only
viable input path.

### 3.2 The controller enumerates in two modes

Controllers such as the IINE L1031 (IINE GAMEPAD) enumerate over USB as a
**Nintendo Switch Pro Controller** (VID `057e`, PID `2009`).
This firmware fully implements that protocol's handshake and report parsing:

```
Nintendo controller detected (in=0x81 mps=64 out=0x01)
claimed iface=0 alt=0
SW -> 80/04 disable-usb-timeout
SW -> 01/03/30 full-report-mode
SW -> 01/10/02 set-playing
Nintendo handshake DONE (3 frames)
*** first 0x30 input report — controller IS streaming ***
```

**The criterion**: if `first 0x30 input report` appears in the log, the gamepad works.

### 3.3 Button mapping

| Gamepad button | retro-go / GBA | Notes |
|---|---|---|
| A | A | |
| B | B | |
| X | X | **Turbo A** (GBA has no native X/Y; the core borrows these bits for rapid fire) |
| Y | Y | **Turbo B** |
| L | L | |
| R | R | |
| ZL | L | GBA has no dedicated trigger bits — merged with L |
| ZR | R | Same |
| Minus | SELECT | |
| Plus | START | |
| Home | MENU | opens the retro-go menu |
| L3 | MENU | |
| R3 | OPTION | |
| Left stick / D-pad | Direction | Both available, use whichever you prefer |

### 3.4 If buttons do nothing

1. **Confirm the port first.** The Tab5 has two USB-C ports (one for charging/debug, one USB-A host).
   The gamepad goes in the **USB-A** port (the wide rectangular one).
2. **Confirm the gamepad is powered on.** Some controllers must be long-pressed to power on before
   the cable is connected.
3. **Read the serial log.** You should see `first 0x30 input report`. If you only get
   `device connected` without that line, the handshake did not complete.

---

## 4. Screen settings

Go to **Options → Screen flip** to change the landscape orientation (`CW90` / `CCW90`). Pick
**whichever one actually looks upright to you**; it is stored in NVS and survives power cycles.

> The firmware shipped here defaults to **CCW90**, which is upright in the normal
> "d-pad at the bottom" grip orientation.

Other relevant settings (same Options menu):

| Option | Description |
|---|---|
| `Screen flip` | Landscape orientation: CW90 / CCW90 |
| `Show FPS` | Live frame rate in the top-right corner (on by default, stored in NVS) |
| `Touch opacity` | Virtual button opacity |
| `Touch skin` | Virtual button skin |

### Virtual button controls

| Button | Function |
|---|---|
| Icon in the top-left | Show / hide all virtual buttons |
| `HIDE` below the D-pad | Same (when hidden, the top-left icon is the way back) |
| The button between L and R | Swap X/Y ↔ L/R (**synchronised** across on-screen buttons and USB gamepad) |

---

## 5. Known limitations

| Symptom | Cause |
|---|---|
| **Some fan-translated / hack GBA games crash on launch** | The ROM triggers an instruction or special mapper the dynarec does not implement. **Swap in a non-hacked ROM and it plays fine.** This is a known weakness of the upstream gpsp core, unrelated to this port. |
| Occasional frame drops under heavy load | The GBA dynarec alone uses ~90% CPU, and landscape transposition needs another 25–30%. Static scenes hold full speed. |
| Bluetooth gamepads do not work | The Tab5 hardware is BLE-only — see §3.1. |

### A note on CPU clock

**Do not set the clock to 400MHz.** On `esp32p4-eco2` this asserts
(`assert failed: 0x40000954`) during the second-stage bootloader and enters a reboot loop —
`cpu_start` is never even reached. This repository pins 360MHz.

---

## 6. Building from source

Full instructions: [`BUILDING.en.md`](BUILDING.en.md).

Short version:

```bash
git clone https://github.com/ducalex/retro-go.git
cd retro-go

# This port's changes
git apply path/to/tab5-port-changes.patch
# New files (USB HID driver, verification scripts) must be copied from source/

cd retro-go-p4
python rg_tool.py --target tab5 build-img launcher gbsp --no-networking
```

Artifact: `retro-go-p4/retro-go_v0.4.7-landscape-dirty_tab5.img`

### Windows build notes

Invoking ESP-IDF directly from Git Bash / MSYS fails (`MSYSTEM` makes `idf_tools.py` fatal, and the
toolchain is not on PATH). `rg_tool.py` handles all three of these:

1. Switches to ESP-IDF's Python venv automatically
2. Clears `MSYSTEM` / `MSYS` / `MINGW` environment variables
3. Injects the directories containing `ccache` and `riscv32-esp-elf-gcc` onto PATH

---

## 7. Getting help

When reporting a problem, attach the serial log (115200 baud). Everything important is in it:

```bash
python serial_tail.py COM16 115200 debug.log
```

`serial_tail.py` has **automatic reconnection** — when the device reboots and the port disappears, it
waits and reconnects instead of silently dropping the log (crash output is often emitted in the
instant before the reboot).