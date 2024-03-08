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

#ifdef SDL_VIDEO_DRIVER_N3DS

#include "../SDL_sysvideo.h"
#include "SDL_n3dsevents_c.h"
#include "SDL_n3dsframebuffer_c.h"
#include "SDL_n3dsswkb.h"
#include "SDL_n3dstouch.h"
#include "SDL_n3dsvideo.h"

#ifdef SDL_VIDEO_VULKAN
#error "Vulkan is configured, but not implemented for n3ds."
#endif
#ifdef SDL_VIDEO_METAL
#error "Metal is configured, but not implemented for n3ds."
#endif
#ifdef SDL_VIDEO_OPENGL_ANY
#error "OpenGL is configured, but not implemented for n3ds."
#endif

SDL_FORCE_INLINE int AddN3DSDisplay(gfxScreen_t screen);

static int N3DS_VideoInit(_THIS);
static void N3DS_VideoQuit(_THIS);
static int N3DS_GetDisplayBounds(SDL_VideoDisplay *display, SDL_Rect *rect);
static int N3DS_CreateSDLWindow(_THIS, SDL_Window *window);
static void N3DS_DestroyWindow(SDL_Window *window);

typedef struct
{
    gfxScreen_t screen;
} DisplayDriverData;

/* N3DS driver bootstrap functions */

static void N3DS_DeleteDevice(_THIS)
{
    SDL_free(_this);
}

static SDL_VideoDevice *N3DS_CreateDevice(void)
{
    SDL_VideoDevice *device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return 0;
    }

    /* Set the function pointers */
    /* Initialization/Query functions */
    device->VideoInit = N3DS_VideoInit;
    device->VideoQuit = N3DS_VideoQuit;
    device->GetDisplayBounds = N3DS_GetDisplayBounds;
    // device->GetDisplayUsableBounds = N3DS_GetDisplayUsableBounds;
    // device->GetDisplayDPI = N3DS_GetDisplayDPI;
    // device->SetDisplayMode = N3DS_SetDisplayMode;

    /* Window functions */
    device->CreateSDLWindow = N3DS_CreateSDLWindow;
    // device->CreateSDLWindowFrom = N3DS_CreateSDLWindowFrom;
    // device->SetWindowTitle = N3DS_SetWindowTitle;
    // device->SetWindowIcon = N3DS_SetWindowIcon;
    // device->SetWindowPosition = N3DS_SetWindowPosition;
    // device->SetWindowSize = N3DS_SetWindowSize;
    // device->SetWindowMinimumSize = N3DS_SetWindowMinimumSize;
    // device->SetWindowMaximumSize = N3DS_SetWindowMaximumSize;
    // device->GetWindowBordersSize = N3DS_GetWindowBordersSize;
    // device->GetWindowSizeInPixels = N3DS_GetWindowSizeInPixels;
    // device->SetWindowOpacity = N3DS_SetWindowOpacity;
    // device->SetWindowModalFor = N3DS_SetWindowModalFor;
    // device->SetWindowInputFocus = N3DS_SetWindowInputFocus;
    // device->ShowWindow = N3DS_ShowWindow;
    // device->HideWindow = N3DS_HideWindow;
    // device->RaiseWindow = N3DS_RaiseWindow;
    // device->MaximizeWindow = N3DS_MaximizeWindow;
    // device->MinimizeWindow = N3DS_MinimizeWindow;
    // device->RestoreWindow = N3DS_RestoreWindow;
    // device->SetWindowBordered = N3DS_SetWindowBordered;
    // device->SetWindowResizable = N3DS_SetWindowResizable;
    // device->SetWindowAlwaysOnTop = N3DS_SetWindowAlwaysOnTop;
    // device->SetWindowFullscreen = N3DS_SetWindowFullscreen;
    // device->SetWindowGammaRamp = N3DS_SetWindowGammaRamp;
    // device->GetWindowGammaRamp = N3DS_GetWindowGammaRamp;
    // device->GetWindowICCProfile = N3DS_GetWindowICCProfile;
    // device->GetWindowDisplayIndex = N3DS_GetWindowDisplayIndex;
    // device->SetWindowMouseRect = N3DS_SetWindowMouseRect;
    // device->SetWindowMouseGrab = N3DS_SetWindowMouseGrab;
    // device->SetWindowKeyboardGrab = N3DS_SetWindowKeyboardGrab;
    device->DestroyWindow = N3DS_DestroyWindow;
    device->CreateWindowFramebuffer = N3DS_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = N3DS_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = N3DS_DestroyWindowFramebuffer;
    // device->OnWindowEnter = N3DS_OnWindowEnter;
    // device->FlashWindow = N3DS_FlashWindow;
    /* Shaped-window functions */
    // device->CreateShaper = N3DS_CreateShaper;
    // device->SetWindowShape = N3DS_SetWindowShape;
    /* Get some platform dependent window information */
    // device->GetWindowWMInfo = N3DS_GetWindowWMInfo;
    /* OpenGL support */
