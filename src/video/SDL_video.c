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

#include "../SDL_internal.h"

/* The high-level video driver subsystem */

#include "SDL.h"
#include "SDL_video.h"
#include "SDL_shape.h"
#include "SDL_sysvideo.h"
#include "SDL_framebuffer.h"
#include "../events/SDL_events_c.h"
#include "../timer/SDL_timer_c.h"

#include "SDL_syswm.h"

#ifdef SDL_VIDEO_OPENGL
#include "SDL_opengl.h"
#endif /* SDL_VIDEO_OPENGL */

#if defined(SDL_VIDEO_OPENGL_ES) && !defined(SDL_VIDEO_OPENGL)
#include "SDL_opengles.h"
#endif /* SDL_VIDEO_OPENGL_ES && !SDL_VIDEO_OPENGL */

/* GL and GLES2 headers conflict on Linux 32 bits */
#if defined(SDL_VIDEO_OPENGL_ES2) && !defined(SDL_VIDEO_OPENGL)
#include "SDL_opengles2.h"
#endif /* SDL_VIDEO_OPENGL_ES2 && !SDL_VIDEO_OPENGL */

#ifndef SDL_VIDEO_OPENGL
#ifndef GL_CONTEXT_RELEASE_BEHAVIOR_KHR
#define GL_CONTEXT_RELEASE_BEHAVIOR_KHR 0x82FB
#endif
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef __LINUX__
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>
#endif

/* Available video drivers */
static const VideoBootStrap *const bootstrap[] = {
    COCOA_BOOTSTRAP_ENTRY
    X11_BOOTSTRAP_ENTRY
    Wayland_BOOTSTRAP_ENTRY
    VIVANTE_BOOTSTRAP_ENTRY
    DirectFB_BOOTSTRAP_ENTRY
    WIN_BOOTSTRAP_ENTRY
    WINRT_BOOTSTRAP_ENTRY
    HAIKU_BOOTSTRAP_ENTRY
    PND_BOOTSTRAP_ENTRY
    UIKit_BOOTSTRAP_ENTRY
    Android_BOOTSTRAP_ENTRY
    PS2_BOOTSTRAP_ENTRY
    PSP_BOOTSTRAP_ENTRY
    VITA_BOOTSTRAP_ENTRY
    N3DS_BOOTSTRAP_ENTRY
    KMSDRM_BOOTSTRAP_ENTRY
    RISCOS_BOOTSTRAP_ENTRY
    RPI_BOOTSTRAP_ENTRY
    NACL_BOOTSTRAP_ENTRY
    Emscripten_BOOTSTRAP_ENTRY
    QNX_BOOTSTRAP_ENTRY
    OS2DIVE_BOOTSTRAP_ENTRY
    OS2VMAN_BOOTSTRAP_ENTRY
    NGAGE_BOOTSTRAP_ENTRY
    OFFSCREEN_BOOTSTRAP_ENTRY
    DUMMY_BOOTSTRAP_ENTRY
    DUMMY_evdev_BOOTSTRAP_ENTRY
};
SDL_COMPILE_TIME_ASSERT(bootstrap_count, SDL_arraysize(bootstrap) == SDL_VIDEODRIVERS_count);
/* MessageBox implementations per video drivers */
static msgBoxFunc *const messagebox[] = {
    COCOA_MSGBOX_ENTRY
    X11_MSGBOX_ENTRY
    Wayland_MSGBOX_ENTRY
    VIVANTE_MSGBOX_ENTRY
    DirectFB_MSGBOX_ENTRY
    WIN_MSGBOX_ENTRY
    WINRT_MSGBOX_ENTRY
    HAIKU_MSGBOX_ENTRY
    PND_MSGBOX_ENTRY
    UIKit_MSGBOX_ENTRY
    Android_MSGBOX_ENTRY
    PS2_MSGBOX_ENTRY
    PSP_MSGBOX_ENTRY
    VITA_MSGBOX_ENTRY
    N3DS_MSGBOX_ENTRY
    KMSDRM_MSGBOX_ENTRY
    RISCOS_MSGBOX_ENTRY
    RPI_MSGBOX_ENTRY
    NACL_MSGBOX_ENTRY
    Emscripten_MSGBOX_ENTRY
    QNX_MSGBOX_ENTRY
    OS2DIVE_MSGBOX_ENTRY
    OS2VMAN_MSGBOX_ENTRY
    NGAGE_MSGBOX_ENTRY
    OFFSCREEN_MSGBOX_ENTRY
    DUMMY_MSGBOX_ENTRY
    DUMMY_evdev_MSGBOX_ENTRY
};
SDL_COMPILE_TIME_ASSERT(messagebox_count, SDL_arraysize(messagebox) == SDL_VIDEODRIVERS_count);
/* The current video driver instance */
static SDL_VideoDevice current_video;

#define TEST_VIDEO(...)                       \
    if (current_video.vdriver_name == NULL) { \
        return __VA_ARGS__;                   \
    }

#define CHECK_VIDEO(...)                      \
    if (current_video.vdriver_name == NULL) { \
        SDL_UninitializedVideo();             \
        return __VA_ARGS__;                   \
    }

#define CHECK_WINDOW_MAGIC(window, retval)                   \
    CHECK_VIDEO(retval)                                      \
    if (!window) {                                           \
        SDL_InvalidParamError("window");                     \
        return retval;                                       \
    }                                                        \
    SDL_assert(window->magic == &current_video.window_magic);

#define CHECK_DISPLAY_INDEX(displayIndex, retval)                         \
    CHECK_VIDEO(retval)                                                   \
    if (displayIndex < 0 || displayIndex >= current_video.num_displays) { \
        SDL_SetError("displayIndex must be in the range 0 - %d",          \
                     current_video.num_displays - 1);                     \
        return retval;                                                    \
    }

#if defined(__MACOSX__) && defined(SDL_VIDEO_DRIVER_COCOA)
/* Support for Mac OS X fullscreen spaces */
extern SDL_bool Cocoa_IsWindowInFullscreenSpace(SDL_Window * window);
extern SDL_bool Cocoa_SetWindowFullscreenSpace(SDL_Window * window, SDL_bool state);
#endif

static Uint32 SDL_DefaultGraphicsBackends(void)
{
#if (defined(SDL_VIDEO_OPENGL) && defined(__MACOSX__)) || (defined(__IPHONEOS__) && !TARGET_OS_MACCATALYST) || defined(__ANDROID__) || defined(__NACL__) || defined(__HAIKU__) || defined(__EMSCRIPTEN__) || defined(__PSP__)
    if (current_video.GL_CreateContext) {
        return SDL_WINDOW_OPENGL;
    }
#endif
#if defined(SDL_VIDEO_METAL) && (TARGET_OS_MACCATALYST || defined(__MACOSX__) || defined(__IPHONEOS__))
    if (current_video.Metal_CreateView) {
        return SDL_WINDOW_METAL;
    }
#endif
    return 0;
}

static SDL_atomic_t SDL_messagebox_count;

/* Convenience functions for driver-specific behavior */
static SDL_bool DisableDisplayModeSwitching(void)
{
#ifdef SDL_VIDEO_DRIVER_WAYLAND
    return SDL_GetVideoDeviceId() == SDL_VIDEODRIVER_Wayland; // VIDEO_DEVICE_QUIRK_DISABLE_DISPLAY_MODE_SWITCHING
#else
    return SDL_FALSE;
#endif
}
static SDL_bool DisableUnsetFullscreenOnMinimize(void)
{
#ifdef SDL_VIDEO_DRIVER_WAYLAND
    return SDL_GetVideoDeviceId() == SDL_VIDEODRIVER_Wayland; // VIDEO_DEVICE_QUIRK_DISABLE_UNSET_FULLSCREEN_ON_MINIMIZE
#else
    return SDL_FALSE;
#endif
}
static SDL_bool IsFullscreenOnly(void) // VIDEO_DEVICE_QUIRK_FULLSCREEN_ONLY
{
#ifdef SDL_VIDEO_DRIVER_RISCOS
    if (SDL_GetVideoDeviceId() == SDL_VIDEODRIVER_RISCOS)
        return SDL_TRUE;
#endif
#ifdef SDL_VIDEO_DRIVER_NACL
    if (SDL_GetVideoDeviceId() == SDL_VIDEODRIVER_NACL)
        return SDL_TRUE;
#endif
    return SDL_FALSE;
}

static void SDL_StartTextInputPrivate(SDL_bool default_value)
{
    SDL_Window *window;

        /* First, enable text events */
        (void)SDL_EventState(SDL_TEXTINPUT, SDL_ENABLE);
        (void)SDL_EventState(SDL_TEXTEDITING, SDL_ENABLE);

        /* Then show the on-screen keyboard, if any */
        if (current_video.ShowScreenKeyboard && SDL_GetHintBoolean(SDL_HINT_ENABLE_SCREEN_KEYBOARD, default_value)) {
            window = SDL_GetFocusWindow();
            if (window) {
                current_video.ShowScreenKeyboard(window);
            }
        }

        /* Finally start the text input system */
        if (current_video.StartTextInput) {
            current_video.StartTextInput();
        }
}

static int SDLCALL cmpmodes(const void *A, const void *B)
{
    const SDL_DisplayMode *a = (const SDL_DisplayMode *)A;
    const SDL_DisplayMode *b = (const SDL_DisplayMode *)B;
    if (a == b) {
        return 0;
    } else if (a->w != b->w) {
        return b->w - a->w;
    } else if (a->h != b->h) {
        return b->h - a->h;
    } else if (a->format != b->format) {
        SDL_assert(SDL_PIXELFLAG(a->format) && SDL_PIXELFLAG(b->format));
        if (SDL_PIXELTYPE(a->format) != SDL_PIXELTYPE(b->format)) {
            return SDL_PIXELTYPE(b->format) - SDL_PIXELTYPE(a->format);
        }
        if (SDL_PIXELLAYOUT(a->format) != SDL_PIXELLAYOUT(b->format)) {
            return SDL_PIXELLAYOUT(b->format) - SDL_PIXELLAYOUT(a->format);
        }
        SDL_assert(SDL_PIXELBPP(a->format) == SDL_PIXELBPP(b->format));
        SDL_assert(SDL_BITSPERPIXEL(a->format) == SDL_BITSPERPIXEL(b->format));

        SDL_assert(SDL_PIXELORDER(a->format) != SDL_PIXELORDER(b->format));
        // if (SDL_PIXELORDER(a->format) != SDL_PIXELORDER(b->format)) {
            return SDL_PIXELORDER(b->format) - SDL_PIXELORDER(a->format);
        // }
    } else { // if (a->refresh_rate != b->refresh_rate) {
        return b->refresh_rate - a->refresh_rate;
    }
    // return 0;
}

static int SDL_UninitializedVideo(void)
{
    return SDL_SetError("Video subsystem has not been initialized");
}

int SDL_GetNumVideoDrivers(void)
{
    return SDL_arraysize(bootstrap);
}

const char *SDL_GetVideoDriver(int index)
{
    if (index >= 0 && index < SDL_arraysize(bootstrap)) {
        return bootstrap[index]->name;
    }
    return NULL;
}

/*
 * Initialize the video and event subsystems -- determine native pixel format
 */
int SDL_VideoInit(const char *driver_name)
{
    SDL_bool init_events = SDL_FALSE;
    SDL_bool init_keyboard = SDL_FALSE;
    SDL_bool init_mouse = SDL_FALSE;
    SDL_bool init_touch = SDL_FALSE;
    int i;
    SDL_bool tried_to_init = SDL_FALSE;

    /* Check to make sure we don't overwrite current_video */
    if (SDL_HasVideoDevice()) {
        SDL_VideoQuit();
    }

#ifndef SDL_TIMERS_DISABLED
    SDL_TicksInit();
#endif

    /* Start the event loop */
    if (SDL_InitSubSystem(SDL_INIT_EVENTS) < 0) {
        goto pre_driver_error;
    }
    init_events = SDL_TRUE;
    if (SDL_KeyboardInit() < 0) {
        goto pre_driver_error;
    }
    init_keyboard = SDL_TRUE;
    if (SDL_MouseInit() < 0) {
        goto pre_driver_error;
    }
    init_mouse = SDL_TRUE;
    if (SDL_TouchInit() < 0) {
        goto pre_driver_error;
    }
    init_touch = SDL_TRUE;

    /* Select the proper video driver */
    if (!driver_name) {
        driver_name = SDL_GetHint(SDL_HINT_VIDEODRIVER);
    }
#if defined(__LINUX__) && defined(SDL_VIDEO_DRIVER_X11)
    if (!driver_name) {
        /* See if it looks like we need X11 */
        SDL_bool force_x11 = SDL_FALSE;
        void *global_symbols = dlopen(NULL, RTLD_LOCAL|RTLD_NOW);

        /* Use linked libraries to detect what quirks we are likely to need */
        if (global_symbols != NULL) {
            if (dlsym(global_symbols, "glxewInit") != NULL) {  /* GLEW (e.g. Frogatto, SLUDGE) */
                force_x11 = SDL_TRUE;
            } else if (dlsym(global_symbols, "cgGLEnableProgramProfiles") != NULL) {  /* NVIDIA Cg (e.g. Awesomenauts, Braid) */
                force_x11 = SDL_TRUE;
            } else if (dlsym(global_symbols, "_Z7ssgInitv") != NULL) {  /* ::ssgInit(void) in plib (e.g. crrcsim) */
                force_x11 = SDL_TRUE;
            }
            dlclose(global_symbols);
        }
        if (force_x11) {
            driver_name = "x11";
        }
    }
#endif
    if (driver_name && *driver_name != '\0') {
        const char *driver_attempt = driver_name;
        while (1) {
            const char *driver_attempt_end = SDL_strchr(driver_attempt, ',');
            size_t driver_attempt_len = (driver_attempt_end) ? (driver_attempt_end - driver_attempt)
                                                                     : SDL_strlen(driver_attempt);

            for (i = 0; i < SDL_arraysize(bootstrap); ++i) {
                if ((driver_attempt_len == SDL_strlen(bootstrap[i]->name)) &&
                    (SDL_strncasecmp(bootstrap[i]->name, driver_attempt, driver_attempt_len) == 0)) {
                    tried_to_init = SDL_TRUE;
                    if (bootstrap[i]->create(&current_video)) {
                        break;
                    }
                }
            }
            if (i < SDL_arraysize(bootstrap)) {
                break;
            }
            if (!driver_attempt_end) {
                /* specific drivers will set the error message if they fail... */
                if (!tried_to_init) {
                    SDL_SetError("%s not available", driver_name);
                }
                goto pre_driver_error;
            }
            driver_attempt = driver_attempt_end + 1;
        }
    } else {
        for (i = 0; i < SDL_arraysize(bootstrap); ++i) {
#ifdef SDL_VIDEO_DRIVER_DUMMY
            if (bootstrap[i] == &DUMMY_bootstrap) {
                continue;
            }
#ifdef SDL_INPUT_LINUXEV
            if (bootstrap[i] == &DUMMY_evdev_bootstrap) {
                continue;
            }
#endif
#endif
#ifdef SDL_VIDEO_DRIVER_OFFSCREEN
            if (bootstrap[i] == &OFFSCREEN_bootstrap) {
                continue;
            }
#endif
            tried_to_init = SDL_TRUE;
            if (bootstrap[i]->create(&current_video)) {
                break;
            }
        }
        if (i >= SDL_arraysize(bootstrap)) {
            /* specific drivers will set the error message if they fail... */
            if (!tried_to_init) {
                SDL_SetError("No available video device");
            }
            goto pre_driver_error;
        }
    }

    /* From this point on, use SDL_VideoQuit to cleanup on error, rather than
    pre_driver_error. */
    current_video.vdriver_index = i;
    current_video.vdriver_name = bootstrap[i]->name;
    current_video.next_object_id = 1;
    current_video.thread = SDL_ThreadID();
    /* Validate the interface */
    SDL_assert(current_video.CreateSDLWindow != NULL);
    SDL_assert(current_video.PumpEvents != NULL);
    SDL_assert((current_video.SendWakeupEvent == NULL && current_video.WaitEventTimeout == NULL) || current_video.wakeup_lock != NULL);
    SDL_assert((current_video.CreateShaper == NULL) == (current_video.SetWindowShape == NULL));
    SDL_assert((current_video.CreateWindowFramebuffer == NULL) == (current_video.UpdateWindowFramebuffer == NULL) && (current_video.CreateWindowFramebuffer == NULL) == (current_video.DestroyWindowFramebuffer == NULL));
    SDL_assert((current_video.SetClipboardText == NULL) == (current_video.GetClipboardText == NULL) && (current_video.SetClipboardText == NULL) == (current_video.HasClipboardText == NULL));
    SDL_assert((current_video.SetPrimarySelectionText == NULL) == (current_video.GetPrimarySelectionText == NULL) && (current_video.SetPrimarySelectionText == NULL) == (current_video.HasPrimarySelectionText == NULL));
#ifdef SDL_VIDEO_METAL
    SDL_assert((current_video.Metal_CreateView == NULL) == (current_video.Metal_DestroyView == NULL) && (current_video.Metal_CreateView == NULL) == (current_video.Metal_GetLayer == NULL));
    SDL_assert((current_video.Metal_CreateView == NULL) == (current_video.Metal_GetDrawableSize == NULL));
#endif
#ifdef SDL_VIDEO_VULKAN
    SDL_assert((current_video.Vulkan_LoadLibrary == NULL) == (current_video.Vulkan_UnloadLibrary == NULL) && (current_video.Vulkan_LoadLibrary == NULL) == (current_video.Vulkan_GetInstanceExtensions == NULL));
    SDL_assert((current_video.Vulkan_LoadLibrary == NULL) == (current_video.Vulkan_CreateSurface == NULL) && (current_video.Vulkan_LoadLibrary == NULL) == (current_video.Vulkan_GetDrawableSize == NULL));
#endif
    SDL_assert(current_video.DestroyWindow != NULL);
    SDL_assert(current_video.DeleteDevice != NULL);
#ifdef SDL_VIDEO_OPENGL_ANY
    /* Set some very sane GL defaults */
    // current_video.gl_config.driver_loaded = 0; -- do not reset. It might be already set (e.g. Vita with PIB) otherwise it should be zero anyway.
    SDL_GL_ResetAttributes();

    current_video.current_glwin_tls = SDL_TLSCreate();
    current_video.current_glctx_tls = SDL_TLSCreate();
#endif
    /* Initialize the video subsystem */
    if (current_video.VideoInit(&current_video) < 0) {
        SDL_VideoQuit();
        return -1;
    }

    /* Make sure some displays were added */
    if (current_video.num_displays == 0) {
        SDL_VideoQuit();
        return SDL_SetError("The SDL video driver did not add any displays");
    }

    /* Disable the screen saver by default. This is a change from <= 2.0.1,
       but most things using SDL are games or media players; you wouldn't
       want a screensaver to trigger if you're playing exclusively with a
       joystick, or passively watching a movie. Things that use SDL but
       function more like a normal desktop app should explicitly reenable the
       screensaver. */
    if (!SDL_GetHintBoolean(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, SDL_FALSE)) {
        SDL_DisableScreenSaver();
    }

#if !defined(SDL_VIDEO_DRIVER_N3DS)
    /* In the initial state we don't want to pop up an on-screen keyboard,
     * but we do want to allow text input from other mechanisms.
     */
    SDL_StartTextInputPrivate(SDL_FALSE);
#endif /* !SDL_VIDEO_DRIVER_N3DS */

    /* We're ready to go! */
    return 0;

pre_driver_error:
    SDL_assert(!SDL_HasVideoDevice());
    if (init_touch) {
        SDL_TouchQuit();
    }
    if (init_mouse) {
        SDL_MouseQuit();
    }
    if (init_keyboard) {
        SDL_KeyboardQuit();
    }
    if (init_events) {
        SDL_QuitSubSystem(SDL_INIT_EVENTS);
    }
    return -1;
}

const char *SDL_GetCurrentVideoDriver(void)
{
    CHECK_VIDEO(NULL)

    return current_video.vdriver_name;
}

SDL_bool SDL_HasVideoDevice(void)
{
    TEST_VIDEO(SDL_FALSE)
    return SDL_TRUE;
}

int SDL_GetVideoDeviceId(void)
{
    SDL_assert(SDL_HasVideoDevice());

    return SDL_VIDEODRIVERS_count == 1 ? 0 : current_video.vdriver_index;
}

SDL_VideoDevice *SDL_GetVideoDevice(void)
{
    return &current_video;
}

void SDL_PrivatePumpEvents(void)
{
    TEST_VIDEO( )
    SDL_assert(current_video.PumpEvents != NULL);

    current_video.PumpEvents();
}

SDL_bool SDL_HasWaitTimeoutSupport(void)
{
    // TEST_VIDEO(SDL_FALSE)
    return current_video.SendWakeupEvent != NULL /*&& current_video.WaitEventTimeout != NULL*/;
}

