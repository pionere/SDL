/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_pspgl_c_h_
#define SDL_pspgl_c_h_

#if defined(SDL_VIDEO_DRIVER_PSP) && defined(SDL_VIDEO_OPENGL)

#include "../SDL_sysvideo.h"
#include "../SDL_egl_c.h"

#define PSP_GL_GetDrawableSize SDL_PrivateGetWindowSizeInPixels

extern void *PSP_GL_GetProcAddress(const char *proc);
extern int PSP_GL_MakeCurrent(SDL_Window *window, SDL_GLContext context);

extern int PSP_GL_SwapWindow(_THIS, SDL_Window *window);
extern SDL_GLContext PSP_GL_CreateContext(_THIS, SDL_Window *window);
extern void PSP_GL_DeleteContext(SDL_GLContext context);

extern int PSP_GL_LoadLibrary(_THIS, const char *path);
extern void PSP_GL_UnloadLibrary(_THIS);
extern int PSP_GL_SetSwapInterval(int interval);
extern int PSP_GL_GetSwapInterval(void);

extern void PSP_GL_DestroySurface(EGLSurface egl_surface);

#endif // SDL_VIDEO_DRIVER_PSP

#endif /* SDL_pspgl_c_h_ */

/* vi: set ts=4 sw=4 expandtab: */
