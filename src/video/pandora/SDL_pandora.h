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

#ifndef __SDL_PANDORA_H__
#define __SDL_PANDORA_H__

#include "../../SDL_internal.h"
#include "../SDL_sysvideo.h"

#ifdef SDL_VIDEO_OPENGL_EGL
#include <GLES/egl.h>

typedef struct Pandora_VideoData
{
    void *opengl_dll_handle;
    EGLDisplay egl_display;     /* OpenGL ES display connection           */
    uint32_t swapinterval;      /* OpenGL ES default swap interval        */
    EGLConfig gles_config;

} Pandora_VideoData;

typedef struct SDL_WindowData
{
    EGLSurface gles_surface;    /* OpenGL ES target rendering surface */

} SDL_WindowData;
#endif

/****************************************************************************/
/* SDL_VideoDevice functions declaration                                    */
/****************************************************************************/

/* Display and window functions */
int PND_VideoInit(_THIS);
void PND_VideoQuit(_THIS);
int PND_SetDisplayMode(SDL_VideoDisplay * display, SDL_DisplayMode * mode);
int PND_CreateSDLWindow(_THIS, SDL_Window * window);
// int PND_CreateSDLWindowFrom(_THIS, SDL_Window * window, const void *data);
// void PND_SetWindowTitle(SDL_Window * window);
// void PND_SetWindowIcon(SDL_Window * window, SDL_Surface * icon);
// void PND_SetWindowPosition(SDL_Window * window);
// void PND_SetWindowSize(SDL_Window * window);
// void PND_ShowWindow(SDL_Window * window);
// void PND_HideWindow(SDL_Window * window);
// void PND_RaiseWindow(SDL_Window * window);
// void PND_MaximizeWindow(SDL_Window * window);
void PND_MinimizeWindow(SDL_Window * window);
// void PND_RestoreWindow(SDL_Window * window);
void PND_DestroyWindow(SDL_Window * window);

/* Window manager function */
#if 0
SDL_bool PND_GetWindowWMInfo(SDL_Window * window,
                             struct SDL_SysWMinfo *info);
#endif
/* OpenGL/OpenGL ES functions */
#ifdef SDL_VIDEO_OPENGL_EGL
#define PND_GL_GetDrawableSize SDL_PrivateGetWindowSizeInPixels

int PND_GL_LoadLibrary(_THIS, const char *path);
void *PND_GL_GetProcAddress(const char *proc);
void PND_GL_UnloadLibrary(_THIS);
SDL_GLContext PND_GL_CreateContext(_THIS, SDL_Window * window);
int PND_GL_MakeCurrent(SDL_Window * window, SDL_GLContext context);
int PND_GL_SetSwapInterval(int interval);
int PND_GL_GetSwapInterval(void);
int PND_GL_SwapWindow(_THIS, SDL_Window * window);
void PND_GL_DeleteContext(SDL_GLContext context);
#endif

#endif /* __SDL_PANDORA_H__ */

/* vi: set ts=4 sw=4 expandtab: */
