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
#include "../SDL_internal.h"

#include "SDL_video.h"
#include "SDL_sysvideo.h"
#include "SDL_blit.h"
#include "SDL_blit_auto.h"
#include "SDL_blit_copy.h"
#include "SDL_blit_slow.h"
#include "SDL_RLEaccel_c.h"
#include "SDL_pixels_c.h"

/* The general purpose software blit routine */
static int SDLCALL SDL_SoftBlit(SDL_Surface *src, SDL_Rect *srcrect,
                                SDL_Surface *dst, SDL_Rect *dstrect)
{
    int okay;
    SDL_bool src_locked;
    SDL_bool dst_locked;

    SDL_assert(!SDL_RectEmpty(srcrect) && !SDL_RectEmpty(dstrect));

    /* Everything is okay at the beginning...  */
    okay = 1;

    /* Lock the destination if it's in hardware */
    dst_locked = SDL_FALSE;
    if (SDL_MUSTLOCK(dst)) {
        if (SDL_LockSurface(dst) < 0) {
            okay = 0;
        } else {
            dst_locked = SDL_TRUE;
        }
    }
    /* Lock the source if it's in hardware */
    src_locked = SDL_FALSE;
    if (SDL_MUSTLOCK(src)) {
        if (SDL_LockSurface(src) < 0) {
            okay = 0;
        } else {
            src_locked = SDL_TRUE;
        }
    }

    /* Set up source and destination buffer pointers, and BLIT! */
    if (okay) {
        SDL_BlitFunc RunBlit;
        SDL_BlitInfo *info = &src->map->info;

        /* Set up the blit information */
        info->src = (Uint8 *)src->pixels +
                    (Uint16)srcrect->y * src->pitch +
                    (Uint16)srcrect->x * info->src_fmt->BytesPerPixel;
        info->src_w = srcrect->w;
        info->src_h = srcrect->h;
        info->src_pitch = src->pitch;
        info->src_skip =
            info->src_pitch - info->src_w * info->src_fmt->BytesPerPixel;
        info->dst =
            (Uint8 *)dst->pixels + (Uint16)dstrect->y * dst->pitch +
            (Uint16)dstrect->x * info->dst_fmt->BytesPerPixel;
        info->dst_w = dstrect->w;
        info->dst_h = dstrect->h;
        info->dst_pitch = dst->pitch;
        info->dst_skip =
            info->dst_pitch - info->dst_w * info->dst_fmt->BytesPerPixel;
        RunBlit = (SDL_BlitFunc)src->map->data;

        /* Run the actual software blit */
        RunBlit(info);
    }

    /* We need to unlock the surfaces if they're locked */
    if (dst_locked) {
        SDL_UnlockSurface(dst);
    }
    if (src_locked) {
        SDL_UnlockSurface(src);
    }
    /* Blit is done! */
    return okay - 1;
}

#if SDL_HAVE_BLIT_AUTO

#if 0
#ifdef __MACOSX__
#include <sys/sysctl.h>

static SDL_bool SDL_UseAltivecPrefetch()
{
    const char key[] = "hw.l3cachesize";
    u_int64_t result = 0;
    size_t typeSize = sizeof(result);

    if (sysctlbyname(key, &result, &typeSize, NULL, 0) == 0 && result > 0) {
        return SDL_TRUE;
    } else {
        return SDL_FALSE;
    }
}
#else
static SDL_bool SDL_UseAltivecPrefetch(void)
{
    /* Just guess G4 */
    return SDL_TRUE;
}
#endif /* __MACOSX__ */

#endif // 0

