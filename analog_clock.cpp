#include "analog_clock.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "gfx_clock.hpp"
#include "ili9488_colors.hpp"
#include "ili9488_driver.hpp"

namespace c565 = ili9488_colors::rgb565;
namespace c888 = ili9488_colors::rgb888;

namespace {
/** 顶/底栏日期、星期使用 2× 点阵（8×16 -> 16×32 像素/字） */
constexpr unsigned kCalFontScale = 2u;
constexpr int kFontCellW = 8 * static_cast<int>(kCalFontScale);
constexpr int kFontCellH = 16 * static_cast<int>(kCalFontScale);
constexpr int kStripH = 40;
constexpr int kTopTextY = (kStripH - kFontCellH) / 2;
constexpr int kBottomStripY = 480 - kStripH;
constexpr int kBottomTextY = kBottomStripY + (kStripH - kFontCellH) / 2;

/** 与 usb_time_sync 中相同的 Sakamoto 公式；返回 1=Sunday … 7=Saturday（与 DS3231 星期寄存器约定一致） */
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
}  // namespace

/**
 * RGB565 表盘配色（黑底上拉开层次，避免全盘纯白）：
 * - 外圈：暗青铜描边，替代刺眼的纯白细圈。
 * - 细刻度：青灰，接近镀铑/银色「铁路轨」分钟圈观感。
 * - 整点大刻度：金黄，模拟镀金或黄铜钉。
 * - 时针：金色；分针：橙色；秒针：红色。
 */
namespace dial_palette {
constexpr uint16_t kFace = c565::BLACK;
/** 外圈细环：深青铜 #5C4033 量级在 RGB565 的近似 */
constexpr uint16_t kRingBronze = 0x5C28U;
/** 细刻度：冷灰蓝（易读但不抢镜） */
constexpr uint16_t kTickMinor = 0x528AU;
/** 整点：金黄 */
constexpr uint16_t kTickMajor = 0xFE20U;
constexpr uint16_t kHandHour = 0xFE20U;
/** 分针：橙色 */
constexpr uint16_t kHandMinute = c565::ORANGE;
/** 秒针：红色 */
constexpr uint16_t kHandSecond = c565::RED;
constexpr uint16_t kHub = 0xFE20U;
}  // namespace dial_palette

AnalogClockView::AnalogClockView(ili9488::ILI9488Driver& driver)
    : drv_(&driver),
      col_face_(dial_palette::kFace),
      col_ring_(dial_palette::kRingBronze),
      col_tick_minor_(dial_palette::kTickMinor),
      col_tick_major_(dial_palette::kTickMajor),
      col_hand_hour_(dial_palette::kHandHour),
      col_hand_minute_(dial_palette::kHandMinute),
      col_hand_second_(dial_palette::kHandSecond),
      col_hub_(dial_palette::kHub) {}

void AnalogClockView::tip_from_time(float angle_deg_from_12cw, int length, int* tx, int* ty) const {
    float rad = angle_deg_from_12cw * (3.14159265f / 180.f);
    *tx = cx_ + static_cast<int>(sinf(rad) * static_cast<float>(length));
    *ty = cy_ - static_cast<int>(cosf(rad) * static_cast<float>(length));
}

void AnalogClockView::erase_hand(int x0, int y0, int x1, int y1, int thick) {
    gfx_clock::draw_line(*drv_, x0, y0, x1, y1, thick, col_face_);
}

void AnalogClockView::draw_hand(int x0, int y0, int x1, int y1, int thick, uint16_t color) {
    gfx_clock::draw_line(*drv_, x0, y0, x1, y1, thick, color);
}

void AnalogClockView::draw_tick_index(int k) {
    k %= 60;
    if (k < 0) k += 60;
    const int r_out = radius_ - 2;
    const int r_in_minor = radius_ - 14;
    float ang = static_cast<float>(k) * 6.f;
    int x0, y0, x1, y1;
    tip_from_time(ang, r_in_minor, &x0, &y0);
    tip_from_time(ang, r_out, &x1, &y1);
    bool major = (k % 5) == 0;
    const uint16_t col = major ? col_tick_major_ : col_tick_minor_;
    gfx_clock::draw_line(*drv_, x0, y0, x1, y1, major ? 4 : 2, col);
}

void AnalogClockView::redraw_all_ticks() {
    for (int k = 0; k < 60; ++k) {
        draw_tick_index(k);
    }
}

void AnalogClockView::draw_center_cap(uint16_t color) {
    gfx_clock::draw_circle(*drv_, cx_, cy_, 9, 7, color);
}

