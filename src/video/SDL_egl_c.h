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

#ifndef SDL_egl_h_
#define SDL_egl_h_

#ifdef SDL_VIDEO_OPENGL_EGL

#include "SDL_egl.h"

#include "SDL_sysvideo.h"

#define SDL_EGL_MAX_DEVICES 8

#if defined(SDL_VIDEO_DRIVER_VITA) || defined(SDL_VIDEO_DRIVER_EMSCRIPTEN)
#define SDL_VIDEO_STATIC_ANGLE 1
#endif

/* Function pointer typedefs for 'new' ANGLE functions, which, unlike
 * the old functions, do not require C++ support and work with plain C.
 */
typedef EGLDisplay(EGLAPIENTRY *eglGetPlatformDisplay_Function) (EGLenum platform, void *native_display, const EGLint *attrib_list);
typedef EGLDisplay(EGLAPIENTRY *eglGetPlatformDisplayEXT_Function)(EGLenum platform, void *native_display, const EGLint *attrib_list);
typedef EGLBoolean(EGLAPIENTRY *eglQueryDevicesEXT_Function)(EGLint max_devices, void **devices, EGLint *num_devices);

typedef struct SDL_EGL_VideoData
{
    void *opengl_dll_handle, *egl_dll_handle;
    EGLDisplay egl_display;
    EGLConfig egl_config;
    int egl_swapinterval;
    int egl_surfacetype;
    int egl_version_major, egl_version_minor;
    EGLint egl_required_visual_id;
    SDL_bool is_offscreen;  /* whether EGL display was offscreen */
    EGLenum apitype;  /* EGL_OPENGL_ES_API, EGL_OPENGL_API, etc */
#ifdef SDL_VIDEO_DRIVER_WINRT
    /* An object created by ANGLE/WinRT (OpenGL ES 2 for WinRT) that gets
     * passed to eglGetDisplay and eglCreateWindowSurface:
     */
    IUnknown *winrt_egl_addon;
#endif
#ifndef SDL_VIDEO_STATIC_ANGLE
    EGLDisplay(EGLAPIENTRY *eglGetDisplay) (NativeDisplayType display);

    // eglGetPlatformDisplay_Function eglGetPlatformDisplay;

    // eglGetPlatformDisplayEXT_Function eglGetPlatformDisplayEXT;

    EGLBoolean(EGLAPIENTRY *eglInitialize) (EGLDisplay dpy, EGLint * major,
                                EGLint * minor);
    EGLBoolean(EGLAPIENTRY  *eglTerminate) (EGLDisplay dpy);

    void *(EGLAPIENTRY *eglGetProcAddress) (const char * procName);

    EGLBoolean(EGLAPIENTRY *eglChooseConfig) (EGLDisplay dpy,
                                  const EGLint * attrib_list,
                                  EGLConfig * configs,
                                  EGLint config_size, EGLint * num_config);

    EGLContext(EGLAPIENTRY *eglCreateContext) (EGLDisplay dpy,
                                   EGLConfig config,
                                   EGLContext share_list,
                                   const EGLint * attrib_list);

    EGLBoolean(EGLAPIENTRY *eglDestroyContext) (EGLDisplay dpy, EGLContext ctx);
#ifdef SDL_VIDEO_DRIVER_OFFSCREEN
    EGLSurface(EGLAPIENTRY *eglCreatePbufferSurface)(EGLDisplay dpy, EGLConfig config,
                                                     EGLint const* attrib_list);
#endif
    EGLSurface(EGLAPIENTRY *eglCreateWindowSurface) (EGLDisplay dpy,
                                         EGLConfig config,
                                         NativeWindowType window,
                                         const EGLint * attrib_list);
    EGLBoolean(EGLAPIENTRY *eglDestroySurface) (EGLDisplay dpy, EGLSurface surface);

    EGLBoolean(EGLAPIENTRY *eglMakeCurrent) (EGLDisplay dpy, EGLSurface draw,
                                 EGLSurface read, EGLContext ctx);

    EGLBoolean(EGLAPIENTRY *eglSwapBuffers) (EGLDisplay dpy, EGLSurface draw);

    EGLBoolean(EGLAPIENTRY *eglSwapInterval) (EGLDisplay dpy, EGLint interval);

    const char *(EGLAPIENTRY *eglQueryString) (EGLDisplay dpy, EGLint name);

    // EGLenum(EGLAPIENTRY *eglQueryAPI)(void);

    EGLBoolean(EGLAPIENTRY  *eglGetConfigAttrib) (EGLDisplay dpy, EGLConfig config,
                                     EGLint attribute, EGLint * value);

    // EGLBoolean(EGLAPIENTRY *eglWaitNative) (EGLint  engine);

    // EGLBoolean(EGLAPIENTRY *eglWaitGL)(void);

    EGLBoolean(EGLAPIENTRY *eglBindAPI)(EGLenum);

    EGLint(EGLAPIENTRY *eglGetError)(void);

    // eglQueryDevicesEXT_Function eglQueryDevicesEXT;

   /* Atomic functions */

    // EGLSyncKHR(EGLAPIENTRY *eglCreateSyncKHR)(EGLDisplay dpy, EGLenum type, const EGLint *attrib_list);

    // EGLBoolean(EGLAPIENTRY *eglDestroySyncKHR)(EGLDisplay dpy, EGLSyncKHR sync);

    // EGLint(EGLAPIENTRY *eglDupNativeFenceFDANDROID)(EGLDisplay dpy, EGLSyncKHR sync);

    // EGLint(EGLAPIENTRY *eglWaitSyncKHR)(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags);

    // EGLint(EGLAPIENTRY *eglClientWaitSyncKHR)(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout);

    /* Atomic functions end */
#endif // !SDL_VIDEO_STATIC_ANGLE
} SDL_EGL_VideoData;

