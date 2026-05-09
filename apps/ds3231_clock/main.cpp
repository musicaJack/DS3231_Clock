/**
 * DS3231 + ILI9488 模拟时钟 + USB CDC 串口校时
 * I2C1: SDA=GPIO7, SCL=GPIO6
 * ILI9488: SPI0（pins: lib/display/include/pin_config.hpp）
 *
 * 【时间源】表盘与日期/星期仅依赖 I2C 读回的 DS3231（模块带电池则掉电仍走时）。
 * 主循环内无任何本地「软件时钟」累加秒；秒针刷新节拍由 RTC 的「秒」变化触发。
 * 【校时】USB 解析成功后一律调用 ds3231_write_time 写入芯片（hh:mm:ss 与 YYYY-MM-DD hh:mm:ss）。
 *
 * 【按键】GP14 短按：时钟 / 月历切换；月历下 GP8 下月、GP9 上月。
 * 默认假定按键按下为低电平（内部上拉）；若相反请将 kKeysActiveLow 改为 false。
 */

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include <cstdio>

#include "analog_clock.hpp"
#include "calendar_month_view.hpp"
#include "ds3231.h"
#include "ili9488_driver.hpp"
#include "pin_config.hpp"
#include "usb_time_sync.hpp"

#define DS3231_I2C_INST i2c1
#define DS3231_SDA_PIN  7u
#define DS3231_SCL_PIN  6u

/** 模式切换 */
static constexpr unsigned PIN_KEY_MODE = 14u;
/** 月历：上一月 / 下一月（与硬件 GP8/GP9 对调后：GP8=下一月，GP9=上一月） */
static constexpr unsigned PIN_KEY_PREV_MONTH = 9u;
static constexpr unsigned PIN_KEY_NEXT_MONTH = 8u;

/**
 * true：按下接地为低电平（常见接法）；
 * false：按下为高电平。
 */
static constexpr bool kKeysActiveLow = true;

static ds3231_t g_rtc;

enum class UiMode : uint8_t { Clock, Calendar };

static void setup_keys() {
    gpio_init(PIN_KEY_MODE);
    gpio_init(PIN_KEY_PREV_MONTH);
    gpio_init(PIN_KEY_NEXT_MONTH);
    gpio_set_dir(PIN_KEY_MODE, GPIO_IN);
    gpio_set_dir(PIN_KEY_PREV_MONTH, GPIO_IN);
    gpio_set_dir(PIN_KEY_NEXT_MONTH, GPIO_IN);
    if (kKeysActiveLow) {
        gpio_pull_up(PIN_KEY_MODE);
        gpio_pull_up(PIN_KEY_PREV_MONTH);
        gpio_pull_up(PIN_KEY_NEXT_MONTH);
    } else {
        gpio_pull_down(PIN_KEY_MODE);
        gpio_pull_down(PIN_KEY_PREV_MONTH);
        gpio_pull_down(PIN_KEY_NEXT_MONTH);
    }
}

static inline bool gpio_pressed(unsigned pin) {
    const bool level = gpio_get(pin);
    return kKeysActiveLow ? !level : level;
}

static void calendar_adjust_month(unsigned& view_y_full, unsigned& view_m, int delta) {
    int y = static_cast<int>(view_y_full);
    int m = static_cast<int>(view_m);
    m += delta;
    while (m < 1) {
        m += 12;
        y -= 1;
    }
    while (m > 12) {
        m -= 12;
        y += 1;
    }
    if (y < 2000) {
        y = 2000;
        m = 1;
    }
    if (y > 2099) {
        y = 2099;
        m = 12;
    }
    view_y_full = static_cast<unsigned>(y);
    view_m = static_cast<unsigned>(m);
}

