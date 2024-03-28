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
#include "SDL_blendfillrect.h"

static void SDL_BlendFillRect_RGB555(SDL_Surface *dst, const SDL_Rect *rect,
                                    SDL_BlendMode blendMode, unsigned r, unsigned g, unsigned b, unsigned a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        FILLRECT(Uint16, DRAW_SETPIXEL_BLEND_RGB555);
        break;
    case SDL_BLENDMODE_ADD:
        FILLRECT(Uint16, DRAW_SETPIXEL_ADD_RGB555);
        break;
    case SDL_BLENDMODE_MOD:
        FILLRECT(Uint16, DRAW_SETPIXEL_MOD_RGB555);
        break;
    case SDL_BLENDMODE_MUL:
        FILLRECT(Uint16, DRAW_SETPIXEL_MUL_RGB555);
        break;
    default:
        FILLRECT(Uint16, DRAW_SETPIXEL_RGB555);
        break;
    }
}

static void SDL_BlendFillRect_RGB565(SDL_Surface *dst, const SDL_Rect *rect,
                                    SDL_BlendMode blendMode, unsigned r, unsigned g, unsigned b, unsigned a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        FILLRECT(Uint16, DRAW_SETPIXEL_BLEND_RGB565);
        break;
    case SDL_BLENDMODE_ADD:
        FILLRECT(Uint16, DRAW_SETPIXEL_ADD_RGB565);
        break;
    case SDL_BLENDMODE_MOD:
        FILLRECT(Uint16, DRAW_SETPIXEL_MOD_RGB565);
        break;
    case SDL_BLENDMODE_MUL:
        FILLRECT(Uint16, DRAW_SETPIXEL_MUL_RGB565);
        break;
    default:
        FILLRECT(Uint16, DRAW_SETPIXEL_RGB565);
        break;
    }
}

static void SDL_BlendFillRect_RGB888(SDL_Surface *dst, const SDL_Rect *rect,
                                    SDL_BlendMode blendMode, unsigned r, unsigned g, unsigned b, unsigned a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        FILLRECT(Uint32, DRAW_SETPIXEL_BLEND_RGB888);
        break;
    case SDL_BLENDMODE_ADD:
        FILLRECT(Uint32, DRAW_SETPIXEL_ADD_RGB888);
        break;
    case SDL_BLENDMODE_MOD:
        FILLRECT(Uint32, DRAW_SETPIXEL_MOD_RGB888);
        break;
    case SDL_BLENDMODE_MUL:
        FILLRECT(Uint32, DRAW_SETPIXEL_MUL_RGB888);
        break;
    default:
        FILLRECT(Uint32, DRAW_SETPIXEL_RGB888);
        break;
    }
}

static void SDL_BlendFillRect_ARGB8888(SDL_Surface *dst, const SDL_Rect *rect,
                                      SDL_BlendMode blendMode, unsigned r, unsigned g, unsigned b, unsigned a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        FILLRECT(Uint32, DRAW_SETPIXEL_BLEND_ARGB8888);
        break;
    case SDL_BLENDMODE_ADD:
        FILLRECT(Uint32, DRAW_SETPIXEL_ADD_ARGB8888);
        break;
    case SDL_BLENDMODE_MOD:
        FILLRECT(Uint32, DRAW_SETPIXEL_MOD_ARGB8888);
        break;
    case SDL_BLENDMODE_MUL:
        FILLRECT(Uint32, DRAW_SETPIXEL_MUL_ARGB8888);
        break;
    default:
        FILLRECT(Uint32, DRAW_SETPIXEL_ARGB8888);
        break;
    }
}

static void SDL_BlendFillRect_RGB16(SDL_Surface *dst, const SDL_Rect *rect,
                                 SDL_BlendMode blendMode, unsigned r, unsigned g, unsigned b, unsigned a)
{
    SDL_PixelFormat *fmt = dst->format;
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        FILLRECT(Uint16, DRAW_SETPIXEL_BLEND_RGB);
        break;
    case SDL_BLENDMODE_ADD:
        FILLRECT(Uint16, DRAW_SETPIXEL_ADD_RGB);
        break;
    case SDL_BLENDMODE_MOD:
        FILLRECT(Uint16, DRAW_SETPIXEL_MOD_RGB);
        break;
    case SDL_BLENDMODE_MUL:
        FILLRECT(Uint16, DRAW_SETPIXEL_MUL_RGB);
        break;
    default:
        FILLRECT(Uint16, DRAW_SETPIXEL_RGB);
        break;
    }
}

