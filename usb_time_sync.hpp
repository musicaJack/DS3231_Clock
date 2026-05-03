#pragma once

#include "ds3231.h"

// USB 串口校对 DS3231（成功则均通过 ds3231_write_time 写入片内 SRAM 映射寄存器，由模块电池保持）：
// - hh:mm:ss — 仅更新时分秒，先读寄存器再写回以保留当日日期
// - yyyy-mm-dd hh:mm:ss 或 yyyy-mm-ddThh:mm:ss — 写入年月日时分秒及推算的星期寄存器

// 非阻塞轮询一行。
// 返回 1=已成功写入 RTC，0=尚无完整行，-1=格式错误
int usb_sync_poll_line(ds3231_t* rtc, ds3231_time_t* out);

bool rtc_set_time_of_day(ds3231_t* rtc, uint8_t h, uint8_t m, uint8_t s);

/** 写入年月日与时分秒，并计算 DS3231 要求的星期寄存器（1=Sunday） */
bool rtc_set_datetime(ds3231_t* rtc, unsigned year_full, unsigned month, unsigned date, uint8_t h,
                      uint8_t m, uint8_t s);

void usb_sync_print_prompt(void);
void usb_sync_print_banner(void);
