#include "ui-renderer.h"

#include "globals.h"
#include "input.h"
#include "status.h"
#include "video.h"
#include "video-helper.h"
#include "keyboard.h"
#include "file_browser.h"
#include "achievements.h"
#include "decoration.h"
#include "fonts.h"

#include "imgs/imgs_retrorun.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <string>

namespace {
status overlays = {};
bool last_menu_frame = false;
const int clear_frame_count = 6;
int clear_frames_left = clear_frame_count;
int loading_frame = 0;
bool loading_overlay_logged = false;
unsigned previous_overlay_mask = 0;
int overlay_cleanup_frames = 0;
std::atomic<int64_t> loading_started_ns{0};

enum class PopupIcon { Pause, FastForward, Camera, Exit, Save, Load, Success,
                       Error, Info, Reset, Slot };

class PopupMessage {
public:
    PopupMessage(PopupIcon icon, std::string text, uint16_t color = 0xffff)
        : icon_(icon), text_(std::move(text)), color_(color) {}

    int width() const {
        constexpr int text_x = 48;
        constexpr int glyph_width = 8;
        constexpr int right_padding = 10;
        return std::max(108, text_x + static_cast<int>(text_.size()) * glyph_width + right_padding);
    }
    static constexpr int height() { return 44; }

    void render(rr_surface_t* target) const {
        if (!target) return;
        uint16_t* pixels = static_cast<uint16_t*>(rr_surface_map(target));
        if (!pixels) return;
        const int w = rr_surface_width_get(target);
        const int h = rr_surface_height_get(target);
        const int stride = rr_surface_stride_get(target) / 2;
        // Fully opaque RGB565 HUD. The rectangular surface is painted to
        // every edge: without an alpha channel, unused black corner pixels
        // would look like an accidental halo over the game.
        constexpr uint16_t popup_background = 0x4a49;
        std::fill(pixels, pixels + stride * h, popup_background);
        fillRect(pixels, stride, w, h, 0, 0, w - 1, 2, 0x6b4d);
        fillRect(pixels, stride, w, h, 0, 3, 2, h - 1, 0x5acb);
        fillRect(pixels, stride, w, h, 3, h - 3, w - 1, h - 1, 0x18e3);
        fillRect(pixels, stride, w, h, w - 3, 3, w - 1, h - 4, 0x2124);
        // Keep the semantic colour attached to the symbol: a small pixel
        // frame integrates the icon without becoming a separator or a tile.
        const uint16_t accent = iconColor();
        // Continuous chamfered frame. The 2x2 corner steps connect the four
        // sides instead of leaving them looking like unrelated straight bars.
        fillRect(pixels, stride, w, h, 9, 6, 35, 7, accent);
        fillRect(pixels, stride, w, h, 7, 8, 8, 9, accent);
        fillRect(pixels, stride, w, h, 36, 8, 37, 9, accent);
        fillRect(pixels, stride, w, h, 6, 10, 7, 33, accent);
        fillRect(pixels, stride, w, h, 37, 10, 38, 33, accent);
        fillRect(pixels, stride, w, h, 7, 34, 8, 35, accent);
        fillRect(pixels, stride, w, h, 36, 34, 37, 35, accent);
        fillRect(pixels, stride, w, h, 9, 36, 35, 37, accent);
        drawIcon(pixels, stride, w, h);
        basic_text_out16_nf_color_clipped(pixels, stride, w, h, 49, 19,
                                         text_.c_str(), 0x0000);
        basic_text_out16_nf_color_clipped(pixels, stride, w, h, 48, 18,
                                         text_.c_str(), color_);
        rr_surface_unmap(target);
    }

private:
    PopupIcon icon_;
    std::string text_;
    uint16_t color_;

