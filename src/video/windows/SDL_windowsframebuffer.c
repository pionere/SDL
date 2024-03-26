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
#include "../../SDL_internal.h"

#if defined(SDL_VIDEO_DRIVER_WINDOWS) && !defined(__XBOXONE__) && !defined(__XBOXSERIES__)

#include "SDL_windowsvideo.h"
#include "SDL_windowsframebuffer.h"

int WIN_CreateWindowFramebuffer(SDL_Window *window, Uint32 *format, void **pixels, int *pitch)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    HDC hdc = data->hdc;
    char bmi_data[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)];
    LPBITMAPINFO bmi;
    HBITMAP hbm;
    int w, h;
    Uint32 surface_format;

    /* Free the old framebuffer surface */
    WIN_DestroyWindowFramebuffer(window);

    WIN_GetWindowSizeInPixels(window, &w, &h);

    /* Find out the format of the screen */
    SDL_zeroa(bmi_data);
    bmi = (LPBITMAPINFO)bmi_data;
    bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

    hbm = CreateCompatibleBitmap(hdc, 1, 1);
    /* The second call to GetDIBits() fills in the bitfields */
    GetDIBits(hdc, hbm, 0, 0, NULL, bmi, DIB_RGB_COLORS);
    GetDIBits(hdc, hbm, 0, 0, NULL, bmi, DIB_RGB_COLORS);
    DeleteObject(hbm);

    surface_format = SDL_PIXELFORMAT_UNKNOWN;
    if (bmi->bmiHeader.biCompression == BI_BITFIELDS) {
        int bpp;
        Uint32 *masks;

        bpp = /* bmi->bmiHeader.biPlanes * */bmi->bmiHeader.biBitCount;
        masks = (Uint32 *)bmi->bmiColors;
        surface_format = SDL_MasksToPixelFormatEnum(bpp, masks[0], masks[1], masks[2], 0);
    }
    if (surface_format == SDL_PIXELFORMAT_UNKNOWN) {
        /* We'll use RGB format for now */
        surface_format = SDL_PIXELFORMAT_RGB888;

        /* Create a new one */
        SDL_zeroa(bmi_data);
        bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi->bmiHeader.biPlanes = 1;
        bmi->bmiHeader.biBitCount = 32;
        bmi->bmiHeader.biCompression = BI_RGB;
    }
    *format = surface_format;

    /* Fill in the size information */
    SDL_assert(!SDL_ISPIXELFORMAT_FOURCC(surface_format));
    *pitch = (((w * SDL_PIXELBPP(surface_format)) + 3) & ~3);
    bmi->bmiHeader.biWidth = w;
    bmi->bmiHeader.biHeight = -h; /* negative for topdown bitmap */
    bmi->bmiHeader.biSizeImage = (DWORD)h * (*pitch);

    data->mdc = CreateCompatibleDC(hdc);
    data->hbm = CreateDIBSection(hdc, bmi, DIB_RGB_COLORS, pixels, NULL, 0);

    if (!data->hbm) {
        return WIN_SetError("Unable to create DIB");
    }
    SelectObject(data->mdc, data->hbm);

    return 0;
}

int WIN_UpdateWindowFramebuffer(SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    int i;

    for (i = 0; i < numrects; ++i) {
        BitBlt(data->hdc, rects[i].x, rects[i].y, rects[i].w, rects[i].h,
               data->mdc, rects[i].x, rects[i].y, SRCCOPY);
    }
    return 0;
}

void WIN_DestroyWindowFramebuffer(SDL_Window *window)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;

    if (!data) {
        /* The window wasn't fully initialized */
        return;
    }

    if (data->mdc) {
        DeleteDC(data->mdc);
        data->mdc = NULL;
    }
    if (data->hbm) {
        DeleteObject(data->hbm);
        data->hbm = NULL;
    }
}

#endif /* SDL_VIDEO_DRIVER_WINDOWS */

/* vi: set ts=4 sw=4 expandtab: */
