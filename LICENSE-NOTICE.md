# retro-go for M5Stack Tab5 — 上游许可与来源说明

[English](LICENSE-NOTICE.en.md) | 简体中文

本仓库是一个**设备移植**，不包含上游模拟器代码的重新分发。
使用前请了解来源与许可。

---

## 1. 本仓库包含什么

| 内容 | 来源 | 许可 |
|---|---|---|
| `firmware/retro-go_v0.4.7-tab5.img` | **编译产物**（二进制镜像） | GPLv2（因其链接了 GPL 组件） |
| `docs/*` | 本移植原创文档 | 随本仓库 |
| `patches/tab5-port-changes.patch` | 本移植相对上游的 diff（**不含上游原始代码**） | GPLv2 |
| `source/drivers/usbhid/*` | 本移植新增的 USB 手柄驱动 | GPLv2 |
| `source/*.py` | 本移植新增的验证 / 日志脚本 | GPLv2 |

**本仓库不含** `components/retro-go/` 的上游原始实现，也不含 `vendor/`。
需要完整源码请自行克隆 [ducalex/retro-go](https://github.com/ducalex/retro-go)。

### 1.1 衍生关系

本仓库不是从零开始的。它是**在已有 Tab5 竖屏移植之上**继续做的横屏移植：

```
ducalex/retro-go  (Alex, GPLv2)
        └── andjiang0083/retro-go-tab5  (Tab5 竖屏基线, v0.4 – v0.4.7, GPLv2)
                └── 本仓库 (横屏 + USB 手柄, GPLv2)
```

`patches/tab5-port-changes.patch` 只包含**本次横屏移植新增的改动**，
不含 andjiang0083 已完成的竖屏工作 —— 那部分代码属于他的贡献。

---

## 2. 上游项目

### 2.1 retro-go

**retro-go** — https://github.com/ducalex/retro-go
作者：**Alex（ducalex）**
许可：**GNU GPL v2**

```
Copyright (C) 2020-2024 ducalex
```

retro-go 是本移植的**最上游基础**。所有模拟器、启动器、菜单、存档、
设置系统均由 Alex 及 retro-go 贡献者实现。

retro-go 本身又整合了大量开源项目（模拟器、PNG 库、封面图等），
各自保留原许可。完整清单见上游仓库的 `README.md` 与 `CREDITS.md`。

**特别说明**：retro-go 内含 GPL 与非 GPL（多为 BSD/LGPL）的第三方组件，
因此整体以 **GPLv2** 发布。使用、修改或再分发本固件时，
请遵守 GPLv2 的条款（最 notably：保留版权声明、源码开放、署名）。

### 2.2 Tab5 竖屏基线移植

**retro-go-tab5** — https://github.com/andjiang0083/retro-go-tab5
作者：**andjiang0083**
许可：**GNU GPL v2**（承袭 retro-go）

这是本仓库的直接上游基线 —— retro-go 进入 M5Stack Tab5 这台设备的第一步由他完成，
已发布 v0.4 至 v0.4.7 共 7 个 Release。

**来自该仓库的工作**（本仓库沿用，未重新实现）：

| 内容 | 说明 |
|---|---|
| Tab5 目标定义 | `targets/tab5/` 板级配置与分区布局 |
| ST7123 显示驱动 | MIPI-DSI 面板初始化、帧缓冲与 DMA 路径 |
| ES8388 音频驱动 | I2S 音频输出通路 |
| SD 卡挂载 | SDMMC 初始化与片上 LDO 供电处理 |
| 双 app 分区结构 | launcher / gbsp 分区表与切换逻辑 |
| GBA dynarec 周期记账对齐 | 修复动态重编译器的等待周期口径，解决"按键不响应" |
| GBA 双缓冲 | 修复快速移动时的"横条闪烁" |
| 存档位恢复修复 | 补上 `RG_BOOT_RESUME` 消费逻辑 |
| 零标签页崩溃防护 | 修复核心分区缺失导致的空指针崩溃 |

本仓库在此基础上新增：横屏转置、USB 手柄驱动（Nintendo Switch Pro 协议）、
虚拟按键与 HIDE 开关、实时 FPS 显示、ES8388 杂音修复。

> GPLv2 要求保留版权声明与署名。上述两部分贡献均已在此明确标注。

---

## 3. 本移植参考的其它项目

| 项目 | 用途 | 许可 |
|---|---|---|
| [Irak4t0n/HowBoyAdvance](https://github.com/Irak4t0n/HowBoyAdvance) | 其 ESP32-P4 版本的 **JIT 缓存 MMU 可执行映射方案**，本移植 `memmap.c::map_jit_block()` 直接借鉴 | GPLv2 |
| [espressif/usb_host_hid](https://github.com/espressif/usb_host_hid) | USB HID host 组件（vendor 目录内，非本仓库分发） | Apache-2.0 |
| [M5StackTab5](https://github.com/m5stack/M5StackTab5) | Tab5 官方 BSP：ST7123 屏幕 / ES8388 音频 / PI4IOE 扩展器驱动 | 见上游仓库 |
| [Espressif ESP-IDF](https://github.com/espressif/esp-idf) | 构建框架、ESP32-P4 SDK | Apache-2.0 |

> ⚠ **HowBoyAdvance 是 GPLv2 项目**，本移植借鉴其代码的部分同样受 GPLv2 约束。
> 相关来源已在本移植源码的注释中标注。

---

## 4. 商标

- **M5Stack / Tab5** 是 M5Stack 公司的产品与商标。本项目是**非官方的第三方移植**，
  与 M5Stack 无隶属或背书关系。
- 各游戏机的商标归各自权利人所有。本仓库不包含任何游戏 ROM。

---

## 5. 作者致谢

**本移植的存在建立在两位前辈的工作之上。**

**Alex（ducalex）** —— retro-go 的作者。retro-go 把十多个模拟器塞进
ESP32 这种资源极其有限的芯片，并在 CPU、内存、flash 上做了深度优化
同时保持兼容性 —— 这是一项了不起的工作。没有它，本项目无从谈起。

**andjiang0083** —— Tab5 竖屏基线移植的作者。正是他把 retro-go 带进了
M5Stack Tab5：板级移植、显示与音频驱动、SD 挂载、分区结构，
以及几个关键的 GBA 核心修复（dynarec 周期记账、双缓冲、存档位恢复）。
本次横屏移植是在他的成果之上继续做的。

本移植只解决了 retro-go 原生不支持的横屏形态与若干外围设备问题。所有核心功能均来自上游。

**感谢 Alex 与 retro-go 的所有贡献者，感谢 andjiang0083，
以及 retro-go 所整合的每一个开源项目的作者。**