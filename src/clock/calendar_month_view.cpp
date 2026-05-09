#include "calendar_month_view.hpp"

#include <cstdio>
#include <cstring>

#include "ili9488_colors.hpp"
#include "ili9488_driver.hpp"

namespace c888 = ili9488_colors::rgb888;

namespace {

unsigned weekday_ds3231_from_gregorian(unsigned y_full, unsigned mo, unsigned d) {
    int y = static_cast<int>(y_full);
    int m = static_cast<int>(mo);
    int day = static_cast<int>(d);
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) {
        y -= 1;
    }
    int w = (y + y / 4 - y / 100 + y / 400 + t[m - 1] + day) % 7;
    if (w < 0) {
        w += 7;
    }
    return static_cast<unsigned>(w) + 1u;
}

bool is_leap_year(unsigned y) {
    if (y % 400 == 0) return true;
    if (y % 100 == 0) return false;
    return (y % 4) == 0;
}

unsigned days_in_month(unsigned y, unsigned m) {
    static const unsigned dim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m < 1 || m > 12) return 31;
    if (m == 2 && is_leap_year(y)) return 29;
    return dim[m - 1];
}

}  // namespace

void calendar_month_paint(ili9488::ILI9488Driver& lcd, unsigned view_year_full, unsigned view_month,
                          const ds3231_time_t& rtc_now) {
    const uint16_t W = lcd.getWidth();
    const uint16_t H = lcd.getHeight();

    const unsigned rtc_y_full = 2000u + static_cast<unsigned>(rtc_now.year % 100);
    const unsigned rtc_m = rtc_now.month;
    const unsigned rtc_d = rtc_now.date;

    lcd.fillScreen(ili9488_colors::rgb565::BLACK);

    constexpr unsigned kTitleScale = 2u;
    constexpr unsigned kDowScale = 2u;
    constexpr unsigned kDayScale = 2u;
    constexpr int kCharW = 8;
    constexpr int title_h = static_cast<int>(16u * kTitleScale);
    constexpr int dow_h = static_cast<int>(16u * kDowScale);

    char title[20];
    std::snprintf(title, sizeof(title), "%04u-%02u", view_year_full, view_month);
    const int title_w = static_cast<int>(std::strlen(title)) * kCharW * static_cast<int>(kTitleScale);
    const int title_x = (static_cast<int>(W) - title_w) / 2;
    if (title_x >= 0) {
        lcd.drawStringScaled(static_cast<uint16_t>(title_x), 6, title, c888::WHITE, c888::BLACK, kTitleScale);
    }

    static const char* dow_labels[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const int header_top = 6 + title_h + 4;
    const int cell_w = static_cast<int>(W) / 7;
    for (int c = 0; c < 7; ++c) {
        const char* lab = dow_labels[c];
        const int lw = static_cast<int>(std::strlen(lab)) * kCharW * static_cast<int>(kDowScale);
        const int lx = c * cell_w + (cell_w - lw) / 2;
        const int ly = header_top;
        if (lx >= 0 && ly >= 0) {
            lcd.drawStringScaled(static_cast<uint16_t>(lx), static_cast<uint16_t>(ly), lab, c888::WHITE, c888::BLACK,
                                 kDowScale);
        }
    }

    const int grid_top = header_top + dow_h + 6;
    const int grid_h = static_cast<int>(H) - grid_top;
    const int cell_h = grid_h / 6;

    const unsigned dim = days_in_month(view_year_full, view_month);
    const unsigned wd1 = weekday_ds3231_from_gregorian(view_year_full, view_month, 1);
    const unsigned start_skip = (wd1 >= 1u && wd1 <= 7u) ? (wd1 - 1u) : 0u;

    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 7; ++col) {
            const int idx = row * 7 + col;
            const int day_num = idx - static_cast<int>(start_skip) + 1;

            const int x0 = col * cell_w;
            const int x1 = (col == 6) ? (static_cast<int>(W) - 1) : (x0 + cell_w - 1);
            const int y0 = grid_top + row * cell_h;
            const int y1 = (row == 5) ? (static_cast<int>(H) - 1) : (y0 + cell_h - 1);

            if (day_num < 1 || static_cast<unsigned>(day_num) > dim) {
                lcd.fillArea(static_cast<uint16_t>(x0), static_cast<uint16_t>(y0), static_cast<uint16_t>(x1),
                             static_cast<uint16_t>(y1), ili9488_colors::rgb565::BLACK);
                continue;
            }

            const unsigned du = static_cast<unsigned>(day_num);
            const bool today =
                (view_year_full == rtc_y_full && view_month == rtc_m && du == rtc_d);
            const uint32_t fg = today ? c888::BLACK : c888::WHITE;
            const uint32_t bg = today ? c888::WHITE : c888::BLACK;
            const uint16_t bg565 = ili9488_colors::rgb888_to_rgb565(bg);

            lcd.fillArea(static_cast<uint16_t>(x0), static_cast<uint16_t>(y0), static_cast<uint16_t>(x1),
                         static_cast<uint16_t>(y1), bg565);

            char day_buf[8];
            std::snprintf(day_buf, sizeof(day_buf), "%u", du);
            const int dw = static_cast<int>(std::strlen(day_buf)) * kCharW * static_cast<int>(kDayScale);
            const int dh = static_cast<int>(16u * kDayScale);
            const int tx = x0 + (x1 - x0 + 1 - dw) / 2;
            const int ty = y0 + (y1 - y0 + 1 - dh) / 2;
            if (tx >= 0 && ty >= 0) {
                lcd.drawStringScaled(static_cast<uint16_t>(tx), static_cast<uint16_t>(ty), day_buf, fg, bg, kDayScale);
            }
        }
    }
}
