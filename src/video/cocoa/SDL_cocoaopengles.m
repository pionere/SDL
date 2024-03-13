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

#if defined(SDL_VIDEO_DRIVER_COCOA) && defined(SDL_VIDEO_OPENGL_EGL)

#include "SDL_cocoavideo.h"
#include "SDL_cocoaopengles.h"
#include "SDL_cocoaopengl.h"

/* EGL implementation of SDL OpenGL support */

void Cocoa_GLES_InitDevice(_THIS)
{
    _this->GL_LoadLibrary = Cocoa_GLES_LoadLibrary;
    _this->GL_GetProcAddress = Cocoa_GLES_GetProcAddress;
    _this->GL_UnloadLibrary = Cocoa_GLES_UnloadLibrary;
    _this->GL_CreateContext = Cocoa_GLES_CreateContext;
    _this->GL_MakeCurrent = Cocoa_GLES_MakeCurrent;
    _this->GL_GetDrawableSize = Cocoa_GLES_GetDrawableSize;
    _this->GL_SetSwapInterval = Cocoa_GLES_SetSwapInterval;
    _this->GL_GetSwapInterval = Cocoa_GLES_GetSwapInterval;
    _this->GL_SwapWindow = Cocoa_GLES_SwapWindow;
    _this->GL_DeleteContext = Cocoa_GLES_DeleteContext;
}

int Cocoa_GLES_LoadLibrary(_THIS, const char *path)
{
    /* If the profile requested is not GL ES, switch over to WIN_GL functions  */
    if (_this->gl_config.profile_mask != SDL_GL_CONTEXT_PROFILE_ES) {
#ifdef SDL_VIDEO_OPENGL_CGL
        /* Switch to CGL based functions */
        Cocoa_GL_InitDevice(_this);
        return Cocoa_GL_LoadLibrary(_this, path);
#else
        return SDL_SetError("SDL not configured with OpenGL/CGL support");
#endif
    }

    return SDL_EGL_LoadLibrary(_this, path, EGL_DEFAULT_DISPLAY, 0);
}

SDL_GLContext Cocoa_GLES_CreateContext(_THIS, SDL_Window * window)
{ @autoreleasepool
{
    SDL_GLContext context;
    SDL_WindowData *data = (__bridge SDL_WindowData *)window->driverdata;

    context = SDL_EGL_CreateContext(_this, data.egl_surface);
    return context;
}}

int Cocoa_GLES_SwapWindow(_THIS, SDL_Window * window)
{ @autoreleasepool
{
    return SDL_EGL_SwapBuffers(((__bridge SDL_WindowData *) window->driverdata).egl_surface);
}}

int Cocoa_GLES_MakeCurrent(SDL_Window * window, SDL_GLContext context)
{ @autoreleasepool
{
    return SDL_EGL_MakeCurrent(window ? ((__bridge SDL_WindowData *) window->driverdata).egl_surface : EGL_NO_SURFACE, context);
}}

int Cocoa_GLES_SetupWindow(_THIS, SDL_Window * window)
{
    NSView* v;
    /* The current context is lost in here; save it and reset it. */
    SDL_WindowData *windowdata = (__bridge SDL_WindowData *) window->driverdata;
    SDL_Window *current_win = SDL_GL_GetCurrentWindow();
    SDL_GLContext current_ctx = SDL_GL_GetCurrentContext();

    /* Create the GLES window surface */
    v = windowdata.nswindow.contentView;
    windowdata.egl_surface = SDL_EGL_CreateSurface(_this, (__bridge NativeWindowType)[v layer]);

    if (windowdata.egl_surface == EGL_NO_SURFACE) {
        return -1;
    }

    return Cocoa_GLES_MakeCurrent(current_win, current_ctx);
}

#endif /* SDL_VIDEO_DRIVER_COCOA && SDL_VIDEO_OPENGL_EGL */

/* vi: set ts=4 sw=4 expandtab: */
