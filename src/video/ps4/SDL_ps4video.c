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

#ifdef SDL_VIDEO_DRIVER_PS4

#include <stdbool.h>
#include <orbis/libkernel.h>
#include <orbis/Pigletv2VSH.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>
#include <orbis/UserService.h>

#include "../SDL_sysvideo.h"
#include "../../render/SDL_sysrender.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_windowevents_c.h"
#include "SDL_timer.h"
#include "SDL_hints.h"

#include "SDL_ps4video.h"
#include "SDL_ps4opengles.h"
#include "SDL_ps4piglet.h"

#ifdef SDL_VIDEO_VULKAN
#error "Vulkan is configured, but not implemented for PS4."
#endif
#ifdef SDL_VIDEO_METAL
#error "Metal is configured, but not implemented for PS4."
#endif
#if defined(SDL_VIDEO_OPENGL_ANY) && !defined(SDL_VIDEO_OPENGL_EGL)
#error "OpenGL is configured, but not the implemented (OSMESA) for PS5."
#endif

/* Only one window supported */
static SDL_Window *ps4_window = NULL;
#ifdef SDL_VIDEO_OPENGL_EGL
static OrbisPglWindow ps4_egl_window;
#endif
static bool ps4_init_done = false;
#ifndef SDL_LOGGING_DISABLED
char log_buffer[1024];

static void *
PS4_logCb(void *userdata, int category, SDL_LogPriority priority, const char *message) {
    snprintf(log_buffer, 1023, "<SDL2> %s\n", message);
    sceKernelDebugOutText(0, log_buffer);
    return NULL;
}
#endif

int
PS4_LoadModules() {
    uint32_t ret;

    if (ps4_init_done) {
        return 0;
    }

    // load common modules
    ret = sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE);
    if (ret != 0) {
        return SDL_SetError("PS4_LoadModules: load module failed: SYSTEM_SERVICE (0x%08x)", ret);
    }

    // load user module
    ret = sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_USER_SERVICE);
    if (ret != 0) {
        return SDL_SetError("PS4_LoadModules: load module failed: USER_SERVICE (0x%08x)", ret);
    }

    // load pad module
    ret = sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_PAD);
    if (ret != 0) {
        return SDL_SetError("PS4_LoadModules: load module failed: PAD (0x%08x)", ret);
    }

    // load audio module
    ret = sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_AUDIOOUT);
    if (ret != 0) {
        return SDL_SetError("PS4_LoadModules: load module failed: AUDIOOUT (0x%08x)", ret);
    }

    // initialize user service (used for pad and audio drivers)
    OrbisUserServiceInitializeParams param;
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    ret = sceUserServiceInitialize(&param);
    if (ret != 0) {
        return SDL_SetError("PS4_LoadModules: sceUserServiceInitialize failed (0x%08x)", ret);
    }

    // hide splash screen (is this mandatory ?)
    sceSystemServiceHideSplashScreen();

    ps4_init_done = true;
    return 0;
}

static void
PS4_DeleteDevice(SDL_VideoDevice *device) {
    LOG_DEBUG_PS4_VIDEO("PS4_Destroy\n");
#ifdef SDL_VIDEO_OPENGL_EGL
    PS4_PigletExit();
#endif
    LOG_DEBUG_PS4_VIDEO("PS4_Destroy done\n");
}

