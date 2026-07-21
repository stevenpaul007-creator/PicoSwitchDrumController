#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "tusb.h"
#include "pico/bootrom.h"

#include "SwitchControl.h"

// -----------------------------
// 输入引脚定义
// -----------------------------
// 这些 GPIO 对应普通按键输入，低电平表示按下。
static const uint8_t kButtonPins[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 14, 15};
// GPIO 按键与 HID 按钮位的映射关系，顺序必须和 kButtonPins 对齐。
static const uint16_t kButtonMasks[] = {
    BUTTON_ZR, BUTTON_R, BUTTON_B, BUTTON_A, BUTTON_Y,
    BUTTON_X, BUTTON_PLUS, BUTTON_HOME, BUTTON_MINUS,
    BUTTON_L, BUTTON_ZL,
};
// 方向键（Hat）输入引脚，低电平表示对应方向按下。
static const uint8_t kHatPins[] = {10, 11, 12, 13};

// ADC1..ADC4 阈值与映射。
// 现在采用“峰值触发 + 短脉冲 + 冷却时间”，每次击打都会生成一次独立按键。
// 映射关系：
//   ADC1 / GPIO26 -> B
//   ADC2 / GPIO27 -> A
//   ADC3 / GPIO28 -> DOWN
//   ADC4 / GPIO29 -> LEFT
static const uint8_t kAdcPins[] = {26, 27, 28, 29};
static const uint16_t kAdcThresholds[] = {600, 600, 600, 600};
// 单次 ADC 触发保持为按下的时间（微秒），至少 30ms。
static const uint32_t kAdcPressUs = 30000;
// 同一通道两次触发之间的最短间隔（微秒）。
static const uint32_t kAdcCooldownUs = 1000;

// 按键去抖时间（微秒）。
static const uint32_t kDebounceUs = 1000;
// LED 总开关：默认关闭；需要时改成 1 即可恢复所有 LED 逻辑。

#define ENABLE_LED_FEEDBACK 1
#ifndef ENABLE_LED_FEEDBACK
#define ENABLE_LED_FEEDBACK 0
#endif

// -----------------------------
// 跨核心共享状态
// -----------------------------
// core1（ADC）采样结果，按位表示当前稳定按下的按键/方向。
static volatile uint32_t g_adc_button_bits = 0;
static volatile uint32_t g_adc_hat_bits = 0;

// ADC 触发位定义。
enum AdcBit : uint32_t {
  ADC_BIT_B = 1u << 0,
  ADC_BIT_A = 1u << 1,
  ADC_BIT_DOWN = 1u << 2,
  ADC_BIT_LEFT = 1u << 3,
};

// -----------------------------
// 通用状态结构
// -----------------------------
// 单路输入的去抖状态：raw 是原始采样，stable 是稳定状态。
typedef struct {
  bool raw;
  bool stable;
  uint64_t changed_at_us;
} DebounceState;

// 单路 ADC 的峰值触发状态：last_active 记录上次是否高于阈值，
// pressed_until_us 控制按键保持时长，cooldown_until_us 防止同一尖峰重复触发。
typedef struct {
  bool last_active;
  uint64_t pressed_until_us;
  uint64_t cooldown_until_us;
} AdcTriggerState;

#if ENABLE_LED_FEEDBACK
// LED 点亮时长（微秒）。
static const uint32_t kLedPulseUs = 200000;
// WS2812B 使用的 GPIO、PIO 和状态机。
static const uint16_t kWs2812Pin = 0;
static const PIO kWs2812Pio = pio0;
static const uint kWs2812Sm = 0;

// LED 事件位定义。
enum LedEventBits : uint32_t {
  LED1_BLUE = 1u << 0,
  LED1_RED = 1u << 1,
  LED2_BLUE = 1u << 2,
  LED2_RED = 1u << 3,
};

// 单颗 WS2812 的脉冲起始时间；当值为 0 时表示该颜色没有被触发过。
typedef struct {
  uint64_t blue_on_us;
  uint64_t red_on_us;
} LedPulseState;

