# 编译指南（M5Stack Tab5 / ESP32-P4）

本文档说明如何从源码构建本仓库提供的固件。

---

## 1. 环境准备

### 1.1 必需组件

| 组件 | 版本 | 说明 |
|---|---|---|
| [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32p4/index.html) | **5.5.2** | 用其它版本可能因 sdkconfig / API 差异编译失败 |
| Python | 3.9 – 3.13 | 需与 ESP-IDF 的 venv 兼容 |
| Git | 任意 | |

ESP-IDF 安装（Windows）：

```powershell
git clone -b v5.5.2 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
install.bat esp32p4
```

安装后需在**每个新开的终端**里执行一次 `export.ps1`（Windows）或 `export.sh`
（Unix）把环境变量加进来。

验证：

```bash
idf.py --version     # 应输出 v5.5.2
```

### 1.2 M5Stack BSP

本移植依赖 `vendor/m5stack_tab5`，它已经包含在源码里，**不需要额外安装**。
它负责 ST7123 屏幕、ES8388 音频、I2C 扩展器（PI4IOE）、USB Host 等外设初始化。

---

## 2. 获取源码并应用本移植的改动

```bash
git clone https://github.com/ducalex/retro-go.git
cd retro-go
```

### 2.1 应用补丁

```bash
git apply patches/tab5-port-changes.patch
```

### 2.2 复制新增文件

补丁只包含对已有文件的修改。以下是**全新增加**的文件，需要手动复制：

| 源文件（仓库内） | 目标位置 |
|---|---|
| `source/drivers/usbhid/rg_usbhid.c` | `retro-go-p4/components/retro-go/drivers/usbhid/` |
| `source/drivers/usbhid/rg_usbhid.h` | `retro-go-p4/components/retro-go/drivers/usbhid/` |
| `source/verify_fps.py` | `retro-go-tab5-main/`（验证工具，可选） |
| `source/test_orientation.py` | `retro-go-tab5-main/`（映射验证工具，可选） |
| `source/serial_tail.py` | `retro-go-tab5-main/`（日志抓取工具，可选） |

```bash
mkdir -p retro-go-p4/components/retro-go/drivers/usbhid
cp source/drivers/usbhid/rg_usbhid.{c,h} retro-go-p4/components/retro-go/drivers/usbhid/
cp source/*.py ./
```

> ⚠ **不要复制 `vendor/` 目录**。它体积大且与 ESP-IDF 版本强相关，
> 建议直接用你本地已经能编译通过的那份。

---

## 3. 编译

```bash
cd retro-go-p4
python rg_tool.py --target tab5 build-img launcher gbsp --no-networking
```

参数含义：

| 参数 | 作用 |
|---|---|
| `--target tab5` | 目标设备（M5Stack Tab5） |
| `build-img` | 构建 launcher 与 gbsp 两个 app 并合并成单一镜像 |
| `launcher gbsp` | 只构建这两个 app（Retro-Go 的基本组成：启动器 + 游戏机） |
| `--no-networking` | 不构建 WiFi / 文件管理器功能（省 flash） |

**预期耗时**：首次全量编译约 6~8 分钟（Windows + 虚拟化环境可能更久）。
增量编译约 1~5 分钟。

### 3.1 产物

```
retro-go-p4/retro-go_v0.4.7-landscape-dirty_tab5.img
```

这是完整的合并镜像，**直接烧录到 `0x0`** 即可（见 [`FLASHING.md`](FLASHING.md)）。

### 3.2 单独构建某个 app（调试用）

```bash
python rg_tool.py --target tab5 launcher
python rg_tool.py --target tab5 gbsp
```

单独构建的 app 需要配合分区表烧录（launcher 在 `0x10000`，gbsp 在 `0x100000`），
日常使用不必如此。

---

## 4. Windows / Git Bash 注意事项

在 Git Bash（MSYS2）环境下直接调 ESP-IDF 会遇到三个问题，
`rg_tool.py` 已全部处理：

