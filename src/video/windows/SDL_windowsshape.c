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

#if defined(SDL_VIDEO_DRIVER_WINDOWS) && !defined(__XBOXONE__) && !defined(__XBOXSERIES__)

#include "SDL_windowsshape.h"
#include "SDL_windowsvideo.h"

SDL_WindowShaper *WIN_CreateShaper(SDL_Window *window)
{
    SDL_WindowShaper *result = (SDL_WindowShaper *)SDL_calloc(1, sizeof(SDL_WindowShaper));

    if (!result) {
        SDL_OutOfMemory();
        return NULL;
    }
    SDL_INLINE_COMPILE_TIME_ASSERT(win_shape_mode, ShapeModeDefault == 0);
    // result->mode.mode = ShapeModeDefault;
    result->mode.parameters.binarizationCutoff = 1;
    // result->userx = result->usery = 0;
    // result->hasshape = SDL_FALSE;
    // result->driverdata = NULL;
    window->shaper = result;
    // WIN_ResizeWindowShape(window);

    return result;
}

static void CombineRectRegions(SDL_ShapeTree *node, void *closure)
{
    HRGN mask_region = *((HRGN *)closure), temp_region = NULL;
    if (node && node->kind == OpaqueShape) {
        /* Win32 API regions exclude their outline, so we widen the region by one pixel in each direction to include the real outline. */
        temp_region = CreateRectRgn(node->data.shape.x, node->data.shape.y, node->data.shape.x + node->data.shape.w + 1, node->data.shape.y + node->data.shape.h + 1);
        if (mask_region != NULL) {
            CombineRgn(mask_region, mask_region, temp_region, RGN_OR);
            DeleteObject(temp_region);
        } else {
            *((HRGN *)closure) = temp_region;
        }
    }
}

int WIN_SetWindowShape(SDL_Window *window, SDL_Surface *shape, const SDL_WindowShapeMode *shape_mode)
{
    SDL_WindowShaper *shaper = window->shaper;
    SDL_ShapeTree *tree;
    HRGN mask_region = NULL;

    SDL_assert(shaper != NULL);
    SDL_assert(shape != NULL);

    tree = SDL_CalculateShapeTree(shape_mode, shape);
    if (tree == NULL) {
        return -1;
    }

    SDL_TraverseShapeTree(tree, &CombineRectRegions, &mask_region);
    SDL_FreeShapeTree(tree);
    SDL_assert(mask_region != NULL);

    SetWindowRgn(((SDL_WindowData *)(window->driverdata))->hwnd, mask_region, TRUE);

    return 0;
}

int WIN_ResizeWindowShape(SDL_Window *window)
{
    SDL_assert(window != NULL);
    SDL_assert(window->shaper != NULL);

    if (window->shaper->hasshape) {
        window->shaper->hasshape = SDL_FALSE;

        window->shaper->userx = window->wrect.x;
        window->shaper->usery = window->wrect.y;
        SDL_SetWindowPosition(window, -1000, -1000);
    }

    return 0;
}

#endif /* SDL_VIDEO_DRIVER_WINDOWS */
