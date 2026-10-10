# Build Guide (M5Stack Tab5 / ESP32-P4)

[English](BUILDING.en.md) | [简体中文](BUILDING.md)

This guide explains how to build the firmware shipped in this repository from source.

---

## 1. Environment setup

### 1.1 Required components

| Component | Version | Notes |
|---|---|---|
| [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32p4/index.html) | **5.5.2** | Other versions may fail to build due to sdkconfig / API differences |
| Python | 3.9 – 3.13 | Must be compatible with ESP-IDF's venv |
| Git | any | |

Installing ESP-IDF (Windows):

```powershell
git clone -b v5.5.2 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
install.bat esp32p4
```

After installing, run `export.ps1` (Windows) or `export.sh` (Unix) once in **every new terminal** to
pull in the environment variables.

Verify:

```bash
idf.py --version     # should print v5.5.2
```

### 1.2 M5Stack BSP

This port depends on `vendor/m5stack_tab5`, which ships with the sources and **requires no extra
installation**. It handles initialisation of the ST7123 display, ES8388 audio, the PI4IOE I2C
expander, and USB Host.

---

## 2. Get the sources and apply this port's changes

```bash
git clone https://github.com/ducalex/retro-go.git
cd retro-go
```

### 2.1 Apply the patch

```bash
git apply patches/tab5-port-changes.patch
```

### 2.2 Copy the new files

The patch only contains modifications to existing files. The following files are **entirely new**
and must be copied manually:

| Source (in this repository) | Destination |
|---|---|
| `source/drivers/usbhid/rg_usbhid.c` | `retro-go-p4/components/retro-go/drivers/usbhid/` |
| `source/drivers/usbhid/rg_usbhid.h` | `retro-go-p4/components/retro-go/drivers/usbhid/` |
| `source/verify_fps.py` | `retro-go-tab5-main/` (verification tool, optional) |
| `source/test_orientation.py` | `retro-go-tab5-main/` (mapping verification tool, optional) |
| `source/serial_tail.py` | `retro-go-tab5-main/` (log capture tool, optional) |

```bash
mkdir -p retro-go-p4/components/retro-go/drivers/usbhid
cp source/drivers/usbhid/rg_usbhid.{c,h} retro-go-p4/components/retro-go/drivers/usbhid/
cp source/*.py ./
```

> ⚠ **Do not copy the `vendor/` directory.** It is large and strongly tied to the ESP-IDF version;
> use the copy you already have building successfully locally.

---

## 3. Build

```bash
cd retro-go-p4
python rg_tool.py --target tab5 build-img launcher gbsp --no-networking
```

Flag meanings:

