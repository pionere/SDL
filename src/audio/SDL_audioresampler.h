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

#ifndef SDL_audioresampler_h_
#define SDL_audioresampler_h_

#include "../SDL_internal.h"

typedef int (*SDL_AudioResampler) (const Uint8 channels, const Uint64 step, // int inrate, const int outrate,
                             const float *inbuffer, const int inframes,
                             float *outbuffer, const int outframes);

extern SDL_AudioResampler SDL_Resampler_Mono;
extern SDL_AudioResampler SDL_Resampler_Stereo;
extern SDL_AudioResampler SDL_Resampler_Generic;

extern int ResamplerPadding();

#endif /* SDL_audioresampler_h_ */

/* vi: set ts=4 sw=4 expandtab: */
