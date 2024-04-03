/*
  Simple DirectMedia Layer
  Copyright (C) 2017 BlackBerry Limited

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

#ifdef SDL_VIDEO_DRIVER_QNX

#include "../SDL_sysvideo.h"
#include "sdl_qnx.h"

#ifdef SDL_VIDEO_VULKAN
#error "Vulkan is configured, but not implemented for QNX."
#endif
#ifdef SDL_VIDEO_METAL
#error "Metal is configured, but not implemented for QNX."
#endif
#if defined(SDL_VIDEO_OPENGL_ANY) && !defined(SDL_VIDEO_OPENGL_EGL)
#error "OpenGL is configured, but not the implemented (EGL) for QNX."
#endif

static screen_context_t context;
static screen_event_t   event;

/**
 * Initializes the QNX video plugin.
 * Creates the Screen context and event handles used for all window operations
 * by the plugin.
 * @param   _THIS
 * @return  0 if successful, -1 on error
 */
static int QNX_VideoInit(_THIS)
{
    int result;
    SDL_VideoDisplay display;

    result = screen_create_context(&context, 0);
    if (result < 0) {
        return result;
    }
    result = screen_create_event(&event);
    if (result < 0) {
        return result;
    }

    SDL_zero(display);
    // FIXME DisplayModes?
    result = SDL_AddVideoDisplay(&display, SDL_FALSE);

    return result;
}

static void QNX_VideoQuit(_THIS)
{
}

/**
 * Creates a new native Screen window and associates it with the given SDL
 * window.
 * @param   _THIS
 * @param   window  SDL window to initialize
 * @return  0 if successful, -1 on error
 */
static int QNX_CreateSDLWindow(_THIS, SDL_Window *window)
{
    window_impl_t   *impl;
    int             size[2];
    int             numbufs;
    int             format;
    int             usage;

    impl = SDL_calloc(1, sizeof(*impl));
    if (!impl) {
        return -1;
    }

    // Create a native window.
    if (screen_create_window(&impl->window, context) < 0) {
        goto fail;
    }

    // Set the native window's size to match the SDL window.
    size[0] = window->wrect.w;
    size[1] = window->wrect.h;

    if (screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_SIZE,
                                      size) < 0) {
        goto fail;
    }

    if (screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_SOURCE_SIZE,
                                      size) < 0) {
        goto fail;
    }

    // Create window buffer(s).
    format = SCREEN_FORMAT_RGBX8888;
    numbufs = 1;
#ifdef SDL_VIDEO_OPENGL_EGL
    if (window->flags & SDL_WINDOW_OPENGL) {
        if (glGetConfig(&impl->conf, &format) < 0) {
            goto fail;
        }
        numbufs = 2;

        usage = SCREEN_USAGE_OPENGL_ES2;
        if (screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_USAGE,
                                          &usage) < 0) {
            return -1;
        }
    }
#endif

    // Set pixel format.
    if (screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_FORMAT,
                                      &format) < 0) {
        goto fail;
    }

    // Create buffer(s).
    if (screen_create_window_buffers(impl->window, numbufs) < 0) {
        goto fail;
    }

    window->driverdata = impl;
    return 0;

fail:
    if (impl->window) {
        screen_destroy_window(impl->window);
    }

    SDL_free(impl);
    return -1;
}

/**
 * Gets a pointer to the Screen buffer associated with the given window. Note
 * that the buffer is actually created in QNX_CreateSDLWindow().
 * @param       window  SDL window to get the buffer for
 * @param[out]  format  Holds the pixel format for the buffer
 * @param[out]  pixels  Holds a pointer to the window's buffer
 * @param[out]  pitch   Holds the number of bytes per line
 * @return  0 if successful, -1 on error
 */
static int QNX_CreateWindowFramebuffer(SDL_Window * window, Uint32 * format,
                        void ** pixels, int *pitch)
{
    window_impl_t   *impl = (window_impl_t *)window->driverdata;
    screen_buffer_t buffer;

    // Get a pointer to the buffer's memory.
    if (screen_get_window_property_pv(impl->window, SCREEN_PROPERTY_BUFFERS,
                                      (void **)&buffer) < 0) {
        return -1;
    }

    if (screen_get_buffer_property_pv(buffer, SCREEN_PROPERTY_POINTER,
                                      pixels) < 0) {
        return -1;
    }

    // Set format and pitch.
    if (screen_get_buffer_property_iv(buffer, SCREEN_PROPERTY_STRIDE,
                                      pitch) < 0) {
        return -1;
    }

    *format = SDL_PIXELFORMAT_RGB888;
    return 0;
}

/**
 * Informs the window manager that the window needs to be updated.
 * @param   window      The window to update
 * @param   rects       An array of reectangular areas to update
 * @param   numrects    Rect array length
 * @return  0 if successful, -1 on error
 */
static int QNX_UpdateWindowFramebuffer(SDL_Window *window, const SDL_Rect *rects,
                        int numrects)
{
    window_impl_t   *impl = (window_impl_t *)window->driverdata;
    screen_buffer_t buffer;

    if (screen_get_window_property_pv(impl->window, SCREEN_PROPERTY_BUFFERS,
                                      (void **)&buffer) < 0) {
        return -1;
    }

    screen_post_window(impl->window, buffer, numrects, (int *)rects, 0);
    screen_flush_context(context, 0);
    return 0;
}

