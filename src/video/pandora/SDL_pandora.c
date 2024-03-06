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

#ifdef SDL_VIDEO_DRIVER_PANDORA

/* SDL internals */
#include "../SDL_sysvideo.h"
#include "SDL_version.h"
#include "SDL_syswm.h"
#include "SDL_loadso.h"
#include "SDL_events.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"

/* PND declarations */
#include "SDL_pandora.h"
#include "SDL_pandora_events.h"

#ifdef SDL_VIDEO_OPENGL_EGL
#include "GLES/gl.h"
/* WIZ declarations */
#ifdef WIZ_GLES_LITE
static NativeWindowType hNativeWnd = 0; /* A handle to the window we will create. */
#endif
#endif

#ifdef SDL_VIDEO_VULKAN
#error "Vulkan is configured, but not implemented for pandora."
#endif
#ifdef SDL_VIDEO_METAL
#error "Metal is configured, but not implemented for pandora."
#endif
#if defined(SDL_VIDEO_OPENGL_ANY) && !defined(SDL_VIDEO_OPENGL_EGL)
#error "OpenGL is configured, but not the implemented (EGL) for pandora."
#endif

#ifdef SDL_VIDEO_OPENGL_EGL
/* Instance */
Pandora_VideoData pandoraVideoData;
#endif

static int PND_EGL_ChooseConfig(_THIS);
static EGLSurface PND_EGL_CreateSurface(_THIS);
static void PND_EGL_DestroySurface(EGLSurface egl_surface);

static void PND_DeleteDevice(_THIS)
{
#ifdef SDL_VIDEO_OPENGL_EGL
    SDL_zero(pandoraVideoData);
#endif
    SDL_free(_this);
}

static SDL_VideoDevice *PND_CreateDevice(void)
{
    SDL_VideoDevice *device;

    /* Initialize SDL_VideoDevice structure */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return NULL;
    }

    /* Set the function pointers */
    /* Initialization/Query functions */
    device->VideoInit = PND_VideoInit;
    device->VideoQuit = PND_VideoQuit;
    // device->GetDisplayBounds = PND_GetDisplayBounds;
    // device->GetDisplayUsableBounds = PND_GetDisplayUsableBounds;
    // device->GetDisplayDPI = PND_GetDisplayDPI;
    device->SetDisplayMode = PND_SetDisplayMode;

    /* Window functions */
    device->CreateSDLWindow = PND_CreateSDLWindow;
    // device->CreateSDLWindowFrom = PND_CreateSDLWindowFrom;
    // device->SetWindowTitle = PND_SetWindowTitle;
    // device->SetWindowIcon = PND_SetWindowIcon;
    // device->SetWindowPosition = PND_SetWindowPosition;
    // device->SetWindowSize = PND_SetWindowSize;
    // device->SetWindowMinimumSize = PND_SetWindowMinimumSize;
    // device->SetWindowMaximumSize = PND_SetWindowMaximumSize;
    // device->GetWindowBordersSize = PND_GetWindowBordersSize;
    // device->GetWindowSizeInPixels = PND_GetWindowSizeInPixels;
    // device->SetWindowOpacity = PND_SetWindowOpacity;
    // device->SetWindowModalFor = PND_SetWindowModalFor;
    // device->SetWindowInputFocus = PND_SetWindowInputFocus;
    // device->ShowWindow = PND_ShowWindow;
    // device->HideWindow = PND_HideWindow;
    // device->RaiseWindow = PND_RaiseWindow;
    // device->MaximizeWindow = PND_MaximizeWindow;
    device->MinimizeWindow = PND_MinimizeWindow;
    // device->RestoreWindow = PND_RestoreWindow;
    // device->SetWindowBordered = PND_SetWindowBordered;
    // device->SetWindowResizable = PND_SetWindowResizable;
    // device->SetWindowAlwaysOnTop = PND_SetWindowAlwaysOnTop;
    // device->SetWindowFullscreen = PND_SetWindowFullscreen;
    // device->SetWindowGammaRamp = PND_SetWindowGammaRamp;
    // device->GetWindowGammaRamp = PND_GetWindowGammaRamp;
    // device->GetWindowICCProfile = PND_GetWindowICCProfile;
    // device->GetWindowDisplayIndex = PND_GetWindowDisplayIndex;
    // device->SetWindowMouseRect = PND_SetWindowMouseRect;
    // device->SetWindowMouseGrab = PND_SetWindowMouseGrab;
    // device->SetWindowKeyboardGrab = PND_SetWindowKeyboardGrab;
    device->DestroyWindow = PND_DestroyWindow;
    // device->CreateWindowFramebuffer = PND_CreateWindowFramebuffer;
    // device->UpdateWindowFramebuffer = PND_UpdateWindowFramebuffer;
    // device->DestroyWindowFramebuffer = PND_DestroyWindowFramebuffer;
    // device->OnWindowEnter = PND_OnWindowEnter;
    // device->FlashWindow = PND_FlashWindow;
    /* Shaped-window functions */
    // device->CreateShaper = PND_CreateShaper;
    // device->SetWindowShape = PND_SetWindowShape;
