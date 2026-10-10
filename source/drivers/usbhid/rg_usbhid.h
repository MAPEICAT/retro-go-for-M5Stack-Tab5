/* USB HID 手柄 —— Tab5 板载 USB-A host 口（ESP32-P4 原生 USB 2.0 OTG，作 host）
 *
 * 为什么走 USB 而不是蓝牙：Tab5 的无线由板载 ESP32-C6 提供（经 SDIO 的 ESP-Hosted），
 * 但 C6 **只有 BLE、没有经典蓝牙**，而多数 Xbox 布局手柄的游戏链路是 BR/EDR；
 * USB 这条路零无线依赖，`usb_host_hid` 组件也已随 BSP vendor 在本地。
 * 蓝牙路线见 memory 里 2026-10-09 的记录（esp_hosted + LE 配对已知缺陷）。
 *
 * 设计：USB 是事件驱动的（设备接入/拔出/报文到达都是回调），而 retro-go 的输入层是
 * **轮询**的（rg_input 后台任务每帧读一次）。所以这里做成"回调写状态、输入层读状态"：
 *   - usbhid 内部由 hid_host 的后台任务驱动，收到报文就更新一个 uint32 位掩码
 *   - rg_input 那边只调 rg_usbhid_get_state() 取当前按住的键（无锁：单写单读、32 位对齐）
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/* 启动 USB host 与 HID 类驱动，并接上 USB-A 口的供电。
 * 幂等：重复调用只会返回上一次的结果（USB host 只能 install 一次）。
 * 返回 false 表示这条链路没起来（没插手柄不算失败 —— 插拔随时可能）。 */
bool rg_usbhid_init(void);

void rg_usbhid_deinit(void);

/* 当前是否有手柄在线（已打开 HID 接口） */
bool rg_usbhid_is_connected(void);

/* 当前按住的键，返回 rg_key_t 的位掩码（RG_KEY_*）。
 * 无手柄 / 未初始化时返回 0。 */
uint32_t rg_usbhid_get_state(void);

/* 已连手柄的产品名（用于日志/调试界面），未连接时返回空串。 */
const char *rg_usbhid_get_device_name(void);