void SDL_PrivateSendWakeupEvent(SDL_Window *window)
{
    SDL_assert(SDL_HasVideoDevice());
    SDL_assert(current_video.SendWakeupEvent != NULL);
    current_video.SendWakeupEvent(window);
}

int SDL_PrivateWaitEventTimeout(int timeout)
{
    SDL_assert(SDL_HasVideoDevice());
    SDL_assert(current_video.WaitEventTimeout != NULL);
    return current_video.WaitEventTimeout(timeout);
}

SDL_bool SDL_OnVideoThread(void)
{
    TEST_VIDEO(SDL_FALSE)
    return SDL_ThreadID() == current_video.thread;
}

static int SDL_CalculateWindowDisplayIndex(SDL_Window *window);
int SDL_AddVideoDisplay(const SDL_VideoDisplay *display, SDL_bool send_event)
{
    SDL_VideoDisplay *displays;
    SDL_Window *window;
    int index;

    displays =
        SDL_realloc(current_video.displays,
                    (current_video.num_displays + 1) * sizeof(*displays));
    if (displays) {
        index = current_video.num_displays++;
        displays[index] = *display;
        current_video.displays = displays;

        if (display->name) {
            displays[index].name = SDL_strdup(display->name);
        } else {
            char name[32];

            SDL_itoa(index, name, 10);
            displays[index].name = SDL_strdup(name);
        }

        for (window = current_video.windows; window; window = window->next) {
            window->display_index = SDL_CalculateWindowDisplayIndex(window);
        }

        if (send_event) {
            SDL_SendDisplayEvent(&current_video.displays[index], SDL_DISPLAYEVENT_CONNECTED, 0);
        }
    } else {
        index = SDL_OutOfMemory();
    }
    return index;
}

void SDL_PrivateResetDisplayModes(SDL_VideoDisplay *display)
{
    int i;

    for (i = display->num_display_modes; i--;) {
        SDL_free(display->display_modes[i].driverdata);
        display->display_modes[i].driverdata = NULL;
    }
    SDL_free(display->display_modes);
    display->display_modes = NULL;
    display->num_display_modes = 0;
    display->max_display_modes = 0;
}

void SDL_DelVideoDisplay(int index)
{
    SDL_VideoDisplay *display;
    SDL_Window *window;
    SDL_assert(index >= 0 && index < current_video.num_displays);

    display = &current_video.displays[index];
    SDL_SendDisplayEvent(display, SDL_DISPLAYEVENT_DISCONNECTED, 0);

    SDL_PrivateResetDisplayModes(display);
    SDL_free(display->driverdata);
    SDL_free(display->name);
    if (index < (current_video.num_displays - 1)) {
        SDL_memmove(display, display + 1, (current_video.num_displays - index - 1) * sizeof(*display));
    }
    --current_video.num_displays;

    for (window = current_video.windows; window; window = window->next) {
        window->display_index = SDL_CalculateWindowDisplayIndex(window);
    }
}

int SDL_GetNumVideoDisplays(void)
{
    CHECK_VIDEO(0)

    return current_video.num_displays;
}

SDL_VideoDisplay *SDL_GetDisplays(int *num_displays)
{
    SDL_assert(SDL_HasVideoDevice());
    SDL_assert(num_displays != NULL);
    *num_displays = current_video.num_displays;
    return &current_video.displays[0];
}

int SDL_GetIndexOfDisplay(SDL_VideoDisplay *display)
{
    int displayIndex;
    SDL_assert(SDL_HasVideoDevice());

    for (displayIndex = 0; displayIndex < current_video.num_displays; ++displayIndex) {
        if (display == &current_video.displays[displayIndex]) {
            return displayIndex;
        }
    }

    /* Couldn't find the display, just use index 0 */
    return 0;
}

void *SDL_GetDisplayDriverData(int displayIndex)
{
    CHECK_DISPLAY_INDEX(displayIndex, NULL);

    return current_video.displays[displayIndex].driverdata;
}

void *SDL_GetWindowDisplayDriverData(SDL_Window *window)
{
    int displayIndex;
    void *data = NULL;

    SDL_assert(window != NULL);

    displayIndex = window->display_index;

    if (displayIndex >= 0 && displayIndex < current_video.num_displays) {
        data = current_video.displays[displayIndex].driverdata;
    }

    return data;
}

SDL_bool SDL_IsVideoContextExternal(void)
{
    return SDL_GetHintBoolean(SDL_HINT_VIDEO_EXTERNAL_CONTEXT, SDL_FALSE);
}

const char *SDL_GetDisplayName(int displayIndex)
{
    CHECK_DISPLAY_INDEX(displayIndex, NULL);

    return current_video.displays[displayIndex].name;
}

int SDL_GetDisplayBounds(int displayIndex, SDL_Rect *rect)
{
    SDL_VideoDisplay *display;

    CHECK_DISPLAY_INDEX(displayIndex, -1);

    if (!rect) {
        return SDL_InvalidParamError("rect");
    }

    display = &current_video.displays[displayIndex];
    SDL_PrivateGetDisplayBounds(display, rect);
    return 0;
}

void SDL_PrivateGetDisplayBounds(SDL_VideoDisplay *display, SDL_Rect *rect)
{
    SDL_assert(SDL_HasVideoDevice());
    SDL_assert(display >= &current_video.displays[0] && display < &current_video.displays[current_video.num_displays]);
    SDL_assert(rect != NULL);

    if (current_video.GetDisplayBounds) {
        if (current_video.GetDisplayBounds(display, rect) == 0) {
            return;
        }
    }

    /* Assume that the displays are left to right */
    rect->x = 0;
    rect->y = 0;
    rect->w = display->current_mode.w;
    rect->h = display->current_mode.h;
    while (display > &current_video.displays[0]) {
        display--;
        rect->x += display->current_mode.w;
    }
}

static int ParseDisplayUsableBoundsHint(SDL_Rect *rect)
{
    const char *hint = SDL_GetHint(SDL_HINT_DISPLAY_USABLE_BOUNDS);
    return hint && (SDL_sscanf(hint, "%d,%d,%d,%d", &rect->x, &rect->y, &rect->w, &rect->h) == 4);
}

int SDL_GetDisplayUsableBounds(int displayIndex, SDL_Rect *rect)
{
    SDL_VideoDisplay *display;

    CHECK_DISPLAY_INDEX(displayIndex, -1);

    if (!rect) {
        return SDL_InvalidParamError("rect");
    }

    display = &current_video.displays[displayIndex];

    if ((displayIndex == 0) && ParseDisplayUsableBoundsHint(rect)) {
        return 0;
    }

    if (current_video.GetDisplayUsableBounds) {
        if (current_video.GetDisplayUsableBounds(display, rect) == 0) {
            return 0;
        }
    }

    /* Oh well, just give the entire display bounds. */
    SDL_PrivateGetDisplayBounds(display, rect);
    return 0;
}

int SDL_GetDisplayDPI(int displayIndex, float *ddpi, float *hdpi, float *vdpi)
{
    SDL_VideoDisplay *display;

    CHECK_DISPLAY_INDEX(displayIndex, -1);

    display = &current_video.displays[displayIndex];

    if (current_video.GetDisplayDPI) {
        if (current_video.GetDisplayDPI(display, ddpi, hdpi, vdpi) == 0) {
            return 0;
        }
    } else {
        return SDL_Unsupported();
    }

    return -1;
}

SDL_DisplayOrientation SDL_GetDisplayOrientation(int displayIndex)
{
    SDL_VideoDisplay *display;

    CHECK_DISPLAY_INDEX(displayIndex, SDL_ORIENTATION_UNKNOWN);

    display = &current_video.displays[displayIndex];
    return display->orientation;
}

SDL_bool SDL_AddDisplayMode(SDL_VideoDisplay *display, const SDL_DisplayMode *mode)
{
    SDL_DisplayMode *modes;
    int i, nmodes;

    /* Make sure we don't already have the mode in the list */
    modes = display->display_modes;
    nmodes = display->num_display_modes;
    for (i = 0; i < nmodes; ++i) {
        if (cmpmodes(mode, &modes[i]) == 0) {
            return SDL_FALSE;
        }
    }

    /* Go ahead and add the new mode */
    if (nmodes == display->max_display_modes) {
        modes =
            SDL_realloc(modes,
                        (display->max_display_modes + 32) * sizeof(*modes));
        if (!modes) {
            return SDL_FALSE;
        }
        display->display_modes = modes;
        display->max_display_modes += 32;
    }
    modes[nmodes] = *mode;
    display->num_display_modes++;

    /* Re-sort video modes */
    SDL_qsort(display->display_modes, display->num_display_modes,
              sizeof(SDL_DisplayMode), cmpmodes);

    return SDL_TRUE;
}

static int SDL_GetNumDisplayModesForDisplay(SDL_VideoDisplay *display)
{
    return display->num_display_modes;
}

int SDL_GetNumDisplayModes(int displayIndex)
{
    CHECK_DISPLAY_INDEX(displayIndex, -1);

    return SDL_GetNumDisplayModesForDisplay(&current_video.displays[displayIndex]);
}

void SDL_ResetDisplayModes(int displayIndex)
{
    CHECK_DISPLAY_INDEX(displayIndex, );

    SDL_PrivateResetDisplayModes(&current_video.displays[displayIndex]);
}

int SDL_GetDisplayMode(int displayIndex, int index, SDL_DisplayMode *mode)
{
    SDL_VideoDisplay *display;

    CHECK_DISPLAY_INDEX(displayIndex, -1);

    display = &current_video.displays[displayIndex];
    if (index < 0 || index >= SDL_GetNumDisplayModesForDisplay(display)) {
        return SDL_SetError("index must be in the range of 0 - %d", SDL_GetNumDisplayModesForDisplay(display) - 1);
    }
    if (mode) {
        *mode = display->display_modes[index];
    }
    return 0;
}

int SDL_GetDesktopDisplayMode(int displayIndex, SDL_DisplayMode *mode)
{
    SDL_VideoDisplay *display;

    CHECK_DISPLAY_INDEX(displayIndex, -1);

    display = &current_video.displays[displayIndex];
    if (mode) {
        *mode = display->desktop_mode;
    }
    return 0;
}

int SDL_GetCurrentDisplayMode(int displayIndex, SDL_DisplayMode *mode)
{
    SDL_VideoDisplay *display;

    CHECK_DISPLAY_INDEX(displayIndex, -1);

    display = &current_video.displays[displayIndex];
    if (mode) {
        *mode = display->current_mode;
    }
    return 0;
}

static SDL_DisplayMode *SDL_GetClosestDisplayModeForDisplay(SDL_VideoDisplay *display,
                                                            const SDL_DisplayMode *mode,
                                                            SDL_DisplayMode *closest)
{
    Uint32 target_format;
    int target_refresh_rate;
    int i, num_display_modes;
    SDL_DisplayMode *current, *match;

    SDL_assert(mode != NULL && closest != NULL);

    /* Default to the desktop format */
    target_format = mode->format;
    if (target_format == SDL_PIXELFORMAT_UNKNOWN) {
        target_format = display->desktop_mode.format;
    }

    /* Default to the desktop refresh rate */
    target_refresh_rate = mode->refresh_rate;
    if (!target_refresh_rate) {
        target_refresh_rate = display->desktop_mode.refresh_rate;
    }

    num_display_modes = SDL_GetNumDisplayModesForDisplay(display);
    match = NULL;
    for (i = 0; i < num_display_modes; ++i) {
        current = &display->display_modes[i];

        if (current->w && (current->w < mode->w)) {
            /* Out of sorted modes large enough here */
            break;
        }
        if (current->h && (current->h < mode->h)) {
            // if (current->w && (current->w == mode->w)) {
            //    /* Out of sorted modes large enough here */
            //    break;
            // }
            /* Wider, but not tall enough, due to a different
               aspect ratio. This mode must be skipped, but closer
               modes may still follow. */
            continue;
        }
        if (!match || current->w < match->w || current->h < match->h) {
            match = current;
            continue;
        }
        SDL_assert(current->w == match->w && current->h == match->h);
        // matching dimensions -> try to choose based on the format
        if (current->format != match->format) {
            int mw = 0, cw = 0;
            if (SDL_PIXELTYPE(match->format) == SDL_PIXELTYPE(target_format)) {
                mw |= 4;
            }
            if (SDL_PIXELTYPE(current->format) == SDL_PIXELTYPE(target_format)) {
                cw |= 4;
            }
            if (SDL_PIXELLAYOUT(match->format) == SDL_PIXELLAYOUT(target_format)) {
                mw |= 2;
            }
            if (SDL_PIXELLAYOUT(current->format) == SDL_PIXELLAYOUT(target_format)) {
                cw |= 2;
            }
            if (SDL_PIXELORDER(match->format) == SDL_PIXELORDER(target_format)) {
                mw |= 1;
            }
            if (SDL_PIXELORDER(current->format) == SDL_PIXELORDER(target_format)) {
                cw |= 1;
            }
            if (cw > mw) {
                match = current;
            }
            continue;
        }
        // everything is the same except for the refresh rate -> select the closest match
        SDL_assert(current->refresh_rate != match->refresh_rate);
        {
            /* Sorted highest refresh to lowest */
            if (current->refresh_rate >= target_refresh_rate) {
                match = current;
            }
        }
    }
    if (match) {
        target_format = match->format;
        if (target_format == SDL_PIXELFORMAT_UNKNOWN) {
            target_format = mode->format;
        }
        closest->format = target_format;
        i = match->w;
        if (!i) {
            i = mode->w;
        }
        closest->w = i;
        i = match->h;
        if (!i) {
            i = mode->h;
        }
        closest->h = i;
        target_refresh_rate = match->refresh_rate;
        if (!target_refresh_rate) {
            target_refresh_rate = mode->refresh_rate;
        }
        closest->refresh_rate = target_refresh_rate;
        closest->driverdata = match->driverdata;

        /*
         * Pick some reasonable defaults if the app and driver don't
         * care
         */
        if (!closest->format) {
            closest->format = SDL_PIXELFORMAT_RGB888;
        }
        if (!closest->w) {
            closest->w = 640;
        }
        if (!closest->h) {
            closest->h = 480;
        }
        match = closest;
    }
    return match;
}

SDL_DisplayMode *SDL_GetClosestDisplayMode(int displayIndex,
                          const SDL_DisplayMode *mode,
                          SDL_DisplayMode *closest)
{
    SDL_VideoDisplay *display;

    CHECK_DISPLAY_INDEX(displayIndex, NULL);

    display = &current_video.displays[displayIndex];

    if (!mode || !closest) {
        SDL_InvalidParamError("mode/closest");
        return NULL;
    }

    return SDL_GetClosestDisplayModeForDisplay(display, mode, closest);
}

static int SDL_SetDisplayModeForDisplay(SDL_VideoDisplay *display, const SDL_DisplayMode *mode)
{
    SDL_DisplayMode display_mode;
    SDL_DisplayMode current_mode;
    int result;

    SDL_assert(SDL_HasVideoDevice());

    /* Mode switching disabled via driver quirk flag, nothing to do and cannot fail. */
    if (DisableDisplayModeSwitching()) {
        return 0;
    }

    if (mode) {
        display_mode = *mode;

        /* Default to the current mode */
        if (display_mode.format == SDL_PIXELFORMAT_UNKNOWN) {
            display_mode.format = display->current_mode.format;
        }
        if (!display_mode.w) {
            display_mode.w = display->current_mode.w;
        }
        if (!display_mode.h) {
            display_mode.h = display->current_mode.h;
        }
        if (!display_mode.refresh_rate) {
            display_mode.refresh_rate = display->current_mode.refresh_rate;
        }

        /* Get a good video mode, the closest one possible */
        if (!SDL_GetClosestDisplayModeForDisplay(display, &display_mode, &display_mode)) {
            return SDL_SetError("No video mode large enough for %dx%d", display_mode.w, display_mode.h);
        }
    } else {
        display_mode = display->desktop_mode;
    }

    /* See if there's anything left to do */
    current_mode = display->current_mode;
    if (SDL_memcmp(&display_mode, &current_mode, sizeof(display_mode)) == 0) {
        return 0;
    }

    /* Actually change the display mode */
    if (!current_video.SetDisplayMode) {
        return SDL_SetError("SDL video driver doesn't support changing display mode");
    }
    result = current_video.SetDisplayMode(display, &display_mode);
    if (result < 0) {
        return result;
    }
    display->current_mode = display_mode;
    return 0;
}

SDL_VideoDisplay *SDL_GetDisplay(int displayIndex)
{
    CHECK_DISPLAY_INDEX(displayIndex, NULL);

    return &current_video.displays[displayIndex];
}

/**
 * Get the distance between the point and the rectangle. If the point is inside
 * the rectangle, the distance is zero, otherwise the result is the
 * Manhattan-distance to avoid overflow.
 */
static int SDL_GetDistanceFromRect(const SDL_Point *point, const SDL_Rect *rect)
{
    const int right = rect->x + rect->w - 1;
    const int bottom = rect->y + rect->h - 1;
    int dx, dy;

    dx = rect->x - point->x;
    if (dx < 0) {
        dx = point->x - right;
        if (dx < 0) {
            dx = 0;
        }
    }

    dy = rect->y - point->y;
    if (dy < 0) {
        dy = point->y - bottom;
        if (dy < 0) {
            dy = 0;
        }
    }

    return dx + dy;
}

static int GetPointDisplayIndex(int x, int y, int w, int h)
{
    int i, dist;
    int closest = -1;
    int closest_dist = 0x7FFFFFFF;
    SDL_Point center;

    center.x = x + (w >> 1);
    center.y = y + (h >> 1);

    i = current_video.num_displays - 1;
    if (i <= 0) {
        if (i != 0)
            SDL_SetError("Couldn't find any displays");
        return i;
    }

    // if (SDL_HasVideoDevice()) {
        for ( ; i >= 0; i--) {
            SDL_Rect display_rect;
            SDL_VideoDisplay *display = &current_video.displays[i];
            SDL_PrivateGetDisplayBounds(display, &display_rect);

            dist = SDL_GetDistanceFromRect(&center, &display_rect);
            if (dist < closest_dist) {
                closest = i;
                closest_dist = dist;
            }
        }
    // }

    SDL_assert(closest >= 0);
    return closest;
}

int SDL_GetPointDisplayIndex(const SDL_Point *point)
{
    if (!point) {
        return SDL_InvalidParamError("point");
    }
    return GetPointDisplayIndex(point->x, point->y, 0, 0);
}

int SDL_GetRectDisplayIndex(const SDL_Rect *rect)
{
    if (!rect) {
        return SDL_InvalidParamError("rect");
    }
    return GetPointDisplayIndex(rect->x, rect->y, rect->w, rect->h);
}

static int SDL_CalculateWindowDisplayIndex(SDL_Window *window)
{
    int displayIndex = -1;

    SDL_assert(window != NULL);
#if 0
    if (current_video.GetWindowDisplayIndex) {
        displayIndex = current_video.GetWindowDisplayIndex(window);
    }
#endif
    /* A backend implementation may fail to get a display index for the window
     * (for example if the window is off-screen), but other code may expect it
     * to succeed in that situation, so we fall back to a generic position-
     * based implementation in that case. */
    if (displayIndex < 0) {
        displayIndex = GetPointDisplayIndex(window->wrect.x, window->wrect.y, window->wrect.w, window->wrect.h);
    }
    return displayIndex;
}

int SDL_GetWindowDisplayIndex(SDL_Window *window)
{
    CHECK_WINDOW_MAGIC(window, -1);

    return window->display_index;
}

SDL_VideoDisplay *SDL_GetDisplayForWindow(SDL_Window *window)
{
    int displayIndex = window->display_index;
    if (displayIndex >= 0) {
        return &current_video.displays[displayIndex];
    } else {
        return NULL;
    }
}

int SDL_SetWindowDisplayMode(SDL_Window *window, const SDL_DisplayMode *mode)
{
    CHECK_WINDOW_MAGIC(window, -1);

    if (mode) {
        window->fullscreen_mode = *mode;
    } else {
        SDL_zero(window->fullscreen_mode);
    }

    if (FULLSCREEN_VISIBLE(window) && (window->flags & SDL_WINDOW_FULLSCREEN_MASK) == SDL_WINDOW_FULLSCREEN) {
        SDL_DisplayMode fullscreen_mode;
        if (SDL_GetWindowDisplayMode(window, &fullscreen_mode) == 0) {
            if (SDL_SetDisplayModeForDisplay(SDL_GetDisplayForWindow(window), &fullscreen_mode) == 0) {
#ifndef __ANDROID__
                /* Android may not resize the window to exactly what our fullscreen mode is, especially on
                 * windowed Android environments like the Chromebook or Samsung DeX.  Given this, we shouldn't
                 * use fullscreen_mode.w and fullscreen_mode.h, but rather get our current native size.  As such,
                 * Android's SetWindowFullscreen will generate the window event for us with the proper final size.
                 */
                SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESIZED, fullscreen_mode.w, fullscreen_mode.h);
#endif
            }
        }
    }
    return 0;
}

