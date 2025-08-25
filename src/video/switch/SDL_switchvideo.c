/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_SWITCH

#include "../SDL_sysvideo.h"
#include "../../render/SDL_sysrender.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_windowevents_c.h"

#include "SDL_switchvideo.h"
#include "SDL_switchopengles.h"
#include "SDL_switchtouch.h"
#include "SDL_switchkeyboard.h"
#include "SDL_switchmouse_c.h"
#include "SDL_switchswkb.h"

/* Currently only one window */
static SDL_Window *switch_window = NULL;
static AppletOperationMode operationMode;

static void
SWITCH_DeleteDevice(SDL_VideoDevice *device)
{
}

static SDL_bool
SWITCH_CreateDevice(SDL_VideoDevice *device)
{
    /* Set the function pointers */
    /* Initialization/Query functions */
    device->VideoInit = SWITCH_VideoInit;
    device->VideoQuit = SWITCH_VideoQuit;
    // device->GetDisplayBounds = SWITCH_GetDisplayBounds;
    // device->GetDisplayUsableBounds = SWITCH_GetDisplayUsableBounds;
    // device->GetDisplayDPI = SWITCH_GetDisplayDPI;
    device->SetDisplayMode = SWITCH_SetDisplayMode;

    /* Window functions */
    device->CreateSDLWindow = SWITCH_CreateSDLWindow;
    // device->CreateSDLWindowFrom = SWITCH_CreateSDLWindowFrom;
    // device->SetWindowTitle = SWITCH_SetWindowTitle;
    // device->SetWindowIcon = SWITCH_SetWindowIcon;
    // device->SetWindowPosition = SWITCH_SetWindowPosition;
    device->SetWindowSize = SWITCH_SetWindowSize;
    // device->SetWindowMinimumSize = SWITCH_SetWindowMinimumSize;
    // device->SetWindowMaximumSize = SWITCH_SetWindowMaximumSize;
    // device->GetWindowBordersSize = SWITCH_GetWindowBordersSize;
    // device->GetWindowSizeInPixels = SWITCH_GetWindowSizeInPixels;
    // device->SetWindowOpacity = SWITCH_SetWindowOpacity;
    // device->SetWindowModalFor = SWITCH_SetWindowModalFor;
    // device->SetWindowInputFocus = SWITCH_SetWindowInputFocus;
    // device->ShowWindow = SWITCH_ShowWindow;
    // device->HideWindow = SWITCH_HideWindow;
    // device->RaiseWindow = SWITCH_RaiseWindow;
    // device->MaximizeWindow = SWITCH_MaximizeWindow;
    // device->MinimizeWindow = SWITCH_MinimizeWindow;
    // device->RestoreWindow = SWITCH_RestoreWindow;
    // device->SetWindowBordered = SWITCH_SetWindowBordered;
    // device->SetWindowResizable = SWITCH_SetWindowResizable;
    // device->SetWindowAlwaysOnTop = SWITCH_SetWindowAlwaysOnTop;
    // device->SetWindowFullscreen = SWITCH_SetWindowFullscreen;
    // device->SetWindowGammaRamp = SWITCH_SetWindowGammaRamp;
    // device->GetWindowGammaRamp = SWITCH_GetWindowGammaRamp;
    // device->GetWindowICCProfile = SWITCH_GetWindowICCProfile;
    // device->GetWindowDisplayIndex = SWITCH_GetWindowDisplayIndex;
    // device->SetWindowMouseRect = SWITCH_SetWindowMouseRect;
    // device->SetWindowMouseGrab = SWITCH_SetWindowMouseGrab;
    // device->SetWindowKeyboardGrab = SWITCH_SetWindowKeyboardGrab;
    device->DestroyWindow = SWITCH_DestroyWindow;
    // * Framebuffer disabled, causes issues on high-framerate updates. SDL still emulates this.
    // device->CreateWindowFramebuffer = SWITCH_CreateWindowFramebuffer;
    // device->UpdateWindowFramebuffer = SWITCH_UpdateWindowFramebuffer;
    // device->DestroyWindowFramebuffer = SWITCH_DestroyWindowFramebuffer;
    // device->OnWindowEnter = SWITCH_OnWindowEnter;
    // device->FlashWindow = SWITCH_FlashWindow;
    /* Shaped-window functions */
    // device->CreateShaper = SWITCH_CreateShaper;
    // device->SetWindowShape = SWITCH_SetWindowShape;
    /* Get some platform dependent window information */
    // device->GetWindowWMInfo = SWITCH_GetWindowWMInfo;

    /* OpenGL support */
#ifdef SDL_VIDEO_OPENGL_EGL
    SWITCH_GLES_InitDevice(device);
#endif

    /* Vulkan support */
#ifdef SDL_VIDEO_VULKAN
    // device->Vulkan_LoadLibrary = SWITCH_Vulkan_LoadLibrary;
    // device->Vulkan_UnloadLibrary = SWITCH_Vulkan_UnloadLibrary;
    // device->Vulkan_GetInstanceExtensions = SWITCH_Vulkan_GetInstanceExtensions;
    // device->Vulkan_CreateSurface = SWITCH_Vulkan_CreateSurface;
    // device->Vulkan_GetDrawableSize = SWITCH_Vulkan_GetDrawableSize;
#endif

    /* Metal support */
#ifdef SDL_VIDEO_METAL
    // device->Metal_CreateView = SWITCH_Metal_CreateView;
    // device->Metal_DestroyView = SWITCH_Metal_DestroyView;
    // device->Metal_GetLayer = SWITCH_Metal_GetLayer;
    // device->Metal_GetDrawableSize = SWITCH_Metal_GetDrawableSize;
#endif

    /* Event manager functions */
    // device->WaitEventTimeout = SWITCH_WaitEventTimeout;
    // device->SendWakeupEvent = SWITCH_SendWakeupEvent;
    device->PumpEvents = SWITCH_PumpEvents;

    /* Screensaver */
    // device->SuspendScreenSaver = SWITCH_SuspendScreenSaver;

    /* Text input */
    device->StartTextInput = SWITCH_StartTextInput;
    device->StopTextInput = SWITCH_StopTextInput;
    // device->SetTextInputRect = SWITCH_SetTextInputRect;
    // device->ClearComposition = SWITCH_ClearComposition;
    // device->IsTextInputShown = SWITCH_IsTextInputShown;

    /* Screen keyboard */
    device->HasScreenKeyboardSupport = SWITCH_HasScreenKeyboardSupport;
    // device->ShowScreenKeyboard = SWITCH_ShowScreenKeyboard;
    // device->HideScreenKeyboard = SWITCH_HideScreenKeyboard;
    device->IsScreenKeyboardShown = SWITCH_IsScreenKeyboardShown;

    /* Clipboard */
    // device->SetClipboardText = SWITCH_SetClipboardText;
    // device->GetClipboardText = SWITCH_GetClipboardText;
    // device->HasClipboardText = SWITCH_HasClipboardText;
    // device->SetPrimarySelectionText = SWITCH_SetPrimarySelectionText;
    // device->GetPrimarySelectionText = SWITCH_GetPrimarySelectionText;
    // device->HasPrimarySelectionText = SWITCH_HasPrimarySelectionText;

    /* Hit-testing */
    // device->SetWindowHitTest = SWITCH_SetWindowHitTest;

    /* Tell window that app enabled drag'n'drop events */
    // device->AcceptDragAndDrop = SWITCH_AcceptDragAndDrop;

    device->DeleteDevice = SWITCH_DeleteDevice;

    return SDL_TRUE;
}

