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

#define SDL_WINDOWTEXTUREDATA "_SDL_WindowTextureData"

typedef struct
{
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    void *pixels;
    int pitch;
    int bytes_per_pixel;
} SDL_WindowTextureData;

int SDL_CreateWindowFramebuffer(SDL_Window *window, Uint32 *format, void **pixels, int *pitch)
{
    const SDL_RendererInfo *info;
    SDL_WindowTextureData *data = SDL_GetWindowData(window, SDL_WINDOWTEXTUREDATA);
    int i, w, h;
    Uint32 texture_format;

    SDL_PrivateGetWindowSizeInPixels(window, &w, &h);

    if (!data) {
        SDL_Renderer *renderer = NULL;
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
            if (!renderer) {
                return SDL_SetError("Requested renderer for " SDL_HINT_FRAMEBUFFER_ACCELERATION " is not available");
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
            if (!renderer) {
                return SDL_SetError("No hardware accelerated renderers available");
            }
        }

        SDL_assert(renderer != NULL); /* should have explicitly checked this above. */

        /* Create the data after we successfully create the renderer (bug #1116) */
        data = (SDL_WindowTextureData *)SDL_calloc(1, sizeof(*data));
        if (!data) {
            SDL_DestroyRenderer(renderer);
            return SDL_OutOfMemory();
        }
        SDL_SetWindowData(window, SDL_WINDOWTEXTUREDATA, data);

        data->renderer = renderer;
    }

    /* Free any old texture and pixel data */
    if (data->texture) {
        SDL_DestroyTexture(data->texture);
        data->texture = NULL;
    }
    SDL_free(data->pixels);
    data->pixels = NULL;

    /* Find the first format without an alpha channel */
    info = SDL_PrivateGetRendererInfo(data->renderer);
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
    }

    data->texture = SDL_CreateTexture(data->renderer, texture_format,
                                      SDL_TEXTUREACCESS_STREAMING,
                                      w, h);
    if (!data->texture) {
        /* codechecker_false_positive [Malloc] Static analyzer doesn't realize allocated `data` is saved to SDL_WINDOWTEXTUREDATA and not leaked here. */
        return -1; /* NOLINT(clang-analyzer-unix.Malloc) */
    }

    /* Create framebuffer data */
    data->bytes_per_pixel = SDL_BYTESPERPIXEL(texture_format);
    data->pitch = (((w * data->bytes_per_pixel) + 3) & ~3);

    {
        /* Make static analysis happy about potential SDL_malloc(0) calls. */
        const size_t allocsize = (size_t)h * data->pitch;
        data->pixels = SDL_malloc((allocsize > 0) ? allocsize : 1);
        if (!data->pixels) {
            return SDL_OutOfMemory();
        }
    }

    *format = texture_format;
    *pixels = data->pixels;
    *pitch = data->pitch;

    /* Make sure we're not double-scaling the viewport */
    SDL_RenderSetViewport(data->renderer, NULL);

    return 0;
}

int SDL_UpdateWindowFramebuffer(SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_WindowTextureData *data;
    SDL_Rect rect;
    void *src;
    int w, h;

    SDL_PrivateGetWindowSizeInPixels(window, &w, &h);

    data = SDL_GetWindowData(window, SDL_WINDOWTEXTUREDATA);
    if (!data || !data->texture) {
        return SDL_SetError("No window texture data");
    }

    /* Update a single rect that contains subrects for best DMA performance */
    if (SDL_GetSpanEnclosingRect(w, h, numrects, rects, &rect)) {
        src = (void *)((Uint8 *)data->pixels +
                       rect.y * data->pitch +
                       rect.x * data->bytes_per_pixel);
        if (SDL_UpdateTexture(data->texture, &rect, src, data->pitch) < 0) {
            return -1;
        }

        if (SDL_RenderCopy(data->renderer, data->texture, NULL, NULL) < 0) {
            return -1;
        }

        SDL_RenderPresent(data->renderer);
    }
    return 0;
}

void SDL_DestroyWindowFramebuffer(SDL_Window *window)
{
    SDL_WindowTextureData *data;

    data = SDL_SetWindowData(window, SDL_WINDOWTEXTUREDATA, NULL);
    if (!data) {
        return;
    }
    if (data->texture) {
        SDL_DestroyTexture(data->texture);
    }
    if (data->renderer) {
        SDL_DestroyRenderer(data->renderer);
    }
    SDL_free(data->pixels);
    SDL_free(data);
}

/* vi: set ts=4 sw=4 expandtab: */
