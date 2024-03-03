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

#if defined(SDL_VIDEO_DRIVER_PSP) && defined(SDL_VIDEO_OPENGL)

#include <GLES/egl.h>
#include <GLES/gl.h>

#include "SDL_pspvideo.h"
#include "SDL_pspgl_c.h"

/*****************************************************************************/
/* SDL OpenGL/OpenGL ES functions                                            */
/*****************************************************************************/

static int PSP_EGL_SetError(const char *message, const char *eglFunctionName)
{
#ifndef SDL_VERBOSE_ERROR_DISABLED
    EGLint eglErrorCode = eglGetError();
    return SDL_SetError("%s (call to %s failed, reporting an error of 0x%x)", message, eglFunctionName, (unsigned int)eglErrorCode);
#else
    return -1;
#endif
}

static int PSP_EGL_InitializeDisplay(EGLDisplay display)
{
    Psp_VideoData *psp_data = &pspVideoData;

    if (display == EGL_NO_DISPLAY) {
        return SDL_SetError("Could not get EGL display");
    }

    if (eglInitialize(display, NULL, NULL) != EGL_TRUE) {
        return PSP_EGL_SetError("Could not initialize EGL", "eglInitialize");
    }
    psp_data->egl_display = display;
    return 0;
}

int PSP_GL_LoadLibrary(_THIS, const char *path)
{
    EGLDisplay display;

    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    return PSP_EGL_InitializeDisplay(display);
}

/* pspgl doesn't provide this call, so stub it out since SDL requires it.
#define GLSTUB(func,params) void func params {}

GLSTUB(glOrtho,(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
                    GLdouble zNear, GLdouble zFar))
*/
void *PSP_GL_GetProcAddress(_THIS, const char *proc)
{
    return eglGetProcAddress(proc);
}

void PSP_GL_UnloadLibrary(_THIS)
{
    Psp_VideoData *psp_data = &pspVideoData;

    if (psp_data->egl_display) {
        eglTerminate(psp_data->egl_display);
        psp_data->egl_display = EGL_NO_DISPLAY;
    }
}

static EGLint width = 480;
static EGLint height = 272;

SDL_GLContext PSP_GL_CreateContext(_THIS, SDL_Window *window)
{
    Psp_VideoData *psp_data = &pspVideoData;
    SDL_WindowData *windowdata = (SDL_WindowData *)window->driverdata;
    EGLint attribs[16];
    EGLDisplay display = psp_data->egl_display;
    EGLContext context;
    EGLSurface surface;
    EGLConfig config;
    EGLint num_configs;
    int i;

    /* EGL init taken from glutCreateWindow() in PSPGL's glut.c. */
    // EGLCHK(display = eglGetDisplay(0));
    // EGLCHK(eglInitialize(display, NULL, NULL));
    window->flags |= SDL_WINDOW_FULLSCREEN;

    /* Setup the config based on SDL's current values. */
    i = 0;
    attribs[i++] = EGL_RED_SIZE;
    attribs[i++] = _this->gl_config.red_size;
    attribs[i++] = EGL_GREEN_SIZE;
    attribs[i++] = _this->gl_config.green_size;
    attribs[i++] = EGL_BLUE_SIZE;
    attribs[i++] = _this->gl_config.blue_size;
    attribs[i++] = EGL_DEPTH_SIZE;
    attribs[i++] = _this->gl_config.depth_size;

    if (_this->gl_config.alpha_size) {
        attribs[i++] = EGL_ALPHA_SIZE;
        attribs[i++] = _this->gl_config.alpha_size;
    }
    if (_this->gl_config.stencil_size) {
        attribs[i++] = EGL_STENCIL_SIZE;
        attribs[i++] = _this->gl_config.stencil_size;
    }

    SDL_assert(i < SDL_arraysize(attribs));
    attribs[i] = EGL_NONE;

    if (eglChooseConfig(display, attribs, &config, 1, &num_configs) == EGL_FALSE || num_configs == 0) {
        PSP_EGL_SetError("Couldn't find matching EGL config", "eglChooseConfig");
        return EGL_NO_CONTEXT;
    }

    if (eglGetConfigAttrib(display, config, EGL_WIDTH, &width) == EGL_FALSE ||
        eglGetConfigAttrib(display, config, EGL_HEIGHT, &height)) {
        PSP_EGL_SetError("Couldn't retrieve display-dimensions", "eglGetConfigAttrib");
        return EGL_NO_CONTEXT;
    }

    context = eglCreateContext(display, config, NULL, NULL);
    if (context == EGL_NO_CONTEXT) {
        PSP_EGL_SetError("Could not create EGL context", "eglCreateContext");
        return EGL_NO_CONTEXT;
    }
    surface = eglCreateWindowSurface(display, config, 0, NULL);
    if (surface == EGL_NO_SURFACE) {
        PSP_EGL_SetError("unable to create an EGL window surface", "eglCreateWindowSurface");
        eglDestroyContext(display, context);
        return EGL_NO_CONTEXT;
    }
    if (!eglMakeCurrent(display, surface, surface, context)) {
        PSP_EGL_SetError("Unable to make EGL context current", "eglMakeCurrent");
        eglDestroySurface(display, surface);
        eglDestroyContext(display, context);
        return EGL_NO_CONTEXT;
    }

    SDL_assert(windowdata->egl_surface == NULL);
    windowdata->egl_surface = surface;

    return context;
}

