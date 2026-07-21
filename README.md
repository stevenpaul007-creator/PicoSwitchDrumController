# RP2040SwitchControl

这是一个面向 **RP2040 + PlatformIO + TinyUSB HID** 的 Switch 控制器工程。

## 特性

- 保留了原工程的 `SwitchControl` 报告结构
- 走 **USB HID**，不使用 USB CDC 串口
- `src/main.cpp` 中实现了 GPIO / ADC 输入汇总
- ADC 采样放在 **core1**，USB / GPIO / LED 逻辑放在 **core0**
- GPIO 和 ADC 都做了去抖；ADC 额外做了滞回
- GPIO0 输出 WS2812B 指示灯，按键触发后亮约 100ms 并渐隐

## 编译

```bash
pio run
```

烧录：

```bash
pio run -t upload
```

默认环境是 `env:pico`。

## 按键映射

### GPIO1-GPIO15

- GPIO1 -> ZR
- GPIO2 -> R
- GPIO3 -> B
- GPIO4 -> A
- GPIO5 -> Y
- GPIO6 -> X
- GPIO7 -> PLUS
- GPIO8 -> HOME
- GPIO9 -> MINUS
- GPIO10 -> RIGHT
- GPIO11 -> DOWN
- GPIO12 -> UP
- GPIO13 -> LEFT
- GPIO14 -> L
- GPIO15 -> ZL

### ADC 映射

- GPIO26 / ADC1 -> B
- GPIO27 / ADC2 -> A
- GPIO28 / ADC3 -> DOWN
- GPIO29 / ADC4 -> LEFT

阈值定义在 `src/main.cpp` 的 `kAdcThresholds[]` 中，你可以直接改成自己的 4 个阈值。

## WS2812B 指示灯

GPIO0 驱动 2 颗 WS2812B：

- 第 1 个 LED
  - A / X -> 蓝色
  - B / Y -> 红色
- 第 2 个 LED
  - UP / LEFT -> 蓝色
  - DOWN / RIGHT -> 红色

每次对应按钮或 ADC 触发时，相关颜色会亮起约 100ms，并做渐慢熄灭。

## 说明

- ADC 与 GPIO 的同名按键是 **OR** 关系，不冲突
- 所有一次性按键触发都会至少保持按下 10ms 再释放
- ADC 触发采用“上升沿 + 去抖 + 滞回”
- WS2812B 逻辑使用 PIO 驱动
