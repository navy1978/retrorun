/*
libgo2 - Support library for the ODROID-GO Advance
Copyright (C) 2020 OtherCrashOverride
Copyright (C) 2023-present  navy1978

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "display.h"
#include "struct.h"

#include "../status.h"
#include "../input.h"

#include "queue.h"
#include "../globals.h"

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm/drm_fourcc.h>
#include <gbm.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <poll.h>

#include <rga/RgaApi.h>

#define EGL_EGLEXT_PROTOTYPES
// #define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
// #include <GLES2/gl2.h>
// #include <GLES2/gl2ext.h>

#include <png.h>

#include <algorithm>

go2_display_t *go2_display_create()
{
    int i;

    go2_display_t *result = (go2_display_t *)malloc(sizeof(*result));
    if (!result)
    {
        logger.log(Logger::ERR,"malloc failed.\n");
        return NULL;
    }

    memset(result, 0, sizeof(*result));

    // Open device
    result->fd = open("/dev/dri/card0", O_RDWR);
    if (result->fd < 0)
    {
        logger.log(Logger::ERR,"open /dev/dri/card0 failed.\n");
        free(result);
        return NULL;
    }

    drmModeRes *resources = drmModeGetResources(result->fd);
    if (!resources)
    {
        logger.log(Logger::ERR,"drmModeGetResources failed: %s\n", strerror(errno));
        close(result->fd);
        free(result);
        return NULL;
    }

    // Find connector
    drmModeConnector *connector;
    for (i = 0; i < resources->count_connectors; i++)
    {
        connector = drmModeGetConnector(result->fd, resources->connectors[i]);
        if (connector->connection == DRM_MODE_CONNECTED)
        {
            break;
        }

        drmModeFreeConnector(connector);
        connector = NULL;
    }

    if (!connector)
    {
        logger.log(Logger::ERR,"DRM_MODE_CONNECTED not found.\n");
        drmModeFreeResources(resources);
        close(result->fd);
        free(result);
        return NULL;
    }

    result->connector_id = connector->connector_id;

    // Find prefered mode
    drmModeModeInfo *mode;
    for (i = 0; i < connector->count_modes; i++)
    {
        drmModeModeInfo *current_mode = &connector->modes[i];
        if (current_mode->type & DRM_MODE_TYPE_PREFERRED)
        {
            mode = current_mode;
            break;
        }

        mode = NULL;
    }

    if (!mode)
    {
        logger.log(Logger::ERR,"DRM_MODE_TYPE_PREFERRED not found.\n");
        drmModeFreeConnector(connector);
        drmModeFreeResources(resources);
        close(result->fd);
        free(result);
        return NULL;
    }

    result->mode = *mode;
    result->width = mode->hdisplay;
    result->height = mode->vdisplay;

    // Find encoder
    drmModeEncoder *encoder = nullptr;
    for (i = 0; i < resources->count_encoders; i++)
    {
        encoder = drmModeGetEncoder(result->fd, resources->encoders[i]);
        if (!encoder) {
            logger.log(Logger::WARN, "Failed to get encoder at index %d (id: %u)", i, resources->encoders[i]);
            continue;
        }else{
            logger.log(Logger::DEB, "Got encoder at index %d (id: %u)", i, resources->encoders[i]);
        }

        logger.log(Logger::DEB, "Checking encoder at index %d: encoder_id = %u", i, encoder->encoder_id);

        if (encoder->encoder_id == connector->encoder_id)
        {
            logger.log(Logger::DEB, "Found matching encoder (id: %u) for connector (id: %u)", 
                   encoder->encoder_id, connector->connector_id);
            break;
        }else{
            logger.log(Logger::WARN, "No matching encoder (id: %u) for connector (id: %u) found", 
                encoder->encoder_id, connector->connector_id);
        }

        drmModeFreeEncoder(encoder);
        encoder = nullptr;
    }

    if (!encoder) {
        logger.log(Logger::ERR, "No matching encoder found for connector (id: %u)", connector->connector_id);
    }

    if (!encoder)
    {

        logger.log(Logger::ERR,"could not find encoder!\n");
        drmModeFreeConnector(connector);
        drmModeFreeResources(resources);
        close(result->fd);
        free(result);
        return NULL;
    }

    result->crtc_id = encoder->crtc_id;
    result->crtc_index = -1;
    for (i = 0; i < resources->count_crtcs; ++i)
    {
        if (resources->crtcs[i] == result->crtc_id)
        {
            result->crtc_index = i;
            break;
        }
    }

    if (false)
    { // this are for vertical sync
        //int refresh_rate = connector->modes[0].vrefresh;
        drmModeCrtcPtr crtc = drmModeGetCrtc(result->fd, encoder->crtc_id);
        drmModeSetCrtc(result->fd, crtc->crtc_id, crtc->buffer_id, crtc->x, crtc->y, &result->connector_id, 1, &crtc->mode);
        drmModeFreeCrtc(crtc);
    }

    drmModeFreeEncoder(encoder);
    drmModeFreeConnector(connector);
    drmModeFreeResources(resources);

    return result;

    
}

void go2_display_destroy(go2_display_t *display)
{
    close(display->fd);
    free(display);
}

int go2_display_width_get(go2_display_t *display)
{
    return display->width;
}

int go2_display_height_get(go2_display_t *display)
{
    return display->height;
}

static void go2_page_flip_handler(int, unsigned int, unsigned int, unsigned int,
                                  void *data)
{
    *static_cast<bool *>(data) = false;
}

void go2_display_present(go2_display_t *display, go2_frame_buffer_t *frame_buffer)
{
    if (!display->modeset_complete)
    {
        int ret = drmModeSetCrtc(display->fd, display->crtc_id, frame_buffer->fb_id,
                                 0, 0, &display->connector_id, 1, &display->mode);
        if (ret)
            logger.log(Logger::ERR, "drmModeSetCrtc failed: %s\n", strerror(errno));
        else
            display->modeset_complete = true;
        return;
    }

    if (display->page_flip_disabled)
    {
        int ret = drmModeSetCrtc(display->fd, display->crtc_id, frame_buffer->fb_id,
                                 0, 0, &display->connector_id, 1, &display->mode);
        if (ret)
            logger.log(Logger::ERR, "drmModeSetCrtc fallback failed: %s\n", strerror(errno));
        return;
    }

    bool waiting_for_flip = true;
    if (drmModePageFlip(display->fd, display->crtc_id, frame_buffer->fb_id,
                        DRM_MODE_PAGE_FLIP_EVENT, &waiting_for_flip) != 0)
    {
        // Some older vendor DRM drivers do not expose page flips reliably.
        // Preserve compatibility by falling back to the historical modeset.
        logger.log(Logger::WARN, "drmModePageFlip failed; using SetCrtc fallback: %s\n",
                   strerror(errno));
        display->page_flip_disabled = true;
        int ret = drmModeSetCrtc(display->fd, display->crtc_id, frame_buffer->fb_id,
                                 0, 0, &display->connector_id, 1, &display->mode);
        if (ret)
            logger.log(Logger::ERR, "drmModeSetCrtc fallback failed: %s\n", strerror(errno));
        return;
    }

    drmEventContext event = {};
    event.version = DRM_EVENT_CONTEXT_VERSION;
    event.page_flip_handler = go2_page_flip_handler;
    while (waiting_for_flip)
    {
        pollfd descriptor = {display->fd, POLLIN, 0};
        int ready = poll(&descriptor, 1, 1000);
        if (ready > 0 && (descriptor.revents & POLLIN))
            drmHandleEvent(display->fd, &event);
        else if (ready < 0 && errno == EINTR)
            continue;
        else if (ready == 0)
        {
            // Do not recycle the previous scanout buffer until the kernel has
            // confirmed the flip. Keep waiting; log once per delayed flip.
            logger.log(Logger::WARN, "DRM page flip is taking longer than one second\n");
            continue;
        }
        else
        {
            logger.log(Logger::ERR, "error waiting for DRM page flip: %s\n",
                       strerror(errno));
            break;
        }
    }
}

static bool go2_plane_supports_format(const drmModePlane *plane, uint32_t format)
{
    for (uint32_t i = 0; i < plane->count_formats; ++i)
        if (plane->formats[i] == format)
            return true;
    return false;
}

static uint64_t go2_plane_type(go2_display_t *display, uint32_t plane_id)
{
    drmModeObjectProperties *properties = drmModeObjectGetProperties(
        display->fd, plane_id, DRM_MODE_OBJECT_PLANE);
    if (!properties)
        return UINT64_MAX;
    uint64_t type = UINT64_MAX;
    for (uint32_t i = 0; i < properties->count_props; ++i)
    {
        drmModePropertyRes *property = drmModeGetProperty(display->fd,
                                                          properties->props[i]);
        if (property && strcmp(property->name, "type") == 0)
            type = properties->prop_values[i];
        if (property)
            drmModeFreeProperty(property);
        if (type != UINT64_MAX)
            break;
    }
    drmModeFreeObjectProperties(properties);
    return type;
}

static uint32_t go2_find_direct_plane(go2_display_t *display, uint32_t format)
{
    if (display->crtc_index < 0)
        return 0;

    drmSetClientCap(display->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    drmModePlaneRes *resources = drmModeGetPlaneResources(display->fd);
    if (!resources)
        return 0;

    uint32_t result = 0;
    uint32_t fallback = 0;
    const uint32_t crtc_mask = 1U << display->crtc_index;
    for (uint32_t i = 0; i < resources->count_planes; ++i)
    {
        drmModePlane *plane = drmModeGetPlane(display->fd, resources->planes[i]);
        if (!plane)
            continue;
        if ((plane->possible_crtcs & crtc_mask) &&
            go2_plane_supports_format(plane, format))
        {
            // Kernel plane type values are overlay=0, primary=1, cursor=2.
            // Prefer an overlay so the modeset framebuffer can remain as the
            // black background and as the seamless RGA fallback.
            const uint64_t type = go2_plane_type(display, plane->plane_id);
            if (type == 0)
                result = plane->plane_id;
            else if (type != 2 && !fallback)
                fallback = plane->plane_id;
        }
        drmModeFreePlane(plane);
        if (result)
            break;
    }
    drmModeFreePlaneResources(resources);
    return result ? result : fallback;
}

static uint64_t go2_rotation_property_value(go2_rotation_t rotation)
{
    switch (rotation)
    {
    case GO2_ROTATION_DEGREES_0: return DRM_MODE_ROTATE_0;
    case GO2_ROTATION_DEGREES_90: return DRM_MODE_ROTATE_90;
    case GO2_ROTATION_DEGREES_180: return DRM_MODE_ROTATE_180;
    case GO2_ROTATION_DEGREES_270: return DRM_MODE_ROTATE_270;
    case GO2_ROTATION_HORIZONTAL: return DRM_MODE_ROTATE_0 | DRM_MODE_REFLECT_X;
    case GO2_ROTATION_VERTICAL: return DRM_MODE_ROTATE_0 | DRM_MODE_REFLECT_Y;
    default: return DRM_MODE_ROTATE_0;
    }
}

static bool go2_set_plane_rotation(go2_display_t *display, uint32_t plane_id,
                                   go2_rotation_t rotation)
{
    drmModeObjectProperties *properties = drmModeObjectGetProperties(
        display->fd, plane_id, DRM_MODE_OBJECT_PLANE);
    if (!properties)
        return rotation == GO2_ROTATION_DEGREES_0;

    bool found = false;
    bool success = rotation == GO2_ROTATION_DEGREES_0;
    for (uint32_t i = 0; i < properties->count_props; ++i)
    {
        drmModePropertyRes *property = drmModeGetProperty(display->fd,
                                                          properties->props[i]);
        if (property && strcmp(property->name, "rotation") == 0)
        {
            found = true;
            success = drmModeObjectSetProperty(display->fd, plane_id,
                                                DRM_MODE_OBJECT_PLANE,
                                                property->prop_id,
                                                go2_rotation_property_value(rotation)) == 0;
        }
        if (property)
            drmModeFreeProperty(property);
        if (found)
            break;
    }
    drmModeFreeObjectProperties(properties);
    return success;
}

bool go2_display_present_surface(go2_display_t *display, go2_surface_t *surface,
                                 int srcX, int srcY, int srcWidth, int srcHeight,
                                 int dstX, int dstY, int dstWidth, int dstHeight,
                                 go2_rotation_t rotation)
{
    if (!display || !surface || display->direct_plane_disabled ||
        !display->modeset_complete || srcWidth <= 0 || srcHeight <= 0 ||
        dstWidth <= 0 || dstHeight <= 0)
        return false;

    if (!display->direct_plane_id || display->direct_plane_format != surface->format)
    {
        display->direct_plane_id = go2_find_direct_plane(display, surface->format);
        display->direct_plane_format = surface->format;
        if (!display->direct_plane_id)
        {
            if (!display->direct_plane_logged)
                logger.log(Logger::WARN,
                           "DRM direct scanout unavailable for format %.4s; using RGA fallback.",
                           reinterpret_cast<const char *>(&surface->format));
            display->direct_plane_logged = true;
            return false;
        }
    }

    if (!surface->direct_fb_id)
    {
        const uint32_t handles[4] = {surface->gem_handle, 0, 0, 0};
        const uint32_t pitches[4] = {static_cast<uint32_t>(surface->stride), 0, 0, 0};
        const uint32_t offsets[4] = {0, 0, 0, 0};
        if (drmModeAddFB2(display->fd, surface->width, surface->height,
                          surface->format, handles, pitches, offsets,
                          &surface->direct_fb_id, 0) != 0)
            return false;
    }

    if (!go2_set_plane_rotation(display, display->direct_plane_id, rotation))
        return false;

    if (drmModeSetPlane(display->fd, display->direct_plane_id, display->crtc_id,
                        surface->direct_fb_id, 0, dstX, dstY, dstWidth, dstHeight,
                        static_cast<uint32_t>(srcX) << 16,
                        static_cast<uint32_t>(srcY) << 16,
                        static_cast<uint32_t>(srcWidth) << 16,
                        static_cast<uint32_t>(srcHeight) << 16) != 0)
    {
        logger.log(Logger::WARN,
                   "DRM direct scanout rejected by plane; using RGA fallback: %s",
                   strerror(errno));
        display->direct_plane_disabled = true;
        return false;
    }

    drmVBlank vblank = {};
    vblank.request.type = DRM_VBLANK_RELATIVE;
    vblank.request.sequence = 1;
    drmWaitVBlank(display->fd, &vblank);
    if (!display->direct_plane_logged)
        logger.log(Logger::INF, "DRM direct scanout enabled (zero-copy GBM to plane).");
    display->direct_plane_logged = true;
    return true;
}

void go2_display_direct_disable(go2_display_t *display)
{
    if (!display || !display->direct_plane_id)
        return;
    drmModeSetPlane(display->fd, display->direct_plane_id, display->crtc_id,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

const char *BACKLIGHT_BRIGHTNESS_NAME = "/sys/class/backlight/backlight/brightness";
const char *BACKLIGHT_BRIGHTNESS_MAX_NAME = "/sys/class/backlight/backlight/max_brightness";
#define BACKLIGHT_BUFFER_SIZE (127)

uint32_t go2_display_backlight_get(go2_display_t *display)
{
    int fd;
    int max = 255;
    int value = 0;
    char buffer[BACKLIGHT_BUFFER_SIZE + 1];

    fd = open(BACKLIGHT_BRIGHTNESS_MAX_NAME, O_RDONLY);
    if (fd > 0)
    {
        memset(buffer, 0, BACKLIGHT_BUFFER_SIZE + 1);

        ssize_t count = read(fd, buffer, BACKLIGHT_BUFFER_SIZE);
        if (count > 0)
        {
            max = atoi(buffer);
        }

        close(fd);

        if (max == 0)
            return 0;
    }

    fd = open(BACKLIGHT_BRIGHTNESS_NAME, O_RDONLY);
    if (fd > 0)
    {
        memset(buffer, 0, BACKLIGHT_BUFFER_SIZE + 1);

        ssize_t count = read(fd, buffer, BACKLIGHT_BUFFER_SIZE);
        if (count > 0)
        {
            value = atoi(buffer);
        }

        close(fd);
    }

    float percent = value / (float)max * 100.0f;
    return (uint32_t)percent;
}

void go2_display_backlight_set(go2_display_t *display, uint32_t value)
{
    int fd;
    int max = 255;
    char buffer[BACKLIGHT_BUFFER_SIZE + 1];

    if (value > 100)
        value = 100;

    fd = open(BACKLIGHT_BRIGHTNESS_MAX_NAME, O_RDONLY);
    if (fd > 0)
    {
        memset(buffer, 0, BACKLIGHT_BUFFER_SIZE + 1);

        ssize_t count = read(fd, buffer, BACKLIGHT_BUFFER_SIZE);
        if (count > 0)
        {
            max = atoi(buffer);
        }

        close(fd);

        if (max == 0)
            return;
    }

    fd = open(BACKLIGHT_BRIGHTNESS_NAME, O_WRONLY);
    if (fd > 0)
    {
        float percent = value / 100.0f * (float)max;
        sprintf(buffer, "%d\n", (uint32_t)percent);

        logger.log(Logger::DEB, "backlight=%d, max=%d\n", (uint32_t)percent, max);

        ssize_t count = write(fd, buffer, strlen(buffer));
        if (count < 0)
        {
            logger.log(Logger::ERR,"go2_display_backlight_set write failed.\n");
        }

        close(fd);
    }
    else
    {
        logger.log(Logger::ERR,"go2_display_backlight_set open failed.\n");
    }
}

int go2_drm_format_get_bpp(uint32_t format)
{
    int result;

    switch (format)
    {
    case DRM_FORMAT_XRGB4444:
    case DRM_FORMAT_XBGR4444:
    case DRM_FORMAT_RGBX4444:
    case DRM_FORMAT_BGRX4444:

    case DRM_FORMAT_ARGB4444:
    case DRM_FORMAT_ABGR4444:
    case DRM_FORMAT_RGBA4444:
    case DRM_FORMAT_BGRA4444:

    case DRM_FORMAT_XRGB1555:
    case DRM_FORMAT_XBGR1555:
    case DRM_FORMAT_RGBX5551:
    case DRM_FORMAT_BGRX5551:

    case DRM_FORMAT_ARGB1555:
    case DRM_FORMAT_ABGR1555:
    case DRM_FORMAT_RGBA5551:
    case DRM_FORMAT_BGRA5551:

    case DRM_FORMAT_RGB565:
    case DRM_FORMAT_BGR565:
        result = 16;
        break;

    case DRM_FORMAT_RGB888:
    case DRM_FORMAT_BGR888:
        result = 24;
        break;

    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_RGBX8888:
    case DRM_FORMAT_BGRX8888:

    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_RGBA8888:
    case DRM_FORMAT_BGRA8888:

    case DRM_FORMAT_XRGB2101010:
    case DRM_FORMAT_XBGR2101010:
    case DRM_FORMAT_RGBX1010102:
    case DRM_FORMAT_BGRX1010102:

    case DRM_FORMAT_ARGB2101010:
    case DRM_FORMAT_ABGR2101010:
    case DRM_FORMAT_RGBA1010102:
    case DRM_FORMAT_BGRA1010102:
        result = 32;
        break;

    default:
        logger.log(Logger::ERR,"unhandled DRM FORMAT.\n");
        result = 0;
    }

    return result;
}

go2_surface_t *go2_surface_create(go2_display_t *display, int width, int height, uint32_t format)
{
    go2_surface_t *result = (go2_surface_t *)malloc(sizeof(*result));
    if (!result)
    {
        logger.log(Logger::ERR,"malloc failed.\n");
        free(result);
        return NULL;
    }

    memset(result, 0, sizeof(*result));

    struct drm_mode_create_dumb args = {0};
    args.width = width;
    args.height = height;
    args.bpp = go2_drm_format_get_bpp(format);
    args.flags = 0;

    int io = drmIoctl(display->fd, DRM_IOCTL_MODE_CREATE_DUMB, &args);
    if (io < 0)
    {
        logger.log(Logger::ERR,"DRM_IOCTL_MODE_CREATE_DUMB failed.\n");
        free(result);
        return NULL;
    }

    result->display = display;
    result->gem_handle = args.handle;
    result->size = args.size;
    result->width = width;
    result->height = height;
    result->stride = args.pitch;
    result->format = format;

    return result;
    /*
    out:
        free(result);
        return NULL;*/
}

