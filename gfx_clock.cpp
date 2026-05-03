#include "gfx_clock.hpp"
#include "ili9488_driver.hpp"
#include <cstdlib>

namespace gfx_clock {

void fill_rect(ili9488::ILI9488Driver& driver, int x, int y, int w, int h, uint16_t color565) {
    if (w <= 0 || h <= 0) return;
    int x1 = x + w - 1;
    int y1 = y + h - 1;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 >= 320) x1 = 319;
    if (y1 >= 480) y1 = 479;
    if (x > x1 || y > y1) return;
    driver.fillArea(static_cast<uint16_t>(x), static_cast<uint16_t>(y),
                    static_cast<uint16_t>(x1), static_cast<uint16_t>(y1), color565);
}

void draw_line(ili9488::ILI9488Driver& driver, int x0, int y0, int x1, int y1, int thickness, uint16_t color565) {
    int dx = std::abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int r = thickness / 2;
    if (r < 0) r = 0;
    for (;;) {
        fill_rect(driver, x0 - r, y0 - r, thickness, thickness, color565);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void draw_circle(ili9488::ILI9488Driver& driver, int cx, int cy, int radius, int thickness, uint16_t color565) {
    if (radius <= 0) return;
    int t = thickness < 1 ? 1 : thickness;
    for (int rr = radius; rr > radius - t && rr > 0; rr--) {
        int x = rr;
        int y = 0;
        int err = 0;
        while (x >= y) {
            driver.drawPixel(static_cast<uint16_t>(cx + x), static_cast<uint16_t>(cy + y), color565);
            driver.drawPixel(static_cast<uint16_t>(cx + y), static_cast<uint16_t>(cy + x), color565);
            driver.drawPixel(static_cast<uint16_t>(cx - y), static_cast<uint16_t>(cy + x), color565);
            driver.drawPixel(static_cast<uint16_t>(cx - x), static_cast<uint16_t>(cy + y), color565);
            driver.drawPixel(static_cast<uint16_t>(cx - x), static_cast<uint16_t>(cy - y), color565);
            driver.drawPixel(static_cast<uint16_t>(cx - y), static_cast<uint16_t>(cy - x), color565);
            driver.drawPixel(static_cast<uint16_t>(cx + y), static_cast<uint16_t>(cy - x), color565);
            driver.drawPixel(static_cast<uint16_t>(cx + x), static_cast<uint16_t>(cy - y), color565);
            y++;
            err += 1 + 2 * y;
            if (2 * (err - x) + 1 > 0) {
                x--;
                err += 1 - 2 * x;
            }
        }
    }
}

}  // namespace gfx_clock