/* OpenGLES functions */
typedef enum SDL_EGL_ExtensionType
{
    SDL_EGL_DISPLAY_EXTENSION,
    SDL_EGL_CLIENT_EXTENSION
} SDL_EGL_ExtensionType;

extern SDL_EGL_VideoData egl_data;

extern int SDL_EGL_InitializeDisplay(EGLDisplay display);
/* SDL_EGL_LoadLibrary can get a display for a specific platform (EGL_PLATFORM_*)
 * or, if 0 is passed, let the implementation decide.
 */
extern int SDL_EGL_LoadLibraryOnly(_THIS, const char *path);
extern int SDL_EGL_LoadLibrary(_THIS, const char *path, NativeDisplayType native_display, EGLenum platform);
extern void *SDL_EGL_GetProcAddress(const char *proc);
extern void SDL_EGL_UnloadLibrary(_THIS);
extern void SDL_EGL_SetRequiredVisualId(int visual_id);
extern int SDL_EGL_ChooseConfig(_THIS);
extern int SDL_EGL_SetSwapInterval(int interval);
extern int SDL_EGL_GetSwapInterval(void);
extern void SDL_EGL_DeleteContext(SDL_GLContext context);
extern EGLSurface *SDL_EGL_CreateSurface(_THIS, NativeWindowType nw);
extern void SDL_EGL_DestroySurface(EGLSurface egl_surface);
#ifdef SDL_VIDEO_DRIVER_OFFSCREEN
extern EGLSurface SDL_EGL_CreateOffscreenSurface(_THIS, int width, int height);
/* Assumes that LoadLibraryOnly() has succeeded */
extern int SDL_EGL_InitializeOffscreen(_THIS);
#endif
/* These need to be wrapped to get the surface for the window by the platform GLES implementation */
extern SDL_GLContext SDL_EGL_CreateContext(_THIS, EGLSurface egl_surface);
extern int SDL_EGL_MakeCurrent(EGLSurface egl_surface, SDL_GLContext context);
extern int SDL_EGL_SwapBuffers(EGLSurface egl_surface);

/* SDL Error-reporting */
extern int SDL_EGL_SetErrorEx(const char *message, const char *eglFunctionName);
#define SDL_EGL_SetError(message, eglFunctionName) SDL_EGL_SetErrorEx(message, eglFunctionName)

/* A few of useful macros */

#define SDL_EGL_SwapWindow_impl(BACKEND)                                                 \
    int                                                                                  \
        BACKEND##_GLES_SwapWindow(_THIS, SDL_Window *window)                             \
    {                                                                                    \
        return SDL_EGL_SwapBuffers(((SDL_WindowData *)window->driverdata)->egl_surface); \
    }

#define SDL_EGL_MakeCurrent_impl(BACKEND)                                                                                          \
    int                                                                                                                            \
        BACKEND##_GLES_MakeCurrent(SDL_Window *window, SDL_GLContext context)                                               \
    {                                                                                                                              \
        return SDL_EGL_MakeCurrent(window ? ((SDL_WindowData *)window->driverdata)->egl_surface : EGL_NO_SURFACE, context); \
    }

#define SDL_EGL_CreateContext_impl(BACKEND)                                                       \
    SDL_GLContext                                                                                 \
        BACKEND##_GLES_CreateContext(_THIS, SDL_Window *window)                                   \
    {                                                                                             \
        return SDL_EGL_CreateContext(_this, ((SDL_WindowData *)window->driverdata)->egl_surface); \
    }

#endif /* SDL_VIDEO_OPENGL_EGL */

#endif /* SDL_egl_h_ */

/* vi: set ts=4 sw=4 expandtab: */
