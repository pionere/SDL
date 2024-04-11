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

static SDL_bool yuv_rgb_sse(
    Uint32 src_format, Uint32 dst_format,
    Uint32 width, Uint32 height,
    const Uint8 *y, const Uint8 *u, const Uint8 *v, Uint32 y_pitch, Uint32 uv_pitch,
    Uint8 *rgb, Uint32 rgb_pitch,
    YCbCrType yuv_type)
{
#ifdef __SSE2__
    if (!SDL_HasSSE2()) {
        return SDL_FALSE;
    }

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
#endif
    return SDL_FALSE;
}

static SDL_bool yuv_rgb_lsx(
    Uint32 src_format, Uint32 dst_format,
    Uint32 width, Uint32 height,
    const Uint8 *y, const Uint8 *u, const Uint8 *v, Uint32 y_pitch, Uint32 uv_pitch,
    Uint8 *rgb, Uint32 rgb_pitch,
    YCbCrType yuv_type)
{
#ifdef __loongarch_sx
    if (!SDL_HasLSX()) {
        return SDL_FALSE;
    }
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
#endif
    return SDL_FALSE;
}

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

    if (yuv_rgb_sse(src_format, dst_format, width, height, yuv_info.y_plane, yuv_info.u_plane, yuv_info.v_plane, yuv_info.y_pitch, yuv_info.uv_pitch, (Uint8 *)dst, dst_pitch, yuv_type)) {
        return 0;
    }

    if (yuv_rgb_lsx(src_format, dst_format, width, height, yuv_info.y_plane, yuv_info.u_plane, yuv_info.v_plane, yuv_info.y_pitch, yuv_info.uv_pitch, (Uint8 *)dst, dst_pitch, yuv_type)) {
        return 0;
    }

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