const VideoBootStrap SWITCH_bootstrap = {
    "Switch",
    SWITCH_CreateDevice
};

/*****************************************************************************/
/* SDL Video and Display initialization/handling functions                   */
/*****************************************************************************/
int
SWITCH_VideoInit(_THIS)
{
    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;
    int result;

    current_mode.w = 1920;
    current_mode.h = 1080;
    current_mode.refresh_rate = 60;
    current_mode.format = SDL_PIXELFORMAT_RGBA8888;
    current_mode.driverdata = NULL;

    SDL_zero(display);
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;
    // display.driverdata = NULL;

    SDL_AddDisplayMode(&display, &current_mode);
    SWITCH_GetDisplayModes(&display);
    result = SDL_AddVideoDisplay(&display, SDL_FALSE);
    if (result < 0) {
        SDL_free(display.display_modes);
    }

    // init psm service
    psmInitialize();
    // init touch
    SWITCH_InitTouch();
    // init keyboard
    SWITCH_InitKeyboard();
    // init mouse
    SWITCH_InitMouse();
    // init software keyboard
    SWITCH_InitSwkb();

    return result;
}

void
SWITCH_VideoQuit(_THIS)
{
#ifdef SDL_VIDEO_OPENGL_EGL
    // this should not be needed if user code is right (SDL_GL_LoadLibrary/SDL_GL_UnloadLibrary calls match)
    // this (user) error doesn't have the same effect on switch thought, as the driver needs to be unloaded (crash)
    if(_this->gl_config.driver_loaded > 0) {
        SWITCH_GLES_UnloadLibrary(_this);
        _this->gl_config.driver_loaded = 0;
    }
#endif
    // exit touch
    SWITCH_QuitTouch();
    // exit keyboard
    SWITCH_QuitKeyboard();
    // exit mouse
    SWITCH_QuitMouse();
    // exit software keyboard
    SWITCH_QuitSwkb();
    // exit psm service
    psmExit();
}

void
SWITCH_GetDisplayModes(SDL_VideoDisplay *display)
{
    SDL_DisplayMode mode;

    // 1920x1080 RGBA8888, default mode
    // SDL_AddDisplayMode(display, &display->current_mode);

    // 1280x720 RGBA8888
    mode.w = 1280;
    mode.h = 720;
    mode.refresh_rate = 60;
    mode.format = SDL_PIXELFORMAT_RGBA8888;
    mode.driverdata = NULL;

    SDL_AddDisplayMode(display, &mode);
}

