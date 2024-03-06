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

#ifdef SDL_VIDEO_DRIVER_ANDROID

/* Android SDL video driver implementation */

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "SDL_hints.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_windowevents_c.h"

#include "SDL_androidvideo.h"
#include "SDL_androidgl.h"
#include "SDL_androidclipboard.h"
#include "SDL_androidevents.h"
#include "SDL_androidkeyboard.h"
#include "SDL_androidmouse.h"
#include "SDL_androidtouch.h"
#include "SDL_androidwindow.h"
#include "SDL_androidvulkan.h"

#ifdef SDL_VIDEO_METAL
#error "Metal is configured, but not implemented for android."
#endif
#if defined(SDL_VIDEO_OPENGL_ANY) && !defined(SDL_VIDEO_OPENGL_EGL)
#error "OpenGL is configured, but not the implemented (EGL) for android."
#endif

/* Instance */
Android_VideoData androidVideoData;

/* Initialization/Query functions */
static int Android_VideoInit(_THIS);
static void Android_VideoQuit(_THIS);
int Android_GetDisplayDPI(SDL_VideoDisplay *display, float *ddpi, float *hdpi, float *vdpi);

/* Android driver bootstrap functions */

/* These are filled in with real values in Android_SetScreenResolution on init (before SDL_main()) */
int Android_SurfaceWidth = 0;
int Android_SurfaceHeight = 0;
static int Android_DeviceWidth = 0;
static int Android_DeviceHeight = 0;
static Uint32 Android_ScreenFormat = SDL_PIXELFORMAT_RGB565; /* Default SurfaceView format, in case this is queried before being filled */
static int Android_ScreenRate = 0;
SDL_sem *Android_PauseSem = NULL;
SDL_sem *Android_ResumeSem = NULL;
SDL_mutex *Android_ActivityMutex = NULL;

static void Android_SuspendScreenSaver(_THIS)
{
    Android_JNI_SuspendScreenSaver(_this->suspend_screensaver);
}

static void Android_DeleteDevice(_THIS)
{
    SDL_zero(androidVideoData);
    SDL_free(_this);
}

static SDL_VideoDevice *Android_CreateDevice(void)
{
    SDL_VideoDevice *device;

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return NULL;
    }

    /* Set the function pointers */
    /* Initialization/Query functions */
    device->VideoInit = Android_VideoInit;
    device->VideoQuit = Android_VideoQuit;
    // device->GetDisplayBounds = Android_GetDisplayBounds;
    // device->GetDisplayUsableBounds = Android_GetDisplayUsableBounds;
    device->GetDisplayDPI = Android_GetDisplayDPI;
    // device->SetDisplayMode = Android_SetDisplayMode;

    /* Window functions */
    device->CreateSDLWindow = Android_CreateSDLWindow;
    // device->CreateSDLWindowFrom = Android_CreateSDLWindowFrom;
    device->SetWindowTitle = Android_SetWindowTitle;
    // device->SetWindowIcon = Android_SetWindowIcon;
    // device->SetWindowPosition = Android_SetWindowPosition;
    // device->SetWindowSize = Android_SetWindowSize;
    // device->SetWindowMinimumSize = Android_SetWindowMinimumSize;
    // device->SetWindowMaximumSize = Android_SetWindowMaximumSize;
    // device->GetWindowBordersSize = Android_GetWindowBordersSize;
    // device->GetWindowSizeInPixels = Android_GetWindowSizeInPixels;
    // device->SetWindowOpacity = Android_SetWindowOpacity;
    // device->SetWindowModalFor = Android_SetWindowModalFor;
    // device->SetWindowInputFocus = Android_SetWindowInputFocus;
    // device->ShowWindow = Android_ShowWindow;
    // device->HideWindow = Android_HideWindow;
    // device->RaiseWindow = Android_RaiseWindow;
    // device->MaximizeWindow = Android_MaximizeWindow;
    device->MinimizeWindow = Android_MinimizeWindow;
    // device->RestoreWindow = Android_RestoreWindow;
    // device->SetWindowBordered = Android_SetWindowBordered;
    device->SetWindowResizable = Android_SetWindowResizable;
    // device->SetWindowAlwaysOnTop = Android_SetWindowAlwaysOnTop;
    device->SetWindowFullscreen = Android_SetWindowFullscreen;
    // device->SetWindowGammaRamp = Android_SetWindowGammaRamp;
    // device->GetWindowGammaRamp = Android_GetWindowGammaRamp;
    // device->GetWindowICCProfile = Android_GetWindowICCProfile;
    // device->GetWindowDisplayIndex = Android_GetWindowDisplayIndex;
    // device->SetWindowMouseRect = Android_SetWindowMouseRect;
    // device->SetWindowMouseGrab = Android_SetWindowMouseGrab;
    // device->SetWindowKeyboardGrab = Android_SetWindowKeyboardGrab;
    device->DestroyWindow = Android_DestroyWindow;
    // device->CreateWindowFramebuffer = Android_CreateWindowFramebuffer;
    // device->UpdateWindowFramebuffer = Android_UpdateWindowFramebuffer;
    // device->DestroyWindowFramebuffer = Android_DestroyWindowFramebuffer;
    // device->OnWindowEnter = Android_OnWindowEnter;
    // device->FlashWindow = Android_FlashWindow;
    /* Shaped-window functions */
    // device->CreateShaper = Android_CreateShaper;
    // device->SetWindowShape = Android_SetWindowShape;
    /* Get some platform dependent window information */
    device->GetWindowWMInfo = Android_GetWindowWMInfo;

    /* OpenGL support */
