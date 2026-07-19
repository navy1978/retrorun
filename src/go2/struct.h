#pragma once
#include "display.h"
#include "queue.h"
#include <semaphore.h>
#include <rga/RgaApi.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

typedef struct go2_display
{
    int fd;
    uint32_t connector_id;
    drmModeModeInfo mode;
    uint32_t width;
    uint32_t height;
    uint32_t crtc_id;
    int crtc_index;
    bool modeset_complete;
    bool page_flip_disabled;
    uint32_t direct_plane_id;
    uint32_t direct_plane_format;
    bool direct_plane_disabled;
    bool direct_plane_logged;
    char drm_driver[64];
    bool direct_rotation_property_found;
    bool direct_rotation_applied;
    bool direct_plane_active;
    int direct_plane_errno;
    uint64_t direct_present_count;
    uint64_t direct_vblank_total_us;
    uint32_t direct_vblank_samples;
    uint32_t direct_vblank_last_us;
    uint32_t direct_vblank_max_us;
    uint32_t direct_vblank_failures;
} go2_display_t;

typedef struct go2_surface
{
    go2_display_t* display;
    uint32_t gem_handle;
    uint64_t size;
    int width;
    int height;
    int stride;
    uint32_t format;
    int prime_fd;
    bool is_mapped;
    uint8_t* map;
    uint32_t direct_fb_id;
} go2_surface_t;

typedef struct go2_frame_buffer
{
    go2_surface_t* surface;
    uint32_t fb_id;
} go2_frame_buffer_t;

typedef struct go2_presenter
{
    go2_display_t* display;
    uint32_t format;
    uint32_t background_color;
    go2_queue_t* freeFrameBuffers;
    go2_queue_t* usedFrameBuffers;
    pthread_mutex_t queueMutex;
    pthread_t renderThread;
    sem_t freeSem;
    sem_t usedSem;
    volatile bool terminating;
} go2_presenter_t;
