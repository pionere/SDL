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

#ifdef SDL_VIDEO_DRIVER_OFFSCREEN

/* Offscreen video driver is similar to dummy driver, however its purpose
 * is enabling applications to use some of the SDL video functionality
 * (notably context creation) while not requiring a display output.
 *
 * An example would be running a graphical program on a headless box
 * for automated testing.
 */

#include "SDL_video.h"

#include "SDL_offscreenvideo.h"
#include "SDL_offscreenevents_c.h"
#include "SDL_offscreenframebuffer_c.h"
#include "SDL_offscreenopengles.h"
#include "SDL_offscreenwindow.h"

/* Initialization/Query functions */
static int OFFSCREEN_VideoInit(_THIS);
static int OFFSCREEN_SetDisplayMode(SDL_VideoDisplay *display, SDL_DisplayMode *mode);
static void OFFSCREEN_VideoQuit(_THIS);

/* OFFSCREEN driver bootstrap functions */

static void OFFSCREEN_DeleteDevice(_THIS)
{
    SDL_free(_this);
}

static SDL_VideoDevice *OFFSCREEN_CreateDevice(void)
{
    SDL_VideoDevice *device;

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return 0;
    }

    /* Set the function pointers */
    /* Initialization/Query functions */
    device->VideoInit = OFFSCREEN_VideoInit;
    device->VideoQuit = OFFSCREEN_VideoQuit;
    // device->GetDisplayBounds = OFFSCREEN_GetDisplayBounds;
    // device->GetDisplayUsableBounds = OFFSCREEN_GetDisplayUsableBounds;
    // device->GetDisplayDPI = OFFSCREEN_GetDisplayDPI;
    device->SetDisplayMode = OFFSCREEN_SetDisplayMode;

    /* Window functions */
    device->CreateSDLWindow = OFFSCREEN_CreateSDLWindow;
    // device->CreateSDLWindowFrom = OFFSCREEN_CreateSDLWindowFrom;
    // device->SetWindowTitle = OFFSCREEN_SetWindowTitle;
    // device->SetWindowIcon = OFFSCREEN_SetWindowIcon;
    // device->SetWindowPosition = OFFSCREEN_SetWindowPosition;
    // device->SetWindowSize = OFFSCREEN_SetWindowSize;
    // device->SetWindowMinimumSize = OFFSCREEN_SetWindowMinimumSize;
    // device->SetWindowMaximumSize = OFFSCREEN_SetWindowMaximumSize;
    // device->GetWindowBordersSize = OFFSCREEN_GetWindowBordersSize;
    // device->GetWindowSizeInPixels = OFFSCREEN_GetWindowSizeInPixels;
    // device->SetWindowOpacity = OFFSCREEN_SetWindowOpacity;
    // device->SetWindowModalFor = OFFSCREEN_SetWindowModalFor;
    // device->SetWindowInputFocus = OFFSCREEN_SetWindowInputFocus;
    // device->ShowWindow = OFFSCREEN_ShowWindow;
    // device->HideWindow = OFFSCREEN_HideWindow;
    // device->RaiseWindow = OFFSCREEN_RaiseWindow;
    // device->MaximizeWindow = OFFSCREEN_MaximizeWindow;
    // device->MinimizeWindow = OFFSCREEN_MinimizeWindow;
    // device->RestoreWindow = OFFSCREEN_RestoreWindow;
    // device->SetWindowBordered = OFFSCREEN_SetWindowBordered;
    // device->SetWindowResizable = OFFSCREEN_SetWindowResizable;
    // device->SetWindowAlwaysOnTop = OFFSCREEN_SetWindowAlwaysOnTop;
    // device->SetWindowFullscreen = OFFSCREEN_SetWindowFullscreen;
    // device->SetWindowGammaRamp = OFFSCREEN_SetWindowGammaRamp;
    // device->GetWindowGammaRamp = OFFSCREEN_GetWindowGammaRamp;
    // device->GetWindowICCProfile = OFFSCREEN_GetWindowICCProfile;
    // device->GetWindowDisplayIndex = OFFSCREEN_GetWindowDisplayIndex;
    // device->SetWindowMouseRect = OFFSCREEN_SetWindowMouseRect;
    // device->SetWindowMouseGrab = OFFSCREEN_SetWindowMouseGrab;
    // device->SetWindowKeyboardGrab = OFFSCREEN_SetWindowKeyboardGrab;
    device->DestroyWindow = OFFSCREEN_DestroyWindow;
    device->CreateWindowFramebuffer = OFFSCREEN_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = OFFSCREEN_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = OFFSCREEN_DestroyWindowFramebuffer;
    // device->OnWindowEnter = OFFSCREEN_OnWindowEnter;
    // device->FlashWindow = OFFSCREEN_FlashWindow;
    /* Shaped-window functions */
    // device->CreateShaper = OFFSCREEN_CreateShaper;
    // device->SetWindowShape = OFFSCREEN_SetWindowShape;
    /* Get some platform dependent window information */
    // device->GetWindowWMInfo = OFFSCREEN_GetWindowWMInfo;

    /* OpenGL support */
