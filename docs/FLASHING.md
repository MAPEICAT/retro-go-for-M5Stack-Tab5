# 烧录指南（M5Stack Tab5 / ESP32-P4）

[English](FLASHING.en.md) | 简体中文

本文档适用于本仓库提供的 `retro-go_v0.4.7-tab5.img`。

---

## 1. 前置条件

| 项目 | 要求 |
|---|---|
| 设备 | M5Stack Tab5（ESP32-P4，rev v1.3，16MB Flash） |
| 主机 | Windows / macOS / Linux，Python 3.8+ |
| 工具 | [ESP-IDF 5.5.2](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32p4/index.html)（仅重新编译时需要；只刷固件不需要） |
| 线材 | **USB-C 数据线**（注意：必须是数据线，不能是只充电的） |

---

## 2. 烧录步骤

### 2.1 确认设备与端口

把 Tab5 用 USB-C 线连到电脑。Windows 上设备会枚举为串口（示例为 `COM16`），
可用「设备管理器 → 端口 (COM 和 LPT)」查看，或：

```powershell
Get-PnpDevice -Class Ports | Select-Object FriendlyName, InstanceId, Status
```

Linux 下为 `/dev/ttyACM0`，macOS 下为 `/dev/cu.usbmodem*`。

> **注意**：Tab5 用的是 ESP32-P4 原生 USB-Serial/JTAG，`esptool` 结尾的
> "Hard resetting via RTS pin" 对它**无效** —— 它没有把 DTR/RTS 接到 EN/IO0。
> 烧录完需要**手动按复位键**或断电重开才能启动新固件。

### 2.2 用 esptool 烧录

```bash
# 找到你的 esptool（三种常见位置，任选其一）
python -m esptool                 # 若已 pip install esptool
~/.espressif/python_env/*/bin/python -m esptool   # ESP-IDF 自带
python -m esp_idf_nvs_tool        # 不适用，用下面这个

# 烧录（把 PORT 换成你的实际端口）
python -m esptool --chip esp32p4 --port COM16 --baud 921600 \
    write_flash --flash_size detect 0x0 retro-go_v0.4.7-tab5.img
```

Linux/macOS：

```bash
python3 -m esptool --chip esp32p4 --port /dev/ttyACM0 --baud 921600 \
    write_flash --flash_size detect 0x0 retro-go_v0.4.7-tab5.img
```

**成功标志**：

```
Wrote 2424832 bytes ...
Hash of data verified.
Hard resetting via RTS pin...
```

看到 `Hash of data verified.` 即写入完成（最后那行 reset 对本设备无效，见上）。

### 2.3 校验（可选但推荐）

```bash
sha256sum retro-go_v0.4.7-tab5.img
# Windows PowerShell:
Get-FileHash retro-go_v0.4.7-tab5.img -Algorithm SHA256
```

期望值见 `firmware/SHA256SUMS.txt`。

### 2.4 启动

按一下 Tab5 的复位键（或断电重开）。

首次启动日志里应能看到（115200 波特率）：

```
launcher v0.4.7-landscape-dirty (Oct 10 2026 23:37:34)
[info] lcd_init: Tab5 DSI ready (ST7123 path): logical 1280x720 -> physical 720x1280, orient=CCW90
[info] USBHID: ready (host + HID class driver up), plug a pad into the USB-A port
```

---

## 3. 关于手柄（重要）

### 3.1 只能有线

**M5Stack Tab5 板载的 ESP32-C6 仅支持 BLE（低功耗蓝牙），不支持经典蓝牙（BR/EDR）**
（已向 M5Stack 官方确认）。

由于 Xbox / Nintendo Switch 系手柄的**无线**链路走的是经典蓝牙，
因此这类手柄**无法通过蓝牙连接 Tab5**。USB-A 口是目前唯一可行的输入方式。

### 3.2 手柄的两种模式

良值 L1031（IINE GAMEPAD）等手柄在 USB 下会枚举为
**Nintendo Switch Pro Controller**（VID `057e`，PID `2009`）。
本固件已完整实现该协议的握手与报文解析：

```
Nintendo controller detected (in=0x81 mps=64 out=0x01)
claimed iface=0 alt=0
SW -> 80/04 disable-usb-timeout
SW -> 01/03/30 full-report-mode
SW -> 01/10/02 set-playing
Nintendo handshake DONE (3 frames)
*** first 0x30 input report — controller IS streaming ***
```

**判据**：只要日志出现 `first 0x30 input report`，手柄就正常工作了。