int SDL_GetWindowDisplayMode(SDL_Window *window, SDL_DisplayMode *mode)
{
    SDL_DisplayMode fullscreen_mode;
    SDL_VideoDisplay *display;

    CHECK_WINDOW_MAGIC(window, -1);

    if (!mode) {
        return SDL_InvalidParamError("mode");
    }

    display = SDL_GetDisplayForWindow(window);

    /* if in desktop size mode, just return the size of the desktop */
    if ((window->flags & SDL_WINDOW_FULLSCREEN_MASK) == SDL_WINDOW_FULLSCREEN_DESKTOP) {
        fullscreen_mode = display->desktop_mode;
    } else {
        fullscreen_mode = window->fullscreen_mode;
        if (!fullscreen_mode.w) {
            fullscreen_mode.w = window->wrect.w;
        }
        if (!fullscreen_mode.h) {
            fullscreen_mode.h = window->wrect.h;
        }        
        if (!SDL_GetClosestDisplayModeForDisplay(display,
                                                    &fullscreen_mode,
                                                    &fullscreen_mode)) {
            return SDL_SetError("Couldn't find display mode match");
        }
    }

    *mode = fullscreen_mode;

    return 0;
}

void *SDL_GetWindowICCProfile(SDL_Window *window, size_t *size)
{
    CHECK_WINDOW_MAGIC(window, NULL);

    if (!current_video.GetWindowICCProfile) {
        SDL_Unsupported();
        return NULL;
    }
    return current_video.GetWindowICCProfile(window, size);
}

Uint32 SDL_GetWindowPixelFormat(SDL_Window *window)
{
    SDL_VideoDisplay *display;

    CHECK_WINDOW_MAGIC(window, SDL_PIXELFORMAT_UNKNOWN);

    display = SDL_GetDisplayForWindow(window);
    return display->current_mode.format;
}

static void SDL_RestoreMousePosition(SDL_Window *window)
{
    int x, y;

    if (window == SDL_GetMouseFocus()) {
        SDL_GetMouseState(&x, &y);
        SDL_WarpMouseInWindow(window, x, y);
    }
}

#ifdef __WINRT__
extern Uint32 WINRT_DetectWindowFlags(SDL_Window *window);
#endif

static int SDL_UpdateFullscreenMode(SDL_Window *window, SDL_bool fullscreen)
{
    SDL_VideoDisplay *display;
    SDL_Window *other;
    SDL_bool resized = SDL_FALSE;

    SDL_assert(window != NULL);

    /* if we are in the process of hiding don't go back to fullscreen */
    if (window->is_hiding && fullscreen) {
        return 0;
    }

#if defined(__MACOSX__) && defined(SDL_VIDEO_DRIVER_COCOA)
    /* if the window is going away and no resolution change is necessary,
       do nothing, or else we may trigger an ugly double-transition
     */
    if (SDL_GetVideoDeviceId() == SDL_VIDEODRIVER_COCOA) { /* don't do this for X11, etc */
        if (window->is_destroying && (window->last_fullscreen_flags & SDL_WINDOW_FULLSCREEN_MASK) == SDL_WINDOW_FULLSCREEN_DESKTOP) {
            return 0;
        }

        /* If we're switching between a fullscreen Space and "normal" fullscreen, we need to get back to normal first. */
        if (fullscreen && ((window->last_fullscreen_flags & SDL_WINDOW_FULLSCREEN_MASK) == SDL_WINDOW_FULLSCREEN_DESKTOP) && ((window->flags & SDL_WINDOW_FULLSCREEN_MASK) == SDL_WINDOW_FULLSCREEN)) {
            if (!Cocoa_SetWindowFullscreenSpace(window, SDL_FALSE)) {
                return -1;
            }
        } else if (fullscreen && ((window->last_fullscreen_flags & SDL_WINDOW_FULLSCREEN_MASK) == SDL_WINDOW_FULLSCREEN) && ((window->flags & SDL_WINDOW_FULLSCREEN_MASK) == SDL_WINDOW_FULLSCREEN_DESKTOP)) {
            display = SDL_GetDisplayForWindow(window);
            SDL_SetDisplayModeForDisplay(display, NULL);
            if (current_video.SetWindowFullscreen) {
                current_video.SetWindowFullscreen(window, display, SDL_FALSE);
            }
        }

        if (Cocoa_SetWindowFullscreenSpace(window, fullscreen)) {
            if (Cocoa_IsWindowInFullscreenSpace(window) != fullscreen) {
                return -1;
            }
            window->last_fullscreen_flags = window->flags;
            return 0;
        }
    }
#elif defined(__WINRT__) && (NTDDI_VERSION < NTDDI_WIN10)
    /* HACK: WinRT 8.x apps can't choose whether or not they are fullscreen
       or not.  The user can choose this, via OS-provided UI, but this can't
       be set programmatically.

       Just look at what SDL's WinRT video backend code detected with regards
       to fullscreen (being active, or not), and figure out a return/error code
       from that.
    */
    if (fullscreen == !(WINRT_DetectWindowFlags(window) & SDL_WINDOW_FULLSCREEN)) {
        /* Uh oh, either:
            1. fullscreen was requested, and we're already windowed
            2. windowed-mode was requested, and we're already fullscreen

            WinRT 8.x can't resolve either programmatically, so we're
            giving up.
        */
        return -1;
    } else {
        /* Whatever was requested, fullscreen or windowed mode, is already
            in-place.
        */
        return 0;
    }
#endif

    display = SDL_GetDisplayForWindow(window);
    if (!display) { /* No display connected, nothing to do. */
        return 0;
    }

    if (fullscreen) {
        /* Hide any other fullscreen windows */
        if (display->fullscreen_window &&
            display->fullscreen_window != window) {
            SDL_MinimizeWindow(display->fullscreen_window);
        }
    }

    /* See if anything needs to be done now */
    if ((display->fullscreen_window == window) == fullscreen) {
        if ((window->last_fullscreen_flags & SDL_WINDOW_FULLSCREEN_MASK) == (window->flags & SDL_WINDOW_FULLSCREEN_DESKTOP)) {
            return 0;
        }
        if (!fullscreen) {
            if (current_video.SetWindowFullscreen) {
                current_video.SetWindowFullscreen(window, display, SDL_FALSE);
            }
            window->last_fullscreen_flags = window->flags;
            return 0;
        }
    }

    /* See if there are any fullscreen windows */
    for (other = current_video.windows; other; other = other->next) {
        SDL_bool setDisplayMode = SDL_FALSE;

        if (other == window) {
            setDisplayMode = fullscreen;
        } else if (FULLSCREEN_VISIBLE(other) &&
                   SDL_GetDisplayForWindow(other) == display) {
            setDisplayMode = SDL_TRUE;
        }

        if (setDisplayMode) {
            SDL_DisplayMode fullscreen_mode;

            if (SDL_GetWindowDisplayMode(other, &fullscreen_mode) == 0) {
                resized = SDL_TRUE;

                if (other->wrect.w == fullscreen_mode.w && other->wrect.h == fullscreen_mode.h) {
                    resized = SDL_FALSE;
                }

                /* only do the mode change if we want exclusive fullscreen */
                if ((other->flags & SDL_WINDOW_FULLSCREEN_MASK) == SDL_WINDOW_FULLSCREEN) {
                    if (SDL_SetDisplayModeForDisplay(display, &fullscreen_mode) < 0) {
                        return -1;
                    }
                } else {
                    if (SDL_SetDisplayModeForDisplay(display, NULL) < 0) {
                        return -1;
                    }
                }

                if (current_video.SetWindowFullscreen) {
                    current_video.SetWindowFullscreen(other, display, SDL_TRUE);
                }
                display->fullscreen_window = other;

                /* Generate a mode change event here */
                if (resized) {
#if !defined(__ANDROID__) && !defined(__WIN32__)
                        /* Android may not resize the window to exactly what our fullscreen mode is, especially on
                         * windowed Android environments like the Chromebook or Samsung DeX.  Given this, we shouldn't
                         * use fullscreen_mode.w and fullscreen_mode.h, but rather get our current native size.  As such,
                         * Android's SetWindowFullscreen will generate the window event for us with the proper final size.
                         */

                        /* This is also unnecessary on Win32 (WIN_SetWindowFullscreen calls SetWindowPos,
                         * WM_WINDOWPOSCHANGED will send SDL_WINDOWEVENT_RESIZED). Also, on Windows with DPI scaling enabled,
                         * we're keeping modes in pixels, but window sizes in dpi-scaled points, so this would be a unit mismatch.
                         */
                        SDL_SendWindowEvent(other, SDL_WINDOWEVENT_RESIZED,
                                            fullscreen_mode.w, fullscreen_mode.h);
#endif
                } else {
                    SDL_OnWindowResized(other);
                }

                SDL_RestoreMousePosition(other);

                window->last_fullscreen_flags = window->flags;
                return 0;
            }
        }
    }

    /* Nope, restore the desktop mode */
    SDL_SetDisplayModeForDisplay(display, NULL);

    if (current_video.SetWindowFullscreen) {
        current_video.SetWindowFullscreen(window, display, SDL_FALSE);
    }
    display->fullscreen_window = NULL;

    /* Generate a mode change event here */
    SDL_OnWindowResized(window);

    /* Restore the cursor position */
    SDL_RestoreMousePosition(window);

    window->last_fullscreen_flags = window->flags;
    return 0;
}

#define CREATE_FLAGS \
    (SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_SKIP_TASKBAR | SDL_WINDOW_POPUP_MENU | SDL_WINDOW_UTILITY | SDL_WINDOW_TOOLTIP | SDL_WINDOW_VULKAN | SDL_WINDOW_MINIMIZED | SDL_WINDOW_METAL)

static SDL_INLINE SDL_bool IsAcceptingDragAndDrop(void)
{
    if ((SDL_GetEventState(SDL_DROPFILE) == SDL_ENABLE) ||
        (SDL_GetEventState(SDL_DROPTEXT) == SDL_ENABLE)) {
        return SDL_TRUE;
    }
    return SDL_FALSE;
}

/* prepare a newly-created window */
static SDL_INLINE void PrepareDragAndDropSupport(SDL_Window *window)
{
    if (current_video.AcceptDragAndDrop) {
        current_video.AcceptDragAndDrop(window, IsAcceptingDragAndDrop());
    }
}

/* toggle d'n'd for all existing windows. */
void SDL_ToggleDragAndDropSupport(void)
{
    // TEST_VIDEO( )

    if (current_video.AcceptDragAndDrop) {
        const SDL_bool enable = IsAcceptingDragAndDrop();
        SDL_Window *window;
        for (window = current_video.windows; window; window = window->next) {
            current_video.AcceptDragAndDrop(window, enable);
        }
    }
}

static void SDL_FinishWindowCreation(SDL_Window *window, Uint32 flags)
{
    PrepareDragAndDropSupport(window);

    if (flags & SDL_WINDOW_MAXIMIZED) {
        SDL_MaximizeWindow(window);
    }
    if (flags & SDL_WINDOW_MINIMIZED) {
        SDL_MinimizeWindow(window);
    }
    if (flags & SDL_WINDOW_FULLSCREEN) {
        SDL_SetWindowFullscreen(window, flags);
    }
    if (flags & SDL_WINDOW_MOUSE_GRABBED) {
        /* We must specifically call SDL_SetWindowGrab() and not
           SDL_SetWindowMouseGrab() here because older applications may use
           this flag plus SDL_HINT_GRAB_KEYBOARD to indicate that they want
           the keyboard grabbed too and SDL_SetWindowMouseGrab() won't do that.
        */
        SDL_SetWindowGrab(window, SDL_TRUE);
    }
    if (flags & SDL_WINDOW_KEYBOARD_GRABBED) {
        SDL_SetWindowKeyboardGrab(window, SDL_TRUE);
    }
    if (!(flags & SDL_WINDOW_HIDDEN)) {
        SDL_ShowWindow(window);
    }
}

static int SDL_ContextNotSupported(const char *name)
{
    return SDL_SetError("%s support is not available in the current SDL video driver", name);
}

static int SDL_DllNotSupported(const char *name)
{
    return SDL_SetError("No dynamic %s support in the current SDL video driver", name);
}

static void CalculateWindowPosition(int *xp, int *yp, int w, int h)
{
    int x = *xp;
    int y = *yp;
    SDL_bool undef_x = SDL_WINDOWPOS_ISUNDEFINED(x) || SDL_WINDOWPOS_ISCENTERED(x);
    SDL_bool undef_y = SDL_WINDOWPOS_ISUNDEFINED(y) || SDL_WINDOWPOS_ISCENTERED(y);

    if (undef_x || undef_y) {
        int displayIndex;
        SDL_Rect bounds;

        if (undef_x) {
            displayIndex = (x & 0xFFFF);
        }
        else {
            displayIndex = (y & 0xFFFF);
        }
        if (displayIndex >= current_video.num_displays) {
            displayIndex = 0;
        }

        SDL_GetDisplayBounds(displayIndex, &bounds);
        if (undef_x) {
            *xp = bounds.x + (bounds.w - w) / 2;
        }
        if (undef_y) {
            *yp = bounds.y + (bounds.h - h) / 2;
        }
    }
}

SDL_Window *SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags)
{
    SDL_Window *window;
    Uint32 type_flags, graphics_flags;
    int displayIndex;

    if (!SDL_HasVideoDevice()) {
        /* Initialize the video system if needed */
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
            return NULL;
        }
    }

    /* ensure no more than one of these flags is set */
    type_flags = flags & (SDL_WINDOW_UTILITY | SDL_WINDOW_TOOLTIP | SDL_WINDOW_POPUP_MENU);
    if (type_flags & (type_flags - 1)) {
        SDL_SetError("Conflicting window flags specified");
        return NULL;
    }

    /* Some platforms can't create zero-sized windows */
    if (w <= 0) {
        w = 1;
    }
    if (h <= 0) {
        h = 1;
    }

    /* Some platforms blow up if the windows are too large. Raise it later? */
    if (w > SDL_WINDOW_MAX_SIZE) {
        w = SDL_WINDOW_MAX_SIZE;
    }
    if (h > SDL_WINDOW_MAX_SIZE) {
        h = SDL_WINDOW_MAX_SIZE;
    }

    /* ensure no more than one of these flags is set */
    graphics_flags = flags & (SDL_WINDOW_OPENGL | SDL_WINDOW_METAL | SDL_WINDOW_VULKAN);
    if (graphics_flags & (graphics_flags - 1)) {
        SDL_SetError("Conflicting window flags specified");
        return NULL;
    }

    /* Some platforms have certain graphics backends enabled by default */
    if (!graphics_flags && !SDL_IsVideoContextExternal()) {
        flags |= SDL_DefaultGraphicsBackends();
    }

    if (flags & SDL_WINDOW_OPENGL) {
#ifdef SDL_VIDEO_OPENGL_ANY
        if (!current_video.GL_CreateContext) {
            SDL_ContextNotSupported("OpenGL");
            return NULL;
        }
        if (SDL_GL_LoadLibrary(NULL) < 0) {
            return NULL;
        }
#else
            SDL_ContextNotSupported("OpenGL");
            return NULL;
#endif
    }

    if (flags & SDL_WINDOW_VULKAN) {
#ifdef SDL_VIDEO_VULKAN
        if (!current_video.Vulkan_CreateSurface) {
            SDL_ContextNotSupported("Vulkan");
            return NULL;
        }
        if (SDL_Vulkan_LoadLibrary(NULL) < 0) {
            return NULL;
        }
#else
            SDL_ContextNotSupported("Vulkan");
            return NULL;
#endif
    }

    if (flags & SDL_WINDOW_METAL) {
#ifdef SDL_VIDEO_METAL
        if (!current_video.Metal_CreateView) {
#else
        {
#endif
            SDL_ContextNotSupported("Metal");
            return NULL;
        }
    }

    CalculateWindowPosition(&x, &y, w, h);
    displayIndex = GetPointDisplayIndex(x, y, w, h); // SDL_CalculateWindowDisplayIndex

    window = (SDL_Window *)SDL_calloc(1, sizeof(*window));
    if (!window) {
        SDL_OutOfMemory();
        return NULL;
    }
    window->magic = &current_video.window_magic;
    window->id = current_video.next_object_id++;
    window->wrect.x = x;
    window->wrect.y = y;
    window->wrect.w = w;
    window->wrect.h = h;

    window->windowed = window->wrect;

    window->display_index = displayIndex;

    if (IsFullscreenOnly()) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }
    if (flags & SDL_WINDOW_FULLSCREEN) {
        SDL_VideoDisplay *display = &current_video.displays[displayIndex];
        SDL_Rect bounds;

        SDL_PrivateGetDisplayBounds(display, &bounds);

        /* for real fullscreen we might switch the resolution, so get width and height
         * from closest supported mode and use that instead of current resolution
         */
        if ((flags & SDL_WINDOW_FULLSCREEN_MASK) == SDL_WINDOW_FULLSCREEN && (bounds.w != w || bounds.h != h)) {
            SDL_DisplayMode fullscreen_mode;
            SDL_zero(fullscreen_mode);
            fullscreen_mode.w = w;
            fullscreen_mode.h = h;
            if (SDL_GetClosestDisplayModeForDisplay(display, &fullscreen_mode, &fullscreen_mode) != NULL) {
                bounds.w = fullscreen_mode.w;
                bounds.h = fullscreen_mode.h;
            }
        }
        window->wrect = bounds;
    }

    window->flags = ((flags & CREATE_FLAGS) | SDL_WINDOW_HIDDEN);
    window->last_fullscreen_flags = window->flags;
    window->opacity = 1.0f;
    window->brightness = 1.0f;
    window->next = current_video.windows;
    // window->is_destroying = SDL_FALSE;

    if (current_video.windows) {
        current_video.windows->prev = window;
    }
    current_video.windows = window;

    if (current_video.CreateSDLWindow && current_video.CreateSDLWindow(&current_video, window) < 0) {
        SDL_DestroyWindow(window);
        return NULL;
    }

    /* Clear minimized if not on windows, only windows handles it at create rather than FinishWindowCreation,
     * but it's important or window focus will get broken on windows!
     */
#if !defined(__WIN32__) && !defined(__GDK__)
    window->flags &= ~SDL_WINDOW_MINIMIZED;
#endif

#if defined(__WINRT__) && (NTDDI_VERSION < NTDDI_WIN10)
    /* HACK: WinRT 8.x apps can't choose whether or not they are fullscreen
       or not.  The user can choose this, via OS-provided UI, but this can't
       be set programmatically.

       Just look at what SDL's WinRT video backend code detected with regards
       to fullscreen (being active, or not), and figure out a return/error code
       from that.
    */
    flags = window->flags;
#endif

    if (title) {
        SDL_SetWindowTitle(window, title);
    }
    SDL_FinishWindowCreation(window, flags);

    /* If the window was created fullscreen, make sure the mode code matches */
    SDL_UpdateFullscreenMode(window, FULLSCREEN_VISIBLE(window));

    return window;
}

SDL_Window *SDL_CreateWindowFrom(const void *data)
{
#if 0
    SDL_Window *window;
    Uint32 flags = SDL_WINDOW_FOREIGN;

    CHECK_VIDEO(NULL)

    if (!current_video.CreateSDLWindowFrom) {
        SDL_Unsupported();
        return NULL;
    }

    if (SDL_GetHintBoolean(SDL_HINT_VIDEO_FOREIGN_WINDOW_OPENGL, SDL_FALSE)) {
#ifdef SDL_VIDEO_OPENGL_ANY
        if (!current_video.GL_CreateContext) {
            SDL_ContextNotSupported("OpenGL");
            return NULL;
        }
        if (SDL_GL_LoadLibrary(NULL) < 0) {
            return NULL;
        }
        flags |= SDL_WINDOW_OPENGL;
#else
            SDL_ContextNotSupported("OpenGL");
            return NULL;
#endif
    }

    if (SDL_GetHintBoolean(SDL_HINT_VIDEO_FOREIGN_WINDOW_VULKAN, SDL_FALSE)) {
#ifdef SDL_VIDEO_VULKAN
        if (!current_video.Vulkan_CreateSurface) {
            SDL_ContextNotSupported("Vulkan");
            return NULL;
        }
        if (flags & SDL_WINDOW_OPENGL) {
            SDL_SetError("Vulkan and OpenGL not supported on same window");
            return NULL;
        }
        if (SDL_Vulkan_LoadLibrary(NULL) < 0) {
            return NULL;
        }
        flags |= SDL_WINDOW_VULKAN;
#else
            SDL_ContextNotSupported("Vulkan");
            return NULL;
#endif
    }

    window = (SDL_Window *)SDL_calloc(1, sizeof(*window));
    if (!window) {
        SDL_OutOfMemory();
        return NULL;
    }
    window->magic = &current_video.window_magic;
    window->id = current_video.next_object_id++;
    window->flags = flags;
    window->last_fullscreen_flags = window->flags;
    // window->is_destroying = SDL_FALSE;
    window->opacity = 1.0f;
    window->brightness = 1.0f;
    window->next = current_video.windows;
    if (current_video.windows) {
        current_video.windows->prev = window;
    }
    current_video.windows = window;

    if (current_video.CreateSDLWindowFrom(&current_video, window, data) < 0) {
        SDL_DestroyWindow(window);
        return NULL;
    }

    window->display_index = SDL_CalculateWindowDisplayIndex(window);
    PrepareDragAndDropSupport(window);

    return window;
#else
    SDL_Unsupported();
    return NULL;
#endif
}

