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
#ifdef SDL_VIDEO_OPENGL_ANY
#error "OpenGL is configured, but not implemented for ngage."
#endif

/* Instance */
Ngage_VideoData ngageVideoData;

/* Initialization/Query functions */
static int NGAGE_VideoInit(_THIS);
static int NGAGE_SetDisplayMode(SDL_VideoDisplay *display, SDL_DisplayMode *mode);
static void NGAGE_VideoQuit(_THIS);

/* NGAGE driver bootstrap functions */

static void NGAGE_DeleteDevice(_THIS)
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

    SDL_free(_this);
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

    /* Set the function pointers */
    /* Initialization/Query functions */
    device->VideoInit = NGAGE_VideoInit;
    device->VideoQuit = NGAGE_VideoQuit;
    // device->GetDisplayBounds = NGAGE_GetDisplayBounds;
    // device->GetDisplayUsableBounds = NGAGE_GetDisplayUsableBounds;
    // device->GetDisplayDPI = NGAGE_GetDisplayDPI;
    device->SetDisplayMode = NGAGE_SetDisplayMode;

    /* Window functions */
    device->CreateSDLWindow = NGAGE_CreateSDLWindow;
    // device->CreateSDLWindowFrom = NGAGE_CreateSDLWindowFrom;
    // device->SetWindowTitle = NGAGE_SetWindowTitle;
    // device->SetWindowIcon = NGAGE_SetWindowIcon;
    // device->SetWindowPosition = NGAGE_SetWindowPosition;
    // device->SetWindowSize = NGAGE_SetWindowSize;
    // device->SetWindowMinimumSize = NGAGE_SetWindowMinimumSize;
    // device->SetWindowMaximumSize = NGAGE_SetWindowMaximumSize;
    // device->GetWindowBordersSize = NGAGE_GetWindowBordersSize;
    // device->GetWindowSizeInPixels = NGAGE_GetWindowSizeInPixels;
    // device->SetWindowOpacity = NGAGE_SetWindowOpacity;
    // device->SetWindowModalFor = NGAGE_SetWindowModalFor;
    // device->SetWindowInputFocus = NGAGE_SetWindowInputFocus;
    // device->ShowWindow = NGAGE_ShowWindow;
    // device->HideWindow = NGAGE_HideWindow;
    // device->RaiseWindow = NGAGE_RaiseWindow;
    // device->MaximizeWindow = NGAGE_MaximizeWindow;
    // device->MinimizeWindow = NGAGE_MinimizeWindow;
    // device->RestoreWindow = NGAGE_RestoreWindow;
    // device->SetWindowBordered = NGAGE_SetWindowBordered;
    // device->SetWindowResizable = NGAGE_SetWindowResizable;
    // device->SetWindowAlwaysOnTop = NGAGE_SetWindowAlwaysOnTop;
    // device->SetWindowFullscreen = NGAGE_SetWindowFullscreen;
    // device->SetWindowGammaRamp = NGAGE_SetWindowGammaRamp;
    // device->GetWindowGammaRamp = NGAGE_GetWindowGammaRamp;
    // device->GetWindowICCProfile = NGAGE_GetWindowICCProfile;
    // device->GetWindowDisplayIndex = NGAGE_GetWindowDisplayIndex;
    // device->SetWindowMouseRect = NGAGE_SetWindowMouseRect;
    // device->SetWindowMouseGrab = NGAGE_SetWindowMouseGrab;
    // device->SetWindowKeyboardGrab = NGAGE_SetWindowKeyboardGrab;
    device->DestroyWindow = NGAGE_DestroyWindow;
    device->CreateWindowFramebuffer = NGAGE_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = NGAGE_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = NGAGE_DestroyWindowFramebuffer;
    // device->OnWindowEnter = NGAGE_OnWindowEnter;
    // device->FlashWindow = NGAGE_FlashWindow;
    /* Shaped-window functions */
    // device->CreateShaper = NGAGE_CreateShaper;
    // device->SetWindowShape = NGAGE_SetWindowShape;
    /* Get some platform dependent window information */
    // device->GetWindowWMInfo = NGAGE_GetWindowWMInfo;

    /* OpenGL support */
