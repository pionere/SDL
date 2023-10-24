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

#include "SDL_nullvideo.h"

#include "../../events/SDL_mouse_c.h"

static SDL_Cursor *DUMMY_CreateDefaultCursor()
{
    SDL_Cursor *cursor = NULL;
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 1, 1, 32, SDL_PIXELFORMAT_ARGB8888);

    if (surface != NULL) {
        // SDL_memset(surface->pixels, 0, (size_t)surface->h * surface->pitch);
        cursor = SDL_CreateColorCursor(surface, 0, 0);
        SDL_FreeSurface(surface);
    }

    return cursor;
}

static SDL_Cursor *DUMMY_CreateCursor(SDL_Surface *surface, int hot_x, int hot_y)
{
    SDL_Cursor *cursor;

    cursor = SDL_calloc(1, sizeof(*cursor));
    if (!cursor) {
        SDL_OutOfMemory();
    }

    return cursor;
}

static void DUMMY_FreeCursor(SDL_Cursor *cursor)
{
    SDL_free(cursor);
}

void DUMMY_InitMouse(_THIS)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    mouse->CreateCursor = DUMMY_CreateCursor;
    mouse->FreeCursor = DUMMY_FreeCursor;
    // mouse->SetRelativeMouseMode = DUMMY_SetRelativeMouseMode; -- TODO: set in case of SDL_INPUT_LINUXEV ?

    SDL_SetDefaultCursor(DUMMY_CreateDefaultCursor());
}

#endif /* SDL_VIDEO_DRIVER_DUMMY */

/* vi: set ts=4 sw=4 expandtab: */
