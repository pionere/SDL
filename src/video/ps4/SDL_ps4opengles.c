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

#ifdef SDL_VIDEO_OPENGL_EGL

#include <orbis/libkernel.h>
#include "SDL_video.h"
#include "SDL_ps4opengles.h"
#include "SDL_ps4video.h"

/* EGL implementation of SDL OpenGL support */

extern uint32_t PS4_PigletModId;

void PS4_GLES_InitDevice(_THIS)
{
    _this->GL_LoadLibrary = PS4_GLES_LoadLibrary;
    _this->GL_GetProcAddress = PS4_GLES_GetProcAddress;
    _this->GL_UnloadLibrary = PS4_GLES_UnloadLibrary;
    _this->GL_CreateContext = PS4_GLES_CreateContext;
    _this->GL_MakeCurrent = PS4_GLES_MakeCurrent;
    _this->GL_GetDrawableSize = PS4_GLES_GetDrawableSize;
    _this->GL_SetSwapInterval = PS4_GLES_SetSwapInterval;
    _this->GL_GetSwapInterval = PS4_GLES_GetSwapInterval;
    _this->GL_SwapWindow = PS4_GLES_SwapWindow;
    _this->GL_DeleteContext = PS4_GLES_DeleteContext;
}

/*void PS4_GLES_DefaultProfileConfig(int *mask, int *major, int *minor)
{
    *mask = SDL_GL_CONTEXT_PROFILE_ES;
    *major = 2;
    *minor = 0;
}*/

int
PS4_GLES_LoadLibrary(_THIS, const char *path) {
    return SDL_EGL_LoadLibrary(_this, path, EGL_DEFAULT_DISPLAY, 0);
}

void *
PS4_GLES_GetProcAddress(const char *proc) {
    void *ptr;
    int res;

    res = sceKernelDlsym((int) PS4_PigletModId, proc, (void **) &ptr);
    if (res != 0) {
        SDL_SetError("PS4_GLES_GetProcAddress: sceKernelDlsym failed: 0x%08x (%s == %p)\n", res, proc, ptr);
        return NULL;
    }

    return ptr;
}

SDL_EGL_CreateContext_impl(PS4)

SDL_EGL_MakeCurrent_impl(PS4)

SDL_EGL_SwapWindow_impl(PS4)

#endif /* SDL_VIDEO_OPENGL_EGL */

/* vi: set ts=4 sw=4 expandtab: */
