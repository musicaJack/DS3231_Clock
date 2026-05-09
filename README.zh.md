# DS3231 模拟表盘（RP2040）

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Raspberry%20Pi%20Pico-red.svg)](https://www.raspberrypi.com/products/raspberry-pi-pico/)
[![Version](https://img.shields.io/badge/Version-3.2.0-green.svg)]

[English](README.md) | 中文
 <img src="imgs/analog-clock.jpg" width="240" />

基于 **树莓派 Pico（RP2040）** 的固件：通过 **DS3231** 实时时钟与 **ILI9488** 3.5 寸屏（320×480，竖屏）显示**模拟表盘**（1–12 数字、刻度圈、时针/分针/秒针），底部水平居中 **`MM/DD`** 与 **英文星期**。支持 **月历界面**（横屏逻辑分辨率 480×320）：黑底白字，RTC **当天**所在日在当前浏览月份内时以反色高亮。显示用日期与时间**仅以 DS3231 为准**，主循环不用本地软件累计代替走时。**USB 串口**可校时，写入成功后保存在 DS3231（带电池模块掉电仍可走时）。**GPIO 按键**可在时钟与月历之间切换，并在月历中翻页月份。

---

## 功能概要

- DS3231 使用 **I2C1**；ILI9488 使用 **SPI0**（默认 40 MHz，见 `pin_config.hpp`）。
- 表盘刷新由 RTC **秒寄存器变化**驱动，而非单纯延时循环。
- **月历**：横屏显示；当日（来自 RTC）仅在**浏览月份包含该日**时高亮。
- **按键**：**GP14** 短按切换时钟 / 月历；月历下 **GP8** 下一月、**GP9** 上一月（默认 **按下为低电平** + 内部上拉，可在 `main.cpp` 修改 `kKeysActiveLow`）。
- USB CDC **非阻塞**按行解析校时字符串。
- 模块边界：`AnalogClockView`、`calendar_month_paint`、`usb_time_sync`、DS3231 API。

---

## 目录结构

| 路径 | 说明 |
|------|------|
| `apps/ds3231_clock/main.cpp` | 应用入口：初始化、主循环、界面模式与按键。 |
| `src/clock/` | 表盘 UI：`AnalogClockView`、`gfx_clock`、月历 `calendar_month_view`。 |
| `src/sync/` | USB 串口校时解析与写 RTC 辅助函数。 |
| `lib/ds3231/` | DS3231 驱动（`ds3231.h`、`ds3231_driver.cpp`）。 |
| `lib/display/` | ILI9488 显示栈（CMake 目标 `display`）。 |

固件目标的包含路径见根目录 `CMakeLists.txt`（`src/clock`、`src/sync`、`lib/ds3231`）。

---

## 硬件接线

### DS3231（I2C1）

| 信号 | GPIO |
|------|------|
| SDA | **GP7** |
| SCL | **GP6** |

默认占用 **I2C1** 与上述引脚；若改线请同步修改 `apps/ds3231_clock/main.cpp` 中的 `DS3231_I2C_INST` / `DS3231_SDA_PIN` / `DS3231_SCL_PIN`。

### ILI9488（SPI0）

引脚定义见 **`lib/display/include/pin_config.hpp`**（与 PicoBoard_Games_ILI9488 参考工程一致）：

| 信号 | GPIO |
|------|------|
| SCK | GP18 |
| MOSI | GP19 |
| CS | GP17 |
| DC | GP20 |
| RST | GP15 |
| BL（背光 PWM） | GP16 |

SPI：**SPI0**，时钟 40 MHz；未使用 MISO（配置为 `255`）。

### 按键（月历界面）

| 功能 | GPIO | 说明 |
|------|------|------|
| 时钟 / 月历切换 | **GP14** | 短按（约 20–900 ms）；与 `kKeysActiveLow` 配合使用内部上/下拉 |
| 月历：上一月 | **GP9** | 与模式键同极性定义 |
| 月历：下一月 | **GP8** | 与模式键同极性定义 |

**说明：** DS3231、SPI 屏与上述 GPIO **无引脚冲突**。

---

## 编译

依赖：

- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
- 环境变量 **`PICO_SDK_PATH`** 指向 SDK 根目录
- 推荐 Ninja，或其它 CMake 生成器

```bash
cd DS3231_Clock
mkdir build && cd build
cmake -G Ninja ..
cmake --build .
```

产物在 `build/` 下：`ds3231_clock.elf`，通常还有 **`ds3231_clock.uf2`**，可拖入 Pico 虚拟 U 盘烧录。

---

## 使用说明

### 设备上操作

1. 上电后从 DS3231 读时并绘制表盘。
2. **GP14** 短按可在**模拟表盘**与**月历**之间切换（月历为横屏）。
3. 月历模式下 **GP8 / GP9** 分别切换**下一月 / 上一月**（浏览月份）。

### USB 校时

1. 打开 USB 虚拟串口（波特率以工程/SDK 配置为准，常见为 115200）。
2. 发送一行并回车：
   - **`hh:mm:ss`** — 仅更新时分秒（实现上会先读 RTC 再写回以保留日期等字段，详见源码）。
   - **`YYYY-MM-DD hh:mm:ss`** 或 **`YYYY-MM-DDThh:mm:ss`** — 写入完整日期时间并计算星期寄存器。

具体格式与 **`usb_sync_poll_line`** 返回值含义见 `src/sync/usb_time_sync.cpp`。

---

## USB 串口输出（启动与日志）

上电约 **800 ms** 后，固件通过 **USB CDC**（与校时同一端口）打印下列信息，便于核对接线与按键映设：

| 输出内容 | 含义 |
|----------|------|
| `usb_sync_print_banner()` 横幅与格式说明 | 当前固件为中文说明；支持的校时字符串格式 |
| `DS3231: I2C1 SDA=GP7 SCL=GP6` | 与 `main.cpp` 中 RTC 引脚定义一致 |
| `ILI9488: SPI0 per lib/display pin_config.hpp` | 显示屏使用 SPI0，细节见 `pin_config.hpp` |
| `Keys: MODE=GP14 PREV=GP9 NEXT=GP8 (active low)` | 按键 GPIO；`PREV`/`NEXT` 表示月历中的**上/下一月** |
| `RTC OSF: set time via USB...` | 仅在振荡器停止标志置位时出现，需校时或检查电池 |
| `DS3231 init failed.` / `ILI9488 init failed.` | 初始化失败，程序停在死循环 |
| `usb_sync_print_prompt()` 提示行 | 可输入校时命令 |

输入校时行并回车后，还会打印 **`usb_sync_poll_line`** 返回成功/失败等反馈（中文提示）。

---

## 二次开发 / API 入口

- **RTC：** `ds3231_init`、`ds3231_read_time`、`ds3231_write_time` 等 — `lib/ds3231/ds3231.h`。
- **显示：** `ili9488::ILI9488Driver` — `lib/display/include/ili9488_driver.hpp`；CMake 链接 **`display`**。
- **表盘：** `AnalogClockView` — `src/clock/analog_clock.hpp`；需已初始化的 `ILI9488Driver`。
- **月历：** `calendar_month_paint` — `src/clock/calendar_month_view.hpp`。
- **USB 校时：** `usb_sync_poll_line`、`rtc_set_datetime` 等 — `src/sync/usb_time_sync.hpp`。

CMake 链接示例：`display`、`pico_stdlib`、`hardware_i2c`、`hardware_spi`、`hardware_pwm`、`hardware_gpio`，以及已启用的 USB stdio 相关依赖。

---

## 许可证

见仓库根目录 [LICENSE](LICENSE)。
