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

/* This is the software implementation of the YUV texture support */

#if SDL_HAVE_YUV

#include "SDL_cpuinfo.h"

#include "SDL_yuv_sw_c.h"
#include "../video/SDL_yuv_c.h"

struct SDL_SW_YUVTexture
{
    Uint32 format;
    int w, h;
    Uint8 *pixels;

    /* These are just so we don't have to allocate them separately */
    Uint16 pitches[3];
    Uint8 *planes[3];

    /* This is a temporary surface in case we have to stretch copy */
    SDL_Surface *stretch;
    SDL_Surface *display;
};

SDL_SW_YUVTexture *SDL_SW_CreateYUVTexture(Uint32 format, int w, int h)
{
    SDL_SW_YUVTexture *swdata;

    switch (format) {
    case SDL_PIXELFORMAT_YV12:
    case SDL_PIXELFORMAT_IYUV:
    case SDL_PIXELFORMAT_YUY2:
    case SDL_PIXELFORMAT_UYVY:
    case SDL_PIXELFORMAT_YVYU:
    case SDL_PIXELFORMAT_NV12:
    case SDL_PIXELFORMAT_NV21:
        break;
    default:
        SDL_SetError("Unsupported YUV format");
        return NULL;
    }

    swdata = (SDL_SW_YUVTexture *)SDL_calloc(1, sizeof(*swdata));
    if (!swdata) {
        SDL_OutOfMemory();
        return NULL;
    }

    swdata->format = format;
    swdata->w = w;
    swdata->h = h;
    {
        size_t dst_size;
        if (SDL_CalculateYUVSize(format, w, h, &dst_size, NULL) < 0) {
            SDL_free(swdata);
            SDL_OutOfMemory();
            return NULL;
        }
        swdata->pixels = (Uint8 *)SDL_SIMDAlloc(dst_size);
        if (!swdata->pixels) {
            SDL_free(swdata);
            SDL_OutOfMemory();
            return NULL;
        }
    }

    /* Find the pitch and offset values for the texture */
    switch (format) {
    case SDL_PIXELFORMAT_YV12:
    case SDL_PIXELFORMAT_IYUV:
        swdata->pitches[0] = w;
        swdata->pitches[1] = (swdata->pitches[0] + 1) / 2;
        swdata->pitches[2] = (swdata->pitches[0] + 1) / 2;
        swdata->planes[0] = swdata->pixels;
        swdata->planes[1] = swdata->planes[0] + swdata->pitches[0] * h;
        swdata->planes[2] = swdata->planes[1] + swdata->pitches[1] * ((h + 1) / 2);
        break;
    case SDL_PIXELFORMAT_YUY2:
    case SDL_PIXELFORMAT_UYVY:
    case SDL_PIXELFORMAT_YVYU:
        swdata->pitches[0] = ((w + 1) / 2) * 4;
        swdata->planes[0] = swdata->pixels;
        break;

    case SDL_PIXELFORMAT_NV12:
    case SDL_PIXELFORMAT_NV21:
        swdata->pitches[0] = w;
        swdata->pitches[1] = 2 * ((swdata->pitches[0] + 1) / 2);
        swdata->planes[0] = swdata->pixels;
        swdata->planes[1] = swdata->planes[0] + swdata->pitches[0] * h;
        break;

    default:
        SDL_assume(!"Unknown pixel format");
        break;
    }

    /* We're all done.. */
    return swdata;
}