int SDL_RecreateWindow(SDL_Window *window, Uint32 flags)
{
    SDL_bool gl_unload = SDL_FALSE;
    SDL_bool gl_load = SDL_FALSE;
    SDL_bool vulkan_unload = SDL_FALSE;
    SDL_bool vulkan_load = SDL_FALSE;
    SDL_bool foreign_win; // whether the old window is foreign (same as whether the new window is foreign at the moment)
    Uint32 graphics_flags;

    /* ensure no more than one of these flags is set */
    graphics_flags = flags & (SDL_WINDOW_OPENGL | SDL_WINDOW_METAL | SDL_WINDOW_VULKAN);
    SDL_assert((graphics_flags & (graphics_flags - 1)) == 0);

    if (flags & SDL_WINDOW_OPENGL) {
#ifdef SDL_VIDEO_OPENGL_ANY
        SDL_assert(current_video.GL_CreateContext != NULL);
#else
        SDL_assume(!"Invalid (OPENGL) window flag in SDL_RecreateWindow.");
#endif
    }
    if (flags & SDL_WINDOW_VULKAN) {
#ifdef SDL_VIDEO_VULKAN
        SDL_assert(current_video.Vulkan_CreateSurface != NULL);
#else
        SDL_assume(!"Invalid (VULKAN) window flag in SDL_RecreateWindow.");
#endif
    }
    if (flags & SDL_WINDOW_METAL) {
#ifdef SDL_VIDEO_METAL
        SDL_assert(current_video.Metal_CreateView != NULL);
#else
        SDL_assume(!"Invalid (METAL) window flag in SDL_RecreateWindow.");
#endif
    }
#if 0
    foreign_win = (window->flags & SDL_WINDOW_FOREIGN) ? SDL_TRUE : SDL_FALSE;
    if (foreign_win) {
        /* Can't destroy and re-create foreign windows, hrm */
        flags |= SDL_WINDOW_FOREIGN;
    } else {
        flags &= ~SDL_WINDOW_FOREIGN;
    }
#else
    SDL_assert(!(flags & SDL_WINDOW_FOREIGN));
    foreign_win = SDL_FALSE;
#endif
    /* Restore video mode, etc. */
    if (!foreign_win) { // (!(window->flags & SDL_WINDOW_FOREIGN)) {
        SDL_HideWindow(window);
    }

    /* Tear down the old native window */
    if (current_video.checked_texture_framebuffer) { /* never checked? No framebuffer to destroy. Don't risk calling the wrong implementation. */
        if (current_video.DestroyWindowFramebuffer) {
            current_video.DestroyWindowFramebuffer(window);
        }
    }
    SDL_DestroyWindowSurface(window);

    if (current_video.DestroyWindow && !foreign_win) { // !(flags & SDL_WINDOW_FOREIGN)) {
        current_video.DestroyWindow(window);
    }

    if ((window->flags & SDL_WINDOW_OPENGL) != (flags & SDL_WINDOW_OPENGL)) {
        if (flags & SDL_WINDOW_OPENGL) {
            gl_load = SDL_TRUE;
        } else {
            gl_unload = SDL_TRUE;
        }
    } else if (flags & SDL_WINDOW_OPENGL) {
        gl_unload = SDL_TRUE;
        gl_load = SDL_TRUE;
    }

    if ((window->flags & SDL_WINDOW_VULKAN) != (flags & SDL_WINDOW_VULKAN)) {
        if (flags & SDL_WINDOW_VULKAN) {
            vulkan_load = SDL_TRUE;
        } else {
            vulkan_unload = SDL_TRUE;
        }
    } else if (flags & SDL_WINDOW_VULKAN) {
        vulkan_unload = SDL_TRUE;
        vulkan_load = SDL_TRUE;
    }

    if (gl_unload) {
        SDL_GL_UnloadLibrary();
    }

    if (vulkan_unload) {
        SDL_Vulkan_UnloadLibrary();
    }

    if (gl_load) {
        if (SDL_GL_LoadLibrary(NULL) < 0) {
            return -1;
        }
    }

    if (vulkan_load) {
        if (SDL_Vulkan_LoadLibrary(NULL) < 0) {
            return -1;
        }
    }

    window->flags = ((flags & (CREATE_FLAGS | SDL_WINDOW_FOREIGN)) | SDL_WINDOW_HIDDEN);
    window->last_fullscreen_flags = window->flags;
    // window->is_destroying = SDL_FALSE; -- there is no turn back after the SDL_DestroyWindow is called...

    if (current_video.CreateSDLWindow && !foreign_win) { // !(flags & SDL_WINDOW_FOREIGN)) {
        if (current_video.CreateSDLWindow(&current_video, window) < 0) {
            if (gl_load) {
                SDL_GL_UnloadLibrary();
                window->flags &= ~SDL_WINDOW_OPENGL;
            }
            if (vulkan_load) {
                SDL_Vulkan_UnloadLibrary();
                window->flags &= ~SDL_WINDOW_VULKAN;
            }
            return -1;
        }
    }

    if (current_video.SetWindowTitle && window->title) {
        current_video.SetWindowTitle(window);
    }

    if (current_video.SetWindowIcon && window->icon) {
        current_video.SetWindowIcon(window, window->icon);
    }

    if (current_video.SetWindowMinimumSize && (window->min_w || window->min_h)) {
        current_video.SetWindowMinimumSize(window);
    }

    if (current_video.SetWindowMaximumSize && (window->max_w || window->max_h)) {
        current_video.SetWindowMaximumSize(window);
    }

    if (window->hit_test) {
        current_video.SetWindowHitTest(window, SDL_TRUE);
    }

    SDL_FinishWindowCreation(window, flags);

    return 0;
}

int SDL_CheckWindow(SDL_Window *window)
{
    CHECK_WINDOW_MAGIC(window, -1);

    return 0;
}

SDL_Window *SDL_GetWindowsOptional(void)
{
    // TEST_VIDEO(NULL)

    return current_video.windows;
}

SDL_Window *SDL_GetWindows(void)
{
    SDL_assert(SDL_HasVideoDevice());
    return current_video.windows;
}

Uint32 SDL_GetWindowID(SDL_Window *window)
{
    CHECK_WINDOW_MAGIC(window, 0);

    return window->id;
}

SDL_Window *SDL_GetWindowFromID(Uint32 id)
{
    SDL_Window *window;

    // TEST_VIDEO(NULL)

    for (window = current_video.windows; window; window = window->next) {
        if (window->id == id) {
            return window;
        }
    }
    return NULL;
}

Uint32 SDL_GetWindowFlags(SDL_Window *window)
{
    CHECK_WINDOW_MAGIC(window, 0);

    return window->flags;
}

void SDL_SetWindowTitle(SDL_Window *window, const char *title)
{
    CHECK_WINDOW_MAGIC(window, );

    if (title == window->title) {
        return;
    }
    SDL_free(window->title);

    window->title = SDL_strdup(title ? title : "");

    if (current_video.SetWindowTitle) {
        current_video.SetWindowTitle(window);
    }
}

const char *SDL_GetWindowTitle(SDL_Window *window)
{
    CHECK_WINDOW_MAGIC(window, "");

    return window->title ? window->title : "";
}

void SDL_SetWindowIcon(SDL_Window *window, SDL_Surface *icon)
{
    CHECK_WINDOW_MAGIC(window, );

    if (!icon) {
        return;
    }

    SDL_FreeSurface(window->icon);

    /* Convert the icon into ARGB8888 */
    window->icon = SDL_ConvertSurfaceFormat(icon, SDL_PIXELFORMAT_ARGB8888, 0);
    if (!window->icon) {
        return;
    }

    if (current_video.SetWindowIcon) {
        current_video.SetWindowIcon(window, window->icon);
    }
}

void *SDL_SetWindowData(SDL_Window *window, const char *name, void *userdata)
{
    SDL_WindowUserData *prev, *data;

    CHECK_WINDOW_MAGIC(window, NULL);

    /* Input validation */
    if (name == NULL || name[0] == '\0') {
        SDL_InvalidParamError("name");
        return NULL;
    }

    /* See if the named data already exists */
    prev = NULL;
    for (data = window->data; data; prev = data, data = data->next) {
        if (data->name && SDL_strcmp(data->name, name) == 0) {
            void *last_value = data->data;

            if (userdata) {
                /* Set the new value */
                data->data = userdata;
            } else {
                /* Delete this value */
                if (prev) {
                    prev->next = data->next;
                } else {
                    window->data = data->next;
                }
                SDL_free(data->name);
                SDL_free(data);
            }
            return last_value;
        }
    }

    /* Add new data to the window */
    if (userdata) {
        data = (SDL_WindowUserData *)SDL_malloc(sizeof(*data));
        data->name = SDL_strdup(name);
        data->data = userdata;
        data->next = window->data;
        window->data = data;
    }
    return NULL;
}

void *SDL_GetWindowData(SDL_Window *window, const char *name)
{
    SDL_WindowUserData *data;

    CHECK_WINDOW_MAGIC(window, NULL);

    /* Input validation */
    if (name == NULL || name[0] == '\0') {
        SDL_InvalidParamError("name");
        return NULL;
    }

    for (data = window->data; data; data = data->next) {
        if (data->name && SDL_strcmp(data->name, name) == 0) {
            return data->data;
        }
    }
    return NULL;
}

void SDL_SetWindowPosition(SDL_Window *window, int x, int y)
{
    CHECK_WINDOW_MAGIC(window, );

    if (SDL_WINDOWPOS_ISUNDEFINED(x)) {
        x = window->windowed.x;
    }
    if (SDL_WINDOWPOS_ISUNDEFINED(y)) {
        y = window->windowed.y;
    }

    CalculateWindowPosition(&x, &y, window->windowed.w, window->windowed.h);

    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        window->windowed.x = x;
        window->windowed.y = y;
    } else {
        window->wrect.x = x;
        window->wrect.y = y;

        if (current_video.SetWindowPosition) {
            current_video.SetWindowPosition(window);
        }
    }
}

void SDL_GetWindowPosition(SDL_Window *window, int *x, int *y)
{
    CHECK_WINDOW_MAGIC(window, );

    /* Fullscreen windows are always at their display's origin */
    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        int displayIndex;

        if (x) {
            *x = 0;
        }
        if (y) {
            *y = 0;
        }

        /* Find the window's monitor and update to the
           monitor offset. */
        displayIndex = window->display_index;
        if (displayIndex >= 0) {
            SDL_Rect bounds;

            SDL_GetDisplayBounds(displayIndex, &bounds);
            if (x) {
                *x = bounds.x;
            }
            if (y) {
                *y = bounds.y;
            }
        }
    } else {
        if (x) {
            *x = window->wrect.x;
        }
        if (y) {
            *y = window->wrect.y;
        }
    }
}

void SDL_SetWindowBordered(SDL_Window *window, SDL_bool bordered)
{
    CHECK_WINDOW_MAGIC(window, );
    if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
        const int want = (bordered != SDL_FALSE); /* normalize the flag. */
        const int have = !(window->flags & SDL_WINDOW_BORDERLESS);
        if ((want != have) && (current_video.SetWindowBordered)) {
            if (want) {
                window->flags &= ~SDL_WINDOW_BORDERLESS;
            } else {
                window->flags |= SDL_WINDOW_BORDERLESS;
            }
            current_video.SetWindowBordered(window, (SDL_bool)want);
        }
    }
}

void SDL_SetWindowResizable(SDL_Window *window, SDL_bool resizable)
{
    CHECK_WINDOW_MAGIC(window, );
    if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
        const int want = (resizable != SDL_FALSE); /* normalize the flag. */
        const int have = ((window->flags & SDL_WINDOW_RESIZABLE) != 0);
        if ((want != have) && (current_video.SetWindowResizable)) {
            if (want) {
                window->flags |= SDL_WINDOW_RESIZABLE;
            } else {
                window->flags &= ~SDL_WINDOW_RESIZABLE;
            }
            current_video.SetWindowResizable(window, (SDL_bool)want);
        }
    }
}

void SDL_SetWindowAlwaysOnTop(SDL_Window *window, SDL_bool on_top)
{
    CHECK_WINDOW_MAGIC(window, );
    if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
        const int want = (on_top != SDL_FALSE); /* normalize the flag. */
        const int have = ((window->flags & SDL_WINDOW_ALWAYS_ON_TOP) != 0);
        if ((want != have) && (current_video.SetWindowAlwaysOnTop)) {
            if (want) {
                window->flags |= SDL_WINDOW_ALWAYS_ON_TOP;
            } else {
                window->flags &= ~SDL_WINDOW_ALWAYS_ON_TOP;
            }

            current_video.SetWindowAlwaysOnTop(window, (SDL_bool)want);
        }
    }
}

void SDL_SetWindowSize(SDL_Window *window, int w, int h)
{
    CHECK_WINDOW_MAGIC(window, );
    if (w <= 0) {
        SDL_InvalidParamError("w");
        return;
    }
    if (h <= 0) {
        SDL_InvalidParamError("h");
        return;
    }

    /* Make sure we don't exceed any window size limits */
    if (window->min_w && w < window->min_w) {
        w = window->min_w;
    }
    if (window->max_w && w > window->max_w) {
        w = window->max_w;
    }
    if (window->min_h && h < window->min_h) {
        h = window->min_h;
    }
    if (window->max_h && h > window->max_h) {
        h = window->max_h;
    }

    window->windowed.w = w;
    window->windowed.h = h;

    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        if (FULLSCREEN_VISIBLE(window) && (window->flags & SDL_WINDOW_FULLSCREEN_MASK) == SDL_WINDOW_FULLSCREEN) {
            window->last_fullscreen_flags = 0;
            SDL_UpdateFullscreenMode(window, SDL_TRUE);
        }
    } else {
        int old_w = window->wrect.w;
        int old_h = window->wrect.h;
        window->wrect.w = w;
        window->wrect.h = h;
        if (current_video.SetWindowSize) {
            current_video.SetWindowSize(window);
        }
        if (window->wrect.w != old_w || window->wrect.h != old_h) {
            /* We didn't get a SDL_WINDOWEVENT_RESIZED event (by design) */
            SDL_OnWindowResized(window);
        }
    }
}

void SDL_GetWindowSize(SDL_Window *window, int *w, int *h)
{
    CHECK_WINDOW_MAGIC(window, );
    if (w) {
        *w = window->wrect.w;
    }
    if (h) {
        *h = window->wrect.h;
    }
}

int SDL_GetWindowBordersSize(SDL_Window *window, int *top, int *left, int *bottom, int *right)
{
    int dummy;

    if (!top) {
        top = &dummy;
    }
    if (!left) {
        left = &dummy;
    }
    if (!right) {
        right = &dummy;
    }
    if (!bottom) {
        bottom = &dummy;
    }

    /* Always initialize, so applications don't have to care */
    *top = *left = *bottom = *right = 0;

    CHECK_WINDOW_MAGIC(window, -1);

    if (!current_video.GetWindowBordersSize) {
        return SDL_Unsupported();
    }

    return current_video.GetWindowBordersSize(window, top, left, bottom, right);
}

void SDL_PrivateGetWindowSizeInPixels(SDL_Window *window, int *w, int *h)
{
    if (current_video.GetWindowSizeInPixels) {
        current_video.GetWindowSizeInPixels(window, w, h);
    } else {
        // SDL_GetWindowSize
        *w = window->wrect.w;
        *h = window->wrect.h;
    }
}

void SDL_GetWindowSizeInPixels(SDL_Window *window, int *w, int *h)
{
    int filter;

    CHECK_WINDOW_MAGIC(window, );

    if (!w) {
        w = &filter;
    }

    if (!h) {
        h = &filter;
    }

    SDL_PrivateGetWindowSizeInPixels(window, w, h);
}

void SDL_SetWindowMinimumSize(SDL_Window *window, int min_w, int min_h)
{
    CHECK_WINDOW_MAGIC(window, );
    if (min_w <= 0) {
        SDL_InvalidParamError("min_w");
        return;
    }
    if (min_h <= 0) {
        SDL_InvalidParamError("min_h");
        return;
    }

    if ((window->max_w && min_w > window->max_w) ||
        (window->max_h && min_h > window->max_h)) {
        SDL_SetError("SDL_SetWindowMinimumSize(): Tried to set minimum size larger than maximum size");
        return;
    }

    window->min_w = min_w;
    window->min_h = min_h;

    if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
        if (current_video.SetWindowMinimumSize) {
            current_video.SetWindowMinimumSize(window);
        }
        /* Ensure that window is not smaller than minimal size */
        SDL_SetWindowSize(window, SDL_max(window->wrect.w, window->min_w), SDL_max(window->wrect.h, window->min_h));
    }
}

void SDL_GetWindowMinimumSize(SDL_Window *window, int *min_w, int *min_h)
{
    CHECK_WINDOW_MAGIC(window, );
    if (min_w) {
        *min_w = window->min_w;
    }
    if (min_h) {
        *min_h = window->min_h;
    }
}

void SDL_SetWindowMaximumSize(SDL_Window *window, int max_w, int max_h)
{
    CHECK_WINDOW_MAGIC(window, );
    if (max_w <= 0) {
        SDL_InvalidParamError("max_w");
        return;
    }
    if (max_h <= 0) {
        SDL_InvalidParamError("max_h");
        return;
    }

    if (max_w < window->min_w || max_h < window->min_h) {
        SDL_SetError("SDL_SetWindowMaximumSize(): Tried to set maximum size smaller than minimum size");
        return;
    }

    window->max_w = max_w;
    window->max_h = max_h;

    if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
        if (current_video.SetWindowMaximumSize) {
            current_video.SetWindowMaximumSize(window);
        }
        /* Ensure that window is not larger than maximal size */
        SDL_SetWindowSize(window, SDL_min(window->wrect.w, window->max_w), SDL_min(window->wrect.h, window->max_h));
    }
}

void SDL_GetWindowMaximumSize(SDL_Window *window, int *max_w, int *max_h)
{
    CHECK_WINDOW_MAGIC(window, );
    if (max_w) {
        *max_w = window->max_w;
    }
    if (max_h) {
        *max_h = window->max_h;
    }
}

void SDL_ShowWindow(SDL_Window *window)
{
    CHECK_WINDOW_MAGIC(window, );

    if (window->flags & SDL_WINDOW_SHOWN) {
        return;
    }

    if (current_video.ShowWindow) {
        current_video.ShowWindow(window);
    }
    SDL_SendWindowEvent(window, SDL_WINDOWEVENT_SHOWN, 0, 0);
}

void SDL_HideWindow(SDL_Window *window)
{
    CHECK_WINDOW_MAGIC(window, );

    if (!(window->flags & SDL_WINDOW_SHOWN)) {
        return;
    }

    window->is_hiding = SDL_TRUE;
    SDL_UpdateFullscreenMode(window, SDL_FALSE);

    if (current_video.HideWindow) {
        current_video.HideWindow(window);
    }
    window->is_hiding = SDL_FALSE;
    SDL_SendWindowEvent(window, SDL_WINDOWEVENT_HIDDEN, 0, 0);
}

void SDL_RaiseWindow(SDL_Window *window)
{
    CHECK_WINDOW_MAGIC(window, );

    if (!(window->flags & SDL_WINDOW_SHOWN)) {
        return;
    }
    if (current_video.RaiseWindow) {
        current_video.RaiseWindow(window);
    }
}

void SDL_MaximizeWindow(SDL_Window *window)
{
    CHECK_WINDOW_MAGIC(window, );

    if (window->flags & SDL_WINDOW_MAXIMIZED) {
        return;
    }

    /* !!! FIXME: should this check if the window is resizable? */

    if (current_video.MaximizeWindow) {
        current_video.MaximizeWindow(window);
    }
}

void SDL_MinimizeWindow(SDL_Window *window)
{
    CHECK_WINDOW_MAGIC(window, );

    if (window->flags & SDL_WINDOW_MINIMIZED) {
        return;
    }

    if (!current_video.MinimizeWindow) {
        return;
    }

    if (!DisableUnsetFullscreenOnMinimize()) {
        SDL_UpdateFullscreenMode(window, SDL_FALSE);
    }

    current_video.MinimizeWindow(window);
}

