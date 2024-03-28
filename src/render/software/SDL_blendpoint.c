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
#include "../../SDL_internal.h"

#if SDL_VIDEO_RENDER_SW

#include "SDL_draw.h"
#include "SDL_blendpoint.h"

static void SDL_BlendPoint_RGB555(SDL_Surface *dst, int x, int y, SDL_BlendMode blendMode,
                                 unsigned r, unsigned g, unsigned b, unsigned a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        DRAW_SETPIXELXY_BLEND_RGB555(x, y);
        break;
    case SDL_BLENDMODE_ADD:
        DRAW_SETPIXELXY_ADD_RGB555(x, y);
        break;
    case SDL_BLENDMODE_MOD:
        DRAW_SETPIXELXY_MOD_RGB555(x, y);
        break;
    case SDL_BLENDMODE_MUL:
        DRAW_SETPIXELXY_MUL_RGB555(x, y);
        break;
    default:
        DRAW_SETPIXELXY_RGB555(x, y);
        break;
    }
}

static void SDL_BlendPoint_RGB565(SDL_Surface *dst, int x, int y, SDL_BlendMode blendMode,
                                 unsigned r, unsigned g, unsigned b, unsigned a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        DRAW_SETPIXELXY_BLEND_RGB565(x, y);
        break;
    case SDL_BLENDMODE_ADD:
        DRAW_SETPIXELXY_ADD_RGB565(x, y);
        break;
    case SDL_BLENDMODE_MOD:
        DRAW_SETPIXELXY_MOD_RGB565(x, y);
        break;
    case SDL_BLENDMODE_MUL:
        DRAW_SETPIXELXY_MUL_RGB565(x, y);
        break;
    default:
        DRAW_SETPIXELXY_RGB565(x, y);
        break;
    }
}

static void SDL_BlendPoint_RGB888(SDL_Surface *dst, int x, int y, SDL_BlendMode blendMode,
                                 unsigned r, unsigned g, unsigned b, unsigned a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        DRAW_SETPIXELXY_BLEND_RGB888(x, y);
        break;
    case SDL_BLENDMODE_ADD:
        DRAW_SETPIXELXY_ADD_RGB888(x, y);
        break;
    case SDL_BLENDMODE_MOD:
        DRAW_SETPIXELXY_MOD_RGB888(x, y);
        break;
    case SDL_BLENDMODE_MUL:
        DRAW_SETPIXELXY_MUL_RGB888(x, y);
        break;
    default:
        DRAW_SETPIXELXY_RGB888(x, y);
        break;
    }
}

static void SDL_BlendPoint_ARGB8888(SDL_Surface *dst, int x, int y, SDL_BlendMode blendMode,
                                   unsigned r, unsigned g, unsigned b, unsigned a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        DRAW_SETPIXELXY_BLEND_ARGB8888(x, y);
        break;
    case SDL_BLENDMODE_ADD:
        DRAW_SETPIXELXY_ADD_ARGB8888(x, y);
        break;
    case SDL_BLENDMODE_MOD:
        DRAW_SETPIXELXY_MOD_ARGB8888(x, y);
        break;
    case SDL_BLENDMODE_MUL:
        DRAW_SETPIXELXY_MUL_ARGB8888(x, y);
        break;
    default:
        DRAW_SETPIXELXY_ARGB8888(x, y);
        break;
    }
}

static void SDL_BlendPoint_RGB16(SDL_Surface *dst, int x, int y, SDL_BlendMode blendMode,
                              unsigned r, unsigned g, unsigned b, unsigned a)
{
    SDL_PixelFormat *fmt = dst->format;
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        DRAW_SETPIXELXY2_BLEND_RGB(x, y);
        break;
    case SDL_BLENDMODE_ADD:
        DRAW_SETPIXELXY2_ADD_RGB(x, y);
        break;
    case SDL_BLENDMODE_MOD:
        DRAW_SETPIXELXY2_MOD_RGB(x, y);
        break;
    case SDL_BLENDMODE_MUL:
        DRAW_SETPIXELXY2_MUL_RGB(x, y);
        break;
    default:
        DRAW_SETPIXELXY2_RGB(x, y);
        break;
    }
}

static void SDL_BlendPoint_RGB32(SDL_Surface *dst, int x, int y, SDL_BlendMode blendMode,
                              unsigned r, unsigned g, unsigned b, unsigned a)
{
    SDL_PixelFormat *fmt = dst->format;
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        DRAW_SETPIXELXY4_BLEND_RGB(x, y);
        break;
    case SDL_BLENDMODE_ADD:
        DRAW_SETPIXELXY4_ADD_RGB(x, y);
        break;
    case SDL_BLENDMODE_MOD:
        DRAW_SETPIXELXY4_MOD_RGB(x, y);
        break;
    case SDL_BLENDMODE_MUL:
        DRAW_SETPIXELXY4_MUL_RGB(x, y);
        break;
    default:
        DRAW_SETPIXELXY4_RGB(x, y);
        break;
    }
}

