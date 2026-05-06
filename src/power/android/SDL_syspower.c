/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_POWER_DISABLED
#ifdef SDL_POWER_ANDROID

#include "SDL_power.h"
#include "../SDL_syspower.h"

#include "../../core/android/SDL_android.h"

SDL_bool SDL_GetPowerInfo_Android(SDL_PowerState *state, int *seconds, int *percent)
{
    SDL_AndroidPowerInfo power_info;
    SDL_PowerState power_state;

    if (Android_JNI_GetPowerInfo(&power_info)) {
        if (power_info.plugged) {
            if (power_info.charged) {
                power_state = SDL_POWERSTATE_CHARGED;
            } else if (power_info.battery) {
                power_state = SDL_POWERSTATE_CHARGING;
            } else {
                power_state = SDL_POWERSTATE_NO_BATTERY;
            }
        } else {
            power_state = SDL_POWERSTATE_ON_BATTERY;
        }
    } else {
        // power_info.seconds = -1;
        power_info.percent = -1;
        power_state = SDL_POWERSTATE_UNKNOWN;
    }

    *seconds = -1; // power_info.seconds;
    *percent = power_info.percent;
    *state = power_state;

    return SDL_TRUE;
}

#endif /* SDL_POWER_ANDROID */
#endif /* SDL_POWER_DISABLED */

/* vi: set ts=4 sw=4 expandtab: */