void SDL_RestoreWindow(SDL_Window *window)
{
    CHECK_WINDOW_MAGIC(window, );

    if (!(window->flags & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED))) {
        return;
    }

    if (current_video.RestoreWindow) {
        current_video.RestoreWindow(window);
    }
}

int SDL_SetWindowFullscreen(SDL_Window *window, Uint32 flags)
{
    Uint32 oldflags;
    CHECK_WINDOW_MAGIC(window, -1);

    flags &= SDL_WINDOW_FULLSCREEN_MASK;
    oldflags = window->flags & SDL_WINDOW_FULLSCREEN_MASK;

    if (flags == oldflags) {
        return 0;
    }

    /* clear the previous flags and OR in the new ones */
    window->flags &= ~SDL_WINDOW_FULLSCREEN_MASK;
    window->flags |= flags;

    if (SDL_UpdateFullscreenMode(window, FULLSCREEN_VISIBLE(window)) == 0) {
        return 0;
    }

    window->flags &= ~SDL_WINDOW_FULLSCREEN_MASK;
    window->flags |= oldflags;
    return -1;
}

/* See if the user or application wants to specifically disable the framebuffer */
static SDL_bool DisableFrameBuffer(void)
{
    SDL_bool result = SDL_FALSE;
    const char *hint = SDL_GetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION);
    if (hint) {
        if ((*hint == '0') || (SDL_strcasecmp(hint, "false") == 0) || (SDL_strcasecmp(hint, "software") == 0)) {
            result = SDL_TRUE;
        }
    }
    return result;
}

static SDL_Surface *SDL_CreateWindowSurface(SDL_Window *window)
{
    Uint32 format;
    void *pixels;
    SDL_bool created_framebuffer = SDL_FALSE;
    int w, h, pitch;

    // help the retarded compilers
#ifdef __WINRT__
    pixels = NULL;
#endif

    /* This will switch the video backend from using a software surface to
       using a GPU texture through the 2D render API, if we think this would
       be more efficient. This only checks once, on demand. */
    if (!current_video.checked_texture_framebuffer) {
        SDL_bool attempt_texture_framebuffer = SDL_TRUE;

#if defined(SDL_VIDEO_DRIVER_DUMMY) /* dummy driver never has GPU support, of course. */
        if (SDL_GetVideoDeviceId() == SDL_VIDEODRIVER_DUMMY) {
            attempt_texture_framebuffer = SDL_FALSE;
        }
#ifdef SDL_INPUT_LINUXEV
        if (SDL_GetVideoDeviceId() == SDL_VIDEODRIVER_DUMMY_evdev) {
            attempt_texture_framebuffer = SDL_FALSE;
        }
#endif // SDL_INPUT_LINUXEV
#endif // SDL_VIDEO_DRIVER_DUMMY
#if defined(__LINUX__) && defined(SDL_VIDEO_DRIVER_X11)
        /* On WSL, direct X11 is faster than using OpenGL for window framebuffers, so try to detect WSL and avoid texture framebuffer. */
        if (SDL_GetVideoDeviceId() == SDL_VIDEODRIVER_X11) {
            struct stat sb;
            SDL_assert(current_video.CreateWindowFramebuffer != NULL);
            if ((stat("/proc/sys/fs/binfmt_misc/WSLInterop", &sb) == 0) || (stat("/run/WSL", &sb) == 0)) { /* if either of these exist, we're on WSL. */
                attempt_texture_framebuffer = SDL_FALSE;
            }
        }
#endif
#if (defined(__WIN32__) || defined(__WINGDK__)) && defined(SDL_VIDEO_DRIVER_WINDOWS) && !defined(__XBOXONE__) && !defined(__XBOXSERIES__) /* GDI BitBlt() is way faster than Direct3D dynamic textures right now. (!!! FIXME: is this still true?) */
        if (SDL_GetVideoDeviceId() == SDL_VIDEODRIVER_WIN) {
            SDL_assert(current_video.CreateWindowFramebuffer != NULL);
            attempt_texture_framebuffer = SDL_FALSE;
        }
#endif
#if defined(__EMSCRIPTEN__) || defined(__PSP__)
        {
            attempt_texture_framebuffer = SDL_FALSE;
        }
#endif

        if (attempt_texture_framebuffer && !DisableFrameBuffer()) {
            if (SDL_CreateWindowFramebuffer(window, &format, &pixels, &pitch) >= 0) {
                /* future attempts will just try to use a texture framebuffer. */
                /* !!! FIXME:  maybe we shouldn't override these but check if we used a texture
                   !!! FIXME:  framebuffer at the right places; is it feasible we could have an
                   !!! FIXME:  accelerated OpenGL window and a second ends up in software? */
                current_video.CreateWindowFramebuffer = SDL_CreateWindowFramebuffer;
                current_video.UpdateWindowFramebuffer = SDL_UpdateWindowFramebuffer;
                current_video.DestroyWindowFramebuffer = SDL_DestroyWindowFramebuffer;
                created_framebuffer = SDL_TRUE;
            }
        }

        current_video.checked_texture_framebuffer = SDL_TRUE; /* don't check this again. */
    }

    if (!created_framebuffer) {
        if (!current_video.CreateWindowFramebuffer || !current_video.UpdateWindowFramebuffer) {
            return NULL;
        }

        if (current_video.CreateWindowFramebuffer(window, &format, &pixels, &pitch) < 0) {
            return NULL;
        }
    }

    if (window->surface) {
        return window->surface;
    }

    SDL_PrivateGetWindowSizeInPixels(window, &w, &h);
    return SDL_CreateRGBSurfaceWithFormatFrom(pixels, w, h, 0, pitch, format);
}

SDL_bool SDL_HasWindowSurface(SDL_Window *window)
{
    CHECK_WINDOW_MAGIC(window, SDL_FALSE);

    return window->surface ? SDL_TRUE : SDL_FALSE;
}

SDL_Surface *SDL_GetWindowSurface(SDL_Window *window)
{
    CHECK_WINDOW_MAGIC(window, NULL);

    if (!window->surface_valid) {
        SDL_DestroyWindowSurface(window);
        window->surface = SDL_CreateWindowSurface(window);
        if (window->surface) {
            window->surface_valid = SDL_TRUE;
            window->surface->flags |= SDL_DONTFREE;
        }
    }
    return window->surface;
}

int SDL_UpdateWindowSurface(SDL_Window *window)
{
    SDL_Rect full_rect;

    CHECK_WINDOW_MAGIC(window, -1);

    full_rect.x = 0;
    full_rect.y = 0;
    SDL_PrivateGetWindowSizeInPixels(window, &full_rect.w, &full_rect.h);

    return SDL_UpdateWindowSurfaceRects(window, &full_rect, 1);
}

int SDL_UpdateWindowSurfaceRects(SDL_Window *window, const SDL_Rect *rects,
                                 int numrects)
{
    CHECK_WINDOW_MAGIC(window, -1);

    if (!window->surface_valid) {
        return SDL_SetError("Window surface is invalid, please call SDL_GetWindowSurface() to get a new surface");
    }

    SDL_assert(current_video.checked_texture_framebuffer); /* we should have done this before we had a valid surface. */

    return current_video.UpdateWindowFramebuffer(window, rects, numrects);
}

int SDL_SetWindowBrightness(SDL_Window * window, float brightness)
{
    Uint16 ramp[256];
    int status;

    CHECK_WINDOW_MAGIC(window, -1);

    SDL_CalculateGammaRamp(brightness, ramp);
    status = SDL_SetWindowGammaRamp(window, ramp, ramp, ramp);
    if (status == 0) {
        window->brightness = brightness;
    }
    return status;
}

float SDL_GetWindowBrightness(SDL_Window * window)
{
    CHECK_WINDOW_MAGIC(window, 1.0f);

    return window->brightness;
}

int SDL_SetWindowOpacity(SDL_Window * window, float opacity)
{
    int retval;
    CHECK_WINDOW_MAGIC(window, -1);

    if (!current_video.SetWindowOpacity) {
        return SDL_Unsupported();
    }

    if (opacity < 0.0f) {
        opacity = 0.0f;
    } else if (opacity > 1.0f) {
        opacity = 1.0f;
    }

    retval = current_video.SetWindowOpacity(window, opacity);
    if (retval == 0) {
        window->opacity = opacity;
    }

    return retval;
}

int SDL_DestroyWindowSurface(SDL_Window *window)
{
    CHECK_WINDOW_MAGIC(window, -1);

    if (window->surface) {
        window->surface->flags &= ~SDL_DONTFREE;
        SDL_FreeSurface(window->surface);
        window->surface = NULL;
        window->surface_valid = SDL_FALSE;
    }
    return 0;
}

int SDL_GetWindowOpacity(SDL_Window *window, float *out_opacity)
{
    CHECK_WINDOW_MAGIC(window, -1);

    if (out_opacity) {
        *out_opacity = window->opacity;
    }

    return 0;
}

int SDL_SetWindowModalFor(SDL_Window *modal_window, SDL_Window *parent_window)
{
    CHECK_WINDOW_MAGIC(modal_window, -1);
    CHECK_WINDOW_MAGIC(parent_window, -1);

    if (!current_video.SetWindowModalFor) {
        return SDL_Unsupported();
    }

    return current_video.SetWindowModalFor(modal_window, parent_window);
}

int SDL_SetWindowInputFocus(SDL_Window *window)
{
    CHECK_WINDOW_MAGIC(window, -1);

    if (!current_video.SetWindowInputFocus) {
        return SDL_Unsupported();
    }

    return current_video.SetWindowInputFocus(window);
}


int SDL_SetWindowGammaRamp(SDL_Window * window, const Uint16 * red,
                                            const Uint16 * green,
                                            const Uint16 * blue)
{
    CHECK_WINDOW_MAGIC(window, -1);

    if (!current_video.SetWindowGammaRamp) {
        return SDL_Unsupported();
    }

    if (!window->gamma) {
        if (SDL_GetWindowGammaRamp(window, NULL, NULL, NULL) < 0) {
            return -1;
        }
        SDL_assert(window->gamma != NULL);
    }

    if (red) {
        SDL_memcpy(&window->gamma[0*256], red, 256*sizeof(Uint16));
    }
    if (green) {
        SDL_memcpy(&window->gamma[1*256], green, 256*sizeof(Uint16));
    }
    if (blue) {
        SDL_memcpy(&window->gamma[2*256], blue, 256*sizeof(Uint16));
    }
    if (window->flags & SDL_WINDOW_INPUT_FOCUS) {
        return current_video.SetWindowGammaRamp(window, window->gamma);
    } else {
        return 0;
    }
}

int SDL_GetWindowGammaRamp(SDL_Window * window, Uint16 * red,
                                            Uint16 * green,
                                            Uint16 * blue)
{
    CHECK_WINDOW_MAGIC(window, -1);

    if (!window->gamma) {
        int i;

        window->gamma = (Uint16 *)SDL_malloc(256*6*sizeof(Uint16));
        if (!window->gamma) {
            return SDL_OutOfMemory();
        }
        window->saved_gamma = window->gamma + 3*256;

        if (current_video.GetWindowGammaRamp) {
            if (current_video.GetWindowGammaRamp(window, window->gamma) < 0) {
                return -1;
            }
        } else {
            /* Create an identity gamma ramp */
            for (i = 0; i < 256; ++i) {
                Uint16 value = (Uint16)((i << 8) | i);

                window->gamma[0*256+i] = value;
                window->gamma[1*256+i] = value;
                window->gamma[2*256+i] = value;
            }
        }
        SDL_memcpy(window->saved_gamma, window->gamma, 3*256*sizeof(Uint16));
    }

    if (red) {
        SDL_memcpy(red, &window->gamma[0*256], 256*sizeof(Uint16));
    }
    if (green) {
        SDL_memcpy(green, &window->gamma[1*256], 256*sizeof(Uint16));
    }
    if (blue) {
        SDL_memcpy(blue, &window->gamma[2*256], 256*sizeof(Uint16));
    }
    return 0;
}

void SDL_UpdateWindowGrab(SDL_Window * window)
{
    SDL_bool keyboard_grabbed, mouse_grabbed;

    if (window->flags & SDL_WINDOW_INPUT_FOCUS) {
        if (SDL_GetMouse()->relative_mode || (window->flags & SDL_WINDOW_MOUSE_GRABBED)) {
            mouse_grabbed = SDL_TRUE;
        } else {
            mouse_grabbed = SDL_FALSE;
        }

        if (window->flags & SDL_WINDOW_KEYBOARD_GRABBED) {
            keyboard_grabbed = SDL_TRUE;
        } else {
            keyboard_grabbed = SDL_FALSE;
        }
    } else {
        mouse_grabbed = SDL_FALSE;
        keyboard_grabbed = SDL_FALSE;
    }

    if (mouse_grabbed || keyboard_grabbed) {
        if (current_video.grabbed_window && (current_video.grabbed_window != window)) {
            /* stealing a grab from another window! */
            current_video.grabbed_window->flags &= ~(SDL_WINDOW_MOUSE_GRABBED | SDL_WINDOW_KEYBOARD_GRABBED);
            if (current_video.SetWindowMouseGrab) {
                current_video.SetWindowMouseGrab(current_video.grabbed_window, SDL_FALSE);
            }
            if (current_video.SetWindowKeyboardGrab) {
                current_video.SetWindowKeyboardGrab(current_video.grabbed_window, SDL_FALSE);
            }
        }
        current_video.grabbed_window = window;
    } else if (current_video.grabbed_window == window) {
        current_video.grabbed_window = NULL; /* ungrabbing input. */
    }

    if (current_video.SetWindowMouseGrab) {
        current_video.SetWindowMouseGrab(window, mouse_grabbed);
    }
    if (current_video.SetWindowKeyboardGrab) {
        current_video.SetWindowKeyboardGrab(window, keyboard_grabbed);
    }
}

void SDL_SetWindowGrab(SDL_Window *window, SDL_bool grabbed)
{
    CHECK_WINDOW_MAGIC(window, );

    SDL_SetWindowMouseGrab(window, grabbed);

    if (SDL_GetHintBoolean(SDL_HINT_GRAB_KEYBOARD, SDL_FALSE)) {
        SDL_SetWindowKeyboardGrab(window, grabbed);
    }
}

void SDL_SetWindowKeyboardGrab(SDL_Window *window, SDL_bool grabbed)
{
    CHECK_WINDOW_MAGIC(window, );

    if (!!grabbed == !!(window->flags & SDL_WINDOW_KEYBOARD_GRABBED)) {
        return;
    }
    if (grabbed) {
        window->flags |= SDL_WINDOW_KEYBOARD_GRABBED;
    } else {
        window->flags &= ~SDL_WINDOW_KEYBOARD_GRABBED;
    }
    SDL_UpdateWindowGrab(window);
}

void SDL_SetWindowMouseGrab(SDL_Window *window, SDL_bool grabbed)
{
    CHECK_WINDOW_MAGIC(window, );

    if (!!grabbed == !!(window->flags & SDL_WINDOW_MOUSE_GRABBED)) {
        return;
    }
    if (grabbed) {
        window->flags |= SDL_WINDOW_MOUSE_GRABBED;
    } else {
        window->flags &= ~SDL_WINDOW_MOUSE_GRABBED;
    }
    SDL_UpdateWindowGrab(window);
}

SDL_bool SDL_GetWindowGrab(SDL_Window *window)
{
    return SDL_GetWindowKeyboardGrab(window) || SDL_GetWindowMouseGrab(window);
}

SDL_bool SDL_GetWindowKeyboardGrab(SDL_Window *window)
{
    CHECK_WINDOW_MAGIC(window, SDL_FALSE);
    return window == current_video.grabbed_window && (current_video.grabbed_window->flags & SDL_WINDOW_KEYBOARD_GRABBED);
}

SDL_bool SDL_GetWindowMouseGrab(SDL_Window *window)
{
    CHECK_WINDOW_MAGIC(window, SDL_FALSE);
    return window == current_video.grabbed_window && (current_video.grabbed_window->flags & SDL_WINDOW_MOUSE_GRABBED);
}

SDL_Window *SDL_GetGrabbedWindow(void)
{
    if (current_video.grabbed_window &&
        (current_video.grabbed_window->flags & (SDL_WINDOW_MOUSE_GRABBED | SDL_WINDOW_KEYBOARD_GRABBED))) {
        return current_video.grabbed_window;
    } else {
        return NULL;
    }
}

int SDL_SetWindowMouseRect(SDL_Window *window, const SDL_Rect *rect)
{
    CHECK_WINDOW_MAGIC(window, -1);

    if (rect) {
        SDL_memcpy(&window->mouse_rect, rect, sizeof(*rect));
    } else {
        SDL_zero(window->mouse_rect);
    }

    if (current_video.SetWindowMouseRect) {
        current_video.SetWindowMouseRect(window);
    }
    return 0;
}

const SDL_Rect *SDL_GetWindowMouseRect(SDL_Window *window)
{
    CHECK_WINDOW_MAGIC(window, NULL);

    if (SDL_RectEmpty(&window->mouse_rect)) {
        return NULL;
    } else {
        return &window->mouse_rect;
    }
}

int SDL_FlashWindow(SDL_Window *window, SDL_FlashOperation operation)
{
    CHECK_WINDOW_MAGIC(window, -1);

    if (operation >= 0 && operation < SDL_FLASH_MAX && current_video.FlashWindow) {
        return current_video.FlashWindow(window, operation);
    }

    return SDL_Unsupported();
}

void SDL_OnWindowShown(SDL_Window *window)
{
    SDL_OnWindowRestored(window);
}

void SDL_OnWindowHidden(SDL_Window *window)
{
    SDL_UpdateFullscreenMode(window, SDL_FALSE);
}

static void UpdateWindowDisplay(SDL_Window *window)
{
    int prev_display_index = window->display_index;
    int new_display_index = SDL_CalculateWindowDisplayIndex(window);

    if (new_display_index == prev_display_index) {
        return;
    }
    window->display_index = new_display_index;

    if (prev_display_index >= 0 && prev_display_index < current_video.num_displays) {
        SDL_VideoDisplay *display = &current_video.displays[prev_display_index];
        if (display->fullscreen_window == window) {
            display->fullscreen_window = NULL;
            if (new_display_index >= 0) {
                SDL_VideoDisplay *new_display = &current_video.displays[new_display_index];
                /* The window was moved to a different display */
                if (new_display->fullscreen_window) {
                    /* Uh oh, there's already a fullscreen window here */
                } else {
                    new_display->fullscreen_window = window;
                }
            }
        }
    }

    if (new_display_index != -1) {
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_DISPLAY_CHANGED, new_display_index, 0);
    }
}

void SDL_OnWindowResized(SDL_Window *window)
{
    window->surface_valid = SDL_FALSE;

    if (!window->is_destroying) {
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_SIZE_CHANGED, window->wrect.w, window->wrect.h);

        UpdateWindowDisplay(window);
    }
}

void SDL_OnWindowMoved(SDL_Window *window)
{
    if (!window->is_destroying) {
        UpdateWindowDisplay(window);
    }
}

void SDL_OnWindowMinimized(SDL_Window *window)
{
    if (!DisableUnsetFullscreenOnMinimize()) {
        SDL_UpdateFullscreenMode(window, SDL_FALSE);
    }
}

void SDL_OnWindowRestored(SDL_Window *window)
{
    /*
     * FIXME: Is this fine to just remove this, or should it be preserved just
     * for the fullscreen case? In principle it seems like just hiding/showing
     * windows shouldn't affect the stacking order; maybe the right fix is to
     * re-decouple OnWindowShown and OnWindowRestored.
     * -- update UIKit_RaiseWindow in case this is reactivated
     */
    /*SDL_RaiseWindow(window);*/

    if (FULLSCREEN_VISIBLE(window)) {
        SDL_UpdateFullscreenMode(window, SDL_TRUE);
    }
}

void SDL_OnWindowEnter(SDL_Window *window)
{
#if 0 // -- see WIN_OnWindowEnter
    if (current_video.OnWindowEnter) {
        current_video.OnWindowEnter(window);
    }
#endif
}

void SDL_OnWindowLeave(SDL_Window *window)
{
}

void SDL_OnWindowFocusGained(SDL_Window *window)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    if (window->gamma && current_video.SetWindowGammaRamp) {
        current_video.SetWindowGammaRamp(window, window->gamma);
    }

    if (mouse && mouse->relative_mode) {
        SDL_SetMouseFocus(window);
        if (mouse->relative_mode_warp) {
            SDL_PerformWarpMouseInWindow(window, window->wrect.w / 2u, window->wrect.h / 2u, SDL_TRUE);
        }
    }

    SDL_UpdateWindowGrab(window);
}