int SDL_SW_UpdateYUVTexture(SDL_SW_YUVTexture *swdata, const SDL_Rect *rect,
                            const void *pixels, int pitch)
{
    switch (swdata->format) {
    case SDL_PIXELFORMAT_YV12:
    case SDL_PIXELFORMAT_IYUV:
        if (rect->x == 0 && rect->y == 0 &&
            rect->w == swdata->w && rect->h == swdata->h) {
            SDL_memcpy(swdata->pixels, pixels,
                       (size_t)(swdata->h * swdata->w) + 2 * ((swdata->h + 1) / 2) * ((swdata->w + 1) / 2));
        } else {
            Uint8 *src, *dst;
            int row;
            size_t length;
            unsigned dst_pitch, dst_pitch1, dst_pitch2;

            /* Copy the Y plane */
            src = (Uint8 *)pixels;
            dst_pitch = swdata->pitches[0];
            dst = swdata->planes[0] + rect->y * dst_pitch + rect->x;
            length = rect->w;
            for (row = 0; row < rect->h; ++row) {
                SDL_memcpy(dst, src, length);
                src += pitch;
                dst += dst_pitch;
            }

            /* Copy the next plane */
            src = (Uint8 *)pixels + rect->h * pitch;
            dst_pitch1 = swdata->pitches[1];
            dst = swdata->planes[1];
            dst += rect->y / 2 * dst_pitch1 + rect->x / 2;
            length = (rect->w + 1) / 2;
            for (row = 0; row < (rect->h + 1) / 2; ++row) {
                SDL_memcpy(dst, src, length);
                src += (pitch + 1) / 2;
                dst += dst_pitch1;
            }

            /* Copy the next plane */
            src = (Uint8 *)pixels + rect->h * pitch + ((rect->h + 1) / 2) * ((pitch + 1) / 2);
            dst_pitch2 = dst_pitch1; // swdata->pitches[2];
            dst = swdata->planes[2];
            dst += rect->y / 2 * dst_pitch2 + rect->x / 2;
            length = (rect->w + 1) / 2;
            for (row = 0; row < (rect->h + 1) / 2; ++row) {
                SDL_memcpy(dst, src, length);
                src += (pitch + 1) / 2;
                dst += dst_pitch2;
            }
        }
        break;
    case SDL_PIXELFORMAT_YUY2:
    case SDL_PIXELFORMAT_UYVY:
    case SDL_PIXELFORMAT_YVYU:
    {
        Uint8 *src, *dst;
        int row;
        unsigned dst_pitch;
        size_t length;

        src = (Uint8 *)pixels;
        dst_pitch = swdata->pitches[0];
        dst =
            swdata->planes[0] + rect->y * dst_pitch +
            rect->x * 2;
        length = 4 * (((size_t)rect->w + 1) / 2);
        for (row = 0; row < rect->h; ++row) {
            SDL_memcpy(dst, src, length);
            src += pitch;
            dst += dst_pitch;
        }
    } break;
    case SDL_PIXELFORMAT_NV12:
    case SDL_PIXELFORMAT_NV21:
    {
        if (rect->x == 0 && rect->y == 0 && rect->w == swdata->w && rect->h == swdata->h) {
            SDL_memcpy(swdata->pixels, pixels,
                       (size_t)(swdata->h * swdata->w) + 2 * ((swdata->h + 1) / 2) * ((swdata->w + 1) / 2));
        } else {

            Uint8 *src, *dst;
            int row;
            unsigned dst_pitch, dst_pitch1;
            size_t length;

            /* Copy the Y plane */
            src = (Uint8 *)pixels;
            dst_pitch = swdata->pitches[0];
            dst = swdata->planes[0] + rect->y * dst_pitch + rect->x;
            length = rect->w;
            for (row = 0; row < rect->h; ++row) {
                SDL_memcpy(dst, src, length);
                src += pitch;
                dst += dst_pitch;
            }

            /* Copy the next plane */
            src = (Uint8 *)pixels + rect->h * pitch;
            dst_pitch1 = swdata->pitches[1];
            dst = swdata->planes[1];
            dst += ((rect->y + 1) / 2) * dst_pitch1 + 2 * (rect->x / 2);
            length = 2 * (((size_t)rect->w + 1) / 2);
            for (row = 0; row < (rect->h + 1) / 2; ++row) {
                SDL_memcpy(dst, src, length);
                src += 2 * ((pitch + 1) / 2);
                dst += dst_pitch1;
            }
        }
    }
    }
    return 0;
}