void AnalogClockView::refresh_calendar_ui(const ds3231_time_t& t) {
    unsigned yfull = 2000u + static_cast<unsigned>(t.year % 100);
    char date_buf[16];
    std::snprintf(date_buf, sizeof(date_buf), "%04u-%02u-%02u", yfull, static_cast<unsigned>(t.month),
                  static_cast<unsigned>(t.date));

    static const char* wday[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const unsigned wd = weekday_ds3231_from_gregorian(yfull, static_cast<unsigned>(t.month),
                                                       static_cast<unsigned>(t.date));
    const char* week_buf = (wd >= 1u && wd <= 7u) ? wday[wd - 1u] : "---";

    if (calendar_valid_ && std::strcmp(date_buf, last_date_shown_) == 0 &&
        std::strcmp(week_buf, last_week_shown_) == 0) {
        return;
    }

    std::strncpy(last_date_shown_, date_buf, sizeof(last_date_shown_));
    last_date_shown_[sizeof(last_date_shown_) - 1] = '\0';
    std::strncpy(last_week_shown_, week_buf, sizeof(last_week_shown_));
    last_week_shown_[sizeof(last_week_shown_) - 1] = '\0';
    calendar_valid_ = true;

    gfx_clock::fill_rect(*drv_, 0, 0, 320, kStripH, col_face_);
    {
        const int wpx = static_cast<int>(std::strlen(date_buf)) * kFontCellW;
        int x = (320 - wpx) / 2;
        if (x < 0) x = 0;
        drv_->drawStringScaled(static_cast<uint16_t>(x), static_cast<uint16_t>(kTopTextY), date_buf, c888::YELLOW, c888::BLACK,
                              kCalFontScale);
    }

    gfx_clock::fill_rect(*drv_, 0, kBottomStripY, 320, kStripH, col_face_);
    {
        const int wpx = static_cast<int>(std::strlen(week_buf)) * kFontCellW;
        int x = (320 - wpx) / 2;
        if (x < 0) x = 0;
        drv_->drawStringScaled(static_cast<uint16_t>(x), static_cast<uint16_t>(kBottomTextY), week_buf, c888::YELLOW,
                              c888::BLACK, kCalFontScale);
    }
}

void AnalogClockView::paint_static_dial() {
    drv_->fillScreen(col_face_);

    gfx_clock::draw_circle(*drv_, cx_, cy_, radius_, 3, col_ring_);

    redraw_all_ticks();
    draw_center_cap(col_hub_);

    hands_ok_ = false;
}

void AnalogClockView::draw_all_hands_at(const ds3231_time_t& t) {
    unsigned h12 = static_cast<unsigned>(t.hours % 24) % 12u;
    float ang_h =
        (static_cast<float>(h12) + static_cast<float>(t.minutes) / 60.f + static_cast<float>(t.seconds) / 3600.f) * 30.f;
    float ang_m = (static_cast<float>(t.minutes) + static_cast<float>(t.seconds) / 60.f) * 6.f;
    float ang_s = static_cast<float>(t.seconds) * 6.f;

    int hx, hy, mx, my, sx, sy;
    tip_from_time(ang_h, h_len_, &hx, &hy);
    tip_from_time(ang_m, m_len_, &mx, &my);
    tip_from_time(ang_s, s_len_, &sx, &sy);

    draw_hand(cx_, cy_, hx, hy, h_th_, col_hand_hour_);
    draw_hand(cx_, cy_, mx, my, m_th_, col_hand_minute_);
    draw_hand(cx_, cy_, sx, sy, s_th_, col_hand_second_);

    h_x1_ = hx;
    h_y1_ = hy;
    m_x1_ = mx;
    m_y1_ = my;
    s_x1_ = sx;
    s_y1_ = sy;
    last_drawn_sec_ = static_cast<uint8_t>(t.seconds % 60);
    hands_ok_ = true;
    draw_center_cap(col_hub_);
}

void AnalogClockView::force_hands_sync(const ds3231_time_t& t) {
    if (hands_ok_) {
        erase_hand(cx_, cy_, h_x1_, h_y1_, h_th_);
        erase_hand(cx_, cy_, m_x1_, m_y1_, m_th_);
        erase_hand(cx_, cy_, s_x1_, s_y1_, s_th_);
        redraw_all_ticks();
    }
    draw_all_hands_at(t);
}

void AnalogClockView::on_second_tick(const ds3231_time_t& t) {
    unsigned h12 = static_cast<unsigned>(t.hours % 24) % 12u;
    float ang_h =
        (static_cast<float>(h12) + static_cast<float>(t.minutes) / 60.f + static_cast<float>(t.seconds) / 3600.f) * 30.f;
    float ang_m = (static_cast<float>(t.minutes) + static_cast<float>(t.seconds) / 60.f) * 6.f;
    float ang_s = static_cast<float>(t.seconds) * 6.f;

    int nhx, nhy, nmx, nmy, nsx, nsy;
    tip_from_time(ang_h, h_len_, &nhx, &nhy);
    tip_from_time(ang_m, m_len_, &nmx, &nmy);
    tip_from_time(ang_s, s_len_, &nsx, &nsy);

    if (!hands_ok_) {
        draw_all_hands_at(t);
        return;
    }

    const bool hour_moved = (nhx != h_x1_ || nhy != h_y1_);
    const bool minute_moved = (nmx != m_x1_ || nmy != m_y1_);

    /* 秒针：每秒必更新 */
    erase_hand(cx_, cy_, s_x1_, s_y1_, s_th_);

    if (hour_moved) {
        erase_hand(cx_, cy_, h_x1_, h_y1_, h_th_);
    }
    if (minute_moved) {
        erase_hand(cx_, cy_, m_x1_, m_y1_, m_th_);
    }

    if (hour_moved || minute_moved) {
        redraw_all_ticks();
    } else {
        draw_tick_index(static_cast<int>(last_drawn_sec_));
    }

    /* 统一按「时针→分针→秒针」叠画，修补各类擦除带来的断痕：
     * - 擦秒针：可能划过静止的时针、分针；
     * - 擦分针（针尖位移时）：粗线亦可能划过时针重叠段；
     * - 擦时针：短射线仍可能划过与其交叉的分针内侧段。
     * 下层先画满时针，再画分针（重叠处为分针压时针），最后秒针最上。 */
    draw_hand(cx_, cy_, nhx, nhy, h_th_, col_hand_hour_);
    draw_hand(cx_, cy_, nmx, nmy, m_th_, col_hand_minute_);
    draw_hand(cx_, cy_, nsx, nsy, s_th_, col_hand_second_);

    h_x1_ = nhx;
    h_y1_ = nhy;
    m_x1_ = nmx;
    m_y1_ = nmy;
    s_x1_ = nsx;
    s_y1_ = nsy;
    last_drawn_sec_ = static_cast<uint8_t>(t.seconds % 60);

    draw_center_cap(col_hub_);
}