// core1 产生的 LED 事件，core0 会在主循环中消费。
static volatile uint32_t g_adc_led_events = 0;
// core0 产生的 LED 事件，core0 自己消费。
static volatile uint32_t g_gpio_led_events = 0;
#else
static inline void record_adc_led_event(uint32_t bits) { (void)bits; }
static inline void record_gpio_led_event(uint32_t bits) { (void)bits; }
static inline void apply_gpio_led_event(uint8_t pin, bool active) {
  (void)pin;
  (void)active;
}
#endif

// 获取当前时间戳，单位微秒。
static uint64_t now_us(void) {
  return time_us_64();
}

// 通用去抖逻辑：
// 1) 如果采样值变化，记录变化时刻；
// 2) 如果采样持续稳定超过 debounce_us，则更新 stable。
static bool debounce_update(DebounceState *state, bool sample, uint64_t now, uint32_t debounce_us) {
  if (sample != state->raw) {
    state->raw = sample;
    state->changed_at_us = now;
  }

  if (state->stable != state->raw && (now - state->changed_at_us) >= debounce_us) {
    state->stable = state->raw;
    return true;
  }

  return false;
}

// ADC 采样是否处于“击中”状态。
static bool adc_sample_active(uint16_t sample, uint16_t threshold) {
  return sample >= threshold;
}

#if ENABLE_LED_FEEDBACK
// 记录 ADC 侧触发的 LED 事件。
static void record_adc_led_event(uint32_t bits) {
  g_adc_led_events |= bits;
}

// 记录 GPIO 侧触发的 LED 事件。
static void record_gpio_led_event(uint32_t bits) {
  g_gpio_led_events |= bits;
}

// 根据 GPIO 编号把触发事件映射到对应的 LED 颜色。
static void apply_gpio_led_event(uint8_t pin, bool active) {
  if (!active) {
    return;
  }

  switch (pin) {
    case 3:
    case 5:
      record_gpio_led_event(LED1_RED);
      break;
    case 4:
    case 6:
      record_gpio_led_event(LED1_BLUE);
      break;
    case 10:
    case 11:
      record_gpio_led_event(LED2_RED);
      break;
    case 12:
    case 13:
      record_gpio_led_event(LED2_BLUE);
      break;
    default:
      break;
  }
}
#endif

// GPIO 低电平表示按下，因此这里取反后返回“是否按下”。
static bool gpio_sample_active(uint8_t pin) {
  return !gpio_get(pin);
}

// core0 扫描 GPIO 输入，输出当前稳定的按键位和方向位。
static void gpio_core0_scan(uint16_t *button_mask, uint32_t *hat_bits) {
  static DebounceState button_state[11];
  static DebounceState hat_state[4];
  uint64_t now = now_us();

  *button_mask = 0;
  *hat_bits = 0;

  for (size_t i = 0; i < 11; ++i) {
    const bool sample = gpio_sample_active(kButtonPins[i]);
    // 只在“从未按下 -> 稳定按下”的那一刻发出 LED 事件。
    if (debounce_update(&button_state[i], sample, now, kDebounceUs) && button_state[i].stable) {
      apply_gpio_led_event(kButtonPins[i], true);
    }
    if (button_state[i].stable) {
      *button_mask |= kButtonMasks[i];
    }
  }

  for (size_t i = 0; i < 4; ++i) {
    const bool sample = gpio_sample_active(kHatPins[i]);
    if (debounce_update(&hat_state[i], sample, now, kDebounceUs) && hat_state[i].stable) {
      apply_gpio_led_event(kHatPins[i], true);
    }
    if (hat_state[i].stable) {
      *hat_bits |= 1u << i;
    }
  }
}

