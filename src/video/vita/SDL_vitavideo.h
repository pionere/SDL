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

#ifndef _SDL_vitavideo_h
#define _SDL_vitavideo_h

#include "../../SDL_internal.h"
#include "../SDL_sysvideo.h"
#include "../SDL_egl_c.h"

#include <psp2/types.h>
#include <psp2/display.h>
#include <psp2/ime_dialog.h>
#include <psp2/sysmodule.h>

typedef struct Vita_VideoData
{
    SceWChar16 ime_buffer[SCE_IME_DIALOG_MAX_TEXT_LENGTH];
    SDL_bool ime_active;
} Vita_VideoData;

typedef struct SDL_WindowData
{
    SDL_bool uses_gles;
    SceUID buffer_uid;
    void *buffer;
#if defined(SDL_VIDEO_VITA_PVR)
    EGLSurface egl_surface;
    EGLContext egl_context;
#endif
} SDL_WindowData;

extern Vita_VideoData vitaVideoData;
extern SDL_Window *Vita_Window;

/****************************************************************************/
/* SDL_VideoDevice functions declaration                                    */
/****************************************************************************/

/* Display and window functions */
int VITA_VideoInit(_THIS);
void VITA_VideoQuit(_THIS);
int VITA_SetDisplayMode(SDL_VideoDisplay *display, SDL_DisplayMode *mode);
int VITA_CreateWindow(_THIS, SDL_Window *window);
int VITA_CreateWindowFrom(_THIS, SDL_Window *window, const void *data);
void VITA_SetWindowTitle(SDL_Window *window);
void VITA_SetWindowIcon(SDL_Window *window, SDL_Surface *icon);
void VITA_SetWindowPosition(SDL_Window *window);
void VITA_SetWindowSize(SDL_Window *window);
void VITA_ShowWindow(SDL_Window *window);
void VITA_HideWindow(_THIS, SDL_Window *window);
void VITA_RaiseWindow(_THIS, SDL_Window *window);
void VITA_MaximizeWindow(SDL_Window *window);
void VITA_MinimizeWindow(SDL_Window *window);
void VITA_RestoreWindow(SDL_Window *window);
void VITA_SetWindowGrab(SDL_Window *window, SDL_bool grabbed);
void VITA_DestroyWindow(_THIS, SDL_Window *window);

/* Window manager function */
SDL_bool VITA_GetWindowWMInfo(SDL_Window * window,
                             struct SDL_SysWMinfo *info);

#ifdef SDL_VIDEO_DRIVER_VITA
#if defined(SDL_VIDEO_VITA_PVR_OGL)
/* OpenGL functions */
int VITA_GL_LoadLibrary(_THIS, const char *path);
SDL_GLContext VITA_GL_CreateContext(_THIS, SDL_Window *window);
void *VITA_GL_GetProcAddress(_THIS, const char *proc);
#endif

/* OpenGLES functions */
int VITA_GLES_LoadLibrary(_THIS, const char *path);
void *VITA_GLES_GetProcAddress(_THIS, const char *proc);
void VITA_GLES_UnloadLibrary(_THIS);
SDL_GLContext VITA_GLES_CreateContext(_THIS, SDL_Window *window);
int VITA_GLES_MakeCurrent(_THIS, SDL_Window *window, SDL_GLContext context);
int VITA_GLES_SetSwapInterval(_THIS, int interval);
int VITA_GLES_GetSwapInterval(_THIS);
int VITA_GLES_SwapWindow(_THIS, SDL_Window *window);
void VITA_GLES_DeleteContext(_THIS, SDL_GLContext context);
#endif

/* VITA on screen keyboard */
SDL_bool VITA_HasScreenKeyboardSupport();
void VITA_ShowScreenKeyboard(SDL_Window *window);
void VITA_HideScreenKeyboard(SDL_Window *window);
SDL_bool VITA_IsScreenKeyboardShown(SDL_Window *window);

void VITA_PumpEvents(_THIS);

#endif /* _SDL_pspvideo_h */

/* vi: set ts=4 sw=4 expandtab: */
