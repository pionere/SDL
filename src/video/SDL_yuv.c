/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../SDL_internal.h"

#include "SDL_endian.h"
#include "SDL_video.h"
#include "SDL_pixels_c.h"
#include "SDL_yuv_c.h"

#include "yuv2rgb/yuv_rgb.h"

#include <limits.h> /* For INT_MAX */

#if SDL_HAVE_YUV
#define SDL_YUV_SD_THRESHOLD 576

static SDL_YUV_CONVERSION_MODE SDL_YUV_ConversionMode = SDL_YUV_CONVERSION_BT601;

static SDL_bool IsPacked4Format(Uint32 format)
{
    return format == SDL_PIXELFORMAT_YUY2 || format == SDL_PIXELFORMAT_UYVY || format == SDL_PIXELFORMAT_YVYU;
}

static SDL_bool IsPlanar2x2Format(Uint32 format)
{
    return format == SDL_PIXELFORMAT_YV12 || format == SDL_PIXELFORMAT_IYUV || format == SDL_PIXELFORMAT_NV12 || format == SDL_PIXELFORMAT_NV21 || format == SDL_PIXELFORMAT_P010;
}

void SDL_SetYUVConversionMode(SDL_YUV_CONVERSION_MODE mode)
{
    if (mode != SDL_YUV_CONVERSION_JPEG &&
        mode != SDL_YUV_CONVERSION_BT601 &&
        mode != SDL_YUV_CONVERSION_BT709 &&
        mode != SDL_YUV_CONVERSION_AUTOMATIC) {
        SDL_InvalidParamError("mode");
        return;
    }

    SDL_YUV_ConversionMode = mode;
}

SDL_YUV_CONVERSION_MODE SDL_GetYUVConversionMode(void)
{
    return SDL_YUV_ConversionMode;
}

SDL_YUV_CONVERSION_MODE SDL_GetYUVConversionModeForResolution(int width, int height)
{
    SDL_YUV_CONVERSION_MODE mode = SDL_GetYUVConversionMode();
    if (mode == SDL_YUV_CONVERSION_AUTOMATIC) {
        if (height <= SDL_YUV_SD_THRESHOLD) {
            mode = SDL_YUV_CONVERSION_BT601;
        } else {
            mode = SDL_YUV_CONVERSION_BT709;
        }
    }
    return mode;
}
#else
void SDL_SetYUVConversionMode(SDL_YUV_CONVERSION_MODE mode)
{
    SDL_Unsupported();
}

SDL_YUV_CONVERSION_MODE SDL_GetYUVConversionMode(void)
{
    return SDL_Unsupported();
}

SDL_YUV_CONVERSION_MODE SDL_GetYUVConversionModeForResolution(int width, int height)
{
    return SDL_Unsupported();
}
#endif

/*
 * Calculate YUV size and pitch. Check for overflow.
 * Output 'pitch' that can be used with SDL_ConvertPixels()
 *
 * return 0 on success, -1 on error
 */
int SDL_CalculateYUVSize(Uint32 format, int w, int h, size_t *size, int *pitch)
{
    if (IsPacked4Format(format)) {
        size_t sz_plane_packed;
        {
            /* sz_plane_packed == ((w + 1) / 2) * h; */
            size_t s1, s2;
            s1 = (size_t)w + 1;
            s1 = s1 / 2;
            if (SDL_size_mul_overflow(s1, h, &s2)) {
                return -1;
            }
            sz_plane_packed = s2;
        }

        switch (format) {
        case SDL_PIXELFORMAT_YUY2: /**< Packed mode: Y0+U0+Y1+V0 (1 plane) */
        case SDL_PIXELFORMAT_UYVY: /**< Packed mode: U0+Y0+V0+Y1 (1 plane) */
        case SDL_PIXELFORMAT_YVYU: /**< Packed mode: Y0+V0+Y1+U0 (1 plane) */
        {
            size_t s1, p1, p2;
            /* dst_size == 4 * sz_plane_packed; */
            if (SDL_size_mul_overflow(sz_plane_packed, 4, &s1)) {
                return -1;
            }
            *size = s1;

            /* pitch == ((w + 1) / 2) * 4; */
            p1 = (size_t)w + 1;
            p1 = p1 / 2;
            if (SDL_size_mul_overflow(p1, 4, &p2) || p2 > INT_MAX) {
                return -1;
            }
            *pitch = (int)p2;
        } break;
        default:
            SDL_assume(!"Unknown packed yuv format");
            return -1;
        }
    } else {
        /* planar formats with 4:2:0 sampling */
        size_t sz_plane, sz_plane_chroma, sz_uv, sz_bytes;
        {
            /* sz_plane == w * h; */
            size_t s1;
            if (SDL_size_mul_overflow(w, h, &s1)) {
                return -1;
            }
            sz_plane = s1;
        }

        {
            /* sz_plane_chroma == ((w + 1) / 2) * ((h + 1) / 2); */
            size_t s1, s2, s3;
            s1 = (size_t)w + 1;
            s1 = s1 / 2;
            s2 = (size_t)h + 1;
            s2 = s2 / 2;
            if (SDL_size_mul_overflow(s1, s2, &s3)) {
                return -1;
            }
            sz_plane_chroma = s3;
        }

        {
            /* sz_uv = sz_plane_chroma + sz_plane_chroma; */
            size_t s1;
            if (SDL_size_mul_overflow(sz_plane_chroma, 2, &s1)) {
                return -1;
            }
            sz_uv = s1;
        }

        {
            /* sz_bytes = sz_plane + sz_uv; */
            size_t s1;
            if (SDL_size_add_overflow(sz_plane, sz_uv, &s1)) {
                return -1;
            }
            sz_bytes = s1;
        }

        switch (format) {
        case SDL_PIXELFORMAT_YV12: /**<  8bit Y + V + U  (3 planes) */
        case SDL_PIXELFORMAT_IYUV: /**<  8bit Y + U + V  (3 planes) */
        case SDL_PIXELFORMAT_NV12: /**<  8bit Y + U/V interleaved  (2 planes) */
        case SDL_PIXELFORMAT_NV21: /**<  8bit Y + V/U interleaved  (2 planes) */
        {
            *pitch = w;

            /* 3 planes: dst_size == sz_plane + sz_plane_chroma + sz_plane_chroma; */
            /* 2 planes: dst_size == sz_plane + (sz_plane_chroma + sz_plane_chroma); */
            *size = sz_bytes;
        } break;
        case SDL_PIXELFORMAT_P010: /**< 16bit Y + U/V interleaved  (2 planes) */
        {
            size_t s1;

            if (w > INT_MAX / sizeof(Uint16)) {
                return -1;
            }
            *pitch = sizeof(Uint16) * w;
            
            if (SDL_size_mul_overflow(sz_bytes, sizeof(Uint16), &s1)) {
                return -1;
            }
            *size = s1;
        } break;
        default:
            SDL_assume(!"Unknown planar yuv format");
            return -1;
        }
    }

    return 0;
}

#if SDL_HAVE_YUV

static int GetYUVConversionType(int width, int height, YCbCrType *yuv_type)
{
    SDL_YUV_CONVERSION_MODE convmode = SDL_GetYUVConversionModeForResolution(width, height);
    switch (convmode) {
    case SDL_YUV_CONVERSION_JPEG:
        *yuv_type = YCBCR_JPEG;
        break;
    case SDL_YUV_CONVERSION_BT601:
        *yuv_type = YCBCR_601;
        break;
    case SDL_YUV_CONVERSION_BT709:
        *yuv_type = YCBCR_709;
        break;
    default:
        SDL_assume(!"Unsupported YUV conversion mode");
        return SDL_SetError("Unsupported YUV conversion mode: %d", convmode);
    }
    return 0;
}

int SDL_InitYUVInfo(int width, int height, Uint32 format, const void *yuv, int yuv_pitch, SDL_YUVInfo *yuv_info)
{
    unsigned res_uv_pitch;

    SDL_assert(width > 0 && height > 0);

    SDL_assert(yuv_info != NULL);

    yuv_info->yuv_format = format;
    yuv_info->y_width = width;
    yuv_info->y_height = height;
    yuv_info->bpp = 1;
    if (IsPacked4Format(format)) {
        size_t sz_channel_w;

        yuv_info->yuv_layout = SDL_YUVLAYOUT_PACKED;
        yuv_info->uv_height = height;

        {
            /* sz_channel_w == ((w + 1) / 2); */
            size_t s1;
            s1 = (size_t)width + 1;
            s1 = s1 / 2;
            sz_channel_w = s1;

            yuv_info->uv_width = s1;
        }

        switch (format) {
        case SDL_PIXELFORMAT_YUY2: /**< Packed mode: Y0+U0+Y1+V0 (1 plane) */
        case SDL_PIXELFORMAT_UYVY: /**< Packed mode: U0+Y0+V0+Y1 (1 plane) */
        case SDL_PIXELFORMAT_YVYU: /**< Packed mode: Y0+V0+Y1+U0 (1 plane) */
        {
            size_t s1, p1;

            /* pitch == sz_channel_w * 4; */
            if (yuv_pitch == 0) {
                p1 = 4 * sz_channel_w;
                yuv_pitch = p1;
            }

            /* dst_size == pitch * h; */
            s1 = yuv_pitch * height;
            yuv_info->yuv_size = s1;
        } break;
        default:
            SDL_assume(!"Unknown packed yuv format");
            return -1;
        }

    } else if (IsPlanar2x2Format(format)) {
        /* planar formats with 4:2:0 sampling */
        size_t sz_plane, sz_plane_chroma, sz_uv, sz_bytes;
        {
            /* sz_plane == w * h; */
            size_t s1;
            s1 = width * height;
            sz_plane = s1;
        }

        {
            /* sz_plane_chroma == ((w + 1) / 2) * ((h + 1) / 2); */
            size_t s1, s2, s3;
            s1 = (size_t)width + 1;
            s1 = s1 / 2;
            s2 = (size_t)height + 1;
            s2 = s2 / 2;
            s3 = s1 * s2;
            sz_plane_chroma = s3;

            yuv_info->uv_width = s1;
            yuv_info->uv_height = s2;
        }

        {
            /* sz_uv = sz_plane_chroma + sz_plane_chroma; */
            size_t s1;
            s1 = sz_plane_chroma * 2;
            sz_uv = s1;
        }

        {
            /* sz_bytes = sz_plane + sz_uv; */
            size_t s1;
            s1 = sz_plane + sz_uv;
            sz_bytes = s1;
        }

        yuv_info->yuv_layout = SDL_YUVLAYOUT_2PLANES;
        switch (format) {
        case SDL_PIXELFORMAT_YV12: /**<  8bit Y + V + U  (3 planes) */
        case SDL_PIXELFORMAT_IYUV: /**<  8bit Y + U + V  (3 planes) */
            yuv_info->yuv_layout = SDL_YUVLAYOUT_3PLANES;
            /* fall-through */
        case SDL_PIXELFORMAT_NV12: /**<  8bit Y + U/V interleaved  (2 planes) */
        case SDL_PIXELFORMAT_NV21: /**<  8bit Y + V/U interleaved  (2 planes) */
        {
            if (yuv_pitch == 0) {
                yuv_pitch = width;
            }

            /* 3 planes: dst_size == sz_plane + sz_plane_chroma + sz_plane_chroma; */
            /* 2 planes: dst_size == sz_plane + (sz_plane_chroma + sz_plane_chroma); */
            yuv_info->yuv_size = sz_bytes;
        } break;
        case SDL_PIXELFORMAT_P010: /**< 16bit Y + U/V interleaved  (2 planes) */
        {
            size_t s1;

            if (yuv_pitch == 0) {
                yuv_pitch = sizeof(Uint16) * width;
            }

            s1 = sz_bytes * sizeof(Uint16);
            yuv_info->yuv_size = s1;
            yuv_info->bpp = sizeof(Uint16);
        } break;
        default:
            SDL_assume(!"Unknown planar yuv format");
            return -1;
        }
    } else {
#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER)
        SDL_SetError("Unsupported YUV format: %s", SDL_GetPixelFormatName(format));
        return -1; // 'smart' compilers again...
#else
        return SDL_SetError("Unsupported YUV format: %s", SDL_GetPixelFormatName(format));
#endif
    }

    yuv_info->y_pitch = yuv_pitch;

    yuv_info->planes[0] = (Uint8 *)yuv;
    yuv_info->y_plane = (Uint8 *)yuv;

    res_uv_pitch = yuv_pitch;
    if (yuv_info->yuv_layout == SDL_YUVLAYOUT_PACKED) {
        /* packed formats */
        switch (format) {
        case SDL_PIXELFORMAT_YUY2:
            yuv_info->v_plane = yuv_info->planes[0] + 3;
            yuv_info->u_plane = yuv_info->planes[0] + 1;
            break;
        case SDL_PIXELFORMAT_UYVY:
            yuv_info->y_plane = yuv_info->planes[0] + 1;
            yuv_info->u_plane = yuv_info->planes[0];
            yuv_info->v_plane = yuv_info->planes[0] + 2;
            break;
        case SDL_PIXELFORMAT_YVYU:
            yuv_info->v_plane = yuv_info->planes[0] + 1;
            yuv_info->u_plane = yuv_info->planes[0] + 3;
            break;
        default:
            SDL_assume(!"Unknown yuv format");
            return -1;
        }
    } else if (yuv_info->yuv_layout == SDL_YUVLAYOUT_2PLANES) {
        /* planar formats (2 planes) */
        res_uv_pitch = 2 * (((unsigned)yuv_pitch + 1) / 2);
        yuv_info->planes[1] = yuv_info->planes[0] + yuv_pitch * height;

        if (format == SDL_PIXELFORMAT_NV12) {
            yuv_info->u_plane = yuv_info->planes[1];
            yuv_info->v_plane = yuv_info->u_plane + 1;
        } else if (format == SDL_PIXELFORMAT_NV21) {
            yuv_info->v_plane = yuv_info->planes[1];
            yuv_info->u_plane = yuv_info->v_plane + 1;
        } else { // if (format == SDL_PIXELFORMAT_P010) {
            yuv_info->u_plane = yuv_info->planes[1];
            yuv_info->v_plane = yuv_info->u_plane + sizeof(Uint16);
        }
    } else { // if (yuv_info->yuv_layout == SDL_YUVLAYOUT_3PLANES) {
        /* planar formats (3 planes) */
        res_uv_pitch = ((unsigned)yuv_pitch + 1) / 2;
        yuv_info->planes[1] = yuv_info->planes[0] + yuv_pitch * height;
        yuv_info->planes[2] = yuv_info->planes[1] + res_uv_pitch * (((unsigned)height + 1) / 2);

        if (format == SDL_PIXELFORMAT_YV12) {
            yuv_info->v_plane = yuv_info->planes[1];
            yuv_info->u_plane = yuv_info->planes[2];
        } else { // if (format == SDL_PIXELFORMAT_IYUV) {
            yuv_info->v_plane = yuv_info->planes[2];
            yuv_info->u_plane = yuv_info->planes[1];
        }
    }

    yuv_info->uv_pitch = res_uv_pitch;
    return 0;
}