#ifdef SDL_VIDEO_OPENGL_EGL
    // device->GL_LoadLibrary = N3DS_GL_LoadLibrary;
    // device->GL_GetProcAddress = N3DS_GL_GetProcAddress;
    // device->GL_UnloadLibrary = N3DS_GL_UnloadLibrary;
    // device->GL_CreateContext = N3DS_GL_CreateContext;
    // device->GL_MakeCurrent = N3DS_GL_MakeCurrent;
    // device->GL_GetDrawableSize = N3DS_GL_GetDrawableSize;
    // device->GL_SetSwapInterval = N3DS_GL_SetSwapInterval;
    // device->GL_GetSwapInterval = N3DS_GL_GetSwapInterval;
    // device->GL_SwapWindow = N3DS_GL_SwapWindow;
    // device->GL_DeleteContext = N3DS_GL_DeleteContext;
    // device->GL_DefaultProfileConfig = N3DS_GL_DefaultProfileConfig;
#endif
    /* Vulkan support */
#ifdef SDL_VIDEO_VULKAN
    // device->Vulkan_LoadLibrary = N3DS_Vulkan_LoadLibrary;
    // device->Vulkan_UnloadLibrary = N3DS_Vulkan_UnloadLibrary;
    // device->Vulkan_GetInstanceExtensions = N3DS_Vulkan_GetInstanceExtensions;
    // device->Vulkan_CreateSurface = N3DS_Vulkan_CreateSurface;
    // device->Vulkan_GetDrawableSize = N3DS_Vulkan_GetDrawableSize;
#endif
    /* Metal support */
#ifdef SDL_VIDEO_METAL
    // device->Metal_CreateView = N3DS_Metal_CreateView;
    // device->Metal_DestroyView = N3DS_Metal_DestroyView;
    // device->Metal_GetLayer = N3DS_Metal_GetLayer;
    // device->Metal_GetDrawableSize = N3DS_Metal_GetDrawableSize;
#endif
    /* Event manager functions */
    // device->WaitEventTimeout = N3DS_WaitEventTimeout;
    // device->SendWakeupEvent = N3DS_SendWakeupEvent;
    device->PumpEvents = N3DS_PumpEvents;

    /* Screensaver */
    // device->SuspendScreenSaver = N3DS_SuspendScreenSaver;

    /* Text input */
    device->StartTextInput = N3DS_StartTextInput;
    device->StopTextInput = N3DS_StopTextInput;
    // device->SetTextInputRect = N3DS_SetTextInputRect;
    // device->ClearComposition = N3DS_ClearComposition;
    // device->IsTextInputShown = N3DS_IsTextInputShown;

    /* Screen keyboard */
    device->HasScreenKeyboardSupport = N3DS_HasScreenKeyboardSupport;
    // device->ShowScreenKeyboard = N3DS_ShowScreenKeyboard;
    // device->HideScreenKeyboard = N3DS_HideScreenKeyboard;
    // device->IsScreenKeyboardShown = N3DS_IsScreenKeyboardShown;

    /* Clipboard */
    // device->SetClipboardText = N3DS_SetClipboardText;
    // device->GetClipboardText = N3DS_GetClipboardText;
    // device->HasClipboardText = N3DS_HasClipboardText;
    // device->SetPrimarySelectionText = N3DS_SetPrimarySelectionText;
    // device->GetPrimarySelectionText = N3DS_GetPrimarySelectionText;
    // device->HasPrimarySelectionText = N3DS_HasPrimarySelectionText;

    /* Hit-testing */
    // device->SetWindowHitTest = N3DS_SetWindowHitTest;

    /* Tell window that app enabled drag'n'drop events */
    // device->AcceptDragAndDrop = N3DS_AcceptDragAndDrop;

    device->DeleteDevice = N3DS_DeleteDevice;

    return device;
}
// "N3DS Video Driver"
const VideoBootStrap N3DS_bootstrap = { "n3ds", N3DS_CreateDevice };