int PSP_GL_MakeCurrent(_THIS, SDL_Window *window, SDL_GLContext context)
{
    Psp_VideoData *psp_data = &pspVideoData;
    EGLSurface egl_surface;

    if (window == NULL) {
        egl_surface = EGL_NO_SURFACE;
        SDL_assert(context == EGL_NO_CONTEXT);
    } else {
        SDL_WindowData *windowdata = (SDL_WindowData *)window->driverdata;
        egl_surface = windowdata->egl_surface;
        SDL_assert(context != EGL_NO_CONTEXT);
    }

    if (!eglMakeCurrent(psp_data->egl_display, egl_surface,
                        egl_surface, context)) {
        return PSP_EGL_SetError("Unable to make EGL context current", "eglMakeCurrent");
    }
    return 0;
}

int PSP_GL_SetSwapInterval(_THIS, int interval)
{
    Psp_VideoData *psp_data = &pspVideoData;
    EGLBoolean status;
    status = eglSwapInterval(psp_data->egl_display, interval);
    if (status == EGL_TRUE) {
        /* Return success to upper level */
        psp_data->egl_swapinterval = interval;
        return 0;
    }
    /* Failed to set swap interval */
    return PSP_EGL_SetError("Unable to set the EGL swap interval", "eglSwapInterval");
}

int PSP_GL_GetSwapInterval(_THIS)
{
    Psp_VideoData *psp_data = &pspVideoData;
    return psp_data->egl_swapinterval;
}

int PSP_GL_SwapWindow(_THIS, SDL_Window *window)
{
    Psp_VideoData *psp_data = &pspVideoData;
    SDL_WindowData *windowdata = (SDL_WindowData *)window->driverdata;

    if (!eglSwapBuffers(psp_data->egl_display, windowdata->egl_surface)) {
        return PSP_EGL_SetError("unable to show color buffer in an OS-native window", "eglSwapBuffers");
    }
    return 0;
}

void PSP_GL_DeleteContext(_THIS, SDL_GLContext context)
{
    Psp_VideoData *psp_data = &pspVideoData;
    if (context != EGL_NO_CONTEXT) {
        SDL_assert(psp_data->egl_display != EGL_NO_DISPLAY);
        eglDestroyContext(psp_data->egl_display, context);
    }
}

void PSP_GL_DestroySurface(_THIS, EGLSurface egl_surface)
{
    if (egl_surface != EGL_NO_SURFACE) {
        Psp_VideoData *psp_data = &pspVideoData;
        SDL_assert(psp_data->egl_display != EGL_NO_DISPLAY);
        eglDestroySurface(psp_data->egl_display, egl_surface);
    }
}

#endif /* SDL_VIDEO_DRIVER_PSP */

/* vi: set ts=4 sw=4 expandtab: */
