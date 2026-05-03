#pragma once

#include <cstdint>

#include "ds3231.h"

namespace ili9488 {
class ILI9488Driver;
}

/**
 * 320×480：纯黑底模拟表盘；顶部居中 YYYY-MM-DD，底部居中星期缩写（Mon…）。
 * 刷新：RTC 每秒调用一次；秒针必擦除重画。
 * 时针/分针：针尖像素变化时才擦除旧针；但擦秒针的黑线可能盖住未移动的时针/分针，故每秒在画新秒针之前用当前角度叠画时针与分针（不重擦）以消除残影。
 * 刻度：仅秒走时补旧秒位一根；时针或分针有位移时整圈补刻度。
 */
class AnalogClockView {
public:
    explicit AnalogClockView(ili9488::ILI9488Driver& driver);

    void paint_static_dial();

    void on_second_tick(const ds3231_time_t& t);

    void force_hands_sync(const ds3231_time_t& t);

    /** 日期/星期变化时刷新顶、底文字（水平居中） */
    void refresh_calendar_ui(const ds3231_time_t& t);

private:
    void erase_hand(int x0, int y0, int x1, int y1, int thick);
    void draw_hand(int x0, int y0, int x1, int y1, int thick, uint16_t color);
    void redraw_all_ticks();
    void draw_tick_index(int k);
    void draw_center_cap(uint16_t color);
    void tip_from_time(float angle_deg_from_12cw, int length, int* tx, int* ty) const;

    void draw_all_hands_at(const ds3231_time_t& t);

    ili9488::ILI9488Driver* drv_;

    int cx_{160};
    int cy_{215};
    int radius_{136};

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

    int h_th_{8}, m_th_{5}, s_th_{2};
    int h_len_{52}, m_len_{96}, s_len_{118};

    bool calendar_valid_{false};
    char last_date_shown_[16]{};
    char last_week_shown_[8]{};
};
