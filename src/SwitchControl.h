#ifndef SWITCH_CONTROL_H
#define SWITCH_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
//ybxa---xyab
// -----------------------------
// Switch 按键位定义
// -----------------------------
#define BUTTON_NONE 0x0000
#define BUTTON_Y 0x0008
#define BUTTON_B 0x0001
#define BUTTON_A 0x0002
#define BUTTON_X 0x0004


#define BUTTON_L 0x0010
#define BUTTON_R 0x0020
#define BUTTON_ZL 0x0040
#define BUTTON_ZR 0x0080
#define BUTTON_MINUS 0x0100
#define BUTTON_PLUS 0x0200
#define BUTTON_LCLICK 0x0400
#define BUTTON_RCLICK 0x0800
#define BUTTON_HOME 0x1000
#define BUTTON_CAPTURE 0x2000

// -----------------------------
// 方向键 Hat 值定义
// -----------------------------
#define HAT_UP 0x00
#define HAT_UP_RIGHT 0x01
#define HAT_RIGHT_UP 0x01
#define HAT_RIGHT 0x02
#define HAT_DOWN_RIGHT 0x03
#define HAT_RIGHT_DOWN 0x03
#define HAT_DOWN 0x04
#define HAT_DOWN_LEFT 0x05
#define HAT_LEFT_DOWN 0x05
#define HAT_LEFT 0x06
#define HAT_UP_LEFT 0x07
#define HAT_LEFT_UP 0x07
#define HAT_CENTER 0x08

// USB 发送给主机的手柄报告结构。
// 这里保持与原工程兼容的 HID 报告布局。
typedef struct __attribute__((packed)) {
  uint16_t Button;     // 按键位图
  uint8_t Hat;         // 十字键方向
  uint8_t LX;          // 左摇杆 X
  uint8_t LY;          // 左摇杆 Y
  uint8_t RX;          // 右摇杆 X
  uint8_t RY;          // 右摇杆 Y
  uint8_t VendorSpec;  // 厂商自定义字节
} USB_JoystickReport_Input_t;

// -----------------------------
// 控制器操作接口
// -----------------------------
// 初始化 USB 报告缓存。
void USBInit(void);
// 立即发送当前 HID 报告。
void sendReport(void);
// 按下 / 松开按键。
void pressButton(uint16_t button);
void releaseButton(uint16_t button);
// 按下 / 松开方向键。
void pressHatButton(uint8_t hat);
void releaseHatButton(void);
// 以百分比形式设置摇杆位置。
void setStickTiltRatio(int8_t lx_per, int8_t ly_per, int8_t rx_per, int8_t ry_per);
// 一次性按键脉冲。
void pushButton(uint16_t button, uint32_t delay_time_msec);
void pushButtonLoop(uint16_t button, uint32_t delay_time_msec, uint16_t loop_num);
void pushButtonContinuous(uint16_t button, uint32_t pushing_time_msec);
// 一次性方向键脉冲。
void pushHatButton(uint8_t hat, uint32_t delay_time_msec);
void pushHatButtonLoop(uint8_t hat, uint32_t delay_time_msec, uint16_t loop_num);
void pushHatButtonContinuous(uint8_t hat, uint32_t pushing_time_msec);
// 摇杆短暂偏转。
void tiltJoystick(int8_t lx_per, int8_t ly_per, int8_t rx_per, int8_t ry_per, uint32_t tilt_time_msec);
// 是否自动在状态变化时发送 HID 报告。
void setAutoSendReport(bool autoSendReport);

#ifdef __cplusplus
}
#endif

#endif
