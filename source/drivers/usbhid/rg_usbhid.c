/* USB HID 手柄驱动 —— 见 rg_usbhid.h 顶部的设计说明。
 *
 * 分层：
 *   bsp_usb_host_start()  启动 P4 的 USB host 控制器（BSP 里已实现，此前无人调用）
 *   hid_host_install()    Espressif 的 HID 类驱动（vendor 在本地），负责枚举/端点/轮询
 *   本文件                 把 HID 报文翻译成 rg_key_t 位掩码
 *
 * ⚠ 已知边界：hid_host 只认 HID 接口（class 0x03）。Xbox 手柄**有线**时常常以
 *   XInput（vendor-specific, class 0xFF）呈现，那种情况下本驱动收不到任何回调 ——
 *   日志会表现为"USB 已启动但没有任何 HID 设备接入"。这本身就是关键诊断信号：
 *   说明要另写 XInput 类驱动，而不是继续调 HID 解析。
 */

/* 先拉进 target 的 config.h —— 本功能由 RG_USB_HID_GAMEPAD 开关，未定义的 target
 * （以及非 Tab5 目标，它们没有 usb/usb_host_hid 这两个依赖）整份编译为空。 */
#include "rg_system.h"

#if RG_USB_HID_GAMEPAD

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#include "usb/usb_host.h"
#include "usb/usb_helpers.h"
#include "usb/hid_host.h"

#include "bsp/m5stack_tab5.h"

#include "rg_system.h"
#include "rg_input.h"
#include "rg_usbhid.h"

/* ── 状态（回调写、输入层读；单写单读 + 32 位对齐，够用不着锁）───────────── */
static volatile uint32_t s_state = 0;
static volatile bool s_connected = false;
static char s_dev_name[48] = {0};
static bool s_host_started = false;
static bool s_hid_installed = false;

/* 报文诊断：只 dump 前若干条，避免刷屏（也避免拖慢轮询） */
static int s_dump_left = 24;
static int s_report_len_seen = 0;
static bool s_seen_0x30 = false;   /* 是否已收到过 0x30 输入报告（判断握手成功的硬指标） */

const char *rg_usbhid_get_device_name(void)
{
    return s_dev_name;
}

uint32_t rg_usbhid_get_state(void)
{
    return s_state;
}

bool rg_usbhid_is_connected(void)
{
    return s_connected;
}

/* ── 报文 -> rg_key_t ────────────────────────────────────────────────────────
 * 这台手柄的形态由 VID/PID 决定（见 hid_driver_cb）。dump 里的真实形态是
 * **Nintendo Switch Pro Controller**（VID 057e PID 2009），其报文格式与 Xbox 完全不同：
 *
 *   ┌─ byte 0 ─────────┬─ byte 1 ─────────┬─ byte 2 ───┬─ byte 3-4 ─┬─ byte 5-6 ─┐
 *   │ Y B A X L R ZL ZR│ Minus Plus L3 R3│ hat | Home │  左摇杆 X  │  右摇杆 X  │
 *   │        +2 未用   │      +4 未用    │  4b | 4b  │     Y      │     Y      │
 *   │   (低->高位 0..7)│   (低->高位 0..7)│ (H<<4)|(L)│   little-endian, 居中≈0x800   │
 *   └──────────────────┴─────────────────┴────────────┴────────────┴────────────┘
 *
 * 键名按**物理位置**对应：Switch 的 Y 在上、B 在右、A 在下、X 在左 —— 字母顺序
 * 恰好和 retro-go 同字母键一致（retro-go 的 A 在右、B 在下，但**物理位置**
 * 仍然一致）。
 *
 * ⚠ Report ID 0x81 不是输入报告 —— 那是手柄对主机 subcommand 的 reply，忽略；
 *   只有 0x30 才是真实按键状态。dump 里的 `81 01 00 03 7a c6 1a e9 b6 98...`
 *   就是因为没切到 0x30 模式、永远只能收到 reply。切模式在 init subcommand 里做。 */

/* ── 0x30 报文体布局：**由真机按键实测确定**（2026-10-10，用户逐个按键采的样）──
 * 报文体（去掉 report ID 0x30 之后，即 p = d+1）：
 *
 *   p[0]      包计数器/定时器（每帧都变，约 +0x54/帧）—— 忽略
 *   p[1]      状态字节，恒定 0x91（已连接/电量）—— 忽略
 *   p[2]      右半按键  bit0 Y  bit1 X  bit2 B  bit3 A  bit6 R  bit7 ZR
 *   p[3]      左半按键  bit0 Minus  bit1 Plus  bit2 L3  bit3 R3  bit4 Home  bit5 Capture
 *             （静止时为 0x80 —— bit7 是常置位的无关位，判断前先掩掉）
 *   p[4]      十字键+肩键 bit0 Down bit1 Up bit2 Right bit3 Left bit6 L bit7 ZL
 *   p[5..7]   左摇杆：12 位打包  LX = p[5] | ((p[6] & 0x0F) << 8)
 *                              LY = (p[6] >> 4) | (p[7] << 4)      中心 = 0x800
 *   p[8..10]  右摇杆：同法                                 中心 = 0x800
 *
 * 实测依据（按键顺序 vs 观测到的字节，1:1 对上，无歧义）：
 *   A→p2=0x08  B→p2=0x04  X→p2=0x02  Y→p2=0x01  R→p2=0x40  ZR→p2=0x80
 *   L→p4=0x40  ZL→p4=0x80  十字上→p4=0x02 下→0x01 左→0x08 右→0x04
 *   +→p3=0x82  −→p3=0x81  Home→p3=0x90
 *   左摇杆左→LX≈0  右→LX≈4092  下→LY≈4090  上→LY≈2   静止→LX=2048 LY=2047
 *
 * LY 方向（2026-10-10 真机确认）：用户实测「摇杆推上 → 角色往下走」，
 * 也就是**摇杆 Y 轴与十字键相反**（十字键的上下经单独验证是正确的）。
 * 所以这里取 1：把摇杆 Y 的上下翻转，让两者一致。
 * 数据侧也对得上 —— 手柄报的 LY 越大越"下"，而我们要"大值=上"，故需翻转。 */
#define SW_LY_INVERT 1

#define USBHID_STICK_CENTER     0x800
#define USBHID_STICK_DEADZONE   500     /* 中心 2048，±500 ≈ ±24% 全行程，够硬不抖 */

#define SW_APPLY_STATE 1   /* 布局已实测确认，可以喂给输入层了 */

