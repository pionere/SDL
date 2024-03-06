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

#ifdef SDL_VIDEO_DRIVER_PSP

/* SDL internals */
#include "../SDL_sysvideo.h"
#include "SDL_version.h"
#include "SDL_syswm.h"
#include "SDL_loadso.h"
#include "SDL_events.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"


/* PSP declarations */
#include "SDL_pspvideo.h"
#include "SDL_pspevents_c.h"
#include "SDL_pspgl_c.h"

#ifdef SDL_VIDEO_VULKAN
#error "Vulkan is configured, but not implemented for psp."
#endif
#if defined(SDL_VIDEO_OPENGL_ANY) && !defined(SDL_VIDEO_OPENGL)
#error "OpenGL is configured, but not the implemented (GL) for psp."
#endif

#ifdef SDL_VIDEO_OPENGL
/* Instance */
Psp_VideoData pspVideoData; 
#endif

static void PSP_DeleteDevice(_THIS)
{
#ifdef SDL_VIDEO_OPENGL
    SDL_zero(pspVideoData);
#endif
    SDL_free(_this);
}

static SDL_VideoDevice *PSP_CreateDevice()
{
    SDL_VideoDevice *device;

    /* Initialize SDL_VideoDevice structure */
    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return NULL;
    }

    /* Set the function pointers */
    /* Initialization/Query functions */
    device->VideoInit = PSP_VideoInit;
    device->VideoQuit = PSP_VideoQuit;
    // device->GetDisplayBounds = PSP_GetDisplayBounds;
    // device->GetDisplayUsableBounds = PSP_GetDisplayUsableBounds;
    // device->GetDisplayDPI = PSP_GetDisplayDPI;
    device->SetDisplayMode = PSP_SetDisplayMode;

    /* Window functions */
    device->CreateSDLWindow = PSP_CreateSDLWindow;
    // device->CreateSDLWindowFrom = PSP_CreateSDLWindowFrom;
    // device->SetWindowTitle = PSP_SetWindowTitle;
    // device->SetWindowIcon = PSP_SetWindowIcon;
    // device->SetWindowPosition = PSP_SetWindowPosition;
    // device->SetWindowSize = PSP_SetWindowSize;
    // device->SetWindowMinimumSize = PSP_SetWindowMinimumSize;
    // device->SetWindowMaximumSize = PSP_SetWindowMaximumSize;
    // device->GetWindowBordersSize = PSP_GetWindowBordersSize;
    // device->GetWindowSizeInPixels = PSP_GetWindowSizeInPixels;
    // device->SetWindowOpacity = PSP_SetWindowOpacity;
    // device->SetWindowModalFor = PSP_SetWindowModalFor;
    // device->SetWindowInputFocus = PSP_SetWindowInputFocus;
    // device->ShowWindow = PSP_ShowWindow;
    // device->HideWindow = PSP_HideWindow;
    // device->RaiseWindow = PSP_RaiseWindow;
    // device->MaximizeWindow = PSP_MaximizeWindow;
    device->MinimizeWindow = PSP_MinimizeWindow;
    // device->RestoreWindow = PSP_RestoreWindow;
    // device->SetWindowBordered = PSP_SetWindowBordered;
    // device->SetWindowResizable = PSP_SetWindowResizable;
    // device->SetWindowAlwaysOnTop = PSP_SetWindowAlwaysOnTop;
    // device->SetWindowFullscreen = PSP_SetWindowFullscreen;
    // device->SetWindowGammaRamp = PSP_SetWindowGammaRamp;
    // device->GetWindowGammaRamp = PSP_GetWindowGammaRamp;
    // device->GetWindowICCProfile = PSP_GetWindowICCProfile;
    // device->GetWindowDisplayIndex = PSP_GetWindowDisplayIndex;
    // device->SetWindowMouseRect = PSP_SetWindowMouseRect;
    // device->SetWindowMouseGrab = PSP_SetWindowMouseGrab;
    // device->SetWindowKeyboardGrab = PSP_SetWindowKeyboardGrab;
    device->DestroyWindow = PSP_DestroyWindow;
    // device->CreateWindowFramebuffer = PSP_CreateWindowFramebuffer;
    // device->UpdateWindowFramebuffer = PSP_UpdateWindowFramebuffer;
    // device->DestroyWindowFramebuffer = PSP_DestroyWindowFramebuffer;
    // device->OnWindowEnter = PSP_OnWindowEnter;
    // device->FlashWindow = PSP_FlashWindow;
    /* Shaped-window functions */
    // device->CreateShaper = PSP_CreateShaper;
    // device->SetWindowShape = PSP_SetWindowShape;
