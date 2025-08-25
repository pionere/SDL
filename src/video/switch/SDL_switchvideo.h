/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

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

#ifndef __SDL_SWITCHVIDEO_H__
#define __SDL_SWITCHVIDEO_H__

#include <switch.h>

#include "../../SDL_internal.h"
#include "../SDL_sysvideo.h"
#ifdef SDL_VIDEO_OPENGL_EGL
#include "SDL_egl.h"

typedef struct SDL_WindowData
{
    EGLSurface egl_surface;
} SDL_WindowData;
#endif

int SWITCH_VideoInit(_THIS);
void SWITCH_VideoQuit(_THIS);
void SWITCH_GetDisplayModes(SDL_VideoDisplay *display);
int SWITCH_SetDisplayMode(SDL_VideoDisplay *display, SDL_DisplayMode *mode);
int SWITCH_CreateSDLWindow(_THIS, SDL_Window *window);
void SWITCH_SetWindowTitle(SDL_Window *window);
void SWITCH_SetWindowIcon(SDL_Window *window, SDL_Surface *icon);
void SWITCH_SetWindowPosition(SDL_Window *window);
void SWITCH_SetWindowSize(SDL_Window *window);
void SWITCH_ShowWindow(SDL_Window *window);
void SWITCH_HideWindow(SDL_Window *window);
void SWITCH_RaiseWindow(SDL_Window *window);
void SWITCH_MaximizeWindow(SDL_Window *window);
void SWITCH_MinimizeWindow(SDL_Window *window);
void SWITCH_RestoreWindow(SDL_Window *window);
void SWITCH_DestroyWindow(SDL_Window *window);
void SWITCH_PumpEvents();

#endif /* __SDL_SWITCHVIDEO_H__ */

/* vi: set ts=4 sw=4 expandtab: */
