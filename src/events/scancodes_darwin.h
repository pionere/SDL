/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

#ifndef scancodes_darwin_h_
#define scancodes_darwin_h_

#include "../../include/SDL_scancode.h"

/* Mac virtual key code to SDL scancode mapping table
   Sources:
   - Inside Macintosh: Text <http://developer.apple.com/documentation/mac/Text/Text-571.html>
   - Apple USB keyboard driver source <http://darwinsource.opendarwin.org/10.4.6.ppc/IOHIDFamily-172.8/IOHIDFamily/Cosmo_USB2ADB.c>
   - experimentation on various ADB and USB ISO keyboards and one ADB ANSI keyboard
*/
extern const SDL_Scancode darwin_scancode_table[128];

#endif /* scancodes_darwin_h_ */

/* vi: set ts=4 sw=4 expandtab: */
