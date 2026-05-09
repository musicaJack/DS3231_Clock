#pragma once

#include <cstdint>

#include "ds3231.h"

namespace ili9488 {
class ILI9488Driver;
}

/**
 * Landscape month grid (logical 480x320): black cells and white text; RTC "today" cell is inverted (white/black)
 * when the viewed month contains that calendar day.
 */
void calendar_month_paint(ili9488::ILI9488Driver& lcd, unsigned view_year_full, unsigned view_month,
                          const ds3231_time_t& rtc_now);
