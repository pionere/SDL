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

#if defined(SDL_VIDEO_DRIVER_VITA) && defined(SDL_VIDEO_VITA_PIB)

#include "SDL_vitavideo.h"
#include "SDL_vitagles_c.h"

/*****************************************************************************/
/* SDL OpenGL/OpenGL ES functions                                            */
/*****************************************************************************/
void VITA_GLES_KeyboardCallback(ScePigletPreSwapData *data)
{
    SceCommonDialogUpdateParam commonDialogParam;
    SDL_zero(commonDialogParam);
    commonDialogParam.renderTarget.colorFormat = data->colorFormat;
    commonDialogParam.renderTarget.surfaceType = data->surfaceType;
    commonDialogParam.renderTarget.colorSurfaceData = data->colorSurfaceData;
    commonDialogParam.renderTarget.depthSurfaceData = data->depthSurfaceData;
    commonDialogParam.renderTarget.width = data->width;
    commonDialogParam.renderTarget.height = data->height;
    commonDialogParam.renderTarget.strideInPixels = data->strideInPixels;
    commonDialogParam.displaySyncObject = data->displaySyncObject;

    sceCommonDialogUpdate(&commonDialogParam);
}

static int VITA_EGL_SetError(const char *message, const char *eglFunctionName)
{
#ifndef SDL_VERBOSE_ERROR_DISABLED
    EGLint eglErrorCode = eglGetError();
    return SDL_SetError("%s (call to %s failed, reporting an error of 0x%x)", message, eglFunctionName, (unsigned int)eglErrorCode);
#else
    return -1;
#endif
}

static int VITA_EGL_InitializeDisplay(EGLDisplay display)
{
    Vita_VideoData *phdata = &vitaVideoData;

    if (display == EGL_NO_DISPLAY) {
        return SDL_SetError("Could not get EGL display");
    }

    if (eglInitialize(display, NULL, NULL) != EGL_TRUE) {
        return VITA_EGL_SetError("Could not initialize EGL", "eglInitialize");
    }
    phdata->egl_display = display;
    return 0;
}

int VITA_GLES_LoadLibrary(_THIS, const char *path)
{
    EGLDisplay display;

    pibInit(PIB_SHACCCG | PIB_GET_PROC_ADDR_CORE);

    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    return VITA_EGL_InitializeDisplay(display);
}

void *VITA_GLES_GetProcAddress(const char *proc)
{
    return eglGetProcAddress(proc);
}

void VITA_GLES_UnloadLibrary(_THIS)
{
    Vita_VideoData *phdata = &vitaVideoData;

    if (phdata->egl_display) {
        eglTerminate(phdata->egl_display);
        phdata->egl_display = EGL_NO_DISPLAY;
    }
}

static EGLint width = 960;
static EGLint height = 544;