| 问题 | 现象 | `rg_tool.py` 的处理 |
|---|---|---|
| `MSYSTEM=MINGW64` 污染环境 | `idf_tools.py` fatal，`idf.py` 在 `check_environment()` 静默退出 | 构建前清除 `MSYSTEM` / `MSYS` / `MINGW` 等变量 |
| 用错 Python | 报缺 `esp_idf_monitor` | 自动切到 ESP-IDF 的 venv Python |
| 工具链不在 PATH | ninja spawn `ccache` / `riscv32-esp-elf-gcc` 报 `CreateProcess failed` | 注入 `IDF_TOOLS_PATH/tools` 下含 `.exe` 的目录 |

**另外：不要删 `build/` 目录来"彻底重编"** —— 链接阶段在 Windows 上偶尔会卡死
（进程存活但几分钟无输出）。正确做法是直接重跑构建命令，ninja 会增量处理；
真卡死了用 `taskkill /F /IM ninja.exe` 杀掉再重跑。

---

## 5. 关键编译配置

以下是本移植相对上游的**关键配置改动**，改之前请先读 [`NOTES.md`](NOTES.md)
里对应条目 —— 多数都是踩过坑之后才定的，不要盲目"优化"。

| 配置 | 值 | 文件 | 说明 |
|---|---|---|---|
| 编译优化 | `-O2` | `launcher/sdkconfig`、`gbsp/sdkconfig` | 上游默认 `-Os`；`-O2` 对模拟器这类计算密集负载更合适 |
| CPU 主频 | **360MHz** | 同上 | ⚠ **切勿改成 400MHz** —— esp32p4-eco2 上会 bootloader 阶段 assert 死循环 |
| 编译目标 | `esp32p4` | `rg_tool.py` | |
| Networking | 关闭 | `--no-networking` | 省 flash |

### 5.1 ⚠ 每个 app 有独立的 sdkconfig

`retro-go-p4/launcher/` 和 `retro-go-p4/gbsp/` **各自有一份 sdkconfig**。
改配置时必须两个都改 —— 只改 launcher 的话，游戏（跑在 gbsp 里）完全不受影响。

这是本项目反复踩过的坑：改完编译、烧录、日志看着一切正常，但性能毫无变化。

---

## 6. 验证工具

### `verify_fps.py` —— 帧率数字的坐标验证

```bash
python verify_fps.py
```

逐像素模拟 FPS 数字的"逻辑坐标 → 物理坐标"变换，验证：

1. **双射性** —— 面积守恒 ⇒ 不丢点、不重复
2. 绘制像素 ⊆ 擦除矩形（保证"擦除范围"与"绘制范围"永远对齐）
3. 不越界
4. 三种朝向（linear / CW90 / CCW90）的映射对照
5. 数字不压游戏画面
6. 数字不压 swap 按钮 / R 肩键

改动 FPS 位置后**必须重跑** —— 这类坐标错误肉眼验不出来。

### `test_orientation.py` —— 显示映射验证

```bash
python test_orientation.py
```

验证 CW90 / CCW90 两条分块转置路径等价于直接公式，且 `CCW90 == rot180(CW90)`，
无丢点 / 重复 / 越界。改显示驱动后重跑。

### `serial_tail.py` —— 日志抓取

```bash
python serial_tail.py COM16 115200 debug.log
```

带自动重连。**排查崩溃必用**：Tab5 每次 panic 重启都会让串口消失再出现，
不带重连的脚本会静默丢掉最关键的崩溃输出。

---

## 7. 常见构建问题

| 报错 | 原因与解法 |
|---|---|
| `'xxx' undeclared` | 静态变量声明在使用点之后。C 里必须前置声明。 |
| `usb/usb_host.h: No such file` | `usb` / `usb_host_hid` 的头文件路径**不通过 REQUIRES 传递**，需在 `components/retro-go/CMakeLists.txt` 里显式 `target_include_directories` |
| ninja 卡死无输出 | `taskkill /F /IM ninja.exe` 后重跑 |
| 编译"假成功"（几秒就完、产物没变） | 检查 `MSYSTEM` 环境变量是否被清除；确认 `.obj` 时间戳确实更新 |

---

## 8. 上游参考

本移植基于 [ducalex/retro-go](https://github.com/ducalex/retro-go)，
详细改动说明见 [`NOTES.md`](NOTES.md)。