void go2_surface_destroy(go2_surface_t *surface)
{
    if (surface->direct_fb_id)
        drmModeRmFB(surface->display->fd, surface->direct_fb_id);
    if (surface->prime_fd > 0)
        close(surface->prime_fd);
    struct drm_mode_destroy_dumb args = {0};
    args.handle = surface->gem_handle;

    int io = drmIoctl(surface->display->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &args);
    if (io < 0)
    {
        logger.log(Logger::ERR,"DRM_IOCTL_MODE_DESTROY_DUMB failed.\n");
    }

    free(surface);
}

int go2_surface_width_get(go2_surface_t *surface)
{
    return surface->width;
}

int go2_surface_height_get(go2_surface_t *surface)
{
    return surface->height;
}

uint32_t go2_surface_format_get(go2_surface_t *surface)
{
    return surface->format;
}

int go2_surface_stride_get(go2_surface_t *surface)
{
    return surface->stride;
}

go2_display_t *go2_surface_display_get(go2_surface_t *surface)
{
    return surface->display;
}

int go2_surface_prime_fd(go2_surface_t *surface)
{
    if (surface->prime_fd <= 0)
    {
        int io = drmPrimeHandleToFD(surface->display->fd, surface->gem_handle, DRM_RDWR | DRM_CLOEXEC, &surface->prime_fd);
        if (io < 0)
        {
            logger.log(Logger::ERR,"drmPrimeHandleToFD failed.\n");
            surface->prime_fd = 0;
            return 0;
        }
    }

    return surface->prime_fd;
}

