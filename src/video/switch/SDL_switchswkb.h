//
// Created by cpasjuste on 22/04/2020.
//

#ifndef SDL2_SDL_SWITCHSWKB_H
#define SDL2_SDL_SWITCHSWKB_H

#include "../../events/SDL_events_c.h"

extern void SWITCH_InitSwkb();
extern void SWITCH_PollSwkb();
extern void SWITCH_QuitSwkb();

extern SDL_bool SWITCH_HasScreenKeyboardSupport();
extern SDL_bool SWITCH_IsScreenKeyboardShown(SDL_Window * window);

extern void SWITCH_StartTextInput();
extern void SWITCH_StopTextInput();

#endif //SDL2_SDL_SWITCHSWKB_H
