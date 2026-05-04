/**
 * DS3231 + ILI9488 模拟时钟 + USB CDC 串口校时
 * I2C1: SDA=GPIO7, SCL=GPIO6
 * ILI9488: SPI0（pins: lib/display/include/pin_config.hpp）
 *
 * 【时间源】表盘与日期/星期仅依赖 I2C 读回的 DS3231（模块带电池则掉电仍走时）。
 * 主循环内无任何本地「软件时钟」累加秒；秒针刷新节拍由 RTC 的「秒」变化触发。
 * 【校时】USB 解析成功后一律调用 ds3231_write_time 写入芯片（hh:mm:ss 与 YYYY-MM-DD hh:mm:ss）。
 */

#include "hardware/i2c.h"
#include "pico/stdlib.h"

#include <cstdio>

#include "analog_clock.hpp"
#include "ds3231.h"
#include "ili9488_driver.hpp"
#include "pin_config.hpp"
#include "usb_time_sync.hpp"

#define DS3231_I2C_INST i2c1
#define DS3231_SDA_PIN  7u
#define DS3231_SCL_PIN  6u

static ds3231_t g_rtc;

int main() {
    stdio_init_all();
    sleep_ms(800);

    usb_sync_print_banner();
    printf("DS3231: I2C1 SDA=GP%u SCL=GP%u\r\n", (unsigned)DS3231_SDA_PIN, (unsigned)DS3231_SCL_PIN);
    printf("ILI9488: SPI0 per lib/display pin_config.hpp\r\n");

    if (!ds3231_init(&g_rtc, DS3231_I2C_INST, DS3231_SDA_PIN, DS3231_SCL_PIN)) {
        printf("DS3231 init failed.\r\n");
        while (true) {
            tight_loop_contents();
        }
    }

    bool osc_stop = false;
    if (ds3231_is_oscillator_stopped(&g_rtc, &osc_stop) && osc_stop) {
        printf("RTC OSF: set time via USB (hh:mm:ss or YYYY-MM-DD hh:mm:ss).\r\n");
    }

    ili9488::ILI9488Driver lcd(ILI9488_GET_SPI_CONFIG());
    if (!lcd.initialize()) {
        printf("ILI9488 init failed.\r\n");
        while (true) {
            tight_loop_contents();
        }
    }

    lcd.setRotation(ili9488::Rotation::Portrait_180);
    lcd.setBacklightBrightness(220);

    AnalogClockView clock_view(lcd);
    clock_view.paint_static_dial();

    usb_sync_print_prompt();

    ds3231_time_t rtc{};
    uint8_t last_render_sec = 255;

    for (;;) {
        const int usb_r = usb_sync_poll_line(&g_rtc, nullptr);

        if (!ds3231_read_time(&g_rtc, &rtc)) {
            sleep_ms(40);
            continue;
        }
        clock_view.refresh_calendar_ui(rtc);

        if (usb_r == 1) {
            clock_view.force_hands_sync(rtc);
            last_render_sec = rtc.seconds;
        } else if (rtc.seconds != last_render_sec) {
            clock_view.on_second_tick(rtc);
            last_render_sec = rtc.seconds;
        }

        /* 不在此循环里周期性 printf：会与 USB 校时输入抢占终端，导致无法输入 */

        sleep_ms(40);
    }
}