static SDL_bool
PS4_CreateDevice(SDL_VideoDevice *device) {
    LOG_DEBUG_PS4_VIDEO("PS4_CreateDevice\n");
#ifndef SDL_LOGGING_DISABLED
    // log to kernel
    SDL_LogSetOutputFunction((SDL_LogOutputFunction) &PS4_logCb, NULL);
#endif
    // initialize modules if not already done
    if (PS4_LoadModules() != 0) {
        return SDL_FALSE;
    }
#ifdef SDL_VIDEO_OPENGL_EGL
    // load piglet
    if (PS4_PigletInit() != 0) {
        return SDL_FALSE;
    }
#endif
    /* Setup amount of available displays */
    device->num_displays = 0;

    /* Set the function pointers */
    /* Initialization/Query functions */
    device->VideoInit = PS4_VideoInit;
    device->VideoQuit = PS4_VideoQuit;
    // device->GetDisplayBounds = PS4_GetDisplayBounds;
    // device->GetDisplayUsableBounds = PS4_GetDisplayUsableBounds;
    // device->GetDisplayDPI = PS4_GetDisplayDPI;
    device->SetDisplayMode = PS4_SetDisplayMode;

    /* Window functions */
    device->CreateSDLWindow = PS4_CreateSDLWindow;
    // device->CreateSDLWindowFrom = PS4_CreateSDLWindowFrom;
    device->SetWindowTitle = PS4_SetWindowTitle;
    device->SetWindowIcon = PS4_SetWindowIcon;
    device->SetWindowPosition = PS4_SetWindowPosition;
    device->SetWindowSize = PS4_SetWindowSize;
    // device->SetWindowMinimumSize = PS4_SetWindowMinimumSize;
    // device->SetWindowMaximumSize = PS4_SetWindowMaximumSize;
    // device->GetWindowBordersSize = PS4_GetWindowBordersSize;
    // device->GetWindowSizeInPixels = PS4_GetWindowSizeInPixels;
    // device->SetWindowOpacity = PS4_SetWindowOpacity;
    // device->SetWindowModalFor = PS4_SetWindowModalFor;
    // device->SetWindowInputFocus = PS4_SetWindowInputFocus;
    device->ShowWindow = PS4_ShowWindow;
    device->HideWindow = PS4_HideWindow;
    device->RaiseWindow = PS4_RaiseWindow;
    device->MaximizeWindow = PS4_MaximizeWindow;
    device->MinimizeWindow = PS4_MinimizeWindow;
    device->RestoreWindow = PS4_RestoreWindow;
    // device->SetWindowBordered = PS4_SetWindowBordered;
    // device->SetWindowResizable = PS4_SetWindowResizable;
    // device->SetWindowAlwaysOnTop = PS4_SetWindowAlwaysOnTop;
    // device->SetWindowFullscreen = PS4_SetWindowFullscreen;
    // device->SetWindowGammaRamp = PS4_SetWindowGammaRamp;
    // device->GetWindowGammaRamp = PS4_GetWindowGammaRamp;
    // device->GetWindowICCProfile = PS4_GetWindowICCProfile;
    // device->GetWindowDisplayIndex = PS4_GetWindowDisplayIndex;
    // device->SetWindowMouseRect = PS4_SetWindowMouseRect;
    // device->SetWindowMouseGrab = PS4_SetWindowMouseGrab;
    // device->SetWindowKeyboardGrab = PS4_SetWindowKeyboardGrab;
    device->DestroyWindow = PS4_DestroyWindow;
    // * Framebuffer disabled, causes issues on high-framerate updates. SDL still emulates this.
    // device->CreateWindowFramebuffer = PS4_CreateWindowFramebuffer;
    // device->UpdateWindowFramebuffer = PS4_UpdateWindowFramebuffer;
    // device->DestroyWindowFramebuffer = PS4_DestroyWindowFramebuffer;
    // device->OnWindowEnter = PS4_OnWindowEnter;
    // device->FlashWindow = PS4_FlashWindow;
    /* Shaped-window functions */
    // device->CreateShaper = PS4_CreateShaper;
    // device->SetWindowShape = PS4_SetWindowShape;
    /* Get some platform dependent window information */
    // device->GetWindowWMInfo = PS4_GetWindowWMInfo;

    /* OpenGL support */
#ifdef SDL_VIDEO_OPENGL_EGL
    PS4_GLES_InitDevice(device);
#endif
    device->PumpEvents = PS4_PumpEvents;

    /* Screensaver */
    // device->SuspendScreenSaver = PS4_SuspendScreenSaver;

    /* Text input */
    // device->StartTextInput = PS4_StartTextInput;
    // device->StopTextInput = PS4_StopTextInput;
    // device->SetTextInputRect = PS4_SetTextInputRect;
    // device->ClearComposition = PS4_ClearComposition;
    // device->IsTextInputShown = PS4_IsTextInputShown;

    /* Screen keyboard */
    // device->HasScreenKeyboardSupport = PS4_HasScreenKeyboardSupport;
    // device->ShowScreenKeyboard = PS4_ShowScreenKeyboard;
    // device->HideScreenKeyboard = PS4_HideScreenKeyboard;
    // device->IsScreenKeyboardShown = PS4_IsScreenKeyboardShown;

    /* Clipboard */
    // device->SetClipboardText = PS4_SetClipboardText;
    // device->GetClipboardText = PS4_GetClipboardText;
    // device->HasClipboardText = PS4_HasClipboardText;
    // device->SetPrimarySelectionText = PS4_SetPrimarySelectionText;
    // device->GetPrimarySelectionText = PS4_GetPrimarySelectionText;
    // device->HasPrimarySelectionText = PS4_HasPrimarySelectionText;

    /* Hit-testing */
    // device->SetWindowHitTest = PS4_SetWindowHitTest;

    /* Tell window that app enabled drag'n'drop events */
    // device->AcceptDragAndDrop = PS4_AcceptDragAndDrop;

    device->DeleteDevice = PS4_DeleteDevice;

    return SDL_TRUE;
}

const VideoBootStrap PS4_bootstrap = {
        "PS4",
        PS4_CreateDevice
};

/*****************************************************************************/
/* SDL Video and Display initialization/handling functions                   */
/*****************************************************************************/
int
PS4_VideoInit(_THIS) {
    LOG_DEBUG_PS4_VIDEO("PS4_VideoInit\n");

    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;

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
    PS4_GetDisplayModes(&display);
    SDL_AddVideoDisplay(&display, SDL_FALSE);

    // TODO
    /*
    // init keyboard
    PS4_InitKeyboard();
    // init mouse
    PS4_InitMouse();
    // init software keyboard
    PS4_InitSwkb();
    */

    return 0;
}