    static void pixel(uint16_t* p, int stride, int w, int h, int x, int y, uint16_t c) {
        if (x >= 0 && x < w && y >= 0 && y < h) p[y * stride + x] = c;
    }
    static void fillRect(uint16_t* p, int stride, int w, int h,
                         int x0, int y0, int x1, int y1, uint16_t c) {
        x0 = std::max(0, x0); y0 = std::max(0, y0);
        x1 = std::min(w - 1, x1); y1 = std::min(h - 1, y1);
        for (int y = y0; y <= y1; ++y)
            std::fill(p + y * stride + x0, p + y * stride + x1 + 1, c);
    }
    static void line(uint16_t* p, int stride, int w, int h,
                     int x0, int y0, int x1, int y1, uint16_t c) {
        int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int error = dx + dy;
        for (;;) {
            pixel(p, stride, w, h, x0, y0, c);
            pixel(p, stride, w, h, x0 + 1, y0, c);
            if (x0 == x1 && y0 == y1) break;
            const int twice = error * 2;
            if (twice >= dy) { error += dy; x0 += sx; }
            if (twice <= dx) { error += dx; y0 += sy; }
        }
    }
    static void circle(uint16_t* p, int stride, int w, int h,
                       int cx, int cy, int radius, uint16_t c) {
        int x = radius, y = 0, error = 0;
        while (x >= y) {
            const int points[][2] = {{x,y},{y,x},{-y,x},{-x,y},{-x,-y},{-y,-x},{y,-x},{x,-y}};
            for (const auto& point : points)
                pixel(p, stride, w, h, cx + point[0], cy + point[1], c);
            ++y;
            if (error <= 0) error += 2 * y + 1;
            if (error > 0) { --x; error -= 2 * x + 1; }
        }
    }
    uint16_t iconColor() const {
        switch (icon_) {
        case PopupIcon::Success: return 0x47e8;
        case PopupIcon::Error: return 0xf249;
        case PopupIcon::FastForward:
        case PopupIcon::Pause: return 0xffe6;
        default: return 0xfbe4;
        }
    }
    void drawIcon(uint16_t* p, int stride, int w, int h) const {
        const uint16_t c = icon_ == PopupIcon::Error ? iconColor() : 0xffff;
        constexpr uint16_t hi = 0xd69a;
        constexpr uint16_t bg = 0x4a49;
        // The frame centre is (22, 21.5). Glyphs use a 24x26 design grid,
        // therefore this origin centres their visual bounds in both axes.
        const int ox = 10, oy = 8;
        auto scaled = [](int value) { return value; };
        auto r = [&](int x0, int y0, int x1, int y1, uint16_t color) {
            fillRect(p, stride, w, h, ox+x0, oy+y0, ox+x1, oy+y1, color);
        };
        auto circ = [&](int cx, int cy, int radius, uint16_t color) {
            circle(p, stride, w, h, ox+scaled(cx), oy+scaled(cy), scaled(radius), color);
        };
        switch (icon_) {
        case PopupIcon::Pause:
            r(3,2,8,25,c); r(15,2,20,25,c);
            r(4,3,7,5,hi); r(16,3,19,5,hi);
            break;
        case PopupIcon::FastForward:
            // The original glyph occupied x=1..20 on the 24-pixel grid.
            // Shift it two pixels right to align its visual centre with the
            // pause bars and the centre of the chamfered frame.
            for (int i=0; i<12; ++i) { r(3+i/2,3+i,5+i/2,4+i,c); r(3+(11-i)/2,15+i,5+(11-i)/2,16+i,c); }
            for (int i=0; i<12; ++i) { r(14+i/2,3+i,16+i/2,4+i,c); r(14+(11-i)/2,15+i,16+(11-i)/2,16+i,c); }
            break;
        case PopupIcon::Camera:
            r(2,8,22,22,c); r(7,4,16,9,c); r(4,10,20,20,bg);
            circ(12,15,5,c); circ(12,15,2,hi); r(18,10,20,12,c); break;
        case PopupIcon::Exit:
            r(2,3,12,23,c); r(5,6,10,20,bg); r(9,12,23,15,c);
            r(18,8,21,19,c); r(21,11,24,16,c); break;
        case PopupIcon::Save:
            r(4,2,20,21,c); r(2,6,22,18,c); r(6,5,18,9,bg);
            r(7,12,17,16,hi); r(9,19,15,23,c); break;
        case PopupIcon::Load:
            r(4,2,20,21,c); r(2,6,22,18,c); r(6,5,18,9,bg);
            r(11,10,14,18,hi); r(8,15,17,18,hi); r(10,18,15,21,hi); break;
        case PopupIcon::Success:
            line(p,stride,w,h,ox+4,oy+13,ox+10,oy+19,c);
            line(p,stride,w,h,ox+10,oy+19,ox+21,oy+7,c); break;
        case PopupIcon::Error:
            for (int i=0; i<4; ++i) {
                line(p,stride,w,h,ox+5+i,oy+5,ox+19+i,oy+19,c);
                line(p,stride,w,h,ox+19-i,oy+5,ox+5-i,oy+19,c);
            }
            break;
        case PopupIcon::Info:
            r(10,9,14,19,c); r(8,18,16,21,c); r(10,4,14,7,c); break;
        case PopupIcon::Reset:
            circ(12,14,9,c); circ(12,14,6,bg); r(12,2,23,9,bg);
            r(17,2,21,11,c); r(14,5,20,10,c); r(7,12,10,15,hi);
            r(15,12,18,15,hi); r(10,10,15,17,hi); break;
        case PopupIcon::Slot:
            r(7,2,21,17,hi); r(4,6,18,21,c); r(1,10,15,25,hi);
            r(4,13,12,16,bg); r(5,19,8,22,c); r(10,19,12,22,c); break;
        }
    }
};

void renderPopup(rr_surface_t*& surface, const PopupMessage& popup) {
    if (surface && (rr_surface_width_get(surface) != popup.width() ||
                    rr_surface_height_get(surface) != PopupMessage::height())) {
        rr_surface_destroy(surface);
        surface = nullptr;
    }
    if (!surface)
        surface = rr_surface_create(display, popup.width(), PopupMessage::height(), format_565);
    popup.render(surface);
}

int64_t steadyNowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

void bindOverlaySurfaces() {
    overlays.bottom_center = status_surface_bottom_center;
    overlays.bottom_left = status_surface_bottom_left;
    overlays.bottom_right = status_surface_bottom_right;
    overlays.top_right = status_surface_top_right;
    overlays.top_left = status_surface_top_left;
    overlays.full = status_surface_full;
    overlays.decoration = decoration_surface();
    overlays.decoration_background = decoration_background_surface();
}

void renderFullOverlay(int width, int height) {
    if (status_surface_full &&
        (rr_surface_width_get(status_surface_full) != width ||
         rr_surface_height_get(status_surface_full) != height)) {
        rr_surface_destroy(status_surface_full);
        status_surface_full = nullptr;
        overlays.full = nullptr;
    }
    if (!status_surface_full)
        status_surface_full = rr_surface_create(display, width, height, format_565);

    if (rr_keyboard_virtual_visible()) {
        rr_keyboard_virtual_render(status_surface_full, width, height);
    } else if (rr_file_browser_visible()) {
        rr_file_browser_render(status_surface_full, width, height);
    } else if (achievements_view_visible()) {
        achievements_view_render(status_surface_full, width, height);
    } else if (input_credits_requested && !showLoading) {
        makeScreenBlackCredits(status_surface_full, width, height);
        showCredits(&status_surface_full);
    } else if (last_menu_frame && !input_info_requested && clear_frames_left > 0) {
        makeScreenBlackCredits(status_surface_full, width, height);
        if (--clear_frames_left == 0) clear_frames_left = clear_frame_count;
    } else if (showLoading) {
        if (!loading_overlay_logged) {
            logger.log(Logger::DEB, "Loading screen: presenting the first loading overlay frame");
            loading_overlay_logged = true;
        }
        const std::string label = (++loading_frame % 30 > 15) ? ". Please wait ." : "  Please wait  ";
        makeScreenBlack(status_surface_full, width, height);
        showFullImage((width - static_cast<int>(retrorun_logo.width)) / 2, 18,
                      retrorun_logo.width, retrorun_logo.height,
                      retrorun_logo.pixel_data, &status_surface_full);
        showTextBigger(width / 2 - static_cast<int>(label.length()) * 4,
                       116, label.c_str(), WHITE, &status_surface_full);
    } else {
        drawMenuInfoBackgroud(status_surface_full, width, height);
        showInfo(gs_w, &status_surface_full);
    }
    overlays.show_full = true;
}

void renderStateMessage() {
    static rr_surface_t* rendered_surface = nullptr;
    static std::string rendered_label;
    static unsigned short rendered_color = 0;
    const bool completed = continueToShowSaveLoadStateDoneImage() ||
                           input_slot_memory_load_done || input_slot_memory_save_done ||
                           input_slot_memory_reset_done;
    const bool requested = input_slot_memory_load_requested || input_slot_memory_save_requested ||
                           input_slot_memory_plus_requested || input_slot_memory_minus_requested ||
                           continueToShowSaveLoadStateImage();
    overlays.show_bottom_center = completed || requested;
    if (!overlays.show_bottom_center) return;

    std::string label;
    unsigned short color = WHITE;
    PopupIcon icon = PopupIcon::Slot;
    if (input_slot_memory_load_done) {
        label = lastLoadSaveStateDoneOk ? "Slot " + std::to_string(currentSlot) + " loaded"
                                        : "Load failed";
        icon = lastLoadSaveStateDoneOk ? PopupIcon::Success : PopupIcon::Error;
        if (!lastLoadSaveStateDoneOk) color = RED;
    } else if (input_slot_memory_save_done) {
        label = "Slot " + std::to_string(currentSlot) + " saved";
        icon = PopupIcon::Success;
    } else if (input_slot_memory_reset_done) {
        label = "Core reset";
        icon = PopupIcon::Reset;
    } else if (input_slot_memory_load_requested) {
        label = "Loading slot " + std::to_string(currentSlot) + "...";
        color = ORANGE;
        icon = PopupIcon::Load;
    } else if (input_slot_memory_save_requested) {
        label = "Saving slot " + std::to_string(currentSlot) + "...";
        color = ORANGE;
        icon = PopupIcon::Save;
    } else {
        label = "Slot " + std::to_string(currentSlot) + " selected";
    }
    if (rendered_surface != status_surface_bottom_center ||
        rendered_label != label || rendered_color != color) {
        renderPopup(status_surface_bottom_center, PopupMessage(icon, label, color));
        rendered_surface = status_surface_bottom_center;
        rendered_label = label;
        rendered_color = color;
    }
}
} // namespace

