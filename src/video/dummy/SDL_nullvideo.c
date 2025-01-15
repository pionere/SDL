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

/* Dummy SDL video driver implementation; this is just enough to make an
 *  SDL-based application THINK it's got a working video driver, for
 *  applications that call SDL_Init(SDL_INIT_VIDEO) when they don't need it,
 *  and also for use as a collection of stubs when porting SDL to a new
 *  platform for which you haven't yet written a valid video driver.
 *
 * This is also a great way to determine bottlenecks: if you think that SDL
 *  is a performance problem for a given platform, enable this driver, and
 *  then see if your application runs faster without video overhead.
 *
 * Initial work by Ryan C. Gordon (icculus@icculus.org). A good portion
 *  of this was cut-and-pasted from Stephane Peter's work in the AAlib
 *  SDL video driver.  Renamed to "DUMMY" by Sam Lantinga.
 */

#include "SDL_mouse.h"
#include "../../events/SDL_events_c.h"

#include "SDL_nullvideo.h"
#include "SDL_nullevents_c.h"
#include "SDL_nullframebuffer_c.h"
#include "SDL_nullmouse.h"
#include "SDL_nullwindow.h"
#include "SDL_hints.h"

/* Initialization/Query functions */
static int DUMMY_VideoInit(_THIS);
static void DUMMY_VideoQuit(_THIS);

#ifdef SDL_INPUT_LINUXEV
static void DUMMY_EVDEV_Poll(void);
#endif

/* DUMMY driver bootstrap functions */

static void DUMMY_DeleteDevice(_THIS)
{
}

static SDL_bool DUMMY_CreateDevice(SDL_VideoDevice *device)
{
    /* Set the function pointers */
    /* Initialization/Query functions */
    device->VideoInit = DUMMY_VideoInit;
    device->VideoQuit = DUMMY_VideoQuit;
    // device->GetDisplayBounds = DUMMY_GetDisplayBounds;
    // device->GetDisplayUsableBounds = DUMMY_GetDisplayUsableBounds;
    // device->GetDisplayDPI = DUMMY_GetDisplayDPI;
    // device->SetDisplayMode = DUMMY_SetDisplayMode;

    /* Window functions */
    device->CreateSDLWindow = DUMMY_CreateSDLWindow;
    // device->CreateSDLWindowFrom = DUMMY_CreateSDLWindowFrom;
    // device->SetWindowTitle = DUMMY_SetWindowTitle;
    // device->SetWindowIcon = DUMMY_SetWindowIcon;
    // device->SetWindowPosition = DUMMY_SetWindowPosition;
    // device->SetWindowSize = DUMMY_SetWindowSize;
    // device->SetWindowMinimumSize = DUMMY_SetWindowMinimumSize;
    // device->SetWindowMaximumSize = DUMMY_SetWindowMaximumSize;
    // device->GetWindowBordersSize = DUMMY_GetWindowBordersSize;
    // device->GetWindowSizeInPixels = DUMMY_GetWindowSizeInPixels;
    // device->SetWindowOpacity = DUMMY_SetWindowOpacity;
    // device->SetWindowModalFor = DUMMY_SetWindowModalFor;
    // device->SetWindowInputFocus = DUMMY_SetWindowInputFocus;
    device->ShowWindow = DUMMY_ShowWindow;
    device->HideWindow = DUMMY_HideWindow;
    // device->RaiseWindow = DUMMY_RaiseWindow;
    // device->MaximizeWindow = DUMMY_MaximizeWindow;
    // device->MinimizeWindow = DUMMY_MinimizeWindow;
    // device->RestoreWindow = DUMMY_RestoreWindow;
    // device->SetWindowBordered = DUMMY_SetWindowBordered;
    // device->SetWindowResizable = DUMMY_SetWindowResizable;
    // device->SetWindowAlwaysOnTop = DUMMY_SetWindowAlwaysOnTop;
    device->SetWindowFullscreen = DUMMY_SetWindowFullscreen;
    // device->SetWindowGammaRamp = DUMMY_SetWindowGammaRamp;
    // device->GetWindowGammaRamp = DUMMY_GetWindowGammaRamp;
    // device->GetWindowICCProfile = DUMMY_GetWindowICCProfile;
    // device->GetWindowDisplayIndex = DUMMY_GetWindowDisplayIndex;
    // device->SetWindowMouseRect = DUMMY_SetWindowMouseRect;
    // device->SetWindowMouseGrab = DUMMY_SetWindowMouseGrab;
    // device->SetWindowKeyboardGrab = DUMMY_SetWindowKeyboardGrab;
    device->DestroyWindow = DUMMY_DestroyWindow;
    device->CreateWindowFramebuffer = DUMMY_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = DUMMY_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = DUMMY_DestroyWindowFramebuffer;
    // device->OnWindowEnter = DUMMY_OnWindowEnter;
    // device->FlashWindow = DUMMY_FlashWindow;
    /* Shaped-window functions */
    // device->CreateShaper = DUMMY_CreateShaper;
    // device->SetWindowShape = DUMMY_SetWindowShape;
    /* Get some platform dependent window information */
    // device->GetWindowWMInfo = DUMMY_GetWindowWMInfo;
    /* OpenGL support */
#ifdef SDL_VIDEO_OPENGL_EGL
    // device->GL_LoadLibrary = DUMMY_GL_LoadLibrary;
    // device->GL_GetProcAddress = DUMMY_GL_GetProcAddress;
    // device->GL_UnloadLibrary = DUMMY_GL_UnloadLibrary;
    // device->GL_CreateContext = DUMMY_GL_CreateContext;
    // device->GL_MakeCurrent = DUMMY_GL_MakeCurrent;
    // device->GL_GetDrawableSize = DUMMY_GL_GetDrawableSize;
    // device->GL_SetSwapInterval = DUMMY_GL_SetSwapInterval;
    // device->GL_GetSwapInterval = DUMMY_GL_GetSwapInterval;
    // device->GL_SwapWindow = DUMMY_GL_SwapWindow;
    // device->GL_DeleteContext = DUMMY_GL_DeleteContext;
#endif
    /* Vulkan support */
#ifdef SDL_VIDEO_VULKAN
    // device->Vulkan_LoadLibrary = DUMMY_Vulkan_LoadLibrary;
    // device->Vulkan_UnloadLibrary = DUMMY_Vulkan_UnloadLibrary;
    // device->Vulkan_GetInstanceExtensions = DUMMY_Vulkan_GetInstanceExtensions;
    // device->Vulkan_CreateSurface = DUMMY_Vulkan_CreateSurface;
    // device->Vulkan_GetDrawableSize = DUMMY_Vulkan_GetDrawableSize;
#endif
    /* Metal support */
#ifdef SDL_VIDEO_METAL
    // device->Metal_CreateView = DUMMY_Metal_CreateView;
    // device->Metal_DestroyView = DUMMY_Metal_DestroyView;
    // device->Metal_GetLayer = DUMMY_Metal_GetLayer;
    // device->Metal_GetDrawableSize = DUMMY_Metal_GetDrawableSize;
#endif
    /* Event manager functions */
    // device->WaitEventTimeout = DUMMY_WaitEventTimeout;
    // device->SendWakeupEvent = DUMMY_SendWakeupEvent;
    device->PumpEvents = DUMMY_PumpEvents;

    /* Screensaver */
    // device->SuspendScreenSaver = DUMMY_SuspendScreenSaver;

    /* Text input */
    // device->StartTextInput = DUMMY_StartTextInput;
    // device->StopTextInput = DUMMY_StopTextInput;
    // device->SetTextInputRect = DUMMY_SetTextInputRect;
    // device->ClearComposition = DUMMY_ClearComposition;
    // device->IsTextInputShown = DUMMY_IsTextInputShown;

    /* Screen keyboard */
    // device->HasScreenKeyboardSupport = DUMMY_HasScreenKeyboardSupport;
    // device->ShowScreenKeyboard = DUMMY_ShowScreenKeyboard;
    // device->HideScreenKeyboard = DUMMY_HideScreenKeyboard;
    // device->IsScreenKeyboardShown = DUMMY_IsScreenKeyboardShown;

    /* Clipboard */
    // device->SetClipboardText = DUMMY_SetClipboardText;
    // device->GetClipboardText = DUMMY_GetClipboardText;
    // device->HasClipboardText = DUMMY_HasClipboardText;
    // device->SetPrimarySelectionText = DUMMY_SetPrimarySelectionText;
    // device->GetPrimarySelectionText = DUMMY_GetPrimarySelectionText;
    // device->HasPrimarySelectionText = DUMMY_HasPrimarySelectionText;

    /* Hit-testing */
    // device->SetWindowHitTest = DUMMY_SetWindowHitTest;

    /* Tell window that app enabled drag'n'drop events */
    // device->AcceptDragAndDrop = DUMMY_AcceptDragAndDrop;

    device->DeleteDevice = DUMMY_DeleteDevice;

    return SDL_TRUE;
}
/* "SDL dummy video driver" */
const VideoBootStrap DUMMY_bootstrap = {
    "dummy", DUMMY_CreateDevice
};

