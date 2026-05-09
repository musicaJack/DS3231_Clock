#pragma once

#include <cstdint>

#include "ds3231.h"

namespace ili9488 {
class ILI9488Driver;
}

/**
 * 320x480 portrait analog dial: silver ticks, 1-12 numerals, white/silver hands, red second hand.
 * Bottom-centered MM/DD + English weekday at 2x scale; numerals at 2x. Calendar overlay redraws only when date text
 * changes to avoid flicker. Minute hand steps once per minute by default (see analog_clock.cpp).
 */
class AnalogClockView {
public:
    explicit AnalogClockView(ili9488::ILI9488Driver& driver);

    void paint_static_dial();

    void on_second_tick(const ds3231_time_t& t);

    void force_hands_sync(const ds3231_time_t& t);

    /** When date or weekday label changes: refresh cache and force_hands_sync; bottom strip redraws then */
    void refresh_calendar_ui(const ds3231_time_t& t);

private:
    void erase_hand(int x0, int y0, int x1, int y1, int thick);
    void draw_hand(int x0, int y0, int x1, int y1, int thick, uint16_t color);
    void redraw_all_ticks();
    void draw_tick_index(int k);
    void draw_center_cap(uint16_t color);
    void tip_from_time(float angle_deg_from_12cw, int length, int* tx, int* ty) const;

    void draw_hour_numerals();
    /** Erase band + MM/DD + weekday; called from draw_all_hands_at when calendar_overlay_dirty_ */
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
    /** Last drawn second-hand tick index; used to restore one tick erased by the second hand */
    uint8_t last_drawn_sec_{0};

    int h_th_{7}, m_th_{5}, s_th_{2};
    int h_len_{50}, m_len_{92}, s_len_{112};

    bool calendar_valid_{false};
    /** When true, draw_all_hands_at draws bottom calendar strip then clears this flag */
    bool calendar_overlay_dirty_{false};
    char last_date_shown_[8]{};
    char last_week_shown_[8]{};
};