/**
 * Destroy the screen buffer associated with the given window. Note
 * that the buffer is actually created in QNX_CreateSDLWindow() and
 * destroyed by QNX_DestroyWindow() so this function does nothing.
 * @param       window  SDL window to get the buffer for
 */
static void QNX_DestroyWindowFramebuffer(SDL_Window * window)
{
    // window_impl_t   *impl = (window_impl_t *)window->driverdata;

    // screen_destroy_window_buffers(impl->window);
}

/**
 * Runs the main event loop.
 */
static void QNX_PumpEvents(void)
{
    int             type;

    for (;;) {
        if (screen_get_event(context, event, 0) < 0) {
            break;
        }

        if (screen_get_event_property_iv(event, SCREEN_PROPERTY_TYPE, &type)
            < 0) {
            break;
        }

        if (type == SCREEN_EVENT_NONE) {
            break;
        }

        switch (type) {
        case SCREEN_EVENT_KEYBOARD:
            handleKeyboardEvent(event);
            break;

        default:
            break;
        }
    }
}

/**
 * Updates the size of the native window using the geometry of the SDL window.
 * @param   window  SDL window to update
 */
static void QNX_SetWindowSize(SDL_Window *window)
{
    window_impl_t   *impl = (window_impl_t *)window->driverdata;
    int             size[2];

    size[0] = window->wrect.w;
    size[1] = window->wrect.h;

    screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_SIZE, size);
    screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_SOURCE_SIZE,
                                  size);
}

/**
 * Makes the native window associated with the given SDL window visible.
 * @param   window  SDL window to update
 */
static void QNX_ShowWindow(SDL_Window *window)
{
    window_impl_t   *impl = (window_impl_t *)window->driverdata;
    const int       visible = 1;

    screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_VISIBLE,
                                  &visible);
}

/**
 * Makes the native window associated with the given SDL window invisible.
 * @param   window  SDL window to update
 */
static void QNX_HideWindow(SDL_Window *window)
{
    window_impl_t   *impl = (window_impl_t *)window->driverdata;
    const int       visible = 0;

    screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_VISIBLE,
        &visible);
}

/**
 * Destroys the native window associated with the given SDL window.
 * @param   window  SDL window that is being destroyed
 */
static void QNX_DestroyWindow(SDL_Window *window)
{
    window_impl_t   *impl = (window_impl_t *)window->driverdata;

    if (impl) {
        screen_destroy_window(impl->window);
        window->driverdata = NULL;
    }
}

/**
 * Frees the plugin object created by QNX_CreateDevice().
 * @param   _THIS  Plugin object to free
 */
static void QNX_DeleteDevice(_THIS)
{
}

/**
 * Creates the QNX video plugin used by SDL.
 * @return  Initialized device if successful, NULL otherwise
 */
