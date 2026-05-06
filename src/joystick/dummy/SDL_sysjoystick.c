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

#if defined(SDL_JOYSTICK_DUMMY) || defined(SDL_JOYSTICK_DISABLED)

/* This is the dummy implementation of the SDL joystick API */

#include "SDL_joystick.h"
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"

SDL_JoystickDriver SDL_DUMMY_JoystickDriver = {
    SDL_JoystickInit_Default,
    SDL_JoystickGetCount_Default,
    SDL_JoystickDetect_Default,
    SDL_JoystickIsDevicePresent_Default,
    SDL_JoystickGetDeviceName_Default,
    SDL_JoystickGetDevicePath_Default,
    SDL_JoystickGetDeviceSteamVirtualGamepadSlot_Default,
    SDL_JoystickGetDevicePlayerIndex_Default,
    SDL_JoystickSetDevicePlayerIndex_Default,
    SDL_JoystickGetDeviceGUID_Default,
    SDL_JoystickGetDeviceInstanceID_Default,
    SDL_JoystickOpen_Default,
    SDL_JoystickRumble_Default,
    SDL_JoystickRumbleTriggers_Default,
    SDL_JoystickGetCapabilities_Default,
    SDL_JoystickSetLED_Default,
    SDL_JoystickSendEffect_Default,
    SDL_JoystickSetSensorsEnabled_Default,
    SDL_JoystickUpdate_Default,
    SDL_JoystickClose_Default,
    SDL_JoystickQuit_Default,
    SDL_JoystickGetGamepadMapping_Default,
};

#endif /* SDL_JOYSTICK_DUMMY || SDL_JOYSTICK_DISABLED */

/* vi: set ts=4 sw=4 expandtab: */