static SDL_BlitFunc SDL_CalculateBlitAuto(const SDL_BlitInfo *info)
{
    const SDL_BlitFuncEntry *entries = SDL_GeneratedBlitFuncTable;
    Uint32 src_format = info->src_fmt->format;
    Uint32 dst_format = info->dst_fmt->format;
    int flags = info->flags;
    int i, flagcheck = (flags & (SDL_COPY_MODULATE_COLOR | SDL_COPY_MODULATE_ALPHA | SDL_COPY_BLEND | SDL_COPY_ADD | SDL_COPY_MOD | SDL_COPY_MUL | SDL_COPY_COLORKEY | SDL_COPY_NEAREST));
#if 0
    static int features = -1;
    /* Get the available CPU features */
    if (features < 0) {
        /* Allow an override for testing .. */
        const char *hint = SDL_getenv("SDL_BLIT_CPU_FEATURES");
        if (hint) {
            (void)SDL_sscanf(hint, "%d", &features);
        }
        if (features < 0) {
            features = SDL_CPU_ANY;

            if (SDL_HasMMX()) {
                features |= SDL_CPU_MMX;
            }
            if (SDL_Has3DNow()) {
                features |= SDL_CPU_3DNOW;
            }
            if (SDL_HasSSE()) {
                features |= SDL_CPU_SSE;
            }
            if (SDL_HasSSE2()) {
                features |= SDL_CPU_SSE2;
            }
            if (SDL_HasAltiVec()) {
                if (SDL_UseAltivecPrefetch()) {
                    features |= SDL_CPU_ALTIVEC_PREFETCH;
                } else {
                    features |= SDL_CPU_ALTIVEC_NOPREFETCH;
                }
            }
        }
    }
#endif
    for (i = 0; entries[i].func; ++i) {
        /* Check for matching pixel formats */
        if (src_format != entries[i].src_format) {
            continue;
        }
        if (dst_format != entries[i].dst_format) {
            continue;
        }

        /* Check flags */
        if ((flagcheck & entries[i].flags) != flagcheck) {
            continue;
        }
#if 0
        /* Check CPU features */
        if ((entries[i].cpu & features) != entries[i].cpu) {
            continue;
        }
#endif
        /* We found the best one! */
        return entries[i].func;
    }
    return NULL;
}
#endif /* SDL_HAVE_BLIT_AUTO */

/* Figure out which of many blit routines to set up on a surface */
int SDL_CalculateBlit(SDL_Surface *surface)
{
    SDL_BlitFunc blit = NULL;
    SDL_BlitMap *map = surface->map;
    SDL_Surface *dst = map->dst;

#if SDL_HAVE_RLE
    /* Clean everything out to start */
    if ((surface->flags & SDL_RLEACCEL) == SDL_RLEACCEL) {
        SDL_UnRLESurface(surface, 1);
    }
#endif

    map->blit = SDL_SoftBlit;
    map->info.src_fmt = surface->format;
    map->info.src_pitch = surface->pitch;
    map->info.dst_fmt = dst->format;
    map->info.dst_pitch = dst->pitch;

#if SDL_HAVE_RLE
    /* See if we can do RLE acceleration */
    if (map->info.flags & SDL_COPY_RLE_DESIRED) {
        if (SDL_RLESurface(surface) == 0) {
            return 0;
        }
    }
#endif

    /* Choose a standard blit function */
    if (map->info.dst_fmt->BitsPerPixel < 8) {
        ; // We don't currently support blitting to < 8 bpp surfaces
    } else if (map->identity && !(map->info.flags & ~SDL_COPY_RLE_DESIRED)) {
        blit = SDL_BlitCopy;
    } else if (map->info.src_fmt->palette) {
        SDL_assert(SDL_ISPIXELFORMAT_INDEXED(map->info.src_fmt->format));
        if (map->info.src_fmt->BitsPerPixel < 8) {
#if SDL_HAVE_BLIT_0
            blit = SDL_CalculateBlit0(&map->info);
#endif
        } else {
            SDL_assert(map->info.src_fmt->BitsPerPixel == 8);
#if SDL_HAVE_BLIT_1
            blit = SDL_CalculateBlit1(&map->info);
#endif
        }
    } else {
        SDL_assert(!SDL_ISPIXELFORMAT_INDEXED(map->info.src_fmt->format));
        if (map->info.src_fmt->Rloss <= 8 && map->info.dst_fmt->Rloss <= 8) {
#if SDL_HAVE_BLIT_A
            if (map->info.flags & SDL_COPY_BLEND) {
                blit = SDL_CalculateBlitA(map);
            } else
#endif
            {
#if SDL_HAVE_BLIT_N
                blit = SDL_CalculateBlitN(map);
#endif
            }
#if SDL_HAVE_BLIT_AUTO
            if (!blit) {
                blit = SDL_CalculateBlitAuto(&map->info);
            }
#endif
        }
#if SDL_HAVE_BLIT_SLOW
#ifndef TEST_SLOW_BLIT
        if (!blit)
#endif
        {
            if (map->info.dst_fmt->palette == NULL) {
                blit = SDL_Blit_Slow;
            }
        }
#endif /* SDL_HAVE_BLIT_SLOW */
    }
    map->data = blit;

    /* Make sure we have a blit function */
    if (!blit) {
        SDL_InvalidateMap(map);
        return SDL_SetError("Blit combination not supported");
    }

    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */
