#include "usb_time_sync.hpp"
#include "pico/stdlib.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static char s_buf[48];
static int s_len;

static void trim_inplace(char* str) {
    if (!str || !*str)
        return;
    char* p = str;
    while (*p == ' ' || *p == '\t')
        p++;
    if (p != str) {
        std::memmove(str, p, std::strlen(p) + 1);
    }
    std::size_t n = std::strlen(str);
    while (n > 0 && (str[n - 1] == ' ' || str[n - 1] == '\t')) {
        str[n - 1] = '\0';
        n--;
    }
}

/** Sakamoto：返回值 0=Sunday … 6=Saturday（与 DS3231 的「星期」换算一致：+1 得 1=Sunday） */
static int weekday_sun0_sat6(int y, int m, int d) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) {
        y -= 1;
    }
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

static bool is_valid_calendar(unsigned y, unsigned mo, unsigned d) {
    if (mo < 1u || mo > 12u || d < 1u || d > 31u)
        return false;
    static const unsigned md[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    unsigned maxd = md[mo - 1];
    if (mo == 2u) {
        const bool leap = ((y % 4u == 0u) && (y % 100u != 0u)) || (y % 400u == 0u);
        maxd = leap ? 29u : 28u;
    }
    return d <= maxd;
}

static uint8_t ds3231_weekday_from_date(unsigned full_year, unsigned month, unsigned date) {
    int w = weekday_sun0_sat6(static_cast<int>(full_year), static_cast<int>(month), static_cast<int>(date));
    if (w < 0)
        w += 7;
    /* DS3231：1–7，1=Sunday */
    return static_cast<uint8_t>(static_cast<unsigned>(w) + 1u);
}

bool rtc_set_datetime(ds3231_t* rtc, unsigned year_full, unsigned month, unsigned date, uint8_t h, uint8_t m,
                      uint8_t s) {
    if (!rtc || year_full < 2000u || year_full > 2099u)
        return false;
    if (!is_valid_calendar(year_full, month, date))
        return false;
    if (h > 23u || m > 59u || s > 59u)
        return false;

    ds3231_time_t t{};
    t.seconds = s;
    t.minutes = m;
    t.hours = h;
    t.date = static_cast<uint8_t>(date);
    t.month = static_cast<uint8_t>(month);
    t.year = static_cast<uint8_t>(year_full % 100u);
    t.day = ds3231_weekday_from_date(year_full, month, date);
    return ds3231_write_time(rtc, &t);
}

void usb_sync_print_prompt(void) {
    printf("\r\n请输入（回车写入 DS3231）: ");
    fflush(stdout);
}

void usb_sync_print_banner(void) {
    printf("\r\n=== DS3231 USB 串口校时 ===\r\n");
    printf("支持格式:\r\n");
    printf("  1) hh:mm:ss              例 14:30:00（只改时分秒，日期不变）\r\n");
    printf("  2) yyyy-mm-dd hh:mm:ss   例 2026-05-03 14:30:00（完整日期时间）\r\n");
    printf("     亦可 yyyy-mm-ddThh:mm:ss（T 分隔日期与时间）\r\n");
}

static bool parse_hms(const char* str, uint8_t* h, uint8_t* m, uint8_t* s) {
    int hh, mm, ss;
    if (std::sscanf(str, "%d:%d:%d", &hh, &mm, &ss) != 3)
        return false;
    if (hh < 0 || hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 59)
        return false;
    *h = static_cast<uint8_t>(hh);
    *m = static_cast<uint8_t>(mm);
    *s = static_cast<uint8_t>(ss);
    return true;
}

static bool parse_ymd_hms(const char* str, unsigned* y_full, unsigned* mo, unsigned* d, uint8_t* h, uint8_t* m,
                          uint8_t* s) {
    int Y, M, D, hh, mm, ss;
    /* yyyy-mm-dd hh:mm:ss 或 yyyy-mm-ddThh:mm:ss（ISO 常用） */
    int n = std::sscanf(str, "%d-%d-%d %d:%d:%d", &Y, &M, &D, &hh, &mm, &ss);
    if (n != 6)
        n = std::sscanf(str, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &hh, &mm, &ss);
    if (n != 6)
        return false;
    if (Y < 2000 || Y > 2099 || M < 1 || M > 12 || D < 1 || D > 31)
        return false;
    if (hh < 0 || hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 59)
        return false;
    if (!is_valid_calendar(static_cast<unsigned>(Y), static_cast<unsigned>(M), static_cast<unsigned>(D)))
        return false;
    *y_full = static_cast<unsigned>(Y);
    *mo = static_cast<unsigned>(M);
    *d = static_cast<unsigned>(D);
    *h = static_cast<uint8_t>(hh);
    *m = static_cast<uint8_t>(mm);
    *s = static_cast<uint8_t>(ss);
    return true;
}

bool rtc_set_time_of_day(ds3231_t* rtc, uint8_t h, uint8_t m, uint8_t s) {
    if (!rtc)
        return false;
    ds3231_time_t t;
    if (!ds3231_read_time(rtc, &t))
        return false;
    t.hours = h;
    t.minutes = m;
    t.seconds = s;
    return ds3231_write_time(rtc, &t);
}

int usb_sync_poll_line(ds3231_t* rtc, ds3231_time_t* out) {
    int c = getchar_timeout_us(0);
    if (c == PICO_ERROR_TIMEOUT)
        return 0;

    char ch = static_cast<char>(c);

    if (ch == '\n' || ch == '\r') {
        if (s_len <= 0)
            return 0;
        s_buf[s_len] = '\0';
        trim_inplace(s_buf);
        printf("\r\n");

        unsigned yf = 0, mo = 0, dd = 0;
        uint8_t h = 0, m = 0, s = 0;

        if (parse_ymd_hms(s_buf, &yf, &mo, &dd, &h, &m, &s)) {
            if (!rtc_set_datetime(rtc, yf, mo, dd, h, m, s)) {
                s_len = 0;
                printf("[失败] 无法写入 DS3231，请检查日期是否合法（2000–2099）。\r\n");
                usb_sync_print_prompt();
                return -1;
            }
            if (out)
                ds3231_read_time(rtc, out);
            s_len = 0;
            printf("[成功] 已同步 yyyy-mm-dd hh:mm:ss -> %04u-%02u-%02u %02u:%02u:%02u（已写入 DS3231）\r\n", yf, mo, dd,
                   h, m, s);
            usb_sync_print_prompt();
            return 1;
        }

        if (parse_hms(s_buf, &h, &m, &s)) {
            if (!rtc_set_time_of_day(rtc, h, m, s)) {
                s_len = 0;
                printf("[失败] 写入 DS3231 失败（请检查 I2C）。\r\n");
                usb_sync_print_prompt();
                return -1;
            }

            if (out)
                ds3231_read_time(rtc, out);
            s_len = 0;
            printf("[成功] 已同步 hh:mm:ss -> %02u:%02u:%02u（日期未改，已写入 DS3231）\r\n", h, m, s);
            usb_sync_print_prompt();
            return 1;
        }

        s_len = 0;
        printf("[格式错误] 请使用: hh:mm:ss  或  yyyy-mm-dd hh:mm:ss\r\n");
        printf("  例如 14:30:00  或  2026-05-03 14:30:00  或  2026-05-03T14:30:00\r\n");
        usb_sync_print_prompt();
        return -1;
    }

    if (ch == 8 || ch == 127) {
        if (s_len > 0) {
            s_len--;
            s_buf[s_len] = '\0';
            printf("\b \b");
            fflush(stdout);
        }
        return 0;
    }

    if (s_len < static_cast<int>(sizeof(s_buf)) - 1u) {
        s_buf[s_len++] = ch;
        putchar(ch);
        fflush(stdout);
    } else {
        s_len = 0;
    }

    return 0;
}