### 3.3 按键映射

| 手柄按键 | retro-go / GBA | 说明 |
|---|---|---|
| A | A | |
| B | B | |
| X | X | **Turbo 连发 A**（GBA 原生无 X/Y 键，核心借这两位做连发） |
| Y | Y | **Turbo 连发 B** |
| L | L | |
| R | R | |
| ZL | L | GBA 无独立扳机位，与 L 合并 |
| ZR | R | 同上 |
| Minus | SELECT | |
| Plus | START | |
| Home | MENU | 呼出 retro-go 菜单 |
| L3 | MENU | |
| R3 | OPTION | |
| 左摇杆 / 十字键 | 方向 | 两者并存，任选 |

### 3.4 若按键无反应

1. **先确认 USB-A 口**：Tab5 上有两个 USB-C（一个充电/调试、一个 USB-A Host）。
   手柄要插在**USB-A**（长方形大口）。
2. **确认手柄已开机**：某些手柄需要先长按电源键开机再插线。
3. **看串口日志**：正常应出现 `first 0x30 input report`。若只有
   `device connected` 而没有这行，说明握手未完成。

---

## 4. 屏幕设置

进入 **Options → Screen flip** 可切换横屏朝向（`CW90` / `CCW90`），
选择**你实际看着正的那个**，会存进 NVS，断电不丢。

> 本仓库提供的固件默认是 **CCW90**（在"键盘在下方"的正常持握方向下画面为正）。

其它相关设置（同在 Options 菜单）：

| 选项 | 说明 |
|---|---|
| `Screen flip` | 横屏朝向 CW90 / CCW90 |
| `Show FPS` | 右上角实时帧率开关（默认开，存 NVS） |
| `Touch opacity` | 虚拟按键透明度 |
| `Touch skin` | 虚拟按键皮肤 |

### 虚拟按键操作

| 按键 | 功能 |
|---|---|
| 左上角图标 | 显示 / 隐藏全部虚拟按键 |
| 十字键下方 `HIDE` | 同上（隐藏态下入口是左上角那颗） |
| L/R 之间那颗 | X/Y ↔ L/R **对调**（屏幕虚拟键与 USB 手柄**同步**生效） |

---

## 5. 已知限制

| 现象 | 原因 |
|---|---|
| **部分汉化/改版 GBA 游戏启动即崩** | 该 ROM 触发动态重编译器（JIT）未实现的指令或特殊 mapper。**换非改版 ROM 即可正常游玩**。这是上游 gpsp 核心的已知弱项，与本移植无关。 |
| 重负载场景偶发掉帧 | GBA 核心 dynarec 本身占用约 90% CPU，横屏转置另需 25~30%。静止画面满帧。 |
| 蓝牙手柄不可用 | Tab5 硬件仅支持 BLE，见 §3.1。 |

### 关于 CPU 主频

**不要把主频改成 400MHz**。在 esp32p4-eco2 上会在二级 bootloader 阶段直接
`assert failed: 0x40000954` 进入重启循环（连 `cpu_start` 都到不了）。
本仓库固定使用 360MHz。

---

## 6. 从源码重新编译

完整说明见 [`BUILDING.md`](docs/BUILDING.md)。

简版：

```bash
git clone https://github.com/ducalex/retro-go.git
cd retro-go

# 本移植的改动
git apply path/to/tab5-port-changes.patch
# 新增文件（USB HID 驱动、验证脚本）需从 source/ 目录复制到对应位置

cd retro-go-p4
python rg_tool.py --target tab5 build-img launcher gbsp --no-networking
```

产物：`retro-go-p4/retro-go_v0.4.7-landscape-dirty_tab5.img`

### Windows 编译注意

在 Git Bash / MSYS 环境下直接调用 ESP-IDF 会失败（`MSYSTEM` 环境变量会让
`idf_tools.py` fatal，且工具链不在 PATH）。`rg_tool.py` 已处理这三件事：

1. 自动切换到 ESP-IDF 的 Python venv
2. 清除 `MSYSTEM` / `MSYS` / `MINGW` 等变量
3. 注入 `ccache`、`riscv32-esp-elf-gcc` 所在目录到 PATH

---

## 7. 获取帮助

遇到问题请附上串口日志（115200 波特率）。关键信息都在里面：

```bash
python serial_tail.py COM16 115200 debug.log
```

`serial_tail.py` 带**自动重连** —— 设备重启导致串口消失时会自动等待并重新连接，
不会静默丢失日志（崩溃输出往往就在重启那一瞬间）。