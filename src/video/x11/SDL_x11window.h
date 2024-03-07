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

#ifndef SDL_x11window_h_
#define SDL_x11window_h_

/* We need to queue the focus in/out changes because they may occur during
   video mode changes and we can respond to them by triggering more mode
   changes.
*/
#define PENDING_FOCUS_TIME 200

#ifdef SDL_VIDEO_OPENGL_EGL
#include <EGL/egl.h>
#endif

typedef enum
{
    PENDING_FOCUS_NONE,
    PENDING_FOCUS_IN,
    PENDING_FOCUS_OUT
} PendingFocusEnum;

typedef struct
{
    SDL_Window *window;
    Window xwindow;
    Visual *visual;
    Colormap colormap;
#ifndef NO_SHARED_MEMORY
    /* MIT shared memory extension information */
    SDL_bool use_mitshm;
    XShmSegmentInfo shminfo;
#endif
    XImage *ximage;
    GC gc;
    XIC ic;
    int border_left;
    int border_right;
    int border_top;
    int border_bottom;
    SDL_bool mouse_grabbed;
    Uint32 last_focus_event_time;
    PendingFocusEnum pending_focus;
    Uint32 pending_focus_time;
    XConfigureEvent last_xconfigure;
    unsigned long user_time;
    Atom xdnd_req;
    Window xdnd_source;
    SDL_bool flashing_window;
    Uint32 flash_cancel_time;
#ifdef SDL_VIDEO_OPENGL_EGL
    EGLSurface egl_surface;
#endif
#ifdef SDL_VIDEO_DRIVER_X11_XFIXES
    SDL_bool pointer_barrier_active;
    PointerBarrier barrier[4];
    SDL_Rect barrier_rect;
#endif /* SDL_VIDEO_DRIVER_X11_XFIXES */
} SDL_WindowData;

extern Uint32 X11_GetNetWMState(SDL_Window *window, Window xwindow);

extern int X11_CreateSDLWindow(_THIS, SDL_Window *window);
extern int X11_CreateSDLWindowFrom(_THIS, SDL_Window *window, const void *data);
extern char *X11_GetWindowTitle(Window xwindow);
extern void X11_SetWindowTitle(SDL_Window * window);
extern void X11_SetWindowIcon(SDL_Window * window, SDL_Surface * icon);
extern void X11_SetWindowPosition(SDL_Window * window);
extern void X11_SetWindowMinimumSize(SDL_Window * window);
extern void X11_SetWindowMaximumSize(SDL_Window * window);
extern int X11_GetWindowBordersSize(SDL_Window * window, int *top, int *left, int *bottom, int *right);
extern int X11_SetWindowOpacity(SDL_Window * window, float opacity);
extern int X11_SetWindowModalFor(SDL_Window * modal_window, SDL_Window * parent_window);
extern int X11_SetWindowInputFocus(SDL_Window * window);
extern void X11_SetWindowSize(SDL_Window * window);
extern void X11_ShowWindow(SDL_Window * window);
extern void X11_HideWindow(SDL_Window * window);
extern void X11_RaiseWindow(SDL_Window * window);
extern void X11_MaximizeWindow(SDL_Window * window);
extern void X11_MinimizeWindow(SDL_Window * window);
extern void X11_RestoreWindow(SDL_Window * window);
extern void X11_SetWindowBordered(SDL_Window * window, SDL_bool bordered);
extern void X11_SetWindowResizable(SDL_Window * window, SDL_bool resizable);
extern void X11_SetWindowAlwaysOnTop(SDL_Window * window, SDL_bool on_top);
extern void X11_SetWindowFullscreen(SDL_Window * window, SDL_VideoDisplay * display, SDL_bool fullscreen);
extern int X11_SetWindowGammaRamp(SDL_Window * window, const Uint16 * ramp);
extern void* X11_GetWindowICCProfile(SDL_Window * window, size_t * size);
extern void X11_SetWindowMouseGrab(SDL_Window * window, SDL_bool grabbed);
extern void X11_SetWindowKeyboardGrab(SDL_Window * window, SDL_bool grabbed);
extern void X11_DestroyWindow(SDL_Window * window);
extern SDL_bool X11_GetWindowWMInfo(SDL_Window * window,
                                    struct SDL_SysWMinfo *info);
extern int X11_SetWindowHitTest(SDL_Window *window, SDL_bool enabled);
extern void X11_AcceptDragAndDrop(SDL_Window *window, SDL_bool accept);
extern int X11_FlashWindow(SDL_Window *window, SDL_FlashOperation operation);

int SDL_X11_SetWindowTitle(Display *display, Window xwindow, char *title);

#endif /* SDL_x11window_h_ */

/* vi: set ts=4 sw=4 expandtab: */