// core1 负责 ADC 采样：
// - 读取 4 路 ADC
// - 检测阈值上穿（峰值触发）
// - 将每次击打转换成一个固定长度的按键脉冲
// - 可选记录 LED 事件
static void adc_core1_entry(void) {
  static AdcTriggerState adc_state[4];
  adc_init();

  for (uint8_t i = 0; i < 4; ++i) {
    adc_gpio_init(kAdcPins[i]);
  }

  while (true) {
    const uint64_t now = now_us();

    for (uint8_t i = 0; i < 4; ++i) {
      adc_select_input(i);
      const uint16_t sample = adc_read();
      const bool active = adc_sample_active(sample, kAdcThresholds[i]);
      const bool rising_edge = active && !adc_state[i].last_active;

      if (rising_edge && now >= adc_state[i].cooldown_until_us) {
        adc_state[i].pressed_until_us = now + kAdcPressUs;
        adc_state[i].cooldown_until_us = now + kAdcCooldownUs;
#if ENABLE_LED_FEEDBACK
        // 这里统一把 ADC 的“峰值触发”写入相应按键位，并触发对应 LED。
        switch (i) {
          case 0:
            g_adc_button_bits |= ADC_BIT_B;
            record_adc_led_event(LED1_RED);
            break;
          case 1:
            g_adc_button_bits |= ADC_BIT_A;
            record_adc_led_event(LED1_BLUE);
            break;
          case 2:
            g_adc_hat_bits |= ADC_BIT_DOWN;
            record_adc_led_event(LED2_RED);
            break;
          case 3:
            g_adc_hat_bits |= ADC_BIT_LEFT;
            record_adc_led_event(LED2_BLUE);
            break;
          default:
            break;
        }
#else
        switch (i) {
          case 0:
            g_adc_button_bits |= ADC_BIT_B;
            break;
          case 1:
            g_adc_button_bits |= ADC_BIT_A;
            break;
          case 2:
            g_adc_hat_bits |= ADC_BIT_DOWN;
            break;
          case 3:
            g_adc_hat_bits |= ADC_BIT_LEFT;
            break;
          default:
            break;
        }
#endif
      }

      adc_state[i].last_active = active;

      const bool pressed = now < adc_state[i].pressed_until_us;
      switch (i) {
        case 0:
          if (pressed) {
            g_adc_button_bits |= ADC_BIT_B;
          } else {
            g_adc_button_bits &= (uint32_t)~ADC_BIT_B;
          }
          break;
        case 1:
          if (pressed) {
            g_adc_button_bits |= ADC_BIT_A;
          } else {
            g_adc_button_bits &= (uint32_t)~ADC_BIT_A;
          }
          break;
        case 2:
          if (pressed) {
            g_adc_hat_bits |= ADC_BIT_DOWN;
          } else {
            g_adc_hat_bits &= (uint32_t)~ADC_BIT_DOWN;
          }
          break;
        case 3:
          if (pressed) {
            g_adc_hat_bits |= ADC_BIT_LEFT;
          } else {
            g_adc_hat_bits &= (uint32_t)~ADC_BIT_LEFT;
          }
          break;
        default:
          break;
      }
    }

    // 适当让出一点时间，避免 ADC 轮询过于紧密。
    sleep_us(100);
  }
}

// 根据方向位组合生成十字键 Hat 值。
static uint8_t read_hat_from_bits(bool right, bool down, bool up, bool left) {
  if (up && right && !down && !left) return HAT_UP_RIGHT;
  if (down && right && !up && !left) return HAT_DOWN_RIGHT;
  if (down && left && !up && !right) return HAT_DOWN_LEFT;
  if (up && left && !down && !right) return HAT_UP_LEFT;
  if (right && !left && !up && !down) return HAT_RIGHT;
  if (down && !up && !left && !right) return HAT_DOWN;
  if (up && !down && !left && !right) return HAT_UP;
  if (left && !right && !up && !down) return HAT_LEFT;
  return HAT_CENTER;
}

// 初始化所有 GPIO 输入，使用内部上拉。
static void init_gpio_inputs(void) {
  for (size_t i = 0; i < sizeof(kButtonPins) / sizeof(kButtonPins[0]); ++i) {
    gpio_init(kButtonPins[i]);
    gpio_set_dir(kButtonPins[i], GPIO_IN);
    gpio_pull_up(kButtonPins[i]);
  }
  for (size_t i = 0; i < sizeof(kHatPins) / sizeof(kHatPins[0]); ++i) {
    gpio_init(kHatPins[i]);
    gpio_set_dir(kHatPins[i], GPIO_IN);
    gpio_pull_up(kHatPins[i]);
  }
}