int SDL_SW_UpdateYUVTexturePlanar(SDL_SW_YUVTexture *swdata, const SDL_Rect *rect,
                                  const Uint8 *Yplane, int Ypitch,
                                  const Uint8 *Uplane, int Upitch,
                                  const Uint8 *Vplane, int Vpitch)
{
    const Uint8 *src;
    Uint8 *dst;
    unsigned src_pitch, src_pitch1, src_pitch2;
    unsigned dst_pitch, dst_pitch1, dst_pitch2;
    unsigned rows, rows1, rows2, row;
    size_t length;

    /* Copy the Y plane */
    src = Yplane;
    dst = swdata->planes[0];
    // -- move to the selected rectangle
    dst_pitch = swdata->pitches[0];
    dst += rect->y * dst_pitch + rect->x;
    // -- copy the bytes
    src_pitch = Ypitch;
    length = rect->w;
    rows = rect->h;
    for (row = 0; row < rows; ++row) {
        SDL_memcpy(dst, src, length);
        src += src_pitch;
        dst += dst_pitch;
    }

    /* Copy the U plane */
    src = Uplane;
    if (swdata->format == SDL_PIXELFORMAT_IYUV) {
        dst_pitch1 = swdata->pitches[1];
        dst = swdata->planes[1];
    } else {
        // SDL_PIXELFORMAT_YV12
        dst_pitch1 = swdata->pitches[2];
        dst = swdata->planes[2];
    }
    // -- move to the selected rectangle
    dst += ((unsigned)rect->y / 2) * dst_pitch1 + (unsigned)rect->x / 2;
    // -- copy the bytes
    src_pitch1 = Upitch;
    length = ((unsigned)rect->w + 1) / 2;
    rows1 = ((unsigned)rect->h + 1) / 2;
    for (row = 0; row < rows1; ++row) {
        SDL_memcpy(dst, src, length);
        src += src_pitch1;
        dst += dst_pitch1;
    }

    /* Copy the V plane */
    src = Vplane;
    if (swdata->format == SDL_PIXELFORMAT_IYUV) {
        dst_pitch2 = swdata->pitches[2];
        dst = swdata->planes[2];
    } else {
        dst_pitch2 = swdata->pitches[1];
        dst = swdata->planes[1];
    }
    // -- move to the selected rectangle
    dst += ((unsigned)rect->y / 2) * dst_pitch2 + (unsigned)rect->x / 2;
    // -- copy the bytes
    src_pitch2 = Vpitch;
    // length = ((unsigned)rect->w + 1) / 2;
    // rows2 = ((unsigned)rect->h + 1) / 2;
    rows2 = rows1;
    for (row = 0; row < rows2; ++row) {
        SDL_memcpy(dst, src, length);
        src += src_pitch2;
        dst += dst_pitch2;
    }
    return 0;
}

int SDL_SW_UpdateNVTexturePlanar(SDL_SW_YUVTexture *swdata, const SDL_Rect *rect,
                                 const Uint8 *Yplane, int Ypitch,
                                 const Uint8 *UVplane, int UVpitch)
{
    const Uint8 *src;
    Uint8 *dst;
    unsigned src_pitch, src_pitch1;
    unsigned dst_pitch, dst_pitch1;
    unsigned rows, rows1, row;
    size_t length;
    const unsigned ubbp = 1;

    /* Copy the Y plane */
    src = Yplane;
    dst = swdata->planes[0];
    // -- move to the selected rectangle
    dst_pitch = swdata->pitches[0];
    dst += rect->y * dst_pitch + rect->x * ubbp;
    // -- copy the bytes
    src_pitch = Ypitch;
    length = rect->w * ubbp;
    rows = rect->h;
    for (row = 0; row < rows; ++row) {
        SDL_memcpy(dst, src, length);
        src += src_pitch;
        dst += dst_pitch;
    }

    /* Copy the UV or VU plane */
    src = UVplane;
    dst = swdata->planes[1];
    // -- move to the selected rectangle
    dst_pitch1 = swdata->pitches[1];
    dst += rect->y * dst_pitch1 + rect->x * ubbp;
    // -- copy the bytes
    src_pitch1 = UVpitch;
    length = (((unsigned)rect->w + 1) / 2) * 2 * ubbp;
    rows1 = ((unsigned)rect->h + 1) / 2;
    for (row = 0; row < rows1; ++row) {
        SDL_memcpy(dst, src, length);
        src += src_pitch1;
        dst += dst_pitch1;
    }

    return 0;
}

