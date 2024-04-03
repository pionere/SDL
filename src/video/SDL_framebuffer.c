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

/* Support for framebuffer emulation using an accelerated renderer */

#include "SDL_hints.h"
#include "SDL_rect_c.h"

#include "../render/SDL_sysrender.h"
#include "SDL_sysvideo.h"
#include "SDL_framebuffer.h"

int SDL_CreateWindowFramebuffer(SDL_Window *window, Uint32 *format, void **pixels, int *pitch)
{
#ifndef SDL_RENDER_DISABLED
    const SDL_RendererInfo *info;
    SDL_Texture *texture;
    SDL_Surface *surface;
    int i, w, h;
    Uint32 texture_format;
    SDL_Renderer *renderer = SDL_GetRenderer(window);
    SDL_bool renderer_created = SDL_FALSE;

    if (renderer) {
        texture = renderer->framebuffer_texture;
        if (texture) {
            SDL_DestroyTexture(texture);
            renderer->framebuffer_texture = NULL;
        }
    } else {
        const char *hint = SDL_GetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION);
        const SDL_bool specific_accelerated_renderer = (hint && *hint != '0' && *hint != '1' &&
                                                        SDL_strcasecmp(hint, "true") != 0 &&
                                                        SDL_strcasecmp(hint, "false") != 0 &&
                                                        SDL_strcasecmp(hint, "software") != 0);
        /* Check to see if there's a specific driver requested */
        if (specific_accelerated_renderer) {
            for (i = 0; i < SDL_GetNumRenderDrivers(); ++i) {
                info = SDL_PrivateGetRenderDriverInfo(i);
                if (SDL_strcasecmp(info->name, hint) == 0) {
                    renderer = SDL_CreateRenderer(window, i, 0);
                    break;
                }
            }
            /* if it was specifically requested, even if SDL_RENDERER_ACCELERATED isn't set, we'll accept this renderer. */
        } else {
            for (i = 0; i < SDL_GetNumRenderDrivers(); ++i) {
                info = SDL_PrivateGetRenderDriverInfo(i);
                if (info->flags & SDL_RENDERER_ACCELERATED) {
                    renderer = SDL_CreateRenderer(window, i, 0);
                    if (renderer) {
                        info = SDL_PrivateGetRendererInfo(renderer);
                        if (info->flags & SDL_RENDERER_ACCELERATED) {
                            break; /* this will work. */
                        }
                        /* wasn't accelerated skip it. */
                        SDL_DestroyRenderer(renderer);
                        renderer = NULL;
                    }
                }
            }
        }

        if (!renderer) {
            if (specific_accelerated_renderer) {
                SDL_SetError("Requested renderer for " SDL_HINT_FRAMEBUFFER_ACCELERATION " is not available");
            } else {
                SDL_SetError("No hardware accelerated renderers available");
            }
            return -1;
        }

        renderer_created = SDL_TRUE;
    }

    /* Find the first format without an alpha channel */
    SDL_INLINE_COMPILE_TIME_ASSERT(framebuffer_format, !SDL_ISPIXELFORMAT_FOURCC(SDL_PIXELFORMAT_UNKNOWN));
    SDL_INLINE_COMPILE_TIME_ASSERT(framebuffer_format, !SDL_ISPIXELFORMAT_ALPHA(SDL_PIXELFORMAT_UNKNOWN));
    texture_format = GetClosestSupportedFormat(renderer, SDL_PIXELFORMAT_UNKNOWN);
    /*info = SDL_PrivateGetRendererInfo(renderer);
    texture_format = info->texture_formats[0];

    for (i = 0; i < (int)info->num_texture_formats; ++i) {
#if SDL_HAVE_YUV
        if (SDL_ISPIXELFORMAT_FOURCC(info->texture_formats[i]))
            continue;
#else
        SDL_assert(!SDL_ISPIXELFORMAT_FOURCC(info->texture_formats[i]));
#endif
        if (!SDL_ISPIXELFORMAT_ALPHA(info->texture_formats[i])) {
            texture_format = info->texture_formats[i];
            break;
        }
    }*/

    SDL_PrivateGetWindowSizeInPixels(window, &w, &h);
    texture = SDL_CreateTexture(renderer, texture_format,
                                      SDL_TEXTUREACCESS_STREAMING,
                                      w, h);
    if (!texture) {
        if (renderer_created) {
            SDL_DestroyRenderer(renderer);
        }
        return -1;
    }

    surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 0, texture_format);
    if (!surface) {
        if (renderer_created) {
            SDL_DestroyRenderer(renderer);
        }
        return -1;
    }

    renderer->framebuffer_texture = texture;
    renderer->info.flags |= SDL_RENDERER_DONTFREE;

    /* Make sure we're not double-scaling the viewport */
    SDL_RenderSetViewport(renderer, NULL);

    window->surface = surface;
    // *format = texture_format;
    // *pixels = framebuffer->pixels;
    // *pitch = framebuffer->pitch;
    return 0;
#else
    return SDL_SetError("No hardware accelerated renderers available");
#endif // SDL_RENDER_DISABLED
}

int SDL_UpdateWindowFramebuffer(SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_Rect rect;
    void *src;
    int w, h, retval;
    SDL_Surface *surface = window->surface;
    SDL_Renderer *renderer = SDL_GetRenderer(window);
    SDL_Texture *texture;

    SDL_assert(surface != NULL);
    SDL_assert(renderer != NULL);
    texture = renderer->framebuffer_texture;
    SDL_assert(texture != NULL);

    /* Update a single rect that contains subrects for best DMA performance */
    // SDL_PrivateGetWindowSizeInPixels(window, &w, &h);
    w = surface->w;
    h = surface->h;
    if (SDL_GetSpanEnclosingRect(w, h, numrects, rects, &rect)) {
        src = (void *)((Uint8 *)surface->pixels +
                       rect.y * surface->pitch +
                       rect.x * surface->format->BytesPerPixel);
        retval = SDL_UpdateTexture(texture, &rect, src, surface->pitch);
        if (retval < 0) {
            return retval;
        }

        retval = SDL_RenderCopyF(renderer, texture, NULL, NULL);
        if (retval < 0) {
            return retval;
        }

        SDL_RenderPresent(renderer);
    }
    return 0;
}

void SDL_DestroyWindowFramebuffer(SDL_Window *window)
{
    SDL_Renderer *renderer = SDL_GetRenderer(window);
    if (renderer && (renderer->info.flags & SDL_RENDERER_DONTFREE)) {
        renderer->info.flags &= ~SDL_RENDERER_DONTFREE;
        SDL_DestroyRenderer(renderer);
    }
}

/* vi: set ts=4 sw=4 expandtab: */
