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

#ifndef SDL_yuv_c_h_
#define SDL_yuv_c_h_

#include "../SDL_internal.h"

typedef enum {
    SDL_YUVLAYOUT_PACKED,
    SDL_YUVLAYOUT_2PLANES,
    SDL_YUVLAYOUT_3PLANES,
} SDL_YUVLayout;

typedef struct {
    Uint32 yuv_format;  /* SDL_PixelFormatEnum */
    uint8_t yuv_layout; /* SDL_YUVLayout */
    uint8_t bpp;        /* y-bytes per pixel */
    int y_width;        /* number of pixels on the y-plane */
    int uv_width;       /* number or u-values on the u-plane (v-values on the v-plane) */
    int y_height;       /* number or pixels on the y-plane */
    int uv_height;      /* number of u-values on the u-plane (v-values on the v-plane) */
    int y_pitch;        /* the width of one row on the y-plane */
    int uv_pitch;       /* the width of one row on the u-plane */

    Uint8 *planes[3];   /* the planes in the same order as in the memory */

    Uint8 *y_plane;     /* direct link to the y-plane */
    Uint8 *u_plane;     /* direct link to the u-plane */
    Uint8 *v_plane;     /* direct link to the v-plane */

    size_t yuv_size;    /* size of the complete texture */
} SDL_YUVInfo;

extern int SDL_InitYUVInfo(int width, int height, Uint32 format, const void *yuv, int yuv_pitch, SDL_YUVInfo *yuv_info);

/* YUV conversion functions */

extern int SDL_ConvertPixels_YUV_to_RGB(int width, int height, Uint32 src_format, const void *src, int src_pitch, Uint32 dst_format, void *dst, int dst_pitch);
extern int SDL_ConvertPixels_RGB_to_YUV(int width, int height, Uint32 src_format, const void *src, int src_pitch, Uint32 dst_format, void *dst, int dst_pitch);
extern int SDL_ConvertPixels_YUV_to_YUV(int width, int height, Uint32 src_format, const void *src, int src_pitch, Uint32 dst_format, void *dst, int dst_pitch);


extern int SDL_CalculateYUVSize(Uint32 format, int w, int h, size_t *size, int *pitch);

#endif /* SDL_yuv_c_h_ */

/* vi: set ts=4 sw=4 expandtab: */
