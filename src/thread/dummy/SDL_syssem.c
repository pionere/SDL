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

/* An implementation of semaphores using mutexes and condition variables */

#include "SDL_timer.h"
#include "SDL_thread.h"
#include "SDL_systhread_c.h"


SDL_sem *
SDL_CreateSemaphore(Uint32 initial_value)
{
    SDL_SetError("SDL not built with thread support");
    return (SDL_sem *) 0;
}

void
SDL_DestroySemaphore(SDL_sem * sem)
{
}

int
SDL_SemTryWait(SDL_sem * sem)
{
    return SDL_SetError("SDL not built with thread support");
}

int
SDL_SemWaitTimeout(SDL_sem * sem, Uint32 timeout)
{
    return SDL_SetError("SDL not built with thread support");
}

int
SDL_SemWait(SDL_sem * sem)
{
    return SDL_SetError("SDL not built with thread support");
}

Uint32
SDL_SemValue(SDL_sem * sem)
{
    return SDL_SetError("SDL not built with thread support");
}

int
SDL_SemPost(SDL_sem * sem)
{
    return SDL_SetError("SDL not built with thread support");
}

/* vi: set ts=4 sw=4 expandtab: */