#ifdef SDL_VIDEO_OPENGL_EGL
    device->GL_LoadLibrary = OFFSCREEN_GL_LoadLibrary;
    device->GL_GetProcAddress = OFFSCREEN_GL_GetProcAddress;
    device->GL_UnloadLibrary = OFFSCREEN_GL_UnloadLibrary;
    device->GL_CreateContext = OFFSCREEN_GL_CreateContext;
    device->GL_MakeCurrent = OFFSCREEN_GL_MakeCurrent;
    device->GL_GetDrawableSize = OFFSCREEN_GL_GetDrawableSize;
    device->GL_SetSwapInterval = OFFSCREEN_GL_SetSwapInterval;
    device->GL_GetSwapInterval = OFFSCREEN_GL_GetSwapInterval;
    device->GL_SwapWindow = OFFSCREEN_GL_SwapWindow;
    device->GL_DeleteContext = OFFSCREEN_GL_DeleteContext;
    // device->GL_DefaultProfileConfig = OFFSCREEN_GL_DefaultProfileConfig;
#endif

    /* Vulkan support */
#ifdef SDL_VIDEO_VULKAN
    // device->Vulkan_LoadLibrary = OFFSCREEN_Vulkan_LoadLibrary;
    // device->Vulkan_UnloadLibrary = OFFSCREEN_Vulkan_UnloadLibrary;
    // device->Vulkan_GetInstanceExtensions = OFFSCREEN_Vulkan_GetInstanceExtensions;
    // device->Vulkan_CreateSurface = OFFSCREEN_Vulkan_CreateSurface;
    // device->Vulkan_GetDrawableSize = OFFSCREEN_Vulkan_GetDrawableSize;
#endif

    /* Metal support */
#ifdef SDL_VIDEO_METAL
    // device->Metal_CreateView = OFFSCREEN_Metal_CreateView;
    // device->Metal_DestroyView = OFFSCREEN_Metal_DestroyView;
    // device->Metal_GetLayer = OFFSCREEN_Metal_GetLayer;
    // device->Metal_GetDrawableSize = OFFSCREEN_Metal_GetDrawableSize;
#endif

    /* Event manager functions */
    // device->WaitEventTimeout = OFFSCREEN_WaitEventTimeout;
    // device->SendWakeupEvent = OFFSCREEN_SendWakeupEvent;
    device->PumpEvents = OFFSCREEN_PumpEvents;

    /* Screensaver */
    // device->SuspendScreenSaver = OFFSCREEN_SuspendScreenSaver;

    /* Text input */
    // device->StartTextInput = OFFSCREEN_StartTextInput;
    // device->StopTextInput = OFFSCREEN_StopTextInput;
    // device->SetTextInputRect = OFFSCREEN_SetTextInputRect;
    // device->ClearComposition = OFFSCREEN_ClearComposition;
    // device->IsTextInputShown = OFFSCREEN_IsTextInputShown;

    /* Screen keyboard */
    // device->HasScreenKeyboardSupport = OFFSCREEN_HasScreenKeyboardSupport;
    // device->ShowScreenKeyboard = OFFSCREEN_ShowScreenKeyboard;
    // device->HideScreenKeyboard = OFFSCREEN_HideScreenKeyboard;
    // device->IsScreenKeyboardShown = OFFSCREEN_IsScreenKeyboardShown;

    /* Clipboard */
    // device->SetClipboardText = OFFSCREEN_SetClipboardText;
    // device->GetClipboardText = OFFSCREEN_GetClipboardText;
    // device->HasClipboardText = OFFSCREEN_HasClipboardText;
    // device->SetPrimarySelectionText = OFFSCREEN_SetPrimarySelectionText;
    // device->GetPrimarySelectionText = OFFSCREEN_GetPrimarySelectionText;
    // device->HasPrimarySelectionText = OFFSCREEN_HasPrimarySelectionText;

    /* Hit-testing */
    // device->SetWindowHitTest = OFFSCREEN_SetWindowHitTest;

    /* Tell window that app enabled drag'n'drop events */
    // device->AcceptDragAndDrop = OFFSCREEN_AcceptDragAndDrop;

    device->free = OFFSCREEN_DeleteDevice;

    return device;
}
/* "SDL offscreen video driver" */
const VideoBootStrap OFFSCREEN_bootstrap = {
    "offscreen", OFFSCREEN_CreateDevice
};

int OFFSCREEN_VideoInit(_THIS)
{
    int result;
    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;

    /* Use a fake 32-bpp desktop mode */
    current_mode.format = SDL_PIXELFORMAT_RGB888;
    current_mode.w = 1024;
    current_mode.h = 768;
    current_mode.refresh_rate = 0;
    current_mode.driverdata = NULL;

    SDL_zero(display);
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;
    // display.driverdata = NULL;

    SDL_zero(current_mode); // add empty mode to let the user set it freely
    SDL_AddDisplayMode(&display, &current_mode);
    result = SDL_AddVideoDisplay(&display, SDL_FALSE);
    if (result < 0) {
        SDL_free(display.display_modes);
    }

    return result;
}

static int OFFSCREEN_SetDisplayMode(SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    return 0;
}

void OFFSCREEN_VideoQuit(_THIS)
{
}

#endif /* SDL_VIDEO_DRIVER_OFFSCREEN */

/* vi: set ts=4 sw=4 expandtab: */