#if 0
    /* Get some platform dependent window information */
    device->GetWindowWMInfo = PSP_GetWindowWMInfo;
#endif


    /* OpenGL support */
#ifdef SDL_VIDEO_OPENGL
    device->GL_LoadLibrary = PSP_GL_LoadLibrary;
    device->GL_GetProcAddress = PSP_GL_GetProcAddress;
    device->GL_UnloadLibrary = PSP_GL_UnloadLibrary;
    device->GL_CreateContext = PSP_GL_CreateContext;
    device->GL_MakeCurrent = PSP_GL_MakeCurrent;
    // device->GL_GetDrawableSize = PSP_GL_GetDrawableSize;
    device->GL_SetSwapInterval = PSP_GL_SetSwapInterval;
    device->GL_GetSwapInterval = PSP_GL_GetSwapInterval;
    device->GL_SwapWindow = PSP_GL_SwapWindow;
    device->GL_DeleteContext = PSP_GL_DeleteContext;
    // device->GL_DefaultProfileConfig = PSP_GL_DefaultProfileConfig;
#endif

    /* Vulkan support */
#ifdef SDL_VIDEO_VULKAN
    // device->Vulkan_LoadLibrary = PSP_Vulkan_LoadLibrary;
    // device->Vulkan_UnloadLibrary = PSP_Vulkan_UnloadLibrary;
    // device->Vulkan_GetInstanceExtensions = PSP_Vulkan_GetInstanceExtensions;
    // device->Vulkan_CreateSurface = PSP_Vulkan_CreateSurface;
    // device->Vulkan_GetDrawableSize = PSP_Vulkan_GetDrawableSize;
#endif

    /* Metal support */
#ifdef SDL_VIDEO_METAL
    // device->Metal_CreateView = PSP_Metal_CreateView;
    // device->Metal_DestroyView = PSP_Metal_DestroyView;
    // device->Metal_GetLayer = PSP_Metal_GetLayer;
    // device->Metal_GetDrawableSize = PSP_Metal_GetDrawableSize;
#endif

    /* Event manager functions */
    // device->WaitEventTimeout = PSP_WaitEventTimeout;
    // device->SendWakeupEvent = PSP_SendWakeupEvent;
    device->PumpEvents = PSP_PumpEvents;

    /* Screensaver */
    // device->SuspendScreenSaver = PSP_SuspendScreenSaver;

    /* Text input */
    // device->StartTextInput = PSP_StartTextInput;
    // device->StopTextInput = PSP_StopTextInput;
    // device->SetTextInputRect = PSP_SetTextInputRect;
    // device->ClearComposition = PSP_ClearComposition;
    // device->IsTextInputShown = PSP_IsTextInputShown;

    /* Screen keyboard */
    device->HasScreenKeyboardSupport = PSP_HasScreenKeyboardSupport;
    device->ShowScreenKeyboard = PSP_ShowScreenKeyboard;
    device->HideScreenKeyboard = PSP_HideScreenKeyboard;
    device->IsScreenKeyboardShown = PSP_IsScreenKeyboardShown;

    /* Clipboard */
    // device->SetClipboardText = PSP_SetClipboardText;
    // device->GetClipboardText = PSP_GetClipboardText;
    // device->HasClipboardText = PSP_HasClipboardText;
    // device->SetPrimarySelectionText = PSP_SetPrimarySelectionText;
    // device->GetPrimarySelectionText = PSP_GetPrimarySelectionText;
    // device->HasPrimarySelectionText = PSP_HasPrimarySelectionText;

    /* Hit-testing */
    // device->SetWindowHitTest = PSP_SetWindowHitTest;

    /* Tell window that app enabled drag'n'drop events */
    // device->AcceptDragAndDrop = PSP_AcceptDragAndDrop;

    device->free = PSP_DeleteDevice;

    return device;
}
/* "PSP Video Driver" */
const VideoBootStrap PSP_bootstrap = {
    "PSP", PSP_CreateDevice
};

