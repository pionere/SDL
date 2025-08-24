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
#include "../../SDL_internal.h"

#ifndef __SDL_PS4VIDEO_H__
#define __SDL_PS4VIDEO_H__

#include "../SDL_sysvideo.h"
#ifdef SDL_VIDEO_OPENGL_EGL
#include "SDL_egl.h"

typedef struct SDL_WindowData
{
    EGLSurface egl_surface;
} SDL_WindowData;
#endif

#ifdef DEBUG_PS4_VIDEO
#define LOG_DEBUG_PS4_VIDEO(msg, ...) SDL_Log(msg, __VA_ARGS__);
#else
#define LOG_DEBUG_PS4_VIDEO(msg, ...)
#endif

int PS4_VideoInit(_THIS);
void PS4_VideoQuit(_THIS);
void PS4_GetDisplayModes(SDL_VideoDisplay *display);
int PS4_SetDisplayMode(SDL_VideoDisplay *display, SDL_DisplayMode *mode);
int PS4_CreateSDLWindow(_THIS, SDL_Window *window);
void PS4_SetWindowSize(SDL_Window *window);
void PS4_DestroyWindow(SDL_Window *window);
void PS4_PumpEvents();

#endif /* __SDL_PS4VIDEO_H__ */

/* vi: set ts=4 sw=4 expandtab: */