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
/**
 * 字体与布局（320×480 竖屏）：
 * - 日历：2×（16×32），底部水平居中；指针圆心不在底部，日历区不走秒刷新。
 * - 数字：2×（16×32），贴合刻度内沿；双位数宽约 32px，与小时角距匹配。
 */
constexpr unsigned kCalFontScale = 2u;
constexpr int kFontCellW = 8 * static_cast<int>(kCalFontScale);
constexpr int kFontCellH = 16 * static_cast<int>(kCalFontScale);

constexpr unsigned kDialNumScale = 2u;
constexpr int kDialCellW = 8 * static_cast<int>(kDialNumScale);
constexpr int kDialCellH = 16 * static_cast<int>(kDialNumScale);

/** 底部两行日历距屏底边（像素） */
constexpr int kCalBottomMargin = 14;
constexpr int kCalEdgePad = 2;
/** 两行日历间距（像素，2× 字时略加） */
constexpr int kCalLineGap = 4;
/** 整点长刻度：线段内端半径 = radius_ − 该值（与数字对齐用 r_tick_in） */
constexpr int kTickInnerInset = 14;
/** 非整点短刻度：内端更靠近外圈，仅缩短细分档长度，不改变整点长刻度 */
constexpr int kTickInnerInsetMinorShort = 8;
/** 数字框「靠刻度一侧」最外沿的目标半径 = r_tick_in − gap（详见 draw_hour_numerals 按角度的 support 对齐） */
constexpr int kNumeralRadialGapPx = 2;
/**
 * false：分针仅在「分钟寄存器」变化时换角度（走秒时不擦分针，省电减闪）；
 * true：分针随秒微扫（每秒更新针尖，模拟机械表）。
 */
constexpr bool kMinuteHandSweep = false;

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
 * 参考主盘：黑底、银白铁路轨式刻度、剑形白/银指针、红秒针。
 */
namespace dial_palette {
constexpr uint16_t kFace = c565::BLACK;
/** 外圈细环：深炭灰 */
constexpr uint16_t kRing = 0x3186U;
/** 细分刻度：冷灰 */
constexpr uint16_t kTickMinor = 0x5ACBU;
/** 整点刻度：亮银白 */
constexpr uint16_t kTickMajor = 0xDEFBU;
constexpr uint16_t kHandHour = 0xFFDFU;   // 近白
constexpr uint16_t kHandMinute = 0xDEDBU; // 略暗银
constexpr uint16_t kHandSecond = c565::RED;
constexpr uint16_t kHub = 0xCE79U;
}  // namespace dial_palette

AnalogClockView::AnalogClockView(ili9488::ILI9488Driver& driver)
    : drv_(&driver),
      col_face_(dial_palette::kFace),
      col_ring_(dial_palette::kRing),
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
    const bool major = (k % 5) == 0;
    const int r_in = major ? (radius_ - kTickInnerInset) : (radius_ - kTickInnerInsetMinorShort);
    float ang = static_cast<float>(k) * 6.f;
    int x0, y0, x1, y1;
    tip_from_time(ang, r_in, &x0, &y0);
    tip_from_time(ang, r_out, &x1, &y1);
    const uint16_t col = major ? col_tick_major_ : col_tick_minor_;
    gfx_clock::draw_line(*drv_, x0, y0, x1, y1, major ? 3 : 2, col);
}

void AnalogClockView::redraw_all_ticks() {
    for (int k = 0; k < 60; ++k) {
        draw_tick_index(k);
    }
}

void AnalogClockView::draw_center_cap(uint16_t color) {
    gfx_clock::draw_circle(*drv_, cx_, cy_, 8, 6, color);
}

void AnalogClockView::draw_hour_numerals() {
    const int r_tick_in = radius_ - kTickInnerInset;
    const float R_outer = static_cast<float>(r_tick_in - kNumeralRadialGapPx);
    const float ay = static_cast<float>(kDialCellH) / 2.f;

    for (int h = 1; h <= 12; ++h) {
        const float ang = static_cast<float>((h % 12) * 30);
        char buf[4];
        std::snprintf(buf, sizeof(buf), "%d", h);
        const float ax = (static_cast<float>(static_cast<int>(std::strlen(buf)) * kDialCellW)) / 2.f;

        const float rad = ang * (3.14159265f / 180.f);
        const float ux = sinf(rad);
        const float uy = -cosf(rad);
        /*
         * 轴对齐字框沿「时针径向」外侧的支撑深度：与矩形 support 一致，
         * 使框上离圆心最远一角落在固定半径 R_outer —— 这样单位数字（外侧投影主要为宽度的一半）
         * 会与 10/11/12（外侧投影主要为高度的一半）到刻度的视觉距离一致。
         */
        const float extent = std::fabs(ux) * ax + std::fabs(uy) * ay;
        const float r_c = R_outer - extent;

        const float tcx = static_cast<float>(cx_) + sinf(rad) * r_c;
        const float tcy = static_cast<float>(cy_) - cosf(rad) * r_c;
        const int tx = static_cast<int>(tcx + 0.5f);
        const int ty = static_cast<int>(tcy + 0.5f);

        const int wpx = static_cast<int>(std::strlen(buf)) * kDialCellW;
        int x = tx - wpx / 2;
        int y = ty - kDialCellH / 2;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        drv_->drawStringScaled(static_cast<uint16_t>(x), static_cast<uint16_t>(y), buf, c888::WHITE, c888::BLACK,
                               kDialNumScale);
    }
}