static SDL_bool QNX_CreateDevice(SDL_VideoDevice *device)
{
    /* Set the function pointers */
    /* Initialization/Query functions */
    device->VideoInit = QNX_VideoInit;
    device->VideoQuit = QNX_VideoQuit;
    // device->GetDisplayBounds = QNX_GetDisplayBounds;
    // device->GetDisplayUsableBounds = QNX_GetDisplayUsableBounds;
    // device->GetDisplayDPI = QNX_GetDisplayDPI;
    // device->SetDisplayMode = QNX_SetDisplayMode;

    /* Window functions */
    device->CreateSDLWindow = QNX_CreateSDLWindow;
    // device->CreateSDLWindowFrom = QNX_CreateSDLWindowFrom;
    // device->SetWindowTitle = QNX_SetWindowTitle;
    // device->SetWindowIcon = QNX_SetWindowIcon;
    // device->SetWindowPosition = QNX_SetWindowPosition;
    device->SetWindowSize = QNX_SetWindowSize;
    // device->SetWindowMinimumSize = QNX_SetWindowMinimumSize;
    // device->SetWindowMaximumSize = QNX_SetWindowMaximumSize;
    // device->GetWindowBordersSize = QNX_GetWindowBordersSize;
    // device->GetWindowSizeInPixels = QNX_GetWindowSizeInPixels;
    // device->SetWindowOpacity = QNX_SetWindowOpacity;
    // device->SetWindowModalFor = QNX_SetWindowModalFor;
    // device->SetWindowInputFocus = QNX_SetWindowInputFocus;
    device->ShowWindow = QNX_ShowWindow;
    device->HideWindow = QNX_HideWindow;
    // device->RaiseWindow = QNX_RaiseWindow;
    // device->MaximizeWindow = QNX_MaximizeWindow;
    // device->MinimizeWindow = QNX_MinimizeWindow;
    // device->RestoreWindow = QNX_RestoreWindow;
    // device->SetWindowBordered = QNX_SetWindowBordered;
    // device->SetWindowResizable = QNX_SetWindowResizable;
    // device->SetWindowAlwaysOnTop = QNX_SetWindowAlwaysOnTop;
    // device->SetWindowFullscreen = QNX_SetWindowFullscreen;
    // device->SetWindowGammaRamp = QNX_SetWindowGammaRamp;
    // device->GetWindowGammaRamp = QNX_GetWindowGammaRamp;
    // device->GetWindowICCProfile = QNX_GetWindowICCProfile;
    // device->GetWindowDisplayIndex = QNX_GetWindowDisplayIndex;
    // device->SetWindowMouseRect = QNX_SetWindowMouseRect;
    // device->SetWindowMouseGrab = QNX_SetWindowMouseGrab;
    // device->SetWindowKeyboardGrab = QNX_SetWindowKeyboardGrab;
    device->DestroyWindow = QNX_DestroyWindow;
    device->CreateWindowFramebuffer = QNX_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = QNX_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = QNX_DestroyWindowFramebuffer;
    // device->OnWindowEnter = QNX_OnWindowEnter;
    // device->FlashWindow = QNX_FlashWindow;
    /* Shaped-window functions */
    // device->CreateShaper = QNX_CreateShaper;
    // device->SetWindowShape = QNX_SetWindowShape;
    /* Get some platform dependent window information */
    // device->GetWindowWMInfo = QNX_GetWindowWMInfo;

    /* OpenGL support */
#ifdef SDL_VIDEO_OPENGL_EGL
    device->GL_LoadLibrary = QNX_GL_LoadLibrary;
    device->GL_GetProcAddress = QNX_GL_GetProcAddress;
    device->GL_UnloadLibrary = QNX_GL_UnloadLibrary;
    device->GL_CreateContext = QNX_GL_CreateContext;
    device->GL_MakeCurrent = QNX_GL_MakeCurrent;
    device->GL_GetDrawableSize = QNX_GL_GetDrawableSize;
    device->GL_SetSwapInterval = QNX_GL_SetSwapInterval;
    device->GL_GetSwapInterval = QNX_GL_GetSwapInterval;
    device->GL_SwapWindow = QNX_GL_SwapWindow;
    device->GL_DeleteContext = QNX_GL_DeleteContext;
#endif

    /* Vulkan support */
#ifdef SDL_VIDEO_VULKAN
    // device->Vulkan_LoadLibrary = QNX_Vulkan_LoadLibrary;
    // device->Vulkan_UnloadLibrary = QNX_Vulkan_UnloadLibrary;
    // device->Vulkan_GetInstanceExtensions = QNX_Vulkan_GetInstanceExtensions;
    // device->Vulkan_CreateSurface = QNX_Vulkan_CreateSurface;
    // device->Vulkan_GetDrawableSize = QNX_Vulkan_GetDrawableSize;
#endif

    /* Metal support */
#ifdef SDL_VIDEO_METAL
    // device->Metal_CreateView = QNX_Metal_CreateView;
    // device->Metal_DestroyView = QNX_Metal_DestroyView;
    // device->Metal_GetLayer = QNX_Metal_GetLayer;
    // device->Metal_GetDrawableSize = QNX_Metal_GetDrawableSize;
#endif

    /* Event manager functions */
    // device->WaitEventTimeout = QNX_WaitEventTimeout;
    // device->SendWakeupEvent = QNX_SendWakeupEvent;
    device->PumpEvents = QNX_PumpEvents;

    /* Screensaver */
    // device->SuspendScreenSaver = QNX_SuspendScreenSaver;

    /* Text input */
    // device->StartTextInput = QNX_StartTextInput;
    // device->StopTextInput = QNX_StopTextInput;
    // device->SetTextInputRect = QNX_SetTextInputRect;
    // device->ClearComposition = QNX_ClearComposition;
    // device->IsTextInputShown = QNX_IsTextInputShown;

    /* Screen keyboard */
    // device->HasScreenKeyboardSupport = QNX_HasScreenKeyboardSupport;
    // device->ShowScreenKeyboard = QNX_ShowScreenKeyboard;
    // device->HideScreenKeyboard = QNX_HideScreenKeyboard;
    // device->IsScreenKeyboardShown = QNX_IsScreenKeyboardShown;

    /* Clipboard */
    // device->SetClipboardText = QNX_SetClipboardText;
    // device->GetClipboardText = QNX_GetClipboardText;
    // device->HasClipboardText = QNX_HasClipboardText;
    // device->SetPrimarySelectionText = QNX_SetPrimarySelectionText;
    // device->GetPrimarySelectionText = QNX_GetPrimarySelectionText;
    // device->HasPrimarySelectionText = QNX_HasPrimarySelectionText;

    /* Hit-testing */
    // device->SetWindowHitTest = QNX_SetWindowHitTest;

    /* Tell window that app enabled drag'n'drop events */
    // device->AcceptDragAndDrop = QNX_AcceptDragAndDrop;

    device->DeleteDevice = QNX_DeleteDevice;

    return SDL_TRUE;
}
/* "QNX Screen" */
const VideoBootStrap QNX_bootstrap = {
    "qnx", QNX_CreateDevice
};

#endif // SDL_VIDEO_DRIVER_QNX