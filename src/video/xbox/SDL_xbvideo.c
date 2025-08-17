/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_XBOX

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
 *  SDL video driver.  Renamed to "XBOX" by Sam Lantinga.
 */

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include "SDL_xbvideo.h"
#include "SDL_xbevents_c.h"
#include "SDL_xbframebuffer_c.h"

#include <hal/video.h>

#ifdef SDL_VIDEO_VULKAN
#error "Vulkan is configured, but not implemented for XBOX."
#endif
#ifdef SDL_VIDEO_METAL
#error "Metal is configured, but not implemented for XBOX."
#endif
#ifdef SDL_VIDEO_OPENGL_ANY
#error "OpenGL is configured, but not the implemented XBOX."
#endif

/* Initialization/Query functions */
static int XBOX_VideoInit(_THIS);
static int XBOX_SetDisplayMode(SDL_VideoDisplay * display, SDL_DisplayMode * mode);
static void XBOX_VideoQuit(_THIS);

/* Currently only one window */
static SDL_Window *xbox_window = NULL;

static int
XBOX_CreateSDLWindow(_THIS, SDL_Window * window)
{
    VIDEO_MODE vm;
    if (xbox_window) {
        return SDL_SetError("Xbox only supports one window");
    }

    /* Adjust the window data to match the screen */
    vm = XVideoGetMode();
    window->wrect.x = 0;
    window->wrect.y = 0;
    window->wrect.w = vm.width;
    window->wrect.h = vm.height;

    window->flags &= ~SDL_WINDOW_RESIZABLE;     /* window is NEVER resizeable */
    window->flags &= ~SDL_WINDOW_HIDDEN;
    window->flags |= SDL_WINDOW_SHOWN;          /* only one window on Xbox */
    window->flags |= SDL_WINDOW_FULLSCREEN;

    /* One window, it always has focus */
    SDL_SetMouseFocus(window);
    SDL_SetKeyboardFocus(window);

    xbox_window = window;

    return 0;
}

static void XBOX_DeleteDevice(_THIS)
{
}