#if ENABLE_LED_FEEDBACK
// WS2812 PIO 程序：
// 这里使用非常短的程序，只保留发送一个像素所需的最小指令集。
static const uint16_t ws2812_instructions[] = {
    (uint16_t)(pio_encode_out(pio_x, 1) | pio_encode_sideset(1, 0) | pio_encode_delay(2)),
    (uint16_t)(pio_encode_jmp_not_x(3) | pio_encode_sideset(1, 1) | pio_encode_delay(1)),
    (uint16_t)(pio_encode_jmp(0) | pio_encode_sideset(1, 1) | pio_encode_delay(4)),
    (uint16_t)(pio_encode_nop() | pio_encode_sideset(1, 0) | pio_encode_delay(4)),
};

static const pio_program_t ws2812_program = {
    .instructions = ws2812_instructions,
    .length = 4,
    .origin = -1,
    .pio_version = PICO_PIO_VERSION,
#if PICO_PIO_VERSION > 0
    .used_gpio_ranges = 0,
#endif
};

// 向 WS2812 状态机写入一个像素，数据格式为 GRB。
static void ws2812_put_pixel(uint32_t grb) {
  pio_sm_put_blocking(kWs2812Pio, kWs2812Sm, grb << 8u);
}

// 将 RGB 转成 WS2812 常用的 GRB 顺序。
static uint32_t ws2812_color(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
}

// 计算 8bit 亮度的渐隐值。
// 这里使用一个简单的二次曲线，让前半段衰减更慢、后半段更快。
static uint8_t fade8(uint64_t age_us, uint64_t duration_us) {
  if (age_us >= duration_us) {
    return 0;
  }

  const uint64_t remaining = duration_us - age_us;
  const uint64_t scaled = remaining * remaining * 255u;
  return (uint8_t)(scaled / (duration_us * duration_us));
}

// 初始化 WS2812 的 PIO、GPIO 和状态机。
static void ws2812_init(void) {
  const int offset = pio_add_program(kWs2812Pio, &ws2812_program);
  pio_sm_config c = pio_get_default_sm_config();

  // 这里配置成 1bit sideset + FIFO 发送像素数据。
  sm_config_set_sideset(&c, 1, false, false);
  sm_config_set_sideset_pins(&c, kWs2812Pin);
  sm_config_set_out_pins(&c, kWs2812Pin, 1);
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
  sm_config_set_out_shift(&c, false, true, 24);
  sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / (800000.0f * 10.0f));
  sm_config_set_wrap(&c, offset, offset + 3);

  pio_gpio_init(kWs2812Pio, kWs2812Pin);
  pio_sm_set_consecutive_pindirs(kWs2812Pio, kWs2812Sm, kWs2812Pin, 1, true);
  pio_sm_init(kWs2812Pio, kWs2812Sm, offset, &c);
  pio_sm_set_enabled(kWs2812Pio, kWs2812Sm, true);
}

// 根据触发时间计算当前 LED 亮度，并推送两个像素。
static void ws2812_render(const LedPulseState *leds, uint64_t now) {
  const uint8_t led1_blue = leds[0].blue_on_us ? fade8(now - leds[0].blue_on_us, kLedPulseUs) : 0;
  const uint8_t led1_red = leds[0].red_on_us ? fade8(now - leds[0].red_on_us, kLedPulseUs) : 0;
  const uint8_t led2_blue = leds[1].blue_on_us ? fade8(now - leds[1].blue_on_us, kLedPulseUs) : 0;
  const uint8_t led2_red = leds[1].red_on_us ? fade8(now - leds[1].red_on_us, kLedPulseUs) : 0;

  ws2812_put_pixel(ws2812_color(led1_red, 0, led1_blue));
  ws2812_put_pixel(ws2812_color(led2_red, 0, led2_blue));
  sleep_us(80);
}
#endif

