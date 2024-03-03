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

#if defined(SDL_VIDEO_DRIVER_DIRECTFB) && defined(SDL_DIRECTFB_OPENGL)

#include "SDL_DirectFB_video.h"

#include "SDL_DirectFB_opengl.h"
#include "SDL_DirectFB_window.h"

#include <directfbgl.h>
#include "SDL_loadso.h"

#define OPENGL_REQUIRS_DLOPEN
#if defined(OPENGL_REQUIRS_DLOPEN) && defined(HAVE_DLOPEN)
#include <dlfcn.h>
#define GL_LoadObject(X)    dlopen(X, (RTLD_NOW|RTLD_GLOBAL))
#define GL_LoadFunction     dlsym
#define GL_UnloadObject     dlclose
#else
#define GL_LoadObject   SDL_LoadObject
#define GL_LoadFunction SDL_LoadFunction
#define GL_UnloadObject SDL_UnloadObject
#endif

int DirectFB_GL_LoadLibrary(_THIS, const char *path)
{
    void *handle;
    SDL_GLDriverData *fbgl_data = &dfbVideoData.fbgl_data;

    SDL_DFB_DEBUG("Loadlibrary : %s\n", path);

    if (!path) {
        path = SDL_getenv("SDL_OPENGL_LIBRARY");
        if (!path) {
            path = "libGL.so.1";
        }
    }

    handle = GL_LoadObject(path);
    if (!handle) {
        /* SDL_LoadObject() will call SDL_SetError() for us. */
        return -1;
    }

#if 0
    fbgl_data->glFinish = DirectFB_GL_GetProcAddress(_this, "glFinish");
    fbgl_data->glFlush = DirectFB_GL_GetProcAddress(_this, "glFlush");
#endif

    SDL_DFB_DEBUG("Loaded library: %s\n", path);

    fbgl_data->dll_handle = handle;

    /* Initialize extensions */
    /* FIXME needed?
     * X11_GL_InitExtensions(_this);
     */

    return 0;
}

void DirectFB_GL_UnloadLibrary(_THIS)
{
    SDL_GLDriverData *fbgl_data = &dfbVideoData.fbgl_data;

    GL_UnloadObject(fbgl_data->dll_handle);
    // fbgl_data->dll_handle = NULL;

    SDL_zero(*fbgl_data);
}

void *DirectFB_GL_GetProcAddress(_THIS, const char *proc)
{
    SDL_GLDriverData *fbgl_data = &dfbVideoData.fbgl_data;

    return GL_LoadFunction(fbgl_data->dll_handle, proc);
}

SDL_GLContext DirectFB_GL_CreateContext(_THIS, SDL_Window * window)
{
    SDL_GLDriverData *fbgl_data = &dfbVideoData.fbgl_data;
    DFB_WindowData *windata = (DFB_WindowData *)window->driverdata;
    DirectFB_GLContext *context;

    SDL_DFB_ALLOC_CLEAR(context, sizeof(DirectFB_GLContext));

    SDL_DFB_CHECKERR(windata->surface->GetGL(windata->surface,
                                             &context->context));

    if (!context->context) {
        SDL_DFB_FREE(context);
        return NULL;
    }

    // context->is_locked = 0;
    context->sdl_window = window;

    context->next = fbgl_data->firstgl;
    fbgl_data->firstgl = context;

    SDL_DFB_CHECK(context->context->Unlock(context->context));

    if (DirectFB_GL_MakeCurrent(_this, window, context) < 0) {
        DirectFB_GL_DeleteContext(_this, context);
        return NULL;
    }

    return context;

  error:
    return NULL;
}

