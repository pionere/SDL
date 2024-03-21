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

#ifdef SDL_VIDEO_DRIVER_DIRECTFB

#include "SDL_DirectFB_video.h"
#include "SDL_DirectFB_modes.h"
#include "SDL_DirectFB_window.h"
#include "SDL_DirectFB_shape.h"

#ifdef SDL_DIRECTFB_OPENGL
#include "SDL_DirectFB_opengl.h"
#endif

#include "SDL_syswm.h"

int DirectFB_CreateSDLWindow(_THIS, SDL_Window * window)
{
    DFB_VideoData *devdata = &dfbVideoData;
    DFB_DisplayData *dispdata = (DFB_DisplayData *) SDL_GetDisplayForWindow(window)->driverdata;
    SDL_DFB_DISPLAYDATA(window);
    DFB_WindowData *windata = NULL;
    DFBWindowOptions wopts;
    DFBWindowDescription desc;
    int x, y;
    int bshaped = 0;

    windata = SDL_calloc(1, sizeof(DFB_WindowData));
    if (windata == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_memset(&desc, 0, sizeof(DFBWindowDescription));
    window->driverdata = (DFB_WindowData *)windata;

    windata->is_managed = devdata->has_own_wm;
#if 1
    SDL_DFB_CHECKERR(devdata->dfb->SetCooperativeLevel(devdata->dfb,
                                                       DFSCL_NORMAL));
    SDL_DFB_CHECKERR(dispdata->layer->SetCooperativeLevel(dispdata->layer,
                                                          DLSCL_ADMINISTRATIVE));
#endif
    /* FIXME ... ughh, ugly */
    if (window->x == -1000 && window->y == -1000)
        bshaped = 1;

    /* Fill the window description. */
    x = window->x;
    y = window->y;

    DirectFB_WM_AdjustWindowLayout(window, window->flags, window->w, window->h);

    /* Create Window */
    desc.caps = 0;
    desc.flags =
        DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_POSX | DWDESC_POSY | DWDESC_SURFACE_CAPS;

    if (bshaped) {
        desc.flags |= DWDESC_CAPS;
        desc.caps |= DWCAPS_ALPHACHANNEL;
    }
    else
    {
        desc.flags |= DWDESC_PIXELFORMAT;
    }

    if (!(window->flags & SDL_WINDOW_BORDERLESS))
        desc.caps |= DWCAPS_NODECORATION;

    desc.posx = x;
    desc.posy = y;
    desc.width = windata->size.w;
    desc.height = windata->size.h;
    desc.pixelformat = dispdata->pixelformat;
    desc.surface_caps = DSCAPS_PREMULTIPLIED;
#if DIRECTFB_MAJOR_VERSION == 1 && DIRECTFB_MINOR_VERSION >= 6
    if (window->flags & SDL_WINDOW_OPENGL) {
        desc.surface_caps |= DSCAPS_GL;
    }
#endif

    /* Create the window. */
    SDL_DFB_CHECKERR(dispdata->layer->CreateWindow(dispdata->layer, &desc,
                                                   &windata->dfbwin));

    /* Set Options */
    SDL_DFB_CHECK(windata->dfbwin->GetOptions(windata->dfbwin, &wopts));

    /* explicit rescaling of surface */
    wopts |= DWOP_SCALE;
    if (window->flags & SDL_WINDOW_RESIZABLE) {
        wopts &= ~DWOP_KEEP_SIZE;
    }
    else {
        wopts |= DWOP_KEEP_SIZE;
    }

    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        wopts |= DWOP_KEEP_POSITION | DWOP_KEEP_STACKING | DWOP_KEEP_SIZE;
        SDL_DFB_CHECK(windata->dfbwin->SetStackingClass(windata->dfbwin, DWSC_UPPER));
    }

    if (bshaped) {
        wopts |= DWOP_SHAPED | DWOP_ALPHACHANNEL;
        wopts &= ~DWOP_OPAQUE_REGION;
    }

    SDL_DFB_CHECK(windata->dfbwin->SetOptions(windata->dfbwin, wopts));

    /* See what we got */
    SDL_DFB_CHECK(DirectFB_WM_GetClientSize
                     (window, &window->w, &window->h));

    /* Get the window's surface. */
    SDL_DFB_CHECKERR(windata->dfbwin->GetSurface(windata->dfbwin,
                                                 &windata->window_surface));

    /* And get a subsurface for rendering */
    SDL_DFB_CHECKERR(windata->window_surface->
                     GetSubSurface(windata->window_surface, &windata->client,
                                   &windata->surface));

    SDL_DFB_CHECK(windata->dfbwin->SetOpacity(windata->dfbwin, 0xFF));

    /* Create Eventbuffer */

    SDL_DFB_CHECKERR(windata->dfbwin->CreateEventBuffer(windata->dfbwin,
                                                        &windata->
                                                        eventbuffer));
    SDL_DFB_CHECKERR(windata->dfbwin->
                     EnableEvents(windata->dfbwin, DWET_ALL));

    /* Create a font */
    /* FIXME: once during Video_Init */
    windata->font = NULL;

    /* Make it the top most window. */
    SDL_DFB_CHECK(windata->dfbwin->RaiseToTop(windata->dfbwin));

    /* remember parent */
    /* windata->sdlwin = window; */

    /* Add to list ... */

    windata->next = devdata->firstwin;
    windata->opacity = 0xFF;
    devdata->firstwin = window;

    /* Draw Frame */
    DirectFB_WM_RedrawLayout(window);

    return 0;
  error:
    return -1;
}

