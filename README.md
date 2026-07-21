# RP2040SwitchControl

这是一个面向 **RP2040 + PlatformIO + Earle Philhower Arduino core + TinyUSB HID** 的 Switch 控制器工程。

## 特性

- 保留了原工程的 `SwitchControl` 报告结构
- 走 **USB HID**，不使用 USB CDC 串口
- `src/main.cpp` 中实现了 GPIO / ADC 输入汇总
- ADC 采样放在 **core1**，USB / GPIO / LED 逻辑放在 **core0**
- GPIO 做去抖；ADC 采用“阈值上升沿 + 短脉冲 + 冷却时间”触发
- ADC 支持 900 暴击阈值，暴击时会触发组合键
- GPIO0 输出 WS2812B 指示灯，按键触发后亮约 200ms 并渐隐

## PlatformIO 配置

默认环境是 `env:pico`，当前使用 Earle Philhower Arduino core：

```ini
platform = https://github.com/maxgerhardt/platform-raspberrypi.git#651837d09a1a58c46bd1bcf4647468d8c5ed92c2
board = pico
framework = arduino
board_build.core = earlephilhower
```

工程自己提供 TinyUSB HID 描述符并在代码中调用 `tusb_init()`，因此 `platformio.ini` 中使用 `-DNO_USB` 避免 Arduino core 自带 USB 描述符与本工程的 `tud_descriptor_*` 回调冲突。

## 编译

```bash
pio run
```

烧录：

```bash
pio run -t upload
```

构建产物会生成在 `.pio/build/pico/`，常用固件文件是 `firmware.uf2`。

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

普通触发阈值定义在 `src/main.cpp` 的 `kAdcThresholds[]` 中，当前默认值：

```cpp
{600, 600, 600, 600}
```

暴击阈值定义在 `kAdcCriticalThresholds[]` 中，当前默认值：

```cpp
{900, 900, 900, 900}
```

ADC 触发采用固定短脉冲，当前按下保持时间为 30ms，同一通道冷却时间为 1ms。

### ADC 暴击组合键

当任意 ADC 通道采样达到 900 时，认为该次触发是“暴击”：

- B 或 DOWN 其中一个暴击：同时按下 `B + DOWN`
- A 或 LEFT 其中一个暴击：同时按下 `A + LEFT`

也就是说：

- GPIO26 / ADC1 / B 暴击 -> `B + DOWN`
- GPIO28 / ADC3 / DOWN 暴击 -> `B + DOWN`
- GPIO27 / ADC2 / A 暴击 -> `A + LEFT`
- GPIO29 / ADC4 / LEFT 暴击 -> `A + LEFT`

## WS2812B 指示灯

GPIO0 驱动 2 颗 WS2812B：

- 第 1 个 LED
  - A / X -> 蓝色
  - B / Y -> 红色
- 第 2 个 LED
  - UP / LEFT -> 蓝色
  - DOWN / RIGHT -> 红色

每次对应按钮或 ADC 触发时，相关颜色会亮起约 200ms，并做渐慢熄灭。

## 说明

- ADC 与 GPIO 的同名按键是 **OR** 关系，不冲突
- ADC 普通触发和暴击触发分别记录上升沿状态，普通触发不会阻止暴击组合键触发
- ADC 的普通键和暴击组合键在每轮 core1 采样后统一汇总，避免多个 ADC 通道互相清除状态
- WS2812B 逻辑使用 PIO 驱动
