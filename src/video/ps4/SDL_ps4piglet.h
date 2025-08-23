//
// Created by cpasjuste on 06/01/2022.
//
#include "../../SDL_internal.h"

#ifndef __SDL_PS4PIGLET_H__
#define __SDL_PS4PIGLET_H__

#ifdef SDL_VIDEO_OPENGL_EGL

#include <stdbool.h>

int PS4_PigletInit();

void PS4_PigletExit();

bool PS4_PigletShaccAvailable();

#endif //SDL_VIDEO_OPENGL_EGL

#endif //__SDL_PS4PIGLET_H__