void *go2_surface_map(go2_surface_t *surface)
{
    if (surface->is_mapped)
        return surface->map;

    int prime_fd = go2_surface_prime_fd(surface);
    // printf("mapping surface with size:%d", surface->size);
    surface->map = (uint8_t *)mmap(NULL, surface->size, PROT_READ | PROT_WRITE, MAP_SHARED, prime_fd, 0);
    if (surface->map == MAP_FAILED)
    {
        logger.log(Logger::ERR,"mmap failed.\n");
        return NULL;
    }

    surface->is_mapped = true;
    return surface->map;
}

void go2_surface_unmap(go2_surface_t *surface)
{
    if (surface->is_mapped)
    {
        munmap(surface->map, surface->size);

        surface->is_mapped = false;
        surface->map = NULL;
    }
}

static uint32_t go2_rkformat_get(uint32_t drm_fourcc)
{
    switch (drm_fourcc)
    {
    case DRM_FORMAT_RGBA8888:
        //logger.log(Logger::DEB,"1.go2_rkformat: RK_FORMAT_RGBA_8888");
        return RK_FORMAT_RGBA_8888;

    case DRM_FORMAT_RGBX8888:
        //logger.log(Logger::DEB,"2.go2_rkformat: RK_FORMAT_RGBX_8888");
        return RK_FORMAT_RGBX_8888;

    case DRM_FORMAT_RGB888:
        //logger.log(Logger::DEB,"3.go2_rkformat: RK_FORMAT_RGB_888");
        return RK_FORMAT_RGB_888;

    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_XRGB8888:
        //logger.log(Logger::DEB,"4.go2_rkformat: RK_FORMAT_RGBX_8888");
        return RK_FORMAT_BGRA_8888;
        //return RK_FORMAT_RGBX_8888; ? is this better here?

    case DRM_FORMAT_RGB565:
        //logger.log(Logger::DEB,"5.go2_rkformat: RK_FORMAT_RGB_565");
        return RK_FORMAT_RGB_565;

    case DRM_FORMAT_RGBA5551:
        //logger.log(Logger::DEB,"6.go2_rkformat: RK_FORMAT_RGBA_5551");
        return RK_FORMAT_RGBA_5551;

    case DRM_FORMAT_RGBA4444:
        //logger.log(Logger::DEB,"7.go2_rkformat: RK_FORMAT_RGBA_4444");
        return RK_FORMAT_RGBA_4444;

    case DRM_FORMAT_BGR888:
        //logger.log(Logger::DEB,"8.go2_rkformat: RK_FORMAT_BGR_888");
        return RK_FORMAT_BGR_888;

    default:
        printf("RKFORMAT not supported. ");
        printf("drm_fourcc=%c%c%c%c\n", drm_fourcc & 0xff, drm_fourcc >> 8 & 0xff, drm_fourcc >> 16 & 0xff, drm_fourcc >> 24);
        return 0;
    }
}

