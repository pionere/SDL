/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_DUMMY

#include "../SDL_sysvideo.h"
#include "SDL_nullframebuffer_c.h"

// #define DUMMY_SURFACE "_SDL_DummySurface"

int DUMMY_CreateWindowFramebuffer(SDL_Window *window, Uint32 *format, void **pixels, int *pitch)
{
    SDL_Surface *surface;
    const Uint32 surface_format = SDL_PIXELFORMAT_RGB888;
    int w, h;

    /* Free the old framebuffer surface */
    // DUMMY_DestroyWindowFramebuffer(window);
    SDL_assert(window->surface == NULL);

    /* Create a new one */
    SDL_PrivateGetWindowSizeInPixels(window, &w, &h);
    surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 0, surface_format);
    if (!surface) {
        return -1;
    }

    /* Save the info and return! */
    window->surface = surface;
    // SDL_SetWindowData(window, DUMMY_SURFACE, surface);
    // *format = surface_format;
    // *pixels = surface->pixels;
    // *pitch = surface->pitch;
    return 0;
}

int DUMMY_UpdateWindowFramebuffer(SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    static int frame_number;
    SDL_Surface *surface;

    // surface = (SDL_Surface *)SDL_GetWindowData(window, DUMMY_SURFACE);
    surface = window->surface;
    if (!surface) {
        return SDL_SetError("Couldn't find dummy surface for window");
    }

    /* Send the data to the display */
    if (SDL_getenv("SDL_VIDEO_DUMMY_SAVE_FRAMES")) {
        char file[128];
        (void)SDL_snprintf(file, sizeof(file), "SDL_window%" SDL_PRIu32 "-%8.8d.bmp",
                           SDL_GetWindowID(window), ++frame_number);
        SDL_SaveBMP(surface, file);
    }
    return 0;
}

void DUMMY_DestroyWindowFramebuffer(SDL_Window *window)
{
    SDL_Surface *surface;

    // surface = (SDL_Surface *)SDL_SetWindowData(window, DUMMY_SURFACE, NULL);
    surface = window->surface;
    SDL_FreeSurface(surface);
}

#endif /* SDL_VIDEO_DRIVER_DUMMY */

/* vi: set ts=4 sw=4 expandtab: */