#ifdef SDL_VIDEO_OPENGL_EGL
    // device->GL_LoadLibrary = NGAGE_GL_LoadLibrary;
    // device->GL_GetProcAddress = NGAGE_GL_GetProcAddress;
    // device->GL_UnloadLibrary = NGAGE_GL_UnloadLibrary;
    // device->GL_CreateContext = NGAGE_GL_CreateContext;
    // device->GL_MakeCurrent = NGAGE_GL_MakeCurrent;
    // device->GL_GetDrawableSize = NGAGE_GL_GetDrawableSize;
    // device->GL_SetSwapInterval = NGAGE_GL_SetSwapInterval;
    // device->GL_GetSwapInterval = NGAGE_GL_GetSwapInterval;
    // device->GL_SwapWindow = NGAGE_GL_SwapWindow;
    // device->GL_DeleteContext = NGAGE_GL_DeleteContext;
    // device->GL_DefaultProfileConfig = NGAGE_GL_DefaultProfileConfig;
#endif

    /* Vulkan support */
#ifdef SDL_VIDEO_VULKAN
    // device->Vulkan_LoadLibrary = NGAGE_Vulkan_LoadLibrary;
    // device->Vulkan_UnloadLibrary = NGAGE_Vulkan_UnloadLibrary;
    // device->Vulkan_GetInstanceExtensions = NGAGE_Vulkan_GetInstanceExtensions;
    // device->Vulkan_CreateSurface = NGAGE_Vulkan_CreateSurface;
    // device->Vulkan_GetDrawableSize = NGAGE_Vulkan_GetDrawableSize;
#endif

    /* Metal support */
#ifdef SDL_VIDEO_METAL
    // device->Metal_CreateView = NGAGE_Metal_CreateView;
    // device->Metal_DestroyView = NGAGE_Metal_DestroyView;
    // device->Metal_GetLayer = NGAGE_Metal_GetLayer;
    // device->Metal_GetDrawableSize = NGAGE_Metal_GetDrawableSize;
#endif

    /* Event manager functions */
    // device->WaitEventTimeout = NGAGE_WaitEventTimeout;
    // device->SendWakeupEvent = NGAGE_SendWakeupEvent;
    device->PumpEvents = NGAGE_PumpEvents;

    /* Screensaver */
    // device->SuspendScreenSaver = NGAGE_SuspendScreenSaver;

    /* Text input */
    // device->StartTextInput = NGAGE_StartTextInput;
    // device->StopTextInput = NGAGE_StopTextInput;
    // device->SetTextInputRect = NGAGE_SetTextInputRect;
    // device->ClearComposition = NGAGE_ClearComposition;
    // device->IsTextInputShown = NGAGE_IsTextInputShown;

    /* Screen keyboard */
    // device->HasScreenKeyboardSupport = NGAGE_HasScreenKeyboardSupport;
    // device->ShowScreenKeyboard = NGAGE_ShowScreenKeyboard;
    // device->HideScreenKeyboard = NGAGE_HideScreenKeyboard;
    // device->IsScreenKeyboardShown = NGAGE_IsScreenKeyboardShown;

    /* Clipboard */
    // device->SetClipboardText = NGAGE_SetClipboardText;
    // device->GetClipboardText = NGAGE_GetClipboardText;
    // device->HasClipboardText = NGAGE_HasClipboardText;
    // device->SetPrimarySelectionText = NGAGE_SetPrimarySelectionText;
    // device->GetPrimarySelectionText = NGAGE_GetPrimarySelectionText;
    // device->HasPrimarySelectionText = NGAGE_HasPrimarySelectionText;

    /* Hit-testing */
    // device->SetWindowHitTest = NGAGE_SetWindowHitTest;

    /* Tell window that app enabled drag'n'drop events */
    // device->AcceptDragAndDrop = NGAGE_AcceptDragAndDrop;

    device->DeleteDevice = NGAGE_DeleteDevice;

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
