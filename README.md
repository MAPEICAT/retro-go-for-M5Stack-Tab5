# retro-go for M5Stack Tab5

把 [retro-go](https://github.com/ducalex/retro-go) 移植到 **M5Stack Tab5**（ESP32-P4），
让这台横屏设备能横着玩 GBA —— 并且能用**有线 USB 手柄**。

> **本项目不是 retro-go 本身**，而是它的一个设备移植。
> 上游与原作者见下方[致谢](#致谢)。

---

## 这是什么

一套让 M5Stack Tab5 跑 retro-go 的固件，主要做了这些事：

| 功能 | 说明 |
|---|---|
| **横屏** | Tab5 的屏幕面板原生是竖屏 720×1280，本移植用驱动做 90° 转置，实现 1280×720 横屏。方向可在设置里切换（CW90 / CCW90），会记住你的选择。 |
| **触摸虚拟手柄** | 在左右两侧 margin 生成十字键 / ABXY / L / R / 系统键，带透明度与 4 套皮肤。 |
| **隐藏按键按钮** | 点一下把全部虚拟按键收起，只留一个图标恢复 —— 纯 UI 操作，手柄在身时很实用。 |
| **有线 USB 手柄** | 支持 Nintendo Switch Pro 协议手柄（含良值 L1031 等）。按键布局由真机逐键实测确认。 |
| **实时帧率** | 右上角显示真实输出帧率，可在设置里关掉。 |
| **音频修复** | 修掉 ESP8388 上的底噪/杂音（根因是 BSP 无条件启用了与输出抢时钟域的麦克风通道）。 |
| **编译优化** | `-Os` → `-O2`，模拟器这类计算密集负载受益明显。 |

已验证：GBA《精灵宝可梦绿宝石》满帧运行（`FPS:60`，跳帧 0），手柄全按键可用。

---

## 快速开始

### 烧录

设备用 USB-C 连电脑，按 [`docs/FLASHING.md`](docs/FLASHING.md) 操作：

```bash
python -m esptool --chip esp32p4 --port COM16 --baud 921600 \
    write_flash --flash_size detect 0x0 firmware/retro-go_v0.4.7-tab5.img
```

看到 `Hash of data verified.` 即成功。然后**按一下复位键**启动
（Tab5 的 DTR/RTS 没接到 EN，软件复位对它无效）。

### 目录结构

SD 卡根目录下的 retro-go 目录按上游标准结构：

```
/sd/retro-go/
├── bios/          BIOS 文件（部分核心需要）
├── roms/          游戏 ROM，按系统分目录
│   ├── gba/
│   ├── gb/  gbc/  nes/  sms/
│   └── ...
└── saves/         存档（自动创建）
```

完整说明见上游 [retro-go README](https://github.com/ducalex/retro-go#usage)。

### 第一次进游戏

1. 插上手柄（USB-A 口，手柄电源灯亮着）
2. 进游戏列表，按 **HOME** 呼出菜单 → **Emulator options** → 选择核心
3. 玩

---

## 手柄

**⚠️ 只能用有线。**

M5Stack Tab5 板载的 ESP32-C6 **仅支持 BLE（低功耗蓝牙），不支持经典蓝牙（BR/EDR）**
（已向 M5Stack 官方确认）。

Xbox / Switch 系手柄的无线链路走的是经典蓝牙，所以在这台设备上**物理不可行**。
USB-A 口是目前唯一的方式。

支持的手柄：任何在 USB 下枚举为 **Nintendo Switch Pro Controller**
（VID `057e`）的手柄，包括但不限于良值 IINE L1031 / IINE GAMEPAD。

按键映射见 [`docs/FLASHING.md`](docs/FLASHING.md#3-按键映射)。

屏幕上的虚拟按键和 USB 手柄**可以同时使用**；点 L/R 之间那颗按钮可做
X/Y ↔ L/R 对调，**两种输入源同步生效**。

---

## 已知限制

| 现象 | 原因 |
|---|---|
| **部分汉化/改版 GBA 游戏启动即崩** | 该 ROM 触发 gpsp 动态重编译器未实现的指令或特殊 mapper。换非改版 ROM 即可正常游玩。这是上游核心的已知弱项。 |
| 重负载场景偶发掉帧 | GBA 核心本身占用约 90% CPU，横屏转置另需 25~30%。静止画面满帧。 |
| 蓝牙手柄不可用 | 硬件限制，见上。 |
| **不要把 CPU 主频改成 400MHz** | esp32p4-eco2 上会 bootloader 阶段 assert 死循环。固定 360MHz。 |

---

## 从源码编译

见 [`docs/BUILDING.md`](docs/BUILDING.md)。

```bash
git clone https://github.com/ducalex/retro-go.git
cd retro-go
git apply patches/tab5-port-changes.patch
mkdir -p retro-go-p4/components/retro-go/drivers/usbhid
cp source/drivers/usbhid/rg_usbhid.{c,h} retro-go-p4/components/retro-go/drivers/usbhid/
cp source/*.py ./

cd retro-go-p4
python rg_tool.py --target tab5 build-img launcher gbsp --no-networking
```

需要 ESP-IDF **5.5.2**。

---

## 仓库内容

```
firmware/retro-go_v0.4.7-tab5.img     可直接烧录的固件
docs/FLASHING.md                        烧录与使用说明
docs/BUILDING.md                        编译指南
docs/NOTES.md                           改动说明与踩坑记录 ← 最有价值的一篇
patches/tab5-port-changes.patch         相对上游的完整 diff
source/drivers/usbhid/                  新增的 USB 手柄驱动
source/verify_fps.py                    FPS 坐标变换验证脚本
source/test_orientation.py              显示映射验证脚本
source/serial_tail.py                   串口日志抓取（带自动重连）
```

---

## 文档

| 文档 | 内容 |
|---|---|
| [`docs/FLASHING.md`](docs/FLASHING.md) | 烧录步骤、手柄说明、设置项、已知限制 |
| [`docs/BUILDING.md`](docs/BUILDING.md) | 编译环境、参数、验证工具、常见构建问题 |
| [`docs/NOTES.md`](docs/NOTES.md) | **每个技术决策的原因，以及踩过的坑** |

`NOTES.md` 记录了横屏方向、触摸映射、USB 握手、ESP-IDF 陷阱等 6 大类问题，
包括一些"看起来像硬件故障、实际是必填字段漏了"这类坑。改代码前建议先读。

---

## 许可

本移植遵循上游 retro-go 的 **GPLv2** 许可。

代码来源于 [ducalex/retro-go](https://github.com/ducalex/retro-go)（GPLv2）；
JIT 缓存的 MMU 映射方案参考了 [Irak4t0n/HowBoyAdvance](https://github.com/Irak4t0n/HowBoyAdvance)（GPLv2）；
USB HID host 组件来自 Espressif（Apache-2.0）。

---

## 致谢

**本项目完全建立在 [ducalex/retro-go](https://github.com/ducalex/retro-go) 之上。**

### 首要致谢

**[Alex（ducalex）](https://github.com/ducalex) —— retro-go 的作者。**

retro-go 是一个极其优秀的项目：把十多个模拟器塞进 ESP32 这种资源极其有限的芯片里，
而且在 CPU、内存、flash 上都做了深度优化，同时保持兼容性。作者投入了大量心血，
让 ODROID-GO、MyRetroGameCase G32 等设备变成了可用的复古游戏机。

本移植只解决了 retro-go 原生不支持的一类设备（M5Stack Tab5 / ESP32-P4 横屏），
**所有模拟器、启动器、菜单、存档系统等核心功能都来自上游**。
没有 Alex 的工作，这个移植根本无从谈起。

**再次感谢 Alex，以及 retro-go 的所有贡献者。**

### retro-go 整合的上游项目

（详见 retro-go 的 `README.md` Acknowledgements 一节）

- [crashoverride/go-play](https://github.com/othercrashoverride/go-play)（Triforce fork）— NES/GBC/SMS 模拟器与基础库
- [pelle7/odroid-go-emu-launcher](https://github.com/pelle7/odroid-go-emu-launcher) — 启动器设计灵感
- [kallisti5/huexpress](https://github.com/kallisti5/huexpress) — PCE-GO 来源
- [libretro/libretro-handy](https://github.com/libretro/libretro-handy) — Lynx 模拟器
- [libretro/snes9x2005](https://github.com/libretro/snes9x2005) — SNES 模拟器
- [PrBoom 2.5.0](http://prboom.sourceforge.net/) — DOOM 引擎
- [bzhxx/gwenesis](https://github.com/bzhxx/gwenesis/) — Genesis 模拟器
- [bzhxx/lcd-game-emulator](https://github.com/bzhxx/lcd-game-emulator/) — Game & Watch 模拟器
- [Marat Fayzullin 的 fMSX](https://fms.komkon.org/fMSX/) — MSX 模拟器
- [lvandeve/lodepng](https://github.com/lvandeve/lodepng/) — PNG 支持
- [Christian_Haitian](https://github.com/christianhaitian) — PCE 封面图

感谢这些项目的作者们。

### 本移植参考的项目

- [Irak4t0n/HowBoyAdvance](https://github.com/Irak4t0n/HowBoyAdvance)（GPLv2）—
  其 ESP32-P4 版本的 **JIT 缓存 MMU 可执行映射方案**，是 `map_jit_block()` 的直接来源
- [espressif/usb_host_hid](https://github.com/espressif/usb_host_hid) — USB HID host 组件
- [M5Stack Tab5 BSP](https://github.com/m5stack/M5StackTab5) — 屏幕 / 音频 / IO 扩展器驱动
- [Espressif ESP-IDF](https://github.com/espressif/esp-idf) — 构建框架

---

## 反馈

遇到问题请附上串口日志（115200）：

```bash
python source/serial_tail.py COM16 115200 debug.log
```

`serial_tail.py` 带自动重连 —— 设备重启导致串口消失时会自动重连，
不会静默丢掉崩溃那一瞬间的输出（这往往正是关键信息）。