#ifdef __SSE2__
static SDL_bool yuv_rgb_sse(
    Uint32 src_format, Uint32 dst_format,
    Uint32 width, Uint32 height,
    const Uint8 *y, const Uint8 *u, const Uint8 *v, Uint32 y_pitch, Uint32 uv_pitch,
    Uint8 *rgb, Uint32 rgb_pitch,
    YCbCrType yuv_type)
{
    switch (src_format) {
    case SDL_PIXELFORMAT_YV12:
    case SDL_PIXELFORMAT_IYUV:
        switch (dst_format) {
        case SDL_PIXELFORMAT_RGB565:
            yuv420_rgb565_sseu(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_RGB24:
            yuv420_rgb24_sseu(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_RGBX8888:
        case SDL_PIXELFORMAT_RGBA8888:
            yuv420_rgba_sseu(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_BGRX8888:
        case SDL_PIXELFORMAT_BGRA8888:
            yuv420_bgra_sseu(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_RGB888:
        case SDL_PIXELFORMAT_ARGB8888:
            yuv420_argb_sseu(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_BGR888:
        case SDL_PIXELFORMAT_ABGR8888:
            yuv420_abgr_sseu(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        default:
            break;
        }
        break;
    case SDL_PIXELFORMAT_YUY2:
    case SDL_PIXELFORMAT_UYVY:
    case SDL_PIXELFORMAT_YVYU:
        switch (dst_format) {
        case SDL_PIXELFORMAT_RGB565:
            yuv422_rgb565_sseu(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_RGB24:
            yuv422_rgb24_sseu(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_RGBX8888:
        case SDL_PIXELFORMAT_RGBA8888:
            yuv422_rgba_sseu(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_BGRX8888:
        case SDL_PIXELFORMAT_BGRA8888:
            yuv422_bgra_sseu(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_RGB888:
        case SDL_PIXELFORMAT_ARGB8888:
            yuv422_argb_sseu(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_BGR888:
        case SDL_PIXELFORMAT_ABGR8888:
            yuv422_abgr_sseu(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        default:
            break;
        }
        break;
    case SDL_PIXELFORMAT_NV12:
    case SDL_PIXELFORMAT_NV21:
        switch (dst_format) {
        case SDL_PIXELFORMAT_RGB565:
            yuvnv12_rgb565_sseu(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_RGB24:
            yuvnv12_rgb24_sseu(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_RGBX8888:
        case SDL_PIXELFORMAT_RGBA8888:
            yuvnv12_rgba_sseu(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_BGRX8888:
        case SDL_PIXELFORMAT_BGRA8888:
            yuvnv12_bgra_sseu(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_RGB888:
        case SDL_PIXELFORMAT_ARGB8888:
            yuvnv12_argb_sseu(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_BGR888:
        case SDL_PIXELFORMAT_ABGR8888:
            yuvnv12_abgr_sseu(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        default:
            break;
        }
        break;
    case SDL_PIXELFORMAT_P010:
        break;
    default:
        SDL_assume(!"Unknown pixel format");
        break;
    }
    return SDL_FALSE;
}
#endif // __SSE2__

#ifdef __loongarch_sx
static SDL_bool yuv_rgb_lsx(
    Uint32 src_format, Uint32 dst_format,
    Uint32 width, Uint32 height,
    const Uint8 *y, const Uint8 *u, const Uint8 *v, Uint32 y_pitch, Uint32 uv_pitch,
    Uint8 *rgb, Uint32 rgb_pitch,
    YCbCrType yuv_type)
{
    if (src_format == SDL_PIXELFORMAT_YV12 ||
        src_format == SDL_PIXELFORMAT_IYUV) {

        switch (dst_format) {
        case SDL_PIXELFORMAT_RGB24:
            yuv420_rgb24_lsx(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_RGBX8888:
        case SDL_PIXELFORMAT_RGBA8888:
            yuv420_rgba_lsx(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_BGRX8888:
        case SDL_PIXELFORMAT_BGRA8888:
            yuv420_bgra_lsx(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_RGB888:
        case SDL_PIXELFORMAT_ARGB8888:
            yuv420_argb_lsx(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_BGR888:
        case SDL_PIXELFORMAT_ABGR8888:
            yuv420_abgr_lsx(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        default:
            break;
        }
    }
    return SDL_FALSE;
}
#endif // __loongarch_sx

static SDL_bool yuv_rgb_std(
    Uint32 src_format, Uint32 dst_format,
    Uint32 width, Uint32 height,
    const Uint8 *y, const Uint8 *u, const Uint8 *v, Uint32 y_pitch, Uint32 uv_pitch,
    Uint8 *rgb, Uint32 rgb_pitch,
    YCbCrType yuv_type)
{
    switch (src_format) {
    case SDL_PIXELFORMAT_YV12:
    case SDL_PIXELFORMAT_IYUV:
        switch (dst_format) {
        case SDL_PIXELFORMAT_RGB565:
            yuv420_rgb565_std(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_RGB24:
            yuv420_rgb24_std(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_RGBX8888:
        case SDL_PIXELFORMAT_RGBA8888:
            yuv420_rgba_std(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_BGRX8888:
        case SDL_PIXELFORMAT_BGRA8888:
            yuv420_bgra_std(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_RGB888:
        case SDL_PIXELFORMAT_ARGB8888:
            yuv420_argb_std(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_BGR888:
        case SDL_PIXELFORMAT_ABGR8888:
            yuv420_abgr_std(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        default:
            break;
        }
        break;
    case SDL_PIXELFORMAT_YUY2:
    case SDL_PIXELFORMAT_UYVY:
    case SDL_PIXELFORMAT_YVYU:
        switch (dst_format) {
        case SDL_PIXELFORMAT_RGB565:
            yuv422_rgb565_std(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_RGB24:
            yuv422_rgb24_std(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_RGBX8888:
        case SDL_PIXELFORMAT_RGBA8888:
            yuv422_rgba_std(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_BGRX8888:
        case SDL_PIXELFORMAT_BGRA8888:
            yuv422_bgra_std(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_RGB888:
        case SDL_PIXELFORMAT_ARGB8888:
            yuv422_argb_std(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_BGR888:
        case SDL_PIXELFORMAT_ABGR8888:
            yuv422_abgr_std(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        default:
            break;
        }
        break;
    case SDL_PIXELFORMAT_NV12:
    case SDL_PIXELFORMAT_NV21:
        switch (dst_format) {
        case SDL_PIXELFORMAT_RGB565:
            yuvnv12_rgb565_std(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_RGB24:
            yuvnv12_rgb24_std(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_RGBX8888:
        case SDL_PIXELFORMAT_RGBA8888:
            yuvnv12_rgba_std(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_BGRX8888:
        case SDL_PIXELFORMAT_BGRA8888:
            yuvnv12_bgra_std(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_RGB888:
        case SDL_PIXELFORMAT_ARGB8888:
            yuvnv12_argb_std(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        case SDL_PIXELFORMAT_BGR888:
        case SDL_PIXELFORMAT_ABGR8888:
            yuvnv12_abgr_std(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        default:
            break;
        }
        break;
    case SDL_PIXELFORMAT_P010:
        switch (dst_format) {
        case SDL_PIXELFORMAT_ARGB8888:
            yuvp010_argb_std(width, height, y, u, v, y_pitch, uv_pitch, rgb, rgb_pitch, yuv_type);
            return SDL_TRUE;
        default:
            break;
        }
        break;
    default:
        SDL_assume(!"Unknown pixel format");
        break;
    }
    return SDL_FALSE;
}

int SDL_ConvertPixels_YUV_to_RGB(int width, int height,
                                 Uint32 src_format, const void *src, int src_pitch,
                                 Uint32 dst_format, void *dst, int dst_pitch)
{
    SDL_YUVInfo yuv_info;
    int retval;
    YCbCrType yuv_type = YCBCR_601;

    SDL_assert(width > 0 && height > 0);
    SDL_assert(src_pitch > 0 && dst_pitch > 0);

    retval = SDL_InitYUVInfo(width, height, src_format, src, src_pitch, &yuv_info);
    if (retval < 0) {
        return retval;
    }
    retval = GetYUVConversionType(width, height, &yuv_type);
    if (retval < 0) {
        return -1;
    }

#ifdef __SSE2__
    if (SDL_HasSSE2()) {
        if (yuv_rgb_sse(src_format, dst_format, width, height, yuv_info.y_plane, yuv_info.u_plane, yuv_info.v_plane, yuv_info.y_pitch, yuv_info.uv_pitch, (Uint8 *)dst, dst_pitch, yuv_type)) {
            return 0;
        }
    }
#endif
#ifdef __loongarch_sx
    if (SDL_HasLSX()) {
        if (yuv_rgb_lsx(src_format, dst_format, width, height, yuv_info.y_plane, yuv_info.u_plane, yuv_info.v_plane, yuv_info.y_pitch, yuv_info.uv_pitch, (Uint8 *)dst, dst_pitch, yuv_type)) {
            return 0;
        }
    }
#endif
    if (yuv_rgb_std(src_format, dst_format, width, height, yuv_info.y_plane, yuv_info.u_plane, yuv_info.v_plane, yuv_info.y_pitch, yuv_info.uv_pitch, (Uint8 *)dst, dst_pitch, yuv_type)) {
        return 0;
    }

    /* No fast path for the RGB format, instead convert using an intermediate buffer */
    if (dst_format != SDL_PIXELFORMAT_ARGB8888) {
        void *tmp;
        unsigned tmp_pitch = (width * sizeof(Uint32));

        tmp = SDL_malloc((size_t)tmp_pitch * height);
        if (!tmp) {
            return SDL_OutOfMemory();
        }

        /* convert src/src_format to tmp/ARGB8888 */
        retval = SDL_ConvertPixels_YUV_to_RGB(width, height, src_format, src, src_pitch, SDL_PIXELFORMAT_ARGB8888, tmp, tmp_pitch);
        if (retval < 0) {
            SDL_free(tmp);
            return retval;
        }

        /* convert tmp/ARGB8888 to dst/RGB */
        retval = SDL_ConvertPixels(width, height, SDL_PIXELFORMAT_ARGB8888, tmp, tmp_pitch, dst_format, dst, dst_pitch);
        SDL_free(tmp);
        return retval;
    }

    return SDL_SetError("Unsupported YUV conversion");
}

struct RGB2YUVFactors
{
    int y_offset;
    float y[3]; /* Rfactor, Gfactor, Bfactor */
    float u[3]; /* Rfactor, Gfactor, Bfactor */
    float v[3]; /* Rfactor, Gfactor, Bfactor */
};

static int SDL_ConvertPixels_ARGB8888_to_YUV(const void *src, unsigned src_pitch, SDL_YUVInfo *dst_yuv_info)
{
    unsigned width = dst_yuv_info->y_width;
    unsigned height = dst_yuv_info->y_height;
    unsigned i, j;

    static struct RGB2YUVFactors RGB2YUVFactorTables[SDL_YUV_CONVERSION_BT709 + 1] = {
        /* ITU-T T.871 (JPEG) */
        {
            0,
            { 0.2990f, 0.5870f, 0.1140f },
            { -0.1687f, -0.3313f, 0.5000f },
            { 0.5000f, -0.4187f, -0.0813f },
        },
        /* ITU-R BT.601-7 */
        {
            16,
            { 0.2568f, 0.5041f, 0.0979f },
            { -0.1482f, -0.2910f, 0.4392f },
            { 0.4392f, -0.3678f, -0.0714f },
        },
        /* ITU-R BT.709-6 */
        {
            16,
            { 0.1826f, 0.6142f, 0.0620f },
            { -0.1006f, -0.3386f, 0.4392f },
            { 0.4392f, -0.3989f, -0.0403f },
        },
    };
    const struct RGB2YUVFactors *cvt = &RGB2YUVFactorTables[SDL_GetYUVConversionModeForResolution(width, height)];

#define MAKE_Y(r, g, b) (Uint8)((int)(cvt->y[0] * (r) + cvt->y[1] * (g) + cvt->y[2] * (b) + 0.5f) + cvt->y_offset)
#define MAKE_U(r, g, b) (Uint8)((int)(cvt->u[0] * (r) + cvt->u[1] * (g) + cvt->u[2] * (b) + 0.5f) + 128)
#define MAKE_V(r, g, b) (Uint8)((int)(cvt->v[0] * (r) + cvt->v[1] * (g) + cvt->v[2] * (b) + 0.5f) + 128)

#define READ_2x2_PIXELS                                                                                     \
    const Uint32 p1 = ((const Uint32 *)curr_row)[2 * i];                                                    \
    const Uint32 p2 = ((const Uint32 *)curr_row)[2 * i + 1];                                                \
    const Uint32 p3 = ((const Uint32 *)next_row)[2 * i];                                                    \
    const Uint32 p4 = ((const Uint32 *)next_row)[2 * i + 1];                                                \
    const Uint32 r = ((p1 & 0x00ff0000) + (p2 & 0x00ff0000) + (p3 & 0x00ff0000) + (p4 & 0x00ff0000)) >> 18; \
    const Uint32 g = ((p1 & 0x0000ff00) + (p2 & 0x0000ff00) + (p3 & 0x0000ff00) + (p4 & 0x0000ff00)) >> 10; \
    const Uint32 b = ((p1 & 0x000000ff) + (p2 & 0x000000ff) + (p3 & 0x000000ff) + (p4 & 0x000000ff)) >> 2;

#define READ_2x1_PIXELS                                             \
    const Uint32 p1 = ((const Uint32 *)curr_row)[2 * i];            \
    const Uint32 p2 = ((const Uint32 *)next_row)[2 * i];            \
    const Uint32 r = ((p1 & 0x00ff0000) + (p2 & 0x00ff0000)) >> 17; \
    const Uint32 g = ((p1 & 0x0000ff00) + (p2 & 0x0000ff00)) >> 9;  \
    const Uint32 b = ((p1 & 0x000000ff) + (p2 & 0x000000ff)) >> 1;

#define READ_1x2_PIXELS                                             \
    const Uint32 p1 = ((const Uint32 *)curr_row)[2 * i];            \
    const Uint32 p2 = ((const Uint32 *)curr_row)[2 * i + 1];        \
    const Uint32 r = ((p1 & 0x00ff0000) + (p2 & 0x00ff0000)) >> 17; \
    const Uint32 g = ((p1 & 0x0000ff00) + (p2 & 0x0000ff00)) >> 9;  \
    const Uint32 b = ((p1 & 0x000000ff) + (p2 & 0x000000ff)) >> 1;

#define READ_1x1_PIXEL                                  \
    const Uint32 p = ((const Uint32 *)curr_row)[2 * i]; \
    const Uint32 r = (p & 0x00ff0000) >> 16;            \
    const Uint32 g = (p & 0x0000ff00) >> 8;             \
    const Uint32 b = (p & 0x000000ff);

#define READ_TWO_RGB_PIXELS                                  \
    const Uint32 p = ((const Uint32 *)curr_row)[2 * i];      \
    const Uint32 r = (p & 0x00ff0000) >> 16;                 \
    const Uint32 g = (p & 0x0000ff00) >> 8;                  \
    const Uint32 b = (p & 0x000000ff);                       \
    const Uint32 p1 = ((const Uint32 *)curr_row)[2 * i + 1]; \
    const Uint32 r1 = (p1 & 0x00ff0000) >> 16;               \
    const Uint32 g1 = (p1 & 0x0000ff00) >> 8;                \
    const Uint32 b1 = (p1 & 0x000000ff);                     \
    const Uint32 R = (r + r1) / 2;                           \
    const Uint32 G = (g + g1) / 2;                           \
    const Uint32 B = (b + b1) / 2;

#define READ_ONE_RGB_PIXEL READ_1x1_PIXEL

    if (dst_yuv_info->yuv_layout != SDL_YUVLAYOUT_PACKED) {
        const Uint8 *curr_row, *next_row;

        Uint8 *plane_y, *plane_u, *plane_v, *plane_interleaved_uv;
        Uint32 y_pitch, y_skip, uv_skip;
        unsigned src_skip, src_pitch_x_2;

        if (dst_yuv_info->bpp != 1) {
            return SDL_SetError("Unsupported YUV destination format: %s", SDL_GetPixelFormatName(dst_yuv_info->yuv_format));
        }

        /* Write Y plane */
        y_pitch = dst_yuv_info->y_pitch;
        // if (y_pitch < width) {
        //     return SDL_SetError("Destination pitch is too small, expected at least %d\n", width);
        // }
        y_skip = (y_pitch - width);
        // if (src_pitch < width * 4) {
        //     return SDL_SetError("Source pitch is too small, expected at least %d\n", width * 4);
        // }
        src_skip = src_pitch - width * 4;

        curr_row = (const Uint8 *)src;
        plane_y = dst_yuv_info->y_plane;
        for (j = 0; j < height; j++) {
            for (i = 0; i < width; i++) {
                const Uint32 p1 = *(const Uint32 *)curr_row;
                const Uint32 r = (p1 & 0x00ff0000) >> 16;
                const Uint32 g = (p1 & 0x0000ff00) >> 8;
                const Uint32 b = (p1 & 0x000000ff);
                *plane_y++ = MAKE_Y(r, g, b);
                curr_row += 4;
            }
            plane_y += y_skip;
            curr_row += src_skip;
        }

        /* Write UV planes */
        curr_row = (const Uint8 *)src;
        next_row = (const Uint8 *)src;
        next_row += src_pitch;
        src_pitch_x_2 = src_pitch * 2;

        switch (dst_yuv_info->yuv_format) {
        case SDL_PIXELFORMAT_YV12:
        case SDL_PIXELFORMAT_IYUV:
            /* Write UV planes, not interleaved */
            plane_u = dst_yuv_info->u_plane;
            plane_v = dst_yuv_info->v_plane;
            uv_skip = dst_yuv_info->uv_pitch - dst_yuv_info->uv_width;
            for (j = 0; j < height / 2; j++) {
                for (i = 0; i < width / 2; i++) {
                    READ_2x2_PIXELS;
                    *plane_u++ = MAKE_U(r, g, b);
                    *plane_v++ = MAKE_V(r, g, b);
                }
                if (width & 0x1) {
                    READ_2x1_PIXELS;
                    *plane_u++ = MAKE_U(r, g, b);
                    *plane_v++ = MAKE_V(r, g, b);
                }
                plane_u += uv_skip;
                plane_v += uv_skip;
                curr_row += src_pitch_x_2;
                next_row += src_pitch_x_2;
            }
            if (height & 1) {
                for (i = 0; i < width / 2; i++) {
                    READ_1x2_PIXELS;
                    *plane_u++ = MAKE_U(r, g, b);
                    *plane_v++ = MAKE_V(r, g, b);
                }
                if (width & 1) {
                    READ_1x1_PIXEL;
                    *plane_u++ = MAKE_U(r, g, b);
                    *plane_v++ = MAKE_V(r, g, b);
                }
                plane_u += uv_skip;
                plane_v += uv_skip;
            }
            break;
        case SDL_PIXELFORMAT_NV12:
            plane_interleaved_uv = dst_yuv_info->u_plane;
            uv_skip = dst_yuv_info->uv_pitch - 2 * dst_yuv_info->uv_width;
            for (j = 0; j < height / 2; j++) {
                for (i = 0; i < width / 2; i++) {
                    READ_2x2_PIXELS;
                    *plane_interleaved_uv++ = MAKE_U(r, g, b);
                    *plane_interleaved_uv++ = MAKE_V(r, g, b);
                }
                if (width & 1) {
                    READ_2x1_PIXELS;
                    *plane_interleaved_uv++ = MAKE_U(r, g, b);
                    *plane_interleaved_uv++ = MAKE_V(r, g, b);
                }
                plane_interleaved_uv += uv_skip;
                curr_row += src_pitch_x_2;
                next_row += src_pitch_x_2;
            }
            if (height & 1) {
                for (i = 0; i < width / 2; i++) {
                    READ_1x2_PIXELS;
                    *plane_interleaved_uv++ = MAKE_U(r, g, b);
                    *plane_interleaved_uv++ = MAKE_V(r, g, b);
                }
                if (width & 1) {
                    READ_1x1_PIXEL;
                    *plane_interleaved_uv++ = MAKE_U(r, g, b);
                    *plane_interleaved_uv++ = MAKE_V(r, g, b);
                }
            }
            break;
        case SDL_PIXELFORMAT_NV21:
            plane_interleaved_uv = dst_yuv_info->v_plane;
            uv_skip = dst_yuv_info->uv_pitch - 2 * dst_yuv_info->uv_width;
            for (j = 0; j < height / 2; j++) {
                for (i = 0; i < width / 2; i++) {
                    READ_2x2_PIXELS;
                    *plane_interleaved_uv++ = MAKE_V(r, g, b);
                    *plane_interleaved_uv++ = MAKE_U(r, g, b);
                }
                if (width & 1) {
                    READ_2x1_PIXELS;
                    *plane_interleaved_uv++ = MAKE_V(r, g, b);
                    *plane_interleaved_uv++ = MAKE_U(r, g, b);
                }
                plane_interleaved_uv += uv_skip;
                curr_row += src_pitch_x_2;
                next_row += src_pitch_x_2;
            }
            if (height & 1) {
                for (i = 0; i < width / 2; i++) {
                    READ_1x2_PIXELS;
                    *plane_interleaved_uv++ = MAKE_V(r, g, b);
                    *plane_interleaved_uv++ = MAKE_U(r, g, b);
                }
                if (width & 1) {
                    READ_1x1_PIXEL;
                    *plane_interleaved_uv++ = MAKE_V(r, g, b);
                    *plane_interleaved_uv++ = MAKE_U(r, g, b);
                }
            }
            break;
        default:
            SDL_assume(!"Unknown format");
            break;
        }
    } else { // dst_yuv_info->yuv_layout == SDL_YUVLAYOUT_PACKED
        const Uint8 *curr_row = (const Uint8 *)src;
        Uint8 *plane = dst_yuv_info->planes[0];
        const unsigned row_size = 4 * dst_yuv_info->uv_width;
        unsigned plane_skip;

        SDL_assert(dst_yuv_info->bpp == 1);

        // if (dst_yuv_info->y_pitch < row_size) {
        //     return SDL_SetError("Destination pitch is too small, expected at least %d\n", row_size);
        // }
        plane_skip = (dst_yuv_info->y_pitch - row_size);

        /* Write YUV plane, packed */
        switch (dst_yuv_info->yuv_format) {
        case SDL_PIXELFORMAT_YUY2:
            for (j = 0; j < height; j++) {
                for (i = 0; i < width / 2; i++) {
                    READ_TWO_RGB_PIXELS;
                    /* Y U Y1 V */
                    *plane++ = MAKE_Y(r, g, b);
                    *plane++ = MAKE_U(R, G, B);
                    *plane++ = MAKE_Y(r1, g1, b1);
                    *plane++ = MAKE_V(R, G, B);
                }
                if (width & 1) {
                    READ_ONE_RGB_PIXEL;
                    /* Y U Y V */
                    *plane++ = MAKE_Y(r, g, b);
                    *plane++ = MAKE_U(r, g, b);
                    *plane++ = MAKE_Y(r, g, b);
                    *plane++ = MAKE_V(r, g, b);
                }
                plane += plane_skip;
                curr_row += src_pitch;
            }
            break;
        case SDL_PIXELFORMAT_UYVY:
            for (j = 0; j < height; j++) {
                for (i = 0; i < width / 2; i++) {
                    READ_TWO_RGB_PIXELS;
                    /* U Y V Y1 */
                    *plane++ = MAKE_U(R, G, B);
                    *plane++ = MAKE_Y(r, g, b);
                    *plane++ = MAKE_V(R, G, B);
                    *plane++ = MAKE_Y(r1, g1, b1);
                }
                if (width & 1) {
                    READ_ONE_RGB_PIXEL;
                    /* U Y V Y */
                    *plane++ = MAKE_U(r, g, b);
                    *plane++ = MAKE_Y(r, g, b);
                    *plane++ = MAKE_V(r, g, b);
                    *plane++ = MAKE_Y(r, g, b);
                }
                plane += plane_skip;
                curr_row += src_pitch;
            }
            break;
        case SDL_PIXELFORMAT_YVYU:
            for (j = 0; j < height; j++) {
                for (i = 0; i < width / 2; i++) {
                    READ_TWO_RGB_PIXELS;
                    /* Y V Y1 U */
                    *plane++ = MAKE_Y(r, g, b);
                    *plane++ = MAKE_V(R, G, B);
                    *plane++ = MAKE_Y(r1, g1, b1);
                    *plane++ = MAKE_U(R, G, B);
                }
                if (width & 1) {
                    READ_ONE_RGB_PIXEL;
                    /* Y V Y U */
                    *plane++ = MAKE_Y(r, g, b);
                    *plane++ = MAKE_V(r, g, b);
                    *plane++ = MAKE_Y(r, g, b);
                    *plane++ = MAKE_U(r, g, b);
                }
                plane += plane_skip;
                curr_row += src_pitch;
            }
            break;
        default:
            SDL_assume(!"Unknown format");
            break;
        }
    }
#undef MAKE_Y
#undef MAKE_U
#undef MAKE_V
#undef READ_2x2_PIXELS
#undef READ_2x1_PIXELS
#undef READ_1x2_PIXELS
#undef READ_1x1_PIXEL
#undef READ_TWO_RGB_PIXELS
#undef READ_ONE_RGB_PIXEL
    return 0;
}

int SDL_ConvertPixels_RGB_to_YUV(int width, int height,
                                 Uint32 src_format, const void *src, int src_pitch,
                                 Uint32 dst_format, void *dst, int dst_pitch)
{
    SDL_YUVInfo dst_yuv_info;
    int retval;
    SDL_assert(width > 0 && height > 0);
    SDL_assert(src_pitch > 0 && dst_pitch > 0);

    retval = SDL_InitYUVInfo(width, height, dst_format, dst, dst_pitch, &dst_yuv_info);
    if (retval < 0) {
        return retval;
    }

#if 0 /* Doesn't handle odd widths */
    /* RGB24 to FOURCC */
    if (src_format == SDL_PIXELFORMAT_RGB24) {
        YCbCrType yuv_type;

        retval = GetYUVConversionType(dst_yuv_info.y_width, dst_yuv_info.y_height, &yuv_type);
        if (retval < 0) {
            return retval;
        }

        rgb24_yuv420_std(dst_yuv_info.y_width, dst_yuv_info.y_height, src, src_pitch, dst_yuv_info.y_plane, dst_yuv_info.u_plane, dst_yuv_info.v_plane, dst_yuv_info.y_pitch, dst_yuv_info.uv_pitch, yuv_type);
        return 0;
    }
#endif

    /* ARGB8888 to FOURCC */
    if (src_format == SDL_PIXELFORMAT_ARGB8888) {
        return SDL_ConvertPixels_ARGB8888_to_YUV(src, src_pitch, &dst_yuv_info);
    }

    /* not ARGB8888 to FOURCC : need an intermediate conversion */
    {
        void *tmp;
        unsigned tmp_pitch = dst_yuv_info.y_width * sizeof(Uint32);

        tmp = SDL_malloc((size_t)tmp_pitch * dst_yuv_info.y_height);
        if (!tmp) {
            return SDL_OutOfMemory();
        }

        /* convert src/src_format to tmp/ARGB8888 */
        retval = SDL_ConvertPixels(dst_yuv_info.y_width, dst_yuv_info.y_height, src_format, src, src_pitch, SDL_PIXELFORMAT_ARGB8888, tmp, tmp_pitch);
        if (retval < 0) {
            SDL_free(tmp);
            return retval;
        }

        /* convert tmp/ARGB8888 to dst/FOURCC */
        retval = SDL_ConvertPixels_ARGB8888_to_YUV(tmp, tmp_pitch, &dst_yuv_info);
        SDL_free(tmp);
        return retval;
    }
}

static int SDL_ConvertPixels_YUV_to_YUV_Copy(const SDL_YUVInfo *src_yuv_info, SDL_YUVInfo *dst_yuv_info)
{
    unsigned src_pitch, dst_pitch; 
    unsigned i, rows, length;
    const Uint8 *src;
    Uint8 *dst;

    src = src_yuv_info->planes[0];
    dst = dst_yuv_info->planes[0];
    src_pitch = src_yuv_info->y_pitch;
    dst_pitch = dst_yuv_info->y_pitch;

    if (src_pitch == dst_pitch) {
        SDL_memcpy(dst, src, src_yuv_info->yuv_size);
        return 0;
    }

    /* copy the Y plane */
    rows = src_yuv_info->y_height;
    if (src_yuv_info->yuv_layout == SDL_YUVLAYOUT_PACKED) {
        /* Packed planes -> add UV values */
        length = 4 * src_yuv_info->uv_width;
    } else {
        /* Planar format */
        length = src_yuv_info->y_width;
    }
    length *= src_yuv_info->bpp;
    for (i = rows; i--;) {
        SDL_memcpy(dst, src, length);
        src = (const Uint8 *)src + src_pitch;
        dst = (Uint8 *)dst + dst_pitch;
    }

    if (src_yuv_info->yuv_layout != SDL_YUVLAYOUT_PACKED) {
        /* copy the UV plane(s) */
        rows = src_yuv_info->uv_height;
        length = src_yuv_info->uv_width;
        length *= src_yuv_info->bpp;
        src_pitch = src_yuv_info->uv_pitch;
        dst_pitch = dst_yuv_info->uv_pitch;
        if (src_yuv_info->yuv_layout == SDL_YUVLAYOUT_3PLANES) {
            /* separate U and V planes */
            rows *= 2;
        } else {
            /* interleaved U/V plane */
            length *= 2;
        }

        for (i = rows; i--;) {
            SDL_memcpy(dst, src, length);
            src = (const Uint8 *)src + src_pitch;
            dst = (Uint8 *)dst + dst_pitch;
        }
    }
    return 0;
}

static int SDL_ConvertPixels_SwapUVPlanes(const SDL_YUVInfo *src_yuv_info, SDL_YUVInfo *dst_yuv_info)
{
    if (src_yuv_info->y_pitch == dst_yuv_info->y_pitch) {
        /* Fast path */
        size_t uv_size = (size_t)src_yuv_info->planes[2] - (size_t)src_yuv_info->planes[1];
        SDL_assert(src_yuv_info->uv_pitch == dst_yuv_info->uv_pitch);
        SDL_memcpy(dst_yuv_info->planes[2], src_yuv_info->planes[1], uv_size);
        SDL_memcpy(dst_yuv_info->planes[1], src_yuv_info->planes[2], uv_size);
    } else {
        const unsigned UVwidth = src_yuv_info->uv_width;
        const unsigned UVheight = src_yuv_info->uv_height;
        const unsigned srcUVPitch = src_yuv_info->uv_pitch;
        const unsigned dstUVPitch = dst_yuv_info->uv_pitch;
        Uint8 *srcUV, *dstUV;
        unsigned y;

        /* Copy the first plane */
        srcUV = src_yuv_info->planes[1];
        dstUV = dst_yuv_info->planes[2];
        for (y = 0; y < UVheight; ++y) {
            SDL_memcpy(dstUV, srcUV, UVwidth);
            srcUV += srcUVPitch;
            dstUV += dstUVPitch;
        }

        /* Copy the second plane */
        srcUV = src_yuv_info->planes[2];
        dstUV = dst_yuv_info->planes[1];
        for (y = 0; y < UVheight; ++y) {
            SDL_memcpy(dstUV, srcUV, UVwidth);
            srcUV += srcUVPitch;
            dstUV += dstUVPitch;
        }
    }
    return 0;
}

static int SDL_ConvertPixels_PackUVPlanes_to_NV(const SDL_YUVInfo *src_yuv_info, SDL_YUVInfo *dst_yuv_info, SDL_bool reverseUV)
{
    unsigned x, y;
    unsigned UVwidth, UVheight, srcUVPitch, srcUVSkip, dstUVPitch, dstUVSkip;
    const Uint8 *src1, *src2;
    Uint8 *dstUV;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    UVwidth = src_yuv_info->uv_width;
    UVheight = src_yuv_info->uv_height;
    srcUVPitch = src_yuv_info->uv_pitch;
    srcUVSkip = srcUVPitch - UVwidth;
    dstUVPitch = dst_yuv_info->uv_pitch;
    dstUVSkip = dstUVPitch - UVwidth * 2;

    /* Skip the Y plane */
    dstUV = dst_yuv_info->planes[1];
    if (reverseUV) {
        src2 = src_yuv_info->planes[1];
        src1 = src_yuv_info->planes[2];
    } else {
        src1 = src_yuv_info->planes[1];
        src2 = src_yuv_info->planes[2];
    }

    y = UVheight;
    while (y--) {
        x = UVwidth;
#ifdef __SSE2__
        if (use_SSE2) {
            while (x >= 16) {
                __m128i u = _mm_loadu_si128((__m128i *)src1);
                __m128i v = _mm_loadu_si128((__m128i *)src2);
                __m128i uv1 = _mm_unpacklo_epi8(u, v);
                __m128i uv2 = _mm_unpackhi_epi8(u, v);
                _mm_storeu_si128((__m128i *)dstUV, uv1);
                _mm_storeu_si128((__m128i *)(dstUV + 16), uv2);
                src1 += 16;
                src2 += 16;
                dstUV += 32;
                x -= 16;
            }
        }
#endif
        while (x--) {
            *dstUV++ = *src1++;
            *dstUV++ = *src2++;
        }
        src1 += srcUVSkip;
        src2 += srcUVSkip;
        dstUV += dstUVSkip;
    }

    return 0;
}

// (SDL_PIXELFORMAT_YV12, SDL_PIXELFORMAT_IYUV) -> SDL_PIXELFORMAT_P010
static int SDL_ConvertPixels_PackUVPlanes_to_P0(const SDL_YUVInfo *src_yuv_info, SDL_YUVInfo *dst_yuv_info, SDL_bool reverseUV)
{
    unsigned x, y;
    unsigned UVwidth, UVheight, srcUVPitch, srcUVSkip, dstUVPitch, dstUVSkip;
    const Uint8 *src1, *src2;
    Uint16 *dstUV;
    const int bpp = sizeof(Uint16);
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    UVwidth = src_yuv_info->uv_width;
    UVheight = src_yuv_info->uv_height;
    srcUVPitch = src_yuv_info->uv_pitch;
    srcUVSkip = srcUVPitch - UVwidth;
    dstUVPitch = dst_yuv_info->uv_pitch;
    dstUVSkip = dstUVPitch - UVwidth * 2 * bpp;

    /* Skip the Y plane */
    dstUV = (Uint16 *)dst_yuv_info->planes[1];
    if (reverseUV) {
        src2 = src_yuv_info->planes[1];
        src1 = src_yuv_info->planes[2];
    } else {
        src1 = src_yuv_info->planes[1];
        src2 = src_yuv_info->planes[2];
    }

    y = UVheight;
    while (y--) {
        x = UVwidth;
#ifdef __SSE2__
        if (use_SSE2) {
            __m128i zero = _mm_setzero_si128();
            while (x >= 16) {
                __m128i u = _mm_loadu_si128((__m128i *)src1);
                __m128i v = _mm_loadu_si128((__m128i *)src2);
                __m128i uv1 = _mm_unpacklo_epi8(u, v);
                __m128i uv2 = _mm_unpackhi_epi8(u, v);
                __m128i uv10 = _mm_unpacklo_epi8(uv1, zero);
                __m128i uv11 = _mm_unpackhi_epi8(uv1, zero);
                __m128i uv20 = _mm_unpacklo_epi8(uv2, zero);
                __m128i uv21 = _mm_unpackhi_epi8(uv2, zero);
                _mm_storeu_si128((__m128i *)dstUV, uv10);
                _mm_storeu_si128((__m128i *)(dstUV + 8), uv11);
                _mm_storeu_si128((__m128i *)(dstUV + 16), uv20);
                _mm_storeu_si128((__m128i *)(dstUV + 24), uv21);
                src1 += 16;
                src2 += 16;
                dstUV += 32;
                x -= 16;
            }
        }
#endif
        while (x--) {
            *dstUV++ = (Uint16)(*src1++) << 8;
            *dstUV++ = (Uint16)(*src2++) << 8;
        }
        src1 += srcUVSkip;
        src2 += srcUVSkip;
        dstUV = (Uint16 *)((Uint8 *)dstUV + dstUVSkip);
    }

    return 0;
}

static int SDL_ConvertPixels_SplitNV_to_UVPlanes(const SDL_YUVInfo *src_yuv_info, SDL_YUVInfo *dst_yuv_info, SDL_bool reverseUV)
{
    unsigned x, y;
    unsigned UVwidth, UVheight, srcUVPitch, srcUVSkip, dstUVPitch, dstUVSkip;
    const Uint8 *srcUV;
    Uint8 *dst1, *dst2;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    UVwidth = src_yuv_info->uv_width;
    UVheight = src_yuv_info->uv_height;
    srcUVPitch = src_yuv_info->uv_pitch;
    srcUVSkip = srcUVPitch - UVwidth * 2;
    dstUVPitch = dst_yuv_info->uv_pitch;
    dstUVSkip = dstUVPitch - UVwidth;

    /* Skip the Y plane */
    srcUV = src_yuv_info->planes[1];
    if (reverseUV) {
        dst2 = dst_yuv_info->planes[1];
        dst1 = dst_yuv_info->planes[2];
    } else {
        dst1 = dst_yuv_info->planes[1];
        dst2 = dst_yuv_info->planes[2];
    }

    y = UVheight;
    while (y--) {
        x = UVwidth;
#ifdef __SSE2__
        if (use_SSE2) {
            __m128i mask = _mm_set1_epi16(0x00FF);
            while (x >= 16) {
                __m128i uv1 = _mm_loadu_si128((__m128i *)srcUV);
                __m128i uv2 = _mm_loadu_si128((__m128i *)(srcUV + 16));
                __m128i u1 = _mm_and_si128(uv1, mask);
                __m128i u2 = _mm_and_si128(uv2, mask);
                __m128i u = _mm_packus_epi16(u1, u2);
                __m128i v1 = _mm_srli_epi16(uv1, 8);
                __m128i v2 = _mm_srli_epi16(uv2, 8);
                __m128i v = _mm_packus_epi16(v1, v2);
                _mm_storeu_si128((__m128i *)dst1, u);
                _mm_storeu_si128((__m128i *)dst2, v);
                srcUV += 32;
                dst1 += 16;
                dst2 += 16;
                x -= 16;
            }
        }
#endif
        while (x--) {
            *dst1++ = *srcUV++;
            *dst2++ = *srcUV++;
        }
        srcUV += srcUVSkip;
        dst1 += dstUVSkip;
        dst2 += dstUVSkip;
    }

    return 0;
}

// SDL_PIXELFORMAT_P010 -> (SDL_PIXELFORMAT_YV12, SDL_PIXELFORMAT_IYUV)
static int SDL_ConvertPixels_SplitP0_to_UVPlanes(const SDL_YUVInfo *src_yuv_info, SDL_YUVInfo *dst_yuv_info, SDL_bool reverseUV)
{
    unsigned x, y;
    unsigned UVwidth, UVheight, srcUVPitch, srcUVSkip, dstUVPitch, dstUVSkip;
    const Uint8 *srcUV;
    Uint8 *dst1, *dst2;
    const int bpp = sizeof(Uint16);
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    UVwidth = src_yuv_info->uv_width;
    UVheight = src_yuv_info->uv_height;
    srcUVPitch = src_yuv_info->uv_pitch;
    srcUVSkip = srcUVPitch - UVwidth * 2;
    dstUVPitch = dst_yuv_info->uv_pitch;
    dstUVSkip = dstUVPitch - UVwidth;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    srcUVSkip -= 1; // revert the shift
#endif
    /* Skip the Y plane */
    srcUV = src_yuv_info->planes[1];
    if (reverseUV) {
        dst2 = dst_yuv_info->planes[1];
        dst1 = dst_yuv_info->planes[2];
    } else {
        dst1 = dst_yuv_info->planes[1];
        dst2 = dst_yuv_info->planes[2];
    }

    y = UVheight;
    while (y--) {
        x = UVwidth;
#ifdef __SSE2__
        if (use_SSE2) {
            __m128i mask = _mm_set1_epi16(0x00FF);
            while (x >= 16) {
                // load the data
                __m128i uv10 = _mm_loadu_si128((__m128i *)srcUV);
                __m128i uv11 = _mm_loadu_si128((__m128i *)(srcUV + 16));
                __m128i uv20 = _mm_loadu_si128((__m128i *)(srcUV + 32));
                __m128i uv21 = _mm_loadu_si128((__m128i *)(srcUV + 48));
                // discard the low byte
                __m128i uv10_ = _mm_srli_epi16(uv10, 8);
                __m128i uv11_ = _mm_srli_epi16(uv11, 8);
                __m128i uv20_ = _mm_srli_epi16(uv20, 8);
                __m128i uv21_ = _mm_srli_epi16(uv21, 8);
                __m128i uv1 = _mm_packus_epi16(uv10_, uv11_);
                __m128i uv2 = _mm_packus_epi16(uv20_, uv21_);
                // split the 'plane'
                __m128i u1 = _mm_and_si128(uv1, mask);
                __m128i u2 = _mm_and_si128(uv2, mask);
                __m128i u = _mm_packus_epi16(u1, u2);
                __m128i v1 = _mm_srli_epi16(uv1, 8);
                __m128i v2 = _mm_srli_epi16(uv2, 8);
                __m128i v = _mm_packus_epi16(v1, v2);
                // store the 'planes'
                _mm_storeu_si128((__m128i *)dst1, u);
                _mm_storeu_si128((__m128i *)dst2, v);
                srcUV += 32;
                dst1 += 16;
                dst2 += 16;
                x -= 16;
            }
        }
#endif
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
        srcUV += 1; // shift to the high byte
#endif
        while (x--) {
            *dst1++ = *srcUV;
            srcUV += bpp;
            *dst2++ = *srcUV;
            srcUV += bpp;
        }
        srcUV += srcUVSkip;
        dst1 += dstUVSkip;
        dst2 += dstUVSkip;
    }

    return 0;
}

static int SDL_ConvertPixels_SwapNV(const SDL_YUVInfo *src_yuv_info, SDL_YUVInfo *dst_yuv_info)
{
    unsigned x, y;
    unsigned UVwidth, UVheight, srcUVPitch, srcUVSkip, dstUVPitch, dstUVSkip;
    const Uint16 *srcUV;
    Uint16 *dstUV;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    UVwidth = src_yuv_info->uv_width;
    UVheight = src_yuv_info->uv_height;
    srcUVPitch = src_yuv_info->uv_pitch;
    srcUVSkip = srcUVPitch - UVwidth * 2;
    dstUVPitch = dst_yuv_info->uv_pitch;
    dstUVSkip = dstUVPitch - UVwidth * 2;

    /* Skip the Y plane */
    srcUV = (const Uint16 *)src_yuv_info->planes[1];
    dstUV = (Uint16 *)dst_yuv_info->planes[1];

    y = UVheight;
    while (y--) {
        x = UVwidth;
#ifdef __SSE2__
        if (use_SSE2) {
            while (x >= 8) {
                // load the data
                __m128i uv = _mm_loadu_si128((__m128i *)srcUV);
                // swap the bytes
                __m128i v = _mm_slli_epi16(uv, 8);
                __m128i u = _mm_srli_epi16(uv, 8);
                __m128i vu = _mm_or_si128(v, u);
                // store the result
                _mm_storeu_si128((__m128i *)dstUV, vu);
                srcUV += 8;
                dstUV += 8;
                x -= 8;
            }
        }
#endif
        while (x--) {
            *dstUV++ = SDL_Swap16(*srcUV++);
        }
        srcUV = (const Uint16 *)((const Uint8 *)srcUV + srcUVSkip);
        dstUV = (Uint16 *)((Uint8 *)dstUV + dstUVSkip);
    }
    return 0;
}

// SDL_PIXELFORMAT_NV12 -> SDL_PIXELFORMAT_P010
static int SDL_ConvertPixels_DecompNV(const SDL_YUVInfo *src_yuv_info, SDL_YUVInfo *dst_yuv_info)
{
    unsigned x, y;
    unsigned UVwidth, UVheight, srcUVPitch, srcUVSkip, dstUVPitch, dstUVSkip;
    const Uint8 *srcUV;
    Uint16 *dstUV;
    const int bpp = 2;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    UVwidth = src_yuv_info->uv_width;
    UVheight = src_yuv_info->uv_height;
    srcUVPitch = src_yuv_info->uv_pitch;
    srcUVSkip = srcUVPitch - UVwidth * 2;
    dstUVPitch = dst_yuv_info->uv_pitch;
    dstUVSkip = dstUVPitch - UVwidth * 2 * bpp;

    /* Skip the Y plane */
    srcUV = src_yuv_info->planes[1];
    dstUV = (Uint16 *)dst_yuv_info->planes[1];

    y = UVheight;
    while (y--) {
        x = UVwidth;
#ifdef __SSE2__
        if (use_SSE2) {
            __m128i zero = _mm_setzero_si128();
            while (x >= 8) {
                // load the data
                __m128i uv = _mm_loadu_si128((__m128i *)srcUV);
                // zfill to 16bits
                __m128i uv0 = _mm_unpacklo_epi8(zero, uv);
                __m128i uv1 = _mm_unpackhi_epi8(zero, uv);
                // store the result
                _mm_storeu_si128((__m128i *)dstUV, uv0);
                _mm_storeu_si128((__m128i *)(dstUV + 8), uv1);
                srcUV += 16;
                dstUV += 16;
                x -= 8;
            }
        }
#endif
        while (x--) {
            Uint16 u = (Uint16)(*srcUV++) << 8;
            Uint16 v = (Uint16)(*srcUV++) << 8;

            *dstUV++ = u;
            *dstUV++ = v;
        }
        srcUV += srcUVSkip;
        dstUV = (Uint16 *)((Uint8 *)dstUV + dstUVSkip);
    }
    return 0;
}

// SDL_PIXELFORMAT_NV21 -> SDL_PIXELFORMAT_P010
static int SDL_ConvertPixels_SwapAndDecompNV(const SDL_YUVInfo *src_yuv_info, SDL_YUVInfo *dst_yuv_info)
{
    unsigned x, y;
    unsigned UVwidth, UVheight, srcUVPitch, srcUVSkip, dstUVPitch, dstUVSkip;
    const Uint8 *srcUV;
    Uint16 *dstUV;
    const int bpp = 2;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    UVwidth = src_yuv_info->uv_width;
    UVheight = src_yuv_info->uv_height;
    srcUVPitch = src_yuv_info->uv_pitch;
    srcUVSkip = srcUVPitch - UVwidth * 2;
    dstUVPitch = dst_yuv_info->uv_pitch;
    dstUVSkip = dstUVPitch - UVwidth * 2 * bpp;

    /* Skip the Y plane */
    srcUV = src_yuv_info->planes[1];
    dstUV = (Uint16 *)dst_yuv_info->planes[1];

    y = UVheight;
    while (y--) {
        x = UVwidth;
#ifdef __SSE2__
        if (use_SSE2) {
            __m128i zero = _mm_setzero_si128();
            while (x >= 8) {
                // load the data
                __m128i uv = _mm_loadu_si128((__m128i *)srcUV);
                // swap the bytes
                __m128i v = _mm_slli_epi16(uv, 8);
                __m128i u = _mm_srli_epi16(uv, 8);
                __m128i vu = _mm_or_si128(v, u);
                // zfill to 16bits
                __m128i vu0 = _mm_unpacklo_epi8(zero, vu);
                __m128i vu1 = _mm_unpackhi_epi8(zero, vu);
                // store the result
                _mm_storeu_si128((__m128i *)dstUV, vu0);
                _mm_storeu_si128((__m128i *)(dstUV + 8), vu1);
                srcUV += 16;
                dstUV += 16;
                x -= 8;
            }
        }
#endif
        while (x--) {
            Uint16 u = (Uint16)(*srcUV++) << 8;
            Uint16 v = (Uint16)(*srcUV++) << 8;

            *dstUV++ = v;
            *dstUV++ = u;
        }
        srcUV += srcUVSkip;
        dstUV = (Uint16 *)((Uint8 *)dstUV + dstUVSkip);
    }
    return 0;
}

// SDL_PIXELFORMAT_P010 -> SDL_PIXELFORMAT_NV12
static int SDL_ConvertPixels_CompNV(const SDL_YUVInfo *src_yuv_info, SDL_YUVInfo *dst_yuv_info)
{
    unsigned x, y;
    unsigned UVwidth, UVheight, srcUVPitch, srcUVSkip, dstUVPitch, dstUVSkip;
    const Uint8 *srcUV;
    Uint8 *dstUV;
    const int bpp = 2;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    UVwidth = src_yuv_info->uv_width;
    UVheight = src_yuv_info->uv_height;
    srcUVPitch = src_yuv_info->uv_pitch;
    srcUVSkip = srcUVPitch - UVwidth * 2 * bpp;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    srcUVSkip -= 1; // revert the shift
#endif
    dstUVPitch = dst_yuv_info->uv_pitch;
    dstUVSkip = dstUVPitch - UVwidth * 2;

    /* Skip the Y plane */
    srcUV = src_yuv_info->planes[1];
    dstUV = dst_yuv_info->planes[1];

    y = UVheight;
    while (y--) {
        x = UVwidth;
#ifdef __SSE2__
        if (use_SSE2) {
            while (x >= 8) {
                // load the data
                __m128i uv0 = _mm_loadu_si128((__m128i *)srcUV);
                __m128i uv1 = _mm_loadu_si128((__m128i *)(srcUV + 16));
                // pack the values
                __m128i uv00 = _mm_srli_epi16(uv0, 8);
                __m128i uv10 = _mm_srli_epi16(uv1, 8);
                __m128i uv00_ = _mm_packus_epi16(uv00, uv10);
                // store the result
                _mm_storeu_si128((__m128i *)dstUV, uv00_);
                srcUV += 32;
                dstUV += 16;
                x -= 8;
            }
        }
#endif

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
        srcUV += 1; // shift to the high byte
#endif
        while (x--) {
            Uint8 u, v;

            u = *srcUV;
            srcUV += bpp;
            v = *srcUV;
            srcUV += bpp;

            *dstUV++ = u;
            *dstUV++ = v;
        }
        srcUV += srcUVSkip;
        dstUV += dstUVSkip;
    }
    return 0;
}

// SDL_PIXELFORMAT_P010 -> SDL_PIXELFORMAT_NV21
static int SDL_ConvertPixels_SwapAndCompNV(const SDL_YUVInfo *src_yuv_info, SDL_YUVInfo *dst_yuv_info)
{
    unsigned x, y;
    unsigned UVwidth, UVheight, srcUVPitch, srcUVSkip, dstUVPitch, dstUVSkip;
    const Uint8 *srcUV;
    Uint8 *dstUV;
    const int bpp = 2;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    UVwidth = src_yuv_info->uv_width;
    UVheight = src_yuv_info->uv_height;
    srcUVPitch = src_yuv_info->uv_pitch;
    srcUVSkip = srcUVPitch - UVwidth * 2 * bpp;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    srcUVSkip -= 1; // revert the shift
#endif
    dstUVPitch = dst_yuv_info->uv_pitch;
    dstUVSkip = dstUVPitch - UVwidth * 2;

    /* Skip the Y plane */
    srcUV = src_yuv_info->planes[1];
    dstUV = dst_yuv_info->planes[1];

    y = UVheight;
    while (y--) {
        x = UVwidth;
#ifdef __SSE2__
        if (use_SSE2) {
            while (x >= 8) {
                // load the data
                __m128i uv0 = _mm_loadu_si128((__m128i *)srcUV);
                __m128i uv1 = _mm_loadu_si128((__m128i *)(srcUV + 16));
                // pack the values
                __m128i uv00 = _mm_srli_epi16(uv0, 8);
                __m128i uv10 = _mm_srli_epi16(uv1, 8);
                __m128i uv00_ = _mm_packus_epi16(uv00, uv10);
                // swap the bytes
                __m128i v = _mm_slli_epi16(uv00_, 8);
                __m128i u = _mm_srli_epi16(uv00_, 8);
                __m128i vu = _mm_or_si128(v, u);
                // store the result
                _mm_storeu_si128((__m128i *)dstUV, vu);
                srcUV += 32;
                dstUV += 16;
                x -= 8;
            }
        }
#endif

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
        srcUV += 1; // shift to the high byte
#endif
        while (x--) {
            Uint8 u, v;

            u = *srcUV;
            srcUV += bpp;
            v = *srcUV;
            srcUV += bpp;

            *dstUV++ = v;
            *dstUV++ = u;
        }
        srcUV += srcUVSkip;
        dstUV += dstUVSkip;
    }
    return 0;
}

static int SDL_ConvertPixels_Planar2x2_to_Planar2x2(const SDL_YUVInfo *src_yuv_info, SDL_YUVInfo *dst_yuv_info)
{
    {   /* Copy Y plane */
        unsigned i, x;
        unsigned height = src_yuv_info->y_height;
        unsigned width = src_yuv_info->y_width;
        unsigned srcY_pitch = src_yuv_info->y_pitch;
        unsigned dstY_pitch = dst_yuv_info->y_pitch;
        const Uint8 *srcY = src_yuv_info->planes[0];
        Uint8 *dstY = dst_yuv_info->planes[0];
        if (src_yuv_info->bpp == dst_yuv_info->bpp) {
            // (SDL_PIXELFORMAT_YV12, SDL_PIXELFORMAT_IYUV, SDL_PIXELFORMAT_NV12, SDL_PIXELFORMAT_NV21) | (SDL_PIXELFORMAT_P010)
            unsigned length = width * src_yuv_info->bpp;
            for (i = height; i--;) {
                SDL_memcpy(dstY, srcY, length);
                srcY += srcY_pitch;
                dstY += dstY_pitch;
            }
        } else if (src_yuv_info->bpp < dst_yuv_info->bpp) {
            // (SDL_PIXELFORMAT_YV12, SDL_PIXELFORMAT_IYUV, SDL_PIXELFORMAT_NV12, SDL_PIXELFORMAT_NV21) -> (SDL_PIXELFORMAT_P010)
            unsigned srcYSkip = srcY_pitch - width;
            unsigned dstYSkip = dstY_pitch - width * sizeof(Uint16);
            SDL_assert(src_yuv_info->bpp == 1);
            SDL_assert(dst_yuv_info->bpp == 2);
            for (i = height; i--;) {
                for (x = width; x--;) {
                    *(Uint16 *)dstY = (Uint16)(*srcY) << 8;
                    srcY++;
                    dstY += sizeof(Uint16); 
                }
                srcY += srcYSkip;
                dstY += dstYSkip;
            }
        } else {
            // (SDL_PIXELFORMAT_P010) -> (SDL_PIXELFORMAT_YV12, SDL_PIXELFORMAT_IYUV, SDL_PIXELFORMAT_NV12, SDL_PIXELFORMAT_NV21)
            unsigned srcYSkip = srcY_pitch - width * sizeof(Uint16);
            unsigned dstYSkip = dstY_pitch - width;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
            srcY += 1; // shift to the high byte
#endif
            SDL_assert(src_yuv_info->bpp == 2);
            SDL_assert(dst_yuv_info->bpp == 1);
            for (i = height; i--;) {
                for (x = width; x--;) {
                    *dstY = *srcY;
                    srcY += sizeof(Uint16);
                    dstY++;
                }
                srcY += srcYSkip;
                dstY += dstYSkip;
            }
        }
    }

    switch (src_yuv_info->yuv_format) {
    case SDL_PIXELFORMAT_YV12:
        switch (dst_yuv_info->yuv_format) {
        case SDL_PIXELFORMAT_IYUV:
            return SDL_ConvertPixels_SwapUVPlanes(src_yuv_info, dst_yuv_info);
        case SDL_PIXELFORMAT_NV12:
            return SDL_ConvertPixels_PackUVPlanes_to_NV(src_yuv_info, dst_yuv_info, SDL_TRUE);
        case SDL_PIXELFORMAT_NV21:
            return SDL_ConvertPixels_PackUVPlanes_to_NV(src_yuv_info, dst_yuv_info, SDL_FALSE);
        case SDL_PIXELFORMAT_P010:
            return SDL_ConvertPixels_PackUVPlanes_to_P0(src_yuv_info, dst_yuv_info, SDL_TRUE);
        default:
            SDL_assume(!"Unsupported YUV format");
            break;
        }
        break;
    case SDL_PIXELFORMAT_IYUV:
        switch (dst_yuv_info->yuv_format) {
        case SDL_PIXELFORMAT_YV12:
            return SDL_ConvertPixels_SwapUVPlanes(src_yuv_info, dst_yuv_info);
        case SDL_PIXELFORMAT_NV12:
            return SDL_ConvertPixels_PackUVPlanes_to_NV(src_yuv_info, dst_yuv_info, SDL_FALSE);
        case SDL_PIXELFORMAT_NV21:
            return SDL_ConvertPixels_PackUVPlanes_to_NV(src_yuv_info, dst_yuv_info, SDL_TRUE);
        case SDL_PIXELFORMAT_P010:
            return SDL_ConvertPixels_PackUVPlanes_to_P0(src_yuv_info, dst_yuv_info, SDL_FALSE);
        default:
            SDL_assume(!"Unsupported YUV format");
            break;
        }
        break;
    case SDL_PIXELFORMAT_NV12:
        switch (dst_yuv_info->yuv_format) {
        case SDL_PIXELFORMAT_YV12:
            return SDL_ConvertPixels_SplitNV_to_UVPlanes(src_yuv_info, dst_yuv_info, SDL_TRUE);
        case SDL_PIXELFORMAT_IYUV:
            return SDL_ConvertPixels_SplitNV_to_UVPlanes(src_yuv_info, dst_yuv_info, SDL_FALSE);
        case SDL_PIXELFORMAT_NV21:
            return SDL_ConvertPixels_SwapNV(src_yuv_info, dst_yuv_info);
        case SDL_PIXELFORMAT_P010:
            return SDL_ConvertPixels_DecompNV(src_yuv_info, dst_yuv_info);
        default:
            SDL_assume(!"Unsupported YUV format");
            break;
        }
        break;
    case SDL_PIXELFORMAT_NV21:
        switch (dst_yuv_info->yuv_format) {
        case SDL_PIXELFORMAT_YV12:
            return SDL_ConvertPixels_SplitNV_to_UVPlanes(src_yuv_info, dst_yuv_info, SDL_FALSE);
        case SDL_PIXELFORMAT_IYUV:
            return SDL_ConvertPixels_SplitNV_to_UVPlanes(src_yuv_info, dst_yuv_info, SDL_TRUE);
        case SDL_PIXELFORMAT_NV12:
            return SDL_ConvertPixels_SwapNV(src_yuv_info, dst_yuv_info);
        case SDL_PIXELFORMAT_P010:
            return SDL_ConvertPixels_SwapAndDecompNV(src_yuv_info, dst_yuv_info);
        default:
            SDL_assume(!"Unsupported YUV format");
            break;
        }
        break;
    case SDL_PIXELFORMAT_P010:
        switch (dst_yuv_info->yuv_format) {
        case SDL_PIXELFORMAT_YV12:
            return SDL_ConvertPixels_SplitP0_to_UVPlanes(src_yuv_info, dst_yuv_info, SDL_TRUE);
        case SDL_PIXELFORMAT_IYUV:
            return SDL_ConvertPixels_SplitP0_to_UVPlanes(src_yuv_info, dst_yuv_info, SDL_FALSE);
        case SDL_PIXELFORMAT_NV12:
            return SDL_ConvertPixels_CompNV(src_yuv_info, dst_yuv_info);
        case SDL_PIXELFORMAT_NV21:
            return SDL_ConvertPixels_SwapAndCompNV(src_yuv_info, dst_yuv_info);
        default:
            SDL_assume(!"Unsupported YUV format");
            break;
        }
        break;
    default:
        SDL_assume(!"Unsupported YUV format");
        break;
    }
    return SDL_SetError("SDL_ConvertPixels_Planar2x2_to_Planar2x2: Unsupported YUV conversion: %s -> %s", SDL_GetPixelFormatName(src_yuv_info->yuv_format),
                        SDL_GetPixelFormatName(dst_yuv_info->yuv_format));
}

#ifdef __SSE2__
#define PACKED4_TO_PACKED4_ROW_SSE2(shuffle)                      \
{                                                                 \
    __m128i zero = _mm_setzero_si128();                           \
    while (x >= 4) {                                              \
        __m128i yuv = _mm_loadu_si128((__m128i *)srcYUV);         \
        __m128i lo = _mm_unpacklo_epi8(yuv, zero);                \
        __m128i hi = _mm_unpackhi_epi8(yuv, zero);                \
        lo = _mm_shufflelo_epi16(lo, shuffle);                    \
        lo = _mm_shufflehi_epi16(lo, shuffle);                    \
        hi = _mm_shufflelo_epi16(hi, shuffle);                    \
        hi = _mm_shufflehi_epi16(hi, shuffle);                    \
        yuv = _mm_packus_epi16(lo, hi);                           \
        _mm_storeu_si128((__m128i *)dstYUV, yuv);                 \
        srcYUV += 16;                                             \
        dstYUV += 16;                                             \
        x -= 4;                                                   \
    }                                                             \
}
#endif

static int SDL_ConvertPixels_YUY2_to_UYVY(const SDL_YUVInfo *src_yuv_info, SDL_YUVInfo *dst_yuv_info)
{
    unsigned x, y;
    unsigned YUVwidth, YUVheight, srcYUVPitch, srcYUVSkip, dstYUVPitch, dstYUVSkip;
    const Uint8 *srcYUV;
    Uint8 *dstYUV;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    YUVwidth = src_yuv_info->uv_width;
    YUVheight = src_yuv_info->uv_height;
    srcYUVPitch = src_yuv_info->uv_pitch;
    srcYUVSkip = srcYUVPitch - YUVwidth * 4;
    dstYUVPitch = dst_yuv_info->uv_pitch;
    dstYUVSkip = dstYUVPitch - YUVwidth * 4;

    srcYUV = src_yuv_info->planes[0];
    dstYUV = dst_yuv_info->planes[0];

    y = YUVheight;
    while (y--) {
        x = YUVwidth;
#ifdef __SSE2__
        if (use_SSE2) {
            while (x >= 4) {
                __m128i yuv = _mm_loadu_si128((__m128i *)srcYUV);
                __m128i u = _mm_slli_epi16(yuv, 8);
                __m128i v = _mm_srli_epi16(yuv, 8);
                __m128i vu = _mm_or_si128(v, u);
                _mm_storeu_si128((__m128i *)dstYUV, vu);
                srcYUV += 16;
                dstYUV += 16;
                x -= 4;
            }
        }
#endif
        while (x--) {
            Uint32 value = *((Uint32 *)srcYUV);

            // value = ((value & 0xFF00FF) << 8) | ((value >> 8) & 0xFF00FF);

            value = SDL_Swap32(value);
            value = SDL_RorLE32(value, 16);

            *((Uint32 *)dstYUV) = value;

            // Uint8 Y1, U, Y2, V;

            // Y1 = srcYUV[0];
            // U = srcYUV[1];
            // Y2 = srcYUV[2];
            // V = srcYUV[3];
            srcYUV += 4;

            // dstYUV[0] = U;
            // dstYUV[1] = Y1;
            // dstYUV[2] = V;
            // dstYUV[3] = Y2;
            dstYUV += 4;
        }
        srcYUV += srcYUVSkip;
        dstYUV += dstYUVSkip;
    }
    return 0;
}

static int SDL_ConvertPixels_YUY2_to_YVYU(const SDL_YUVInfo *src_yuv_info, SDL_YUVInfo *dst_yuv_info)
{
    unsigned x, y;
    unsigned YUVwidth, YUVheight, srcYUVPitch, srcYUVSkip, dstYUVPitch, dstYUVSkip;
    const Uint8 *srcYUV;
    Uint8 *dstYUV;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    YUVwidth = src_yuv_info->uv_width;
    YUVheight = src_yuv_info->uv_height;
    srcYUVPitch = src_yuv_info->uv_pitch;
    srcYUVSkip = srcYUVPitch - YUVwidth * 4;
    dstYUVPitch = dst_yuv_info->uv_pitch;
    dstYUVSkip = dstYUVPitch - YUVwidth * 4;

    srcYUV = src_yuv_info->planes[0];
    dstYUV = dst_yuv_info->planes[0];

    y = YUVheight;
    while (y--) {
        x = YUVwidth;
#ifdef __SSE2__
        if (use_SSE2) {
            PACKED4_TO_PACKED4_ROW_SSE2(_MM_SHUFFLE(1, 2, 3, 0));
        }
#endif
        while (x--) {
            Uint32 value = *((Uint32 *)srcYUV);
            Uint8 *bytes = (Uint8 *)&value;
            Uint8 tmp = bytes[1];
            bytes[1] = bytes[3];
            bytes[3] = tmp;
            *((Uint32 *)dstYUV) = value;

            // Uint8 Y1, U, Y2, V;
            // Y1 = srcYUV[0];
            // U = srcYUV[1];
            // Y2 = srcYUV[2];
            // V = srcYUV[3];
            srcYUV += 4;

            // dstYUV[0] = Y1;
            // dstYUV[1] = V;
            // dstYUV[2] = Y2;
            // dstYUV[3] = U;
            dstYUV += 4;
        }
        srcYUV += srcYUVSkip;
        dstYUV += dstYUVSkip;
    }
    return 0;
}

/*static int SDL_ConvertPixels_UYVY_to_YUY2(const SDL_YUVInfo *src_yuv_info, SDL_YUVInfo *dst_yuv_info)
{
    unsigned x, y;
    unsigned YUVwidth, YUVheight, srcYUVPitch, srcYUVSkip, dstYUVPitch, dstYUVSkip;
    const Uint8 *srcYUV;
    Uint8 *dstYUV;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    YUVwidth = src_yuv_info->uv_width;
    YUVheight = src_yuv_info->uv_height;
    srcYUVPitch = src_yuv_info->uv_pitch;
    srcYUVSkip = srcYUVPitch - YUVwidth * 4;
    dstYUVPitch = dst_yuv_info->uv_pitch;
    dstYUVSkip = dstYUVPitch - YUVwidth * 4;

    srcYUV = src_yuv_info->planes[0];
    dstYUV = dst_yuv_info->planes[0];

    y = YUVheight;
    while (y--) {
        x = YUVwidth;
#ifdef __SSE2__
        if (use_SSE2) {
            while (x >= 4) {
                __m128i yuv = _mm_loadu_si128((__m128i *)srcYUV);
                __m128i u = _mm_slli_epi16(yuv, 8);
                __m128i v = _mm_srli_epi16(yuv, 8);
                __m128i vu = _mm_or_si128(v, u);
                _mm_storeu_si128((__m128i *)dstYUV, vu);
                srcYUV += 16;
                dstYUV += 16;
                x -= 4;
            }
        }
#endif
        while (x--) {
            Uint8 Y1, U, Y2, V;

            U = srcYUV[0];
            Y1 = srcYUV[1];
            V = srcYUV[2];
            Y2 = srcYUV[3];
            srcYUV += 4;

            dstYUV[0] = Y1;
            dstYUV[1] = U;
            dstYUV[2] = Y2;
            dstYUV[3] = V;
            dstYUV += 4;
        }
        srcYUV += srcYUVSkip;
        dstYUV += dstYUVSkip;
    }
    return 0;
}*/

static int SDL_ConvertPixels_UYVY_to_YVYU(const SDL_YUVInfo *src_yuv_info, SDL_YUVInfo *dst_yuv_info)
{
    unsigned x, y;
    unsigned YUVwidth, YUVheight, srcYUVPitch, srcYUVSkip, dstYUVPitch, dstYUVSkip;
    const Uint8 *srcYUV;
    Uint8 *dstYUV;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    YUVwidth = src_yuv_info->uv_width;
    YUVheight = src_yuv_info->uv_height;
    srcYUVPitch = src_yuv_info->uv_pitch;
    srcYUVSkip = srcYUVPitch - YUVwidth * 4;
    dstYUVPitch = dst_yuv_info->uv_pitch;
    dstYUVSkip = dstYUVPitch - YUVwidth * 4;

    srcYUV = src_yuv_info->planes[0];
    dstYUV = dst_yuv_info->planes[0];

    y = YUVheight;
    while (y--) {
        x = YUVwidth;
#ifdef __SSE2__
        if (use_SSE2) {
            while (x >= 4) {
                __m128i yuv = _mm_loadu_si128((__m128i *)srcYUV);
                __m128i u = _mm_srli_epi32(yuv, 8);
                __m128i v = _mm_slli_epi32(yuv, 24);
                __m128i vu = _mm_or_si128(v, u);
                _mm_storeu_si128((__m128i *)dstYUV, vu);
                srcYUV += 16;
                dstYUV += 16;
                x -= 4;
            }
        }
#endif
        while (x--) {
            Uint32 value = *((Uint32 *)srcYUV);

            value = SDL_RorLE32(value, 8);

            *((Uint32 *)dstYUV) = value;

            // Uint8 Y1, U, Y2, V;
            // U = srcYUV[0];
            // Y1 = srcYUV[1];
            // V = srcYUV[2];
            // Y2 = srcYUV[3];
            srcYUV += 4;

            // dstYUV[0] = Y1;
            // dstYUV[1] = V;
            // dstYUV[2] = Y2;
            // dstYUV[3] = U;
            dstYUV += 4;
        }
        srcYUV += srcYUVSkip;
        dstYUV += dstYUVSkip;
    }
    return 0;
}

/*static int SDL_ConvertPixels_YVYU_to_YUY2(const SDL_YUVInfo *src_yuv_info, SDL_YUVInfo *dst_yuv_info)
{
    unsigned x, y;
    unsigned YUVwidth, YUVheight, srcYUVPitch, srcYUVSkip, dstYUVPitch, dstYUVSkip;
    const Uint8 *srcYUV;
    Uint8 *dstYUV;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    YUVwidth = src_yuv_info->uv_width;
    YUVheight = src_yuv_info->uv_height;
    srcYUVPitch = src_yuv_info->uv_pitch;
    srcYUVSkip = srcYUVPitch - YUVwidth * 4;
    dstYUVPitch = dst_yuv_info->uv_pitch;
    dstYUVSkip = dstYUVPitch - YUVwidth * 4;

    srcYUV = src_yuv_info->planes[0];
    dstYUV = dst_yuv_info->planes[0];

    y = YUVheight;
    while (y--) {
        x = YUVwidth;
#ifdef __SSE2__
        if (use_SSE2) {
            PACKED4_TO_PACKED4_ROW_SSE2(_MM_SHUFFLE(1, 2, 3, 0));
        }
#endif
        while (x--) {
            Uint8 Y1, U, Y2, V;

            Y1 = srcYUV[0];
            V = srcYUV[1];
            Y2 = srcYUV[2];
            U = srcYUV[3];
            srcYUV += 4;

            dstYUV[0] = Y1;
            dstYUV[1] = U;
            dstYUV[2] = Y2;
            dstYUV[3] = V;
            dstYUV += 4;
        }
        srcYUV += srcYUVSkip;
        dstYUV += dstYUVSkip;
    }
    return 0;
}*/

static int SDL_ConvertPixels_YVYU_to_UYVY(const SDL_YUVInfo *src_yuv_info, SDL_YUVInfo *dst_yuv_info)
{
    unsigned x, y;
    unsigned YUVwidth, YUVheight, srcYUVPitch, srcYUVSkip, dstYUVPitch, dstYUVSkip;
    const Uint8 *srcYUV;
    Uint8 *dstYUV;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    YUVwidth = src_yuv_info->uv_width;
    YUVheight = src_yuv_info->uv_height;
    srcYUVPitch = src_yuv_info->uv_pitch;
    srcYUVSkip = srcYUVPitch - YUVwidth * 4;
    dstYUVPitch = dst_yuv_info->uv_pitch;
    dstYUVSkip = dstYUVPitch - YUVwidth * 4;

    srcYUV = src_yuv_info->planes[0];
    dstYUV = dst_yuv_info->planes[0];

    y = YUVheight;
    while (y--) {
        x = YUVwidth;
#ifdef __SSE2__
        if (use_SSE2) {
            while (x >= 4) {
                __m128i yuv = _mm_loadu_si128((__m128i *)srcYUV);
                __m128i u = _mm_slli_epi32(yuv, 8);
                __m128i v = _mm_srli_epi32(yuv, 24);
                __m128i vu = _mm_or_si128(v, u);
                _mm_storeu_si128((__m128i *)dstYUV, vu);
                srcYUV += 16;
                dstYUV += 16;
                x -= 4;
            }
        }
#endif
        while (x--) {
            Uint32 value = *((Uint32 *)srcYUV);

            value = SDL_RolLE32(value, 8);

            *((Uint32 *)dstYUV) = value;

            // Uint8 Y1, U, Y2, V;

            // Y1 = srcYUV[0];
            // V = srcYUV[1];
            // Y2 = srcYUV[2];
            // U = srcYUV[3];
            srcYUV += 4;

            // dstYUV[0] = U;
            // dstYUV[1] = Y1;
            // dstYUV[2] = V;
            // dstYUV[3] = Y2;
            dstYUV += 4;
        }
        srcYUV += srcYUVSkip;
        dstYUV += dstYUVSkip;
    }
    return 0;
}

static int SDL_ConvertPixels_Packed4_to_Packed4(const SDL_YUVInfo *src_yuv_info, SDL_YUVInfo *dst_yuv_info)
{
    switch (src_yuv_info->yuv_format) {
    case SDL_PIXELFORMAT_YUY2:
        switch (dst_yuv_info->yuv_format) {
        case SDL_PIXELFORMAT_UYVY:
            return SDL_ConvertPixels_YUY2_to_UYVY(src_yuv_info, dst_yuv_info);
        case SDL_PIXELFORMAT_YVYU:
            return SDL_ConvertPixels_YUY2_to_YVYU(src_yuv_info, dst_yuv_info);
        default:
            SDL_assume(!"Unknown packed yuv destination format");
            break;
        }
        break;
    case SDL_PIXELFORMAT_UYVY:
        switch (dst_yuv_info->yuv_format) {
        case SDL_PIXELFORMAT_YUY2:
            // return SDL_ConvertPixels_UYVY_to_YUY2(src_yuv_info, dst_yuv_info); -- same as SDL_ConvertPixels_YUY2_to_UYVY
            return SDL_ConvertPixels_YUY2_to_UYVY(src_yuv_info, dst_yuv_info);
        case SDL_PIXELFORMAT_YVYU:
            return SDL_ConvertPixels_UYVY_to_YVYU(src_yuv_info, dst_yuv_info);
        default:
            SDL_assume(!"Unknown packed yuv destination format");
            break;
        }
        break;
    case SDL_PIXELFORMAT_YVYU:
        switch (dst_yuv_info->yuv_format) {
        case SDL_PIXELFORMAT_YUY2:
            // return SDL_ConvertPixels_YVYU_to_YUY2(src_yuv_info, dst_yuv_info); -- same as SDL_ConvertPixels_YUY2_to_YVYU
            return SDL_ConvertPixels_YUY2_to_YVYU(src_yuv_info, dst_yuv_info);
        case SDL_PIXELFORMAT_UYVY:
            return SDL_ConvertPixels_YVYU_to_UYVY(src_yuv_info, dst_yuv_info);
        default:
            SDL_assume(!"Unknown packed yuv destination format");
            break;
        }
        break;
    default:
        SDL_assume(!"Unknown packed yuv source format");
        break;
    }
    return SDL_SetError("SDL_ConvertPixels_Packed4_to_Packed4: Unsupported YUV conversion: %s -> %s", SDL_GetPixelFormatName(src_yuv_info->yuv_format),
                        SDL_GetPixelFormatName(dst_yuv_info->yuv_format));
}

static int SDL_ConvertPixels_Planar2x2_to_Packed4(const SDL_YUVInfo *src_yuv_info, SDL_YUVInfo *dst_yuv_info)
{
    unsigned x, y, lastx, lasty;
    const Uint8 *srcY1, *srcY2, *srcU, *srcV;
    Uint32 srcY_pitch, srcUV_pitch;
    Uint32 srcYSkip, srcUVSkip, srcUV_pixel_stride;
    Uint8 *dstY1, *dstY2, *dstU1, *dstU2, *dstV1, *dstV2;
    Uint32 dstY_pitch, dstUV_pitch;
    Uint32 dstYUVSkip;

    srcY1 = src_yuv_info->y_plane;
    srcU = src_yuv_info->u_plane;
    srcV = src_yuv_info->v_plane;
    srcY_pitch = src_yuv_info->y_pitch;
    srcUV_pitch = src_yuv_info->uv_pitch;

    srcY2 = srcY1 + srcY_pitch;
    srcYSkip = (srcY_pitch - src_yuv_info->y_width);

    if (src_yuv_info->yuv_layout == SDL_YUVLAYOUT_2PLANES) {
        if (src_yuv_info->bpp == 1) {
            // SDL_PIXELFORMAT_NV12, SDL_PIXELFORMAT_NV21
            srcUV_pixel_stride = 2;
            srcUVSkip = srcUV_pitch - 2 * src_yuv_info->uv_width;
        } else {
            // SDL_PIXELFORMAT_P010
            SDL_assert(src_yuv_info->bpp == 2);
            srcUV_pixel_stride = 2 * sizeof(Uint16);
            srcUVSkip = srcUV_pitch - 2 * src_yuv_info->uv_width * sizeof(Uint16);
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
            // shift to the high byte
            srcY1 += 1;
            srcU += 1;
            srcV += 1;
            srcY2 += 1;
#endif
        }
    } else {
        // SDL_PIXELFORMAT_YV12, SDL_PIXELFORMAT_IYUV
        SDL_assert(src_yuv_info->yuv_layout == SDL_YUVLAYOUT_3PLANES);
        SDL_assert(src_yuv_info->bpp == 1);
        srcUV_pixel_stride = 1;
        srcUVSkip = (srcUV_pitch - src_yuv_info->uv_width);
    }

    dstY1 = dst_yuv_info->y_plane;
    dstU1 = dst_yuv_info->u_plane;
    dstV1 = dst_yuv_info->v_plane;
    dstY_pitch = dst_yuv_info->y_pitch;
    dstUV_pitch = dst_yuv_info->uv_pitch;

    dstY2 = dstY1 + dstY_pitch;
    dstU2 = dstU1 + dstUV_pitch;
    dstV2 = dstV1 + dstUV_pitch;
    dstYUVSkip = dstY_pitch - dst_yuv_info->uv_width * 4;

    /* Copy 2x2 blocks of pixels at a time */
    lasty = src_yuv_info->y_height - 1;
    lastx = src_yuv_info->y_width - 1;
    for (y = 0; y < lasty; y += 2) {
        for (x = 0; x < lastx; x += 2) {
            /* Row 1 */
            *dstY1 = *srcY1++;
            dstY1 += 2;
            *dstY1 = *srcY1++;
            dstY1 += 2;
            *dstU1 = *srcU;
            *dstV1 = *srcV;

            /* Row 2 */
            *dstY2 = *srcY2++;
            dstY2 += 2;
            *dstY2 = *srcY2++;
            dstY2 += 2;
            *dstU2 = *srcU;
            *dstV2 = *srcV;

            srcU += srcUV_pixel_stride;
            srcV += srcUV_pixel_stride;
            dstU1 += 4;
            dstU2 += 4;
            dstV1 += 4;
            dstV2 += 4;
        }

        /* Last column */
        if (x == lastx) {
            /* Row 1 */
            *dstY1 = *srcY1;
            dstY1 += 2;
            *dstY1 = *srcY1++;
            dstY1 += 2;
            *dstU1 = *srcU;
            *dstV1 = *srcV;

            /* Row 2 */
            *dstY2 = *srcY2;
            dstY2 += 2;
            *dstY2 = *srcY2++;
            dstY2 += 2;
            *dstU2 = *srcU;
            *dstV2 = *srcV;

            srcU += srcUV_pixel_stride;
            srcV += srcUV_pixel_stride;
            dstU1 += 4;
            dstU2 += 4;
            dstV1 += 4;
            dstV2 += 4;
        }

        srcY1 += srcYSkip + srcY_pitch;
        srcY2 += srcYSkip + srcY_pitch;
        srcU += srcUVSkip;
        srcV += srcUVSkip;
        dstY1 += dstYUVSkip + dstY_pitch;
        dstY2 += dstYUVSkip + dstY_pitch;
        dstU1 += dstYUVSkip + dstUV_pitch;
        dstU2 += dstYUVSkip + dstUV_pitch;
        dstV1 += dstYUVSkip + dstUV_pitch;
        dstV2 += dstYUVSkip + dstUV_pitch;
    }

    /* Last row */
    if (y == lasty) {
        for (x = 0; x < lastx; x += 2) {
            /* Row 1 */
            *dstY1 = *srcY1++;
            dstY1 += 2;
            *dstY1 = *srcY1++;
            dstY1 += 2;
            *dstU1 = *srcU;
            *dstV1 = *srcV;

            srcU += srcUV_pixel_stride;
            srcV += srcUV_pixel_stride;
            dstU1 += 4;
            dstV1 += 4;
        }

        /* Last column */
        if (x == lastx) {
            /* Row 1 */
            *dstY1 = *srcY1;
            dstY1 += 2;
            *dstY1 = *srcY1++;
            dstY1 += 2;
            *dstU1 = *srcU;
            *dstV1 = *srcV;

            srcU += srcUV_pixel_stride;
            srcV += srcUV_pixel_stride;
            dstU1 += 4;
            dstV1 += 4;
        }
    }
    return 0;
}

static int SDL_ConvertPixels_Packed4_to_Planar2x2(const SDL_YUVInfo *src_yuv_info, SDL_YUVInfo *dst_yuv_info)
{
    unsigned x, y, lastx, lasty;
    const Uint8 *srcY1, *srcY2, *srcU1, *srcU2, *srcV1, *srcV2;
    Uint32 srcY_pitch, srcUV_pitch;
    Uint32 srcYUVSkip;
    Uint8 *dstY1, *dstY2, *dstU, *dstV;
    Uint32 dstY_pitch, dstUV_pitch;
    Uint32 dstYSkip, dstUVSkip, dstUV_pixel_stride;

    srcY1 = src_yuv_info->y_plane;
    srcU1 = src_yuv_info->u_plane;
    srcV1 = src_yuv_info->v_plane;
    srcY_pitch = src_yuv_info->y_pitch;
    srcUV_pitch = src_yuv_info->uv_pitch;

    srcY2 = srcY1 + srcY_pitch;
    srcU2 = srcU1 + srcUV_pitch;
    srcV2 = srcV1 + srcUV_pitch;
    srcYUVSkip = srcY_pitch - src_yuv_info->uv_width * 4;

    dstY1 = dst_yuv_info->y_plane;
    dstU = dst_yuv_info->u_plane;
    dstV = dst_yuv_info->v_plane;
    dstY_pitch = dst_yuv_info->y_pitch;
    dstUV_pitch = dst_yuv_info->uv_pitch;

    dstY2 = dstY1 + dstY_pitch;
    dstYSkip = dstY_pitch - dst_yuv_info->y_width;

    if (dst_yuv_info->yuv_layout == SDL_YUVLAYOUT_2PLANES) {
        if (dst_yuv_info->bpp == 1) {
            // SDL_PIXELFORMAT_NV12, SDL_PIXELFORMAT_NV21
            dstUV_pixel_stride = 2;
            dstUVSkip = dstUV_pitch - 2 * dst_yuv_info->uv_width;
        } else {
            // SDL_PIXELFORMAT_P010
            SDL_assert(dst_yuv_info->bpp == 2);
            SDL_memset(dstY1, 0, dst_yuv_info->yuv_size);
            dstUV_pixel_stride = 2 * sizeof(Uint16);
            dstUVSkip = dstUV_pitch - 2 * dst_yuv_info->uv_width * sizeof(Uint16);
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
            // shift to the high byte
            dstY1 += 1;
            dstU += 1;
            dstV += 1;
            dstY2 += 1;
#endif
        }
    } else {
        // SDL_PIXELFORMAT_YV12, SDL_PIXELFORMAT_IYUV
        SDL_assert(dst_yuv_info->yuv_layout == SDL_YUVLAYOUT_3PLANES);
        SDL_assert(dst_yuv_info->bpp == 1);
        dstUV_pixel_stride = 1;
        dstUVSkip = dstUV_pitch - dst_yuv_info->uv_width;
    }

    /* Copy 2x2 blocks of pixels at a time */
    lasty = src_yuv_info->y_height - 1;
    lastx = src_yuv_info->y_width - 1;
    for (y = 0; y < lasty; y += 2) {
        for (x = 0; x < lastx; x += 2) {
            /* Row 1 */
            *dstY1++ = *srcY1;
            srcY1 += 2;
            *dstY1++ = *srcY1;
            srcY1 += 2;

            /* Row 2 */
            *dstY2++ = *srcY2;
            srcY2 += 2;
            *dstY2++ = *srcY2;
            srcY2 += 2;

            *dstU = (Uint8)(((Uint32)*srcU1 + *srcU2) / 2);
            *dstV = (Uint8)(((Uint32)*srcV1 + *srcV2) / 2);

            srcU1 += 4;
            srcU2 += 4;
            srcV1 += 4;
            srcV2 += 4;
            dstU += dstUV_pixel_stride;
            dstV += dstUV_pixel_stride;
        }

        /* Last column */
        if (x == lastx) {
            /* Row 1 */
            *dstY1 = *srcY1;
            srcY1 += 2;
            *dstY1++ = *srcY1;
            srcY1 += 2;

            /* Row 2 */
            *dstY2 = *srcY2;
            srcY2 += 2;
            *dstY2++ = *srcY2;
            srcY2 += 2;

            *dstU = (Uint8)(((Uint32)*srcU1 + *srcU2) / 2);
            *dstV = (Uint8)(((Uint32)*srcV1 + *srcV2) / 2);

            srcU1 += 4;
            srcU2 += 4;
            srcV1 += 4;
            srcV2 += 4;
            dstU += dstUV_pixel_stride;
            dstV += dstUV_pixel_stride;
        }

        srcY1 += srcYUVSkip + srcY_pitch;
        srcY2 += srcYUVSkip + srcY_pitch;
        srcU1 += srcYUVSkip + srcUV_pitch;
        srcU2 += srcYUVSkip + srcUV_pitch;
        srcV1 += srcYUVSkip + srcUV_pitch;
        srcV2 += srcYUVSkip + srcUV_pitch;
        dstY1 += dstYSkip + dstY_pitch;
        dstY2 += dstYSkip + dstY_pitch;
        dstU += dstUVSkip;
        dstV += dstUVSkip;
    }

    /* Last row */
    if (y == lasty) {
        for (x = 0; x < lastx; x += 2) {
            *dstY1++ = *srcY1;
            srcY1 += 2;
            *dstY1++ = *srcY1;
            srcY1 += 2;

            *dstU = *srcU1;
            *dstV = *srcV1;

            srcU1 += 4;
            srcV1 += 4;
            dstU += dstUV_pixel_stride;
            dstV += dstUV_pixel_stride;
        }

        /* Last column */
        if (x == lastx) {
            *dstY1 = *srcY1;
            *dstU = *srcU1;
            *dstV = *srcV1;
        }
    }
    return 0;
}

#endif /* SDL_HAVE_YUV */

int SDL_ConvertPixels_YUV_to_YUV(int width, int height,
                                 Uint32 src_format, const void *src, int src_pitch,
                                 Uint32 dst_format, void *dst, int dst_pitch)
{
#if SDL_HAVE_YUV
    int retval;
    SDL_YUVInfo src_yuv_info, dst_yuv_info;

    SDL_assert(width > 0 && height > 0);
    SDL_assert(src_pitch > 0 && dst_pitch > 0);

    retval = SDL_InitYUVInfo(width, height, src_format, src, src_pitch, &src_yuv_info);
    if (retval < 0) {
        return retval;
    }

    retval = SDL_InitYUVInfo(width, height, dst_format, dst, dst_pitch, &dst_yuv_info);
    if (retval < 0) {
        return retval;
    }

    if (src_yuv_info.yuv_format == dst_yuv_info.yuv_format) {
        return SDL_ConvertPixels_YUV_to_YUV_Copy(&src_yuv_info, &dst_yuv_info);
    }

    if (src_yuv_info.yuv_layout != SDL_YUVLAYOUT_PACKED) {
        if (dst_yuv_info.yuv_layout != SDL_YUVLAYOUT_PACKED) {
            return SDL_ConvertPixels_Planar2x2_to_Planar2x2(&src_yuv_info, &dst_yuv_info);
        } else {
            return SDL_ConvertPixels_Planar2x2_to_Packed4(&src_yuv_info, &dst_yuv_info);
        }
    } else {
        if (dst_yuv_info.yuv_layout != SDL_YUVLAYOUT_PACKED) {
            return SDL_ConvertPixels_Packed4_to_Planar2x2(&src_yuv_info, &dst_yuv_info);
        } else {
            return SDL_ConvertPixels_Packed4_to_Packed4(&src_yuv_info, &dst_yuv_info);
        }
    }
#else
    return SDL_SetError("SDL not built with YUV support");
#endif
}

/* vi: set ts=4 sw=4 expandtab: */
