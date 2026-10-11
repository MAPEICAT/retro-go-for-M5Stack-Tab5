# 补丁基线说明 / Patch baseline

[English](BASE.en.md) | 简体中文

---

## ⚠ 先看这里：不要从 `ducalex/retro-go` 打这个补丁

本目录的 `tab5-port-changes.patch` **以 `retro-go-tab5` 为基线**，
不是 `ducalex/retro-go`。克隆后者再 `git apply` **必定失败**。

```bash
# ✅ 正确
git clone https://github.com/andjiang0083/retro-go-tab5.git
cd retro-go-tab5
git apply /path/to/tab5-port-changes.patch

# ❌ 错误：路径对不上，打不上
git clone https://github.com/ducalex/retro-go.git
```

---

## 1. 基线提交

| 项 | 值 |
|---|---|
| 仓库 | [`andjiang0083/retro-go-tab5`](https://github.com/andjiang0083/retro-go-tab5) |
| 分支 | `main` |
| 基线提交 | **`bee1ef0`** — `license: 补 SPDX 文件头与第三方通知` |
| 许可 | GPL-2.0 |

如果 `git apply` 报 hunk 失败，先核对基线：

```bash
git log -1              # 应停在 bee1ef0
git apply --check /path/to/tab5-port-changes.patch
```

---

## 2. 为什么不能用ducalex/retro-go

补丁里全部13 个文件的路径都以 **`retro-go-p4/`** 开头，例如：

```
retro-go-p4/components/retro-go/rg_display.c
retro-go-p4/components/retro-go/drivers/display/mipi_dsi_tab5.h
retro-go-p4/gbsp/components/gbsp-libretro/cpu_threaded.c
retro-go-p4/rg_tool.py
vendor/m5stack_tab5/m5stack_tab5.c
```

而 **`ducalex/retro-go` 上游没有 `retro-go-p4/` 这一层**（它的 P4 移植组织方式不同）。
所以补丁找不到对应文件。

### 实测数据

按基线上下文逐行核对，补丁 13 个文件的 **691 行**基线上下文里：

| 指标 | 数值 |
|---|---|
| 能在 `retro-go-tab5` 中逐字找到 | **678 行（98.1%）** |
| 100% 命中的文件 | 10 个 |

10 个 100% 命中的文件包括：

| 文件 | 命中 |
|---|---|
| `rg_touch_overlay.c` | 265 / 265 |
| `vendor/m5stack_tab5.c` | 53 / 53 |
| `rg_input.c` | 47 / 47 |
| `rg_gui.c` | 40 / 40 |

这直接证明：**补丁的基线是 `retro-go-tab5`，而不是 du-calex 上游。**

（这组数据由 andjiang0083 在本 issue 中实测提供。）

---

## 3. 补丁里出现的 retro-go-tab5 独有文件

以下文件**只存在于 `retro-go-tab5`**，`ducalex/retro-go` 中没有 ——
这也是"基线搞错了就会打不上"的直接原因：

- `retro-go-p4/components/retro-go/targets/tab5/touch_layout.h`
- `retro-go-p4/components/retro-go/rg_touch_overlay.c` / `.h`
- `retro-go-p4/rg_tool.py`
- `retro-go-p4/components/retro-go/drivers/display/mipi_dsi_tab5.h`

---

## 4. 这个补丁包含什么

13 个文件，全部是**本次新增**的改动（相对 `retro-go-tab5` 基线）：

| 文件 | 本次改动 |
|---|---|
| `components/retro-go/CMakeLists.txt` | 加 `usb` / `usb_host_hid` 依赖 + 头文件路径 |
| `drivers/display/mipi_dsi_tab5.h` | 90° 转置写入、双朝向、FPS 合成区；移除 E3 预读拷贝 |
| `rg_display.c` / `.h` | `rg_display_get_flip180()` —— 朝向唯一真源（NVS 持久化） |
| `rg_gui.c` | 方向切换菜单项 |
| `rg_input.c` | 接线 USB HID 输入路径；HIDE 命中判定 |
| `rg_touch_overlay.c` / `.h` | HIDE 常驻按钮；X/Y↔L/R 交换同步到 USB；FPS 绘制 |
| `targets/tab5/config.h` | 修正被误设为恒等映射的触摸映射 |
| `targets/tab5/touch_layout.h` | HIDE 按钮几何 |
| `gbsp/.../cpu_threaded.c` | 移除临时诊断探针 |
| `rg_tool.py` | Windows / Git Bash 构建环境修复 |
| `vendor/m5stack_tab5/m5stack_tab5.c` | 不再创建 I2S RX 麦克风通道（消除杂音） |

### 真正的新增文件（不在补丁里，需手动复制）

补丁只改已有文件。以下是全新文件：

| 源文件（本仓库） | 目标位置 |
|---|---|
| `source/drivers/usbhid/rg_usbhid.c` | `retro-go-p4/components/retro-go/drivers/usbhid/` |
| `source/drivers/usbhid/rg_usbhid.h` | 同上 |
| `source/verify_fps.py` | 仓库根（验证工具，可选） |
| `source/test_orientation.py` | 仓库根（验证工具，可选） |
| `source/serial_tail.py` | 仓库根（日志工具，可选） |

```bash
mkdir -p retro-go-p4/components/retro-go/drivers/usbhid
cp source/drivers/usbhid/rg_usbhid.{c,h} retro-go-p4/components/retro-go/drivers/usbhid/
cp source/*.py ./
```

---

## 5. 与上游 v0.4.9 的关系

`retro-go-tab5` 已于 2026-10-10 发布 **v0.4.9**，其中包含
「一份镜像支持横竖屏切换（方向运行时化）」。

**方向运行时化与本移植的横屏部分功能重叠。** 坦率说明各自的增量：

| 功能 | 归属 |
|---|---|
| 方向运行时切换 | **上游 v0.4.9 已有** |
| **USB 手柄驱动**（`drivers/usbhid/`，Nintendo Switch Pro 协议） | **本移植独有** —— 上游 `drivers/` 下只有 `audio` 和 `display` |
| **I2S RX 消噪**（消除 ES8388 底噪） | **本移植独有** |
| **E3 预读拷贝移除**（释放 48KB 内部 SRAM） | **本移植独有** |
| 触摸映射修正 | 本移植 |

本仓库是**平行分支**，不是 retro-go-tab5 的下游继任。两者可以各自演进。

---

## 6. 镜像版本横幅

仓库里的 `firmware/retro-go_v0.4.7-tab5.img` 编译自本仓库源码，
横幅为 `RG_TARGET_TAB5-0 v0.4.7-landscape-dirty SDK:v5.5.2-dirty esp32p4`。
其中 `landscape-dirty` 是本移植的构建标记。

日志字符串（如 `Tab5 DSI ready (ST7123 path)`、`Tab5 audio: ES8388 ready at %d Hz`）
与 `retro-go-tab5` 对应源码逐字一致 —— 因为基线就是它。
机种表（Atari Lynx / ColecoVision / PC Engine / Game & Watch 等 11 种）同样来自基线。