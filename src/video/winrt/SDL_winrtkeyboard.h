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
#ifndef SDL_winrtkeyboard_h_
#define SDL_winrtkeyboard_h_

/*
 * Internal-use, C++/CX functions:
 */
#ifdef __cplusplus_winrt

extern void WINRT_ProcessKeyDownEvent(Windows::UI::Core::KeyEventArgs ^ args);
extern void WINRT_ProcessKeyUpEvent(Windows::UI::Core::KeyEventArgs ^ args);
extern void WINRT_ProcessCharacterReceivedEvent(Windows::UI::Core::CharacterReceivedEventArgs ^ args);

#if NTDDI_VERSION >= NTDDI_WIN10
extern void WINTRT_InitialiseInputPaneEvents(void);
extern SDL_bool WINRT_HasScreenKeyboardSupport(void);
extern void WINRT_ShowScreenKeyboard(SDL_Window *window);
extern void WINRT_HideScreenKeyboard(SDL_Window *window);
extern SDL_bool WINRT_IsScreenKeyboardShown(SDL_Window *window);
#endif // NTDDI_VERSION >= ...

#endif // ifdef __cplusplus_winrt

#endif // SDL_winrtkeyboard_h_

/* vi: set ts=4 sw=4 expandtab: */