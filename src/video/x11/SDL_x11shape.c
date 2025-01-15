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

#if defined(SDL_VIDEO_DRIVER_X11) && defined(SDL_VIDEO_DRIVER_X11_XSHAPE)

#include "SDL_x11video.h"
#include "SDL_x11shape.h"
#include "SDL_x11window.h"
#include "../SDL_shape_internals.h"

SDL_WindowShaper *X11_CreateShaper(SDL_Window *window)
{
    SDL_WindowShaper *result;
    SDL_ShapeData *data;

    result = SDL_calloc(1, sizeof(SDL_WindowShaper));
    data = SDL_malloc(sizeof(SDL_ShapeData));
    if (!result || !data) {
        SDL_free(data);
        SDL_free(result);
        SDL_OutOfMemory();
        return NULL;
    }
    SDL_INLINE_COMPILE_TIME_ASSERT(x11_shape_mode, ShapeModeDefault == 0);
    // result->mode.mode = ShapeModeDefault;
    result->mode.parameters.binarizationCutoff = 1;
    // result->userx = result->usery = 0;
    // result->hasshape = SDL_FALSE;
    result->driverdata = data;
    data->bitmapsize = 0;
    data->bitmap = NULL;
    window->shaper = result;
    if (X11_ResizeWindowShape(window) != 0) {
        SDL_free(data);
        SDL_free(result);
        window->shaper = NULL;
        return NULL;
    }

    return result;
}

int X11_ResizeWindowShape(SDL_Window *window)
{
    SDL_ShapeData *data;
    unsigned int bitmapsize;

    SDL_assert(window != NULL);
    SDL_assert(window->shaper != NULL);
    data = (SDL_ShapeData *)window->shaper->driverdata;
    SDL_assert(data != NULL);

    bitmapsize = (window->wrect.w + 7) >> 3; // (w + (ppb - 1)) / ppb;

    bitmapsize *= window->wrect.h;
    if (data->bitmapsize != bitmapsize || !data->bitmap) {
        data->bitmapsize = bitmapsize;
        SDL_free(data->bitmap);
        data->bitmap = SDL_malloc(data->bitmapsize);
        if (!data->bitmap) {
            return SDL_OutOfMemory();
        }
    }
    SDL_memset(data->bitmap, 0, data->bitmapsize);

    if (window->shaper->hasshape) {
        window->shaper->hasshape = SDL_FALSE;

        window->shaper->userx = window->wrect.x;
        window->shaper->usery = window->wrect.y;
        SDL_SetWindowPosition(window, -1000, -1000);
    }

    return 0;
}

int X11_SetWindowShape(SDL_Window *window, SDL_Surface *shape, const SDL_WindowShapeMode *shape_mode)
{
    SDL_WindowShaper *shaper = window->shaper;
    SDL_ShapeData *data;
    SDL_WindowData *windowdata;
    Pixmap shapemask;

    SDL_assert(shaper != NULL);
    SDL_assert(shape != NULL);
    data = shaper->driverdata;
    SDL_assert(data != NULL);

    /* Assume that shaper->alphacutoff already has a value, because SDL_SetWindowShape() should have given it one. */
    SDL_CalculateShapeBitmap(shape_mode, shape, data->bitmap, 8);

    windowdata = (SDL_WindowData *)(window->driverdata);
    shapemask = X11_XCreateBitmapFromData(x11VideoData.display, windowdata->xwindow, data->bitmap, window->wrect.w, window->wrect.h);

    X11_XShapeCombineMask(x11VideoData.display, windowdata->xwindow, ShapeBounding, 0, 0, shapemask, ShapeSet);
    X11_XSync(x11VideoData.display, False);

    X11_XFreePixmap(x11VideoData.display, shapemask);

    return 0;
}

#endif /* SDL_VIDEO_DRIVER_X11 && SDL_VIDEO_DRIVER_X11_XSHAPE */