static void go2_surface_blit_internal(go2_surface_t *srcSurface, int srcX, int srcY,
                      int srcWidth, int srcHeight, go2_surface_t *dstSurface,
                      int dstX, int dstY, int dstWidth, int dstHeight,
                      go2_rotation_t rotation, bool alpha_blend)
{
    rga_info_t dst = {0};
    dst.fd = go2_surface_prime_fd(dstSurface);
    dst.mmuFlag = 1;
    dst.rect.xoffset = dstX;
    dst.rect.yoffset = dstY;
    dst.rect.width = dstWidth;
    dst.rect.height = dstHeight;
    dst.rect.wstride = dstSurface->stride / (go2_drm_format_get_bpp(dstSurface->format) / 8);
    dst.rect.hstride = dstSurface->height;
    dst.rect.format = go2_rkformat_get(dstSurface->format);

    rga_info_t src = {0};
    src.fd = go2_surface_prime_fd(srcSurface);
    src.mmuFlag = 1;


    switch (rotation)
    {
    case GO2_ROTATION_DEGREES_0:
        src.rotation = 0;
        break;

    case GO2_ROTATION_DEGREES_90:
        src.rotation = HAL_TRANSFORM_ROT_90;
        break;

    case GO2_ROTATION_DEGREES_180:
        src.rotation = HAL_TRANSFORM_ROT_180;
        break;

    case GO2_ROTATION_DEGREES_270:
        src.rotation = HAL_TRANSFORM_ROT_270;
        break;
    case GO2_ROTATION_HORIZONTAL:
        src.rotation = HAL_TRANSFORM_FLIP_H;
        break;    
    case GO2_ROTATION_VERTICAL:
        src.rotation = HAL_TRANSFORM_FLIP_H;
        break;  
    



    default:
        logger.log(Logger::ERR,"rotation not supported.\n");
        return;
    }

    src.rect.xoffset = srcX;
    src.rect.yoffset = srcY;
    src.rect.width = srcWidth;
    src.rect.height = srcHeight;
    src.rect.wstride = srcSurface->stride / (go2_drm_format_get_bpp(srcSurface->format) / 8);
    src.rect.hstride = srcSurface->height;
    src.rect.format = go2_rkformat_get(srcSurface->format);
    if (alpha_blend)
        src.blend = 0xff0105; // Porter-Duff source-over with per-pixel alpha.

#if 0
    enum
    {
        CATROM    = 0x0,
        MITCHELL  = 0x1,
        HERMITE   = 0x2,
        B_SPLINE  = 0x3,
    };  /*bicubic coefficient*/
#endif
    src.scale_mode = 4;

    int ret = c_RkRgaBlit(&src, &dst, NULL);
    if (ret)
    {
        logger.log(Logger::ERR,"c_RkRgaBlit failed.\n");
    }
}

void go2_surface_blit(go2_surface_t *srcSurface, int srcX, int srcY,
                      int srcWidth, int srcHeight, go2_surface_t *dstSurface,
                      int dstX, int dstY, int dstWidth, int dstHeight,
                      go2_rotation_t rotation)
{
    go2_surface_blit_internal(srcSurface, srcX, srcY, srcWidth, srcHeight,
                              dstSurface, dstX, dstY, dstWidth, dstHeight,
                              rotation, false);
}

void go2_surface_blit_alpha(go2_surface_t *srcSurface, int srcX, int srcY,
                            int srcWidth, int srcHeight, go2_surface_t *dstSurface,
                            int dstX, int dstY, int dstWidth, int dstHeight,
                            go2_rotation_t rotation)
{
    go2_surface_blit_internal(srcSurface, srcX, srcY, srcWidth, srcHeight,
                              dstSurface, dstX, dstY, dstWidth, dstHeight,
                              rotation, true);
}

int go2_surface_save_as_png(go2_surface_t *surface, const char *filename)
{
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep *row_pointers = NULL;

    png_byte color_type = 0;
    png_byte bit_depth = 0;
    switch (surface->format)
    {
    case DRM_FORMAT_RGBA8888:
        color_type = PNG_COLOR_TYPE_RGBA;
        bit_depth = 8;
        break;

    case DRM_FORMAT_RGB888:
        color_type = PNG_COLOR_TYPE_RGB;
        bit_depth = 8;
        break;

    case DRM_FORMAT_RGBX8888:
    case DRM_FORMAT_RGB565:
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_RGBA5551:
    case DRM_FORMAT_RGBA4444:
    case DRM_FORMAT_BGR888:

    default:
        logger.log(Logger::ERR,"The image format is not supported.\n");
        return -2;
    }

    // based on http://zarb.org/~gc/html/libpng.html

    /* create file */
    FILE *fp = fopen(filename, "wb");
    if (!fp)
    {
        logger.log(Logger::ERR,"fopen failed. filename='%s'\n", filename);
        return -1;
    }

    /* initialize stuff */
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!png_ptr)
    {
        logger.log(Logger::ERR,"png_create_write_struct failed.\n");
        fclose(fp);
        return -1;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        logger.log(Logger::ERR,"png_create_info_struct failed.\n");
        fclose(fp);
        return -1;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        logger.log(Logger::ERR,"init_io failed.\n");
        if (info_ptr)
            png_destroy_info_struct(png_ptr, &info_ptr);

        if (png_ptr)
            png_destroy_write_struct(&png_ptr, (png_infopp)NULL);

        if (row_pointers)
            free(row_pointers);

        if (fp)
            fclose(fp);

        return -1;
    }

    png_init_io(png_ptr, fp);

    /* write header */
    if (setjmp(png_jmpbuf(png_ptr)))
    {
        logger.log(Logger::ERR,"write header failed.\n");
        if (info_ptr)
            png_destroy_info_struct(png_ptr, &info_ptr);

        if (png_ptr)
            png_destroy_write_struct(&png_ptr, (png_infopp)NULL);

        if (row_pointers)
            free(row_pointers);

        if (fp)
            fclose(fp);

        return -1;
    }

    png_set_IHDR(png_ptr, info_ptr, surface->width, surface->height,
                 bit_depth, color_type, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);

    /* write bytes */
    png_bytep src = (png_bytep)go2_surface_map(surface);
    row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * surface->height);
    for (int y = 0; y < surface->height; ++y)
    {
        row_pointers[y] = src + (surface->stride * y);
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        logger.log(Logger::ERR,"writing bytes failed.\n");
        if (info_ptr)
            png_destroy_info_struct(png_ptr, &info_ptr);

        if (png_ptr)
            png_destroy_write_struct(&png_ptr, (png_infopp)NULL);

        if (row_pointers)
            free(row_pointers);

        if (fp)
            fclose(fp);

        return -1;
    }

    png_write_image(png_ptr, row_pointers);

    /* end write */
    if (setjmp(png_jmpbuf(png_ptr)))
    {
        logger.log(Logger::ERR,"end of write failed.\n");
        if (info_ptr)
            png_destroy_info_struct(png_ptr, &info_ptr);

        if (png_ptr)
            png_destroy_write_struct(&png_ptr, (png_infopp)NULL);

        if (row_pointers)
            free(row_pointers);

        if (fp)
            fclose(fp);

        return -1;
    }

    png_write_end(png_ptr, NULL);

    /* cleanup heap allocation */
    free(row_pointers);

    fclose(fp);
    return 0;
}

