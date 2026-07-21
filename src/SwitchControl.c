#include "SwitchControl.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "pico/stdlib.h"
#include "tusb.h"

// 当前要发送给主机的 HID 报告缓冲。
static USB_JoystickReport_Input_t controller_report;
// 单次脉冲按键的最短按下时长（毫秒）。
static const uint32_t BUTTON_PUSHING_MSEC = 10;
// 是否在状态变化后自动发送报告。
static bool _autoSend = true;

// 保证按下时长不小于最短阈值。
static uint32_t clamp_press_ms(uint32_t requested_ms) {
  return (requested_ms < BUTTON_PUSHING_MSEC) ? BUTTON_PUSHING_MSEC : requested_ms;
}

// 将百分比输入映射到 0~255 的摇杆轴数值。
static uint8_t percent_to_axis(int8_t percent) {
  if (percent > 100) {
    percent = 100;
  } else if (percent < -100) {
    percent = -100;
  }

  const int32_t value = 128 + ((int32_t)percent * 127) / 100;
  if (value < 0) {
    return 0;
  }
  if (value > 255) {
    return 255;
  }
  return (uint8_t)value;
}

// 延时期间持续跑 TinyUSB 任务，避免 USB 超时。
static void controller_delay_ms(uint32_t delay_time_msec) {
  while (delay_time_msec--) {
    tud_task();
    sleep_ms(1);
  }
}

// 初始化报告缓存，把摇杆放回中心位。
void USBInit(void) {
  memset(&controller_report, 0, sizeof(controller_report));
  controller_report.LX = 128;
  controller_report.LY = 128;
  controller_report.RX = 128;
  controller_report.RY = 128;
  controller_report.Hat = HAT_CENTER;
}

// 向主机发送当前 HID 报告。
void sendReport(void) {
  if (!tud_mounted() || !tud_hid_ready()) {
    return;
  }

  (void)tud_hid_n_report(0, 0, &controller_report, sizeof(controller_report));
}

// 按下一个按钮位。
void pressButton(uint16_t button) {
  controller_report.Button |= button;
  if (_autoSend) {
    sendReport();
  }
}

// 松开一个按钮位。
void releaseButton(uint16_t button) {
  controller_report.Button &= (uint16_t)~button;
  if (_autoSend) {
    sendReport();
  }
}

// 设置方向键 Hat 值。
void pressHatButton(uint8_t hat) {
  controller_report.Hat = hat;
  if (_autoSend) {
    sendReport();
  }
}

// 释放方向键，回到中心位。
void releaseHatButton(void) {
  controller_report.Hat = HAT_CENTER;
  if (_autoSend) {
    sendReport();
  }
}

// 设置四个摇杆轴的位置。
void setStickTiltRatio(int8_t lx_per, int8_t ly_per, int8_t rx_per, int8_t ry_per) {
  controller_report.LX = percent_to_axis(lx_per);
  controller_report.LY = percent_to_axis(ly_per);
  controller_report.RX = percent_to_axis(rx_per);
  controller_report.RY = percent_to_axis(ry_per);
  if (_autoSend) {
    sendReport();
  }
}

// 按下按钮，等待一段时间后松开。
void pushButton(uint16_t button, uint32_t delay_time_msec) {
  pressButton(button);
  if (!_autoSend) {
    sendReport();
  }
  controller_delay_ms(BUTTON_PUSHING_MSEC);
  releaseButton(button);
  if (!_autoSend) {
    sendReport();
  }
  controller_delay_ms(delay_time_msec);
  controller_delay_ms(BUTTON_PUSHING_MSEC);
}

// 连续执行多次按钮脉冲。
void pushButtonLoop(uint16_t button, uint32_t delay_time_msec, uint16_t loop_num) {
  for (uint32_t i = 0; i < loop_num; i++) {
    pressButton(button);
    if (!_autoSend) {
      sendReport();
    }
    controller_delay_ms(BUTTON_PUSHING_MSEC);
    releaseButton(button);
    if (!_autoSend) {
      sendReport();
    }
    controller_delay_ms(delay_time_msec);
  }
  controller_delay_ms(BUTTON_PUSHING_MSEC);
}

// 长按按钮一段时间后释放。
void pushButtonContinuous(uint16_t button, uint32_t pushing_time_msec) {
  const uint32_t hold_time_msec = clamp_press_ms(pushing_time_msec);
  pressButton(button);
  if (!_autoSend) {
    sendReport();
  }
  controller_delay_ms(hold_time_msec);
  releaseButton(button);
  if (!_autoSend) {
    sendReport();
  }
  controller_delay_ms(BUTTON_PUSHING_MSEC);
}

// 按下方向键，等待后释放。
void pushHatButton(uint8_t hat, uint32_t delay_time_msec) {
  pressHatButton(hat);
  if (!_autoSend) {
    sendReport();
  }
  controller_delay_ms(BUTTON_PUSHING_MSEC);
  releaseHatButton();
  if (!_autoSend) {
    sendReport();
  }
  controller_delay_ms(delay_time_msec);
  controller_delay_ms(BUTTON_PUSHING_MSEC);
}

// 连续执行多次方向键脉冲。
void pushHatButtonLoop(uint8_t hat, uint32_t delay_time_msec, uint16_t loop_num) {
  for (uint32_t i = 0; i < loop_num; i++) {
    pressHatButton(hat);
    if (!_autoSend) {
      sendReport();
    }
    controller_delay_ms(BUTTON_PUSHING_MSEC);
    releaseHatButton();
    if (!_autoSend) {
      sendReport();
    }
    controller_delay_ms(delay_time_msec);
  }
  controller_delay_ms(BUTTON_PUSHING_MSEC);
}

// 长按方向键一段时间后释放。
void pushHatButtonContinuous(uint8_t hat, uint32_t pushing_time_msec) {
  const uint32_t hold_time_msec = clamp_press_ms(pushing_time_msec);
  pressHatButton(hat);
  if (!_autoSend) {
    sendReport();
  }
  controller_delay_ms(hold_time_msec);
  releaseHatButton();
  if (!_autoSend) {
    sendReport();
  }
  controller_delay_ms(BUTTON_PUSHING_MSEC);
}

// 临时偏转摇杆，随后恢复到中心位。
void tiltJoystick(int8_t lx_per, int8_t ly_per, int8_t rx_per, int8_t ry_per, uint32_t tilt_time_msec) {
  setStickTiltRatio(lx_per, ly_per, rx_per, ry_per);
  if (!_autoSend) {
    sendReport();
  }
  controller_delay_ms(tilt_time_msec);
  setStickTiltRatio(0, 0, 0, 0);
  if (!_autoSend) {
    sendReport();
  }
  controller_delay_ms(BUTTON_PUSHING_MSEC);
}

// 设置是否自动发送报告。
void setAutoSendReport(bool autoSendReport) {
  _autoSend = autoSendReport;
}