/*int DirectFB_CreateSDLWindowFrom(_THIS, SDL_Window * window, const void *data)
{
    return SDL_Unsupported();
}*/

void DirectFB_SetWindowTitle(SDL_Window * window)
{
    DFB_WindowData *windata = (DFB_WindowData *)window->driverdata;

    if (windata->is_managed) {
        windata->wm_needs_redraw = 1;
        DirectFB_WM_RedrawLayout(window);
    } else {
        SDL_Unsupported();
    }
}

void DirectFB_SetWindowIcon(SDL_Window * window, SDL_Surface * icon)
{
    DFB_VideoData *devdata = &dfbVideoData;
    DFB_WindowData *windata = (DFB_WindowData *)window->driverdata;
    SDL_Surface *surface = NULL;

    if (icon) {
        DFBSurfaceDescription dsc;
        Uint32 *dest;
        Uint32 *p;
        int pitch, i;

        /* Convert the icon to ARGB for modern window managers */
        surface = SDL_ConvertSurfaceFormat(icon, SDL_PIXELFORMAT_ARGB8888, 0);
        if (!surface) {
            return;
        }
        dsc.flags =
            DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT | DSDESC_CAPS;
        dsc.caps = DSCAPS_VIDEOONLY;
        dsc.width = surface->w;
        dsc.height = surface->h;
        dsc.pixelformat = DSPF_ARGB;

        SDL_DFB_CHECKERR(devdata->dfb->CreateSurface(devdata->dfb, &dsc,
                                                     &windata->icon));

        SDL_DFB_CHECKERR(windata->icon->Lock(windata->icon, DSLF_WRITE,
                                             (void *) &dest, &pitch));

        p = surface->pixels;
        for (i = 0; i < surface->h; i++)
            SDL_memcpy((char *) dest + i * pitch,
                   (char *) p + i * surface->pitch, 4 * surface->w);

        SDL_DFB_CHECK(windata->icon->Unlock(windata->icon));
        SDL_FreeSurface(surface);
    } else {
        SDL_DFB_RELEASE(windata->icon);
    }
    return;
  error:
    SDL_FreeSurface(surface);
    SDL_DFB_RELEASE(windata->icon);
    return;
}

void DirectFB_SetWindowPosition(SDL_Window * window)
{
    DFB_WindowData *windata = (DFB_WindowData *)window->driverdata;
    IDirectFBWindow *dfbwin = windata->dfbwin;
    int x, y;

    x = window->x;
    y = window->y;

    DirectFB_WM_AdjustWindowLayout(window, window->flags, window->w, window->h);
    SDL_DFB_CHECK(dfbwin->MoveTo(dfbwin, x, y));
}

