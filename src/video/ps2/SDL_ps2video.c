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

#ifdef SDL_VIDEO_DRIVER_PS2

/* PS2 SDL video driver implementation; this is just enough to make an
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
 *  SDL video driver.  Renamed to "PS2" by Sam Lantinga.
 */

#include "SDL_hints.h"
#include "SDL_mouse.h"
#include "../../events/SDL_events_c.h"

#include "SDL_ps2video.h"

#ifdef SDL_VIDEO_VULKAN
#error "Vulkan is configured, but not implemented for ps2."
#endif
#ifdef SDL_VIDEO_METAL
#error "Metal is configured, but not implemented for ps2."
#endif
#ifdef SDL_VIDEO_OPENGL_ANY
#error "OpenGL is configured, but not implemented for ps2."
#endif

/* PS2 driver bootstrap functions */

static int PS2_SetDisplayMode(SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    return 0;
}

static void PS2_DeleteDevice(_THIS)
{
    SDL_free(_this);
}

static int PS2_CreateSDLWindow(_THIS, SDL_Window *window)
{
    SDL_SetKeyboardFocus(window);

    /* Window has been successfully created */
    return 0;
}

static void PS2_DestroyWindow(SDL_Window *window)
{
}

static int PS2_VideoInit(_THIS)
{
    int result;
    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;

    /* 32 bpp for default */
    current_mode.format = SDL_PIXELFORMAT_ABGR8888;
    current_mode.w = 640;
    current_mode.h = 480;
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

    return result;
}

static void PS2_VideoQuit(_THIS)
{
}

static void PS2_PumpEvents(void)
{
    /* do nothing. */
}

