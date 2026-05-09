#pragma once

#include "ds3231.h"

// USB line parser for DS3231 (successful writes use ds3231_write_time; battery-backed RTC retains values):
// - hh:mm:ss — time only; reads RTC first then writes back to preserve date fields
// - yyyy-mm-dd hh:mm:ss or yyyy-mm-ddThh:mm:ss — full date/time plus computed weekday register

// Non-blocking: collects one line from stdin.
// Returns 1 = RTC written OK, 0 = no complete line yet, -1 = parse/write error
int usb_sync_poll_line(ds3231_t* rtc, ds3231_time_t* out);

bool rtc_set_time_of_day(ds3231_t* rtc, uint8_t h, uint8_t m, uint8_t s);

/** Full date/time with DS3231 weekday register (1 = Sunday) */
bool rtc_set_datetime(ds3231_t* rtc, unsigned year_full, unsigned month, unsigned date, uint8_t h,
                      uint8_t m, uint8_t s);

void usb_sync_print_prompt(void);
void usb_sync_print_banner(void);