void DirectFB_SetWindowSize(SDL_Window * window)
{
    DFB_WindowData *windata = (DFB_WindowData *)window->driverdata;
    IDirectFBWindow *dfbwin;

    if (SDL_IsShapedWindow(window))
        DirectFB_ResizeWindowShape(window);

    dfbwin = windata->dfbwin;
    if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
        int cw;
        int ch;

        /* Make sure all events are disabled for this operation ! */
        SDL_DFB_CHECKERR(dfbwin->DisableEvents(dfbwin,
                                                        DWET_ALL));
        SDL_DFB_CHECKERR(DirectFB_WM_GetClientSize(window, &cw, &ch));

        if (cw != window->w || ch != window->h) {

            DirectFB_WM_AdjustWindowLayout(window, window->flags, window->w, window->h);
            SDL_DFB_CHECKERR(dfbwin->Resize(dfbwin,
                                                     windata->size.w,
                                                     windata->size.h));
        }

        SDL_DFB_CHECKERR(DirectFB_WM_GetClientSize
                     (window, &window->w, &window->h));
        DirectFB_AdjustWindowSurface(window);

        SDL_DFB_CHECKERR(dfbwin->EnableEvents(dfbwin,
                                                       DWET_ALL));

    }
    return;
  error:
    SDL_DFB_CHECK(dfbwin->EnableEvents(dfbwin, DWET_ALL));
    return;
}

void DirectFB_ShowWindow(SDL_Window * window)
{
    DFB_WindowData *windata = (DFB_WindowData *)window->driverdata;
    IDirectFBWindow *dfbwin = windata->dfbwin;

    SDL_DFB_CHECK(dfbwin->SetOpacity(dfbwin, windata->opacity));

}

void DirectFB_HideWindow(SDL_Window * window)
{
    DFB_WindowData *windata = (DFB_WindowData *)window->driverdata;
    IDirectFBWindow *dfbwin = windata->dfbwin;

    SDL_DFB_CHECK(dfbwin->GetOpacity(dfbwin, &windata->opacity));
    SDL_DFB_CHECK(dfbwin->SetOpacity(dfbwin, 0));
}

void DirectFB_RaiseWindow(SDL_Window * window)
{
    DFB_WindowData *windata = (DFB_WindowData *)window->driverdata;
    IDirectFBWindow *dfbwin = windata->dfbwin;

    SDL_DFB_CHECK(dfbwin->RaiseToTop(dfbwin));
    SDL_DFB_CHECK(dfbwin->RequestFocus(dfbwin));
}

void DirectFB_MaximizeWindow(SDL_Window * window)
{
    SDL_VideoDisplay *display = SDL_GetDisplayForWindow(window);
    DFB_WindowData *windata = (DFB_WindowData *)window->driverdata;
    IDirectFBWindow *dfbwin = windata->dfbwin;
    DFBWindowOptions wopts;

    SDL_DFB_CHECK(dfbwin->GetPosition(dfbwin,
                                 &windata->restore.x, &windata->restore.y));
    SDL_DFB_CHECK(dfbwin->GetSize(dfbwin, &windata->restore.w,
                             &windata->restore.h));

    DirectFB_WM_AdjustWindowLayout(window, window->flags | SDL_WINDOW_MAXIMIZED, display->current_mode.w, display->current_mode.h) ;

    SDL_DFB_CHECK(dfbwin->MoveTo(dfbwin, 0, 0));
    SDL_DFB_CHECK(dfbwin->Resize(dfbwin,
                            display->current_mode.w, display->current_mode.h));

    /* Set Options */
    SDL_DFB_CHECK(dfbwin->GetOptions(dfbwin, &wopts));
    wopts |= DWOP_KEEP_SIZE | DWOP_KEEP_POSITION;
    SDL_DFB_CHECK(dfbwin->SetOptions(dfbwin, wopts));
}

void DirectFB_MinimizeWindow(SDL_Window * window)
{
    /* FIXME: Size to 32x32 ? */

    SDL_Unsupported();
}