static SDL_bool XBOX_CreateDevice(SDL_VideoDevice *device)
{
    /* Set the function pointers */
    /* Initialization/Query functions */
    device->VideoInit = XBOX_VideoInit;
    device->VideoQuit = XBOX_VideoQuit;
    // device->GetDisplayBounds = XBOX_GetDisplayBounds;
    // device->GetDisplayUsableBounds = XBOX_GetDisplayUsableBounds;
    // device->GetDisplayDPI = XBOX_GetDisplayDPI;
    device->SetDisplayMode = XBOX_SetDisplayMode;

    /* Window functions */
    device->CreateSDLWindow = XBOX_CreateSDLWindow;
    // device->CreateSDLWindowFrom = XBOX_CreateSDLWindowFrom;
    // device->SetWindowTitle = XBOX_SetWindowTitle;
    // device->SetWindowIcon = XBOX_SetWindowIcon;
    // device->SetWindowPosition = XBOX_SetWindowPosition;
    // device->SetWindowSize = XBOX_SetWindowSize;
    // device->SetWindowMinimumSize = XBOX_SetWindowMinimumSize;
    // device->SetWindowMaximumSize = XBOX_SetWindowMaximumSize;
    // device->GetWindowBordersSize = XBOX_GetWindowBordersSize;
    // device->GetWindowSizeInPixels = XBOX_GetWindowSizeInPixels;
    // device->SetWindowOpacity = XBOX_SetWindowOpacity;
    // device->SetWindowModalFor = XBOX_SetWindowModalFor;
    // device->SetWindowInputFocus = XBOX_SetWindowInputFocus;
    // device->ShowWindow = XBOX_ShowWindow;
    // device->HideWindow = XBOX_HideWindow;
    // device->RaiseWindow = XBOX_RaiseWindow;
    // device->MaximizeWindow = XBOX_MaximizeWindow;
    // device->MinimizeWindow = XBOX_MinimizeWindow;
    // device->RestoreWindow = XBOX_RestoreWindow;
    // device->SetWindowBordered = XBOX_SetWindowBordered;
    // device->SetWindowResizable = XBOX_SetWindowResizable;
    // device->SetWindowAlwaysOnTop = XBOX_SetWindowAlwaysOnTop;
    // device->SetWindowFullscreen = XBOX_SetWindowFullscreen;
    // device->SetWindowGammaRamp = XBOX_SetWindowGammaRamp;
    // device->GetWindowGammaRamp = XBOX_GetWindowGammaRamp;
    // device->GetWindowICCProfile = XBOX_GetWindowICCProfile;
    // device->GetWindowDisplayIndex = XBOX_GetWindowDisplayIndex;
    // device->SetWindowMouseRect = XBOX_SetWindowMouseRect;
    // device->SetWindowMouseGrab = XBOX_SetWindowMouseGrab;
    // device->SetWindowKeyboardGrab = XBOX_SetWindowKeyboardGrab;
    // device->DestroyWindow = XBOX_DestroyWindow;
    // * Framebuffer disabled, causes issues on high-framerate updates. SDL still emulates this.
    device->CreateWindowFramebuffer = SDL_XBOX_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = SDL_XBOX_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = SDL_XBOX_DestroyWindowFramebuffer;
    // device->OnWindowEnter = XBOX_OnWindowEnter;
    // device->FlashWindow = XBOX_FlashWindow;
    /* Shaped-window functions */
    // device->CreateShaper = XBOX_CreateShaper;
    // device->SetWindowShape = XBOX_SetWindowShape;
    /* Get some platform dependent window information */
    // device->GetWindowWMInfo = XBOX_GetWindowWMInfo;

    /* OpenGL support */
#ifdef SDL_VIDEO_OPENGL_ANY
    // device->GL_LoadLibrary = XBOX_GL_LoadLibrary;
    // device->GL_GetProcAddress = XBOX_GL_GetProcAddress;
    // device->GL_CreateContext = XBOX_GL_CreateContext;
    // device->GL_UnloadLibrary = XBOX_GLES_UnloadLibrary;
    // device->GL_MakeCurrent = XBOX_GLES_MakeCurrent;
    // device->GL_GetDrawableSize = XBOX_GLES_GetDrawableSize;
    // device->GL_SetSwapInterval = XBOX_GLES_SetSwapInterval;
    // device->GL_GetSwapInterval = XBOX_GLES_GetSwapInterval;
    // device->GL_SwapWindow = XBOX_GLES_SwapWindow;
    // device->GL_DeleteContext = XBOX_GLES_DeleteContext;
#endif

    /* Vulkan support */
#ifdef SDL_VIDEO_VULKAN
    // device->Vulkan_LoadLibrary = XBOX_Vulkan_LoadLibrary;
    // device->Vulkan_UnloadLibrary = XBOX_Vulkan_UnloadLibrary;
    // device->Vulkan_GetInstanceExtensions = XBOX_Vulkan_GetInstanceExtensions;
    // device->Vulkan_CreateSurface = XBOX_Vulkan_CreateSurface;
    // device->Vulkan_GetDrawableSize = XBOX_Vulkan_GetDrawableSize;
#endif

    /* Metal support */
#ifdef SDL_VIDEO_METAL
    // device->Metal_CreateView = XBOX_Metal_CreateView;
    // device->Metal_DestroyView = XBOX_Metal_DestroyView;
    // device->Metal_GetLayer = XBOX_Metal_GetLayer;
    // device->Metal_GetDrawableSize = XBOX_Metal_GetDrawableSize;
#endif

    /* Event manager functions */
    // device->WaitEventTimeout = XBOX_WaitEventTimeout;
    // device->SendWakeupEvent = XBOX_SendWakeupEvent;
    device->PumpEvents = XBOX_PumpEvents;

    /* Screensaver */
    // device->SuspendScreenSaver = XBOX_SuspendScreenSaver;

    /* Text input */
    // device->StartTextInput = XBOX_StartTextInput;
    // device->StopTextInput = XBOX_StopTextInput;
    // device->SetTextInputRect = XBOX_SetTextInputRect;
    // device->ClearComposition = XBOX_ClearComposition;
    // device->IsTextInputShown = XBOX_IsTextInputShown;

    /* Screen keyboard */
    // device->HasScreenKeyboardSupport = XBOX_HasScreenKeyboardSupport;
    // device->ShowScreenKeyboard = XBOX_ShowScreenKeyboard;
    // device->HideScreenKeyboard = XBOX_HideScreenKeyboard;
    // device->IsScreenKeyboardShown = XBOX_IsScreenKeyboardShown;

    /* Clipboard */
    // device->SetClipboardText = XBOX_SetClipboardText;
    // device->GetClipboardText = XBOX_GetClipboardText;
    // device->HasClipboardText = XBOX_HasClipboardText;
    // device->SetPrimarySelectionText = XBOX_SetPrimarySelectionText;
    // device->GetPrimarySelectionText = XBOX_GetPrimarySelectionText;
    // device->HasPrimarySelectionText = XBOX_HasPrimarySelectionText;

    /* Hit-testing */
    // device->SetWindowHitTest = XBOX_SetWindowHitTest;

    /* Tell window that app enabled drag'n'drop events */
    // device->AcceptDragAndDrop = XBOX_AcceptDragAndDrop;

    device->DeleteDevice = XBOX_DeleteDevice;

    return SDL_TRUE;
}

const VideoBootStrap XBOX_bootstrap = {
    "xbox", XBOX_CreateDevice
};

int XBOX_VideoInit(_THIS)
{
    int result;
    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;
    VIDEO_MODE vm = XVideoGetMode();

    /* Select display mode based on Xbox video mode */
    current_mode.format = pixelFormatSelector(vm.bpp);
    current_mode.w = vm.width;
    current_mode.h = vm.height;
    current_mode.refresh_rate = vm.refresh;
    current_mode.driverdata = NULL;

    SDL_zero(display);
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;

    result = SDL_AddVideoDisplay(&display, SDL_FALSE);
    if (result < 0) {
        SDL_free(display.display_modes);
    }

    /* We're done! */
    return result;
}

static int
XBOX_SetDisplayMode(SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    return 0;
}

void
XBOX_VideoQuit(_THIS)
{
}

#endif /* SDL_VIDEO_DRIVER_XBOX */

/* vi: set ts=4 sw=4 expandtab: */