#ifdef SDL_VIDEO_OPENGL_EGL
    device->GL_LoadLibrary = Android_GLES_LoadLibrary;
    device->GL_GetProcAddress = Android_GLES_GetProcAddress;
    device->GL_UnloadLibrary = Android_GLES_UnloadLibrary;
    device->GL_CreateContext = Android_GLES_CreateContext;
    device->GL_MakeCurrent = Android_GLES_MakeCurrent;
    // device->GL_GetDrawableSize = Android_GLES_GetDrawableSize;
    device->GL_SetSwapInterval = Android_GLES_SetSwapInterval;
    device->GL_GetSwapInterval = Android_GLES_GetSwapInterval;
    device->GL_SwapWindow = Android_GLES_SwapWindow;
    device->GL_DeleteContext = Android_GLES_DeleteContext;
    // device->GL_DefaultProfileConfig = Android_GLES_DefaultProfileConfig;
#endif

    /* Vulkan support */
#ifdef SDL_VIDEO_VULKAN
    device->Vulkan_LoadLibrary = Android_Vulkan_LoadLibrary;
    device->Vulkan_UnloadLibrary = Android_Vulkan_UnloadLibrary;
    device->Vulkan_GetInstanceExtensions = Android_Vulkan_GetInstanceExtensions;
    device->Vulkan_CreateSurface = Android_Vulkan_CreateSurface;
    // device->Vulkan_GetDrawableSize = Android_Vulkan_GetDrawableSize;
#endif

    /* Metal support */
#ifdef SDL_VIDEO_METAL
    // device->Metal_CreateView = Android_Metal_CreateView;
    // device->Metal_DestroyView = Android_Metal_DestroyView;
    // device->Metal_GetLayer = Android_Metal_GetLayer;
    // device->Metal_GetDrawableSize = Android_Metal_GetDrawableSize;
#endif

    /* Event manager functions */
    // device->WaitEventTimeout = Android_WaitEventTimeout;
    // device->SendWakeupEvent = Android_SendWakeupEvent;
    if (SDL_GetHintBoolean(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, SDL_TRUE)) {
        device->PumpEvents = Android_PumpEvents_Blocking;
    } else {
        device->PumpEvents = Android_PumpEvents_NonBlocking;
    }

    /* Screensaver */
    device->SuspendScreenSaver = Android_SuspendScreenSaver;

    /* Text input */
    // device->StartTextInput = Android_StartTextInput;
    // device->StopTextInput = Android_StopTextInput;
    device->SetTextInputRect = Android_SetTextInputRect;
    // device->ClearComposition = Android_ClearComposition;
    // device->IsTextInputShown = Android_IsTextInputShown;

    /* Screen keyboard */
    device->HasScreenKeyboardSupport = Android_HasScreenKeyboardSupport;
    device->ShowScreenKeyboard = Android_ShowScreenKeyboard;
    device->HideScreenKeyboard = Android_HideScreenKeyboard;
    device->IsScreenKeyboardShown = Android_IsScreenKeyboardShown;

    /* Clipboard */
    device->SetClipboardText = Android_SetClipboardText;
    device->GetClipboardText = Android_GetClipboardText;
    device->HasClipboardText = Android_HasClipboardText;
    // device->SetPrimarySelectionText = Android_SetPrimarySelectionText;
    // device->GetPrimarySelectionText = Android_GetPrimarySelectionText;
    // device->HasPrimarySelectionText = Android_HasPrimarySelectionText;

    /* Hit-testing */
    // device->SetWindowHitTest = Android_SetWindowHitTest;

    /* Tell window that app enabled drag'n'drop events */
    // device->AcceptDragAndDrop = Android_AcceptDragAndDrop;

    device->free = Android_DeleteDevice;

    return device;
}
/* "SDL Android video driver" */
const VideoBootStrap Android_bootstrap = {
    "Android", Android_CreateDevice
};