go2_frame_buffer_t *go2_frame_buffer_create(go2_surface_t *surface)
{
    go2_frame_buffer_t *result = (go2_frame_buffer_t *)malloc(sizeof(*result));
    if (!result)
    {
        logger.log(Logger::ERR,"malloc failed.\n");
        return NULL;
    }

    memset(result, 0, sizeof(*result));

    result->surface = surface;

    const uint32_t handles[4] = {surface->gem_handle, 0, 0, 0};
    const uint32_t pitches[4] = {(uint32_t)surface->stride, 0, 0, 0};
    const uint32_t offsets[4] = {0, 0, 0, 0};

    int ret = drmModeAddFB2(surface->display->fd,
                            surface->width,
                            surface->height,
                            surface->format,
                            handles,
                            pitches,
                            offsets,
                            &result->fb_id,
                            0);
    if (ret)
    {
        logger.log(Logger::ERR,"drmModeAddFB2 failed.\n");
        free(result);
        return NULL;
    }

    return result;
}

void go2_frame_buffer_destroy(go2_frame_buffer_t *frame_buffer)
{
    int ret = drmModeRmFB(frame_buffer->surface->display->fd, frame_buffer->fb_id);
    if (ret)
    {
        logger.log(Logger::ERR,"drmModeRmFB failed.\n");
    }

    free(frame_buffer);
}

go2_surface_t *go2_frame_buffer_surface_get(go2_frame_buffer_t *frame_buffer)
{
    return frame_buffer->surface;
}

#if 0
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
#endif

#define BUFFER_COUNT (2)

static void *go2_presenter_renderloop(void *arg)
{
    go2_presenter_t *presenter = (go2_presenter_t *)arg;
    go2_frame_buffer_t *prevFrameBuffer = NULL;

    presenter->terminating = false;
    while (!presenter->terminating)
    {
        sem_wait(&presenter->usedSem);
        if (presenter->terminating)
            break;

        pthread_mutex_lock(&presenter->queueMutex);

        if (go2_queue_count_get(presenter->usedFrameBuffers) < 1)
        {
            logger.log(Logger::ERR,"no framebuffer available.\n");
            exit(1);
        }

        go2_frame_buffer_t *dstFrameBuffer = (go2_frame_buffer_t *)go2_queue_pop(presenter->usedFrameBuffers);

        pthread_mutex_unlock(&presenter->queueMutex);

        go2_display_present(presenter->display, dstFrameBuffer);

        if (prevFrameBuffer)
        {
            pthread_mutex_lock(&presenter->queueMutex);
            go2_queue_push(presenter->freeFrameBuffers, prevFrameBuffer);
            pthread_mutex_unlock(&presenter->queueMutex);

            sem_post(&presenter->freeSem);
        }

        prevFrameBuffer = dstFrameBuffer;
    }

    // The currently scanned-out buffer is owned by the render loop. Return it
    // before shutdown so presenter destruction releases every DRM framebuffer.
    if (prevFrameBuffer)
    {
        pthread_mutex_lock(&presenter->queueMutex);
        go2_queue_push(presenter->freeFrameBuffers, prevFrameBuffer);
        pthread_mutex_unlock(&presenter->queueMutex);
        sem_post(&presenter->freeSem);
    }

    return NULL;
}

go2_presenter_t *go2_presenter_create(go2_display_t *display, uint32_t format, uint32_t background_color)
{
    go2_presenter_t *result = (go2_presenter_t *)malloc(sizeof(*result));
    if (!result)
    {
        logger.log(Logger::ERR,"malloc failed.\n");
        return NULL;
    }

    memset(result, 0, sizeof(*result));

    result->display = display;
    result->format = format;
    result->background_color = background_color;
    result->freeFrameBuffers = go2_queue_create(BUFFER_COUNT);
    result->usedFrameBuffers = go2_queue_create(BUFFER_COUNT);

    int width = go2_display_width_get(display);
    int height = go2_display_height_get(display);

    for (int i = 0; i < BUFFER_COUNT; ++i)
    {
        go2_surface_t *surface = go2_surface_create(display, width, height, format);
        go2_frame_buffer_t *frameBuffer = go2_frame_buffer_create(surface);

        go2_queue_push(result->freeFrameBuffers, frameBuffer);
    }

    sem_init(&result->usedSem, 0, 0);
    sem_init(&result->freeSem, 0, BUFFER_COUNT);

    pthread_mutex_init(&result->queueMutex, NULL);

    pthread_create(&result->renderThread, NULL, go2_presenter_renderloop, result);

    return result;
}

void go2_presenter_destroy(go2_presenter_t *presenter)
{
    presenter->terminating = true;
    sem_post(&presenter->usedSem);

    pthread_join(presenter->renderThread, NULL);
    pthread_mutex_destroy(&presenter->queueMutex);

    sem_destroy(&presenter->freeSem);
    sem_destroy(&presenter->usedSem);

    while (go2_queue_count_get(presenter->usedFrameBuffers) > 0)
    {
        go2_frame_buffer_t *frameBuffer = (go2_frame_buffer_t *)go2_queue_pop(presenter->usedFrameBuffers);

        go2_surface_t *surface = frameBuffer->surface;

        go2_frame_buffer_destroy(frameBuffer);
        go2_surface_destroy(surface);
    }

    while (go2_queue_count_get(presenter->freeFrameBuffers) > 0)
    {
        go2_frame_buffer_t *frameBuffer = (go2_frame_buffer_t *)go2_queue_pop(presenter->freeFrameBuffers);

        go2_surface_t *surface = frameBuffer->surface;

        go2_frame_buffer_destroy(frameBuffer);
        go2_surface_destroy(surface);
    }

    free(presenter);
}

void go2_presenter_post(go2_presenter_t *presenter, go2_surface_t *surface, int srcX, int srcY, int srcWidth, int srcHeight, int dstX, int dstY, int dstWidth, int dstHeight, go2_rotation_t rotation)
{
    // During fast-forward the core must never be paced by the display queue.
    // If all scanout buffers are busy, discard this presentation attempt and
    // let retro_run() continue immediately. Normal gameplay remains blocking.
    if (input_ffwd_requested) {
        if (sem_trywait(&presenter->freeSem) != 0)
            return;
    } else {
        while (sem_wait(&presenter->freeSem) != 0 && errno == EINTR) {
        }
    }
    pthread_mutex_lock(&presenter->queueMutex);
    go2_frame_buffer_t *dstFrameBuffer =
        (go2_frame_buffer_t *)go2_queue_pop(presenter->freeFrameBuffers);
    pthread_mutex_unlock(&presenter->queueMutex);
    if (!dstFrameBuffer) {
        logger.log(Logger::ERR, "free framebuffer semaphore/queue mismatch.\n");
        return;
    }

    go2_surface_t *dstSurface = go2_frame_buffer_surface_get(dstFrameBuffer);

   
    go2_surface_blit(surface, srcX, srcY, srcWidth, srcHeight, dstSurface, dstX, dstY, dstWidth, dstHeight, rotation);

    int push_result = go2_queue_push(presenter->usedFrameBuffers, dstFrameBuffer);
    if (push_result != 0)
    {
        logger.log(Logger::ERR,"queue push failed.\n");
        exit(1);
    }

    sem_post(&presenter->usedSem);
}

bool go2_presenter_post_direct(go2_presenter_t *presenter, go2_surface_t *surface,
                               int srcX, int srcY, int srcWidth, int srcHeight,
                               int dstX, int dstY, int dstWidth, int dstHeight,
                               go2_rotation_t rotation)
{
    if (!presenter)
        return false;
    return go2_display_present_surface(presenter->display, surface, srcX, srcY,
                                       srcWidth, srcHeight, dstX, dstY,
                                       dstWidth, dstHeight, rotation);
}

void go2_presenter_direct_disable(go2_presenter_t *presenter)
{
    if (presenter)
        go2_display_direct_disable(presenter->display);
}

void go2_presenter_black(go2_presenter_t *presenter, int dstX, int dstY, int dstWidth, int dstHeight, go2_rotation_t rotation)
{
    sem_wait(&presenter->freeSem);
    pthread_mutex_lock(&presenter->queueMutex);
    go2_frame_buffer_t *dstFrameBuffer =
        (go2_frame_buffer_t *)go2_queue_pop(presenter->freeFrameBuffers);
    pthread_mutex_unlock(&presenter->queueMutex);
    if (!dstFrameBuffer) {
        logger.log(Logger::ERR, "free framebuffer semaphore/queue mismatch.\n");
        return;
    }

    

    int push_result = go2_queue_push(presenter->usedFrameBuffers, dstFrameBuffer);
    if (push_result != 0)
    {
        logger.log(Logger::ERR,"queue push failed.\n");
        exit(1);
    }

    sem_post(&presenter->usedSem);
}