/* Switch Pro 标准 0x30 输入报告解码。p/n 是去掉 report ID 后的报文体。 */
static void usbhid_decode_switch_pro(const uint8_t *p, size_t n)
{
    uint32_t st = 0;
    if (n < 12) return;

    if (!s_seen_0x30)
    {
        s_seen_0x30 = true;
        RG_LOGI("USBHID: *** first 0x30 input report — controller IS streaming ***\n");
    }

    const uint8_t right = p[2];              /* Y X B A . . R ZR      */
    const uint8_t leftb = p[3] & 0x7F;       /* Minus Plus L3 R3 Home Capture（bit7 常置位，掩掉） */
    const uint8_t dpad  = p[4];              /* Down Up Right Left . . L ZL */

    const int lx = p[5] | ((p[6] & 0x0F) << 8);
    const int ly = (p[6] >> 4) | (p[7] << 4);
    (void)(p[8] | ((p[9] & 0x0F) << 8));     /* 右摇杆 X —— 暂不映射 */
    (void)((p[9] >> 4) | (p[10] << 4));      /* 右摇杆 Y —— 暂不映射 */

    /* 面键：按**物理位置**对应（这套位序与 Nintendo 标准一致，正好是
     * 右半 Y/X/B/A + R/ZR —— 与 retro-go 的 Y/X/B/A 位置完全对得上，不用翻转） */
    if (right & 0x01) st |= RG_KEY_Y;        /* 左 */
    if (right & 0x02) st |= RG_KEY_X;        /* 上 */
    if (right & 0x04) st |= RG_KEY_B;        /* 下 */
    if (right & 0x08) st |= RG_KEY_A;        /* 右 */

    /* 肩键与扳机都映射到 L/R —— 玩家按扳机的意图也是 L/R，不区分更省心 */
    if (right & 0x40) st |= RG_KEY_R;        /* R  */
    if (right & 0x80) st |= RG_KEY_R;        /* ZR */
    if (dpad  & 0x40) st |= RG_KEY_L;        /* L  */
    if (dpad  & 0x80) st |= RG_KEY_L;        /* ZL */

    if (leftb & 0x01) st |= RG_KEY_SELECT;   /* Minus → SELECT */
    if (leftb & 0x02) st |= RG_KEY_START;    /* Plus  → START  */
    if (leftb & 0x04) st |= RG_KEY_MENU;     /* L3    → MENU   */
    if (leftb & 0x08) st |= RG_KEY_OPTION;   /* R3    → OPTION */
    if (leftb & 0x10) st |= RG_KEY_MENU;     /* Home  → MENU（手柄上的 Home 是圆键） */

    /* 十字键 */
    if (dpad & 0x01) st |= RG_KEY_DOWN;
    if (dpad & 0x02) st |= RG_KEY_UP;
    if (dpad & 0x04) st |= RG_KEY_RIGHT;
    if (dpad & 0x08) st |= RG_KEY_LEFT;

    /* 左摇杆兜底出方向（十字键也出，两种手型都顺手） */
    if (lx < USBHID_STICK_CENTER - USBHID_STICK_DEADZONE) st |= RG_KEY_LEFT;
    if (lx > USBHID_STICK_CENTER + USBHID_STICK_DEADZONE) st |= RG_KEY_RIGHT;
#if SW_LY_INVERT
    if (ly < USBHID_STICK_CENTER - USBHID_STICK_DEADZONE) st |= RG_KEY_DOWN;
    if (ly > USBHID_STICK_CENTER + USBHID_STICK_DEADZONE) st |= RG_KEY_UP;
#else
    if (ly < USBHID_STICK_CENTER - USBHID_STICK_DEADZONE) st |= RG_KEY_UP;
    if (ly > USBHID_STICK_CENTER + USBHID_STICK_DEADZONE) st |= RG_KEY_DOWN;
#endif

#if SW_APPLY_STATE
    s_state = st;
#else
    (void)st;
#endif
}

/* 拿到一条原始报文：dump + 解析。Switch Pro 模式下只处理 report ID 0x30。 */
static void usbhid_handle_report(const uint8_t *data, size_t len)
{
    if (s_dump_left > 0)
    {
        --s_dump_left;
        char hex[3 * 24 + 1];
        size_t k = 0, lim = (len < 24) ? len : 24;
        for (size_t i = 0; i < lim; ++i)
            k += snprintf(hex + k, sizeof(hex) - k, "%02x ", data[i]);
        RG_LOGI("USBHID report[%d] len=%d id=0x%02x: %s\n",
                24 - s_dump_left, (int)len, len ? data[0] : 0, hex);
    }
    s_report_len_seen = (int)len;

    if (len < 2) return;
    const uint8_t report_id = data[0];
    const uint8_t *p = data + 1;
    const size_t   n = len - 1;

    if (report_id == 0x30)            /* Switch Pro 标准输入报告 —— 唯一含按键的报文 */
    {
        usbhid_decode_switch_pro(p, n);
    }
    else if (report_id == 0x81)
    {
        /* USB 初始化 reply。byte 1（=p[0]）是 subtype，对应我们发的 0x80 命令：
         *   0x01 = 报 MAC（含 device_type：0x03 = Pro Controller）
         *   0x02 = handshake echo
         *   0x03 = 波特率切换结果
         *   0x04 = 超时禁用结果
         * 用它判断握手走到哪一步 —— 这是 0x30 不来时最有用的诊断。 */
        static int n81 = 0;
        if (n81 < 12)
        {
            ++n81;
            RG_LOGI("USBHID: 0x81 init-reply subtype=0x%02x dev=0x%02x\n",
                    n >= 1 ? p[0] : 0xFF, n >= 3 ? p[2] : 0xFF);
        }
    }
    else if (report_id == 0x21)
    {
        /* subcommand reply：byte 13（含 packet ID 的布局）= ACK 字节，
         * 最高位 1 = ACK、0 = NACK；低 7 位是返回的数据类型。 */
        static int n21 = 0;
        if (n21 < 12)
        {
            ++n21;
            RG_LOGI("USBHID: 0x21 subcmd-reply ack=0x%02x\n", n >= 13 ? p[12] : 0xFF);
        }
    }
    /* 其余一律忽略 —— 不解析、不覆盖按键状态 */
}

/* ── HID 接口事件（每个接口一个回调）───────────────────────────────────── */
static void hid_interface_cb(hid_host_device_handle_t handle,
                             const hid_host_interface_event_t event,
                             void *arg)
{
    (void)arg;
    switch (event)
    {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
    {
        uint8_t data[64] = {0};
        size_t len = 0;
        if (hid_host_device_get_raw_input_report_data(handle, data, sizeof(data), &len) == ESP_OK && len)
            usbhid_handle_report(data, len);
        break;
    }
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        RG_LOGI("USBHID: device disconnected\n");
        s_state = 0;
        s_connected = false;
        s_dev_name[0] = 0;
        s_dump_left = 24;             /* 下次接入重新 dump */
        s_seen_0x30 = false;          /* 下次接入重新报告"是否进入 streaming" */
        hid_host_device_close(handle);
        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        /* 传输错误是常态（拔线瞬间、端点 stall），不刷日志，避免掩盖真问题 */
        s_state = 0;
        break;
    default:
        break;
    }
}

/* wchar_t（UTF-16LE）-> 窄字符。产品名基本是 ASCII，非 ASCII 一律退化成 '?'。 */
static void usbhid_wstr_to_ascii(const wchar_t *src, char *dst, size_t dst_size)
{
    size_t i = 0;
    for (; i + 1 < dst_size && src[i]; ++i)
        dst[i] = (src[i] < 0x80) ? (char)src[i] : '?';
    dst[i] = 0;
}

/* ── HID 驱动事件（设备级）────────────────────────────────────────────────── */

/* ── Switch Pro USB 初始化（v5：按 Chromium device/gamepad/nintendo_controller.cc 的格式）
 *
 * 前 5 版都栽在同一处：我把 subcommand 当成裸 payload 发（3 字节），而 Switch Pro 的
 * output report 是**固定 64 字节的帧**：
 *   [0]     report ID = 0x01
 *   [1]     序列号（低 4 位，每次递增）
 *   [2..9]  rumble 8 字节（neutral = 00 01 40 40 00 01 40 40）
 *   [10]    subcommand ID
 *   [11..]  subcommand 参数
 * 发 {0x01, 0x10, 0x02} 的结果是"序列号=0x01、rumble 从 0x10 起"，**subcommand 位置
 * 完全错位** —— 手柄收到的是垃圾，所以既不切模式也一直回 0x81 reply。
 * 走 SET_REPORT 控制传输时 report ID 由 wValue 携带，data 从 [1] 起算（63 字节）。
 *
 * 另一个致命遗漏：**output report 0x80 这条 USB 专用初始化通道**。Chromium 定义：
 *   kUsbReportIdOutput80 = 0x80，subtype：01=请求MAC 02=handshake 03=波特率
 *                                        04=禁用USB超时 05=恢复超时
 * 其中 04 的官方注释写着：
 *   "Forces the controller to only talk over USB HID without any timeouts.
 *    This is required for the Pro Controller to not time out and revert to Bluetooth."
 * ——没有它，手柄超时后**回退蓝牙**，永不发 0x30 输入报告。这就是 0x30 一直为 0 的根因。
 *
 * 初始化顺序照抄 Chromium：0x80 01 → 02 → 03 → 04，然后 0x01 subcmd 03 30（切
 * standard full mode），最后 0x01 subcmd 30 01（点亮 1 号 player LED，表示"已连上"）。 */

