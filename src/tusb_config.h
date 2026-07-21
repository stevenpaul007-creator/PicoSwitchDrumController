#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#include "tusb_option.h"

// TinyUSB 目标平台：RP2040。
#define CFG_TUSB_MCU OPT_MCU_RP2040
// 设备模式 + 全速。
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
// EP0 控制端点大小。
#define CFG_TUD_ENDPOINT0_SIZE 64
// 只启用 HID。
#define CFG_TUD_HID 1
#define CFG_TUD_HID_EP_BUFSIZE 64
#define CFG_TUD_CDC 0
#define CFG_TUD_MSC 0
#define CFG_TUD_MIDI 0
#define CFG_TUD_VENDOR 0

#endif