int Android_VideoInit(_THIS)
{
    Android_VideoData *videodata = &androidVideoData;
    int result;
    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;

    videodata->isPaused = SDL_FALSE;
    videodata->isPausing = SDL_FALSE;
    videodata->pauseAudio = SDL_GetHintBoolean(SDL_HINT_ANDROID_BLOCK_ON_PAUSE_PAUSEAUDIO, SDL_TRUE);

    current_mode.format = Android_ScreenFormat;
    current_mode.w = Android_DeviceWidth;
    current_mode.h = Android_DeviceHeight;
    current_mode.refresh_rate = Android_ScreenRate;
    current_mode.driverdata = NULL;

    SDL_zero(display);
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;
    // display.driverdata = NULL;
    display.orientation = Android_JNI_GetDisplayOrientation();

    SDL_AddDisplayMode(&display, &current_mode);
    result = SDL_AddVideoDisplay(&display, SDL_FALSE);
    if (result < 0) {
        SDL_free(display.display_modes);
    }

    Android_InitTouch();

    Android_InitMouse();

    return result;
}

void Android_VideoQuit(_THIS)
{
    Android_QuitMouse();
    Android_QuitTouch();
}

int Android_GetDisplayDPI(SDL_VideoDisplay *display, float *ddpi, float *hdpi, float *vdpi)
{
    return Android_JNI_GetDisplayDPI(ddpi, hdpi, vdpi);
}

void Android_SetScreenResolution(int surfaceWidth, int surfaceHeight, int deviceWidth, int deviceHeight, float rate)
{
    Android_SurfaceWidth = surfaceWidth;
    Android_SurfaceHeight = surfaceHeight;
    Android_DeviceWidth = deviceWidth;
    Android_DeviceHeight = deviceHeight;
    Android_ScreenRate = (int)rate;
}

static Uint32 format_to_pixelFormat(int format)
{
    Uint32 pf;
    if (format == AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM) { /* 1 */
        pf = SDL_PIXELFORMAT_RGBA8888;
    } else if (format == AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM) { /* 2 */
        pf = SDL_PIXELFORMAT_RGBX8888;
    } else if (format == AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM) { /* 3 */
        pf = SDL_PIXELFORMAT_RGB24;
    } else if (format == AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM) { /* 4*/
        pf = SDL_PIXELFORMAT_RGB565;
    } else if (format == 5) {
        pf = SDL_PIXELFORMAT_BGRA8888;
    } else if (format == 6) {
        pf = SDL_PIXELFORMAT_RGBA5551;
    } else if (format == 7) {
        pf = SDL_PIXELFORMAT_RGBA4444;
    } else if (format == 0x115) {
        /* HAL_PIXEL_FORMAT_BGR_565 */
        pf = SDL_PIXELFORMAT_RGB565;
    } else {
        pf = SDL_PIXELFORMAT_UNKNOWN;
    }
    return pf;
}

void Android_SetFormat(int format_wanted, int format_got)
{
    Uint32 pf_wanted;
    Uint32 pf_got;

    pf_wanted = format_to_pixelFormat(format_wanted);
    pf_got = format_to_pixelFormat(format_got);

    Android_ScreenFormat = pf_got;

    SDL_Log("pixel format wanted %s (%d), got %s (%d)",
            SDL_GetPixelFormatName(pf_wanted), format_wanted,
            SDL_GetPixelFormatName(pf_got), format_got);
}

void Android_OnResize(void)
{
    /*
      Update the resolution of the desktop mode, so that the window
      can be properly resized. The screen resolution change can for
      example happen when the Activity enters or exits immersive mode,
      which can happen after VideoInit().
    */
    SDL_Window *window;

    if (SDL_HasVideoDevice()) {
        int num_displays;
        SDL_VideoDisplay *displays = SDL_GetDisplays(&num_displays);
        if (num_displays > 0) {
            displays[0].desktop_mode.format = Android_ScreenFormat;
            displays[0].desktop_mode.w = Android_DeviceWidth;
            displays[0].desktop_mode.h = Android_DeviceHeight;
            displays[0].desktop_mode.refresh_rate = Android_ScreenRate;

            window = Android_Window;
            if (window) {
                /* Force the current mode to match the resize otherwise the SDL_WINDOWEVENT_RESTORED event
                 * will fall back to the old mode */
                SDL_assert(SDL_GetDisplayForWindow(window) == &displays[0]);
                displays[0].display_modes[0].format = Android_ScreenFormat;
                displays[0].display_modes[0].w = Android_DeviceWidth;
                displays[0].display_modes[0].h = Android_DeviceHeight;
                displays[0].display_modes[0].refresh_rate = Android_ScreenRate;
                displays[0].current_mode = displays[0].display_modes[0];

                SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESIZED, Android_SurfaceWidth, Android_SurfaceHeight);
            }
        } else {
            SDL_assert(Android_Window == NULL);
        }
    }
}

void Android_OnOrientationChanged(SDL_DisplayOrientation orientation)
{
    if (SDL_HasVideoDevice()) {
        int num_displays;
        SDL_VideoDisplay *displays = SDL_GetDisplays(&num_displays);
        if (num_displays > 0) {
            SDL_SendDisplayEvent(&displays[0], SDL_DISPLAYEVENT_ORIENTATION, orientation);
        }
    }
}

#endif /* SDL_VIDEO_DRIVER_ANDROID */

/* vi: set ts=4 sw=4 expandtab: */
