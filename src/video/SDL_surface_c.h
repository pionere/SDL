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

#ifndef SDL_surface_c_h_
#define SDL_surface_c_h_

#include "../SDL_internal.h"

/* Useful functions and variables from SDL_surface.c */

#include "SDL_render.h"
#include "SDL_surface.h"

#if !SDL_HAVE_RLE
#undef SDL_MUSTLOCK
#define SDL_MUSTLOCK(S) SDL_FALSE
#endif

/* private functions defined in SDL_surface.c */
extern SDL_Color SDL_PrivateGetSurfaceColorMod(SDL_Surface *surface);
extern int SDL_PrivateSetSurfaceColorMod(SDL_Surface *surface, SDL_Color colorMod);

extern int SDL_PrivateUpperBlitScaled(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect, SDL_ScaleMode scaleMode);

#endif /* SDL_surface_c_h_ */

/* vi: set ts=4 sw=4 expandtab: */