int DirectFB_GL_MakeCurrent(_THIS, SDL_Window * window, SDL_GLContext context)
{
    SDL_GLDriverData *fbgl_data = &dfbVideoData.fbgl_data;
    DirectFB_GLContext *ctx = (DirectFB_GLContext *) context;
    DirectFB_GLContext *p;

    for (p = fbgl_data->firstgl; p; p = p->next)
    {
       if (p->is_locked) {
         SDL_DFB_CHECKERR(p->context->Unlock(p->context));
         p->is_locked = 0;
       }

    }

    if (ctx) {
        SDL_DFB_CHECKERR(ctx->context->Lock(ctx->context));
        ctx->is_locked = 1;
    }

    return 0;
  error:
    return -1;
}

int DirectFB_GL_SetSwapInterval(_THIS, int interval)
{
    return SDL_Unsupported();
}

int DirectFB_GL_GetSwapInterval(_THIS)
{
    return 0;
}

int DirectFB_GL_SwapWindow(_THIS, SDL_Window * window)
{
    SDL_GLDriverData *fbgl_data = &dfbVideoData.fbgl_data;
    DFB_WindowData *windata = (DFB_WindowData *)window->driverdata;
    DirectFB_GLContext *p;

#if 0
    if (fbgl_data->glFinish)
        fbgl_data->glFinish();
    else if (fbgl_data->glFlush)
        fbgl_data->glFlush();
#endif

    for (p = fbgl_data->firstgl; p; p = p->next)
        if (p->sdl_window == window && p->is_locked)
        {
            SDL_DFB_CHECKERR(p->context->Unlock(p->context));
            p->is_locked = 0;
        }

    SDL_DFB_CHECKERR(windata->window_surface->Flip(windata->window_surface,NULL,  DSFLIP_PIPELINE |DSFLIP_BLIT | DSFLIP_ONSYNC ));
    return 0;
  error:
    return -1;
}

void DirectFB_GL_DeleteContext(_THIS, SDL_GLContext context)
{
    SDL_GLDriverData *fbgl_data = &dfbVideoData.fbgl_data;
    DirectFB_GLContext *ctx = (DirectFB_GLContext *) context;
    DirectFB_GLContext *p;

    if (ctx->is_locked)
        SDL_DFB_CHECK(ctx->context->Unlock(ctx->context));
    SDL_DFB_RELEASE(ctx->context);

    for (p = fbgl_data->firstgl; p && p->next != ctx; p = p->next)
        ;
    if (p)
        p->next = ctx->next;
    else
        fbgl_data->firstgl = ctx->next;

    SDL_DFB_FREE(ctx);
}

void DirectFB_GL_FreeWindowContexts(SDL_Window * window)
{
    SDL_GLDriverData *fbgl_data = &dfbVideoData.fbgl_data;
    DirectFB_GLContext *p;

    for (p = fbgl_data->firstgl; p; p = p->next)
        if (p->sdl_window == window)
        {
            if (p->is_locked)
                SDL_DFB_CHECK(p->context->Unlock(p->context));
            SDL_DFB_RELEASE(p->context);
        }
}

void DirectFB_GL_ReAllocWindowContexts(SDL_Window * window)
{
    SDL_GLDriverData *fbgl_data = &dfbVideoData.fbgl_data;
    DirectFB_GLContext *p;

    for (p = fbgl_data->firstgl; p; p = p->next)
        if (p->sdl_window == window)
        {
            DFB_WindowData *windata = (DFB_WindowData *)window->driverdata;
            SDL_DFB_CHECK(windata->surface->GetGL(windata->surface,
                                             &p->context));
            if (p->is_locked)
                SDL_DFB_CHECK(p->context->Lock(p->context));
        }
}

void DirectFB_GL_DestroyWindowContexts(SDL_Window * window)
{
    SDL_GLDriverData *fbgl_data = &dfbVideoData.fbgl_data;
    DirectFB_GLContext *p;

    for (p = fbgl_data->firstgl; p; p = p->next)
        if (p->sdl_window == window)
            DirectFB_GL_DeleteContext(_this, p);
}

#endif /* SDL_VIDEO_DRIVER_DIRECTFB */

/* vi: set ts=4 sw=4 expandtab: */