#if 0
    /* Get some platform dependent window information */
    device->GetWindowWMInfo = PND_GetWindowWMInfo;
#endif

    /* OpenGL support */
#ifdef SDL_VIDEO_OPENGL_EGL
    device->GL_LoadLibrary = PND_GL_LoadLibrary;
    device->GL_GetProcAddress = PND_GL_GetProcAddress;
    device->GL_UnloadLibrary = PND_GL_UnloadLibrary;
    device->GL_CreateContext = PND_GL_CreateContext;
    device->GL_MakeCurrent = PND_GL_MakeCurrent;
    // device->GL_GetDrawableSize = PND_GL_GetDrawableSize;
    device->GL_SetSwapInterval = PND_GL_SetSwapInterval;
    device->GL_GetSwapInterval = PND_GL_GetSwapInterval;
    device->GL_SwapWindow = PND_GL_SwapWindow;
    device->GL_DeleteContext = PND_GL_DeleteContext;
    // device->GL_DefaultProfileConfig = PND_GL_DefaultProfileConfig;
#endif

    /* Vulkan support */
#ifdef SDL_VIDEO_VULKAN
    // device->Vulkan_LoadLibrary = PND_Vulkan_LoadLibrary;
    // device->Vulkan_UnloadLibrary = PND_Vulkan_UnloadLibrary;
    // device->Vulkan_GetInstanceExtensions = PND_Vulkan_GetInstanceExtensions;
    // device->Vulkan_CreateSurface = PND_Vulkan_CreateSurface;
    // device->Vulkan_GetDrawableSize = PND_Vulkan_GetDrawableSize;
#endif

    /* Metal support */
#ifdef SDL_VIDEO_METAL
    // device->Metal_CreateView = PND_Metal_CreateView;
    // device->Metal_DestroyView = PND_Metal_DestroyView;
    // device->Metal_GetLayer = PND_Metal_GetLayer;
    // device->Metal_GetDrawableSize = PND_Metal_GetDrawableSize;
#endif

    /* Event manager functions */
    // device->WaitEventTimeout = PND_WaitEventTimeout;
    // device->SendWakeupEvent = PND_SendWakeupEvent;
    device->PumpEvents = PND_PumpEvents;

    /* Screensaver */
    // device->SuspendScreenSaver = PND_SuspendScreenSaver;

    /* Text input */
    // device->StartTextInput = PND_StartTextInput;
    // device->StopTextInput = PND_StopTextInput;
    // device->SetTextInputRect = PND_SetTextInputRect;
    // device->ClearComposition = PND_ClearComposition;
    // device->IsTextInputShown = PND_IsTextInputShown;

    /* Screen keyboard */
    // device->HasScreenKeyboardSupport = PND_HasScreenKeyboardSupport;
    // device->ShowScreenKeyboard = PND_ShowScreenKeyboard;
    // device->HideScreenKeyboard = PND_HideScreenKeyboard;
    // device->IsScreenKeyboardShown = PND_IsScreenKeyboardShown;

    /* Clipboard */
    // device->SetClipboardText = PND_SetClipboardText;
    // device->GetClipboardText = PND_GetClipboardText;
    // device->HasClipboardText = PND_HasClipboardText;
    // device->SetPrimarySelectionText = PND_SetPrimarySelectionText;
    // device->GetPrimarySelectionText = PND_GetPrimarySelectionText;
    // device->HasPrimarySelectionText = PND_HasPrimarySelectionText;

    /* Hit-testing */
    // device->SetWindowHitTest = PND_SetWindowHitTest;

    /* Tell window that app enabled drag'n'drop events */
    // device->AcceptDragAndDrop = PND_AcceptDragAndDrop;

    device->free = PND_DeleteDevice;

    return device;
}
/* "SDL Wiz Video Driver" / "SDL Pandora Video Driver" */
const VideoBootStrap PND_bootstrap = {
#ifdef WIZ_GLES_LITE
    "wiz",
#else
    "pandora",
#endif
    PND_CreateDevice
};

