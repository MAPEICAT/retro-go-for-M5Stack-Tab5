# retro-go for M5Stack Tab5

[English](README.en.md) | 简体中文

把 [retro-go](https://github.com/ducalex/retro-go) 装到 **M5Stack Tab5**（ESP32-P4）上的
**平行分支**，主打**有线 USB 手柄**与**音频杂音修复**。

基线是 [andjiang0083/retro-go-tab5](https://github.com/andjiang0083/retro-go-tab5) 的 Tab5 移植。

> **本项目不是 retro-go 本身**，而是它的一个设备移植。
> 上游与原作者见下方[致谢](#致谢)。

---

## 这是什么

**在 [andjiang0083/retro-go-tab5](https://github.com/andjiang0083/retro-go-tab5) 的 Tab5 基线之上，
补上 USB 手柄与音频杂音修复的平行分支。**

真正属于本移植的增量：

| 增量 | 说明 |
|---|---|
| **有线 USB 手柄驱动** | 新增 `drivers/usbhid/`，支持 Nintendo Switch Pro 协议手柄（含良值 L1031 / IINE GAMEPAD）。按键布局由真机逐键实测确认。**上游 `drivers/` 下只有 `audio` 与 `display`，没有这一层。** |
| **音频杂音修复** | 修掉 ES8388 上的底噪（根因是 BSP 无条件启用了与输出抢 I2S 时钟域的麦克风通道）。 |
| **E3 预读拷贝移除** | 那是一次SRAM→SRAM 的零收益搬运，白占48KB 内部 SRAM。移除后 profile 数据也不再忽快忽慢。 |
| **触摸映射修正** | `config.h` 里的恒等映射曾用 `#ifndef` 把 `rg_input.c` 的 90° 逆映射静默屏蔽掉，导致显示转了触摸没转。 |
| **实时帧率显示** | 右上角显示真实输出帧率，可在设置里关闭。 |
| **构建环境修复** | `rg_tool.py` 处理 Windows / Git Bash 下的 `MSYSTEM` 污染、Python venv 切换、工具链 PATH。 |

已验证：GBA《精灵宝可梦绿宝石》满帧运行（`FPS:60`，跳帧 0），手柄全按键可用。

### 与上游 v0.4.9 的重叠，如实说明

`retro-go-tab5` 已于 2026-10-10 发布 **v0.4.9**，其中包含
「一份镜像支持横竖屏切换（方向运行时化）」。

**方向运行时化与本移植早期的横屏方案功能重叠，上游已先一步实现。**
本移植把横屏部分收敛为「与上游一致的运行时切换」（见
[`docs/FLASHING.md`](docs/FLASHING.md) 的 `Screen flip` 设置项），
差异化重点放在上面列的 USB 手柄与音频修复。

两个项目是**平行分支**，不是上下游关系，可各自演进。

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
# ⚠ 补丁基线是 andjiang0083/retro-go-tab5，不是 ducalex/retro-go
git clone https://github.com/andjiang0083/retro-go-tab5.git
cd retro-go-tab5
git apply /path/to/patches/tab5-port-changes.patch
mkdir -p retro-go-p4/components/retro-go/drivers/usbhid
cp source/drivers/usbhid/rg_usbhid.{c,h} retro-go-p4/components/retro-go/drivers/usbhid/
cp source/*.py ./

cd retro-go-p4
python rg_tool.py --target tab5 build-img launcher gbsp --no-networking
```

基线提交 `bee1ef0`。需要 ESP-IDF **5.5.2**。

---

## 仓库内容

```
firmware/retro-go_v0.4.7-tab5.img     可直接烧录的固件
docs/FLASHING.en.md                        烧录与使用说明（英文）
docs/BUILDING.en.md                        编译指南（英文）
docs/NOTES.en.md                           改动说明与踩坑记录（英文）← 最有价值的一篇
patches/tab5-port-changes.patch         相对上游的完整 diff
source/drivers/usbhid/                  新增的 USB 手柄驱动
source/verify_fps.py                    FPS 坐标变换验证脚本
source/test_orientation.py              显示映射验证脚本
source/serial_tail.py                   串口日志抓取（带自动重连）
```

每份文档都有中英两个版本，`.md` 为中文、`.en.md` 为英文，文首可互相切换。

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
Tab5 基线来自 [andjiang0083/retro-go-tab5](https://github.com/andjiang0083/retro-go-tab5)（GPLv2）；
JIT 缓存的 MMU 映射方案参考了 [Irak4t0n/HowBoyAdvance](https://github.com/Irak4t0n/HowBoyAdvance)（GPLv2）；
USB HID host 组件来自 Espressif（Apache-2.0）。

完整归属表见 [`LICENSE-NOTICE.md`](LICENSE-NOTICE.md)。

---

## 致谢

**本项目完全建立在 [ducalex/retro-go](https://github.com/ducalex/retro-go) 之上，
而retro-go 的 Tab5 移植则来自 [andjiang0083](https://github.com/andjiang0083/retro-go-tab5)。**

### 首要致谢

**[Alex（ducalex）](https://github.com/ducalex) —— retro-go 的作者。**

retro-go 是一个极其优秀的项目：把十多个模拟器塞进 ESP32 这种资源极其有限的芯片里，
而且在 CPU、内存、flash 上都做了深度优化，同时保持兼容性。作者投入了大量心血，
让 ODROID-GO、MyRetroGameCase G32 等设备变成了可用的复古游戏机。

本移植只解决了 retro-go 原生不支持的一类设备（M5Stack Tab5 / ESP32-P4 横屏），
**所有模拟器、启动器、菜单、存档系统等核心功能都来自上游**。
没有 Alex 的工作，这个移植根本无从谈起。

**再次感谢 Alex，以及 retro-go 的所有贡献者。**

### Tab5 基线移植：andjiang0083

**本仓库的 Tab5 支持建立在一个已有的 Tab5 移植之上，
由 [andjiang0083](https://github.com/andjiang0083/retro-go-tab5) 完成
（仓库 `github.com/andjiang0083/retro-go-tab5`，GPLv2）。
本仓库是以它为基线的**平行分支**，不是其下游继任。**

来自那份基线工作的内容包括：

| 内容 | 说明 |
|---|---|
| Tab5 目标定义 | `targets/tab5/` 的板级配置与分区布局 |
| ST7123 显示驱动 | MIPI-DSI 面板初始化、帧缓冲与 DMA 路径 |
| ES8388 音频驱动 | I2S 音频输出通路 |
| SD 卡挂载 | SDMMC 初始化与 LDO 供电处理 |
| 双 app 分区结构 | launcher / gbsp 的分区表与切换逻辑 |
| **GBA dynarec 周期记账对齐** | 修好"按键不响应"——把动态重编译器的等待周期口径逐项对齐解释器 |
| **GBA 双缓冲** | 修好快速移动时的"横条闪烁"——建立两块 surface 并在提交后轮换 |
| **存档位恢复修复** | 补上 `RG_BOOT_RESUME` 的消费逻辑，让"继续游戏 + 存档位"生效 |
| 零标签页崩溃防护 | 修好双 app 形态下核心分区缺失导致的空指针崩溃 |
| **方向运行时化（v0.4.9）** | 一份镜像支持横竖屏切换 |
| GBA zip 支持、多机种扩展等 | 见其仓库提交历史 |

> 本移植的补丁以 `retro-go-tab5` 的 `bee1ef0` 为基线。
> 该项目后续发布的v0.4.9（方向运行时化）已覆盖横屏方向切换，
> 本移植因此把横屏部分收敛为与之一致的实现，差异化聚焦 USB 手柄与音频修复。

**本次新增（USB 手柄驱动、虚拟按键与 HIDE 开关、实时 FPS 显示、
音频杂音修复、E3 预读移除、触摸映射修正）在其工作基础上完成。**

如果没有 andjiang0083 把 retro-go 带进 Tab5，这些工作不会有起点。
**在此明确致谢。**

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