int main() {
    stdio_init_all();
    sleep_ms(800);

    usb_sync_print_banner();
    printf("DS3231: I2C1 SDA=GP%u SCL=GP%u\r\n", (unsigned)DS3231_SDA_PIN, (unsigned)DS3231_SCL_PIN);
    printf("ILI9488: SPI0 per lib/display pin_config.hpp\r\n");
    printf("Keys: MODE=GP%u PREV=GP%u NEXT=GP%u (%s)\r\n", PIN_KEY_MODE, PIN_KEY_PREV_MONTH, PIN_KEY_NEXT_MONTH,
           kKeysActiveLow ? "active low" : "active high");

    setup_keys();

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

    constexpr ili9488::Rotation kRotationClock = ili9488::Rotation::Portrait_180;
    /** 月历横屏：相对竖屏旋转 90°（与原先 Landscape_90 相比为反向，现为 Landscape_270） */
    constexpr ili9488::Rotation kRotationCalendar = ili9488::Rotation::Landscape_270;

    lcd.setRotation(kRotationClock);
    lcd.setBacklightBrightness(220);

    AnalogClockView clock_view(lcd);
    clock_view.paint_static_dial();

    usb_sync_print_prompt();

    UiMode mode = UiMode::Clock;
    unsigned view_year_full = 2000u;
    unsigned view_month = 1u;

    bool prev_mode_key = false;
    uint32_t mode_key_press_ms = 0;

    bool prev_prev_key = false;
    bool prev_next_key = false;
    uint32_t last_nav_ms = 0;

    bool cal_snap_valid = false;
    uint8_t cal_snap_y = 0;
    uint8_t cal_snap_m = 0;
    uint8_t cal_snap_d = 0;

    ds3231_time_t rtc{};
    uint8_t last_render_sec = 255;

    for (;;) {
        const int usb_r = usb_sync_poll_line(&g_rtc, nullptr);
        const uint32_t now_ms = to_ms_since_boot(get_absolute_time());

        if (!ds3231_read_time(&g_rtc, &rtc)) {
            sleep_ms(40);
            continue;
        }

        const bool mode_key = gpio_pressed(PIN_KEY_MODE);
        if (mode_key && !prev_mode_key) {
            mode_key_press_ms = now_ms;
        }
        if (!mode_key && prev_mode_key) {
            const uint32_t dur = now_ms - mode_key_press_ms;
            if (dur >= 20 && dur < 900) {
                if (mode == UiMode::Clock) {
                    mode = UiMode::Calendar;
                    view_year_full = 2000u + static_cast<unsigned>(rtc.year % 100);
                    view_month = rtc.month;
                    lcd.setRotation(kRotationCalendar);
                    calendar_month_paint(lcd, view_year_full, view_month, rtc);
                    cal_snap_valid = true;
                    cal_snap_y = rtc.year;
                    cal_snap_m = rtc.month;
                    cal_snap_d = rtc.date;
                } else {
                    mode = UiMode::Clock;
                    lcd.setRotation(kRotationClock);
                    clock_view.paint_static_dial();
                    clock_view.force_hands_sync(rtc);
                    last_render_sec = rtc.seconds;
                    cal_snap_valid = false;
                }
            }
        }
        prev_mode_key = mode_key;

        if (mode == UiMode::Calendar) {
            if (gpio_pressed(PIN_KEY_PREV_MONTH) && !prev_prev_key &&
                (now_ms - last_nav_ms > 280u)) {
                calendar_adjust_month(view_year_full, view_month, -1);
                calendar_month_paint(lcd, view_year_full, view_month, rtc);
                last_nav_ms = now_ms;
            }
            if (gpio_pressed(PIN_KEY_NEXT_MONTH) && !prev_next_key &&
                (now_ms - last_nav_ms > 280u)) {
                calendar_adjust_month(view_year_full, view_month, 1);
                calendar_month_paint(lcd, view_year_full, view_month, rtc);
                last_nav_ms = now_ms;
            }
            prev_prev_key = gpio_pressed(PIN_KEY_PREV_MONTH);
            prev_next_key = gpio_pressed(PIN_KEY_NEXT_MONTH);

            if (cal_snap_valid &&
                (rtc.year != cal_snap_y || rtc.month != cal_snap_m || rtc.date != cal_snap_d)) {
                cal_snap_y = rtc.year;
                cal_snap_m = rtc.month;
                cal_snap_d = rtc.date;
                calendar_month_paint(lcd, view_year_full, view_month, rtc);
            }
        } else {
            prev_prev_key = gpio_pressed(PIN_KEY_PREV_MONTH);
            prev_next_key = gpio_pressed(PIN_KEY_NEXT_MONTH);
        }

        if (mode == UiMode::Clock) {
            clock_view.refresh_calendar_ui(rtc);
        }

        if (usb_r == 1) {
            if (mode == UiMode::Clock) {
                clock_view.force_hands_sync(rtc);
                last_render_sec = rtc.seconds;
            } else {
                calendar_month_paint(lcd, view_year_full, view_month, rtc);
                cal_snap_y = rtc.year;
                cal_snap_m = rtc.month;
                cal_snap_d = rtc.date;
                cal_snap_valid = true;
            }
        } else if (mode == UiMode::Clock && rtc.seconds != last_render_sec) {
            clock_view.on_second_tick(rtc);
            last_render_sec = rtc.seconds;
        }

        sleep_ms(40);
    }
}