int SDL_SW_LockYUVTexture(SDL_SW_YUVTexture *swdata, const SDL_Rect *rect,
                          void **pixels, int *pitch)
{
    int sw_pitch;
    unsigned offset = 0;
    SDL_assert(swdata != NULL);
    SDL_assert(rect != NULL);

    sw_pitch = swdata->pitches[0];

    if (rect->x != 0 || rect->y != 0) {
        switch (swdata->format) {
        case SDL_PIXELFORMAT_YUY2:
        case SDL_PIXELFORMAT_UYVY:
        case SDL_PIXELFORMAT_YVYU:
            break;
        case SDL_PIXELFORMAT_YV12:
        case SDL_PIXELFORMAT_IYUV:
        case SDL_PIXELFORMAT_NV12:
        case SDL_PIXELFORMAT_NV21:
        case SDL_PIXELFORMAT_P010:
            return SDL_SetError("YV12, IYUV, NV12, NV21, P010 textures only support full surface locks");
        default:
            SDL_assume(!"Unknown pixel format");
            break;
        }

        offset = rect->y * sw_pitch + rect->x * 2;
    }
    *pixels = swdata->planes[0] + offset;
    *pitch = sw_pitch;
    return 0;
}

void SDL_SW_UnlockYUVTexture(SDL_SW_YUVTexture *swdata)
{
}

int SDL_SW_CopyYUVToRGB(SDL_SW_YUVTexture *swdata, const SDL_Rect *srcrect,
                        Uint32 target_format, int w, int h, void *pixels,
                        int pitch)
{
    SDL_bool stretch;
    int retval;

    /* Make sure we're set up to display in the desired format */
    if (swdata->display && target_format != swdata->display->format->format) {
        SDL_FreeSurface(swdata->display);
        swdata->display = NULL;
        SDL_FreeSurface(swdata->stretch);
        swdata->stretch = NULL;
    }

    stretch = SDL_FALSE;
    if (srcrect->x || srcrect->y || srcrect->w < swdata->w || srcrect->h < swdata->h) {
        /* The source rectangle has been clipped.
           Using a scratch surface is easier than adding clipped
           source support to all the blitters, plus that would
           slow them down in the general unclipped case.
         */
        stretch = SDL_TRUE;
    } else if ((srcrect->w != w) || (srcrect->h != h)) {
        stretch = SDL_TRUE;
    }
    if (stretch) {
        if (swdata->display) {
            swdata->display->w = w;
            swdata->display->h = h;
            swdata->display->pixels = pixels;
            swdata->display->pitch = pitch;
        } else {
            swdata->display = SDL_CreateRGBSurfaceWithFormatFrom(pixels, w, h, 0, pitch, target_format);
            if (!swdata->display) {
                return -1;
            }
        }
        if (!swdata->stretch) {
            swdata->stretch = SDL_CreateRGBSurfaceWithFormat(0, swdata->w, swdata->h, 0, target_format);
            if (!swdata->stretch) {
                return -1;
            }
        }
        pixels = swdata->stretch->pixels;
        pitch = swdata->stretch->pitch;
    }

    retval = SDL_ConvertPixels(swdata->w, swdata->h, swdata->format,
                          swdata->planes[0], swdata->pitches[0],
                          target_format, pixels, pitch);
    if (retval == 0 && stretch) {
        retval = SDL_SoftStretch(swdata->stretch, srcrect, swdata->display, NULL);
    }
    return retval;
}

void SDL_SW_DestroyYUVTexture(SDL_SW_YUVTexture *swdata)
{
    if (swdata) {
        SDL_SIMDFree(swdata->pixels);
        SDL_FreeSurface(swdata->stretch);
        SDL_FreeSurface(swdata->display);
        SDL_free(swdata);
    }
}

#endif /* SDL_HAVE_YUV */

/* vi: set ts=4 sw=4 expandtab: */