static void SDL_BlendFillRect_RGB32(SDL_Surface *dst, const SDL_Rect *rect,
                                 SDL_BlendMode blendMode, unsigned r, unsigned g, unsigned b, unsigned a)
{
    SDL_PixelFormat *fmt = dst->format;
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        FILLRECT(Uint32, DRAW_SETPIXEL_BLEND_RGB);
        break;
    case SDL_BLENDMODE_ADD:
        FILLRECT(Uint32, DRAW_SETPIXEL_ADD_RGB);
        break;
    case SDL_BLENDMODE_MOD:
        FILLRECT(Uint32, DRAW_SETPIXEL_MOD_RGB);
        break;
    case SDL_BLENDMODE_MUL:
        FILLRECT(Uint32, DRAW_SETPIXEL_MUL_RGB);
        break;
    default:
        FILLRECT(Uint32, DRAW_SETPIXEL_RGB);
        break;
    }
}

static void SDL_BlendFillRect_RGBA(SDL_Surface *dst, const SDL_Rect *rect,
                                  SDL_BlendMode blendMode, unsigned r, unsigned g, unsigned b, unsigned a)
{
    SDL_PixelFormat *fmt = dst->format;
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case SDL_BLENDMODE_BLEND:
        FILLRECT(Uint32, DRAW_SETPIXEL_BLEND_RGBA);
        break;
    case SDL_BLENDMODE_ADD:
        FILLRECT(Uint32, DRAW_SETPIXEL_ADD_RGBA);
        break;
    case SDL_BLENDMODE_MOD:
        FILLRECT(Uint32, DRAW_SETPIXEL_MOD_RGBA);
        break;
    case SDL_BLENDMODE_MUL:
        FILLRECT(Uint32, DRAW_SETPIXEL_MUL_RGBA);
        break;
    default:
        FILLRECT(Uint32, DRAW_SETPIXEL_RGBA);
        break;
    }
}

typedef void (*BlendRectFunc)(SDL_Surface *dst, const SDL_Rect *rect, SDL_BlendMode blendMode,
                              unsigned r, unsigned g, unsigned b, unsigned a);

static BlendRectFunc SDL_CalculateBlendRectFunc(const SDL_PixelFormat *fmt)
{
    /* This function doesn't work on surfaces < 8 bpp */
    if (fmt->BitsPerPixel < 8) {
        return NULL;
    }

    switch (fmt->BitsPerPixel) {
    case 15:
        switch (fmt->Rmask) {
        case 0x7C00:
            return SDL_BlendFillRect_RGB555;
        }
        break;
    case 16:
        switch (fmt->Rmask) {
        case 0xF800:
            return SDL_BlendFillRect_RGB565;
        }
        break;
    case 32:
        switch (fmt->Rmask) {
        case 0x00FF0000:
            if (!fmt->Amask) {
                return SDL_BlendFillRect_RGB888;
            } else {
                return SDL_BlendFillRect_ARGB8888;
            }
            /* break; -Wunreachable-code-break */
        }
        break;
    default:
        break;
    }

    if (!fmt->Amask) {
        if (fmt->BytesPerPixel == 4) {
            return SDL_BlendFillRect_RGB32;
        }
        if (fmt->BytesPerPixel == 2) {
            return SDL_BlendFillRect_RGB16;
        }
    } else {
        if (fmt->BytesPerPixel == 4) {
            return SDL_BlendFillRect_RGBA;
        }
    }
    return NULL;
}
#if 0
int SDL_BlendFillRect(SDL_Surface *dst, const SDL_Rect *rect,
                      SDL_BlendMode blendMode, const SDL_Color color)
{
    BlendRectFunc func;
    unsigned r, g, b, a;

    SDL_Rect clipped;

    SDL_assert(dst != NULL);

    func = SDL_CalculateBlendRectFunc(dst->format);
    if (!func) {
        return SDL_SetError("Unsupported surface format");
    }

    /* If 'rect' == NULL, then fill the whole surface */
    if (rect) {
        /* Perform clipping */
        if (!SDL_IntersectRect(rect, &dst->clip_rect, &clipped)) {
            return 0;
        }
        rect = &clipped;
    } else {
        rect = &dst->clip_rect;
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

    func(dst, rect, blendMode, r, g, b, a);
    return 0;
}
#endif
int SDL_BlendFillRects(SDL_Surface *dst, const SDL_Rect *rects, int count,
                       SDL_BlendMode blendMode, const SDL_Color color)
{
    BlendRectFunc func;
    unsigned r, g, b, a;
    SDL_Rect rect;
    int i;

    SDL_assert(dst != NULL);

    func = SDL_CalculateBlendRectFunc(dst->format);
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

    for (i = 0; i < count; ++i) {
        /* Perform clipping */
        if (!SDL_IntersectRect(&rects[i], &dst->clip_rect, &rect)) {
            continue;
        }
        func(dst, &rect, blendMode, r, g, b, a);
    }
    return 0;
}

#endif /* SDL_VIDEO_RENDER_SW */

/* vi: set ts=4 sw=4 expandtab: */