#ifdef SDL_VIDEO_OPENGL_EGL
static void SWITCH_setEglSurfaceSize(int w, int h)
{
    if (switch_window != NULL) {
        SDL_WindowData *data = (SDL_WindowData *) switch_window->driverdata;
        SDL_assert(data != NULL);
        if (data->egl_surface != EGL_NO_SURFACE) {
            SDL_VideoDevice *_this;
            NWindow *nWindow = nwindowGetDefault();
            SDL_GLContext ctx = SDL_GL_GetCurrentContext();
            SDL_EGL_MakeCurrent(NULL, NULL);
            SDL_EGL_DestroySurface(data->egl_surface);
            nwindowSetDimensions(nWindow, w, h);
            _this = SDL_GetVideoDevice();
            data->egl_surface = SDL_EGL_CreateSurface(_this, nWindow);
            SDL_EGL_MakeCurrent(data->egl_surface, ctx);
        }
    }
}
#endif

int
SWITCH_SetDisplayMode(SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
#ifdef SDL_VIDEO_OPENGL_EGL
    SWITCH_setEglSurfaceSize(mode->w, mode->h);
#endif
    return 0;
}

int
SWITCH_CreateSDLWindow(_THIS, SDL_Window *window)
{
    Result rc;
#ifdef SDL_VIDEO_OPENGL_EGL
    SDL_WindowData *window_data = NULL;
#endif
    NWindow *nWindow = NULL;

    if (switch_window != NULL) {
        return SDL_SetError("Switch only supports one window");
    }

    nWindow = nwindowGetDefault();

    rc = nwindowSetDimensions(nWindow, window->wrect.w, window->wrect.h);
    if (R_FAILED(rc)) {
        return SDL_SetError("Could not set NWindow dimensions: 0x%x", rc);
    }
#ifdef SDL_VIDEO_OPENGL_EGL
    window_data = (SDL_WindowData *) SDL_calloc(1, sizeof(SDL_WindowData));
    if (window_data == NULL) {
        return SDL_OutOfMemory();
    }

    window_data->egl_surface = SDL_EGL_CreateSurface(_this, nWindow);
    if (window_data->egl_surface == EGL_NO_SURFACE) {
        return -1;
    }
    /* Setup driver data for this window */
    window->driverdata = window_data;
#endif
    switch_window = window;

    /* starting operation mode */
    operationMode = appletGetOperationMode();

    /* One window, it always has focus */
    SDL_SetMouseFocus(window);
    SDL_SetKeyboardFocus(window);

    /* Window has been successfully created */
    return 0;
}

void
SWITCH_DestroyWindow(SDL_Window *window)
{
    if (window == switch_window) {
#ifdef SDL_VIDEO_OPENGL_EGL
        SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
        if (data != NULL) {
            if (data->egl_surface != EGL_NO_SURFACE) {
                SDL_EGL_MakeCurrent(NULL, NULL);
                SDL_EGL_DestroySurface(data->egl_surface);
            }
            if(window->driverdata != NULL) {
                SDL_free(window->driverdata);
                window->driverdata = NULL;
            }
        }
#endif
        switch_window = NULL;
    }
}

void
SWITCH_SetWindowTitle(SDL_Window *window)
{
}
void
SWITCH_SetWindowIcon(SDL_Window *window, SDL_Surface *icon)
{
}
void
SWITCH_SetWindowPosition(SDL_Window *window)
{
}
void
SWITCH_SetWindowSize(SDL_Window *window)
{
#ifdef SDL_VIDEO_OPENGL_EGL
    SDL_assert(window == switch_window);
    SWITCH_setEglSurfaceSize(switch_window->wrect.w, switch_window->wrect.h);
#endif
}
void
SWITCH_ShowWindow(SDL_Window *window)
{
}
void
SWITCH_HideWindow(SDL_Window *window)
{
}
void
SWITCH_RaiseWindow(SDL_Window *window)
{
}
void
SWITCH_MaximizeWindow(SDL_Window *window)
{
}
void
SWITCH_MinimizeWindow(SDL_Window *window)
{
}
void
SWITCH_RestoreWindow(SDL_Window *window)
{
}

void
SWITCH_PumpEvents()
{
    AppletOperationMode om;

    if (!appletMainLoop()) {
        SDL_Event ev;
        ev.type = SDL_QUIT;
        SDL_PushEvent(&ev);
        return;
    }

    // we don't want other inputs overlapping with software keyboard
    if(!SDL_IsTextInputActive()) {
        SWITCH_PollTouch();
        SWITCH_PollKeyboard();
        SWITCH_PollMouse();
    }
    SWITCH_PollSwkb();

    // handle docked / un-docked modes
    // note that SDL_WINDOW_RESIZABLE is only possible in windowed mode,
    // so we don't care about current fullscreen/windowed status
    if(switch_window != NULL && switch_window->flags & SDL_WINDOW_RESIZABLE) {
        om = appletGetOperationMode();
        if(om != operationMode) {
            operationMode = om;
            if(operationMode == AppletOperationMode_Handheld) {
                SDL_SetWindowSize(switch_window, 1280, 720);
            } else {
                SDL_SetWindowSize(switch_window, 1920, 1080);
            }
        }
    }
}

#endif /* SDL_VIDEO_DRIVER_SWITCH */