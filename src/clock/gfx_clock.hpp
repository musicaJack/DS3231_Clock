#pragma once

#include <cstdint>

namespace ili9488 {
class ILI9488Driver;
}

namespace gfx_clock {

void fill_rect(ili9488::ILI9488Driver& driver, int x, int y, int w, int h, uint16_t color565);
void draw_line(ili9488::ILI9488Driver& driver, int x0, int y0, int x1, int y1, int thickness, uint16_t color565);
void draw_circle(ili9488::ILI9488Driver& driver, int cx, int cy, int radius, int thickness, uint16_t color565);

}  // namespace gfx_clock
