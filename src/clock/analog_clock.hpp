#pragma once

#include <cstdint>

#include "ds3231.h"

namespace ili9488 {
class ILI9488Driver;
}

/**
 * 320×480：黑底模拟表盘（银白刻度、1–12 数字、白/银指针、红秒针）。
 * 底部水平居中：MM/DD + 星期，2× 字；1–12 为 2× 字。日历 overlay 仅在内容变化时绘制，避免秒级闪烁。
 * 默认分针按分钟跳步（见 analog_clock.cpp）。刻度：秒走补单格；时/分针动时整圈补刻度。
 */
class AnalogClockView {
public:
    explicit AnalogClockView(ili9488::ILI9488Driver& driver);

    void paint_static_dial();

    void on_second_tick(const ds3231_time_t& t);

    void force_hands_sync(const ds3231_time_t& t);

    /** 日期/星期变化时更新缓存并 force_hands_sync；底部日历仅此时重绘 */
    void refresh_calendar_ui(const ds3231_time_t& t);

private:
    void erase_hand(int x0, int y0, int x1, int y1, int thick);
    void draw_hand(int x0, int y0, int x1, int y1, int thick, uint16_t color);
    void redraw_all_ticks();
    void draw_tick_index(int k);
    void draw_center_cap(uint16_t color);
    void tip_from_time(float angle_deg_from_12cw, int length, int* tx, int* ty) const;

    void draw_hour_numerals();
    /** 底部居中铺底 + MM/DD 与星期；由 draw_all_hands_at 在 calendar_overlay_dirty_ 时调用 */
    void paint_calendar_overlay();
    void draw_all_hands_at(const ds3231_time_t& t);

    ili9488::ILI9488Driver* drv_;

    int cx_{160};
    int cy_{218};
    int radius_{128};

    uint16_t col_face_;
    uint16_t col_ring_;
    uint16_t col_tick_minor_;
    uint16_t col_tick_major_;
    uint16_t col_hand_hour_;
    uint16_t col_hand_minute_;
    uint16_t col_hand_second_;
    uint16_t col_hub_;

    bool hands_ok_{false};
    int h_x1_{0}, h_y1_{0}, m_x1_{0}, m_y1_{0}, s_x1_{0}, s_y1_{0};
    /** 上一帧已绘制的「秒针所指刻度」索引，用于补回被秒针擦掉的单根刻度 */
    uint8_t last_drawn_sec_{0};

    int h_th_{7}, m_th_{5}, s_th_{2};
    int h_len_{50}, m_len_{92}, s_len_{112};

    bool calendar_valid_{false};
    /** 为 true 时 draw_all_hands_at 末尾绘制底部日历并清回 false */
    bool calendar_overlay_dirty_{false};
    char last_date_shown_[8]{};
    char last_week_shown_[8]{};
};
