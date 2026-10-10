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

---

## 2. 上游项目

**retro-go** — https://github.com/ducalex/retro-go
作者：**Alex（ducalex）**
许可：**GNU GPL v2**

```
Copyright (C) 2020-2024 ducalex
```

retro-go 是本移植的**唯一基础**。所有模拟器、启动器、菜单、存档、
设置系统均由 Alex 及 retro-go 贡献者实现。

retro-go 本身又整合了大量开源项目（模拟器、PNG 库、封面图等），
各自保留原许可。完整清单见上游仓库的 `README.md` 与 `CREDITS.md`。

**特别说明**：retro-go 内含 GPL 与非 GPL（多为 BSD/LGPL）的第三方组件，
因此整体以 **GPLv2** 发布。使用、修改或再分发本固件时，
请遵守 GPLv2 的条款（最 notably：保留版权声明、源码开放、署名）。

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

**本移植的存在完全建立在 Alex（ducalex）的 retro-go 之上。**

retro-go 把十多个模拟器塞进 ESP32 这种资源极其有限的芯片，
并在 CPU、内存、flash 上做了深度优化同时保持兼容性 —— 这是一项了不起的工作。

本移植只解决了 retro-go 原生不支持的一类硬件（ESP32-P4 横屏设备）与
若干外围设备问题。所有核心功能均来自上游。

**感谢 Alex 与 retro-go 的所有贡献者，以及 retro-go 所整合的每一个开源项目的作者。**