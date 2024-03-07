/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_DRIVER_DUMMY

// #include "../SDL_sysvideo.h"

#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_windowevents_c.h"

#include "SDL_nullwindow.h"

int DUMMY_CreateSDLWindow(_THIS, SDL_Window *window)
{
    return 0;
}

void DUMMY_ShowWindow(SDL_Window *window)
{
    SDL_SetMouseFocus(window);
    SDL_SetKeyboardFocus(window);
}

void DUMMY_HideWindow(SDL_Window *window)
{
    SDL_SetMouseFocus(NULL);
    SDL_SetKeyboardFocus(NULL);
}

void DUMMY_SetWindowFullscreen(SDL_Window * window, SDL_VideoDisplay * display, SDL_bool fullscreen)
{
    int w, h;

    if (fullscreen) {
        w = display->current_mode.w;
        h = display->current_mode.h;
    } else {
        w = window->windowed.w;
        h = window->windowed.h;
    }

    if (window->w != w || window->h != h) {
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESIZED, w, h);
    }
}

#endif /* SDL_VIDEO_DRIVER_DUMMY */

/* vi: set ts=4 sw=4 expandtab: */