static int SDL_ConvertPixels_ARGB8888_to_YUV(unsigned width, unsigned height, const void *src, unsigned src_pitch, Uint32 dst_format, void *dst, unsigned dst_pitch)
{
    SDL_YUVInfo yuv_info;
    int retval;
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

    retval = SDL_InitYUVInfo(width, height, dst_format, dst, dst_pitch, &yuv_info);
    if (retval < 0) {
        return retval;
    }

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

    if (yuv_info.yuv_layout != SDL_YUVLAYOUT_PACKED) {
        const Uint8 *curr_row, *next_row;

        Uint8 *plane_y, *plane_u, *plane_v, *plane_interleaved_uv;
        Uint32 y_pitch, y_skip, uv_skip;
        unsigned src_pitch_x_2;

        if (yuv_info.bpp != 1) {
            return SDL_SetError("Unsupported YUV destination format: %s", SDL_GetPixelFormatName(yuv_info.yuv_format));
        }

        width = yuv_info.y_width;
        height = yuv_info.y_height;

        /* Write Y plane */
        y_pitch = yuv_info.y_pitch;
        // if (y_pitch < width) {
        //     return SDL_SetError("Destination pitch is too small, expected at least %d\n", width);
        // }
        y_skip = (y_pitch - width);

        curr_row = (const Uint8 *)src;
        plane_y = yuv_info.y_plane;
        for (j = 0; j < height; j++) {
            for (i = 0; i < width; i++) {
                const Uint32 p1 = ((const Uint32 *)curr_row)[i];
                const Uint32 r = (p1 & 0x00ff0000) >> 16;
                const Uint32 g = (p1 & 0x0000ff00) >> 8;
                const Uint32 b = (p1 & 0x000000ff);
                *plane_y++ = MAKE_Y(r, g, b);
            }
            plane_y += y_skip;
            curr_row += src_pitch;
        }

        /* Write UV planes */
        curr_row = (const Uint8 *)src;
        next_row = (const Uint8 *)src;
        next_row += src_pitch;
        src_pitch_x_2 = src_pitch * 2;

        switch (yuv_info.yuv_format) {
        case SDL_PIXELFORMAT_YV12:
        case SDL_PIXELFORMAT_IYUV:
            /* Write UV planes, not interleaved */
            plane_u = yuv_info.u_plane;
            plane_v = yuv_info.v_plane;
            uv_skip = yuv_info.uv_pitch - yuv_info.uv_width;
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
            plane_interleaved_uv = yuv_info.u_plane;
            uv_skip = yuv_info.uv_pitch - 2 * yuv_info.uv_width;
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
            plane_interleaved_uv = yuv_info.v_plane;
            uv_skip = yuv_info.uv_pitch - 2 * yuv_info.uv_width;
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
    } else { // yuv_info.yuv_layout == SDL_YUVLAYOUT_PACKED
        const Uint8 *curr_row = (const Uint8 *)src;
        Uint8 *plane = yuv_info.planes[0];
        const unsigned row_size = 4 * yuv_info.uv_width;
        unsigned plane_skip;

        SDL_assert(yuv_info.bpp == 1);

        width = yuv_info.y_width;
        height = yuv_info.y_height;

        // if (yuv_info.y_pitch < row_size) {
        //     return SDL_SetError("Destination pitch is too small, expected at least %d\n", row_size);
        // }
        plane_skip = (yuv_info.y_pitch - row_size);

        /* Write YUV plane, packed */
        switch (yuv_info.yuv_format) {
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
    SDL_assert(width > 0 && height > 0);
    SDL_assert(src_pitch > 0 && dst_pitch > 0);

#if 0 /* Doesn't handle odd widths */
    /* RGB24 to FOURCC */
    if (src_format == SDL_PIXELFORMAT_RGB24) {
        int retval;
        SDL_YUVInfo yuv_info;
        Uint8 *y;
        Uint8 *u;
        Uint8 *v;
        Uint32 y_pitch;
        Uint32 uv_pitch;
        YCbCrType yuv_type;

        retval = SDL_InitYUVInfo(width, height, dst_format, dst, dst_pitch, &yuv_info);
        if (retval < 0) {
            return retval;
        }

        y = yuv_info.y_plane;
        u = yuv_info.u_plane;
        v = yuv_info.v_plane;
        y_pitch = yuv_info.y_pitch;
        uv_pitch = yuv_info.uv_pitch;

        if (GetYUVConversionType(width, height, &yuv_type) < 0) {
            return -1;
        }

        rgb24_yuv420_std(width, height, src, src_pitch, y, u, v, y_pitch, uv_pitch, yuv_type);
        return 0;
    }
#endif

    /* ARGB8888 to FOURCC */
    if (src_format == SDL_PIXELFORMAT_ARGB8888) {
        return SDL_ConvertPixels_ARGB8888_to_YUV(width, height, src, src_pitch, dst_format, dst, dst_pitch);
    }

    /* not ARGB8888 to FOURCC : need an intermediate conversion */
    {
        int ret;
        void *tmp;
        unsigned tmp_pitch = (width * sizeof(Uint32));

        tmp = SDL_malloc((size_t)tmp_pitch * height);
        if (!tmp) {
            return SDL_OutOfMemory();
        }

        /* convert src/src_format to tmp/ARGB8888 */
        ret = SDL_ConvertPixels(width, height, src_format, src, src_pitch, SDL_PIXELFORMAT_ARGB8888, tmp, tmp_pitch);
        if (ret < 0) {
            SDL_free(tmp);
            return ret;
        }

        /* convert tmp/ARGB8888 to dst/FOURCC */
        ret = SDL_ConvertPixels_ARGB8888_to_YUV(width, height, tmp, tmp_pitch, dst_format, dst, dst_pitch);
        SDL_free(tmp);
        return ret;
    }
}

static int SDL_ConvertPixels_YUV_to_YUV_Copy(unsigned width, unsigned height, Uint32 format,
                                             const void *src, unsigned src_pitch, void *dst, unsigned dst_pitch)
{
    int retval;
    SDL_YUVInfo src_yuv_info, dst_yuv_info;
    unsigned i, rows, length;

    retval = SDL_InitYUVInfo(width, height, format, src, src_pitch, &src_yuv_info);
    if (retval < 0) {
        return retval;
    }

    retval = SDL_InitYUVInfo(width, height, format, dst, dst_pitch, &dst_yuv_info);
    SDL_assert(retval == 0);

    if (src_pitch == dst_pitch) {
        SDL_memcpy(dst, src, src_yuv_info.yuv_size);
        return 0;
    }

    /* copy the Y plane */
    src = src_yuv_info.planes[0];
    src_pitch = src_yuv_info.y_pitch;
    dst = dst_yuv_info.planes[0];
    dst_pitch = dst_yuv_info.y_pitch;
    rows = src_yuv_info.y_height;
    if (src_yuv_info.yuv_layout == SDL_YUVLAYOUT_PACKED) {
        /* Packed planes -> add UV values */
        length = 4 * src_yuv_info.uv_width;
    } else {
        length = src_yuv_info.y_width;
    }
    length *= src_yuv_info.bpp;
    for (i = rows; i--;) {
        SDL_memcpy(dst, src, length);
        src = (const Uint8 *)src + src_pitch;
        dst = (Uint8 *)dst + dst_pitch;
    }

    if (src_yuv_info.yuv_layout != SDL_YUVLAYOUT_PACKED) {
        /* copy the UV plane(s) */
        rows = src_yuv_info.uv_height;
        length = src_yuv_info.uv_width;
        length *= src_yuv_info.bpp;
        src_pitch = src_yuv_info.uv_pitch;
        dst_pitch = dst_yuv_info.uv_pitch;
        if (src_yuv_info.yuv_layout == SDL_YUVLAYOUT_3PLANES) {
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

static int SDL_ConvertPixels_SwapUVPlanes(unsigned width, unsigned height, const void *src, unsigned src_pitch, void *dst, unsigned dst_pitch)
{
    unsigned y;
    const unsigned UVwidth = (width + 1) / 2;
    const unsigned UVheight = (height + 1) / 2;

    /* Skip the Y plane */
    src = (const Uint8 *)src + height * src_pitch;
    dst = (Uint8 *)dst + height * dst_pitch;

    if (src == dst) {
        unsigned UVpitch = (dst_pitch + 1) / 2;
        Uint8 *tmp;
        Uint8 *row1 = dst;
        Uint8 *row2 = (Uint8 *)dst + UVheight * UVpitch;

        /* Allocate a temporary row for the swap */
        tmp = (Uint8 *)SDL_malloc(UVwidth);
        if (!tmp) {
            return SDL_OutOfMemory();
        }
        for (y = 0; y < UVheight; ++y) {
            SDL_memcpy(tmp, row1, UVwidth);
            SDL_memcpy(row1, row2, UVwidth);
            SDL_memcpy(row2, tmp, UVwidth);
            row1 += UVpitch;
            row2 += UVpitch;
        }
        SDL_free(tmp);
    } else {
        const Uint8 *srcUV;
        Uint8 *dstUV;
        unsigned srcUVPitch = ((src_pitch + 1) / 2);
        unsigned dstUVPitch = ((dst_pitch + 1) / 2);

        /* Copy the first plane */
        srcUV = (const Uint8 *)src;
        dstUV = (Uint8 *)dst + UVheight * dstUVPitch;
        for (y = 0; y < UVheight; ++y) {
            SDL_memcpy(dstUV, srcUV, UVwidth);
            srcUV += srcUVPitch;
            dstUV += dstUVPitch;
        }

        /* Copy the second plane */
        dstUV = (Uint8 *)dst;
        for (y = 0; y < UVheight; ++y) {
            SDL_memcpy(dstUV, srcUV, UVwidth);
            srcUV += srcUVPitch;
            dstUV += dstUVPitch;
        }
    }
    return 0;
}

static int SDL_ConvertPixels_PackUVPlanes_to_NV(unsigned width, unsigned height, const void *src, unsigned src_pitch, void *dst, unsigned dst_pitch, SDL_bool reverseUV)
{
    unsigned x, y;
    const unsigned UVwidth = (width + 1) / 2;
    const unsigned UVheight = (height + 1) / 2;
    const unsigned srcUVPitch = ((src_pitch + 1) / 2);
    const unsigned srcUVPitchLeft = srcUVPitch - UVwidth;
    const unsigned dstUVPitch = ((dst_pitch + 1) / 2) * 2;
    const unsigned dstUVPitchLeft = dstUVPitch - UVwidth * 2;
    const Uint8 *src1, *src2;
    Uint8 *dstUV;
    Uint8 *tmp = NULL;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    /* Skip the Y plane */
    src = (const Uint8 *)src + height * src_pitch;
    dst = (Uint8 *)dst + height * dst_pitch;

    if (src == dst) {
        /* Need to make a copy of the buffer so we don't clobber it while converting */
        tmp = (Uint8 *)SDL_malloc((size_t)2 * UVheight * srcUVPitch);
        if (!tmp) {
            return SDL_OutOfMemory();
        }
        SDL_memcpy(tmp, src, (size_t)2 * UVheight * srcUVPitch);
        src = tmp;
    }

    if (reverseUV) {
        src2 = (const Uint8 *)src;
        src1 = src2 + UVheight * srcUVPitch;
    } else {
        src1 = (const Uint8 *)src;
        src2 = src1 + UVheight * srcUVPitch;
    }
    dstUV = (Uint8 *)dst;

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
        src1 += srcUVPitchLeft;
        src2 += srcUVPitchLeft;
        dstUV += dstUVPitchLeft;
    }

    if (tmp) {
        SDL_free(tmp);
    }
    return 0;
}

static int SDL_ConvertPixels_SplitNV_to_UVPlanes(unsigned width, unsigned height, const void *src, unsigned src_pitch, void *dst, unsigned dst_pitch, SDL_bool reverseUV)
{
    unsigned x, y;
    const unsigned UVwidth = (width + 1) / 2;
    const unsigned UVheight = (height + 1) / 2;
    const unsigned srcUVPitch = ((src_pitch + 1) / 2) * 2;
    const unsigned srcUVPitchLeft = srcUVPitch - UVwidth * 2;
    const unsigned dstUVPitch = ((dst_pitch + 1) / 2);
    const unsigned dstUVPitchLeft = dstUVPitch - UVwidth;
    const Uint8 *srcUV;
    Uint8 *dst1, *dst2;
    Uint8 *tmp = NULL;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    /* Skip the Y plane */
    src = (const Uint8 *)src + height * src_pitch;
    dst = (Uint8 *)dst + height * dst_pitch;

    if (src == dst) {
        /* Need to make a copy of the buffer so we don't clobber it while converting */
        tmp = (Uint8 *)SDL_malloc((size_t)UVheight * srcUVPitch);
        if (!tmp) {
            return SDL_OutOfMemory();
        }
        SDL_memcpy(tmp, src, (size_t)UVheight * srcUVPitch);
        src = tmp;
    }

    if (reverseUV) {
        dst2 = (Uint8 *)dst;
        dst1 = dst2 + UVheight * dstUVPitch;
    } else {
        dst1 = (Uint8 *)dst;
        dst2 = dst1 + UVheight * dstUVPitch;
    }
    srcUV = (const Uint8 *)src;

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
        srcUV += srcUVPitchLeft;
        dst1 += dstUVPitchLeft;
        dst2 += dstUVPitchLeft;
    }

    if (tmp) {
        SDL_free(tmp);
    }
    return 0;
}

static int SDL_ConvertPixels_SwapNV(unsigned width, unsigned height, const void *src, unsigned src_pitch, void *dst, unsigned dst_pitch)
{
    unsigned x, y;
    const unsigned UVwidth = (width + 1) / 2;
    const unsigned UVheight = (height + 1) / 2;
    const unsigned srcUVPitch = ((src_pitch + 1) / 2) * 2;
    const unsigned srcUVPitchLeft = (srcUVPitch - UVwidth * 2) / sizeof(Uint16);
    const unsigned dstUVPitch = ((dst_pitch + 1) / 2) * 2;
    const unsigned dstUVPitchLeft = (dstUVPitch - UVwidth * 2) / sizeof(Uint16);
    const Uint16 *srcUV;
    Uint16 *dstUV;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    /* Skip the Y plane */
    src = (const Uint8 *)src + height * src_pitch;
    dst = (Uint8 *)dst + height * dst_pitch;

    srcUV = (const Uint16 *)src;
    dstUV = (Uint16 *)dst;
    y = UVheight;
    while (y--) {
        x = UVwidth;
#ifdef __SSE2__
        if (use_SSE2) {
            while (x >= 8) {
                __m128i uv = _mm_loadu_si128((__m128i *)srcUV);
                __m128i v = _mm_slli_epi16(uv, 8);
                __m128i u = _mm_srli_epi16(uv, 8);
                __m128i vu = _mm_or_si128(v, u);
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
        srcUV += srcUVPitchLeft;
        dstUV += dstUVPitchLeft;
    }
    return 0;
}

static int SDL_ConvertPixels_Planar2x2_to_Planar2x2(unsigned width, unsigned height,
                                                    Uint32 src_format, const void *src, unsigned src_pitch,
                                                    Uint32 dst_format, void *dst, unsigned dst_pitch)
{
    if (src != dst) {
        /* Copy Y plane */
        unsigned i;
        const Uint8 *srcY = (const Uint8 *)src;
        Uint8 *dstY = (Uint8 *)dst;
        for (i = height; i--;) {
            SDL_memcpy(dstY, srcY, width);
            srcY += src_pitch;
            dstY += dst_pitch;
        }
    }

    switch (src_format) {
    case SDL_PIXELFORMAT_YV12:
        switch (dst_format) {
        case SDL_PIXELFORMAT_IYUV:
            return SDL_ConvertPixels_SwapUVPlanes(width, height, src, src_pitch, dst, dst_pitch);
        case SDL_PIXELFORMAT_NV12:
            return SDL_ConvertPixels_PackUVPlanes_to_NV(width, height, src, src_pitch, dst, dst_pitch, SDL_TRUE);
        case SDL_PIXELFORMAT_NV21:
            return SDL_ConvertPixels_PackUVPlanes_to_NV(width, height, src, src_pitch, dst, dst_pitch, SDL_FALSE);
        case SDL_PIXELFORMAT_P010:
            break;
        default:
            SDL_assume(!"Unsupported YUV format");
            break;
        }
        break;
    case SDL_PIXELFORMAT_IYUV:
        switch (dst_format) {
        case SDL_PIXELFORMAT_YV12:
            return SDL_ConvertPixels_SwapUVPlanes(width, height, src, src_pitch, dst, dst_pitch);
        case SDL_PIXELFORMAT_NV12:
            return SDL_ConvertPixels_PackUVPlanes_to_NV(width, height, src, src_pitch, dst, dst_pitch, SDL_FALSE);
        case SDL_PIXELFORMAT_NV21:
            return SDL_ConvertPixels_PackUVPlanes_to_NV(width, height, src, src_pitch, dst, dst_pitch, SDL_TRUE);
        case SDL_PIXELFORMAT_P010:
            break;
        default:
            SDL_assume(!"Unsupported YUV format");
            break;
        }
        break;
    case SDL_PIXELFORMAT_NV12:
        switch (dst_format) {
        case SDL_PIXELFORMAT_YV12:
            return SDL_ConvertPixels_SplitNV_to_UVPlanes(width, height, src, src_pitch, dst, dst_pitch, SDL_TRUE);
        case SDL_PIXELFORMAT_IYUV:
            return SDL_ConvertPixels_SplitNV_to_UVPlanes(width, height, src, src_pitch, dst, dst_pitch, SDL_FALSE);
        case SDL_PIXELFORMAT_NV21:
            return SDL_ConvertPixels_SwapNV(width, height, src, src_pitch, dst, dst_pitch);
        case SDL_PIXELFORMAT_P010:
            break;
        default:
            SDL_assume(!"Unsupported YUV format");
            break;
        }
        break;
    case SDL_PIXELFORMAT_NV21:
        switch (dst_format) {
        case SDL_PIXELFORMAT_YV12:
            return SDL_ConvertPixels_SplitNV_to_UVPlanes(width, height, src, src_pitch, dst, dst_pitch, SDL_FALSE);
        case SDL_PIXELFORMAT_IYUV:
            return SDL_ConvertPixels_SplitNV_to_UVPlanes(width, height, src, src_pitch, dst, dst_pitch, SDL_TRUE);
        case SDL_PIXELFORMAT_NV12:
            return SDL_ConvertPixels_SwapNV(width, height, src, src_pitch, dst, dst_pitch);
        case SDL_PIXELFORMAT_P010:
            break;
        default:
            SDL_assume(!"Unsupported YUV format");
            break;
        }
        break;
    case SDL_PIXELFORMAT_P010:
        switch (dst_format) {
        case SDL_PIXELFORMAT_YV12:
        case SDL_PIXELFORMAT_IYUV:
        case SDL_PIXELFORMAT_NV12:
        case SDL_PIXELFORMAT_NV21:
            break;
        default:
            SDL_assume(!"Unsupported YUV format");
            break;
        }
        break;
    default:
        SDL_assume(!"Unsupported YUV format");
        break;
    }
    return SDL_SetError("SDL_ConvertPixels_Planar2x2_to_Planar2x2: Unsupported YUV conversion: %s -> %s", SDL_GetPixelFormatName(src_format),
                        SDL_GetPixelFormatName(dst_format));
}

#ifdef __SSE2__
#define PACKED4_TO_PACKED4_ROW_SSE2(shuffle)                      \
    while (x >= 4) {                                              \
        __m128i yuv = _mm_loadu_si128((__m128i *)srcYUV);         \
        __m128i lo = _mm_unpacklo_epi8(yuv, _mm_setzero_si128()); \
        __m128i hi = _mm_unpackhi_epi8(yuv, _mm_setzero_si128()); \
        lo = _mm_shufflelo_epi16(lo, shuffle);                    \
        lo = _mm_shufflehi_epi16(lo, shuffle);                    \
        hi = _mm_shufflelo_epi16(hi, shuffle);                    \
        hi = _mm_shufflehi_epi16(hi, shuffle);                    \
        yuv = _mm_packus_epi16(lo, hi);                           \
        _mm_storeu_si128((__m128i *)dstYUV, yuv);                 \
        srcYUV += 16;                                             \
        dstYUV += 16;                                             \
        x -= 4;                                                   \
    }

#endif

static int SDL_ConvertPixels_YUY2_to_UYVY(unsigned width, unsigned height, const void *src, unsigned src_pitch, void *dst, unsigned dst_pitch)
{
    unsigned x, y;
    const unsigned YUVwidth = (width + 1) / 2;
    const unsigned srcYUVPitchLeft = (src_pitch - YUVwidth * 4);
    const unsigned dstYUVPitchLeft = (dst_pitch - YUVwidth * 4);
    const Uint8 *srcYUV = (const Uint8 *)src;
    Uint8 *dstYUV = (Uint8 *)dst;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    y = height;
    while (y--) {
        x = YUVwidth;
#ifdef __SSE2__
        if (use_SSE2) {
            PACKED4_TO_PACKED4_ROW_SSE2(_MM_SHUFFLE(2, 3, 0, 1));
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
        srcYUV += srcYUVPitchLeft;
        dstYUV += dstYUVPitchLeft;
    }
    return 0;
}

static int SDL_ConvertPixels_YUY2_to_YVYU(unsigned width, unsigned height, const void *src, unsigned src_pitch, void *dst, unsigned dst_pitch)
{
    unsigned x, y;
    const unsigned YUVwidth = (width + 1) / 2;
    const unsigned srcYUVPitchLeft = (src_pitch - YUVwidth * 4);
    const unsigned dstYUVPitchLeft = (dst_pitch - YUVwidth * 4);
    const Uint8 *srcYUV = (const Uint8 *)src;
    Uint8 *dstYUV = (Uint8 *)dst;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    y = height;
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
        srcYUV += srcYUVPitchLeft;
        dstYUV += dstYUVPitchLeft;
    }
    return 0;
}

/*static int SDL_ConvertPixels_UYVY_to_YUY2(unsigned width, unsigned height, const void *src, unsigned src_pitch, void *dst, unsigned dst_pitch)
{
    unsigned x, y;
    const unsigned YUVwidth = (width + 1) / 2;
    const unsigned srcYUVPitchLeft = (src_pitch - YUVwidth * 4);
    const unsigned dstYUVPitchLeft = (dst_pitch - YUVwidth * 4);
    const Uint8 *srcYUV = (const Uint8 *)src;
    Uint8 *dstYUV = (Uint8 *)dst;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    y = height;
    while (y--) {
        x = YUVwidth;
#ifdef __SSE2__
        if (use_SSE2) {
            PACKED4_TO_PACKED4_ROW_SSE2(_MM_SHUFFLE(2, 3, 0, 1));
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
        srcYUV += srcYUVPitchLeft;
        dstYUV += dstYUVPitchLeft;
    }
    return 0;
}*/

static int SDL_ConvertPixels_UYVY_to_YVYU(unsigned width, unsigned height, const void *src, unsigned src_pitch, void *dst, unsigned dst_pitch)
{
    unsigned x, y;
    const unsigned YUVwidth = (width + 1) / 2;
    const unsigned srcYUVPitchLeft = (src_pitch - YUVwidth * 4);
    const unsigned dstYUVPitchLeft = (dst_pitch - YUVwidth * 4);
    const Uint8 *srcYUV = (const Uint8 *)src;
    Uint8 *dstYUV = (Uint8 *)dst;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    y = height;
    while (y--) {
        x = YUVwidth;
#ifdef __SSE2__
        if (use_SSE2) {
            PACKED4_TO_PACKED4_ROW_SSE2(_MM_SHUFFLE(0, 3, 2, 1));
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
        srcYUV += srcYUVPitchLeft;
        dstYUV += dstYUVPitchLeft;
    }
    return 0;
}

/*static int SDL_ConvertPixels_YVYU_to_YUY2(unsigned width, unsigned height, const void *src, unsigned src_pitch, void *dst, unsigned dst_pitch)
{
    unsigned x, y;
    const unsigned YUVwidth = (width + 1) / 2;
    const unsigned srcYUVPitchLeft = (src_pitch - YUVwidth * 4);
    const unsigned dstYUVPitchLeft = (dst_pitch - YUVwidth * 4);
    const Uint8 *srcYUV = (const Uint8 *)src;
    Uint8 *dstYUV = (Uint8 *)dst;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    y = height;
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
        srcYUV += srcYUVPitchLeft;
        dstYUV += dstYUVPitchLeft;
    }
    return 0;
}*/

static int SDL_ConvertPixels_YVYU_to_UYVY(unsigned width, unsigned height, const void *src, unsigned src_pitch, void *dst, unsigned dst_pitch)
{
    unsigned x, y;
    const unsigned YUVwidth = (width + 1) / 2;
    const unsigned srcYUVPitchLeft = (src_pitch - YUVwidth * 4);
    const unsigned dstYUVPitchLeft = (dst_pitch - YUVwidth * 4);
    const Uint8 *srcYUV = (const Uint8 *)src;
    Uint8 *dstYUV = (Uint8 *)dst;
#ifdef __SSE2__
    const SDL_bool use_SSE2 = SDL_HasSSE2();
#endif

    y = height;
    while (y--) {
        x = YUVwidth;
#ifdef __SSE2__
        if (use_SSE2) {
            PACKED4_TO_PACKED4_ROW_SSE2(_MM_SHUFFLE(2, 1, 0, 3));
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
        srcYUV += srcYUVPitchLeft;
        dstYUV += dstYUVPitchLeft;
    }
    return 0;
}

static int SDL_ConvertPixels_Packed4_to_Packed4(unsigned width, unsigned height,
                                                Uint32 src_format, const void *src, unsigned src_pitch,
                                                Uint32 dst_format, void *dst, unsigned dst_pitch)
{
    switch (src_format) {
    case SDL_PIXELFORMAT_YUY2:
        switch (dst_format) {
        case SDL_PIXELFORMAT_UYVY:
            return SDL_ConvertPixels_YUY2_to_UYVY(width, height, src, src_pitch, dst, dst_pitch);
        case SDL_PIXELFORMAT_YVYU:
            return SDL_ConvertPixels_YUY2_to_YVYU(width, height, src, src_pitch, dst, dst_pitch);
        default:
            SDL_assume(!"Unknown packed yuv destination format");
            break;
        }
        break;
    case SDL_PIXELFORMAT_UYVY:
        switch (dst_format) {
        case SDL_PIXELFORMAT_YUY2:
            // return SDL_ConvertPixels_UYVY_to_YUY2(width, height, src, src_pitch, dst, dst_pitch); -- same as SDL_ConvertPixels_YUY2_to_UYVY
            return SDL_ConvertPixels_YUY2_to_UYVY(width, height, src, src_pitch, dst, dst_pitch);
        case SDL_PIXELFORMAT_YVYU:
            return SDL_ConvertPixels_UYVY_to_YVYU(width, height, src, src_pitch, dst, dst_pitch);
        default:
            SDL_assume(!"Unknown packed yuv destination format");
            break;
        }
        break;
    case SDL_PIXELFORMAT_YVYU:
        switch (dst_format) {
        case SDL_PIXELFORMAT_YUY2:
            // return SDL_ConvertPixels_YVYU_to_YUY2(width, height, src, src_pitch, dst, dst_pitch); -- same as SDL_ConvertPixels_YUY2_to_YVYU
            return SDL_ConvertPixels_YUY2_to_YVYU(width, height, src, src_pitch, dst, dst_pitch);
        case SDL_PIXELFORMAT_UYVY:
            return SDL_ConvertPixels_YVYU_to_UYVY(width, height, src, src_pitch, dst, dst_pitch);
        default:
            SDL_assume(!"Unknown packed yuv destination format");
            break;
        }
        break;
    default:
        SDL_assume(!"Unknown packed yuv source format");
        break;
    }
    return SDL_SetError("SDL_ConvertPixels_Packed4_to_Packed4: Unsupported YUV conversion: %s -> %s", SDL_GetPixelFormatName(src_format),
                        SDL_GetPixelFormatName(dst_format));
}

static int SDL_ConvertPixels_Planar2x2_to_Packed4(unsigned width, unsigned height,
                                                  Uint32 src_format, const void *src, unsigned src_pitch,
                                                  Uint32 dst_format, void *dst, unsigned dst_pitch)
{
    int retval;
    SDL_YUVInfo src_yuv_info, dst_yuv_info;
    unsigned x, y, lastx, lasty;
    const Uint8 *srcY1, *srcY2, *srcU, *srcV;
    Uint32 srcY_pitch, srcUV_pitch;
    Uint32 srcY_pitch_left, srcUV_pitch_left, srcUV_pixel_stride;
    Uint8 *dstY1, *dstY2, *dstU1, *dstU2, *dstV1, *dstV2;
    Uint32 dstY_pitch, dstUV_pitch;
    Uint32 dst_pitch_left;

    if (src == dst) {
        return SDL_SetError("Can't change YUV plane types in-place");
    }

    retval = SDL_InitYUVInfo(width, height, src_format, src, src_pitch, &src_yuv_info);
    SDL_assert(retval == 0);

    srcY1 = src_yuv_info.y_plane;
    srcU = src_yuv_info.u_plane;
    srcV = src_yuv_info.v_plane;
    srcY_pitch = src_yuv_info.y_pitch;
    srcUV_pitch = src_yuv_info.uv_pitch;

    srcY2 = srcY1 + srcY_pitch;
    srcY_pitch_left = (srcY_pitch - width);

    switch (src_format) {
    case SDL_PIXELFORMAT_NV12:
    case SDL_PIXELFORMAT_NV21:
        srcUV_pixel_stride = 2;
        srcUV_pitch_left = (srcUV_pitch - 2 * ((width + 1) / 2));
        break;
    case SDL_PIXELFORMAT_YV12:
    case SDL_PIXELFORMAT_IYUV:
        srcUV_pixel_stride = 1;
        srcUV_pitch_left = (srcUV_pitch - ((width + 1) / 2));
        break;
    case SDL_PIXELFORMAT_P010:
        return SDL_Unsupported();
    default:
        SDL_assume(!"Unknown yuv format");
        return -1;
    }

    retval = SDL_InitYUVInfo(width, height, dst_format, dst, dst_pitch, &dst_yuv_info);
    SDL_assert(retval == 0);

    dstY1 = dst_yuv_info.y_plane;
    dstU1 = dst_yuv_info.u_plane;
    dstV1 = dst_yuv_info.v_plane;
    dstY_pitch = dst_yuv_info.y_pitch;
    dstUV_pitch = dst_yuv_info.uv_pitch;

    dstY2 = dstY1 + dstY_pitch;
    dstU2 = dstU1 + dstUV_pitch;
    dstV2 = dstV1 + dstUV_pitch;
    dst_pitch_left = (dstY_pitch - 4 * ((width + 1) / 2));

    /* Copy 2x2 blocks of pixels at a time */
    lasty = height - 1;
    lastx = width - 1;
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

        srcY1 += srcY_pitch_left + srcY_pitch;
        srcY2 += srcY_pitch_left + srcY_pitch;
        srcU += srcUV_pitch_left;
        srcV += srcUV_pitch_left;
        dstY1 += dst_pitch_left + dstY_pitch;
        dstY2 += dst_pitch_left + dstY_pitch;
        dstU1 += dst_pitch_left + dstUV_pitch;
        dstU2 += dst_pitch_left + dstUV_pitch;
        dstV1 += dst_pitch_left + dstUV_pitch;
        dstV2 += dst_pitch_left + dstUV_pitch;
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

static int SDL_ConvertPixels_Packed4_to_Planar2x2(unsigned width, unsigned height,
                                                  Uint32 src_format, const void *src, unsigned src_pitch,
                                                  Uint32 dst_format, void *dst, unsigned dst_pitch)
{
    int retval;
    SDL_YUVInfo src_yuv_info, dst_yuv_info;
    unsigned x, y, lastx, lasty;
    const Uint8 *srcY1, *srcY2, *srcU1, *srcU2, *srcV1, *srcV2;
    Uint32 srcY_pitch, srcUV_pitch;
    Uint32 src_pitch_left;
    Uint8 *dstY1, *dstY2, *dstU, *dstV;
    Uint32 dstY_pitch, dstUV_pitch;
    Uint32 dstY_pitch_left, dstUV_pitch_left, dstUV_pixel_stride;

    if (src == dst) {
        return SDL_SetError("Can't change YUV plane types in-place");
    }

    retval = SDL_InitYUVInfo(width, height, src_format, src, src_pitch, &src_yuv_info);
    SDL_assert(retval == 0);

    srcY1 = src_yuv_info.y_plane;
    srcU1 = src_yuv_info.u_plane;
    srcV1 = src_yuv_info.v_plane;
    srcY_pitch = src_yuv_info.y_pitch;
    srcUV_pitch = src_yuv_info.uv_pitch;

    srcY2 = srcY1 + srcY_pitch;
    srcU2 = srcU1 + srcUV_pitch;
    srcV2 = srcV1 + srcUV_pitch;
    src_pitch_left = (srcY_pitch - 4 * ((width + 1) / 2));

    retval = SDL_InitYUVInfo(width, height, dst_format, dst, dst_pitch, &dst_yuv_info);
    SDL_assert(retval == 0);

    dstY1 = dst_yuv_info.y_plane;
    dstU = dst_yuv_info.u_plane;
    dstV = dst_yuv_info.v_plane;
    dstY_pitch = dst_yuv_info.y_pitch;
    dstUV_pitch = dst_yuv_info.uv_pitch;

    dstY2 = dstY1 + dstY_pitch;
    dstY_pitch_left = (dstY_pitch - width);

    switch (dst_format) {
    case SDL_PIXELFORMAT_NV12:
    case SDL_PIXELFORMAT_NV21:
        dstUV_pixel_stride = 2;
        dstUV_pitch_left = (dstUV_pitch - 2 * ((width + 1) / 2));
        break;
    case SDL_PIXELFORMAT_YV12:
    case SDL_PIXELFORMAT_IYUV:
        dstUV_pixel_stride = 1;
        dstUV_pitch_left = (dstUV_pitch - ((width + 1) / 2));
        break;
    case SDL_PIXELFORMAT_P010:
        return SDL_Unsupported();
    default:
        SDL_assume(!"Unknown yuv format");
        return -1;
    }

    /* Copy 2x2 blocks of pixels at a time */
    lasty = height - 1;
    lastx = width - 1;
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

        srcY1 += src_pitch_left + srcY_pitch;
        srcY2 += src_pitch_left + srcY_pitch;
        srcU1 += src_pitch_left + srcUV_pitch;
        srcU2 += src_pitch_left + srcUV_pitch;
        srcV1 += src_pitch_left + srcUV_pitch;
        srcV2 += src_pitch_left + srcUV_pitch;
        dstY1 += dstY_pitch_left + dstY_pitch;
        dstY2 += dstY_pitch_left + dstY_pitch;
        dstU += dstUV_pitch_left;
        dstV += dstUV_pitch_left;
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
    SDL_bool src_planar = SDL_FALSE;
    SDL_bool dst_planar = SDL_FALSE;

    SDL_assert(width > 0 && height > 0);
    SDL_assert(src_pitch > 0 && dst_pitch > 0);
#if SDL_HAVE_YUV
    if (src_format == dst_format) {
        if (src == dst) {
            /* Nothing to do */
            return 0;
        }
        return SDL_ConvertPixels_YUV_to_YUV_Copy(width, height, src_format, src, src_pitch, dst, dst_pitch);
    }

    if (IsPlanar2x2Format(src_format)) {
        src_planar = SDL_TRUE;
    } else if (!IsPacked4Format(src_format)) {
        return SDL_SetError("SDL_ConvertPixels_YUV_to_YUV: Unsupported YUV conversion from %s", SDL_GetPixelFormatName(src_format));
    }
    if (IsPlanar2x2Format(dst_format)) {
        dst_planar = SDL_TRUE;
    } else if (!IsPacked4Format(dst_format)) {
        return SDL_SetError("SDL_ConvertPixels_YUV_to_YUV: Unsupported YUV conversion to %s", SDL_GetPixelFormatName(dst_format));
    }

    if (src_planar) {
        if (dst_planar) {
            return SDL_ConvertPixels_Planar2x2_to_Planar2x2(width, height, src_format, src, src_pitch, dst_format, dst, dst_pitch);
        } else {
            return SDL_ConvertPixels_Planar2x2_to_Packed4(width, height, src_format, src, src_pitch, dst_format, dst, dst_pitch);
        }
    } else {
        if (dst_planar) {
            return SDL_ConvertPixels_Packed4_to_Planar2x2(width, height, src_format, src, src_pitch, dst_format, dst, dst_pitch);
        } else {
            return SDL_ConvertPixels_Packed4_to_Packed4(width, height, src_format, src, src_pitch, dst_format, dst, dst_pitch);
        }
    }
#else
    return SDL_SetError("SDL not built with YUV support");
#endif
}

/* vi: set ts=4 sw=4 expandtab: */
