#include "ui-renderer.h"

#include "globals.h"
#include "input.h"
#include "status.h"
#include "video.h"
#include "video-helper.h"
#include "keyboard.h"
#include "file_browser.h"
#include "achievements.h"

#include "imgs/imgs_fast_forwarding.h"
#include "imgs/imgs_numbers.h"
#include "imgs/imgs_pause.h"
#include "imgs/imgs_press.h"
#include "imgs/imgs_retrorun.h"
#include "imgs/imgs_screenshot.h"

#include <atomic>
#include <chrono>
#include <string>

namespace {
status overlays = {};
bool last_menu_frame = false;
const int clear_frame_count = 6;
int clear_frames_left = clear_frame_count;
int loading_frame = 0;
bool loading_overlay_logged = false;
bool previous_overlay_visible = false;
std::atomic<int64_t> loading_started_ns{0};

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
    const bool completed = continueToShowSaveLoadStateDoneImage() ||
                           input_slot_memory_load_done || input_slot_memory_save_done ||
                           input_slot_memory_reset_done;
    const bool requested = input_slot_memory_load_requested || input_slot_memory_save_requested ||
                           input_slot_memory_plus_requested || input_slot_memory_minus_requested ||
                           continueToShowSaveLoadStateImage();
    overlays.show_bottom_center = completed || requested;
    if (!overlays.show_bottom_center) return;

    if (!status_surface_bottom_center)
        status_surface_bottom_center = rr_surface_create(display, 150, 20, format_565);
    makeScreenBlack(status_surface_bottom_center, 150, 20);

    std::string label;
    unsigned short color = WHITE;
    if (input_slot_memory_load_done) {
        label = lastLoadSaveStateDoneOk ? " SLOT:" + std::to_string(currentSlot) + " LOADED."
                                        : " LOAD FAILED!";
        if (!lastLoadSaveStateDoneOk) color = RED;
    } else if (input_slot_memory_save_done) {
        label = " SLOT:" + std::to_string(currentSlot) + " SAVED.";
    } else if (input_slot_memory_reset_done) {
        label = " CORE RESET DONE.";
    } else if (input_slot_memory_load_requested) {
        label = " LOADING SLOT:" + std::to_string(currentSlot) + " ...";
        color = ORANGE;
    } else if (input_slot_memory_save_requested) {
        label = " SAVING SLOT:" + std::to_string(currentSlot) + " ...";
        color = ORANGE;
    } else {
        label = " SLOT:" + std::to_string(currentSlot) + " SELECTED.";
    }
    showTextBigger(0, 5, label.c_str(), color, &status_surface_bottom_center);
}
} // namespace

bool uiRenderOverlays(const void* frame, unsigned width, unsigned height, size_t pitch) {
    bool visible = false;
    int overlay_width = static_cast<int>(width);
    int overlay_height = static_cast<int>(height);

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
        if (!status_surface_top_right)
            status_surface_top_right = rr_surface_create(display, numbers.width * 2,
                                                         numbers.height / 10, format_565);
        showFPSImage();
        visible = true;
    }

    if (screenshot_requested && !input_info_requested && !input_credits_requested)
        takeScreenshot(overlay_width, overlay_height);
    overlays.show_bottom_right = continueToShowScreenshotImage();
    if (overlays.show_bottom_right) {
        showImage(screenshot, &status_surface_bottom_right);
        visible = true;
    }

    const bool achievement_notification = achievements_notification_visible();
    overlays.show_top_left = input_ffwd_requested || input_message || achievement_notification;
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
        } else if (input_message)
            showText(10, 10, status_message.c_str(), WHITE, &status_surface_top_left);
        else
            showImage(fast, &status_surface_top_left);
        visible = true;
    }

    overlays.show_bottom_left = false;
    if (input_exit_requested_firstTime && !input_info_requested && !input_credits_requested) {
        showImage(quit, &status_surface_bottom_left);
        overlays.show_bottom_left = true;
    }
    if (input_pause_requested && !input_info_requested) {
        showImage(pause_img, &status_surface_bottom_left);
        overlays.show_bottom_left = true;
    }
    checkPaused();

    renderStateMessage();
    visible = visible || overlays.show_bottom_left || overlays.show_bottom_center;
    const bool overlay_visible = overlays.show_full || overlays.show_top_left ||
                                 overlays.show_top_right || overlays.show_bottom_left ||
                                 overlays.show_bottom_right || overlays.show_bottom_center;
    // Present one final composition after the last overlay disappears. GO2
    // game frames may not cover the letterbox area where it was drawn.
    const bool clean_disappeared_overlays = previous_overlay_visible && !overlay_visible;
    visible = visible || clean_disappeared_overlays;

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
    }

    last_menu_frame = input_info_requested || rr_keyboard_virtual_visible() ||
                      rr_file_browser_visible() || achievements_view_visible() ||
                      clear_frames_left != clear_frame_count;
    previous_overlay_visible = overlay_visible;
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