static SDL_bool ShouldMinimizeOnFocusLoss(SDL_Window *window)
{
    const char *hint;

    if (!(window->flags & SDL_WINDOW_FULLSCREEN) || window->is_destroying) {
        return SDL_FALSE;
    }

#if defined(__MACOSX__) && defined(SDL_VIDEO_DRIVER_COCOA)
    if (SDL_GetVideoDeviceId() == SDL_VIDEODRIVER_COCOA) {  /* don't do this for X11, etc */
        if (Cocoa_IsWindowInFullscreenSpace(window)) {
            return SDL_FALSE;
        }
    }
#endif

#ifdef __ANDROID__
    {
        extern SDL_bool Android_JNI_ShouldMinimizeOnFocusLoss(void);
        if (!Android_JNI_ShouldMinimizeOnFocusLoss()) {
            return SDL_FALSE;
        }
    }
#endif

    /* Real fullscreen windows should minimize on focus loss so the desktop video mode is restored */
    hint = SDL_GetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS);
    if (!hint || !*hint || SDL_strcasecmp(hint, "auto") == 0) {
        if ((window->flags & SDL_WINDOW_FULLSCREEN_MASK) == SDL_WINDOW_FULLSCREEN_DESKTOP ||
            DisableDisplayModeSwitching()) {
            return SDL_FALSE;
        } else {
            return SDL_TRUE;
        }
    }
    return SDL_GetHintBoolean(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, SDL_FALSE);
}

void SDL_OnWindowFocusLost(SDL_Window *window)
{
    if (window->gamma && current_video.SetWindowGammaRamp) {
        current_video.SetWindowGammaRamp(window, window->saved_gamma);
    }

    SDL_UpdateWindowGrab(window);

    if (ShouldMinimizeOnFocusLoss(window)) {
        SDL_MinimizeWindow(window);
    }
}

/* !!! FIXME: is this different than SDL_GetKeyboardFocus()?
   !!! FIXME:  Also, SDL_GetKeyboardFocus() is O(1), this isn't. */
SDL_Window *SDL_GetFocusWindow(void)
{
    SDL_Window *window;

    // TEST_VIDEO(NULL)

    for (window = current_video.windows; window; window = window->next) {
        if (window->flags & SDL_WINDOW_INPUT_FOCUS) {
            return window;
        }
    }
    return NULL;
}

SDL_Window *SDL_CreateShapedWindow(const char *title, unsigned int x, unsigned int y, unsigned int w, unsigned int h, Uint32 flags)
{
    SDL_Window *result = NULL;
    if (current_video.CreateShaper != NULL) {
        result = SDL_CreateWindow(title, -1000, -1000, w, h, (flags | SDL_WINDOW_BORDERLESS) & (~SDL_WINDOW_FULLSCREEN_MASK) & (~SDL_WINDOW_RESIZABLE) /* & (~SDL_WINDOW_SHOWN) */);
        if (result) {
            result->shaper = current_video.CreateShaper(result);
            if (result->shaper) {
                result->shaper->userx = x;
                result->shaper->usery = y;
                SDL_assert(result->shaper->mode.mode == ShapeModeDefault);
                SDL_assert(result->shaper->mode.parameters.binarizationCutoff == 1);
                SDL_assert(result->shaper->hasshape == SDL_FALSE);
            } else {
                SDL_DestroyWindow(result);
                result = NULL;
            }
        }
    } else {
        SDL_Unsupported();
    }
    return result;
}

int SDL_SetWindowShape(SDL_Window *window, SDL_Surface *shape, SDL_WindowShapeMode *shape_mode)
{
    int result;

    CHECK_WINDOW_MAGIC(window, SDL_NONSHAPEABLE_WINDOW)

    if (!SDL_IsShapedWindow(window)) {
        /* The window given was not a shapeable window. */
        return SDL_NONSHAPEABLE_WINDOW;
    }
    if (!shape || shape->w != window->wrect.w || shape->h != window->wrect.h) {
        /* Invalid shape argument. */
        return SDL_INVALID_SHAPE_ARGUMENT;
    }

    if (!shape_mode) {
        shape_mode = &window->shaper->mode;
    } else if (shape_mode->mode != ShapeModeDefault &&
        shape_mode->mode != ShapeModeBinarizeAlpha &&
        shape_mode->mode != ShapeModeReverseBinarizeAlpha &&
        shape_mode->mode != ShapeModeColorKey) {
        return SDL_INVALID_SHAPE_ARGUMENT;
    }

    if (!shape->format ||
        (shape->format->BytesPerPixel == 0 || shape->format->BytesPerPixel > 4) ||
        (shape->format->Amask == 0 && SDL_SHAPEMODEALPHA(shape_mode->mode))) {
        return SDL_INVALID_SHAPE_ARGUMENT;
    }

    result = current_video.SetWindowShape(window, shape, shape_mode);
    if (result == 0) {
        window->shaper->hasshape = SDL_TRUE;
        window->shaper->mode = *shape_mode;

        if (window->shaper->userx != 0 && window->shaper->usery != 0) {
            int x = window->shaper->userx;
            int y = window->shaper->usery;

            CalculateWindowPosition(&x, &y, window->wrect.w, window->wrect.h);

            SDL_SetWindowPosition(window, x, y);
            window->shaper->userx = 0;
            window->shaper->usery = 0;
        }
    } else {
        if (window->shaper->hasshape) {
            SDL_SetWindowPosition(window, -1000, -1000);
        }
        window->shaper->hasshape = SDL_FALSE;
    }
    return result;
}

void SDL_DestroyWindow(SDL_Window *window)
{
    SDL_VideoDisplay *display;

    CHECK_WINDOW_MAGIC(window, );

    window->is_destroying = SDL_TRUE;

    /* Restore video mode, etc. */
//    if (!(window->flags & SDL_WINDOW_FOREIGN)) {
        SDL_HideWindow(window);
//    }

    /* Make sure this window no longer has focus */
    if (SDL_GetKeyboardFocus() == window) {
        SDL_SetKeyboardFocus(NULL);
    }
    if (SDL_GetMouseFocus() == window) {
        SDL_SetMouseFocus(NULL);
    }

#ifdef SDL_VIDEO_OPENGL_ANY
    /* make no context current if this is the current context window. */
    if (current_video.current_glwin == window) { // TODO: SDL_GL_GetCurrentWindow() == window ?
        SDL_assert(window->flags & SDL_WINDOW_OPENGL);
        SDL_GL_MakeCurrent(NULL, NULL);
    }
#endif
    if (current_video.checked_texture_framebuffer) { /* never checked? No framebuffer to destroy. Don't risk calling the wrong implementation. */
        if (current_video.DestroyWindowFramebuffer) {
            current_video.DestroyWindowFramebuffer(window);
        }
    }
    SDL_DestroyWindowSurface(window);
    if (current_video.DestroyWindow) {
        current_video.DestroyWindow(window);
    }
    if (window->flags & SDL_WINDOW_OPENGL) {
        SDL_GL_UnloadLibrary();
    }
    if (window->flags & SDL_WINDOW_VULKAN) {
        SDL_Vulkan_UnloadLibrary();
    }

    display = SDL_GetDisplayForWindow(window);
    if (display->fullscreen_window == window) {
        display->fullscreen_window = NULL;
    }

    if (current_video.grabbed_window == window) {
        current_video.grabbed_window = NULL; /* ungrabbing input. */
    }

    if (current_video.wakeup_window == window) {
        current_video.wakeup_window = NULL;
    }

    /* Now invalidate magic */
    window->magic = NULL;

    /* Free memory associated with the window */
    SDL_free(window->title);
    SDL_FreeSurface(window->icon);
    SDL_free(window->gamma);
    while (window->data) {
        SDL_WindowUserData *data = window->data;

        window->data = data->next;
        SDL_free(data->name);
        SDL_free(data);
    }

    /* Unlink the window from the list */
    if (window->next) {
        window->next->prev = window->prev;
    }
    if (window->prev) {
        window->prev->next = window->next;
    } else {
        current_video.windows = window->next;
    }

    SDL_free(window);
}

SDL_bool SDL_IsScreenSaverEnabled(void)
{
    // TEST_VIDEO(SDL_TRUE)

    return current_video.suspend_screensaver ? SDL_FALSE : SDL_TRUE;
}

void SDL_EnableScreenSaver(void)
{
    // TEST_VIDEO( )

    if (!current_video.suspend_screensaver) {
        return;
    }
    current_video.suspend_screensaver = SDL_FALSE;
    if (current_video.SuspendScreenSaver) {
        current_video.SuspendScreenSaver(SDL_FALSE);
    }
}

void SDL_DisableScreenSaver(void)
{
    TEST_VIDEO( )

    if (current_video.suspend_screensaver) {
        return;
    }
    current_video.suspend_screensaver = SDL_TRUE;
    if (current_video.SuspendScreenSaver) {
        current_video.SuspendScreenSaver(SDL_TRUE);
    }
}

void SDL_VideoQuit(void)
{
    int i;

    TEST_VIDEO( )

    /* Halt event processing before doing anything else */
    SDL_TouchQuit();
    SDL_MouseQuit();
    SDL_KeyboardQuit();
    SDL_QuitSubSystem(SDL_INIT_EVENTS);

    SDL_EnableScreenSaver();

    /* Clean up the system video */
    while (current_video.windows) {
        SDL_DestroyWindow(current_video.windows);
    }
    current_video.VideoQuit(&current_video);

    for (i = 0; i < current_video.num_displays; ++i) {
        SDL_VideoDisplay *display = &current_video.displays[i];
        SDL_PrivateResetDisplayModes(display);
        SDL_free(display->driverdata);
        SDL_free(display->name);
    }

    SDL_free(current_video.displays);
    current_video.displays = NULL;
    current_video.num_displays = 0;

    SDL_free(current_video.clipboard_text);
    current_video.clipboard_text = NULL;
    SDL_free(current_video.primary_selection_text);
    current_video.primary_selection_text = NULL;
    current_video.DeleteDevice(&current_video);
    SDL_zero(current_video);
}
#ifdef SDL_VIDEO_OPENGL_ANY
int SDL_GL_LoadLibrary(const char *path)
{
    int retval;

    CHECK_VIDEO(-1)

    if (current_video.gl_config.driver_loaded) {
        retval = 0;
    } else {
        if (!current_video.GL_LoadLibrary) {
            return SDL_DllNotSupported("OpenGL");
        }
        retval = current_video.GL_LoadLibrary(&current_video, path);
    }
    if (retval == 0) {
        ++current_video.gl_config.driver_loaded;
    }
    return retval;
}

void *SDL_GL_GetProcAddress(const char *proc)
{
    void *func;

    // CHECK_VIDEO(NULL)

    func = NULL;
    if (current_video.gl_config.driver_loaded) {
        if (current_video.GL_GetProcAddress) {
            func = current_video.GL_GetProcAddress(proc);
        } else {
            SDL_DllNotSupported("GL");
        }
    } else {
        SDL_SetError("No GL driver has been loaded");
    }
    return func;
}

void SDL_GL_UnloadLibrary(void)
{
    // CHECK_VIDEO( )

    if (current_video.gl_config.driver_loaded > 0) {
        if (--current_video.gl_config.driver_loaded > 0) {
            return;
        }
        current_video.GL_UnloadLibrary(&current_video);
    }
}

static SDL_INLINE SDL_bool isAtLeastGL3(const char *verstr)
{
    return verstr && (SDL_atoi(verstr) >= 3);
}

SDL_bool SDL_GL_ExtensionSupported(const char *extension)
{
    const GLubyte *(APIENTRY * glGetStringFunc)(GLenum);
    const char *extensions;
    const char *start;
    const char *where, *terminator;

    /* Extension names should not have spaces. */
    where = SDL_strchr(extension, ' ');
    if (where || *extension == '\0') {
        return SDL_FALSE;
    }
    /* See if there's an environment variable override */
    start = SDL_getenv(extension);
    if (start && *start == '0') {
        return SDL_FALSE;
    }

    /* Lookup the available extensions */

    glGetStringFunc = SDL_GL_GetProcAddress("glGetString");
    if (!glGetStringFunc) {
        return SDL_FALSE;
    }

    if (isAtLeastGL3((const char *)glGetStringFunc(GL_VERSION))) {
        const GLubyte *(APIENTRY * glGetStringiFunc)(GLenum, GLuint);
        void(APIENTRY * glGetIntegervFunc)(GLenum pname, GLint * params);
        GLint num_exts = 0;
        GLint i;

        glGetStringiFunc = SDL_GL_GetProcAddress("glGetStringi");
        glGetIntegervFunc = SDL_GL_GetProcAddress("glGetIntegerv");
        if ((!glGetStringiFunc) || (!glGetIntegervFunc)) {
            return SDL_FALSE;
        }

#ifndef GL_NUM_EXTENSIONS
#define GL_NUM_EXTENSIONS 0x821D
#endif
        glGetIntegervFunc(GL_NUM_EXTENSIONS, &num_exts);
        for (i = 0; i < num_exts; i++) {
            const char *thisext = (const char *)glGetStringiFunc(GL_EXTENSIONS, i);
            if (SDL_strcmp(thisext, extension) == 0) {
                return SDL_TRUE;
            }
        }

        return SDL_FALSE;
    }

    /* Try the old way with glGetString(GL_EXTENSIONS) ... */

    extensions = (const char *)glGetStringFunc(GL_EXTENSIONS);
    if (!extensions) {
        return SDL_FALSE;
    }
    /*
     * It takes a bit of care to be fool-proof about parsing the OpenGL
     * extensions string. Don't be fooled by sub-strings, etc.
     */

    start = extensions;

    for (;;) {
        where = SDL_strstr(start, extension);
        if (!where) {
            break;
        }

        terminator = where + SDL_strlen(extension);
        if (where == extensions || *(where - 1) == ' ') {
            if (*terminator == ' ' || *terminator == '\0') {
                return SDL_TRUE;
            }
        }

        start = terminator;
    }
    return SDL_FALSE;
}

/* Deduce supported ES profile versions from the supported
   ARB_ES*_compatibility extensions. There is no direct query.

   This is normally only called when the OpenGL driver supports
   {GLX,WGL}_EXT_create_context_es2_profile.
 */
void SDL_GL_DeduceMaxSupportedESProfile(int *major, int *minor)
{
/* THIS REQUIRES AN EXISTING GL CONTEXT THAT HAS BEEN MADE CURRENT. */
/*  Please refer to https://bugzilla.libsdl.org/show_bug.cgi?id=3725 for discussion. */
    /* XXX This is fragile; it will break in the event of release of
     * new versions of OpenGL ES.
     */
    if (SDL_GL_ExtensionSupported("GL_ARB_ES3_2_compatibility")) {
        *major = 3;
        *minor = 2;
    } else if (SDL_GL_ExtensionSupported("GL_ARB_ES3_1_compatibility")) {
        *major = 3;
        *minor = 1;
    } else if (SDL_GL_ExtensionSupported("GL_ARB_ES3_compatibility")) {
        *major = 3;
        *minor = 0;
    } else {
        *major = 2;
        *minor = 0;
    }
}

void SDL_GL_ResetAttributes(void)
{
    // TEST_VIDEO( )

    current_video.gl_config.red_size = 3;
    current_video.gl_config.green_size = 3;
    current_video.gl_config.blue_size = 2;
    current_video.gl_config.alpha_size = 0;
    current_video.gl_config.buffer_size = 0;
    current_video.gl_config.depth_size = 16;
    current_video.gl_config.stencil_size = 0;
    current_video.gl_config.double_buffer = 1;
    current_video.gl_config.accum_red_size = 0; // SDL_VIDEO_OPENGL
    current_video.gl_config.accum_green_size = 0; // SDL_VIDEO_OPENGL
    current_video.gl_config.accum_blue_size = 0; // SDL_VIDEO_OPENGL
    current_video.gl_config.accum_alpha_size = 0; // SDL_VIDEO_OPENGL
    current_video.gl_config.stereo = 0; // SDL_VIDEO_OPENGL
    current_video.gl_config.multisamplebuffers = 0;
    current_video.gl_config.multisamplesamples = 0;
    current_video.gl_config.floatbuffers = 0;
    current_video.gl_config.accelerated = -1; /* accelerated or not, both are fine */
    current_video.gl_config.flags = 0;
    current_video.gl_config.share_with_current_context = 0;
    current_video.gl_config.release_behavior = SDL_GL_CONTEXT_RELEASE_BEHAVIOR_FLUSH;  // SDL_VIDEO_OPENGL
    current_video.gl_config.reset_notification = SDL_GL_CONTEXT_RESET_NO_NOTIFICATION; // SDL_VIDEO_OPENGL
    current_video.gl_config.framebuffer_srgb_capable = 0;
    current_video.gl_config.no_error = 0;
    current_video.gl_config.retained_backing = 1; // SDL_VIDEO_DRIVER_UIKIT && (defined(SDL_VIDEO_OPENGL_ES) || defined(SDL_VIDEO_OPENGL_ES2))
    current_video.gl_config.egl_surfacetype = 0;

#ifdef SDL_VIDEO_OPENGL
    current_video.gl_config.major_version = 2;
    current_video.gl_config.minor_version = 1;
    current_video.gl_config.profile_mask = 0;
#elif defined(SDL_VIDEO_OPENGL_ES2)
    current_video.gl_config.major_version = 2;
    current_video.gl_config.minor_version = 0;
    current_video.gl_config.profile_mask = SDL_GL_CONTEXT_PROFILE_ES;
#elif defined(SDL_VIDEO_OPENGL_ES)
    current_video.gl_config.major_version = 1;
    current_video.gl_config.minor_version = 1;
    current_video.gl_config.profile_mask = SDL_GL_CONTEXT_PROFILE_ES;
#endif
}

int SDL_GL_SetAttribute(SDL_GLattr attr, int value)
{
    int retval;

    CHECK_VIDEO(-1)

    retval = 0;
    switch (attr) {
    case SDL_GL_RED_SIZE:
        current_video.gl_config.red_size = value;
        break;
    case SDL_GL_GREEN_SIZE:
        current_video.gl_config.green_size = value;
        break;
    case SDL_GL_BLUE_SIZE:
        current_video.gl_config.blue_size = value;
        break;
    case SDL_GL_ALPHA_SIZE:
        current_video.gl_config.alpha_size = value;
        break;
    case SDL_GL_DOUBLEBUFFER:
        current_video.gl_config.double_buffer = value;
        break;
    case SDL_GL_BUFFER_SIZE:
        current_video.gl_config.buffer_size = value;
        break;
    case SDL_GL_DEPTH_SIZE:
        current_video.gl_config.depth_size = value;
        break;
    case SDL_GL_STENCIL_SIZE:
        current_video.gl_config.stencil_size = value;
        break;
    case SDL_GL_ACCUM_RED_SIZE:
        current_video.gl_config.accum_red_size = value;
        break;
    case SDL_GL_ACCUM_GREEN_SIZE:
        current_video.gl_config.accum_green_size = value;
        break;
    case SDL_GL_ACCUM_BLUE_SIZE:
        current_video.gl_config.accum_blue_size = value;
        break;
    case SDL_GL_ACCUM_ALPHA_SIZE:
        current_video.gl_config.accum_alpha_size = value;
        break;
    case SDL_GL_STEREO:
        current_video.gl_config.stereo = value;
        break;
    case SDL_GL_MULTISAMPLEBUFFERS:
        current_video.gl_config.multisamplebuffers = value;
        break;
    case SDL_GL_MULTISAMPLESAMPLES:
        current_video.gl_config.multisamplesamples = value;
        break;
    case SDL_GL_FLOATBUFFERS:
        current_video.gl_config.floatbuffers = value;
        break;
    case SDL_GL_ACCELERATED_VISUAL:
        current_video.gl_config.accelerated = value;
        break;
    case SDL_GL_RETAINED_BACKING:
        current_video.gl_config.retained_backing = value;
        break;
    case SDL_GL_CONTEXT_MAJOR_VERSION:
        current_video.gl_config.major_version = value;
        break;
    case SDL_GL_CONTEXT_MINOR_VERSION:
        current_video.gl_config.minor_version = value;
        break;
    case SDL_GL_CONTEXT_EGL:
        /* FIXME: SDL_GL_CONTEXT_EGL to be deprecated in SDL 2.1 */
        current_video.gl_config.profile_mask = value != 0 ? SDL_GL_CONTEXT_PROFILE_ES : 0;
        break;
    case SDL_GL_CONTEXT_FLAGS:
        if (value & ~(
#ifdef DEBUG_RENDER
                      SDL_GL_CONTEXT_DEBUG_FLAG |
#endif
                      SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG |
                      SDL_GL_CONTEXT_ROBUST_ACCESS_FLAG |
                      SDL_GL_CONTEXT_RESET_ISOLATION_FLAG)) {
            retval = SDL_SetError("Unknown OpenGL context flag %d", value);
            break;
        }
        current_video.gl_config.flags = value;
        break;
    case SDL_GL_CONTEXT_PROFILE_MASK:
        if (value != 0 &&
            value != SDL_GL_CONTEXT_PROFILE_CORE &&
            value != SDL_GL_CONTEXT_PROFILE_COMPATIBILITY &&
            value != SDL_GL_CONTEXT_PROFILE_ES) {
            retval = SDL_SetError("Unknown OpenGL context profile %d", value);
            break;
        }
        current_video.gl_config.profile_mask = value;
        break;
    case SDL_GL_SHARE_WITH_CURRENT_CONTEXT:
        current_video.gl_config.share_with_current_context = value;
        break;
    case SDL_GL_FRAMEBUFFER_SRGB_CAPABLE:
        current_video.gl_config.framebuffer_srgb_capable = value;
        break;
    case SDL_GL_CONTEXT_RELEASE_BEHAVIOR:
        current_video.gl_config.release_behavior = value;
        break;
    case SDL_GL_CONTEXT_RESET_NOTIFICATION:
        current_video.gl_config.reset_notification = value;
        break;
    case SDL_GL_CONTEXT_NO_ERROR:
        current_video.gl_config.no_error = value;
        break;
    case SDL_GL_SURFACETYPE_EGL:
        current_video.gl_config.egl_surfacetype = value;
        break;
    default:
        retval = SDL_SetError("Unknown OpenGL attribute");
        break;
    }
    return retval;
}