int mx = 0;

void blit_surface_status(go2_presenter_t *presenter, go2_surface_t *source_surface, go2_surface_t *dest_surface, int dest_width, int dest_height, go2_rotation_t rotation, STATUS_POSITION position, bool isWideScreen)
{

    double dest_x = 0;
    double dest_y = 0;
    double dest_width_scaled = source_surface->width;
    double dest_height_scaled = source_surface->height;

    double max_width = presenter->display->width;
    double max_height = presenter->display->height;
    double coor_x_0 = 0;
    double coor_y_0 = 0;

    double scarto_w = presenter->display->width - dest_width;
    double scarto_h = presenter->display->height - dest_height;


    
   
    if (isWideScreen) 
    {
        max_width = dest_width;   // presenter->display->width;
        max_height = dest_height; // presenter->display->height-390;
        coor_x_0 = (presenter->display->width - dest_width) / 2;
        coor_y_0 = (presenter->display->height - max_height) / 2; // 195;
    }
    // screen aspect ratio
    /*double  display_aspec= dest_width/dest_height;
      double current_ar = 1.33333;

    max_height= max_height;
                    max_width= (max_height * current_ar /display_aspec);
                    coor_x_0 = (presenter->display->width / 2) - (max_width / 2);
                    coor_y_0 = 0;*/

    // int dest2HeightScaled = dest2Height * scaleFactor * 0.75; // scale height by 0.75 to maintain 4:3 aspect ratio

    // Existing Anbernic devices in this group expose a landscape scanout
    // despite their logical rotation. The Miniloong Pocket 1 is different:
    // its DRM mode is physically portrait and its platform transform has
    // already been composed into rotation. Use the generic rotated-blit
    // layout below so notification surfaces keep their proportions.
    if (hasDeviceRotatedScreen() && !isMiniloongPocket1())
    {
        // Scale the surface dimensions based on the display resolution
        if ((rotation== GO2_ROTATION_DEGREES_0 || rotation== GO2_ROTATION_DEGREES_180)){
            // Tate is off
            dest_width_scaled = source_surface->width * (max_width / 480);
            dest_height_scaled = source_surface->height * (max_height / 320);

        }  else{
            // in tate mode we need to rotate in the images
            dest_height_scaled = source_surface->width * (max_width / 480);
            dest_width_scaled = source_surface->height * (max_height / 320);

        }
        // Double the size if it's too small
        //dest_width_scaled *= 1.5;
        //dest_height_scaled *= 1.5;

        if (position == BUTTOM_LEFT)
        {
            dest_x = coor_x_0;
            dest_y = max_height - dest_height_scaled;
        }
        else if (position == BUTTOM_RIGHT)
        {
            dest_x = max_width - dest_width_scaled;
            if (isRG503()){
                dest_x =max_width -dest_width_scaled /2;
            }
            dest_y = max_height - dest_height_scaled;
        }
        else if (position == BUTTOM_CENTER) // new
        {
            dest_x = (max_width - dest_width_scaled) / 2;
            if (isRG503()){
                dest_x = presenter->display->width/2 - dest_width_scaled/2;
            }
            dest_y = max_height - dest_height_scaled;
        }
        else if (position == TOP_RIGHT)
        {
            dest_x = max_width - dest_width_scaled;
            if (isRG503()){
                dest_x =max_width + dest_width_scaled;
            }
            dest_y = coor_y_0;
        }
        else if (position == TOP_LEFT)
        {
            dest_x = coor_x_0;
            dest_y = coor_y_0;
        }
        else if (position == FULL)
        {
            dest_x = coor_x_0;
            dest_y = coor_y_0;
            dest_width_scaled = max_width;
            dest_height_scaled = max_height;
        }
    }
    else 
    {
        
        // Scale the surface dimensions based on the display resolution and rotation
        if (rotation== GO2_ROTATION_DEGREES_270 || rotation== GO2_ROTATION_DEGREES_90){
            // Tate is off
            dest_width_scaled = source_surface->height * (max_height / 480);
            dest_height_scaled = source_surface->width * (max_width / 320);
        }  else{
            // in tate mode we need to rotate in the images
            dest_height_scaled = source_surface->height * (max_height / 480);
            dest_width_scaled = source_surface->width * (max_width / 320);
        }

        /*printf("presenter->display->height:%u  dest_height:%d coor_y_0:%f dest_height_scaled:%f\n",
            presenter->display->height,
            dest_height,
            coor_y_0,
            dest_height_scaled);*/
        if (position == BUTTOM_LEFT)
        {
            dest_x = max_width - dest_width_scaled;
            dest_y = max_height - dest_height_scaled + (scarto_h / 2);
        }
        else if (position == BUTTOM_RIGHT)
        {
            dest_x = max_width - dest_width_scaled + (scarto_w / 2);
            dest_y = coor_y_0;
        }
        else if (position == BUTTOM_CENTER)//new
        {
            dest_x = (max_width - dest_width_scaled)+scarto_w;// / 2 + (scarto_w / 2);
            dest_y =  presenter->display->height/2 - dest_height_scaled/2;/// + (scarto_h / 2);
        }
        else if (position == TOP_RIGHT)
        {
            dest_x = coor_x_0;
            dest_y = coor_y_0;
        }
        else if (position == TOP_LEFT)
        {
            dest_x = coor_x_0;
            dest_y = max_height - dest_height_scaled + (scarto_h / 2);
        }
        else if (position == FULL)
        {
            dest_x = coor_x_0;               // 0+(scarto_w/4)+1;//coor_x_0+(scarto_w/2);
            dest_y = coor_y_0;               // 0+(scarto_h/4)+1;//coor_y_0+(scarto_h/2);
            dest_width_scaled = max_width;   //+(scarto_w/2)-2;
            dest_height_scaled = max_height; //+(scarto_h/2)-2;
        }
    }

    // Blit the surface to the destination surface
    if (source_surface != nullptr)
    {
        go2_surface_blit(source_surface, 0, 0, source_surface->width, source_surface->height, dest_surface, dest_x, dest_y, dest_width_scaled, dest_height_scaled, rotation);
    }
}