void
PS4_VideoQuit(_THIS) {
    LOG_DEBUG_PS4_VIDEO("PS4_VideoQuit\n");

    // TODO
    /*
    // exit keyboard
    PS4_QuitKeyboard();
    // exit mouse
    PS4_QuitMouse();
    // exit software keyboard
    PS4_QuitSwkb();
    */
}

void
PS4_GetDisplayModes(SDL_VideoDisplay *display) {
    LOG_DEBUG_PS4_VIDEO("PS4_GetDisplayModes\n");

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
void PS4_setEglSurfaceSize(int w, int h)
{
    if (ps4_window != NULL) {
        SDL_WindowData *data = (SDL_WindowData *) ps4_window->driverdata;
        SDL_assert(data != NULL);
        if (data->egl_surface != EGL_NO_SURFACE) {
            SDL_VideoDevice *_this;
            SDL_GLContext ctx = SDL_GL_GetCurrentContext();
            SDL_EGL_MakeCurrent(NULL, NULL);
            SDL_EGL_DestroySurface(data->egl_surface);

            ps4_egl_window.uWidth = w;
            ps4_egl_window.uHeight = h;

            _this = SDL_GetVideoDevice();
            data->egl_surface = SDL_EGL_CreateSurface(_this, &ps4_egl_window);
            SDL_EGL_MakeCurrent(data->egl_surface, ctx);
        }
    }
}
#endif

int
PS4_SetDisplayMode(SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    LOG_DEBUG_PS4_VIDEO("PS4_SetDisplayMode\n");
#ifdef SDL_VIDEO_OPENGL_EGL
    PS4_setEglSurfaceSize(mode->w, mode->h);
#endif
    return 0;
}

int
PS4_CreateSDLWindow(_THIS, SDL_Window *window) {
    LOG_DEBUG_PS4_VIDEO("PS4_CreateWindow\n");
#ifdef SDL_VIDEO_OPENGL_EGL
    SDL_WindowData *window_data = NULL;
#endif
    if (ps4_window != NULL) {
        return SDL_SetError("ps4 only supports one window");
    }
#ifdef SDL_VIDEO_OPENGL_EGL
    window_data = (SDL_WindowData *) SDL_calloc(1, sizeof(SDL_WindowData));
    if (window_data == NULL) {
        return SDL_OutOfMemory();
    }

    ps4_egl_window.uWidth = window->wrect.w;
    ps4_egl_window.uHeight = window->wrect.h;

    window_data->egl_surface = SDL_EGL_CreateSurface(_this, &ps4_egl_window);
    if (window_data->egl_surface == EGL_NO_SURFACE) {
        return SDL_SetError("could not create egl window surface");
    }

    /* Setup driver data for this window */
    window->driverdata = window_data;
#endif
    ps4_window = window;

    /* One window, it always has focus */
    SDL_SetMouseFocus(window);
    SDL_SetKeyboardFocus(window);

    /* Window has been successfully created */
    return 0;
}

void
PS4_DestroyWindow(SDL_Window *window) {
    LOG_DEBUG_PS4_VIDEO("PS4_DestroyWindow\n");

    if (window == ps4_window) {
#ifdef SDL_VIDEO_OPENGL_EGL
        SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
        if (data != NULL) {
            if (data->egl_surface != EGL_NO_SURFACE) {
                SDL_EGL_MakeCurrent(NULL, NULL);
                SDL_EGL_DestroySurface(data->egl_surface);
            }
            if (window->driverdata != NULL) {
                SDL_free(window->driverdata);
                window->driverdata = NULL;
            }
        }
#endif
        ps4_window = NULL;
    }
}

void
PS4_SetWindowTitle(SDL_Window *window) {
}

void
PS4_SetWindowIcon(SDL_Window *window, SDL_Surface *icon) {
}

void
PS4_SetWindowPosition(SDL_Window *window) {
}

void
PS4_SetWindowSize(SDL_Window *window) {
    LOG_DEBUG_PS4_VIDEO("PS4_SetWindowSize\n");
#ifdef SDL_VIDEO_OPENGL_EGL
    SDL_assert(window == ps4_window);
    PS4_setEglSurfaceSize(window->wrect.w, window->wrect.h);
#endif
}

void
PS4_ShowWindow(SDL_Window *window) {
}

void
PS4_HideWindow(SDL_Window *window) {
}

void
PS4_RaiseWindow(SDL_Window *window) {
}

void
PS4_MaximizeWindow(SDL_Window *window) {
}

void
PS4_MinimizeWindow(SDL_Window *window) {
}

void
PS4_RestoreWindow(SDL_Window *window) {
}

void
PS4_SetWindowGrab(_THIS, SDL_Window *window, SDL_bool grabbed) {
}

void
PS4_PumpEvents() {

    // TODO
    /*
    // we don't want other inputs overlapping with software keyboard
    if(!SDL_IsTextInputActive()) {
        PS4_PollKeyboard();
        PS4_PollMouse();
    }
    PS4_PollSwkb();
    */
}

#endif /* SDL_VIDEO_DRIVER_PS4 */
