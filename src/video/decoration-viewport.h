#pragma once

#include <cmath>

namespace rr {

// RetroArch custom viewport offsets are relative to its viewport bias, not
// absolute framebuffer coordinates. Work in top-left display coordinates;
// panel rotation is applied by the decoration layout code afterwards.
inline bool decoration_viewport_position(int display_width, int display_height,
                                        int width, int height, int* x, int* y,
                                        double bias_x = 0.5, double bias_y = 0.5)
{
    if (!x || !y || display_width <= 0 || display_height <= 0 ||
        width <= 0 || height <= 0 || width > display_width || height > display_height ||
        !std::isfinite(bias_x) || !std::isfinite(bias_y) ||
        bias_x < 0.0 || bias_x > 1.0 || bias_y < 0.0 || bias_y > 1.0)
        return false;

    const double left = *x + (display_width - width) * bias_x;
    const double top = *y + (display_height - height) * bias_y;
    if (left < 0.0 || top < 0.0 || left + width > display_width ||
        top + height > display_height)
        return false;

    *x = static_cast<int>(left);
    *y = static_cast<int>(top);
    return true;
}

} // namespace rr
