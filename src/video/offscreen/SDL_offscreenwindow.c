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

#ifdef SDL_VIDEO_DRIVER_OFFSCREEN

#include "SDL_offscreenvideo.h"
#include "SDL_offscreenwindow.h"
#include "SDL_offscreenopengles.h"

int OFFSCREEN_CreateSDLWindow(_THIS, SDL_Window *window)
{
#ifdef SDL_VIDEO_OPENGL_EGL
    SDL_WindowData *offscreen_window = SDL_calloc(1, sizeof(SDL_WindowData));

    if (!offscreen_window) {
        return SDL_OutOfMemory();
    }

    window->driverdata = offscreen_window;

    if (window->flags & SDL_WINDOW_OPENGL) {

        offscreen_window->egl_surface = SDL_EGL_CreateOffscreenSurface(_this, window->wrect.w, window->wrect.h);

        if (offscreen_window->egl_surface == EGL_NO_SURFACE) {
            return -1;
        }
    } else {
        offscreen_window->egl_surface = EGL_NO_SURFACE;
    }
#endif /* SDL_VIDEO_OPENGL_EGL */

    return 0;
}

void OFFSCREEN_DestroyWindow(SDL_Window *window)
{
#ifdef SDL_VIDEO_OPENGL_EGL
    SDL_WindowData *offscreen_window = window->driverdata;

    if (offscreen_window) {
        SDL_EGL_DestroySurface(offscreen_window->egl_surface);
        SDL_free(offscreen_window);
        window->driverdata = NULL;
    }
#endif
}

#endif /* SDL_VIDEO_DRIVER_OFFSCREEN */

/* vi: set ts=4 sw=4 expandtab: */
