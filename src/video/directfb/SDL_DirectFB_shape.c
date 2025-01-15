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

#ifdef SDL_VIDEO_DRIVER_DIRECTFB

#include "SDL_DirectFB_video.h"
#include "SDL_DirectFB_shape.h"
#include "SDL_DirectFB_window.h"

#include "../SDL_shape_internals.h"

SDL_WindowShaper *DirectFB_CreateShaper(SDL_Window* window)
{
    SDL_WindowShaper* result = SDL_calloc(1, sizeof(SDL_WindowShaper));
    SDL_ShapeData* data = SDL_malloc(sizeof(SDL_ShapeData));

    if (!result || !data) {
        SDL_free(data);
        SDL_free(result);
        SDL_OutOfMemory();
        return NULL;
    }
    SDL_INLINE_COMPILE_TIME_ASSERT(directfb_shape_mode, ShapeModeDefault == 0);
    // result->mode.mode = ShapeModeDefault;
    result->mode.parameters.binarizationCutoff = 1;
    // result->userx = result->usery = 0;
    // result->hasshape = SDL_FALSE;
    result->driverdata = data;
    data->surface = NULL;
    window->shaper = result;
    // DirectFB_ResizeWindowShape(window);

    return result;
}

int DirectFB_ResizeWindowShape(SDL_Window* window)
{
    SDL_assert(window != NULL);
    SDL_assert(window->shaper != NULL);

    SDL_DFB_RELEASE(data->surface);

    if (window->shaper->hasshape) {
        window->shaper->hasshape = SDL_FALSE;

        window->shaper->userx = window->wrect.x;
        window->shaper->usery = window->wrect.y;
        SDL_SetWindowPosition(window, -1000, -1000);
    }

    return 0;
}

int DirectFB_SetWindowShape(SDL_Window *window, SDL_Surface *shape, const SDL_WindowShapeMode *shape_mode)
{
    SDL_WindowShaper *shaper = window->shaper;
    DFB_VideoData *devdata = &dfbVideoData;
    SDL_ShapeData *data;
    Uint32 *pixels;
    Sint32 pitch;
    Uint32 h, w;
    Uint8  *src, *bitmap = NULL;
    DFBSurfaceDescription dsc;

    SDL_assert(shaper != NULL);
    SDL_assert(shape != NULL);
    data = shaper->driverdata;
    SDL_assert(data != NULL);

    {
        SDL_DFB_RELEASE(data->surface);

        dsc.flags = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT | DSDESC_CAPS;
        dsc.width = shape->w;
        dsc.height = shape->h;
        dsc.caps = DSCAPS_PREMULTIPLIED;
        dsc.pixelformat = DSPF_ARGB;

        SDL_DFB_CHECKERR(devdata->dfb->CreateSurface(devdata->dfb, &dsc, &data->surface));
        SDL_DFB_ALLOC_CLEAR(bitmap, shape->w * shape->h);
        /* Assume that shaper->alphacutoff already has a value, because SDL_SetWindowShape() should have given it one. */
        SDL_CalculateShapeBitmap(shape_mode, shape, bitmap, 1);

        src = bitmap;

        SDL_DFB_CHECK(data->surface->Lock(data->surface, DSLF_WRITE | DSLF_READ, (void **) &pixels, &pitch));

        h = window->h;
        while (h--) {
            for (w = 0; w < window->w; w++) {
                if (*src)
                    pixels[w] = 0xFFFFFFFF;
                else
                    pixels[w] = 0;
                src++;

            }
            pixels += (pitch >> 2);
        }
        SDL_DFB_CHECK(data->surface->Unlock(data->surface));
        SDL_DFB_FREE(bitmap);

        /* FIXME: Need to call this here - Big ?? */
        DirectFB_WM_RedrawLayout(window);
    }

    return 0;
error:
    return -1;
}

#endif /* SDL_VIDEO_DRIVER_DIRECTFB */
