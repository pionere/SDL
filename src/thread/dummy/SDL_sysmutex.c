/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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

/* An implementation of mutexes using semaphores */

#include <SDL3/SDL_thread.h>
#include "SDL_systhread_c.h"


SDL_mutex *
SDL_CreateMutex(void)
{
    SDL_SetError("SDL not built with thread support");
    return (SDL_mutex *) 0;
}

void
SDL_DestroyMutex(SDL_mutex * mutex)
{
}

int
SDL_mutexP(SDL_mutex * mutex)
{
    return SDL_SetError("SDL not built with thread support");
}

int
SDL_TryLockMutex(SDL_mutex * mutex)
{
    return SDL_SetError("SDL not built with thread support");
}

int
SDL_mutexV(SDL_mutex * mutex)
{
    return SDL_SetError("SDL not built with thread support");
}

/* vi: set ts=4 sw=4 expandtab: */
