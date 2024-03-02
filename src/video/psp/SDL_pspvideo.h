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

#ifndef SDL_pspvideo_h_
#define SDL_pspvideo_h_

#include "../../SDL_internal.h"
#include "../SDL_sysvideo.h"

#ifdef SDL_VIDEO_OPENGL
// #include <GLES2/gl2.h>
//#include "../SDL_egl_c.h"
#include <GLES/egl.h>

typedef struct Psp_VideoData
{
    EGLDisplay egl_display;     /* OpenGL ES display connection           */
    uint32_t egl_swapinterval;  /* OpenGL ES default swap interval        */

} Psp_VideoData;

typedef struct
{
    EGLSurface egl_surface;
} SDL_WindowData;

extern Psp_VideoData pspVideoData; 
#endif

/****************************************************************************/
/* SDL_VideoDevice functions declaration                                    */
/****************************************************************************/

/* Display and window functions */
int PSP_VideoInit(_THIS);
void PSP_VideoQuit(_THIS);
int PSP_SetDisplayMode(SDL_VideoDisplay *display, SDL_DisplayMode *mode);
int PSP_CreateWindow(_THIS, SDL_Window *window);
// int PSP_CreateWindowFrom(_THIS, SDL_Window *window, const void *data);
// void PSP_SetWindowTitle(SDL_Window *window);
// void PSP_SetWindowIcon(SDL_Window *window, SDL_Surface *icon);
// void PSP_SetWindowPosition(SDL_Window *window);
// void PSP_SetWindowSize(SDL_Window *window);
// void PSP_ShowWindow(SDL_Window *window);
// void PSP_HideWindow(SDL_Window *window);
// void PSP_RaiseWindow(_THIS, SDL_Window *window);
// void PSP_MaximizeWindow(SDL_Window *window);
void PSP_MinimizeWindow(SDL_Window *window);
// void PSP_RestoreWindow(SDL_Window *window);
void PSP_DestroyWindow(_THIS, SDL_Window *window);

/* Window manager function */
SDL_bool PSP_GetWindowWMInfo(SDL_Window * window,
                             struct SDL_SysWMinfo *info);

/* OpenGL/OpenGL ES functions */
int PSP_GL_LoadLibrary(_THIS, const char *path);
void *PSP_GL_GetProcAddress(_THIS, const char *proc);
void PSP_GL_UnloadLibrary(_THIS);
SDL_GLContext PSP_GL_CreateContext(_THIS, SDL_Window *window);
int PSP_GL_MakeCurrent(_THIS, SDL_Window *window, SDL_GLContext context);
int PSP_GL_SetSwapInterval(_THIS, int interval);
int PSP_GL_GetSwapInterval(_THIS);
int PSP_GL_SwapWindow(_THIS, SDL_Window *window);
void PSP_GL_DeleteContext(_THIS, SDL_GLContext context);

/* PSP on screen keyboard */
SDL_bool PSP_HasScreenKeyboardSupport();
void PSP_ShowScreenKeyboard(SDL_Window *window);
void PSP_HideScreenKeyboard(SDL_Window *window);
SDL_bool PSP_IsScreenKeyboardShown(SDL_Window *window);

#endif /* SDL_pspvideo_h_ */

/* vi: set ts=4 sw=4 expandtab: */