/*****************************************************************************/
/* SDL Video and Display initialization/handling functions                   */
/*****************************************************************************/
int PSP_VideoInit(_THIS)
{
    int result;
    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;

    if (PSP_EventInit() < 0) {
        return -1;  /* error string would already be set */
    }

    /* 32 bpp for default */
    current_mode.format = SDL_PIXELFORMAT_ABGR8888;
    current_mode.w = 480;
    current_mode.h = 272;
    current_mode.refresh_rate = 60;
    current_mode.driverdata = NULL;

    SDL_zero(display);
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;
    // display.driverdata = NULL;

    SDL_AddDisplayMode(&display, &current_mode);
    /* 16 bpp secondary mode */
    current_mode.format = SDL_PIXELFORMAT_BGR565;
    // display.desktop_mode = current_mode;
    // display.current_mode = current_mode;
    SDL_AddDisplayMode(&display, &current_mode);
    result = SDL_AddVideoDisplay(&display, SDL_FALSE);
    if (result < 0) {
        SDL_free(display.display_modes);
    }

    return result;
}

void PSP_VideoQuit(_THIS)
{
    PSP_EventQuit();
}

int PSP_SetDisplayMode(SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    return 0;
}
#define EGLCHK(stmt)                           \
    do {                                       \
        EGLint err;                            \
                                               \
        stmt;                                  \
        err = eglGetError();                   \
        if (err != EGL_SUCCESS) {              \
            SDL_SetError("EGL error %d", err); \
            return 0;                          \
        }                                      \
    } while (0)

int PSP_CreateSDLWindow(_THIS, SDL_Window *window)
{
    if (window->flags & SDL_WINDOW_OPENGL) {
#ifdef SDL_VIDEO_OPENGL
        SDL_WindowData *data;

        /* Allocate the window data */
        data = (SDL_WindowData *)SDL_calloc(1, sizeof(*data));
        if (!data) {
            return SDL_OutOfMemory();
        }
        // data->egl_surface = PSP_GL_CreateSurface(_this, (NativeWindowType)windowdata->hwnd);

        // if (data->egl_surface == EGL_NO_SURFACE) {
        //    return -1;
        // }
        window->driverdata = data;
#else
        return SDL_SetError("Could not create GL window (GL support not configured)");
#endif
    }

    SDL_SetKeyboardFocus(window);

    /* Window has been successfully created */
    return 0;
}

/*int PSP_CreateSDLWindowFrom(_THIS, SDL_Window *window, const void *data)
{
    return SDL_Unsupported();
}

void PSP_SetWindowTitle(SDL_Window *window)
{
}
void PSP_SetWindowIcon(SDL_Window *window, SDL_Surface *icon)
{
}
void PSP_SetWindowPosition(SDL_Window *window)
{
}
void PSP_SetWindowSize(SDL_Window *window)
{
}
void PSP_ShowWindow(SDL_Window *window)
{
}
void PSP_HideWindow(SDL_Window *window)
{
}
void PSP_RaiseWindow(_THIS, SDL_Window *window)
{
}
void PSP_MaximizeWindow(SDL_Window *window)
{
}*/
void PSP_MinimizeWindow(SDL_Window *window)
{
}
/*void PSP_RestoreWindow(SDL_Window *window)
{
}*/
void PSP_DestroyWindow(SDL_Window *window)
{
#ifdef SDL_VIDEO_OPENGL
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    if (data) {
        PSP_GL_DestroySurface(data->egl_surface);
        SDL_free(data);
        window->driverdata = NULL;
    }
#endif
}

/*****************************************************************************/
/* SDL Window Manager function                                               */
/*****************************************************************************/
#if 0
SDL_bool PSP_GetWindowWMInfo(SDL_Window * window, struct SDL_SysWMinfo *info)
{
    if (info->version.major <= SDL_MAJOR_VERSION) {
        return SDL_TRUE;
    } else {
        SDL_SetError("Application not compiled with SDL %d",
                     SDL_MAJOR_VERSION);
        return SDL_FALSE;
    }

    /* Failed to get window manager information */
    return SDL_FALSE;
}
#endif


/* TO Write Me */
SDL_bool PSP_HasScreenKeyboardSupport()
{
    return SDL_FALSE;
}
void PSP_ShowScreenKeyboard(SDL_Window *window)
{
}
void PSP_HideScreenKeyboard(SDL_Window *window)
{
}
SDL_bool PSP_IsScreenKeyboardShown(SDL_Window *window)
{
    return SDL_FALSE;
}

#endif /* SDL_VIDEO_DRIVER_PSP */

/* vi: set ts=4 sw=4 expandtab: */
