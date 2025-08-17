/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_XBOX

#include "../SDL_sysvideo.h"
#include "SDL_xbframebuffer_c.h"
#include "SDL_assert.h"

#include <hal/video.h>


int XBOX_CreateWindowFramebuffer(SDL_Window * window, Uint32 * format, void ** pixels, int *pitch)
{
    SDL_Surface *surface;
    VIDEO_MODE vm = XVideoGetMode();
    const Uint32 surface_format = pixelFormatSelector(vm.bpp);
    int w, h;

    /* Free the old framebuffer surface */
    // XBOX_DestroyWindowFramebuffer(window);
    SDL_assert(window->surface == NULL);

    /* Create a new one */
    SDL_GetWindowSize(window, &w, &h); // SDL_PrivateGetWindowSizeInPixels ?
    surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 0, surface_format);
    if (!surface) {
        return -1;
    }

    /* Save the info and return! */
    window->surface = surface;
    // SDL_SetWindowData(window, XBOX_SURFACE, surface);
    //*format = surface_format;
    //*pixels = surface->pixels;
    //*pitch = surface->pitch;
    return 0;
}

int XBOX_UpdateWindowFramebuffer(SDL_Window * window, const SDL_Rect * rects, int numrects)
{
    SDL_Surface *surface;
    VIDEO_MODE vm;
    const void *src;
    Uint32 src_format, dst_format;
    int src_pitch, dst_bytes_per_pixel, dst_pitch, width, height;
    void *dst;

    // surface = (SDL_Surface *)SDL_GetWindowData(window, XBOX_SURFACE);
    surface = window->surface;
    if (!surface) {
        return SDL_SetError("Couldn't find framebuffer surface for window");
    }

    vm = XVideoGetMode();

    // Get information about SDL window surface
    src = surface->pixels;
    src_format = surface->format->format;
    src_pitch = surface->pitch;

    // Get information about GPU framebuffer
    dst = XVideoGetFB();
    dst_format = pixelFormatSelector(vm.bpp);
    dst_bytes_per_pixel = SDL_BYTESPERPIXEL(dst_format);
    dst_pitch = vm.width * dst_bytes_per_pixel;

    // Check if the SDL window fits into GPU framebuffer
    width = surface->w;
    height = surface->h;
    SDL_assert(width <= vm.width);
    SDL_assert(height <= vm.height);

    // Copy SDL window surface to GPU framebuffer
    SDL_ConvertPixels(width, height, src_format, src, src_pitch, dst_format, dst, dst_pitch);

    // Writeback WC buffers
    XVideoFlushFB();

    return 0;
}

void XBOX_DestroyWindowFramebuffer(SDL_Window * window)
{
    SDL_Surface *surface;

    // surface = (SDL_Surface *)SDL_SetWindowData(window, XBOX_SURFACE, NULL);
    surface = window->surface;
    SDL_FreeSurface(surface);
}

#endif /* SDL_VIDEO_DRIVER_XBOX */

/* vi: set ts=4 sw=4 expandtab: */