// 主循环：
// 1) 维护 USB HID
// 2) 合并 GPIO + ADC 输入状态
// 3) 根据变化发送 HID 报告
// 4) 更新 WS2812 指示灯
int main() {
  USBInit();
  setAutoSendReport(false);
  tusb_init();
  init_gpio_inputs();

  if (gpio_get(kHatPins[0]) == 0) {
      // 进入 USB Bootloader 模式
      // 参数 0, 0 表示禁用活动 LED 并在闪存中禁用特殊的活动接口
      reset_usb_boot(0, 0);
  }

#if ENABLE_LED_FEEDBACK
  ws2812_init();
#endif
  multicore_launch_core1(adc_core1_entry);

  uint16_t last_buttons = BUTTON_NONE;
  uint8_t last_hat = HAT_CENTER;
  bool sent_initial_state = false;
#if ENABLE_LED_FEEDBACK
  LedPulseState leds[2] = {};
#endif

  while (true) {
    tud_task();

    const uint32_t adc_button_bits = g_adc_button_bits;
    const uint32_t adc_hat_bits = g_adc_hat_bits;
#if ENABLE_LED_FEEDBACK
    const uint64_t now = now_us();
    // 原子交换，避免 core1 正在写入时丢事件。
    const uint32_t adc_events = __atomic_exchange_n((uint32_t *)&g_adc_led_events, 0u, __ATOMIC_SEQ_CST);
#endif

    uint16_t gpio_buttons = BUTTON_NONE;
    uint32_t gpio_hat_bits = 0;
    gpio_core0_scan(&gpio_buttons, &gpio_hat_bits);
#if ENABLE_LED_FEEDBACK
    const uint32_t gpio_events = g_gpio_led_events;
    g_gpio_led_events = 0;

    // 先消费 GPIO 侧 LED 事件。
    if (gpio_events & LED1_BLUE) {
      leds[0].blue_on_us = now;
    }
    if (gpio_events & LED1_RED) {
      leds[0].red_on_us = now;
    }
    if (gpio_events & LED2_BLUE) {
      leds[1].blue_on_us = now;
    }
    if (gpio_events & LED2_RED) {
      leds[1].red_on_us = now;
    }

    // 再消费 ADC 侧 LED 事件。
    if (adc_events & LED1_BLUE) {
      leds[0].blue_on_us = now;
    }
    if (adc_events & LED1_RED) {
      leds[0].red_on_us = now;
    }
    if (adc_events & LED2_BLUE) {
      leds[1].blue_on_us = now;
    }
    if (adc_events & LED2_RED) {
      leds[1].red_on_us = now;
    }
#endif

    const uint16_t current_buttons = gpio_buttons |
        ((adc_button_bits & ADC_BIT_B) ? BUTTON_B : 0) |
        ((adc_button_bits & ADC_BIT_A) ? BUTTON_A : 0);
    const bool right = (gpio_hat_bits & (1u << 0)) != 0;
    const bool down = (gpio_hat_bits & (1u << 1)) != 0 || (adc_hat_bits & ADC_BIT_DOWN) != 0;
    const bool up = (gpio_hat_bits & (1u << 2)) != 0;
    const bool left = (gpio_hat_bits & (1u << 3)) != 0 || (adc_hat_bits & ADC_BIT_LEFT) != 0;
    const uint8_t current_hat = read_hat_from_bits(right, down, up, left);

    // 只有状态变化时才发送 HID 报告，减少总线开销。
    if (!sent_initial_state || current_buttons != last_buttons || current_hat != last_hat) {
      const uint16_t changed = last_buttons ^ current_buttons;

      for (size_t i = 0; i < sizeof(kButtonMasks) / sizeof(kButtonMasks[0]); ++i) {
        const uint16_t button = kButtonMasks[i];
        if ((changed & button) == 0) {
          continue;
        }
        if ((current_buttons & button) != 0) {
          pressButton(button);
        } else {
          releaseButton(button);
        }
      }

      if (current_hat != last_hat) {
        if (current_hat == HAT_CENTER) {
          releaseHatButton();
        } else {
          pressHatButton(current_hat);
        }
      }

      sendReport();
      last_buttons = current_buttons;
      last_hat = current_hat;
      sent_initial_state = true;
    }

#if ENABLE_LED_FEEDBACK
    ws2812_render(leds, now);
#endif

    // 主循环节奏控制，避免忙等过高。
    sleep_us(1000);
  }
}