typedef struct {
    hid_host_device_handle_t handle;
} switch_pro_init_arg_t;

/* 发一条 output report 0x01（rumble + subcommand）。payload 63 字节，见上面布局。 */
static esp_err_t switch_pro_subcmd(hid_host_device_handle_t h, uint8_t subcmd,
                                   const uint8_t *args, size_t nargs)
{
    static uint8_t seq = 0;
    uint8_t buf[63] = {0};
    buf[0] = (uint8_t)(seq++ & 0x0F);                                  /* 序列号 */
    buf[1] = 0x00; buf[2] = 0x01; buf[3] = 0x40; buf[4] = 0x40;        /* 左 rumble neutral */
    buf[5] = 0x00; buf[6] = 0x01; buf[7] = 0x40; buf[8] = 0x40;        /* 右 rumble neutral */
    buf[9] = subcmd;
    if (args && nargs)
    {
        size_t n = (nargs > 53) ? 53 : nargs;
        memcpy(&buf[10], args, n);
    }
    return hid_class_request_set_report(h, /* report_type = OUTPUT */ 2, 0x01, buf, sizeof(buf));
}

/* 发一条 output report 0x80（USB 专用初始化）。subtype 放在 payload 首个字节。 */
static esp_err_t switch_pro_usb80(hid_host_device_handle_t h, uint8_t subtype)
{
    uint8_t buf[63] = {0};
    buf[0] = subtype;
    return hid_class_request_set_report(h, /* report_type = OUTPUT */ 2, 0x80, buf, sizeof(buf));
}

static void switch_pro_init_task(void *arg)
{
    switch_pro_init_arg_t *a = (switch_pro_init_arg_t *)arg;
    hid_host_device_handle_t h = a->handle;

    /* 等枚举收尾 —— device_start 刚返回时控制端点还在忙，立刻发会 ESP_ERR_TIMEOUT */
    vTaskDelay(pdMS_TO_TICKS(300));

    /* ── USB 专用初始化通道（0x80），顺序照抄 Chromium ────────────────── */
    esp_err_t r;
    r = switch_pro_usb80(h, 0x01);   /* 请求 MAC（手柄会回 0x81 报文） */
    RG_LOGI("USBHID: SP 80/01 request-mac  = %s\n", r == ESP_OK ? "OK" : "FAIL");
    vTaskDelay(pdMS_TO_TICKS(60));

    r = switch_pro_usb80(h, 0x02);   /* handshake */
    RG_LOGI("USBHID: SP 80/02 handshake   = %s\n", r == ESP_OK ? "OK" : "FAIL");
    vTaskDelay(pdMS_TO_TICKS(60));

    r = switch_pro_usb80(h, 0x03);   /* 切 3Mbit 波特率 */
    RG_LOGI("USBHID: SP 80/03 baudrate    = %s\n", r == ESP_OK ? "OK" : "FAIL");
    vTaskDelay(pdMS_TO_TICKS(60));

    /* ★ 关键一步：禁用 USB 超时。没有它手柄会回退蓝牙、不发 0x30 */
    r = switch_pro_usb80(h, 0x04);
    RG_LOGI("USBHID: SP 80/04 disable-timeout = %s  <-- critical\n", r == ESP_OK ? "OK" : "FAIL");
    vTaskDelay(pdMS_TO_TICKS(100));

    /* ── 切 standard full report mode（0x30 @60Hz）────────────────────── */
    const uint8_t mode_arg[] = {0x30};
    r = switch_pro_subcmd(h, 0x03, mode_arg, sizeof(mode_arg));
    RG_LOGI("USBHID: SP 01/03/30 set-full-mode = %s\n", r == ESP_OK ? "OK" : "FAIL");
    vTaskDelay(pdMS_TO_TICKS(60));

    /* 点亮 player 1 LED —— 纯视觉反馈，跟能不能用无关，但便于判断"握手到底走到哪" */
    const uint8_t led_arg[] = {0x01};
    r = switch_pro_subcmd(h, 0x30, led_arg, sizeof(led_arg));
    RG_LOGI("USBHID: SP 01/30/01 player-led  = %s\n", r == ESP_OK ? "OK" : "FAIL");

    RG_LOGI("USBHID: SP init done — expecting 0x30 input reports now\n");

    free(a);
    vTaskDelete(NULL);
}