static void SDL_BlendPoint_RGBA32(SDL_Surface *dst, int x, int y, SDL_BlendMode blendMode,
                               unsigned r, unsigned g, unsigned b, unsigned a)
{
    SDL_PixelFormat *fmt = dst->format;
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        DRAW_SETPIXELXY4_BLEND_RGBA(x, y);
        break;
    case SDL_BLENDMODE_ADD:
        DRAW_SETPIXELXY4_ADD_RGBA(x, y);
        break;
    case SDL_BLENDMODE_MOD:
        DRAW_SETPIXELXY4_MOD_RGBA(x, y);
        break;
    case SDL_BLENDMODE_MUL:
        DRAW_SETPIXELXY4_MUL_RGBA(x, y);
        break;
    default:
        DRAW_SETPIXELXY4_RGBA(x, y);
        break;
    }
}

typedef void (*BlendPointFunc)(SDL_Surface *dst, int x, int y, SDL_BlendMode blendMode,
                              unsigned r, unsigned g, unsigned b, unsigned a);

static BlendPointFunc SDL_CalculateBlendPointFunc(const SDL_PixelFormat *fmt)
{
    /* This function doesn't work on surfaces < 8 bpp */
    if (fmt->BitsPerPixel < 8) {
        return NULL;
    }
    switch (fmt->BitsPerPixel) {
    case 15:
        switch (fmt->Rmask) {
        case 0x7C00:
            return SDL_BlendPoint_RGB555;
        }
        break;
    case 16:
        switch (fmt->Rmask) {
        case 0xF800:
            return SDL_BlendPoint_RGB565;
        }
        break;
    case 32:
        switch (fmt->Rmask) {
        case 0x00FF0000:
            if (!fmt->Amask) {
                return SDL_BlendPoint_RGB888;
            } else {
                return SDL_BlendPoint_ARGB8888;
            }
            /* break; -Wunreachable-code-break */
        }
        break;
    default:
        break;
    }

    if (!fmt->Amask) {
        if (fmt->BytesPerPixel == 4) {
            return SDL_BlendPoint_RGB32;
        }
        if (fmt->BytesPerPixel == 2) {
            return SDL_BlendPoint_RGB16;
        }
    } else {
        if (fmt->BytesPerPixel == 4) {
            return SDL_BlendPoint_RGBA32;
        }
    }
    return NULL;
}

int SDL_BlendPoint(SDL_Surface *dst, int x, int y, SDL_BlendMode blendMode, const SDL_Color color)
{
    BlendPointFunc func;
    unsigned r, g, b, a;

    SDL_assert(dst != NULL);

    func = SDL_CalculateBlendPointFunc(dst->format);
    if (!func) {
        return SDL_SetError("Unsupported surface format");
    }

    /* Perform clipping */
    if (x < dst->clip_rect.x || y < dst->clip_rect.y ||
        x >= (dst->clip_rect.x + dst->clip_rect.w) ||
        y >= (dst->clip_rect.y + dst->clip_rect.h)) {
        return 0;
    }

    r = color.r;
    g = color.g;
    b = color.b;
    a = color.a;
    if (blendMode == SDL_BLENDMODE_BLEND || blendMode == SDL_BLENDMODE_ADD) {
        r = DRAW_MUL(r, a);
        g = DRAW_MUL(g, a);
        b = DRAW_MUL(b, a);
    }

    func(dst, x, y, blendMode, r, g, b, a);
    return 0;
}

int SDL_BlendPoints(SDL_Surface *dst, const SDL_Point *points, int count,
                    SDL_BlendMode blendMode, const SDL_Color color)
{
    int minx, miny;
    int maxx, maxy;
    int i, x, y;
    BlendPointFunc func;
    unsigned r, g, b, a;

    SDL_assert(dst != NULL);

    func = SDL_CalculateBlendPointFunc(dst->format);
    if (!func) {
        return SDL_SetError("Unsupported surface format");
    }

    r = color.r;
    g = color.g;
    b = color.b;
    a = color.a;
    if (blendMode == SDL_BLENDMODE_BLEND || blendMode == SDL_BLENDMODE_ADD) {
        r = DRAW_MUL(r, a);
        g = DRAW_MUL(g, a);
        b = DRAW_MUL(b, a);
    }

    minx = dst->clip_rect.x;
    maxx = dst->clip_rect.x + dst->clip_rect.w - 1;
    miny = dst->clip_rect.y;
    maxy = dst->clip_rect.y + dst->clip_rect.h - 1;

    for (i = 0; i < count; ++i) {
        x = points[i].x;
        y = points[i].y;

        if (x < minx || x > maxx || y < miny || y > maxy) {
            continue;
        }
        func(dst, x, y, blendMode, r, g, b, a);
    }
    return 0;
}

#endif /* SDL_VIDEO_RENDER_SW */

/* vi: set ts=4 sw=4 expandtab: */