int SDL_GL_GetAttribute(SDL_GLattr attr, int *value)
{
    GLenum(APIENTRY * glGetErrorFunc)(void);
    GLenum attrib = 0;
    GLenum error = 0;

    /*
     * Some queries in Core Profile desktop OpenGL 3+ contexts require
     * glGetFramebufferAttachmentParameteriv instead of glGetIntegerv. Note that
     * the enums we use for the former function don't exist in OpenGL ES 2, and
     * the function itself doesn't exist prior to OpenGL 3 and OpenGL ES 2.
     */
#ifdef SDL_VIDEO_OPENGL
    const GLubyte *(APIENTRY * glGetStringFunc)(GLenum name);
    void(APIENTRY * glGetFramebufferAttachmentParameterivFunc)(GLenum target, GLenum attachment, GLenum pname, GLint * params);
    GLenum attachment = GL_BACK_LEFT;
    GLenum attachmentattrib = 0;
#endif

    if (!value) {
        return SDL_InvalidParamError("value");
    }

    /* Clear value in any case */
    *value = 0;

    CHECK_VIDEO(-1)

    switch (attr) {
    case SDL_GL_RED_SIZE:
#ifdef SDL_VIDEO_OPENGL
        attachmentattrib = GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE;
#endif
        attrib = GL_RED_BITS;
        break;
    case SDL_GL_BLUE_SIZE:
#ifdef SDL_VIDEO_OPENGL
        attachmentattrib = GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE;
#endif
        attrib = GL_BLUE_BITS;
        break;
    case SDL_GL_GREEN_SIZE:
#ifdef SDL_VIDEO_OPENGL
        attachmentattrib = GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE;
#endif
        attrib = GL_GREEN_BITS;
        break;
    case SDL_GL_ALPHA_SIZE:
#ifdef SDL_VIDEO_OPENGL
        attachmentattrib = GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE;
#endif
        attrib = GL_ALPHA_BITS;
        break;
    case SDL_GL_DOUBLEBUFFER:
#ifdef SDL_VIDEO_OPENGL
        attrib = GL_DOUBLEBUFFER;
        break;
#else
        /* OpenGL ES 1.0 and above specifications have EGL_SINGLE_BUFFER      */
        /* parameter which switches double buffer to single buffer. OpenGL ES */
        /* SDL driver must set proper value after initialization              */
        *value = current_video.gl_config.double_buffer;
        return 0;
#endif
    case SDL_GL_DEPTH_SIZE:
#ifdef SDL_VIDEO_OPENGL
        attachment = GL_DEPTH;
        attachmentattrib = GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE;
#endif
        attrib = GL_DEPTH_BITS;
        break;
    case SDL_GL_STENCIL_SIZE:
#ifdef SDL_VIDEO_OPENGL
        attachment = GL_STENCIL;
        attachmentattrib = GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE;
#endif
        attrib = GL_STENCIL_BITS;
        break;
#ifdef SDL_VIDEO_OPENGL
    case SDL_GL_ACCUM_RED_SIZE:
        attrib = GL_ACCUM_RED_BITS;
        break;
    case SDL_GL_ACCUM_GREEN_SIZE:
        attrib = GL_ACCUM_GREEN_BITS;
        break;
    case SDL_GL_ACCUM_BLUE_SIZE:
        attrib = GL_ACCUM_BLUE_BITS;
        break;
    case SDL_GL_ACCUM_ALPHA_SIZE:
        attrib = GL_ACCUM_ALPHA_BITS;
        break;
    case SDL_GL_STEREO:
        attrib = GL_STEREO;
        break;
#else
    case SDL_GL_ACCUM_RED_SIZE:
    case SDL_GL_ACCUM_GREEN_SIZE:
    case SDL_GL_ACCUM_BLUE_SIZE:
    case SDL_GL_ACCUM_ALPHA_SIZE:
    case SDL_GL_STEREO:
        /* none of these are supported in OpenGL ES */
        *value = 0;
        return 0;
#endif
    case SDL_GL_MULTISAMPLEBUFFERS:
        attrib = GL_SAMPLE_BUFFERS;
        break;
    case SDL_GL_MULTISAMPLESAMPLES:
        attrib = GL_SAMPLES;
        break;
    case SDL_GL_CONTEXT_RELEASE_BEHAVIOR:
#ifdef SDL_VIDEO_OPENGL
        attrib = GL_CONTEXT_RELEASE_BEHAVIOR;
#else
        attrib = GL_CONTEXT_RELEASE_BEHAVIOR_KHR;
#endif
        break;
    case SDL_GL_BUFFER_SIZE:
    {
        int rsize = 0, gsize = 0, bsize = 0, asize = 0;

        /* There doesn't seem to be a single flag in OpenGL for this! */
        if (SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &rsize) < 0) {
            return -1;
        }
        if (SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &gsize) < 0) {
            return -1;
        }
        if (SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &bsize) < 0) {
            return -1;
        }
        if (SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &asize) < 0) {
            return -1;
        }

        *value = rsize + gsize + bsize + asize;
        return 0;
    }
    case SDL_GL_ACCELERATED_VISUAL:
    {
        /* FIXME: How do we get this information? */
        *value = (current_video.gl_config.accelerated != 0);
        return 0;
    }
    case SDL_GL_RETAINED_BACKING:
    {
        *value = current_video.gl_config.retained_backing;
        return 0;
    }
    case SDL_GL_CONTEXT_MAJOR_VERSION:
    {
        *value = current_video.gl_config.major_version;
        return 0;
    }
    case SDL_GL_CONTEXT_MINOR_VERSION:
    {
        *value = current_video.gl_config.minor_version;
        return 0;
    }
    case SDL_GL_CONTEXT_EGL:
    {   /* FIXME: SDL_GL_CONTEXT_EGL to be deprecated in SDL 2.1 */
        *value = current_video.gl_config.profile_mask == SDL_GL_CONTEXT_PROFILE_ES ? 1 : 0;
        return 0;
    }
    case SDL_GL_CONTEXT_FLAGS:
    {
        *value = current_video.gl_config.flags;
        return 0;
    }
    case SDL_GL_CONTEXT_PROFILE_MASK:
    {
        *value = current_video.gl_config.profile_mask;
        return 0;
    }
    case SDL_GL_SHARE_WITH_CURRENT_CONTEXT:
    {
        *value = current_video.gl_config.share_with_current_context;
        return 0;
    }
    case SDL_GL_FRAMEBUFFER_SRGB_CAPABLE:
    {
        *value = current_video.gl_config.framebuffer_srgb_capable;
        return 0;
    }
    case SDL_GL_CONTEXT_NO_ERROR:
    {
        *value = current_video.gl_config.no_error;
        return 0;
    }
    case SDL_GL_CONTEXT_RESET_NOTIFICATION:
    {
        *value = current_video.gl_config.reset_notification;
        return 0;
    }
    case SDL_GL_FLOATBUFFERS:
    {
        *value = current_video.gl_config.floatbuffers;
        return 0;
    }
    case SDL_GL_SURFACETYPE_EGL:
    {
        *value = current_video.gl_config.egl_surfacetype;
        return 0;
    }
    default:
        return SDL_SetError("Unknown OpenGL attribute");
    }

#ifdef SDL_VIDEO_OPENGL
    glGetStringFunc = SDL_GL_GetProcAddress("glGetString");
    if (!glGetStringFunc) {
        return -1;
    }

    if (attachmentattrib && isAtLeastGL3((const char *)glGetStringFunc(GL_VERSION))) {
        /* glGetFramebufferAttachmentParameteriv needs to operate on the window framebuffer for this, so bind FBO 0 if necessary. */
        GLint current_fbo = 0;
        void(APIENTRY * glGetIntegervFunc)(GLenum pname, GLint * params) = SDL_GL_GetProcAddress("glGetIntegerv");
        void(APIENTRY * glBindFramebufferFunc)(GLenum target, GLuint fbo) = SDL_GL_GetProcAddress("glBindFramebuffer");
        if (glGetIntegervFunc && glBindFramebufferFunc) {
            glGetIntegervFunc(GL_DRAW_FRAMEBUFFER_BINDING, &current_fbo);
        }

        glGetFramebufferAttachmentParameterivFunc = SDL_GL_GetProcAddress("glGetFramebufferAttachmentParameteriv");
        if (glGetFramebufferAttachmentParameterivFunc) {
            if (glBindFramebufferFunc && (current_fbo != 0)) {
                glBindFramebufferFunc(GL_DRAW_FRAMEBUFFER, 0);
            }
            glGetFramebufferAttachmentParameterivFunc(GL_FRAMEBUFFER, attachment, attachmentattrib, (GLint *)value);
            if (glBindFramebufferFunc && (current_fbo != 0)) {
                glBindFramebufferFunc(GL_DRAW_FRAMEBUFFER, current_fbo);
            }
        } else {
            return -1;
        }
    } else
#endif
    {
        void(APIENTRY * glGetIntegervFunc)(GLenum pname, GLint * params);
        glGetIntegervFunc = SDL_GL_GetProcAddress("glGetIntegerv");
        if (glGetIntegervFunc) {
            glGetIntegervFunc(attrib, (GLint *)value);
        } else {
            return -1;
        }
    }

    glGetErrorFunc = SDL_GL_GetProcAddress("glGetError");
    if (!glGetErrorFunc) {
        return -1;
    }

    error = glGetErrorFunc();
    if (error != GL_NO_ERROR) {
        if (error == GL_INVALID_ENUM) {
            return SDL_SetError("OpenGL error: GL_INVALID_ENUM");
        } else if (error == GL_INVALID_VALUE) {
            return SDL_SetError("OpenGL error: GL_INVALID_VALUE");
        }
        return SDL_SetError("OpenGL error: %08X", error);
    }
    return 0;
}

#define NOT_AN_OPENGL_WINDOW "The specified window isn't an OpenGL window"

SDL_GLContext SDL_GL_CreateContext(SDL_Window *window)
{
    SDL_GLContext ctx = NULL;
    CHECK_WINDOW_MAGIC(window, NULL);

    if (!(window->flags & SDL_WINDOW_OPENGL)) {
        SDL_SetError(NOT_AN_OPENGL_WINDOW);
        return NULL;
    }

    SDL_assert(current_video.gl_config.driver_loaded);

    ctx = current_video.GL_CreateContext(&current_video, window);

    /* Creating a context is assumed to make it current in the SDL driver. */
    if (ctx) {
        current_video.current_glwin = window;
        current_video.current_glctx = ctx;
        SDL_TLSSet(current_video.current_glwin_tls, window, NULL);
        SDL_TLSSet(current_video.current_glctx_tls, ctx, NULL);
    }
    return ctx;
}

int SDL_GL_MakeCurrent(SDL_Window *window, SDL_GLContext context)
{
    int retval;

    // CHECK_VIDEO(-1)

    if (!current_video.gl_config.driver_loaded) {
        return SDL_SetError("No GL driver has been loaded");
    }

    if (window == SDL_GL_GetCurrentWindow() &&
        context == SDL_GL_GetCurrentContext()) {
        /* We're already current. */
        return 0;
    }

    if (!context) {
        window = NULL;
    } else if (window) {
        CHECK_WINDOW_MAGIC(window, -1);

        if (!(window->flags & SDL_WINDOW_OPENGL)) {
            return SDL_SetError(NOT_AN_OPENGL_WINDOW);
        }
    } else if (!current_video.gl_config.gl_allow_no_surface) {
        return SDL_SetError("Use of OpenGL without a window is not supported on this platform");
    }

    retval = current_video.GL_MakeCurrent(window, context);
    if (retval == 0) {
        current_video.current_glwin = window;
        current_video.current_glctx = context;
        SDL_TLSSet(current_video.current_glwin_tls, window, NULL);
        SDL_TLSSet(current_video.current_glctx_tls, context, NULL);
    }
    return retval;
}

SDL_Window *SDL_GL_GetCurrentWindow(void)
{
    CHECK_VIDEO(NULL)

    return (SDL_Window *)SDL_TLSGet(current_video.current_glwin_tls);
}

SDL_GLContext SDL_GL_GetCurrentContext(void)
{
    CHECK_VIDEO(NULL)

    return (SDL_GLContext)SDL_TLSGet(current_video.current_glctx_tls);
}

void SDL_GL_GetDrawableSize(SDL_Window * window, int *w, int *h)
{
    int filter;

    CHECK_WINDOW_MAGIC(window, );

    if (!w) {
        w = &filter;
    }

    if (!h) {
        h = &filter;
    }

    if (current_video.GL_GetDrawableSize) {
        current_video.GL_GetDrawableSize(window, w, h);
    } else {
        SDL_PrivateGetWindowSizeInPixels(window, w, h);
    }
}

int SDL_GL_SetSwapInterval(int interval)
{
    CHECK_VIDEO(-1)

    if (SDL_GL_GetCurrentContext() == NULL) {
        return SDL_SetError("No OpenGL context has been made current");
    } else if (current_video.GL_SetSwapInterval) {
        SDL_assert(current_video.gl_config.driver_loaded);
        return current_video.GL_SetSwapInterval(interval);
    } else {
        return SDL_Unsupported();
    }
}

int SDL_GL_GetSwapInterval(void)
{
    TEST_VIDEO(0)

    if (SDL_GL_GetCurrentContext() == NULL) {
        return 0;
    } else if (current_video.GL_GetSwapInterval) {
        SDL_assert(current_video.gl_config.driver_loaded);
        return current_video.GL_GetSwapInterval();
    } else {
        return 0;
    }
}

int SDL_GL_SwapWindowWithResult(SDL_Window *window)
{
    CHECK_WINDOW_MAGIC(window, -1);

    if (!(window->flags & SDL_WINDOW_OPENGL)) {
        return SDL_SetError(NOT_AN_OPENGL_WINDOW);
    }

    SDL_assert(current_video.gl_config.driver_loaded);

    if (SDL_GL_GetCurrentWindow() != window) {
        return SDL_SetError("The specified window has not been made current");
    }

    return current_video.GL_SwapWindow(&current_video, window);
}

void SDL_GL_SwapWindow(SDL_Window *window)
{
    SDL_GL_SwapWindowWithResult(window);
}

void SDL_GL_DeleteContext(SDL_GLContext context)
{
    // TEST_VIDEO( )

    if (!context || !current_video.gl_config.driver_loaded) {
        return;
    }

    if (SDL_GL_GetCurrentContext() == context) {
        SDL_GL_MakeCurrent(NULL, NULL);
    }

    current_video.GL_DeleteContext(context);
}
#else
int SDL_GL_LoadLibrary(const char *path)
{
    return SDL_DllNotSupported("OpenGL");
}
void *SDL_GL_GetProcAddress(const char *proc)
{
    SDL_Unsupported();
    return NULL;
}
void SDL_GL_UnloadLibrary(void)
{
}
SDL_bool SDL_GL_ExtensionSupported(const char *extension)
{
    return SDL_FALSE;
}
void SDL_GL_ResetAttributes(void)
{
}
int SDL_GL_SetAttribute(SDL_GLattr attr, int value)
{
    return SDL_Unsupported();
}
int SDL_GL_GetAttribute(SDL_GLattr attr, int *value)
{
    return SDL_Unsupported();
}
SDL_GLContext SDL_GL_CreateContext(SDL_Window *window)
{
    SDL_Unsupported();
    return NULL;
}
int SDL_GL_MakeCurrent(SDL_Window *window, SDL_GLContext context)
{
    return SDL_Unsupported();
}
SDL_Window *SDL_GL_GetCurrentWindow(void)
{
    SDL_Unsupported();
    return NULL;
}
SDL_GLContext SDL_GL_GetCurrentContext(void)
{
    SDL_Unsupported();
    return NULL;
}
void SDL_GL_GetDrawableSize(SDL_Window * window, int *w, int *h)
{
    SDL_Unsupported();
}
int SDL_GL_SetSwapInterval(int interval)
{
    return SDL_Unsupported();
}

int SDL_GL_GetSwapInterval(void)
{
    return 0;
}

int SDL_GL_SwapWindowWithResult(SDL_Window *window)
{
    return SDL_Unsupported();
}

void SDL_GL_SwapWindow(SDL_Window *window)
{
    SDL_GL_SwapWindowWithResult(window);
}

void SDL_GL_DeleteContext(SDL_GLContext context)
{
    SDL_Unsupported();
}
#endif // SDL_VIDEO_OPENGL_ANY
#if 0 /* FIXME */
/*
 * Utility function used by SDL_WM_SetIcon(); flags & 1 for color key, flags
 * & 2 for alpha channel.
 */
static void CreateMaskFromColorKeyOrAlpha(SDL_Surface * icon, Uint8 * mask, int flags)
{
    int x, y;
    Uint32 colorkey;
#define SET_MASKBIT(icon, x, y, mask) \
    mask[(y * ((icon->w + 7) / 8u)) + (x / 8u)] &= ~(0x01 << (7 - (x % 8u)))

    colorkey = icon->format->colorkey;
    switch (icon->format->BytesPerPixel) {
    case 1:
        {
            Uint8 *pixels;
            for (y = 0; y < icon->h; ++y) {
                pixels = (Uint8 *) icon->pixels + y * icon->pitch;
                for (x = 0; x < icon->w; ++x) {
                    if (*pixels++ == colorkey) {
                        SET_MASKBIT(icon, x, y, mask);
                    }
                }
            }
        }
        break;

    case 2:
        {
            Uint16 *pixels;
            for (y = 0; y < icon->h; ++y) {
                pixels = (Uint16 *) icon->pixels + y * icon->pitch / 2u;
                for (x = 0; x < icon->w; ++x) {
                    if ((flags & 1) && *pixels == colorkey) {
                        SET_MASKBIT(icon, x, y, mask);
                    } else if ((flags & 2)
                               && (*pixels & icon->format->Amask) == 0) {
                        SET_MASKBIT(icon, x, y, mask);
                    }
                    pixels++;
                }
            }
        }
        break;

    case 4:
        {
            Uint32 *pixels;
            for (y = 0; y < icon->h; ++y) {
                pixels = (Uint32 *) icon->pixels + y * icon->pitch / 4u;
                for (x = 0; x < icon->w; ++x) {
                    if ((flags & 1) && *pixels == colorkey) {
                        SET_MASKBIT(icon, x, y, mask);
                    } else if ((flags & 2)
                               && (*pixels & icon->format->Amask) == 0) {
                        SET_MASKBIT(icon, x, y, mask);
                    }
                    pixels++;
                }
            }
        }
        break;
    }
}

/*
 * Sets the window manager icon for the display window.
 */
void SDL_WM_SetIcon(SDL_Surface * icon, Uint8 * mask)
{
    if (icon && current_video.SetIcon) {
        /* Generate a mask if necessary, and create the icon! */
        if (mask == NULL) {
            int mask_len = icon->h * (icon->w + 7) / 8u;
            int flags = 0;
            mask = (Uint8 *) SDL_malloc(mask_len);
            if (mask == NULL) {
                SDL_OutOfMemory();
                return;
            }
            SDL_memset(mask, ~0, mask_len);
            if (icon->flags & SDL_SRCCOLORKEY)
                flags |= 1;
            if (icon->flags & SDL_SRCALPHA)
                flags |= 2;
            if (flags) {
                CreateMaskFromColorKeyOrAlpha(icon, mask, flags);
            }
            current_video.SetIcon(&current_video, icon, mask);
            SDL_free(mask);
        } else {
            current_video.SetIcon(&current_video, icon, mask);
        }
    }
}
#endif

