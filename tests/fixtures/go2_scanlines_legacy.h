#pragma once
// Golden pre-optimization drawNonOpenGL from RetroRun 15359b4c.
// Original GPL-2.0-or-later implementation, Copyright navy1978 / OtherCrashOverride.
// The body is preserved verbatim; only its name and platform fixture are adapted.
#include "platform/platform.h"
#include <algorithm>
#include <vector>
#include <cstdint>
#include <cstring>
namespace legacy_scanlines {
struct Surface {std::uint8_t* pixels;int stride;std::uint32_t format;};
inline Surface* surface=nullptr;
inline std::uint32_t color_format=RR_PIXEL_FORMAT_RGB565;
inline rr_video_shader_t videoShader=RR_VIDEO_SHADER_SCANLINES;
inline void* rr_surface_map(Surface* p){return p->pixels;}
inline void rr_surface_unmap(Surface*){}
inline int rr_surface_stride_get(Surface* p){return p->stride;}
inline std::uint32_t rr_surface_format_get(Surface* p){return p->format;}
inline int rr_pixel_format_bpp(std::uint32_t f){return f==RR_PIXEL_FORMAT_RGB565?16:f==RR_PIXEL_FORMAT_RGB888?24:32;}
inline void draw(const void *data, unsigned width, unsigned height, size_t pitch)
{

    uint8_t *src = (uint8_t *)data;
    uint8_t *mapped = (uint8_t *)rr_surface_map(surface);
    uint8_t *dst = mapped;
    if (dst == nullptr)
    {
        return;
    }
    int bpp = rr_pixel_format_bpp(rr_surface_format_get(surface)) / 8;

    int yy = height;
    while (yy > 0)
    {
        if (color_format == RR_PIXEL_FORMAT_RGBA5551)
        {
            uint32_t *src2 = (uint32_t *)src;
            uint32_t *dst2 = (uint32_t *)dst;

            for (int x = 0; x < (short)width / 2; ++x)
            {
                uint32_t pixel = src2[x];
                pixel = ((pixel << 1) & (~0x3f003f)) | (pixel & 0x1f001f);
                dst2[x] = pixel;
            }
        }
        else
        {
            memcpy(dst, src, width * bpp);
        }

        src += pitch;
        dst += rr_surface_stride_get(surface);
        --yy;
    }

    if (videoShader != RR_VIDEO_SHADER_OFF)
    {
        const uint32_t format = rr_surface_format_get(surface);
        const int stride = rr_surface_stride_get(surface);
        const bool crt = videoShader == RR_VIDEO_SHADER_CRT;
        // Rebuild the effect map only when geometry or mode changes. Q8 gains
        // remove floating point, division and vignette math from every frame.
        static std::vector<uint16_t> effect_gain;
        static std::vector<uint8_t> grille_mask;
        static unsigned cached_width = 0;
        static unsigned cached_height = 0;
        static bool cached_crt = false;
        if (cached_width != width || cached_height != height ||
            cached_crt != crt || effect_gain.size() != width * height)
        {
            effect_gain.resize(static_cast<size_t>(width) * height);
            grille_mask.resize(width);
            for (unsigned px = 0; px < width; ++px)
                grille_mask[px] = static_cast<uint8_t>(px % 3U);
            for (unsigned py = 0; py < height; ++py)
            {
                const float normalized_y = height > 1
                    ? (2.0f * py / static_cast<float>(height - 1)) - 1.0f : 0.0f;
                const float scanline = (py & 1U) ? 0.68f : 1.0f;
                for (unsigned px = 0; px < width; ++px)
                {
                    float factor = scanline;
                    if (crt)
                    {
                        const float normalized_x = width > 1
                            ? (2.0f * px / static_cast<float>(width - 1)) - 1.0f : 0.0f;
                        factor *= std::max(0.62f,
                            1.0f - 0.18f * (normalized_x * normalized_x +
                                            normalized_y * normalized_y));
                    }
                    effect_gain[static_cast<size_t>(py) * width + px] =
                        static_cast<uint16_t>(std::clamp(factor, 0.0f, 1.0f) * 256.0f);
                }
            }
            cached_width = width;
            cached_height = height;
            cached_crt = crt;
        }
        auto scale_channel = [](unsigned value, unsigned gain, unsigned maximum) {
            return std::min(maximum, (value * gain + 128U) >> 8U);
        };

        for (unsigned py = 0; py < height; ++py)
        {
            uint8_t *row = mapped + static_cast<size_t>(py) * stride;
            for (unsigned px = 0; px < width; ++px)
            {
                const unsigned gain = effect_gain[static_cast<size_t>(py) * width + px];
                const unsigned dim_gain = (gain * 225U) >> 8U;
                const unsigned mask = grille_mask[px];

                if (format == RR_PIXEL_FORMAT_RGB565)
                {
                    uint16_t &pixel = reinterpret_cast<uint16_t *>(row)[px];
                    unsigned red = (pixel >> 11) & 0x1f;
                    unsigned green = (pixel >> 5) & 0x3f;
                    unsigned blue = pixel & 0x1f;
                    if (crt) {
                        red = scale_channel(red, mask == 0 ? gain : dim_gain, 0x1f);
                        green = scale_channel(green, mask == 1 ? gain : dim_gain, 0x3f);
                        blue = scale_channel(blue, mask == 2 ? gain : dim_gain, 0x1f);
                    } else {
                        red = scale_channel(red, gain, 0x1f);
                        green = scale_channel(green, gain, 0x3f);
                        blue = scale_channel(blue, gain, 0x1f);
                    }
                    pixel = static_cast<uint16_t>((red << 11) | (green << 5) | blue);
                }
                else if (format == RR_PIXEL_FORMAT_XRGB8888 ||
                         format == RR_PIXEL_FORMAT_RGBA8888)
                {
                    uint32_t &pixel = reinterpret_cast<uint32_t *>(row)[px];
                    unsigned red = (pixel >> 16) & 0xff;
                    unsigned green = (pixel >> 8) & 0xff;
                    unsigned blue = pixel & 0xff;
                    red = scale_channel(red, crt && mask != 0 ? dim_gain : gain, 0xff);
                    green = scale_channel(green, crt && mask != 1 ? dim_gain : gain, 0xff);
                    blue = scale_channel(blue, crt && mask != 2 ? dim_gain : gain, 0xff);
                    pixel = (pixel & 0xff000000U) | (red << 16) | (green << 8) | blue;
                }
                else if (format == RR_PIXEL_FORMAT_RGB888)
                {
                    uint8_t *pixel = row + px * 3;
                    pixel[0] = static_cast<uint8_t>(scale_channel(pixel[0], crt && mask != 0 ? dim_gain : gain, 0xff));
                    pixel[1] = static_cast<uint8_t>(scale_channel(pixel[1], crt && mask != 1 ? dim_gain : gain, 0xff));
                    pixel[2] = static_cast<uint8_t>(scale_channel(pixel[2], crt && mask != 2 ? dim_gain : gain, 0xff));
                }
            }
        }
    }
    rr_surface_unmap(surface);
}
}