void AnalogClockView::paint_calendar_overlay() {
    if (!calendar_valid_) {
        return;
    }

    const int date_w = static_cast<int>(std::strlen(last_date_shown_)) * kFontCellW;
    const int week_w = static_cast<int>(std::strlen(last_week_shown_)) * kFontCellW;
    const int total_h = kFontCellH + kCalLineGap + kFontCellH;

    const int y_week = 480 - kCalBottomMargin - kFontCellH;
    const int y_date = y_week - kCalLineGap - kFontCellH;
    const int x_date = (320 - date_w) / 2;
    const int x_week = (320 - week_w) / 2;

    const int text_l = (x_date < x_week ? x_date : x_week) - kCalEdgePad;
    const int text_r =
        (x_date + date_w > x_week + week_w ? x_date + date_w : x_week + week_w) + kCalEdgePad;
    int box_x = text_l;
    int box_w = text_r - text_l;
    if (box_x < 0) {
        box_w += box_x;
        box_x = 0;
    }
    if (box_x + box_w > 320) {
        box_w = 320 - box_x;
    }
    const int box_y = y_date - kCalEdgePad;
    const int box_h = total_h + 2 * kCalEdgePad;
    if (box_w > 0 && box_h > 0) {
        gfx_clock::fill_rect(*drv_, box_x, box_y, box_w, box_h, col_face_);
    }

    drv_->drawStringScaled(static_cast<uint16_t>(x_date), static_cast<uint16_t>(y_date), last_date_shown_, c888::WHITE,
                           c888::BLACK, kCalFontScale);
    drv_->drawStringScaled(static_cast<uint16_t>(x_week), static_cast<uint16_t>(y_week), last_week_shown_,
                           c888::LIGHTGREY, c888::BLACK, kCalFontScale);
}

void AnalogClockView::refresh_calendar_ui(const ds3231_time_t& t) {
    char date_buf[8];
    std::snprintf(date_buf, sizeof(date_buf), "%02u/%02u", static_cast<unsigned>(t.month),
                  static_cast<unsigned>(t.date));

    static const char* wday[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    unsigned yfull = 2000u + static_cast<unsigned>(t.year % 100);
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
    calendar_overlay_dirty_ = true;

    force_hands_sync(t);
}

void AnalogClockView::paint_static_dial() {
    drv_->fillScreen(col_face_);
    calendar_valid_ = false;
    calendar_overlay_dirty_ = false;

    gfx_clock::draw_circle(*drv_, cx_, cy_, radius_, 3, col_ring_);

    redraw_all_ticks();
    draw_hour_numerals();
    draw_center_cap(col_hub_);

    hands_ok_ = false;
}

void AnalogClockView::draw_all_hands_at(const ds3231_time_t& t) {
    unsigned h12 = static_cast<unsigned>(t.hours % 24) % 12u;
    float ang_h =
        (static_cast<float>(h12) + static_cast<float>(t.minutes) / 60.f + static_cast<float>(t.seconds) / 3600.f) * 30.f;
    float ang_m = kMinuteHandSweep ? (static_cast<float>(t.minutes) + static_cast<float>(t.seconds) / 60.f) * 6.f
                                   : static_cast<float>(t.minutes % 60) * 6.f;
    float ang_s = static_cast<float>(t.seconds) * 6.f;

    int hx, hy, mx, my, sx, sy;
    tip_from_time(ang_h, h_len_, &hx, &hy);
    tip_from_time(ang_m, m_len_, &mx, &my);
    tip_from_time(ang_s, s_len_, &sx, &sy);

    draw_hour_numerals();

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
    if (calendar_valid_ && calendar_overlay_dirty_) {
        paint_calendar_overlay();
        calendar_overlay_dirty_ = false;
    }
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
    float ang_m = kMinuteHandSweep ? (static_cast<float>(t.minutes) + static_cast<float>(t.seconds) / 60.f) * 6.f
                                   : static_cast<float>(t.minutes % 60) * 6.f;
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

    draw_hour_numerals();

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
