// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <string_view>

// RGB565 matches the shared CPU OSD surfaces. No parsing or resource ownership.
namespace osd {
struct Palette {
    uint16_t primary = 0xffff, secondary = 0xbdf7, label = 0x7bef, infoValue = 0xbdf7;
    uint16_t normal = 0x7bef, disabled = 0x7bef, heading = 0xfbe0;
    uint16_t focusText = 0xffff, focusSDL = 0x214a, focusGO2 = 0x2b5f;
    uint16_t active = 0x07e0, error = 0xf800, cursor = 0xffe0, subtitle = 0xffe0;
    uint16_t background = 0x0841, menuBody = 0x0000, menuFrame = 0x4a4a;
    uint16_t keyPanel = 0x1082, keyFocus = 0x39e7, keyText = 0xffe0;
    uint16_t popup = 0x4a49, bevelTop = 0x6b4d, bevelLeft = 0x5acb;
    uint16_t bevelBottom = 0x18e3, bevelRight = 0x2124;
    uint16_t popupAccent = 0xfbe4, success = 0x47e8, popupError = 0xf249;
    uint16_t warning = 0xffe6, iconHighlight = 0xd69a;
    uint16_t shadow = 0x0000, fpsShadow = 0x4208, fpsBackground = 0x0000;
    uint16_t track = 0x2104, trackInside = 0x0000;
};
struct Metrics {
    int glyphWidth = 8, glyphHeight = 8;
    int handheldWidth = 240, handheldHeight = 160, maxVisibleRows = 0;
    const char* footerHint = nullptr;
    int handheldRow = 10, desktopRow = 12, rowMargin = 4, textMargin = 12;
    int frameHeight = 12, popupHeight = 44, popupTextX = 48;
    int popupTextY = 18, popupPadding = 10, popupMinimumWidth = 108;
    int fpsPadding = 6, largeFpsPadding = 8;
};
struct Theme {
    const char* name = "classic";
    Palette colors{};
    Metrics metrics{};
    uint8_t menuOpacity = 255;
    bool emphasizeValues = false;
};
inline constexpr Theme classic{};
constexpr uint16_t rgb565(unsigned red, unsigned green, unsigned blue) {
    return static_cast<uint16_t>(((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3));
}
inline constexpr Theme retrorun = [] {
    Theme t;
    t.name = "retrorun";
    auto& c = t.colors;
    c.primary = rgb565(235,235,240); c.secondary = rgb565(145,145,150);
    c.label = c.secondary; c.infoValue = c.primary; c.normal = c.primary; c.disabled = rgb565(100,100,105);
    c.heading = c.primary; c.focusText = c.primary;
    c.focusSDL = c.focusGO2 = rgb565(195,0,25); // Launcher focus #C30019.
    c.active = rgb565(255,205,64); // Exact Launcher/Favorites gold #FFCD40.
    c.error = c.popupError = rgb565(230,0,30);
    c.cursor = c.focusSDL; c.subtitle = c.secondary;
    c.background = c.menuBody = rgb565(12,12,16);
    c.menuFrame = rgb565(55,55,60);
    c.keyPanel = rgb565(24,24,28); c.keyFocus = rgb565(25,24,29); c.keyText = c.primary;
    c.popup = rgb565(24,24,28); c.popupAccent = c.active;
    c.success = c.active; c.warning = c.active;
    c.bevelTop = c.active;
    c.bevelLeft = c.bevelBottom = c.bevelRight = c.popup;
    c.iconHighlight = c.secondary; c.fpsShadow = c.shadow;
    c.track = rgb565(65,65,70); c.trackInside = c.background;
    c.fpsBackground = c.background;
    t.metrics.handheldWidth = 320; t.metrics.handheldHeight = 240;
    t.metrics.handheldRow = t.metrics.desktopRow = 14;
    t.metrics.maxVisibleRows = 10; t.metrics.footerHint = "A Select   B Back";
    t.menuOpacity = 220; t.emphasizeValues = true;
    return t;
}();
// Selected once during config loading, before render/audio workers start.
inline const Theme* current = &classic;
inline const Theme& theme() { return *current; }
inline bool select(std::string_view name) {
    current = name == "retrorun" ? &retrorun : &classic;
    return name.empty() || name == "classic" || name == "retrorun";
}
inline uint16_t valueColor(std::string_view value, uint16_t normal) {
    if (!theme().emphasizeValues) return normal;
    if (value == "yes" || value == "ON" || value == "On" || value == "on" || value == "enabled" || value == "Enabled" || value == "true" || value == "True")
        return theme().colors.active;
    if (value == "no" || value == "OFF" || value == "Off" || value == "off" || value == "disabled" || value == "Disabled" || value == "false" || value == "False")
        return theme().colors.disabled;
    return normal;
}
inline uint16_t focusFill(bool nativeGO2) {
    return nativeGO2 ? theme().colors.focusGO2 : theme().colors.focusSDL;
}
} // namespace osd