SDL_GLContext VITA_GLES_CreateContext(_THIS, SDL_Window *window)
{
    Vita_VideoData *phdata = &vitaVideoData;
    SDL_WindowData *windowdata = (SDL_WindowData *)window->driverdata;
    EGLint attribs[32];
    EGLDisplay display = phdata->egl_display;
    EGLContext context;
    EGLSurface surface;
    EGLConfig config;
    EGLint num_configs;
    PFNEGLPIGLETVITASETPRESWAPCALLBACKSCEPROC preSwapCallback;
    int i;

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    // EGLCHK(display = eglGetDisplay(0));
    // EGLCHK(eglInitialize(display, NULL, NULL));
    window->flags |= SDL_WINDOW_FULLSCREEN;

    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        // VITA_EGL_SetError("Couldn't bin to GLES-api", "eglBindAPI");
        // return EGL_NO_CONTEXT;
    }

    i = 0;
    attribs[i++] = EGL_RED_SIZE;
    attribs[i++] = 8;
    attribs[i++] = EGL_GREEN_SIZE;
    attribs[i++] = 8;
    attribs[i++] = EGL_BLUE_SIZE;
    attribs[i++] = 8;
    attribs[i++] = EGL_DEPTH_SIZE;
    attribs[i++] = 0;
    attribs[i++] = EGL_ALPHA_SIZE;
    attribs[i++] = 8;
    attribs[i++] = EGL_STENCIL_SIZE;
    attribs[i++] = 0;

    attribs[i++] = EGL_SURFACE_TYPE;
    attribs[i++] = 5;

    attribs[i++] = EGL_RENDERABLE_TYPE;
    attribs[i++] = EGL_OPENGL_ES2_BIT;

    attribs[i++] = EGL_CONFORMANT;
    attribs[i++] = EGL_OPENGL_ES2_BIT;

    attribs[i++] = EGL_NONE;

    if (eglChooseConfig(display, attribs, &config, 1, &num_configs) == EGL_FALSE || num_configs == 0) {
        VITA_EGL_SetError("Couldn't find matching EGL config", "eglChooseConfig");
        return EGL_NO_CONTEXT;
    }

    surface = eglCreateWindowSurface(display, config, VITA_WINDOW_960X544, NULL);
    if (surface == EGL_NO_SURFACE) {
        VITA_EGL_SetError("unable to create an EGL window surface", "eglCreateWindowSurface");
        return EGL_NO_CONTEXT;
    }
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT) {
        VITA_EGL_SetError("Could not create EGL context", "eglCreateContext");
        eglDestroySurface(display, surface);
        return EGL_NO_CONTEXT;
    }
    if (!eglMakeCurrent(display, surface, surface, context)) {
        VITA_EGL_SetError("Unable to make EGL context current", "eglMakeCurrent");
        eglDestroySurface(display, surface);
        eglDestroyContext(display, context);
        return EGL_NO_CONTEXT;
    }
    if (!eglQuerySurface(display, surface, EGL_WIDTH, &width) ||
        !eglQuerySurface(display, surface, EGL_HEIGHT, &height)) {
        VITA_EGL_SetError("Couldn't retrieve display-dimensions", "eglQuerySurface");
        eglDestroySurface(display, surface);
        eglDestroyContext(display, context);
        return EGL_NO_CONTEXT;
    }

    preSwapCallback = (PFNEGLPIGLETVITASETPRESWAPCALLBACKSCEPROC)eglGetProcAddress("eglPigletVitaSetPreSwapCallbackSCE");
    preSwapCallback(VITA_GLES_KeyboardCallback);

    SDL_assert(windowdata->egl_surface == NULL);
    windowdata->egl_surface = surface;

    return context;
}

int VITA_GLES_MakeCurrent(SDL_Window *window, SDL_GLContext context)
{
    Vita_VideoData *phdata = &vitaVideoData;
    EGLSurface egl_surface;

    if (!window) {
        egl_surface = EGL_NO_SURFACE;
    } else {
        SDL_WindowData *windowdata = (SDL_WindowData *)window->driverdata;
        egl_surface = windowdata->egl_surface;
        SDL_assert(context != EGL_NO_CONTEXT);
        SDL_assert(egl_surface != EGL_NO_SURFACE);
    }

    if (!eglMakeCurrent(phdata->egl_display, egl_surface,
                        egl_surface, context)) {
        return VITA_EGL_SetError("Unable to make EGL context current", "eglMakeCurrent");
    }
    return 0;
}

int VITA_GLES_SetSwapInterval(int interval)
{
    Vita_VideoData *phdata = &vitaVideoData;
    EGLBoolean status;
    status = eglSwapInterval(phdata->egl_display, interval);
    if (status == EGL_TRUE) {
        /* Return success to upper level */
        phdata->egl_swapinterval = interval;
        return 0;
    }
    /* Failed to set swap interval */
    return VITA_EGL_SetError("Unable to set the EGL swap interval", "eglSwapInterval");
}

int VITA_GLES_GetSwapInterval()
{
    Vita_VideoData *phdata = &vitaVideoData;
    return phdata->egl_swapinterval;
}

int VITA_GLES_SwapWindow(_THIS, SDL_Window *window)
{
    Vita_VideoData *phdata = &vitaVideoData;
    SDL_WindowData *windowdata = (SDL_WindowData *)window->driverdata;

    if (!eglSwapBuffers(phdata->egl_display, windowdata->egl_surface)) {
        return VITA_EGL_SetError("unable to show color buffer in an OS-native window", "eglSwapBuffers");
    }
    return 0;
}

void VITA_GLES_DeleteContext(SDL_GLContext context)
{
    Vita_VideoData *phdata = &vitaVideoData;
    if (context != EGL_NO_CONTEXT) {
        SDL_assert(phdata->egl_display != EGL_NO_DISPLAY);
        eglDestroyContext(phdata->egl_display, context);
    }
}

void VITA_GLES_DestroySurface(EGLSurface egl_surface)
{
    if (egl_surface != EGL_NO_SURFACE) {
        Vita_VideoData *phdata = &vitaVideoData;
        SDL_assert(phdata->egl_display != EGL_NO_DISPLAY);
        eglDestroySurface(phdata->egl_display, egl_surface);
    }
}

#endif /* SDL_VIDEO_DRIVER_VITA */

/* vi: set ts=4 sw=4 expandtab: */