bool uiRenderOverlays(const void* frame, unsigned width, unsigned height, size_t pitch) {
    bool visible = false;
    int overlay_width = static_cast<int>(width);
    int overlay_height = static_cast<int>(height);
    overlays.decoration = decoration_surface();
    overlays.show_decoration = overlays.decoration != nullptr;
    visible = overlays.show_decoration;

    if (input_info_requested || input_credits_requested || rr_keyboard_virtual_visible() ||
        rr_file_browser_visible() || achievements_view_visible() || last_menu_frame || showLoading) {
        if (!showLoading)
            updateUIMenuDimensions(w, h);
        overlay_width = INFO_MENU_WIDTH;
        overlay_height = INFO_MENU_HEIGHT;
        renderFullOverlay(overlay_width, overlay_height);
        visible = true;
    } else {
        overlays.show_full = false;
        if (!isOpenGL) drawNonOpenGL(frame, width, height, pitch);
    }

    overlays.show_top_right = input_fps_requested && !input_info_requested && !input_credits_requested;
    if (overlays.show_top_right) {
        showFPSImage();
        visible = true;
    }

    if (screenshot_requested && !input_info_requested && !input_credits_requested)
        takeScreenshot(overlay_width, overlay_height);
    const bool previous_screenshot_visible = overlays.show_bottom_right;
    overlays.show_bottom_right = continueToShowScreenshotImage();
    if (overlays.show_bottom_right) {
        if (!previous_screenshot_visible || !status_surface_bottom_right)
            renderPopup(status_surface_bottom_right,
                        PopupMessage(PopupIcon::Camera, "Screenshot saved"));
        visible = true;
    }

    const bool achievement_notification = achievements_notification_visible();
    overlays.show_top_left = fastForwardNotificationVisible() || input_message || achievement_notification;
    if (overlays.show_top_left) {
        if (achievement_notification) {
            constexpr int notification_width = 300;
            constexpr int notification_height = 58;
            if (status_surface_top_left &&
                (rr_surface_width_get(status_surface_top_left) != notification_width ||
                 rr_surface_height_get(status_surface_top_left) != notification_height)) {
                rr_surface_destroy(status_surface_top_left);
                status_surface_top_left = nullptr;
            }
            if (!status_surface_top_left)
                status_surface_top_left = rr_surface_create(display, notification_width,
                                                            notification_height, format_565);
            achievements_render_notification(status_surface_top_left);
        } else if (input_message) {
            renderPopup(status_surface_top_left,
                        PopupMessage(PopupIcon::Info, status_message));
        } else if (fastForwardNotificationVisible()) {
            renderPopup(status_surface_top_left,
                        PopupMessage(PopupIcon::FastForward, "Fast-forward"));
        }
        visible = true;
    }

    const bool previous_bottom_left_visible = overlays.show_bottom_left;
    overlays.show_bottom_left = false;
    const PopupMessage* bottom_left_popup = nullptr;
    const PopupMessage exit_popup(PopupIcon::Exit, "Hold to exit");
    const PopupMessage pause_popup(PopupIcon::Pause, "Paused");
    if (input_exit_requested_firstTime && !input_info_requested && !input_credits_requested) {
        bottom_left_popup = &exit_popup;
        overlays.show_bottom_left = true;
    }
    if (input_pause_requested && !input_info_requested) {
        bottom_left_popup = &pause_popup;
        overlays.show_bottom_left = true;
    }
    if (bottom_left_popup && (!previous_bottom_left_visible || !status_surface_bottom_left))
        renderPopup(status_surface_bottom_left, *bottom_left_popup);
    checkPaused();

    renderStateMessage();
    visible = visible || overlays.show_bottom_left || overlays.show_bottom_center;
    const unsigned overlay_mask = (overlays.show_full ? 1U : 0U) |
                                  (overlays.show_top_left ? 2U : 0U) |
                                  (overlays.show_top_right ? 4U : 0U) |
                                  (overlays.show_bottom_left ? 8U : 0U) |
                                  (overlays.show_bottom_right ? 16U : 0U) |
                                  (overlays.show_bottom_center ? 32U : 0U);
    const unsigned complete_overlay_mask = overlay_mask |
                                           (overlays.show_decoration ? 64U : 0U);
    // GO2 recycles three framebuffers. Clear each one only when the overlay
    // layout changes, then leave the steady-state FPS/overlay path untouched.
#ifndef RR_PLATFORM_SDL
    static int previous_game_x = -1, previous_game_y = -1;
    static int previous_game_w = -1, previous_game_h = -1;
    if (complete_overlay_mask != previous_overlay_mask || x != previous_game_x ||
        y != previous_game_y || w != previous_game_w || h != previous_game_h)
        overlay_cleanup_frames = 3;
    overlays.clean_full = overlay_cleanup_frames > 0;
    visible = visible || overlays.clean_full;
#else
    overlays.clean_full = false;
#endif

    bool use_software_presenter = presenter != nullptr;
#ifdef RR_PLATFORM_SDL
    // SDL_Renderer and OpenGL cannot safely present alternately to the same
    // KMSDRM window. Hardware cores compose these surfaces in
    // rr_context_swap_buffers() immediately after this function returns.
    if (isOpenGL)
        use_software_presenter = false;
#endif
    if (visible && use_software_presenter) {
        bindOverlaySurfaces();
        if (showLoading) {
            // Software cores do not need the game frame beneath the opaque
            // loading page. Presenting it directly also avoids losing the
            // freshly drawn logo in a multi-surface composition.
            rr_presenter_post(presenter, status_surface_full,
                              0, 0, overlay_width, overlay_height,
                              x, y, w, h, getRotation());
            uiNotifyLoadingPresented();
            rr_presenter_wait_for_loading_screen(presenter, 700);
        } else {
            rr_surface_t* base = isOpenGL ? gles_surface : surface;
            const int source_y = isOpenGL ? gs_h - static_cast<int>(height) : 0;
            rr_presenter_post_multiple(presenter, base, &overlays,
                                       0, source_y, overlay_width, overlay_height,
                                       x, y, w, h, getRotation(), getBlitRotation(), isWideScreen);
        }
        if (overlay_cleanup_frames > 0) --overlay_cleanup_frames;
    }

    last_menu_frame = input_info_requested || rr_keyboard_virtual_visible() ||
                      rr_file_browser_visible() || achievements_view_visible() ||
                      clear_frames_left != clear_frame_count;
    previous_overlay_mask = complete_overlay_mask;
#ifndef RR_PLATFORM_SDL
    previous_game_x = x; previous_game_y = y;
    previous_game_w = w; previous_game_h = h;
#endif
    return visible;
}

status* uiCurrentOverlays() {
    bindOverlaySurfaces();
    return &overlays;
}

void uiNotifyLoadingPresented() {
    int64_t expected = 0;
    loading_started_ns.compare_exchange_strong(expected, steadyNowNs());
}

bool uiLoadingMinimumDurationElapsed() {
    constexpr int64_t minimum_duration_ns = 700LL * 1000LL * 1000LL;
    const int64_t started = loading_started_ns.load();
    return started != 0 && steadyNowNs() - started >= minimum_duration_ns;
}
