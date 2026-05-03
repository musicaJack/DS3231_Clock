# DS3231_Clock

RP2040 + **DS3231** + **ILI9488** 3.5″（320×480）模拟表盘；顶/底显示日期与星期。**时间与日期仅以 DS3231 为准**（模块装电池则掉电仍走时）；**USB 校时成功后会写入 DS3231**，掉电后仍保持。

## 硬件接线

| 功能 | 接口 | 引脚 |
|------|------|------|
| DS3231 | I2C1 | SDA = **GPIO7**，SCL = **GPIO6** |
| ILI9488 | SPI0 | SCK=18, MOSI=19, CS=17, DC=20, RST=15, BL=16 |

详见 `lib/display/include/pin_config.hpp`（与 PicoBoard_Games_ILI9488 一致）。**DS3231 占用 GP6/GP7**，与 SPI 屏无冲突。

## 构建

需安装 [Pico SDK](https://github.com/raspberrypi/pico-sdk)，并设置环境变量 `PICO_SDK_PATH`。

```bash
mkdir build && cd build
cmake -G Ninja ..
cmake --build .
```

生成 `ds3231_clock.uf2`，拖入 Pico 烧录。

## 使用

1. 上电后从 **DS3231** 读时显示；USB 串口可输入 **`hh:mm:ss`** 或 **`YYYY-MM-DD hh:mm:ss`**（例 `2026-05-03 14:30:00`），回车后写入 **DS3231 芯片**。
2. 指针与日期刷新节律由 RTC 的「秒」变化驱动；**不使用 RP2040 本地软件计时替代 RTC**。

## 源码说明

- `lib/ds3231/`：RTC 驱动  
- `lib/display/`：ILI9488 驱动（源自游戏演示工程，已精简 CMake，去掉摇杆/可选字体库）  
- `analog_clock.*`：表盘绘制  
- `usb_time_sync.*`：USB 行编辑与校时  
- `gfx_clock.*`：线段/圆等绘图封装  