void DirectFB_RestoreWindow(SDL_Window * window)
{
    DFB_WindowData *windata = (DFB_WindowData *)window->driverdata;
    IDirectFBWindow *dfbwin = windata->dfbwin;
    DFBWindowOptions wopts;

    /* Set Options */
    SDL_DFB_CHECK(dfbwin->GetOptions(dfbwin, &wopts));
    wopts &= ~(DWOP_KEEP_SIZE | DWOP_KEEP_POSITION);
    SDL_DFB_CHECK(dfbwin->SetOptions(dfbwin, wopts));

    /* Window layout */
    DirectFB_WM_AdjustWindowLayout(window, window->flags & ~(SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED),
        windata->restore.w, windata->restore.h);
    SDL_DFB_CHECK(dfbwin->Resize(dfbwin, windata->restore.w,
                            windata->restore.h));
    SDL_DFB_CHECK(dfbwin->MoveTo(dfbwin, windata->restore.x,
                            windata->restore.y));

    if (!(window->flags & SDL_WINDOW_RESIZABLE))
        wopts |= DWOP_KEEP_SIZE;

    if (window->flags & SDL_WINDOW_FULLSCREEN)
        wopts |= DWOP_KEEP_POSITION | DWOP_KEEP_SIZE;
    SDL_DFB_CHECK(dfbwin->SetOptions(dfbwin, wopts));


}

void DirectFB_SetWindowMouseGrab(SDL_Window * window, SDL_bool grabbed)
{
    DFB_WindowData *windata = (DFB_WindowData *)window->driverdata;
    IDirectFBWindow *dfbwin = windata->dfbwin;

    if (grabbed) {
        SDL_DFB_CHECK(dfbwin->GrabPointer(dfbwin));
    } else {
        SDL_DFB_CHECK(dfbwin->UngrabPointer(dfbwin));
    }
}

void DirectFB_SetWindowKeyboardGrab(SDL_Window * window, SDL_bool grabbed)
{
    DFB_WindowData *windata = (DFB_WindowData *)window->driverdata;
    IDirectFBWindow *dfbwin = windata->dfbwin;

    if (grabbed) {
        SDL_DFB_CHECK(dfbwin->GrabKeyboard(dfbwin));
    } else {
        SDL_DFB_CHECK(dfbwin->UngrabKeyboard(dfbwin));
    }
}

void DirectFB_DestroyWindow(SDL_Window * window)
{
    DFB_VideoData *devdata = &dfbVideoData;
    DFB_WindowData *windata = (DFB_WindowData *)window->driverdata;
    IDirectFBWindow *dfbwin = windata->dfbwin;
    DFB_WindowData *p;
    if (windata) {
    /* Some cleanups */
    SDL_DFB_CHECK(dfbwin->UngrabPointer(dfbwin));
    SDL_DFB_CHECK(dfbwin->UngrabKeyboard(dfbwin));

#ifdef SDL_DIRECTFB_OPENGL
    DirectFB_GL_DestroyWindowContexts(window);
#endif

    if (window->shaper) {
        SDL_ShapeData *data = window->shaper->driverdata;
        SDL_DFB_RELEASE(data->surface);
        SDL_DFB_FREE(data);
        SDL_DFB_FREE(window->shaper);
    }

    SDL_DFB_CHECK(windata->window_surface->SetFont(windata->window_surface, NULL));
    SDL_DFB_CHECK(windata->surface->ReleaseSource(windata->surface));
    SDL_DFB_CHECK(windata->window_surface->ReleaseSource(windata->window_surface));
    SDL_DFB_RELEASE(windata->icon);
    SDL_DFB_RELEASE(windata->font);
    SDL_DFB_RELEASE(windata->eventbuffer);
    SDL_DFB_RELEASE(windata->surface);
    SDL_DFB_RELEASE(windata->window_surface);

    SDL_DFB_RELEASE(windata->dfbwin);

    /* Remove from list ... */

    p = devdata->firstwin->driverdata;

    while (p && p->next != window)
        p = (p->next ? p->next->driverdata : NULL);
    if (p)
        p->next = windata->next;
    else
        devdata->firstwin = windata->next;
    SDL_free(windata);
    window->driverdata = NULL;
    }
}

SDL_bool DirectFB_GetWindowWMInfo(SDL_Window * window,
                         struct SDL_SysWMinfo * info)
{
    const Uint32 version = ((((Uint32) info->version.major) * 1000000) +
                            (((Uint32) info->version.minor) * 10000) +
                            (((Uint32) info->version.patch)));

    DFB_VideoData *devdata = &dfbVideoData;
    DFB_WindowData *windata = (DFB_WindowData *)window->driverdata;