void go2_presenter_post_multiple(go2_presenter_t *presenter, go2_surface_t *surface1, go2_status_t *status_obj, int srcX, int srcY, int srcWidth, int srcHeight, int dstX, int dstY, int dstWidth, int dstHeight, go2_rotation_t rotation, go2_rotation_t blitRotation, bool isWideScreen)
{

    //printf("Name of rotation: %s\n", rotation_names[rotation]);
    //printf("Name of blitRotation: %s\n", rotation_names[blitRotation]);

    if (!presenter) {
        logger.log(Logger::ERR, "ERROR: presenter is NULL! Skipping presenter_post.");
        return;
    }
    // Keep the emulation thread non-blocking while fast-forwarding. This path
    // is used whenever status elements or screen decorations are composited.
    if (input_ffwd_requested) {
        if (sem_trywait(&presenter->freeSem) != 0)
            return;
    } else {
        while (sem_wait(&presenter->freeSem) != 0 && errno == EINTR) {
        }
    }

    pthread_mutex_lock(&presenter->queueMutex);

    go2_frame_buffer_t *dstFrameBuffer = (go2_frame_buffer_t *)go2_queue_pop(presenter->freeFrameBuffers);

    pthread_mutex_unlock(&presenter->queueMutex);

    if (!dstFrameBuffer)
    {
        logger.log(Logger::ERR, "free framebuffer semaphore/queue mismatch.\n");
        return;
    }

    go2_surface_t *dstSurface = go2_frame_buffer_surface_get(dstFrameBuffer);
    const bool rotated_canvas = rotation == GO2_ROTATION_DEGREES_90 ||
                                rotation == GO2_ROTATION_DEGREES_270;
    const int canvas_width = rotated_canvas ? dstSurface->height : dstSurface->width;
    const int canvas_height = rotated_canvas ? dstSurface->width : dstSurface->height;

    if (status_obj->clean_full) {
        void* dstPixels = go2_surface_map(dstSurface);
        if (dstPixels) {
            memset(dstPixels, 0, dstSurface->stride * dstSurface->height);
            go2_surface_unmap(dstSurface);
        }
        if (status_obj->show_decoration && status_obj->decoration &&
            status_obj->decoration->format != DRM_FORMAT_RGBA8888) {
            go2_surface_blit(status_obj->decoration, 0, 0,
                             status_obj->decoration->width, status_obj->decoration->height,
                             dstSurface, 0, 0, canvas_width, canvas_height,
                             rotation);
        }
    }

    // Full frontend pages are opaque. Do not blit the core underneath them:
    // besides wasting an RGA operation, menu dimensions may differ from the
    // core allocation (for example 320x240 over mGBA's 256x160 surface).
    if (!status_obj->show_full)
        go2_surface_blit(surface1, srcX, srcY, srcWidth, srcHeight,
                         dstSurface, dstX, dstY, dstWidth, dstHeight, rotation);

    // PNG bezels are true foreground overlays: their transparent opening
    // reveals the game while opaque curved edges mask its rectangular frame.
    // RGA performs this source-over blend in hardware.
    if (!status_obj->show_full && status_obj->show_decoration && status_obj->decoration &&
        status_obj->decoration->format == DRM_FORMAT_RGBA8888) {
        // Each presenter framebuffer keeps the static artwork outside the
        // core's destination rectangle. A complete blend is only required
        // while all three recycled buffers are being cleaned (menu close,
        // decoration change, viewport change). During steady gameplay the
        // core can only damage its own rectangle, so restore that intersection
        // instead of reading and writing the full screen every frame.
        int blend_x = status_obj->clean_full ? 0 : dstX;
        int blend_y = status_obj->clean_full ? 0 : dstY;
        int blend_width = status_obj->clean_full ? canvas_width : dstWidth;
        int blend_height = status_obj->clean_full ? canvas_height : dstHeight;
        if (blend_x < 0) { blend_width += blend_x; blend_x = 0; }
        if (blend_y < 0) { blend_height += blend_y; blend_y = 0; }
        blend_width = std::min(blend_width, canvas_width - blend_x);
        blend_height = std::min(blend_height, canvas_height - blend_y);
        if (blend_width > 0 && blend_height > 0) {
            const int source_x = blend_x * status_obj->decoration->width /
                                 canvas_width;
            const int source_y = blend_y * status_obj->decoration->height /
                                 canvas_height;
            const int source_right = (blend_x + blend_width) *
                                     status_obj->decoration->width /
                                     canvas_width;
            const int source_bottom = (blend_y + blend_height) *
                                      status_obj->decoration->height /
                                      canvas_height;
            go2_surface_blit_alpha(status_obj->decoration,
                                   source_x, source_y,
                                   std::max(1, source_right - source_x),
                                   std::max(1, source_bottom - source_y),
                                   dstSurface, blend_x, blend_y,
                                   blend_width, blend_height,
                                   rotation);
        }
    }

    go2_surface_t *surface = NULL;

    if (status_obj->show_bottom_left)
    {
        surface = status_obj->bottom_left;
        if (surface != nullptr)
        {
            blit_surface_status(presenter, surface, dstSurface, dstWidth, dstHeight, blitRotation, BUTTOM_LEFT, isWideScreen);
        }
    }

    if (status_obj->show_bottom_right)
    {
        surface = status_obj->bottom_right;
        if (surface != nullptr)
        {
            blit_surface_status(presenter, surface, dstSurface, dstWidth, dstHeight, blitRotation, BUTTOM_RIGHT, isWideScreen);
        }
    }


    if (status_obj->show_bottom_center) // new
    {
        surface = status_obj->bottom_center;
        if (surface != nullptr)
        {
            blit_surface_status(presenter, surface, dstSurface, dstWidth, dstHeight, blitRotation, BUTTOM_CENTER, isWideScreen);
        }
    }
    if (status_obj->show_top_right)
    {
        surface = status_obj->top_right;
        if (surface != nullptr)
        {
            blit_surface_status(presenter, surface, dstSurface, dstWidth, dstHeight, blitRotation, TOP_RIGHT, isWideScreen);
        }
    }

    if (status_obj->show_top_left)
    {
        surface = status_obj->top_left;
        if (surface != nullptr)
        {
            blit_surface_status(presenter, surface, dstSurface, dstWidth, dstHeight, blitRotation, TOP_LEFT, isWideScreen);
        }
    }

    if (status_obj->show_full)
    {
        surface = status_obj->full;
        if (surface != nullptr)
        {
            // printf("status non è null!!!");
            blit_surface_status(presenter, surface, dstSurface, dstWidth - 50, dstHeight - 50, blitRotation, FULL, isWideScreen);
        }
    }

    pthread_mutex_lock(&presenter->queueMutex);
    go2_queue_push(presenter->usedFrameBuffers, dstFrameBuffer);
    pthread_mutex_unlock(&presenter->queueMutex);

    sem_post(&presenter->usedSem);
}

#define BUFFER_MAX (3)

typedef struct buffer_surface_pair
{
    struct gbm_bo *gbmBuffer;
    go2_surface_t *surface;
} buffer_surface_pair_t;

typedef struct go2_context
{
    go2_display_t *display;
    int width;
    int height;
    go2_context_attributes_t attributes;
    struct gbm_device *gbmDevice;
    EGLDisplay eglDisplay;
    struct gbm_surface *gbmSurface;
    EGLSurface eglSurface;
    EGLContext eglContext;
    uint32_t drmFourCC;
    buffer_surface_pair_t bufferMap[BUFFER_MAX];
    int bufferCount;
} go2_context_t;

static EGLConfig FindConfig(EGLDisplay eglDisplay, int redBits, int greenBits, int blueBits, int alphaBits, int depthBits, int stencilBits)
{
    EGLint configAttributes[] =
        {
            EGL_RED_SIZE, redBits,
            EGL_GREEN_SIZE, greenBits,
            EGL_BLUE_SIZE, blueBits,
            EGL_ALPHA_SIZE, alphaBits,

            EGL_DEPTH_SIZE, depthBits,
            EGL_STENCIL_SIZE, stencilBits,

            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,

            EGL_NONE};

    int num_configs;
    EGLBoolean success = eglChooseConfig(eglDisplay, configAttributes, NULL, 0, &num_configs);
    if (success != EGL_TRUE)
    {
        logger.log(Logger::ERR,"eglChooseConfig failed.\n");
        exit(1);
    }

    // EGLConfig* configs = new EGLConfig[num_configs];
    EGLConfig configs[num_configs];
    success = eglChooseConfig(eglDisplay, configAttributes, configs, num_configs, &num_configs);
    if (success != EGL_TRUE)
    {
        logger.log(Logger::ERR,"eglChooseConfig failed.\n");
        exit(1);
    }

    EGLConfig match = 0;
    for (int i = 0; i < num_configs; ++i)
    {
        EGLint configRedSize;
        EGLint configGreenSize;
        EGLint configBlueSize;
        EGLint configAlphaSize;
        EGLint configDepthSize;
        EGLint configStencilSize;

        eglGetConfigAttrib(eglDisplay, configs[i], EGL_RED_SIZE, &configRedSize);
        eglGetConfigAttrib(eglDisplay, configs[i], EGL_GREEN_SIZE, &configGreenSize);
        eglGetConfigAttrib(eglDisplay, configs[i], EGL_BLUE_SIZE, &configBlueSize);
        eglGetConfigAttrib(eglDisplay, configs[i], EGL_ALPHA_SIZE, &configAlphaSize);
        eglGetConfigAttrib(eglDisplay, configs[i], EGL_DEPTH_SIZE, &configDepthSize);
        eglGetConfigAttrib(eglDisplay, configs[i], EGL_STENCIL_SIZE, &configStencilSize);

        // printf("Egl::FindConfig: index=%d, red=%d, green=%d, blue=%d, alpha=%d\n",
        //	i, configRedSize, configGreenSize, configBlueSize, configAlphaSize);

        if (configRedSize == redBits &&
            configBlueSize == blueBits &&
            configGreenSize == greenBits &&
            configAlphaSize == alphaBits &&
            configDepthSize == depthBits &&
            configStencilSize == stencilBits)
        {
            match = configs[i];
            break;
        }
    }

    return match;
}