/*****************************************************************************/
/* SDL Video and Display initialization/handling functions                   */
/*****************************************************************************/
int PND_VideoInit(_THIS)
{
    int result;
    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;

    current_mode.format = SDL_PIXELFORMAT_RGB565;
#ifdef WIZ_GLES_LITE
    current_mode.w = 320;
    current_mode.h = 240;
#else
    current_mode.w = 800;
    current_mode.h = 480;
#endif
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

void PND_VideoQuit(_THIS)
{

}

int PND_SetDisplayMode(SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    return 0;
}

int PND_CreateSDLWindow(_THIS, SDL_Window * window)
{
#ifdef SDL_VIDEO_OPENGL_EGL
    SDL_WindowData *wdata;

    /* Allocate window internal data */
    wdata = (SDL_WindowData *) SDL_calloc(1, sizeof(SDL_WindowData));
    if (!wdata) {
        return SDL_OutOfMemory();
    }

    /* Setup driver data for this window */
    window->driverdata = wdata;

    /* Check if window must support OpenGL ES rendering */
    if (window->flags & SDL_WINDOW_OPENGL) {
        wdata->gles_surface = PND_EGL_CreateSurface(_this);
        if (wdata->egl_surface == EGL_NO_SURFACE) {
            return -1;
        }
    }
#endif
    /* Window has been successfully created */
    return 0;
}

/*int PND_CreateSDLWindowFrom(_THIS, SDL_Window * window, const void *data)
{
    return -1;
}

void PND_SetWindowTitle(SDL_Window * window)
{
}
void PND_SetWindowIcon(SDL_Window * window, SDL_Surface * icon)
{
}
void PND_SetWindowPosition(SDL_Window * window)
{
}
void PND_SetWindowSize(SDL_Window * window)
{
}
void PND_ShowWindow(SDL_Window * window)
{
}
void PND_HideWindow(SDL_Window * window)
{
}
void PND_RaiseWindow(_THIS, SDL_Window * window)
{
}
void PND_MaximizeWindow(SDL_Window * window)
{
}*/
void PND_MinimizeWindow(SDL_Window * window)
{
}
/*void PND_RestoreWindow(SDL_Window * window)
{
}*/
void PND_DestroyWindow(SDL_Window * window)
{
#ifdef SDL_VIDEO_OPENGL_EGL
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    if (data) {
        PND_EGL_DestroySurface(data->gles_surface);

        SDL_free(data);
        window->driverdata = NULL;
    }
#endif
}

/*****************************************************************************/
/* SDL Window Manager function                                               */
/*****************************************************************************/
#if 0
SDL_bool PND_GetWindowWMInfo(SDL_Window * window, struct SDL_SysWMinfo *info)
{
    if (info->version.major <= SDL_MAJOR_VERSION) {
        return SDL_TRUE;
    } else {
        SDL_SetError("application not compiled with SDL %d",
                     SDL_MAJOR_VERSION);
        return SDL_FALSE;
    }

    /* Failed to get window manager information */
    return SDL_FALSE;
}
#endif

/*****************************************************************************/
/* SDL OpenGL/OpenGL ES functions                                            */
/*****************************************************************************/
#ifdef SDL_VIDEO_OPENGL_EGL
static int PND_EGL_InitializeDisplay(EGLDisplay display)
{
    Pandora_VideoData *phdata = &pandoraVideoData;

    if (display == EGL_NO_DISPLAY) {
        return SDL_SetError("Could not get EGL display");
    }

    if (eglInitialize(display, NULL, NULL) != EGL_TRUE) {
        return SDL_SetError("Could not initialize EGL");
    }
    phdata->egl_display = display;
    return 0;
}

int PND_GL_LoadLibrary(_THIS, const char *path)
{
    EGLDisplay display;

    /* Check if OpenGL ES library is specified for GF driver */
    if (!path) {
        path = SDL_getenv("SDL_OPENGL_LIBRARY");
        if (!path) {
            path = SDL_getenv("SDL_OPENGLES_LIBRARY");
        }
    }

    /* Check if default library loading requested */
    if (!path) {
        /* Already linked with GF library which provides egl* subset of  */
        /* functions, use Common profile of OpenGL ES library by default */
#ifdef WIZ_GLES_LITE
        path = "/lib/libopengles_lite.so";
#else
        path = "/usr/lib/libGLES_CM.so";
#endif
    }

    /* Load dynamic library */
    phdata->opengl_dll_handle = SDL_LoadObject(path);
    if (!phdata->opengl_dll_handle) {
        /* Failed to load new GL ES library */
        return SDL_SetError("PND: Failed to locate OpenGL ES library");
    }

    /* New OpenGL ES library is loaded */

    /* Create connection to OpenGL ES */
    display = eglGetDisplay((NativeDisplayType)0);
    if (PND_EGL_InitializeDisplay(display) < 0) {
        PND_GL_UnloadLibrary(_this);
        return -1;
    }

    _this->gl_config.accelerated = 1;

    /* Under PND OpenGL ES output can't be double buffered */
    _this->gl_config.double_buffer = 0;

    return 0;
}

void *PND_GL_GetProcAddress(const char *proc)
{
    void *function_address;

    /* Try to get function address through the egl interface */
    function_address = eglGetProcAddress(proc);
    if (function_address) {
        return function_address;
    }

    /* Then try to get function in the OpenGL ES library */
    if (phdata->opengl_dll_handle) {
        function_address =
            SDL_LoadFunction(phdata->opengl_dll_handle, proc);
        if (function_address) {
            return function_address;
        }
    }

    /* Failed to get GL ES function address pointer */
    SDL_SetError("PND: Cannot locate OpenGL ES function name");
    return NULL;
}

void PND_GL_UnloadLibrary(_THIS)
{
    Pandora_VideoData *phdata = &pandoraVideoData;

    eglTerminate(phdata->egl_display);
    phdata->egl_display = EGL_NO_DISPLAY;

    /* Unload OpenGL ES library */
    SDL_UnloadObject(phdata->opengl_dll_handle);
    phdata->opengl_dll_handle = NULL;
}

static int PND_EGL_ChooseConfig(_THIS)
{
    Pandora_VideoData *phdata = &pandoraVideoData;
    EGLBoolean status;
    EGLint configs;
    uint32_t attr_pos;
    EGLint attr_value;
    EGLint cit;
    EGLint gles_attributes[32];        /* OpenGL ES attributes for context   */
    EGLConfig gles_configs[32];
    EGLint gles_config;         /* OpenGL ES configuration index      */

    /* Prepare attributes list to pass them to OpenGL ES */
    attr_pos = 0;
    gles_attributes[attr_pos++] = EGL_SURFACE_TYPE;
    gles_attributes[attr_pos++] = EGL_WINDOW_BIT;
    gles_attributes[attr_pos++] = EGL_RED_SIZE;
    gles_attributes[attr_pos++] = _this->gl_config.red_size;
    gles_attributes[attr_pos++] = EGL_GREEN_SIZE;
    gles_attributes[attr_pos++] = _this->gl_config.green_size;
    gles_attributes[attr_pos++] = EGL_BLUE_SIZE;
    gles_attributes[attr_pos++] = _this->gl_config.blue_size;
    gles_attributes[attr_pos++] = EGL_ALPHA_SIZE;

    /* Setup alpha size in bits */
    if (_this->gl_config.alpha_size) {
        gles_attributes[attr_pos++] = _this->gl_config.alpha_size;
    } else {
        gles_attributes[attr_pos++] = EGL_DONT_CARE;
    }

    /* Setup color buffer size */
    if (_this->gl_config.buffer_size) {
        gles_attributes[attr_pos++] = EGL_BUFFER_SIZE;
        gles_attributes[attr_pos++] = _this->gl_config.buffer_size;
    } else {
        gles_attributes[attr_pos++] = EGL_BUFFER_SIZE;
        gles_attributes[attr_pos++] = EGL_DONT_CARE;
    }

    /* Setup depth buffer bits */
    gles_attributes[attr_pos++] = EGL_DEPTH_SIZE;
    gles_attributes[attr_pos++] = _this->gl_config.depth_size;

    /* Setup stencil bits */
    if (_this->gl_config.stencil_size) {
        gles_attributes[attr_pos++] = EGL_STENCIL_SIZE;
        gles_attributes[attr_pos++] = _this->gl_config.buffer_size;
    } else {
        gles_attributes[attr_pos++] = EGL_STENCIL_SIZE;
        gles_attributes[attr_pos++] = EGL_DONT_CARE;
    }

    /* Set number of samples in multisampling */
    if (_this->gl_config.multisamplesamples) {
        gles_attributes[attr_pos++] = EGL_SAMPLES;
        gles_attributes[attr_pos++] =
            _this->gl_config.multisamplesamples;
    }

    /* Multisample buffers, OpenGL ES 1.0 spec defines 0 or 1 buffer */
    if (_this->gl_config.multisamplebuffers) {
        gles_attributes[attr_pos++] = EGL_SAMPLE_BUFFERS;
        gles_attributes[attr_pos++] =
            _this->gl_config.multisamplebuffers;
    }

    /* Finish attributes list */
    SDL_assert(attr_pos < SDL_arraysize(gles_attributes));
    gles_attributes[attr_pos] = EGL_NONE;

    /* Request first suitable framebuffer configuration */
    status = eglChooseConfig(phdata->egl_display, gles_attributes,
                             gles_configs, SDL_arraysize(gles_configs), &configs);
    if (status != EGL_TRUE) {
        return SDL_SetError("PND: Can't find closest configuration for OpenGL ES");
    }

    /* Check if nothing has been found, try "don't care" settings */
    if (configs == 0) {
        int32_t it;
        int32_t jt;
        GLint depthbits[4] = { 32, 24, 16, EGL_DONT_CARE };

        for (it = 0; it < 4; it++) {
            for (jt = 16; jt >= 0; jt--) {
                /* Don't care about color buffer bits, use what exist */
                /* Replace previous set data with EGL_DONT_CARE       */
                attr_pos = 0;
                gles_attributes[attr_pos++] = EGL_SURFACE_TYPE;
                gles_attributes[attr_pos++] = EGL_WINDOW_BIT;
                gles_attributes[attr_pos++] = EGL_RED_SIZE;
                gles_attributes[attr_pos++] = EGL_DONT_CARE;
                gles_attributes[attr_pos++] = EGL_GREEN_SIZE;
                gles_attributes[attr_pos++] = EGL_DONT_CARE;
                gles_attributes[attr_pos++] = EGL_BLUE_SIZE;
                gles_attributes[attr_pos++] = EGL_DONT_CARE;
                gles_attributes[attr_pos++] = EGL_ALPHA_SIZE;
                gles_attributes[attr_pos++] = EGL_DONT_CARE;
                gles_attributes[attr_pos++] = EGL_BUFFER_SIZE;
                gles_attributes[attr_pos++] = EGL_DONT_CARE;

                /* Try to find requested or smallest depth */
                if (_this->gl_config.depth_size) {
                    gles_attributes[attr_pos++] = EGL_DEPTH_SIZE;
                    gles_attributes[attr_pos++] = depthbits[it];
                } else {
                    gles_attributes[attr_pos++] = EGL_DEPTH_SIZE;
                    gles_attributes[attr_pos++] = EGL_DONT_CARE;
                }

                if (_this->gl_config.stencil_size) {
                    gles_attributes[attr_pos++] = EGL_STENCIL_SIZE;
                    gles_attributes[attr_pos++] = jt;
                } else {
                    gles_attributes[attr_pos++] = EGL_STENCIL_SIZE;
                    gles_attributes[attr_pos++] = EGL_DONT_CARE;
                }

                gles_attributes[attr_pos++] = EGL_SAMPLES;
                gles_attributes[attr_pos++] = EGL_DONT_CARE;
                gles_attributes[attr_pos++] = EGL_SAMPLE_BUFFERS;
                gles_attributes[attr_pos++] = EGL_DONT_CARE;
                gles_attributes[attr_pos] = EGL_NONE;

                /* Request first suitable framebuffer configuration */
                status =
                    eglChooseConfig(phdata->egl_display,
                                    gles_attributes,
                                    gles_configs, SDL_arraysize(gles_configs), &configs);

                if (status != EGL_TRUE) {
                    return SDL_SetError("PND: Can't find closest configuration for OpenGL ES");
                }
                if (configs != 0) {
                    break;
                }
            }
            if (configs != 0) {
                break;
            }
        }

        /* No available configs */
        if (configs == 0) {
            return SDL_SetError("PND: Can't find any configuration for OpenGL ES");
        }
    }

    /* Initialize config index */
    gles_config = 0;

    /* Now check each configuration to find out the best */
    for (cit = 0; cit < configs; cit++) {
        uint32_t stencil_found;
        uint32_t depth_found;

        stencil_found = 0;
        depth_found = 0;

        if (_this->gl_config.stencil_size) {
            status =
                eglGetConfigAttrib(phdata->egl_display,
                                   gles_configs[cit], EGL_STENCIL_SIZE,
                                   &attr_value);
            if (status == EGL_TRUE) {
                if (attr_value != 0) {
                    stencil_found = 1;
                }
            }
        } else {
            stencil_found = 1;
        }

        if (_this->gl_config.depth_size) {
            status =
                eglGetConfigAttrib(phdata->egl_display,
                                   gles_configs[cit], EGL_DEPTH_SIZE,
                                   &attr_value);
            if (status == EGL_TRUE) {
                if (attr_value != 0) {
                    depth_found = 1;
                }
            }
        } else {
            depth_found = 1;
        }

        /* Exit from loop if found appropriate configuration */
        if (depth_found && stencil_found) {
            gles_config = cit;
            break;
        }
    }

    phdata->gles_config = gles_configs[gles_config];

    /* Get back samples and samplebuffers configurations. Rest framebuffer */
    /* parameters could be obtained through the OpenGL ES API              */
    status =
        eglGetConfigAttrib(phdata->egl_display,
                           phdata->gles_config,
                           EGL_SAMPLES, &attr_value);
    if (status == EGL_TRUE) {
        _this->gl_config.multisamplesamples = attr_value;
    }
    status =
        eglGetConfigAttrib(phdata->egl_display,
                           phdata->gles_config,
                           EGL_SAMPLE_BUFFERS, &attr_value);
    if (status == EGL_TRUE) {
        _this->gl_config.multisamplebuffers = attr_value;
    }

    /* Get back stencil and depth buffer sizes */
    status =
        eglGetConfigAttrib(phdata->egl_display,
                           phdata->gles_config,
                           EGL_DEPTH_SIZE, &attr_value);
    if (status == EGL_TRUE) {
        _this->gl_config.depth_size = attr_value;
    }
    status =
        eglGetConfigAttrib(phdata->egl_display,
                           phdata->gles_config,
                           EGL_STENCIL_SIZE, &attr_value);
    if (status == EGL_TRUE) {
        _this->gl_config.stencil_size = attr_value;
    }

    return 0;
}

static EGLSurface PND_EGL_CreateSurface(_THIS)
{
    Pandora_VideoData *phdata = &pandoraVideoData;
    EGLSurface surface;

    if (PND_EGL_ChooseConfig(_this) != 0) {
        return EGL_NO_SURFACE;
    }

#ifdef WIZ_GLES_LITE
    SDL_assert(hNativeWnd == NULL);
    hNativeWnd = (NativeWindowType)SDL_malloc(16 * 1024);
    if (!hNativeWnd) {
        SDL_OutOfMemory();
        return EGL_NO_SURFACE;
    } else {
        SDL_Log("SDL: Wiz framebuffer allocated: %X\n", hNativeWnd);
    }

    wdata->gles_surface =
        eglCreateWindowSurface(phdata->egl_display,
                               phdata->gles_config,
                               hNativeWnd, NULL );
#else
    wdata->gles_surface =
        eglCreateWindowSurface(phdata->egl_display,
                               phdata->gles_config,
                               (NativeWindowType) 0, NULL);
#endif

    if (surface == EGL_NO_SURFACE) {
        SDL_SetError("unable to create an EGL window surface");
    }
    return surface;
}

static void PND_EGL_DestroySurface(EGLSurface egl_surface)
{
#ifdef WIZ_GLES_LITE
    if (hNativeWnd != 0) {
        SDL_free(hNativeWnd);
        hNativeWnd = 0;
        SDL_Log("SDL: Wiz framebuffer released\n");
    }
#endif
    if (egl_surface != EGL_NO_SURFACE) {
        Pandora_VideoData *phdata = &pandoraVideoData;
        eglDestroySurface(phdata->egl_display, egl_surface);
    }
}

SDL_GLContext PND_GL_CreateContext(_THIS, SDL_Window *window)
{
    Pandora_VideoData *phdata = &pandoraVideoData;
    SDL_GLContext gles_context;

    /* Create OpenGL ES context */
    gles_context =
        eglCreateContext(phdata->egl_display,
                         phdata->gles_config, NULL, NULL);
    if (gles_context == EGL_NO_CONTEXT) {
        SDL_SetError("PND: OpenGL ES context creation has been failed");
        return NULL;
    }

    /* Make just created context current */
    if (PND_GL_MakeCurrent(window, gles_context) < 0) {
        /* Destroy OpenGL ES surface */
        eglDestroyContext(phdata->egl_display, gles_context);
        SDL_SetError("PND: Can't set OpenGL ES context on creation");
        return NULL;
    }

    /* GL ES context was successfully created */
    return gles_context;
}

int PND_GL_MakeCurrent(SDL_Window *window, SDL_GLContext context)
{
    Pandora_VideoData *phdata = &pandoraVideoData;
    SDL_WindowData *wdata;
    EGLBoolean status;
    EGLSurface egl_surface;

    if (!window) {
        egl_surface = EGL_NO_SURFACE;
        SDL_assert(context == EGL_NO_CONTEXT);
    } else {
        wdata = (SDL_WindowData *) window->driverdata;
        egl_surface = wdata->gles_surface;
        SDL_assert(context != EGL_NO_CONTEXT);
        SDL_assert(egl_surface != EGL_NO_SURFACE);
    }
    status =
        eglMakeCurrent(phdata->egl_display, egl_surface,
            egl_surface, context);
    if (status != EGL_TRUE) {
        /* Failed to set current GL ES context */
        return SDL_SetError("PND: Can't set OpenGL ES context");
    }
    return 0;
}

int PND_GL_SetSwapInterval(int interval)
{
    Pandora_VideoData *phdata = &pandoraVideoData;
    EGLBoolean status;

    /* Check if OpenGL ES connection has been initialized */
    if (phdata->egl_display != EGL_NO_DISPLAY) {
        /* Set swap OpenGL ES interval */
        status = eglSwapInterval(phdata->egl_display, interval);
        if (status == EGL_TRUE) {
            /* Return success to upper level */
            phdata->swapinterval = interval;
            return 0;
        }
    }

    /* Failed to set swap interval */
    return SDL_SetError("PND: Cannot set swap interval");
}

int PND_GL_GetSwapInterval(void)
{
    return pandoraVideoData.swapinterval;
}

int PND_GL_SwapWindow(_THIS, SDL_Window * window)
{
    Pandora_VideoData *phdata = &pandoraVideoData;
    SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;

    /* Many applications do not uses glFinish(), so we call it for them */
    glFinish();

    /* Wait until OpenGL ES rendering is completed */
    eglWaitGL();

    eglSwapBuffers(phdata->egl_display, wdata->gles_surface);
    return 0;
}

void PND_GL_DeleteContext(SDL_GLContext context)
{
    Pandora_VideoData *phdata = &pandoraVideoData;

    /* Check if OpenGL ES connection has been initialized */
    if (phdata->egl_display != EGL_NO_DISPLAY) {
        if (context != EGL_NO_CONTEXT) {
            eglDestroyContext(phdata->egl_display, context);
        }
    }
}

#endif // SDL_VIDEO_OPENGL_EGL

#endif /* SDL_VIDEO_DRIVER_PANDORA */

/* vi: set ts=4 sw=4 expandtab: */