    /* Before 2.0.6, it was possible to build an SDL with DirectFB support
       (SDL_SysWMinfo will be large enough to hold DirectFB info), but build
       your app against SDL headers that didn't have DirectFB support
       (SDL_SysWMinfo could be smaller than DirectFB needs. This would lead
       to an app properly using SDL_GetWindowWMInfo() but we'd accidentally
       overflow memory on the stack or heap. To protect against this, we've
       padded out the struct unconditionally in the headers and DirectFB will
       just return an error for older apps using this function. Those apps
       will need to be recompiled against newer headers or not use DirectFB,
       maybe by forcing SDL_VIDEODRIVER=x11. */
    if (version < 2000006) {
        info->subsystem = SDL_SYSWM_UNKNOWN;
        SDL_SetError("Version must be 2.0.6 or newer");
        return SDL_FALSE;
    }

    if (info->version.major == SDL_MAJOR_VERSION) {
        info->subsystem = SDL_SYSWM_DIRECTFB;
        info->info.dfb.dfb = devdata->dfb;
        info->info.dfb.window = windata->dfbwin;
        info->info.dfb.surface = windata->surface;
        return SDL_TRUE;
    } else {
        SDL_SetError("Application not compiled with SDL %d",
                     SDL_MAJOR_VERSION);
        return SDL_FALSE;
    }
}

IDirectFBSurface *DirectFB_GetFBSurface(SDL_Window * window)
{
    DFB_WindowData *windata = (DFB_WindowData *)window->driverdata;
    return windata->surface;
}

IDirectFBWindow *DirectFB_GetFBWindow(SDL_Window * window)
{
    DFB_WindowData *windata = (DFB_WindowData *)window->driverdata;
    return windata->dfbwin;
}

void DirectFB_AdjustWindowSurface(SDL_Window * window)
{
    DFB_WindowData *windata = (DFB_WindowData *)window->driverdata;
    int adjust = windata->wm_needs_redraw;
    int cw, ch;

    DirectFB_WM_AdjustWindowLayout(window, window->flags, window->w, window->h);

    SDL_DFB_CHECKERR(windata->
                     window_surface->GetSize(windata->window_surface, &cw,
                                             &ch));
    if (cw != windata->size.w || ch != windata->size.h) {
        adjust = 1;
    }

    if (adjust) {
#ifdef SDL_DIRECTFB_OPENGL
        DirectFB_GL_FreeWindowContexts(window);
#endif

#if (DFB_VERSION_ATLEAST(1,2,1))
        SDL_DFB_CHECKERR(windata->dfbwin->ResizeSurface(windata->dfbwin,
                                                        windata->size.w,
                                                        windata->size.h));
        SDL_DFB_CHECKERR(windata->surface->MakeSubSurface(windata->surface,
                                                          windata->
                                                          window_surface,
                                                          &windata->client));
#else
        DFBWindowOptions opts;

        SDL_DFB_CHECKERR(windata->dfbwin->GetOptions(windata->dfbwin, &opts));
        /* recreate subsurface */
        SDL_DFB_RELEASE(windata->surface);

        if (opts & DWOP_SCALE)
            SDL_DFB_CHECKERR(windata->dfbwin->ResizeSurface(windata->dfbwin,
                                                            windata->size.w,
                                                            windata->size.h));
        SDL_DFB_CHECKERR(windata->window_surface->
                         GetSubSurface(windata->window_surface,
                                       &windata->client, &windata->surface));
#endif
        DirectFB_WM_RedrawLayout(window);

#ifdef SDL_DIRECTFB_OPENGL
        DirectFB_GL_ReAllocWindowContexts(window);
#endif
   }
  error:
    return;
}

int DirectFB_SetWindowOpacity(SDL_Window * window, float opacity)
{
    const Uint8 alpha = (Uint8) ((unsigned int) (opacity * 255.0f));
    DFB_WindowData *windata = (DFB_WindowData *)window->driverdata;
    SDL_DFB_CHECKERR(windata->dfbwin->SetOpacity(windata->dfbwin, alpha));
    windata->opacity = alpha;
    return 0;

error:
    return -1;
}

#endif /* SDL_VIDEO_DRIVER_DIRECTFB */