SDL_bool SDL_GetWindowWMInfo(SDL_Window * window, struct SDL_SysWMinfo *info)
{
    CHECK_WINDOW_MAGIC(window, SDL_FALSE);

    if (!info) {
        SDL_InvalidParamError("info");
        return SDL_FALSE;
    }
    info->subsystem = SDL_SYSWM_UNKNOWN;

    if (!current_video.GetWindowWMInfo) {
        SDL_Unsupported();
        return SDL_FALSE;
    }
    return (current_video.GetWindowWMInfo(window, info));
}
// keyboard
void SDL_StartTextInput(void)
{
    CHECK_VIDEO( )

    SDL_StartTextInputPrivate(SDL_TRUE);
}

void SDL_ClearComposition(void)
{
    CHECK_VIDEO( )

    if (current_video.ClearComposition) {
        current_video.ClearComposition();
    }
}

SDL_bool SDL_IsTextInputShown(void)
{
    CHECK_VIDEO(SDL_FALSE)

    if (current_video.IsTextInputShown) {
        return current_video.IsTextInputShown();
    }

    return SDL_FALSE;
}

SDL_bool SDL_IsTextInputActive(void)
{
    return SDL_GetEventState(SDL_TEXTINPUT) == SDL_ENABLE;
}

void SDL_StopTextInput(void)
{
    SDL_Window *window;

    /* Stop the text input system */
    // if (SDL_HasVideoDevice()) {
        if (current_video.StopTextInput) {
            current_video.StopTextInput();
        }

        /* Hide the on-screen keyboard, if any */
        if (current_video.HideScreenKeyboard && SDL_GetHintBoolean(SDL_HINT_ENABLE_SCREEN_KEYBOARD, SDL_TRUE)) {
            window = SDL_GetFocusWindow();
            if (window) {
                current_video.HideScreenKeyboard(window);
            }
        }
    // }

    /* Finally disable text events */
    (void)SDL_EventState(SDL_TEXTINPUT, SDL_DISABLE);
    (void)SDL_EventState(SDL_TEXTEDITING, SDL_DISABLE);
}

void SDL_SetTextInputRect(const SDL_Rect *rect)
{
    CHECK_VIDEO( )

    if (current_video.SetTextInputRect) {
        if (rect) {
            current_video.SetTextInputRect(rect);
        } else {
            SDL_InvalidParamError("rect");
        }
    }
}

SDL_bool SDL_HasScreenKeyboardSupport(void)
{
    CHECK_VIDEO(SDL_FALSE)

    if (current_video.HasScreenKeyboardSupport) {
        return current_video.HasScreenKeyboardSupport();
    }
    return SDL_FALSE;
}

SDL_bool SDL_IsScreenKeyboardShown(SDL_Window *window)
{
    CHECK_WINDOW_MAGIC(window, SDL_FALSE)

    if (current_video.IsScreenKeyboardShown) {
        return current_video.IsScreenKeyboardShown(window);
    }
    return SDL_FALSE;
}

void SDL_TextInputInit(void)
{
    // TEST_VIDEO( )

    if (current_video.StartTextInput) {
        current_video.StartTextInput();
    }
}

void SDL_TextInputQuit(void)
{
    // TEST_VIDEO( )

    if (current_video.StopTextInput) {
        current_video.StopTextInput();
    }
}

// clipboard
int SDL_SetClipboardText(const char *text)
{
    if (!SDL_HasVideoDevice()) {
        return SDL_SetError("Video subsystem must be initialized to set clipboard text");
    }

    if (!text) {
        text = "";
    }
    if (current_video.SetClipboardText) {
        return current_video.SetClipboardText(text);
    } else {
        SDL_free(current_video.clipboard_text);
        current_video.clipboard_text = SDL_strdup(text);
        return 0;
    }
}

int SDL_SetPrimarySelectionText(const char *text)
{
    if (!SDL_HasVideoDevice()) {
        return SDL_SetError("Video subsystem must be initialized to set primary selection text");
    }

    if (!text) {
        text = "";
    }
    if (current_video.SetPrimarySelectionText) {
        return current_video.SetPrimarySelectionText(text);
    } else {
        SDL_free(current_video.primary_selection_text);
        current_video.primary_selection_text = SDL_strdup(text);
        return 0;
    }
}

char *SDL_GetClipboardText(void)
{
    if (!SDL_HasVideoDevice()) {
        SDL_SetError("Video subsystem must be initialized to get clipboard text");
        return SDL_strdup("");
    }

    if (current_video.GetClipboardText) {
        return current_video.GetClipboardText();
    } else {
        const char *text = current_video.clipboard_text;
        if (!text) {
            text = "";
        }
        return SDL_strdup(text);
    }
}

char *SDL_GetPrimarySelectionText(void)
{
    if (!SDL_HasVideoDevice()) {
        SDL_SetError("Video subsystem must be initialized to get primary selection text");
        return SDL_strdup("");
    }

    if (current_video.GetPrimarySelectionText) {
        return current_video.GetPrimarySelectionText();
    } else {
        const char *text = current_video.primary_selection_text;
        if (!text) {
            text = "";
        }
        return SDL_strdup(text);
    }
}

SDL_bool SDL_HasClipboardText(void)
{
    if (!SDL_HasVideoDevice()) {
        SDL_SetError("Video subsystem must be initialized to check clipboard text");
        return SDL_FALSE;
    }

    if (current_video.HasClipboardText) {
        return current_video.HasClipboardText();
    } else {
        if (current_video.clipboard_text && current_video.clipboard_text[0] != '\0') {
            return SDL_TRUE;
        } else {
            return SDL_FALSE;
        }
    }
}

SDL_bool SDL_HasPrimarySelectionText(void)
{
    if (!SDL_HasVideoDevice()) {
        SDL_SetError("Video subsystem must be initialized to check primary selection text");
        return SDL_FALSE;
    }

    if (current_video.HasPrimarySelectionText) {
        return current_video.HasPrimarySelectionText();
    }

    if (current_video.primary_selection_text && current_video.primary_selection_text[0] != '\0') {
        return SDL_TRUE;
    }

    return SDL_FALSE;
}
// messagebox
int SDL_GetMessageBoxCount(void)
{
    return SDL_AtomicGet(&SDL_messagebox_count);
}

int SDL_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonid)
{
    int dummybutton;
    int retval = -1;
    SDL_bool relative_mode;
    int show_cursor_prev;
    msgBoxFunc *mbFunc = NULL;
    SDL_Window *current_window;
    SDL_MessageBoxData mbdata;

    if (!messageboxdata) {
        return SDL_InvalidParamError("messageboxdata");
    } else if (messageboxdata->numbuttons < 0) {
        return SDL_SetError("Invalid number of buttons");
    }

    (void)SDL_AtomicIncRef(&SDL_messagebox_count);

    current_window = SDL_GetKeyboardFocus();
    relative_mode = SDL_GetRelativeMouseMode();
    SDL_UpdateMouseCapture(SDL_FALSE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    show_cursor_prev = SDL_ShowCursor(SDL_ENABLE);
    SDL_ResetKeyboard();

    if (!buttonid) {
        buttonid = &dummybutton;
    }

    SDL_memcpy(&mbdata, messageboxdata, sizeof(*messageboxdata));
    if (!mbdata.title) {
        mbdata.title = "";
    }
    if (!mbdata.message) {
        mbdata.message = "";
    }
    messageboxdata = &mbdata;

    /* It's completely fine to call this function before video is initialized */
    if (SDL_HasVideoDevice()) {
        mbFunc =  messagebox[SDL_GetVideoDeviceId()];
    }
    if (mbFunc != NULL) {
        retval = mbFunc(messageboxdata, buttonid);
    } else {
        int i;
        for (i = 0; i < SDL_arraysize(messagebox); i++) {
            if (messagebox[i] != NULL) {
                mbFunc = messagebox[i];
                retval = mbFunc(messageboxdata, buttonid);
                if (retval >= 0) {
                    break;
                }
            }
        }
        if (retval < 0 && !mbFunc) {
            SDL_SetError("No message system available");
        }
    }

    (void)SDL_AtomicDecRef(&SDL_messagebox_count);

    if (current_window) {
        SDL_RaiseWindow(current_window);
    }

    SDL_ShowCursor(show_cursor_prev);
    SDL_SetRelativeMouseMode(relative_mode);
    SDL_UpdateMouseCapture(SDL_FALSE);

    return retval;
}

int SDL_ShowSimpleMessageBox(Uint32 flags, const char *title, const char *message, SDL_Window *window)
{
#ifdef __EMSCRIPTEN__
    /* !!! FIXME: propose a browser API for this, get this #ifdef out of here? */
    /* Web browsers don't (currently) have an API for a custom message box
       that can block, but for the most common case (SDL_ShowSimpleMessageBox),
       we can use the standard Javascript alert() function. */
    if (!title) {
        title = "";
    }
    if (!message) {
        message = "";
    }
    EM_ASM({
        alert(UTF8ToString($0) + "\n\n" + UTF8ToString($1));
    },
            title, message);
    return 0;
#else
    SDL_MessageBoxData data;
    SDL_MessageBoxButtonData button;

    SDL_zero(data);
    data.flags = flags;
    data.title = title;
    data.message = message;
    data.numbuttons = 1;
    data.buttons = &button;
    data.window = window;

    SDL_zero(button);
    button.flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    button.flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
    button.text = "OK";

    return SDL_ShowMessageBox(&data, NULL);
#endif
}

SDL_bool SDL_ShouldAllowTopmost(void)
{
    return SDL_GetHintBoolean(SDL_HINT_ALLOW_TOPMOST, SDL_TRUE);
}

int SDL_SetWindowHitTest(SDL_Window *window, SDL_HitTest callback, void *callback_data)
{
    int result;

    CHECK_WINDOW_MAGIC(window, -1);

    if (current_video.SetWindowHitTest) {
        result = current_video.SetWindowHitTest(window, callback != NULL);
    } else {
        result = SDL_Unsupported();
    }
    if (result >= 0) {
        window->hit_test = callback;
        window->hit_test_data = callback_data;
    }

    return result;
}

float SDL_ComputeDiagonalDPI(int hpix, int vpix, float hinches, float vinches)
{
    float den2 = hinches * hinches + vinches * vinches;
    if (den2 <= 0.0f) {
        return 0.0f;
    }

    return (float)(SDL_sqrt((double)hpix * (double)hpix + (double)vpix * (double)vpix) / SDL_sqrt((double)den2));
}

/*
 * Functions used by iOS application delegates
 */
void SDL_OnApplicationWillTerminate(void)
{
    SDL_SendAppEvent(SDL_APP_TERMINATING);
}

void SDL_OnApplicationDidReceiveMemoryWarning(void)
{
    SDL_SendAppEvent(SDL_APP_LOWMEMORY);
}

void SDL_OnApplicationWillResignActive(void)
{
    // if (SDL_HasVideoDevice()) {
        SDL_Window *window;
        for (window = current_video.windows; window; window = window->next) {
            SDL_SendWindowEvent(window, SDL_WINDOWEVENT_FOCUS_LOST, 0, 0);
            SDL_SendWindowEvent(window, SDL_WINDOWEVENT_MINIMIZED, 0, 0);
        }
    // }
    SDL_SendAppEvent(SDL_APP_WILLENTERBACKGROUND);
}

void SDL_OnApplicationDidEnterBackground(void)
{
    SDL_SendAppEvent(SDL_APP_DIDENTERBACKGROUND);
}

void SDL_OnApplicationWillEnterForeground(void)
{
    SDL_SendAppEvent(SDL_APP_WILLENTERFOREGROUND);
}

void SDL_OnApplicationDidBecomeActive(void)
{
    SDL_Window *window;

    SDL_SendAppEvent(SDL_APP_DIDENTERFOREGROUND);

    // if (SDL_HasVideoDevice()) {
        for (window = current_video.windows; window; window = window->next) {
            SDL_SendWindowEvent(window, SDL_WINDOWEVENT_FOCUS_GAINED, 0, 0);
            SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESTORED, 0, 0);
        }
    // }
}

#ifdef SDL_VIDEO_VULKAN
#define NOT_A_VULKAN_WINDOW "The specified window isn't a Vulkan window"

int SDL_Vulkan_LoadLibrary(const char *path)
{
    int retval;

    CHECK_VIDEO(-1)

    if (current_video.vulkan_config.loader_loaded) {
        retval = 0;
    } else {
        if (!current_video.Vulkan_LoadLibrary) {
            return SDL_DllNotSupported("Vulkan");
        }
        retval = current_video.Vulkan_LoadLibrary(&current_video.vulkan_config, path);
    }
    if (retval == 0) {
        current_video.vulkan_config.loader_loaded++;
    }
    return retval;
}

void *SDL_Vulkan_GetVkGetInstanceProcAddr(void)
{
    // CHECK_VIDEO(NULL)

    if (!current_video.vulkan_config.loader_loaded) {
        SDL_SetError("No Vulkan driver has been loaded");
        return NULL;
    }
    return current_video.vulkan_config.vkGetInstanceProcAddr;
}

void SDL_Vulkan_UnloadLibrary(void)
{
    // CHECK_VIDEO( )

    if (current_video.vulkan_config.loader_loaded > 0) {
        if (--current_video.vulkan_config.loader_loaded > 0) {
            return;
        }
        if (current_video.Vulkan_UnloadLibrary) {
            current_video.Vulkan_UnloadLibrary(&current_video.vulkan_config);
        }
    }
}

SDL_bool SDL_Vulkan_GetInstanceExtensions(SDL_Window *window, unsigned *count, const char **names)
{
    CHECK_VIDEO(SDL_FALSE)

    if (!current_video.Vulkan_GetInstanceExtensions) {
        SDL_Unsupported();
        return SDL_FALSE;
    }

    if (!count) {
        SDL_InvalidParamError("count");
        return SDL_FALSE;
    }

    return current_video.Vulkan_GetInstanceExtensions(count, names);
}

SDL_bool SDL_Vulkan_CreateSurface(SDL_Window *window,
                                  VkInstance instance,
                                  VkSurfaceKHR *surface)
{
    CHECK_WINDOW_MAGIC(window, SDL_FALSE);

    if (!instance) {
        SDL_InvalidParamError("instance");
        return SDL_FALSE;
    }

    if (!surface) {
        SDL_InvalidParamError("surface");
        return SDL_FALSE;
    }

    if (!current_video.vulkan_config.loader_loaded) {
        SDL_SetError("No Vulkan driver has been loaded");
        return SDL_FALSE;
    }

    return current_video.Vulkan_CreateSurface(&current_video.vulkan_config, window, instance, surface);
}

void SDL_Vulkan_GetDrawableSize(SDL_Window *window, int *w, int *h)
{
    int filter;

    CHECK_WINDOW_MAGIC(window, );

    if (!w) {
        w = &filter;
    }

    if (!h) {
        h = &filter;
    }

    if (current_video.Vulkan_GetDrawableSize) {
        current_video.Vulkan_GetDrawableSize(window, w, h);
    } else {
        SDL_PrivateGetWindowSizeInPixels(window, w, h);
    }
}
#else
int SDL_Vulkan_LoadLibrary(const char *path)
{
    return SDL_DllNotSupported("Vulkan");
}

void *SDL_Vulkan_GetVkGetInstanceProcAddr(void)
{
    SDL_Unsupported();
    return NULL;
}

void SDL_Vulkan_UnloadLibrary(void)
{
    SDL_Unsupported();
}

SDL_bool SDL_Vulkan_GetInstanceExtensions(SDL_Window *window, unsigned *count, const char **names)
{
    SDL_Unsupported();
    return SDL_FALSE;
}

SDL_bool SDL_Vulkan_CreateSurface(SDL_Window *window,
                                  VkInstance instance,
                                  VkSurfaceKHR *surface)
{
    SDL_Unsupported();
    return SDL_FALSE;
}

void SDL_Vulkan_GetDrawableSize(SDL_Window *window, int *w, int *h)
{
    SDL_Unsupported();
}
#endif // SDL_VIDEO_VULKAN
#ifdef SDL_VIDEO_METAL
SDL_MetalView SDL_Metal_CreateView(SDL_Window *window)
{
    CHECK_WINDOW_MAGIC(window, NULL);

    if (!(window->flags & SDL_WINDOW_METAL)) {
        /* No problem, we can convert to Metal */
        if (window->flags & SDL_WINDOW_OPENGL) {
            window->flags &= ~SDL_WINDOW_OPENGL;
            SDL_GL_UnloadLibrary();
        }
        if (window->flags & SDL_WINDOW_VULKAN) {
            window->flags &= ~SDL_WINDOW_VULKAN;
            SDL_Vulkan_UnloadLibrary();
        }
        window->flags |= SDL_WINDOW_METAL;
    }

    return current_video.Metal_CreateView(window);
}

void SDL_Metal_DestroyView(SDL_MetalView view)
{
    // TEST_VIDEO( )

    if (view && current_video.Metal_DestroyView) {
        current_video.Metal_DestroyView(view);
    }
}

void *SDL_Metal_GetLayer(SDL_MetalView view)
{
    CHECK_VIDEO(NULL)

    if (!view) {
        SDL_InvalidParamError("view");
        return NULL;
    }

    if (current_video.Metal_GetLayer) {
        return current_video.Metal_GetLayer(view);
    } else {
        SDL_Unsupported();
        return NULL;
    }
}

void SDL_Metal_GetDrawableSize(SDL_Window *window, int *w, int *h)
{
    int filter;

    CHECK_WINDOW_MAGIC(window, );

    if (!w) {
        w = &filter;
    }

    if (!h) {
        h = &filter;
    }

    if (current_video.Metal_GetDrawableSize) {
        current_video.Metal_GetDrawableSize(window, w, h);
    } else {
        SDL_PrivateGetWindowSizeInPixels(window, w, h);
    }
}
#else
SDL_MetalView SDL_Metal_CreateView(SDL_Window *window)
{
    SDL_Unsupported();
    return NULL;
}

void SDL_Metal_DestroyView(SDL_MetalView view)
{
    SDL_Unsupported();
}

void *SDL_Metal_GetLayer(SDL_MetalView view)
{
    SDL_Unsupported();
    return NULL;
}

void SDL_Metal_GetDrawableSize(SDL_Window *window, int *w, int *h)
{
    SDL_Unsupported();
}
#endif // SDL_VIDEO_METAL

#if defined(SDL_VIDEO_DRIVER_X11) || defined(SDL_VIDEO_DRIVER_WAYLAND) || defined(SDL_VIDEO_DRIVER_EMSCRIPTEN)
const char *SDL_GetCSSCursorName(SDL_SystemCursor id, const char **fallback_name)
{
    const char* result;
    /* Reference: https://www.w3.org/TR/css-ui-4/#cursor */
    /* Also in: https://www.freedesktop.org/wiki/Specifications/cursor-spec/ */
    SDL_COMPILE_TIME_ASSERT(video_SystemCursors, SDL_NUM_SYSTEM_CURSORS == 12);
    switch (id) {
    case SDL_SYSTEM_CURSOR_ARROW:
        result = "default";
        break;
    case SDL_SYSTEM_CURSOR_IBEAM:
        result = "text";
        break;
    case SDL_SYSTEM_CURSOR_WAIT:
        result = "wait";
        break;
    case SDL_SYSTEM_CURSOR_CROSSHAIR:
        result = "crosshair";
        break;
    case SDL_SYSTEM_CURSOR_WAITARROW:
        result = "progress";
        break;
    case SDL_SYSTEM_CURSOR_SIZENWSE:
        if (fallback_name) {
            /* only a single arrow */
            *fallback_name = "nw-resize";
        }
        result = "nwse-resize";
        break;
    case SDL_SYSTEM_CURSOR_SIZENESW:
        if (fallback_name) {
            /* only a single arrow */
            *fallback_name = "ne-resize";
        }
        result = "nesw-resize";
        break;
    case SDL_SYSTEM_CURSOR_SIZEWE:
        if (fallback_name) {
            *fallback_name = "col-resize";
        }
        result = "ew-resize";
        break;
    case SDL_SYSTEM_CURSOR_SIZENS:
        if (fallback_name) {
            *fallback_name = "row-resize";
        }
        result = "ns-resize";
        break;
    case SDL_SYSTEM_CURSOR_SIZEALL:
        result = "all-scroll";
        break;
    case SDL_SYSTEM_CURSOR_NO:
        result = "not-allowed";
        break;
    case SDL_SYSTEM_CURSOR_HAND:
        result = "pointer";
        break;
#if 0
    case SDL_SYSTEM_CURSOR_WINDOW_TOPLEFT:
        result = "nw-resize";
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_TOP:
        result = "n-resize";
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_TOPRIGHT:
        result = "ne-resize";
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_RIGHT:
        result = "e-resize";
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_BOTTOMRIGHT:
        result = "se-resize";
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_BOTTOM:
        result = "s-resize";
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_BOTTOMLEFT:
        result = "sw-resize";
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_LEFT:
        result = "w-resize";
        break;
#endif
    default:
        SDL_assume(!"Unknown system cursor");
        result = "default";
        break;
    }
    return result;
}
#endif

/* vi: set ts=4 sw=4 expandtab: */
