#pragma once

#include <cstdint>

#include "ds3231.h"

namespace ili9488 {
class ILI9488Driver;
}

/**
 * 横屏月历（逻辑分辨率 480×320）：黑底白字，RTC「当天」所在格为白底黑字（仅当浏览月份含该日时高亮）。
 */
void calendar_month_paint(ili9488::ILI9488Driver& lcd, unsigned view_year_full, unsigned view_month,
                          const ds3231_time_t& rtc_now);