| Flag | Effect |
|---|---|
| `--target tab5` | Target device (M5Stack Tab5) |
| `build-img` | Build the launcher and gbsp apps and merge them into a single image |
| `launcher gbsp` | Build only these two apps (retro-go's basic composition: launcher + game console) |
| `--no-networking` | Skip WiFi / file-manager features (saves flash) |

**Expected duration**: a full first build takes roughly 6–8 minutes (possibly longer on Windows or in
a virtualised environment). Incremental builds take 1–5 minutes.

### 3.1 Artifact

```
retro-go-p4/retro-go_v0.4.7-landscape-dirty_tab5.img
```

This is the complete merged image — **flash it directly to `0x0`**
(see [`FLASHING.en.md`](FLASHING.en.md)).

### 3.2 Building a single app (for debugging)

```bash
python rg_tool.py --target tab5 launcher
python rg_tool.py --target tab5 gbsp
```

Individually-built apps must be flashed against the partition table (launcher at `0x10000`,
gbsp at `0x100000`). There is no need to do this for normal use.

---

## 4. Windows / Git Bash notes

Invoking ESP-IDF directly from Git Bash (MSYS2) hits three problems, all of which `rg_tool.py`
already handles:

| Problem | Symptom | What `rg_tool.py` does |
|---|---|---|
| `MSYSTEM=MINGW64` pollutes the environment | `idf_tools.py` goes fatal; `idf.py` silently exits from `check_environment()` | Clears `MSYSTEM` / `MSYS` / `MINGW` before building |
| Wrong Python | Reports missing `esp_idf_monitor` | Switches to ESP-IDF's venv Python automatically |
| Toolchain not on PATH | ninja fails to spawn `ccache` / `riscv32-esp-elf-gcc` with `CreateProcess failed` | Injects directories under `IDF_TOOLS_PATH/tools` that contain `.exe` files |

**Additionally: do not delete `build/` to force a "clean rebuild"** — the link stage occasionally
hangs on Windows (the process stays alive but produces no output for minutes). The correct approach is
to simply re-run the build command; ninja handles incrementality. If it really is stuck, kill it with
`taskkill /F /IM ninja.exe` and re-run.

---

## 5. Key build configuration

The following are the **notable configuration changes** this port makes relative to upstream. Read
the matching entry in [`NOTES.en.md`](NOTES.en.md) before changing any of them — most were decided
after hitting a specific failure, so do not "optimise" them blindly.

| Setting | Value | File | Notes |
|---|---|---|---|
| Optimisation | `-O2` | `launcher/sdkconfig`, `gbsp/sdkconfig` | Upstream defaults to `-Os`; `-O2` suits compute-bound work like emulator cores |
| CPU clock | **360MHz** | same | ⚠ **Never change to 400MHz** — on `esp32p4-eco2` the bootloader asserts and reboot-loops |
| Build target | `esp32p4` | `rg_tool.py` | |
| Networking | disabled | `--no-networking` | Saves flash |

### 5.1 ⚠ Each app has its own sdkconfig

`retro-go-p4/launcher/` and `retro-go-p4/gbsp/` **each have their own sdkconfig**.
When changing configuration you must change both — if you only change the launcher, the games (which
run inside gbsp) are entirely unaffected.

This is a trap this project hit repeatedly: build, flash, log all look correct, yet performance is
completely unchanged.

---

## 6. Verification tools

### `verify_fps.py` — pixel-level verification of the FPS counter coordinates

```bash
python verify_fps.py
```

It simulates the FPS digits' "logical coordinate → physical coordinate" transform pixel by pixel and
verifies:

1. **Bijectivity** — area is preserved, hence no dropped and no duplicated pixels
2. Drawn pixels ⊆ erase rectangle (guaranteeing that "erase region" and "draw region" always align)
3. No out-of-bounds writes
4. The mapping for all three orientations (linear / CW90 / CCW90)
5. The digits do not overlap the game image
6. The digits do not overlap the swap button / R shoulder button

**You must re-run this after changing the FPS position** — coordinate errors of this kind are not
detectable by eye.

### `test_orientation.py` — display mapping verification

```bash
python test_orientation.py
```

Verifies that the two tiled-transpose paths for CW90 / CCW90 are equivalent to the direct formula and
that `CCW90 == rot180(CW90)`, with no dropped / duplicated / out-of-bounds pixels. Re-run after changing
the display driver.

### `serial_tail.py` — log capture

```bash
python serial_tail.py COM16 115200 debug.log
```

Has automatic reconnection. **Essential for debugging crashes**: every Tab5 panic makes the serial port
vanish and reappear, and a script without reconnection silently loses the most important output.

---

## 7. Common build problems

| Error | Cause and fix |
|---|---|
| `'xxx' undeclared` | The static variable is declared after its use point. C requires forward declarations. |
| `usb/usb_host.h: No such file` | Header paths for `usb` / `usb_host_hid` are **not** propagated via `REQUIRES`; you must add an explicit `target_include_directories` in `components/retro-go/CMakeLists.txt` |
| ninja hangs with no output | `taskkill /F /IM ninja.exe`, then re-run |
| "False success" build (finishes in seconds, artifact unchanged) | Check that `MSYSTEM` has been cleared; confirm the `.obj` timestamps really did update |

---

## 8. Upstream reference

This port is based on [ducalex/retro-go](https://github.com/ducalex/retro-go).
For a detailed account of every change, see [`NOTES.en.md`](NOTES.en.md).