static void hid_driver_cb(hid_host_device_handle_t hid_dev_handle,
                          const hid_host_driver_event_t event,
                          void *arg)
{
    (void)arg;
    if (event != HID_HOST_DRIVER_EVENT_CONNECTED)
        return;

    hid_host_dev_params_t dev_params = {0};
    hid_host_device_get_params(hid_dev_handle, &dev_params);

    /* 先拿设备信息：VID/PID 是判断"到底是哪只手柄"的第一手依据 */
    char prod[40] = {0}, manu[24] = {0};
    uint16_t vid = 0, pid = 0;
    hid_host_dev_info_t dev_info = {0};
    if (hid_host_get_device_info(hid_dev_handle, &dev_info) == ESP_OK)
    {
        vid = dev_info.VID;
        pid = dev_info.PID;
        usbhid_wstr_to_ascii(dev_info.iProduct, prod, sizeof(prod));
        usbhid_wstr_to_ascii(dev_info.iManufacturer, manu, sizeof(manu));
    }

    RG_LOGI("USBHID: device connected addr=%d iface=%d subclass=%d proto=%d VID=%04x PID=%04x \"%s\" (%s)\n",
            dev_params.addr, dev_params.iface_num, dev_params.sub_class, dev_params.proto,
            vid, pid, prod, manu);

    snprintf(s_dev_name, sizeof(s_dev_name), "%s", prod[0] ? prod : "USB HID gamepad");

    hid_host_device_config_t dev_cfg = {
        .callback = hid_interface_cb,
        .callback_arg = NULL,
    };
    esp_err_t err = hid_host_device_open(hid_dev_handle, &dev_cfg);
    if (err != ESP_OK)
    {
        RG_LOGW("USBHID: device_open failed (0x%x)\n", (unsigned)err);
        return;
    }
    err = hid_host_device_start(hid_dev_handle);
    if (err != ESP_OK)
    {
        RG_LOGW("USBHID: device_start failed (0x%x)\n", (unsigned)err);
        hid_host_device_close(hid_dev_handle);
        return;
    }

    /* Nintendo Switch Pro Controller：VID 057e，PID 2009 = wired Pro Controller；
     * 还有 200e (USB charging cable)，也走相同协议。
     * 启动后台 task 跑完整握手序列（不能在回调里同步发，详见 switch_pro_init_task）。 */
    if (vid == 0x057e && (pid == 0x2009 || pid == 0x200e))
    {
        switch_pro_init_arg_t *arg = malloc(sizeof(*arg));
        if (arg)
        {
            arg->handle = hid_dev_handle;
            if (xTaskCreate(switch_pro_init_task, "sp_init", 2048, arg, 4, NULL) != pdTRUE)
            {
                RG_LOGW("USBHID: xTaskCreate(sp_init) failed\n");
                free(arg);
            }
        }
    }
    else
    {
        RG_LOGW("USBHID: unknown gamepad VID/PID, default parsing may be wrong\n");
    }

    s_connected = true;
    RG_LOGI("USBHID: interface open, waiting for reports\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * XInput 通道（vendor-specific，class 0xFF）
 *
 * 背景：手柄在 PC 上显示为 "XInput Controller #1"，说明它的 USB 接口里有一个
 * XInput 接口。hid_host 只认 class 0x03 的 HID 接口 → 我们拿到的是 Nintendo
 * Pro Controller 兼容层；那条路要完整 Switch 握手，且命令必须走**中断 OUT**
 * 端点（usb_host_hid 组件不提供，只有控制传输），实测握手全部 ACK 但手柄毫无
 * 反应 —— 走不通。XInput 是固定 20 字节报文，实现简单得多，所以并行走这条。
 *
 * 本段同时承担诊断：遍历并打印**每个接口**的 class/subclass/protocol 与端点。
 * 即使最终没找到 XInput 接口，日志也能告诉我们这设备到底长什么样。
 * ═══════════════════════════════════════════════════════════════════════════ */

#define XI_REPORT_LEN  20
#define XI_STICK_DEAD  8000      /* int16 量程 ±32767 的 ~24% */
#define XI_BUF_SIZE    64

static usb_host_client_handle_t s_xi_client = NULL;
static usb_device_handle_t      s_xi_dev = NULL;
static bool                     s_xi_streaming = false;
static bool                     s_xi_probed = false;   /* 本次插入是否已 probe 过（拔插时清） */
static int                      s_xi_dump = 20;
static int                      s_xi_fail_streak = 0;  /* 连续多少次扫描打不开设备 */
static int                      s_xi_vbus_cycles = 0;  /* 已强制重启过几次 USB-A 口供电 */
static bool                     s_xi_ghost_logged = false;

/* ── Nintendo Pro 控制器（USB 直连）状态 ──────────────────────────────────────
 * 实测描述符：1 个 HID 接口（class 0x03, sub 0, proto 0），2 个端点 ——
 *   ep 0x81 中断 IN  64B     ep 0x01 中断 OUT 64B
 * 这是 Pro 控制器 USB 直连的标准形态。**那个中断 OUT 就是之前卡死我们的东西**：
 * Switch Pro 的握手/配置命令必须走中断 OUT，而 usb_host_hid 组件只提供控制传输
 * SET_REPORT（6 步全 ACK 但手柄零反应，就是这个原因）。现在我们在 usb_host 裸层
 * 自己提中断 OUT 传输，这条路才第一次真正打开。
 *
 * 不握手的后果实测：手柄**每秒自我复位一次**（观测到地址从 1 一路涨到 80），
 * 因为 Nintendo 的固件在没有主机 keep-alive 时会超时重启。 */
static uint8_t          s_sw_in_ep = 0;
static uint8_t          s_sw_out_ep = 0;
static usb_transfer_t  *s_sw_out_x = NULL;
static usb_transfer_t  *s_sw_in_x = NULL;
static uint8_t          s_sw_seq = 0;      /* 输出帧计数器（低 4 位） */
static int              s_sw_step = 0;     /* 0/1/2 = 三帧握手；3 = 完成 */
static int              s_sw_in_dump = 0; /* 输入报文 dump 条数 */
static uint8_t          s_sw_last[64];     /* 上一条已 dump 的报文，用于算差异 */
static int              s_sw_last_len = 0;
static int64_t          s_sw_last_dump_us = 0;
static bool             s_sw_streaming = false;  /* 已收到 0x30（手柄稳定推数据了） */
static int              s_sw_in_status_log = 8;  /* IN 传输异常状态打印条数 */

/* 我们认领了哪个接口（用完要 release，否则该接口的端点句柄不归还）。 */
static uint8_t          s_xi_claimed_intf = 0;
static bool             s_xi_has_claim = false;

/* XInput 20 字节报文布局：
 *   [0]=0x00  [1]=0x14（长度）
 *   [2] 按键低位  bit0 Up  1 Down  2 Left  3 Right  4 Start  5 Back  6 L3  7 R3
 *   [3] 按键高位  bit0 LB  1 RB    2 Guide 3 A      4 B      5 X     6 Y
 *   [4] LT   [5] RT                        （0..255）
 *   [6..7] 左摇杆 X   [8..9] 左摇杆 Y      （int16 小端）
 *   [10..11] 右摇杆 X [12..13] 右摇杆 Y
 */
static void xi_parse(const uint8_t *d, size_t n)
{
    if (n < 14) return;

    if (!s_xi_streaming)
    {
        s_xi_streaming = true;
        RG_LOGI("USBHID: *** first XInput report — gamepad IS streaming ***\n");
    }

    if (s_xi_dump > 0)
    {
        --s_xi_dump;
        char hex[3 * XI_REPORT_LEN + 1];
        size_t k = 0, lim = (n < XI_REPORT_LEN) ? n : XI_REPORT_LEN;
        for (size_t i = 0; i < lim; ++i)
            k += snprintf(hex + k, sizeof(hex) - k, "%02x ", d[i]);
        RG_LOGI("USBHID: XI[%d] len=%d: %s\n", 20 - s_xi_dump, (int)n, hex);
    }

    uint32_t st = 0;
    const uint8_t lo = d[2], hi = d[3];

    if (lo & 0x01) st |= RG_KEY_UP;
    if (lo & 0x02) st |= RG_KEY_DOWN;
    if (lo & 0x04) st |= RG_KEY_LEFT;
    if (lo & 0x08) st |= RG_KEY_RIGHT;
    if (lo & 0x10) st |= RG_KEY_START;
    if (lo & 0x20) st |= RG_KEY_SELECT;

    if (hi & 0x01) st |= RG_KEY_L;      /* LB */
    if (hi & 0x02) st |= RG_KEY_R;      /* RB */
    /* 面键按**物理位置**对应：XInput 的 A 在下、B 在右，与 retro-go（任天堂布局）
     * 的 B 在下、A 在右 位置一致 → A→B、B→A；X(左)/Y(上) 同理。 */
    if (hi & 0x08) st |= RG_KEY_B;      /* XInput A（下） */
    if (hi & 0x10) st |= RG_KEY_A;      /* XInput B（右） */
    if (hi & 0x20) st |= RG_KEY_Y;      /* XInput X（左） */
    if (hi & 0x40) st |= RG_KEY_X;      /* XInput Y（上） */

    if (d[4] > 128) st |= RG_KEY_L;     /* LT 过阈值也算 L */
    if (d[5] > 128) st |= RG_KEY_R;     /* RT 同上 */

    const int16_t lx = (int16_t)(d[6] | (d[7] << 8));
    const int16_t ly = (int16_t)(d[8] | (d[9] << 8));
    if (lx < -XI_STICK_DEAD) st |= RG_KEY_LEFT;
    if (lx >  XI_STICK_DEAD) st |= RG_KEY_RIGHT;
    if (ly < -XI_STICK_DEAD) st |= RG_KEY_UP;
    if (ly >  XI_STICK_DEAD) st |= RG_KEY_DOWN;

    s_state = st;
}

/* 中断 IN 传输完成回调：解析 + 立刻重新提交（持续轮询） */
static void xi_xfer_cb(usb_transfer_t *xfer)
{
    if (xfer->status == USB_TRANSFER_STATUS_COMPLETED)
    {
        xi_parse(xfer->data_buffer, xfer->actual_num_bytes);
        usb_host_transfer_submit(xfer);
    }
    else if (xfer->status == USB_TRANSFER_STATUS_NO_DEVICE ||
             xfer->status == USB_TRANSFER_STATUS_CANCELED)
    {
        RG_LOGI("USBHID: XI transfer stopped (status=%d)\n", (int)xfer->status);
        s_xi_streaming = false;
        usb_host_transfer_free(xfer);
    }
    else
    {
        /* 超时/溢出等：重提，但要限流，否则会形成忙循环 */
        vTaskDelay(pdMS_TO_TICKS(10));
        usb_host_transfer_submit(xfer);
    }
}

/* ══ Nintendo Pro 控制器：中断 OUT 握手 + 中断 IN 轮询 ══════════════════════
 * 输出帧固定 64 字节（Chromium nintendo_controller.cc 的布局）：
 *   [0]     report ID（0x01 = 带 rumble 的子命令；0x80 = USB 专用子命令）
 *   [1]     帧计数器（低 4 位递增）
 *   [2..9]  rumble 8 字节（中性值 00 01 40 40 00 01 40 40；0x80 帧则全 0）
 *   [10]    子命令 ID
 *   [11..]  子命令参数
 *
 * 三帧顺序（顺序不能换）：
 *   1) 0x80 / 0x04 —— disable USB timeout。必须**第一个**发，否则手柄几秒后
 *      自我复位（这正是之前 80 次重新枚举的原因）。
 *   2) 0x01 / 0x03 0x30 —— 输入报告模式 = 标准全量 0x30，手柄从此 60Hz 推报文。
 *   3) 0x01 / 0x10 0x02 —— set status: playing。 */
static const uint8_t SW_NEUTRAL_RUMBLE[8] = {0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40};

static void sw_out_step(void);
static void sw_in_submit(void);

static void sw_out_cb(usb_transfer_t *x)
{
    if (x->status != USB_TRANSFER_STATUS_COMPLETED)
        RG_LOGW("USBHID: SW out xfer status=%d (step %d)\n", (int)x->status, s_sw_step);
    ++s_sw_step;
    if (s_sw_step <= 2)
        sw_out_step();
    else
    {
        RG_LOGI("USBHID: Nintendo handshake DONE (3 frames) — waiting for 0x30 reports\n");
        sw_in_submit();
    }
}

/* 构造并提交第 s_sw_step 帧 */
static void sw_out_step(void)
{
    if (!s_sw_out_x)
        return;

    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    ++s_sw_seq;

    switch (s_sw_step)
    {
    case 0:
        buf[0] = 0x80;
        buf[1] = s_sw_seq & 0x0F;
        buf[10] = 0x04;                      /* disable USB timeout */
        RG_LOGI("USBHID: SW -> 80/04 disable-usb-timeout\n");
        break;
    case 1:
        buf[0] = 0x01;
        buf[1] = s_sw_seq & 0x0F;
        memcpy(buf + 2, SW_NEUTRAL_RUMBLE, 8);
        buf[10] = 0x03;                      /* set input report mode */
        buf[11] = 0x30;                      /* standard full */
        RG_LOGI("USBHID: SW -> 01/03/30 full-report-mode\n");
        break;
    default:
        buf[0] = 0x01;
        buf[1] = s_sw_seq & 0x0F;
        memcpy(buf + 2, SW_NEUTRAL_RUMBLE, 8);
        buf[10] = 0x10;                      /* set status */
        buf[11] = 0x02;                      /* playing */
        RG_LOGI("USBHID: SW -> 01/10/02 set-playing\n");
        break;
    }

    memcpy(s_sw_out_x->data_buffer, buf, sizeof(buf));
    s_sw_out_x->num_bytes = sizeof(buf);
    if (usb_host_transfer_submit(s_sw_out_x) != ESP_OK)
        RG_LOGW("USBHID: SW out submit FAILED at step %d\n", s_sw_step);
}

/* 输入报文：dump 前若干条 + 解析 0x30；处理完立刻重新提交以持续轮询 */
static void sw_in_cb(usb_transfer_t *x)
{
    if (x->status == USB_TRANSFER_STATUS_COMPLETED && x->actual_num_bytes >= 2)
    {
        const uint8_t *d = x->data_buffer;
        const size_t n = x->actual_num_bytes;

        /* ★ 采样触发条件：**载荷（byte2 之后）发生变化**才打印。
         *
         * 第一版按"时间节流"（6 条/秒）是错的：byte1 是每帧都变的包计数器，
         * 空闲时也在刷，150 条的额度 22 秒就被烧光 —— 等用户真按键时已经没额度了
         * （实测 150 条全是同一个空闲载荷）。改成按载荷变化过滤后：
         * **空闲 == 0 条，按键 == 一次状态变化一条**。 */
        const size_t chk = (n < 24) ? n : 24;
        bool changed = (s_sw_last_len == 0);
        for (size_t i = 2; i < chk && !changed; ++i)
            if (i >= (size_t)s_sw_last_len || d[i] != s_sw_last[i])
                changed = true;

        const int64_t now_us = esp_timer_get_time();
        if (changed && s_sw_in_dump > 0 && (now_us - s_sw_last_dump_us) > 60000)
        {
            s_sw_last_dump_us = now_us;
            --s_sw_in_dump;

            char diff[3 * 24 + 1], hex[3 * 24 + 1];
            size_t kd = 0, k = 0;
            diff[0] = 0;
            for (size_t i = 0; i < chk; ++i)
            {
                if (i >= (size_t)s_sw_last_len || d[i] != s_sw_last[i])
                    kd += snprintf(diff + kd, sizeof(diff) - kd, "%d:%02x ", (int)i, d[i]);
                k += snprintf(hex + k, sizeof(hex) - k, "%02x ", d[i]);
            }
            RG_LOGI("USBHID: SW CHG id=0x%02x diff[%s] full[%s]\n", d[0], diff, hex);

            const size_t cp = (n < 64) ? n : 64;
            memcpy(s_sw_last, d, cp);
            s_sw_last_len = (int)cp;
        }

        if (d[0] == 0x30)
        {
            if (!s_sw_streaming)
            {
                s_sw_streaming = true;
                RG_LOGI("USBHID: Nintendo streaming (0x30) — handshake accepted\n");
            }
            usbhid_decode_switch_pro(d + 1, n - 1);   /* 去掉 report ID 后按 Switch 布局解 */
        }
        else if (d[0] == 0x21)
            RG_LOGI("USBHID: SW 0x21 subcmd-reply\n");
        else if (d[0] == 0x81)
            RG_LOGI("USBHID: SW 0x81 init-reply\n");
    }
    else if (x->status != USB_TRANSFER_STATUS_COMPLETED &&
             x->status != USB_TRANSFER_STATUS_CANCELED)
    {
        /* 超时/溢出：说明手柄没在发数据。前几次打出来，之后限流，避免刷屏。 */
        if (s_sw_in_status_log > 0)
        {
            --s_sw_in_status_log;
            RG_LOGW("USBHID: SW in xfer status=%d (no data from pad)\n", (int)x->status);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (x->device_handle)
        usb_host_transfer_submit(x);
}

static void sw_in_submit(void)
{
    if (s_sw_in_x)
        usb_host_transfer_submit(s_sw_in_x);
}

/* 起 Nintendo 通道：认领接口 → 分配两个 64B 传输 → 提第一帧握手。
 * 返回 true = 已接管（句柄不能 close）；false = 失败，调用方负责 close。 */
static bool nintendo_start(usb_device_handle_t dev_hdl, uint8_t intf, uint8_t alt,
                           uint8_t in_ep, uint8_t out_ep)
{
    /* ★ 必须先认领接口：不认领则库不给端点分配句柄，提交端点传输会报
     * ESP_ERR_NOT_FOUND（"Get EP handle error"）。这是 USB Host 的硬性步骤。 */
    esp_err_t cerr = usb_host_interface_claim(s_xi_client, dev_hdl, intf, alt);
    if (cerr != ESP_OK)
    {
        RG_LOGW("USBHID: interface_claim(iface=%d alt=%d) failed (0x%x)\n",
                intf, alt, (unsigned)cerr);
        return false;
    }
    RG_LOGI("USBHID: claimed iface=%d alt=%d\n", intf, alt);

    if (usb_host_transfer_alloc(64, 0, &s_sw_out_x) != ESP_OK || !s_sw_out_x)
    {
        RG_LOGW("USBHID: SW out transfer alloc failed\n");
        usb_host_interface_release(s_xi_client, dev_hdl, intf);
        return false;
    }
    if (usb_host_transfer_alloc(64, 0, &s_sw_in_x) != ESP_OK || !s_sw_in_x)
    {
        RG_LOGW("USBHID: SW in transfer alloc failed\n");
        usb_host_transfer_free(s_sw_out_x);
        s_sw_out_x = NULL;
        usb_host_interface_release(s_xi_client, dev_hdl, intf);
        return false;
    }

    s_sw_out_x->device_handle = dev_hdl;
    s_sw_out_x->bEndpointAddress = out_ep;
    s_sw_out_x->callback = sw_out_cb;
    s_sw_out_x->context = NULL;

    s_sw_in_x->device_handle = dev_hdl;
    s_sw_in_x->bEndpointAddress = in_ep;
    s_sw_in_x->callback = sw_in_cb;
    s_sw_in_x->context = NULL;
    /* ★ 中断 IN 必须设 num_bytes = 接收缓冲长度。漏了它传输长度就是 0，
     * 永远不会带回任何数据 —— 这就是"OUT 三帧全 OK、IN 一条都收不到"的原因。 */
    s_sw_in_x->num_bytes = 64;

    s_sw_in_ep = in_ep;
    s_sw_out_ep = out_ep;
    s_sw_step = 0;
    s_sw_seq = 0;
    s_sw_in_dump = 0;
    s_sw_last_len = 0;
    s_sw_last_dump_us = 0;
    s_sw_streaming = false;
    s_sw_in_status_log = 8;
    s_seen_0x30 = false;

    s_xi_dev = dev_hdl;          /* 记下来，DEV_GONE 时要 close */
    s_xi_claimed_intf = intf;
    s_xi_has_claim = true;
    s_connected = true;
    snprintf(s_dev_name, sizeof(s_dev_name), "Nintendo Pro (USB)");

    RG_LOGI("USBHID: Nintendo channel up (out=0x%02x in=0x%02x) -> handshake\n",
            out_ep, in_ep);
    sw_out_step();
    return true;
}

static void nintendo_stop(void)
{
    if (s_sw_in_x)
    {
        s_sw_in_x->device_handle = NULL;   /* 阻止回调里重新提交 */
        usb_host_transfer_free(s_sw_in_x);
        s_sw_in_x = NULL;
    }
    if (s_sw_out_x)
    {
        s_sw_out_x->device_handle = NULL;
        usb_host_transfer_free(s_sw_out_x);
        s_sw_out_x = NULL;
    }
    s_sw_step = 0;
    s_sw_streaming = false;
}

/* 归还接口认领。设备还在时必须调用，否则下一次接入会因为接口被占而 claim 失败。 */
static void xi_release_claim(usb_device_handle_t dev_hdl)
{
    if (s_xi_has_claim && dev_hdl && s_xi_client)
    {
        esp_err_t r = usb_host_interface_release(s_xi_client, dev_hdl, s_xi_claimed_intf);
        if (r != ESP_OK)
            RG_LOGI("USBHID: interface_release(iface=%d) -> 0x%x (device already gone?)\n",
                    s_xi_claimed_intf, (unsigned)r);
    }
    s_xi_has_claim = false;
}

/* 遍历配置描述符：打印所有接口/端点；发现 class 0xFF + 中断 IN 即认定 XInput。
 *
 * ★ 返回 true = 我们**接管**了这个设备（句柄由 s_xi_dev 持有，调用方不要再 close）；
 *   false = 不归我们用，**调用方必须 usb_host_device_close()**。
 *
 * 为什么要这么较真：USB Host 库的回收逻辑是「open_count==0 才允许 free」，
 * 而 open 过不 close 的设备被拔掉后会永久卡在 is_gone 状态、所有后续 open
 * 都返回 ESP_ERR_INVALID_STATE(0x103)。我们之前就是因为"发现不是 XInput
 * 就直接 return、不 close"，把每个插进来的设备都锁成了幽灵。 */
static bool xi_probe(usb_device_handle_t dev_hdl)
{
    const usb_config_desc_t *cfg = NULL;
    if (usb_host_get_active_config_descriptor(dev_hdl, &cfg) != ESP_OK || !cfg)
    {
        RG_LOGW("USBHID: XI get-config-desc failed\n");
        return false;
    }

    /* 先认一下这是谁：VID/PID 是判断"哪只手柄"的第一手依据 */
    uint16_t vid = 0, pid = 0;
    const usb_device_desc_t *ddesc = NULL;
    if (usb_host_get_device_descriptor(dev_hdl, &ddesc) == ESP_OK && ddesc)
    {
        vid = ddesc->idVendor;
        pid = ddesc->idProduct;
    }
    RG_LOGI("USBHID: device VID=%04x PID=%04x\n", vid, pid);

    RG_LOGI("USBHID: cfg: %d interface(s), %d bytes\n", cfg->bNumInterfaces, cfg->wTotalLength);

    uint8_t  xi_ep = 0;
    uint16_t xi_mps = 0;
    uint8_t  hid_in = 0, hid_out = 0;      /* Nintendo 形态：HID 接口上的中断 IN/OUT */
    uint16_t hid_in_mps = 0;
    /* ★ 端点所属的接口号/备用设置：**提交端点传输前必须 claim**，否则库不会分配
     * 端点句柄，提交会直接报 ESP_ERR_NOT_FOUND（实测日志：
     * "USB HOST: Get EP handle error: ESP_ERR_NOT_FOUND"）。 */
    uint8_t  xi_intf = 0, xi_alt = 0;
    uint8_t  hid_intf = 0, hid_alt = 0;
    bool     xi_intf_set = false, hid_intf_set = false;

    for (int i = 0; i < cfg->bNumInterfaces; ++i)
    {
        int off = 0;
        const usb_intf_desc_t *intf = usb_parse_interface_descriptor(cfg, (uint8_t)i, 0, &off);
        if (!intf)
            continue;

        RG_LOGI("USBHID: iface%d class=0x%02x sub=0x%02x proto=0x%02x eps=%d\n",
                i, intf->bInterfaceClass, intf->bInterfaceSubClass,
                intf->bInterfaceProtocol, intf->bNumEndpoints);

        for (int e = 0; e < intf->bNumEndpoints; ++e)
        {
            int eoff = 0;
            const usb_ep_desc_t *ep = usb_parse_endpoint_descriptor_by_index(
                intf, e, cfg->wTotalLength, &eoff);
            if (!ep)
                continue;
            RG_LOGI("USBHID:   ep%d addr=0x%02x attr=0x%02x mps=%d\n",
                    e, ep->bEndpointAddress, ep->bmAttributes, ep->wMaxPacketSize);

            const bool is_in  = (ep->bEndpointAddress & 0x80) != 0;
            const bool is_int = (ep->bmAttributes & 0x03) == 0x03;
            if (intf->bInterfaceClass == 0xFF && is_in && is_int && !xi_ep)
            {
                xi_ep  = ep->bEndpointAddress;
                xi_mps = ep->wMaxPacketSize;
                xi_intf = intf->bInterfaceNumber;
                xi_alt = intf->bAlternateSetting;
                xi_intf_set = true;
            }
            /* Nintendo 形态：HID 接口 + 大包(≥32B)中断 IN + 中断 OUT。
             * Pro 控制器 USB 直连就是「1 接口 / 64B IN / 64B OUT」这一种。 */
            if (intf->bInterfaceClass == 0x03 && is_int)
            {
                if (is_in && !hid_in && ep->wMaxPacketSize >= 32)
                {
                    hid_in = ep->bEndpointAddress;
                    hid_in_mps = ep->wMaxPacketSize;
                    hid_intf = intf->bInterfaceNumber;
                    hid_alt = intf->bAlternateSetting;
                    hid_intf_set = true;
                }
                else if (!is_in && !hid_out && ep->wMaxPacketSize >= 32)
                {
                    hid_out = ep->bEndpointAddress;
                    hid_intf = intf->bInterfaceNumber;
                    hid_alt = intf->bAlternateSetting;
                    hid_intf_set = true;
                }
            }
        }
    }

    /* Nintendo Pro 控制器（VID 057e）：有自己的握手协议，走中断 OUT。
     * 必须在 XInput 判断之前，因为它也是 class 0x03，会被当成"不是 XInput"丢掉。 */
    if (vid == 0x057e && hid_in && hid_out)
    {
        RG_LOGI("USBHID: Nintendo controller detected (in=0x%02x mps=%d out=0x%02x)\n",
                hid_in, hid_in_mps, hid_out);
        return nintendo_start(dev_hdl, hid_intf, hid_alt, hid_in, hid_out);
    }

    if (!xi_ep)
    {
        RG_LOGW("USBHID: no vendor(0xFF)+int-IN interface and not a Nintendo pad — ignoring\n");
        return false;
    }

    /* ★ 提交端点传输前必须认领接口（同 Nintendo 路径，见那边注释） */
    esp_err_t cerr = usb_host_interface_claim(s_xi_client, dev_hdl, xi_intf, xi_alt);
    if (cerr != ESP_OK)
    {
        RG_LOGW("USBHID: XI interface_claim(iface=%d alt=%d) failed (0x%x)\n",
                xi_intf, xi_alt, (unsigned)cerr);
        return false;
    }
    s_xi_claimed_intf = xi_intf;
    s_xi_has_claim = true;

    usb_transfer_t *xfer = NULL;
    if (usb_host_transfer_alloc(XI_BUF_SIZE, 0, &xfer) != ESP_OK || !xfer)
    {
        RG_LOGW("USBHID: XI transfer alloc failed\n");
        xi_release_claim(dev_hdl);
        return false;
    }
    xfer->device_handle = dev_hdl;
    xfer->bEndpointAddress = xi_ep;
    xfer->callback = xi_xfer_cb;
    xfer->context = NULL;
    xfer->num_bytes = XI_BUF_SIZE;   /* 中断 IN 必填，否则收不到数据（同 Nintendo 路径） */

    if (usb_host_transfer_submit(xfer) != ESP_OK)
    {
        RG_LOGW("USBHID: XI transfer submit failed\n");
        usb_host_transfer_free(xfer);
        xi_release_claim(dev_hdl);
        return false;
    }

    s_xi_dev = dev_hdl;
    s_connected = true;
    snprintf(s_dev_name, sizeof(s_dev_name), "XInput gamepad");
    RG_LOGI("USBHID: XI polling started (ep=0x%02x mps=%d)\n", xi_ep, xi_mps);
    return true;
}

static void xi_client_cb(const usb_host_client_event_msg_t *msg, void *arg)
{
    (void)arg;
    switch (msg->event)
    {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
    {
        RG_LOGI("USBHID: XI new device addr=%d\n", msg->new_dev.address);
        usb_device_handle_t dev = NULL;
        if (usb_host_device_open(s_xi_client, msg->new_dev.address, &dev) == ESP_OK && dev)
        {
            /* xi_probe 返回 false = 不是我们的设备，**必须** close —— 不 close 就会
             * 把设备锁成"拔不掉的幽灵"（详见 xi_probe 的注释）。 */
            if (!xi_probe(dev))
                usb_host_device_close(s_xi_client, dev);
        }
        else
            RG_LOGW("USBHID: XI device open failed\n");
        break;
    }
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
        RG_LOGI("USBHID: XI device gone\n");
        s_xi_streaming = false;
        s_xi_probed = false;      /* 允许下次接入重新 probe */
        nintendo_stop();          /* 释放 Nintendo 两个传输（内含停止回调重提） */
        /* 我们持有的句柄也必须 close，否则设备永远不回收（open_count 归不了零）。
         * 顺序：先 release 接口认领（设备已 gone 时可能失败，无妨），再 close。 */
        if (s_xi_dev)
        {
            xi_release_claim(s_xi_dev);
            usb_host_device_close(s_xi_client, s_xi_dev);
            s_xi_dev = NULL;
        }
        s_xi_dump = 20;
        s_state = 0;
        break;
    default:
        break;
    }
}

/* 扫描当前已接入的设备 —— 热插拔事件之外的兜底。
 *
 * 为什么必须**周期**调用（两个时序空档都要兜住）：
 *   1) USB Host 的 NEW_DEV 只发给"client 注册之后"接入的设备 —— 手柄若在 init
 *      之前就插好了，事件早已发过，我们现在收不到；
 *   2) 反过来，usb_host_start 刚返回时设备可能**还没枚举完**，此刻扫描也是空的。
 * 所以 task 用短超时轮询事件 + 每约 2 秒重扫一次，两种情况都能覆盖。 */
static void xi_scan_existing(void)
{
    if (s_xi_probed)
        return;

    uint8_t addrs[8] = {0};
    int n = 0;
    if (usb_host_device_addr_list_fill(8, addrs, &n) != ESP_OK || n <= 0)
        return;

    bool all_ok = true;
    for (int i = 0; i < n; ++i)
    {
        usb_device_handle_t dev = NULL;
        esp_err_t err = usb_host_device_open(s_xi_client, addrs[i], &dev);
        if (err == ESP_OK && dev)
        {
            RG_LOGI("USBHID: XI opened addr=%d (%d device(s) on bus)\n", addrs[i], n);
            if (!xi_probe(dev))
                usb_host_device_close(s_xi_client, dev);   /* 不是 XInput：立刻还回去，别锁成幽灵 */
        }
        else
        {
            /* err == ESP_ERR_INVALID_STATE(0x103) 的确切含义（查 IDF 源码 usbh.c/usbh_devs_open）：
             *   is_gone || waiting_free —— 设备**已被标记为「已拔出」或「待释放」**。
             * 也就是说：地址表里这个条目是**幽灵**（曾经枚举成功、现在已经不在了），
             * **不是**「还没枚举完」。别再拿它当"快好了"的信号。
             *
             * 幽灵条目不会自己消失（没被任何 client 打开过，也就没人 close 它），
             * 所以我们会一直扫到它。下面的 VBUS 重启就是为这种情况准备的兜底。 */
            all_ok = false;
            if (!s_xi_ghost_logged)
            {
                s_xi_ghost_logged = true;
                RG_LOGW("USBHID: addr=%d is a GONE/ghost entry (0x%x) — no live pad on the bus\n",
                        addrs[i], (unsigned)err);
            }
        }
    }
    if (all_ok)
    {
        s_xi_probed = true;
        s_xi_fail_streak = 0;
    }
    else
    {
        ++s_xi_fail_streak;
    }
}

static void xi_client_task(void *arg)
{
    (void)arg;
    if (!s_xi_client)
    {
        vTaskDelete(NULL);
        return;
    }

    int tick = 0;
    while (1)
    {
        /* 短超时轮询：既处理事件，也留出时间做周期重扫 */
        usb_host_client_handle_events(s_xi_client, pdMS_TO_TICKS(100));
        if (++tick >= 10)          /* ≈1 秒一次：设备枚举只需几十~几百 ms，重试要够快 */
        {
            tick = 0;
            xi_scan_existing();

            /* ── Nintendo 保活：手柄没进入推流状态时，每秒重发一次握手 ────────
             * 两个理由：
             *  1) Nintendo 固件在没有主机输出报文时会超时自我复位（实测周期
             *     约 2.5s，地址一路涨）。周期性输出报文本身就是 keep-alive。
             *  2) 万一它在我们handshake 之后才刚复位完，重发能把状态补上。 */
            if (s_sw_out_x && s_sw_step >= 3 && !s_sw_streaming)
            {
                s_sw_step = 0;
                sw_out_step();
            }

            /* ── USB-A 口供电强制重启（自愈）────────────────────────────────
             * 触发场景：P4 这边反复复位/USB 主机层重启，而手柄一直有电、
             * 它的 USB 从机栈以为"我已经被配置过了"，于是**再也不重新枚举**。
             * 表现就是地址表里留一个 GONE 幽灵条目、open 永远 0x103。
             * （我这几轮大量用 esptool 复位，正是在制造这个状态。）
             *
             * 唯一可靠的解法是**断一次 VBUS**让手柄整颗 USB 从机重启：
             * 关 → 停 400ms → 开 → 手柄重新上电、重新枚举、发 NEW_DEV。
             *
             * 只在「连续 6 秒打不开 + 从未成功过」时做，最多 3 次 ——
             * 没插手柄时也只是空转几次，不刷屏、不占 CPU。 */
            if (s_xi_fail_streak >= 6 && s_xi_vbus_cycles < 3 && !s_connected)
            {
                ++s_xi_vbus_cycles;
                s_xi_fail_streak = 0;
                RG_LOGW("USBHID: no usable device for 6s -> power-cycling USB-A VBUS (try %d/3)\n",
                        s_xi_vbus_cycles);
                bsp_set_usb_5v_en(false);
                vTaskDelay(pdMS_TO_TICKS(400));
                bsp_set_usb_5v_en(true);
                s_xi_probed = false;
                s_xi_ghost_logged = false;
            }
        }
    }
}

/* 注册 XI client，并**主动扫描已经插着的设备**。
 *
 * 为什么必须主动扫：USB Host 的 NEW_DEV 事件只发给"注册那一刻之后"接入的设备。
 * 我们在 init 时才注册，而手柄那时往往已经插好并枚举完了 —— 光等事件会永远等不到。
 * 实测日志里只有 "XI client registered"、没有 "XI new device"，就是这个原因。 */
static bool xi_client_start(void)
{
    usb_host_client_config_t cfg = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {
            .client_event_callback = xi_client_cb,
            .callback_arg = NULL,
        },
    };
    if (usb_host_client_register(&cfg, &s_xi_client) != ESP_OK || !s_xi_client)
    {
        RG_LOGW("USBHID: XI client register failed\n");
        return false;
    }
    RG_LOGI("USBHID: XI client registered\n");

    /* 先扫一次（此刻可能还没枚举完，没关系 —— task 里每 2 秒会重扫） */
    xi_scan_existing();
    return true;
}

bool rg_usbhid_init(void)
{
    if (s_host_started)
        return true;   /* 已初始化过（host + XI client 幂等） */

    /* USB-A 口的 5V 供电要显式打开（经 PI4IOE5V6408 扩展器，见 BSP）。
     *
     * ⚠ 实测的坑：`bsp_set_usb_5v_en()` 走 I2C，而 **I2C 总线在本函数被调用时
     * 还没初始化**（日志：`i2c.master: i2c handle not initialized`）——
     * 所以之前所有"Vbus 断电重启"其实都是空操作。先补上 bsp_i2c_init()
     * （幂等，BSP 内部有 i2c_initialized 守卫）。 */
    esp_err_t ierr = bsp_i2c_init();
    if (ierr != ESP_OK)
        RG_LOGW("USBHID: bsp_i2c_init failed (0x%x) — USB 5V 控制不可用\n", (unsigned)ierr);

    bsp_set_usb_5v_en(false);
    vTaskDelay(pdMS_TO_TICKS(400));
    bsp_set_usb_5v_en(true);
    s_xi_vbus_cycles = 0;
    s_xi_fail_streak = 0;
    s_xi_ghost_logged = false;

    esp_err_t err = bsp_usb_host_start(BSP_USB_HOST_POWER_MODE_USB_DEV, true);
    if (err != ESP_OK)
    {
        RG_LOGW("USBHID: usb_host_start failed (0x%x)\n", (unsigned)err);
        return false;
    }
    s_host_started = true;

    /* XI client 必须**先于** hid_host_install 注册，并在注册后主动扫描已插着的设备
     * （原因见 xi_client_start 的注释）。 */
    if (xi_client_start())
    {
        if (xTaskCreate(xi_client_task, "xi_client", 4096, NULL, 4, NULL) != pdTRUE)
            RG_LOGW("USBHID: xi_client task create failed\n");
    }

    /* ⚠ 不安装 hid_host —— 这是关键取舍。
     *
     * IDF 的 USB Host 库规定：**一个设备同一时刻只能被一个 client 打开**。
     * hid_host 一旦 install，就会自动 open 它扫描到的 HID 接口（我们这手柄的
     * Nintendo Pro Controller 兼容层），于是我们自己的 XI client 再 open 同一设备时
     * 会持续拿到 ESP_ERR_INVALID_STATE（实测每秒重试一次、每次都是 0x103，永不成功）。
     *
     * 而 HID 那条路本来就走不通：Switch Pro 的命令必须走**中断 OUT**端点，
     * usb_host_hid 组件只提供控制传输 SET_REPORT，实测 6 步握手全 ACK 但手柄毫无反应。
     * 所以干脆放弃 HID，把设备整个让给 XInput 通道。
     *
     * 下面的 hid_driver_cb / switch_pro_* 等代码保留 —— 它们是协议记录，
     * 不 install 就永远不会被触发。 */
    RG_LOGI("USBHID: hid_host NOT installed (device reserved for the XInput path)\n");
    s_hid_installed = false;

    RG_LOGI("USBHID: ready (host up; XInput path active), plug a pad into the USB-A port\n");
    return true;
}

void rg_usbhid_deinit(void)
{
    if (s_hid_installed)
    {
        hid_host_uninstall();
        s_hid_installed = false;
    }
    if (s_host_started)
    {
        bsp_usb_host_stop();
        s_host_started = false;
    }
    s_state = 0;
    s_connected = false;
    s_dev_name[0] = 0;
}

#endif /* RG_USB_HID_GAMEPAD */
