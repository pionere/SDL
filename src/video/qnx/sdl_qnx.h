/*
  Simple DirectMedia Layer
  Copyright (C) 2017 BlackBerry Limited

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

#ifndef __SDL_QNX_H__
#define __SDL_QNX_H__

#include "../SDL_sysvideo.h"
#include <screen/screen.h>

#ifdef SDL_VIDEO_OPENGL_EGL
#include <EGL/egl.h>
#endif

typedef struct
{
    screen_window_t window;
#ifdef SDL_VIDEO_OPENGL_EGL
    EGLSurface      surface;
    EGLConfig       conf;
#endif
} window_impl_t;

extern void handleKeyboardEvent(screen_event_t event);

#ifdef SDL_VIDEO_OPENGL_EGL
extern int glGetConfig(EGLConfig *pconf, int *pformat);
extern int QNX_GL_LoadLibrary(_THIS, const char *name);
void *QNX_GL_GetProcAddress(const char *proc);
extern SDL_GLContext QNX_GL_CreateContext(_THIS, SDL_Window *window);
extern int QNX_GL_SetSwapInterval(int interval);
extern int QNX_GL_SwapWindow(_THIS, SDL_Window *window);
extern int QNX_GL_MakeCurrent(SDL_Window * window, SDL_GLContext context);
extern void QNX_GL_DeleteContext(SDL_GLContext context);
extern void QNX_GL_UnloadLibrary(_THIS);
#endif

#endif