#ifdef SDL_INPUT_LINUXEV
static SDL_bool DUMMY_CreateDeviceEvdev(_THIS)
{
    SDL_bool result = DUMMY_CreateDevice(_this);
    if (result) {
        _this->PumpEvents = DUMMY_EVDEV_Poll;
    }

    return result;
}

const VideoBootStrap DUMMY_evdev_bootstrap = {
    "evdev", DUMMY_CreateDeviceEvdev
};
void SDL_EVDEV_Init(void);
void SDL_EVDEV_Poll(void);
void SDL_EVDEV_Quit(void);
static void DUMMY_EVDEV_Poll(void)
{
    SDL_EVDEV_Poll();
}
#endif

int DUMMY_VideoInit(_THIS)
{
    int result;
    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;

    /* Use a fake 32-bpp desktop mode */
    current_mode.format = SDL_PIXELFORMAT_RGB888;
    current_mode.w = 1024;
    current_mode.h = 768;
    current_mode.refresh_rate = 60;
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

#ifdef SDL_INPUT_LINUXEV
    SDL_EVDEV_Init();
#endif

    // DUMMY_InitKeyboard();
    DUMMY_InitMouse();

    return result;
}

void DUMMY_VideoQuit(_THIS)
{
#ifdef SDL_INPUT_LINUXEV
    SDL_EVDEV_Quit();
#endif

    // DUMMY_QuitKeyboard();
    // DUMMY_QuitMouse();
}

#endif /* SDL_VIDEO_DRIVER_DUMMY */

/* vi: set ts=4 sw=4 expandtab: */