static int N3DS_VideoInit(_THIS)
{
    int result;
    gfxInit(GSP_RGBA8_OES, GSP_RGBA8_OES, false);
    hidInit();

    result = AddN3DSDisplay(GFX_TOP);
    if (result == 0) {
        result = AddN3DSDisplay(GFX_BOTTOM);
    }

    N3DS_InitTouch();
    N3DS_SwkbInit();

    return result;
}

SDL_FORCE_INLINE int
AddN3DSDisplay(gfxScreen_t screen)
{
    int result;
    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;
    DisplayDriverData *display_driver_data = SDL_calloc(1, sizeof(DisplayDriverData));
    if (!display_driver_data) {
        return SDL_OutOfMemory();
    }

    display_driver_data->screen = screen;

    current_mode.format = FRAMEBUFFER_FORMAT;
    current_mode.w = (screen == GFX_TOP) ? GSP_SCREEN_HEIGHT_TOP : GSP_SCREEN_HEIGHT_BOTTOM;
    current_mode.h = GSP_SCREEN_WIDTH;
    current_mode.refresh_rate = 60;
    current_mode.driverdata = NULL;

    SDL_zero(display);
    display.name = (screen == GFX_TOP) ? "N3DS top screen" : "N3DS bottom screen";
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;
    display.driverdata = display_driver_data;

    SDL_AddDisplayMode(&display, &current_mode);
    result = SDL_AddVideoDisplay(&display, SDL_FALSE);
    if (result < 0) {
        SDL_free(display.display_modes);
        SDL_free(display_driver_data);
    }

    return result;
}

static void N3DS_VideoQuit(_THIS)
{
    N3DS_SwkbQuit();
    N3DS_QuitTouch();

    hidExit();
    gfxExit();
}

static int N3DS_GetDisplayBounds(SDL_VideoDisplay *display, SDL_Rect *rect)
{
    DisplayDriverData *driver_data = (DisplayDriverData *)display->driverdata;

    rect->x = 0;
    rect->y = (driver_data->screen == GFX_TOP) ? 0 : GSP_SCREEN_WIDTH;
    rect->w = display->current_mode.w;
    rect->h = display->current_mode.h;

    return 0;
}

static int N3DS_CreateSDLWindow(_THIS, SDL_Window *window)
{
    DisplayDriverData *display_data;
    SDL_WindowData *window_data = (SDL_WindowData *)SDL_calloc(1, sizeof(SDL_WindowData));
    if (!window_data) {
        return SDL_OutOfMemory();
    }
    display_data = (DisplayDriverData *)SDL_GetDisplayDriverData(window->display_index);
    window_data->screen = display_data->screen;
    window->driverdata = window_data;
    SDL_SetKeyboardFocus(window);
    return 0;
}

static void N3DS_DestroyWindow(SDL_Window *window)
{
    SDL_free(window->driverdata);
    window->driverdata = NULL;
}

#endif /* SDL_VIDEO_DRIVER_N3DS */

/* vi: set sts=4 ts=4 sw=4 expandtab: */
