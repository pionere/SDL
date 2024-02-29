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

#include <stdlib.h>
#ifdef NULL
#undef NULL
#endif
#include "../../SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_NGAGE

#ifdef __cplusplus
extern "C" {
#endif

#include "SDL_video.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#ifdef __cplusplus
}
#endif

#include "SDL_ngagevideo.h"
#include "SDL_ngagewindow.h"
#include "SDL_ngageevents_c.h"
#include "SDL_ngageframebuffer_c.h"

#ifdef SDL_VIDEO_VULKAN
#error "Vulkan is configured, but not implemented for ngage."
#endif
#ifdef SDL_VIDEO_METAL
#error "Metal is configured, but not implemented for ngage."
#endif

/* Instance */
Ngage_VideoData ngageVideoData;

/* Initialization/Query functions */
static int NGAGE_VideoInit(_THIS);
static int NGAGE_SetDisplayMode(SDL_VideoDisplay *display, SDL_DisplayMode *mode);
static void NGAGE_VideoQuit(_THIS);

/* NGAGE driver bootstrap functions */

static void NGAGE_DeleteDevice(SDL_VideoDevice *device)
{
    Ngage_VideoData *phdata = &ngageVideoData;

        /* Free Epoc resources */

        /* Disable events for me */
        if (phdata->NGAGE_WsEventStatus != KRequestPending) {
            phdata->NGAGE_WsSession.EventReadyCancel();
        }
        if (phdata->NGAGE_RedrawEventStatus != KRequestPending) {
            phdata->NGAGE_WsSession.RedrawReadyCancel();
        }

        free(phdata->NGAGE_DrawDevice);

        if (phdata->NGAGE_WsWindow.WsHandle()) {
            phdata->NGAGE_WsWindow.Close();
        }

        if (phdata->NGAGE_WsWindowGroup.WsHandle()) {
            phdata->NGAGE_WsWindowGroup.Close();
        }

        delete phdata->NGAGE_WindowGc;
        phdata->NGAGE_WindowGc = NULL;

        delete phdata->NGAGE_WsScreen;
        phdata->NGAGE_WsScreen = NULL;

        if (phdata->NGAGE_WsSession.WsHandle()) {
            phdata->NGAGE_WsSession.Close();
        }

        SDL_zero(ngageVideoData);

    SDL_free(device);
}

static SDL_VideoDevice *NGAGE_CreateDevice(void)
{
    SDL_VideoDevice *device;
    Ngage_VideoData *phdata = &ngageVideoData;

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return 0;
    }

    /* General video */
    device->VideoInit = NGAGE_VideoInit;
    device->VideoQuit = NGAGE_VideoQuit;
    device->SetDisplayMode = NGAGE_SetDisplayMode;
    device->PumpEvents = NGAGE_PumpEvents;
    device->CreateWindowFramebuffer = SDL_NGAGE_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = SDL_NGAGE_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = SDL_NGAGE_DestroyWindowFramebuffer;
    device->free = NGAGE_DeleteDevice;

    /* "Window" */
    device->CreateSDLWindow = NGAGE_CreateWindow;
    device->DestroyWindow = NGAGE_DestroyWindow;

    return device;
}

const VideoBootStrap NGAGE_bootstrap = {
    "ngage", NGAGE_CreateDevice
};

int NGAGE_VideoInit(_THIS)
{
    int result;
    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;

    /* Use 12-bpp desktop mode */
    current_mode.format = SDL_PIXELFORMAT_RGB444;
    current_mode.w = 176;
    current_mode.h = 208;
    current_mode.refresh_rate = 0;
    current_mode.driverdata = NULL;

    SDL_zero(display);
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;
    // display.driverdata = NULL;

    SDL_AddDisplayMode(&display, &current_mode);
    result = SDL_AddVideoDisplay(&display, SDL_FALSE);
    if (result < 0) {
        SDL_free(display.display_modes);
    }

    return result;
}

static int NGAGE_SetDisplayMode(SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    return 0;
}

void NGAGE_VideoQuit(_THIS)
{
}

#endif /* SDL_VIDEO_DRIVER_NGAGE */

/* vi: set ts=4 sw=4 expandtab: */