go2_context_t *go2_context_create(go2_display_t *display, int width, int height, const go2_context_attributes_t *attributes)
{
    EGLBoolean success;

    go2_context_t *result = (go2_context_t *)malloc(sizeof(*result));
    if (!result)
    {
        logger.log(Logger::ERR,"malloc failed.\n");
        return NULL;
    }

    memset(result, 0, sizeof(*result));

    result->display = display;
    result->width = width;
    result->height = height;
    result->attributes = *attributes;

    result->gbmDevice = gbm_create_device(display->fd);
    if (!result->gbmDevice)
    {
        logger.log(Logger::ERR,"gbm_create_device failed.\n");
        free(result);
        return NULL;
    }

    PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display = NULL;
    get_platform_display = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    if (get_platform_display == NULL)
    {
        logger.log(Logger::ERR,"eglGetProcAddress failed.\n");
        gbm_device_destroy(result->gbmDevice);
        free(result);
        return NULL;
    }

    result->eglDisplay = get_platform_display(EGL_PLATFORM_GBM_KHR, result->gbmDevice, NULL);
    if (result->eglDisplay == EGL_NO_DISPLAY)
    {
        logger.log(Logger::ERR,"eglGetPlatformDisplayEXT failed.\n");
        gbm_device_destroy(result->gbmDevice);
        free(result);
        return NULL;
    }

    // Initialize EGL
    EGLint major;
    EGLint minor;
    success = eglInitialize(result->eglDisplay, &major, &minor);
    if (success != EGL_TRUE)
    {
        logger.log(Logger::ERR,"eglInitialize failed.\n");
        gbm_device_destroy(result->gbmDevice);
        free(result);
        return NULL;
    }

    logger.log(Logger::DEB,"EGL: major=%d, minor=%d\n", major, minor);
    logger.log(Logger::DEB,"EGL: Vendor=%s\n", eglQueryString(result->eglDisplay, EGL_VENDOR));
    logger.log(Logger::DEB,"EGL: Version=%s\n", eglQueryString(result->eglDisplay, EGL_VERSION));
    logger.log(Logger::DEB,"EGL: ClientAPIs=%s\n", eglQueryString(result->eglDisplay, EGL_CLIENT_APIS));
    logger.log(Logger::DEB,"EGL: Extensions=%s\n", eglQueryString(result->eglDisplay, EGL_EXTENSIONS));
    logger.log(Logger::DEB,"EGL: ClientExtensions=%s\n", eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS));
    

    EGLConfig eglConfig = FindConfig(result->eglDisplay, attributes->red_bits, attributes->green_bits,
                                     attributes->blue_bits, attributes->alpha_bits, attributes->depth_bits, attributes->stencil_bits);

    // Get the native visual id associated with the config
    // int visual_id;
    eglGetConfigAttrib(result->eglDisplay, eglConfig, EGL_NATIVE_VISUAL_ID, (EGLint *)&result->drmFourCC);

    result->gbmSurface = gbm_surface_create(result->gbmDevice,
                                            width,
                                            height,
                                            result->drmFourCC,
                                            GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!result->gbmSurface)
    {
        logger.log(Logger::ERR,"gbm_surface_create failed.\n");
        exit(1);
    }

    result->eglSurface = eglCreateWindowSurface(result->eglDisplay, eglConfig, (EGLNativeWindowType)result->gbmSurface, NULL);
    if (result->eglSurface == EGL_NO_SURFACE)
    {
        logger.log(Logger::ERR,"eglCreateWindowSurface failed\n");
        exit(1);
    }

    // Create a context
    eglBindAPI(EGL_OPENGL_ES_API);

    EGLint contextAttributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, attributes->major,
        EGL_NONE};

    result->eglContext = eglCreateContext(result->eglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttributes);
    if (result->eglContext == EGL_NO_CONTEXT)
    {
        logger.log(Logger::ERR,"eglCreateContext failed\n");
        exit(1);
    }

    success = eglMakeCurrent(result->eglDisplay, result->eglSurface, result->eglSurface, result->eglContext);
    if (success != EGL_TRUE)
    {
        logger.log(Logger::ERR,"eglMakeCurrent failed\n");
        exit(1);
    }

    return result;

    
}

void go2_context_destroy(go2_context_t *context)
{
    // Remove DRM framebuffer registrations while their GBM objects are still
    // alive. Destroying the GBM surface first may invalidate GEM handles on
    // vendor kernels before drmModeRmFB gets a chance to release them.
    for (int i = 0; i < context->bufferCount; ++i)
    {
        if (context->bufferMap[i].surface->direct_fb_id)
            drmModeRmFB(context->display->fd,
                        context->bufferMap[i].surface->direct_fb_id);
        if (context->bufferMap[i].surface->prime_fd > 0)
            close(context->bufferMap[i].surface->prime_fd);
        free(context->bufferMap[i].surface);
    }

    eglDestroyContext(context->eglDisplay, context->eglContext);
    eglDestroySurface(context->eglDisplay, context->eglSurface);
    gbm_surface_destroy(context->gbmSurface);
    eglTerminate(context->eglDisplay);
    gbm_device_destroy(context->gbmDevice);

    free(context);
}

void *go2_context_egldisplay_get(go2_context_t *context)
{
    return context->eglDisplay;
}

void go2_context_make_current(go2_context_t *context)
{
    EGLBoolean success = eglMakeCurrent(context->eglDisplay, context->eglSurface, context->eglSurface, context->eglContext);
    if (success != EGL_TRUE)
    {
        logger.log(Logger::ERR,"eglMakeCurrent failed\n");
        exit(1);
    }
}

void go2_context_swap_buffers(go2_context_t *context)
{
    EGLBoolean ret = eglSwapBuffers(context->eglDisplay, context->eglSurface);
    if (ret == EGL_FALSE)
    {
        logger.log(Logger::ERR,"eglSwapBuffers failed\n");
        // exit(1);
    }
}

go2_surface_t *go2_context_surface_lock(go2_context_t *context)
{
    struct gbm_bo *bo = gbm_surface_lock_front_buffer(context->gbmSurface);
    if (!bo)
    {
        logger.log(Logger::ERR,"gbm_surface_lock_front_buffer failed.\n");
        exit(1);
    }

    go2_surface_t *surface = NULL;
    for (int i = 0; i < context->bufferCount; ++i)
    {
        buffer_surface_pair_t *pair = &context->bufferMap[i];
        if (pair->gbmBuffer == bo)
        {
            surface = pair->surface;
            break;
        }
    }

    if (!surface)
    {
        if (context->bufferCount >= BUFFER_MAX)
        {
            logger.log(Logger::ERR,"swap buffers count exceeded.\n");
            exit(1);
        }

        surface = (go2_surface_t *)malloc(sizeof(*surface));
        if (!surface)
        {
            logger.log(Logger::ERR,"malloc failed.\n");
            exit(1);
        }

        memset(surface, 0, sizeof(*surface));

        surface->display = context->display;
        surface->gem_handle = gbm_bo_get_handle(bo).u32;
        surface->size = gbm_bo_get_stride(bo);
        surface->width = gbm_bo_get_width(bo);
        surface->height = gbm_bo_get_height(bo);
        surface->stride = gbm_bo_get_stride(bo);
        surface->format = context->drmFourCC;

        buffer_surface_pair_t *pair = &context->bufferMap[context->bufferCount++];
        pair->gbmBuffer = bo;
        pair->surface = surface;

        // printf("added buffer - bo=%p, count=%d\n", bo, context->bufferCount);
    }

    return surface;
}

void go2_context_surface_unlock(go2_context_t *context, go2_surface_t *surface)
{
    struct gbm_bo *bo = NULL;
    for (int i = 0; i < context->bufferCount; ++i)
    {
        buffer_surface_pair_t *pair = &context->bufferMap[i];
        if (pair->surface == surface)
        {
            bo = pair->gbmBuffer;
            break;
        }
    }

    if (!bo)
    {
        exit(1);
    }

    gbm_surface_release_buffer(context->gbmSurface, bo);
}
