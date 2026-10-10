# M5Stack Tab5 移植：改动说明与踩坑记录

[English](NOTES.en.md) | 简体中文

本文档记录把 [retro-go](https://github.com/ducalex/retro-go) 移植到
**M5Stack Tab5**（ESP32-P4）时做的改动，以及每个决定背后的原因。

**很多注释里写"这里以前踩过坑"的条目，都能在本文找到详细描述** ——
写下来是为了让后来者（包括我自己）不必重新趟一遍。

---

## 目录

- [移植概览](#移植概览)
- [硬件约束（先读这一节）](#硬件约束先读这一节)
- [改动清单](#改动清单)
- [踩坑记录](#踩坑记录)
  - [1. 横屏：为什么不能用 PPA 硬件旋转](#1-横屏为什么不能用-ppa-硬件旋转)
  - [2. 横屏方向：为什么做成"运行时可切换"](#2-横屏方向为什么做成运行时可切换)
  - [3. 触摸映射：被 `#ifndef` 静默屏蔽了](#3-触摸映射被-ifndef-静默屏蔽了)
  - [4. USB 手柄：三个真正的坑](#4-usb-手柄三个真正的坑)
  - [5. ESP-IDF 的 USB Host 组件有两个隐形陷阱](#5-esp-idf-的-usb-host-组件有两个隐形陷阱)
  - [6. GBA 崩溃：不是系统的锅](#6-gba-崩溃不是系统的锅)
- [遗留事项](#遗留事项)

---

## 移植概览

| 项目 | 值 |
|---|---|
| 目标设备 | M5Stack Tab5 |
| SoC | ESP32-P4（rev v1.3，esp32p4-eco2） |
| 屏幕 | ST7123（720×1280 竖屏面板） |
| 音频 | ES8388 codec + I2S |
| 主机接口 | USB-A（USB Host） |
| 无线 | 板载 ESP32-C6，**仅 BLE** |
| 固件大小 | 2,424,832 B |
| ESP-IDF | 5.5.2 |
| 主频 | 360 MHz（**不能用 400MHz**） |
| 编译优化 | `-O2` |

主要功能：横屏 + 触摸虚拟手柄 + 有线 USB 手柄 + HIDE 按键开关 + 实时帧率显示。

---

## 硬件约束（先读这一节）

这三条是**物理限制，不是配置问题**，决定了整个移植方案的形状。

### 1. 屏幕面板原生竖屏，横屏必须靠软件转置

Tab5 的 ST7123 面板原生是 720×1280 竖屏，且 MADCTL 寄存器读出来是 `0x00`
（无硬件旋转）。所以横屏 1280×720 只能由驱动做 90° 转置后写入。

### 2. Tab5 只支持 BLE，不支持经典蓝牙

**已向 M5Stack 官方确认。** 板载 ESP32-C6 是 BLE-only 芯片。

后果：Xbox / Switch 系手柄的**无线**链路走经典蓝牙（BR/EDR），
在这台设备上**物理不可行**。所以 USB-A 有线是唯一路径。

（顺带一提：即使打通 esp_hosted + 手工配 SDIO 引脚，也只能得到 BLE，对这些手柄依然无效。）

### 3. USB Host 走 USB-A 口，`hid_host` 组件不能用于目标手柄

目标手柄（良值 L1031 / IINE GAMEPAD）在 USB 下枚举为
**Nintendo Switch Pro Controller**（VID `057e`，PID `2009`）。
`usb_host_hid` 组件只认 HID class 接口，且**不提供中断 OUT 端点 API**，
而 Switch Pro 协议必须走中断 OUT。最终只能自己基于 `usb_host` 库实现。

---

## 改动清单

13 个文件有改动 + 1 个新目录。完整 diff 见 `patches/tab5-port-changes.patch`。

### 新增文件

| 文件 | 作用 |
|---|---|
| `components/retro-go/drivers/usbhid/rg_usbhid.c/.h` | USB 手柄驱动（XInput + Nintendo 双协议） |
| `components/retro-go/CMakeLists.txt` | 加入 `usb` / `usb_host_hid` 依赖 + 头文件路径 |
| `targets/tab5/touch_layout.h` | 新增 HIDE 按钮几何 |
| `rg_tool.py` | Windows/Git Bash 构建环境修复 |

### 横屏显示

| 文件 | 改动 |
|---|---|
| `drivers/display/mipi_dsi_tab5.h` | 90° 转置写入、CCW90/CW90 双朝向、FPS 合成区 |
| `rg_display.c/.h` | `rg_display_get_flip180()` —— 横屏朝向的唯一真源（NVS 持久化） |
| `rg_gui.c` | `Screen flip` 菜单项 |

### 触摸虚拟手柄

| 文件 | 改动 |
|---|---|
| `targets/tab5/config.h` | 修正被误设为恒等映射的触摸映射；启用 USB HID 路径 |
| `rg_input.c` | 接线 USB HID 输入路径；HIDE 按钮命中判定 |
| `rg_touch_overlay.c/.h` | HIDE 常驻按钮；X/Y↔L/R 交换同步到 USB 路径；FPS 绘制 |
| `mipi_dsi_tab5.h` | 移除 48KB 无效的 SRAM→SRAM 预读拷贝 |

### 音频

| 文件 | 改动 |
|---|---|
| `vendor/m5stack_tab5/m5stack_tab5.c` | 不再创建 RX 麦克风通道（与 TX 抢 I2S 时钟域 → 杂音）；加深 TX DMA 缓冲 |

### GBA 核心

| 文件 | 改动 |
|---|---|
| `gbsp/components/gbsp-libretro/cpu_threaded.c` | 移除临时诊断探针 |
| `launcher/sdkconfig`、`gbsp/sdkconfig` | `-Os` → `-O2` |

---

## 踩坑记录

### 1. 横屏：为什么不能用 PPA 硬件旋转

**结论：在这块板子上 PPA 的落点天生比 CPU 差，不值得做。**

历史上试过 PPA 硬件旋转，实测每块 ~25ms，而 CPU 转置同样一块只要 ~1.2ms ——
慢 20 倍，这也是当年实测掉到 1~2fps 的原因。

根因是**结构性**的：PPA 的目标 `tab5_fb` 是 DPI 帧缓冲 1280×720×2 = **1.84MB**，
内部 SRAM 总共都没这么大，它只能落在 PSRAM；而面板正以约 88MB/s 持续扫描读这块 PSRAM。
PPA 一写进去就和外设抢带宽，加上 op 是阻塞的，CPU 还要干等。

CPU 路径反而占便宜：查 linker map 可知，中转缓冲 `tab5_scratch` 在**内部 SRAM**
（`0x4ff40000`），最后一脚才 DMA 到 PSRAM。

顺带修掉一个伪优化：曾有个"E3 预读"机制，理由是"源数据在 PSRAM，得先读进 SRAM"——
但 `tab5_line_buffer` **本来就在内部 SRAM**（`0x4ff54000`）。这是一次 SRAM→SRAM
的同容量搬运，**零收益、纯开销**。更糟的是它有阈值（块 ≤48KB 才触发），
导致同一套代码两种耗时、profile 数据忽快忽慢。已移除，顺带释放 48KB 内部 SRAM。

> **教训**：优化前先确认前提。"源在 PSRAM"这个前提是错的，而错误前提下的优化
> 会伪装成"有时快有时慢"，很难联想到是它。

---

### 2. 横屏方向：为什么做成"运行时可切换"

**结论：编译期定死方向是错的，正确朝向取决于面板模组在机壳里的实际安装方向 + 用户持握方向，代码推不出来。**

演进过程：

1. 最初按"顺时针 90°"写（`逻辑 (lx,ly) → 物理 (px,py) = (719-ly, lx)`）
2. 用户报"上下颠倒" → 改成逆时针 90° + X 翻（`= (ly, 1279-lx)`）
3. 用户仍报"颠倒"

关键推理：这两个映射**互为 180° 旋转**。同一种持握方向下必定一对一错 ——
如果两种都报颠倒，说明"编译期定死一个方向"这条路本身就不成立。

期间还走过弯路：用 D-pad 的箭头方向来判断屏幕朝向。**D-pad 是 180° 自对称的**
（上下左右箭头连同位置一起翻转后看起来一模一样），**完全提供不了方向信息**，
只有文字能判。

最终方案：`rg_display_get_flip180()` 作唯一真点（NVS 持久化），
显示映射 / 触摸逆映射 / 虚拟按键层三者全部从它派生 —— 保证永远自洽。

映射算法本身用 Python 逐像素仿真验证过（12/12 通过，含 32 的非整数倍边界），
确认 `CCW90 == rot180(CW90)`、无丢点/重复/越界。

> **教训**：设备没有可用重力计时，屏幕朝向**只能靠目视确认**。
> 与其猜，不如给用户一个开关。

---

### 3. 触摸映射：被 `#ifndef` 静默屏蔽了

`targets/tab5/config.h` 里把触摸映射写成了**恒等映射**，注释还标着"【竖屏分支】"：

```c
#define RG_TOUCH_LOGICAL_FROM_PHYS(px, py, lx, ly) \
    do { (lx) = (px); (ly) = (py); } while (0)
```

而 `rg_input.c` 里那套 90° 逆映射是被 `#ifndef RG_TOUCH_LOGICAL_FROM_PHYS` 包着的 ——
**config.h 先定义，rg_input.c 那份永远不会生效**。

结果：显示侧转 90°、触摸侧不转，两边差 90°。这也解释了"改了 rg_input.c 却毫无变化"。

> **教训（这类 bug 极难查）**：宏的 `defined` 只看"有没有定义"，不看值。
> 同类问题还有一次：诊断时用 `#if defined(RG_USB_HID_GAMEPAD)` 想关闭功能，
> 把值改成 `0` **不等于关闭**。两次都因为"改动没真正生效"而得出错误结论。
> **凡是"改了变量/宏来关闭某功能"的诊断，必须验证改动真的生效**
> （镜像大小变化 / 特征字符串计数 / 日志行），不能只看源码改对了。

---

### 4. USB 手柄：三个真正的坑

这一路从"完全没反应"到"可用"跨了 5 个版本，三个真正的坑如下。

#### 坑 A：`set_report` 走控制传输，Switch Pro 不理它

最初用 `hid_class_request_set_report()` 发 Nintendo 的 subcommand
（0x80/04 disable-timeout、0x01/03/30、0x01/10/02 等），全部返回
`Control transfer timeout`。

**决定性证据**：格式改对（v5，正确 64 字节帧）之后，日志与格式改错（v3，3 字节垃圾）
**完全一样** —— 0x81×2、0x21×0、0x30×0。格式从错改对必然改变行为，
一模一样说明**手柄压根没处理这些命令**。

原因：Switch Pro 通过**中断 OUT 端点**接收命令，而 `hid_class_request_set_report()`
走的是**控制传输**（USB 规范允许设备对控制传输的 SET_REPORT 只 ACK 不处理）。
佐证：Linux `hid-nintendo` 和 Chromium 都用 `hid_write()` = 中断 OUT，
没人用控制传输。

#### 坑 B：设备句柄泄漏制造"幽灵条目"

修好握手后设备仍连不上，日志反复报：

```
E (3695) USB HOST: usbh_devs_open error: ESP_ERR_INVALID_STATE
[warn] USBHID: XI open addr=1 failed
```

去读 ESP-IDF 源码才看清 `ESP_ERR_INVALID_STATE(0x103)` 的**确切**含义：

```c
// components/usb/usbh.c  usbh_devs_open()
if (dev_obj->dynamic.flags.is_gone ||           // 设备已经断开
        dev_obj->dynamic.flags.waiting_free) { // 设备正等待释放
    ret = ESP_ERR_INVALID_STATE;
}
```

**它跟"枚举没完成"无关**（我一开始判断错了）。而
`usb_host_device_addr_list_fill()` 只列出已配置完成的设备。两条合起来：

> 总线上那个 addr=1 的条目，曾经完整枚举成功过，之后被标记为"已拔出"，
> 但对象一直没人回收 → 成了幽灵条目。

真凶是**我自己的 bug**：`xi_probe()` 发现"不是 XInput"就直接 `return`，
**从不 `usb_host_device_close()`**。而 IDF 的回收条件是「`open_count == 0` 才允许 free」：

```c
dev_obj->dynamic.flags.is_gone = 1;
if (open_count == 0)  DEV_ACTION_FREE            // 立刻回收
else                  DEV_ACTION_PROP_GONE_EVT   // 等对方 close 才回收
```

句柄一泄漏 → `open_count` 永远不为 0 → 设备永不回收 → 之后所有 `open` 全部 0x103。

**所以用户每一次插手柄，它其实都枚举成功了** —— 是我的代码当场把它锁死，
然后我自己一直在敲那个已经死掉的条目。

> **教训**：凡是"探测 → 拿到对象 → 以为它在"的流程，都要问
> 「**这个对象代表的实体现在还活着吗**」。句柄存在 ≠ 实体存在。
> 这是本项目第三次犯同一族错误（见 §6 与下面的通用教训）。

#### 坑 C：IN 传输漏了 `num_bytes` —— 一个必填字段

这个最冤。症状是"手柄完全不响应"，看起来像硬件或协议问题，
我为此绕了 5 个版本。

修好握手、认领接口之后，中断 IN 仍然**一条报文都收不到**，手柄还在每 2.5 秒自我复位。

`usb_transfer_t` 结构体里有个必填字段 `num_bytes`（要读/写多少字节），
我漏了。补上之后立刻：

| 指标 | 之前 | 之后 |
|---|---|---|
| 中断 IN 报文 | **0 条** | 持续流入 |
| `0x30` 输入报告 | **0** | 流起来了 |
| 手柄自我复位 | **80 次** | **1 次** |
| UI 负载 | BUSY 100% / FPS 4 | BUSY 7% / FPS 103 |

> **教训**：排查"设备完全没反应"时，**先核对 API 调用是否完整合规**
> （claim？num_bytes？对齐？端点方向？），再去怀疑硬件和协议。
> 这类必填字段漏掉后的症状，伪装得和硬件故障一模一样。

#### 附：按键字节布局必须实测，不能照抄"标准布局"

报文开头两个字节**根本不是按键**：`p[0]` 是每帧递增的包计数器、
`p[1]` 是恒定 `0x91` 的状态位。我照抄的"标准 Pro 布局"（按键从 `p[0]` 起）
**整体错位了 2 字节**，于是把计数器当成按键、解出一堆恒定假按键。

**这种整体错位靠肉眼看 dump 根本发现不了，只有用"按键差异"才能把真实偏移钉出来。**

最终布局（由真机逐键按压实测确认，字节值与按键顺序 1:1 对应）：

| 按键 | 字节 |
|---|---|
| A / B / X / Y | `p2` = `08` / `04` / `02` / `01` |
| R / ZR | `p2` = `40` / `80` |
| L / ZL | `p4` = `40` / `80` |
| 十字 上/下/左/右 | `p4` = `02` / `01` / `08` / `04` |
| + / − / Home | `p3` = `82` / `81` / `90` |
| 左摇杆 静止/左/右 | LX = `2048` / `≈0` / `≈4092` |

> **教训**：协议文档和"通用布局"都不能替代实测。
> 采样逻辑也要注意：最初的版本按时间节流打印，**采样额度在空闲帧的计数器
> 变化上被烧光了**。改成"载荷变化才打印"才一次到位。

---

### 5. ESP-IDF 的 USB Host 组件有两个隐形陷阱

#### 陷阱 A：`hid_host` 会静默独占设备

`hid_host_install()` 之后，它会自动 open 找到的 HID 接口。
一个设备**只能被一个 client 打开** —— 于是同一设备上的其他访问路径全被堵死，
而 `hid_host` **不会告诉你**。

要同时访问非 HID 接口（如 XInput 的 vendor class 0xFF），只能把 `hid_host` 摘掉。

#### 陷阱 B：注册回调收不到"注册之前就在"的设备

`usb_host_client_register()` 之后，只有**新接入**的设备才会发 `NEW_DEV` 事件。
如果设备在注册之前就插着，事件永远不会来。

必须**主动枚举**：

```c
uint8_t addrs[8] = {0};
int n = 0;
usb_host_device_addr_list_fill(8, addrs, &n);
```

#### 附：头文件路径不通过 REQUIRES 传递

`usb`（IDF 自带）和 `usb_host_hid`（vendor）的头文件路径
**不会**通过 `COMPONENT_REQUIRES` 传给消费方。必须在
`components/retro-go/CMakeLists.txt` 里显式：

```cmake
target_include_directories(${COMPONENT_LIB} PRIVATE
    "$ENV{IDF_PATH}/components/usb/include"
    "${CMAKE_CURRENT_LIST_DIR}/../../../vendor/managed_components/espressif__usb_host_hid/include"
)
```

> 注意路径层级：`CMakeLists.txt` 在 `retro-go-p4/components/retro-go/` 下，
> 到 `retro-go-tab5-main/vendor/` 需要 `../../../`。

---

### 6. GBA 崩溃：不是系统的锅

**现象**：某些 GBA 游戏启动即崩（`Instruction access fault`，`MEPC = MTVAL = 0xfffffffe`），
每次启动必崩，1:1 对应。

排查过程（每一步都做了单变量验证）：

| 怀疑对象 | 验证方法 | 结果 |
|---|---|---|
| USB 手柄 | 拔手柄 | 仍崩 |
| USB 主机栈占内存 | 从镜像里**彻底剥离**（镜像 −64KB，特征串归零） | 仍崩 |
| 开机读档路径 | 加开关跳过 `.srm` + 存档位载入 | 仍崩 |
| JIT 基础设施 | 加探针实测 | `ret` 正常、`C2M/M2C` 都 `ESP_OK`、连续 5 个块正常 |

**换成非改版 ROM → 一次都没崩**（`FPS:60 (0+60+0)`，`skipped=0`，panic 计数 0）。

**结论**：汉化/改版 ROM 触发动态重编译器（JIT）未实现的指令或特殊 mapper，
这是上游 gpsp 核心的已知弱项，与本移植无关。

想彻底修需要给 dynarec 加"未实现指令 → 回退解释器执行该条"的兜底，
而不是让它跳到 `-2` 的野地址 —— 那是另一件工程。

> **教训**：下次遇到"某游戏必崩"，**第一件事是换一个 ROM 对照**，
> 而不是去改系统代码。清单式排查（每步单变量验证）能排除掉所有错误方向，
> 但前提是**先找到一个"能跑"的样本**作为基准。

---

### 通用教训汇总

这几条在本项目中重复出现多次，值得单独列出：

1. **改动后必须验证改动真的生效。** 镜像大小变化 / 特征字符串计数 / 日志行。
   两次"假成功"（`#if defined()` 掩盖了把值改 0、编译 6 秒但产物没变）
   都导致过错误结论。

2. **"注册回调等事件"的框架，要问"在我注册之前就存在的对象，会不会通知我"。**
   USB Host 的答案是不会，必须主动枚举。

3. **"探测—失败—放弃"要先区分失败是永久性还是暂时性的。**
   暂时性失败必须重试。

4. **句柄 / 条目存在 ≠ 实体存在。** 本项目第三次犯这一族错误。

5. **同一文件内"配置类静态变量"统一放前部。** 声明在使用点之后会编译失败。

6. **多分支代码的验证脚本必须让每个分支用它自己的输入边界**，
   否则"失败"来自验证器而非被测代码。

7. **坐标变换类改动必须离线逐像素验证**，肉眼验不出来 ——
   本项目已在"横屏/竖屏""FPS 位置"上栽了两次。

8. **靠"按键顺序"推断方向不可靠。** 摇杆 Y 是唯一无法自证的轴，
   必须让用户上手确认，别在注释里留"若反了改这里"就当完事。

---

## 遗留事项

| 事项 | 状态 |
|---|---|
| 汉化/改版 GBA ROM 崩溃 | 未修（需 dynarec 加解释器回退兜底） |
| 重负载场景掉帧 | 未修（GBA dynarec 占约 90% CPU，横屏转置另需 25~30%。静止画面满帧） |
| ZL/ZR → Turbo | 未实现（当前与 L/R 合并；GBA 无独立扳机位） |
| X/Y 用途 | 未定（当前是 Turbo 连发 A/B） |
| 蓝牙手柄 | **不可行**（硬件仅 BLE，见 §硬件约束） |

---

## 上游与致谢

本移植完全建立在 **[ducalex/retro-go](https://github.com/ducalex/retro-go)** 之上
（GPLv2）。没有 Alex（ducalex）以及 retro-go 的各位贡献者，就不会有这个固件。

上游还整合了以下开源项目（详见 retro-go 的 `README.md` Acknowledgements 一节）：

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

本移植另参考了：

- [Irak4t0n/HowBoyAdvance](https://github.com/Irak4t0n/HowBoyAdvance)（GPLv2）—
  其 P4 版本的 JIT 缓存 MMU 可执行映射方案，是本移植 `map_jit_block()` 的基础
- [espressif/usb_host_hid](https://github.com/espressif/usb_host_hid) — USB HID host 组件
- [M5Stack Tab5 BSP](https://github.com/m5stack/M5StackTab5) — 外设驱动

感谢所有为 retro-go 及这些开源项目付出的人。