static SDL_bool PS2_CreateDevice(SDL_VideoDevice *device)
{
    /* Set the function pointers */
    /* Initialization/Query functions */
    device->VideoInit = PS2_VideoInit;
    device->VideoQuit = PS2_VideoQuit;
    // device->GetDisplayBounds = PS2_GetDisplayBounds;
    // device->GetDisplayUsableBounds = PS2_GetDisplayUsableBounds;
    // device->GetDisplayDPI = PS2_GetDisplayDPI;
    device->SetDisplayMode = PS2_SetDisplayMode;

    /* Window functions */
    device->CreateSDLWindow = PS2_CreateSDLWindow;
    // device->CreateSDLWindowFrom = PS2_CreateSDLWindowFrom;
    // device->SetWindowTitle = PS2_SetWindowTitle;
    // device->SetWindowIcon = PS2_SetWindowIcon;
    // device->SetWindowPosition = PS2_SetWindowPosition;
    // device->SetWindowSize = PS2_SetWindowSize;
    // device->SetWindowMinimumSize = PS2_SetWindowMinimumSize;
    // device->SetWindowMaximumSize = PS2_SetWindowMaximumSize;
    // device->GetWindowBordersSize = PS2_GetWindowBordersSize;
    // device->GetWindowSizeInPixels = PS2_GetWindowSizeInPixels;
    // device->SetWindowOpacity = PS2_SetWindowOpacity;
    // device->SetWindowModalFor = PS2_SetWindowModalFor;
    // device->SetWindowInputFocus = PS2_SetWindowInputFocus;
    // device->ShowWindow = PS2_ShowWindow;
    // device->HideWindow = PS2_HideWindow;
    // device->RaiseWindow = PS2_RaiseWindow;
    // device->MaximizeWindow = PS2_MaximizeWindow;
    // device->MinimizeWindow = PS2_MinimizeWindow;
    // device->RestoreWindow = PS2_RestoreWindow;
    // device->SetWindowBordered = PS2_SetWindowBordered;
    // device->SetWindowResizable = PS2_SetWindowResizable;
    // device->SetWindowAlwaysOnTop = PS2_SetWindowAlwaysOnTop;
    // device->SetWindowFullscreen = PS2_SetWindowFullscreen;
    // device->SetWindowGammaRamp = PS2_SetWindowGammaRamp;
    // device->GetWindowGammaRamp = PS2_GetWindowGammaRamp;
    // device->GetWindowICCProfile = PS2_GetWindowICCProfile;
    // device->GetWindowDisplayIndex = PS2_GetWindowDisplayIndex;
    // device->SetWindowMouseRect = PS2_SetWindowMouseRect;
    // device->SetWindowMouseGrab = PS2_SetWindowMouseGrab;
    // device->SetWindowKeyboardGrab = PS2_SetWindowKeyboardGrab;
    device->DestroyWindow = PS2_DestroyWindow;
    // device->CreateWindowFramebuffer = PS2_CreateWindowFramebuffer;
    // device->UpdateWindowFramebuffer = PS2_UpdateWindowFramebuffer;
    // device->DestroyWindowFramebuffer = PS2_DestroyWindowFramebuffer;
    // device->OnWindowEnter = PS2_OnWindowEnter;
    // device->FlashWindow = PS2_FlashWindow;
    /* Shaped-window functions */
    // device->CreateShaper = PS2_CreateShaper;
    // device->SetWindowShape = PS2_SetWindowShape;
    /* Get some platform dependent window information */
    // device->GetWindowWMInfo = PS2_GetWindowWMInfo;

    /* OpenGL support */
#ifdef SDL_VIDEO_OPENGL_EGL
    // device->GL_LoadLibrary = PS2_GL_LoadLibrary;
    // device->GL_GetProcAddress = PS2_GL_GetProcAddress;
    // device->GL_UnloadLibrary = PS2_GL_UnloadLibrary;
    // device->GL_CreateContext = PS2_GL_CreateContext;
    // device->GL_MakeCurrent = PS2_GL_MakeCurrent;
    // device->GL_GetDrawableSize = PS2_GL_GetDrawableSize;
    // device->GL_SetSwapInterval = PS2_GL_SetSwapInterval;
    // device->GL_GetSwapInterval = PS2_GL_GetSwapInterval;
    // device->GL_SwapWindow = PS2_GL_SwapWindow;
    // device->GL_DeleteContext = PS2_GL_DeleteContext;
#endif

    /* Vulkan support */
#ifdef SDL_VIDEO_VULKAN
    // device->Vulkan_LoadLibrary = PS2_Vulkan_LoadLibrary;
    // device->Vulkan_UnloadLibrary = PS2_Vulkan_UnloadLibrary;
    // device->Vulkan_GetInstanceExtensions = PS2_Vulkan_GetInstanceExtensions;
    // device->Vulkan_CreateSurface = PS2_Vulkan_CreateSurface;
    // device->Vulkan_GetDrawableSize = PS2_Vulkan_GetDrawableSize;
#endif

    /* Metal support */
#ifdef SDL_VIDEO_METAL
    // device->Metal_CreateView = PS2_Metal_CreateView;
    // device->Metal_DestroyView = PS2_Metal_DestroyView;
    // device->Metal_GetLayer = PS2_Metal_GetLayer;
    // device->Metal_GetDrawableSize = PS2_Metal_GetDrawableSize;
#endif

    /* Event manager functions */
    // device->WaitEventTimeout = PS2_WaitEventTimeout;
    // device->SendWakeupEvent = PS2_SendWakeupEvent;
    device->PumpEvents = PS2_PumpEvents;

    /* Screensaver */
    // device->SuspendScreenSaver = PS2_SuspendScreenSaver;

    /* Text input */
    // device->StartTextInput = PS2_StartTextInput;
    // device->StopTextInput = PS2_StopTextInput;
    // device->SetTextInputRect = PS2_SetTextInputRect;
    // device->ClearComposition = PS2_ClearComposition;
    // device->IsTextInputShown = PS2_IsTextInputShown;

    /* Screen keyboard */
    // device->HasScreenKeyboardSupport = PS2_HasScreenKeyboardSupport;
    // device->ShowScreenKeyboard = PS2_ShowScreenKeyboard;
    // device->HideScreenKeyboard = PS2_HideScreenKeyboard;
    // device->IsScreenKeyboardShown = PS2_IsScreenKeyboardShown;

    /* Clipboard */
    // device->SetClipboardText = PS2_SetClipboardText;
    // device->GetClipboardText = PS2_GetClipboardText;
    // device->HasClipboardText = PS2_HasClipboardText;
    // device->SetPrimarySelectionText = PS2_SetPrimarySelectionText;
    // device->GetPrimarySelectionText = PS2_GetPrimarySelectionText;
    // device->HasPrimarySelectionText = PS2_HasPrimarySelectionText;

    /* Hit-testing */
    // device->SetWindowHitTest = PS2_SetWindowHitTest;

    /* Tell window that app enabled drag'n'drop events */
    // device->AcceptDragAndDrop = PS2_AcceptDragAndDrop;

    device->DeleteDevice = PS2_DeleteDevice;

    return SDL_TRUE;
}
// "PS2 Video Driver"
const VideoBootStrap PS2_bootstrap = {
    "PS2",
    PS2_CreateDevice
};

#endif /* SDL_VIDEO_DRIVER_PS2 */

/* vi: set ts=4 sw=4 expandtab: */
