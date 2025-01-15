/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#if defined(SDL_VIDEO_DRIVER_X11) && defined(SDL_VIDEO_OPENGL_EGL)

#include "SDL_hints.h"
#include "SDL_x11video.h"
#include "SDL_x11opengles.h"
#include "SDL_x11opengl.h"

/* EGL implementation of SDL OpenGL support */

void X11_GLES_InitDevice(_THIS)
{
    _this->GL_LoadLibrary = X11_GLES_LoadLibrary;
    _this->GL_GetProcAddress = X11_GLES_GetProcAddress;
    _this->GL_UnloadLibrary = X11_GLES_UnloadLibrary;
    _this->GL_CreateContext = X11_GLES_CreateContext;
    _this->GL_MakeCurrent = X11_GLES_MakeCurrent;
    _this->GL_GetDrawableSize = X11_GLES_GetDrawableSize;
    _this->GL_SetSwapInterval = X11_GLES_SetSwapInterval;
    _this->GL_GetSwapInterval = X11_GLES_GetSwapInterval;
    _this->GL_SwapWindow = X11_GLES_SwapWindow;
    _this->GL_DeleteContext = X11_GLES_DeleteContext;
}

int X11_GLES_LoadLibrary(_THIS, const char *path)
{
    /* If the profile requested is not GL ES, switch over to X11_GL functions  */
    if (_this->gl_config.profile_mask != SDL_GL_CONTEXT_PROFILE_ES &&
        !SDL_GetHintBoolean(SDL_HINT_VIDEO_X11_FORCE_EGL, SDL_FALSE)) {
#ifdef SDL_VIDEO_OPENGL_GLX
        /* Switch to GLX based functions */
        X11_GL_InitDevice(_this);
        return X11_GL_PrivateLoadLibrary(_this, path);
#else
        return SDL_SetError("SDL not configured with OpenGL/GLX support");
#endif
    }

    return X11_GLES_PrivateLoadLibrary(_this, path);
}

int X11_GLES_PrivateLoadLibrary(_THIS, const char *path)
{
    X11_VideoData *data = &x11VideoData;
    return SDL_EGL_LoadLibrary(_this, path, (NativeDisplayType) data->display, 0);
}

XVisualInfo *X11_GLES_GetVisual(_THIS, Display *display, int screen)
{

    XVisualInfo *egl_visualinfo = NULL;
    EGLint visual_id;
    XVisualInfo vi_in;
    int out_count;

    SDL_assert(egl_data.eglGetConfigAttrib != NULL);

    if (egl_data.eglGetConfigAttrib(egl_data.egl_display,
                                            egl_data.egl_config,
                                            EGL_NATIVE_VISUAL_ID,
                                            &visual_id) == EGL_FALSE ||
        !visual_id) {
        /* Use the default visual when all else fails */
        vi_in.screen = screen;
        egl_visualinfo = X11_XGetVisualInfo(display,
                                            VisualScreenMask,
                                            &vi_in, &out_count);
    } else {
        vi_in.screen = screen;
        vi_in.visualid = visual_id;
        egl_visualinfo = X11_XGetVisualInfo(display, VisualScreenMask | VisualIDMask, &vi_in, &out_count);
    }

    return egl_visualinfo;
}

SDL_GLContext X11_GLES_CreateContext(_THIS, SDL_Window *window)
{
    SDL_GLContext context;
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    Display *display = x11VideoData.display;

    X11_XSync(display, False);
    context = SDL_EGL_CreateContext(_this, data->egl_surface);
    X11_XSync(display, False);

    return context;
}

SDL_EGL_SwapWindow_impl(X11)
    SDL_EGL_MakeCurrent_impl(X11)

#endif /* SDL_VIDEO_DRIVER_X11 && SDL_VIDEO_OPENGL_EGL */

    /* vi: set ts=4 sw=4 expandtab: */
