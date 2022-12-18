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
#ifndef SDL_internal_h_
#define SDL_internal_h_

/* Many of SDL's features require _GNU_SOURCE on various platforms */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* Need this so Linux systems define fseek64o, ftell64o and off64_t */
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

/* This is for a variable-length array at the end of a struct:
    struct x { int y; char z[SDL_VARIABLE_LENGTH_ARRAY]; };
   Use this because GCC 2 needs different magic than other compilers. */
#if (defined(__GNUC__) && (__GNUC__ <= 2)) || defined(__CC_ARM) || defined(__cplusplus)
#define SDL_VARIABLE_LENGTH_ARRAY 1
#else
#define SDL_VARIABLE_LENGTH_ARRAY
#endif

#define SDL_MAX_SMALL_ALLOC_STACKSIZE          128
#define SDL_small_alloc(type, count, pisstack) ((*(pisstack) = ((sizeof(type) * (count)) < SDL_MAX_SMALL_ALLOC_STACKSIZE)), (*(pisstack) ? SDL_stack_alloc(type, count) : (type *)SDL_malloc(sizeof(type) * (count))))
#define SDL_small_free(ptr, isstack) \
    if ((isstack)) {                 \
        SDL_stack_free(ptr);         \
    } else {                         \
        SDL_free(ptr);               \
    }

#include "dynapi/SDL_dynapi.h"

#if SDL_DYNAMIC_API
#include "dynapi/SDL_dynapi_overrides.h"
/* force DECLSPEC off...it's all internal symbols now.
   These will have actual #defines during SDL_dynapi.c only */
#define DECLSPEC
#endif

#include "build_config/SDL_build_config.h"

#ifdef __APPLE__
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE 1 /* for memset_pattern4() */
#endif
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#if defined(HAVE_STDLIB_H)
#include <stdlib.h>
#elif defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif
#if defined(HAVE_STDDEF_H)
#include <stddef.h>
#endif
#if defined(HAVE_STDARG_H)
#include <stdarg.h>
#endif
#ifdef HAVE_STRING_H
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
#if defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#elif defined(HAVE_STDINT_H)
#include <stdint.h>
#endif
#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_FLOAT_H
#include <float.h>
#endif

/* If you run into a warning that O_CLOEXEC is redefined, update the SDL configuration header for your platform to add HAVE_O_CLOEXEC */
#ifndef HAVE_O_CLOEXEC
#define O_CLOEXEC 0
#endif

/* A few #defines to reduce SDL footprint. */
#ifndef SDL_LEAN_AND_MEAN
#define SDL_LEAN_AND_MEAN 0
#endif
/* Check config. */
#ifdef SDL_BLIT_0_DISABLED
#define SDL_HAVE_BLIT_0                 0
#endif
#ifdef SDL_BLIT_1_DISABLED
#define SDL_HAVE_BLIT_1                 0
#endif
#ifdef SDL_BLIT_A_DISABLED
#define SDL_HAVE_BLIT_A                 0
#endif
#ifdef SDL_BLIT_N_DISABLED
#define SDL_HAVE_BLIT_N                 0
#endif
#ifdef SDL_BLIT_N_RGB565_DISABLED
#define SDL_HAVE_BLIT_N_RGB565          0
#endif
#ifdef SDL_BLIT_AUTO_DISABLED
#define SDL_HAVE_BLIT_AUTO              0
#endif
#ifdef SDL_BLIT_SLOW_DISABLED
#define SDL_HAVE_BLIT_SLOW              0
#endif
#ifdef SDL_RLE_ACCEL_DISABLED
#define SDL_HAVE_RLE                    0
#endif
#ifdef SDL_BLIT_TRANSFORM_DISABLED
#define SDL_HAVE_BLIT_TRANSFORM         0
#endif
#ifdef SDL_YUV_FORMAT_DISABLED
#define SDL_HAVE_YUV                    0
#endif

/* Optimized functions from 'SDL_blit_0.c'
   - blit with source BitsPerPixel < 8, palette */
#ifndef SDL_HAVE_BLIT_0
#define SDL_HAVE_BLIT_0 !SDL_LEAN_AND_MEAN
#endif

/* Optimized functions from 'SDL_blit_1.c'
   - blit with source BytesPerPixel == 1, palette */
#ifndef SDL_HAVE_BLIT_1
#define SDL_HAVE_BLIT_1 !SDL_LEAN_AND_MEAN
#endif

/* Optimized functions from 'SDL_blit_A.c'
   - blit with 'SDL_BLENDMODE_BLEND' blending mode */
#ifndef SDL_HAVE_BLIT_A
#define SDL_HAVE_BLIT_A !SDL_LEAN_AND_MEAN
#endif

/* Optimized functions from 'SDL_blit_N.c'
   - blit with COLORKEY mode, or nothing */
#ifndef SDL_HAVE_BLIT_N
#define SDL_HAVE_BLIT_N !SDL_LEAN_AND_MEAN
#endif

/* Optimized functions from 'SDL_blit_N.c'
   - RGB565 conversion with Lookup tables */
#ifndef SDL_HAVE_BLIT_N_RGB565
#define SDL_HAVE_BLIT_N_RGB565 !SDL_LEAN_AND_MEAN
#endif

/* Optimized functions from 'SDL_blit_AUTO.c'
   - blit with modulate color, modulate alpha, any blending mode
   - scaling or not */
#ifndef SDL_HAVE_BLIT_AUTO
#define SDL_HAVE_BLIT_AUTO !SDL_LEAN_AND_MEAN
#endif

/* Unoptimized functions from 'SDL_blit_slow.c' */
#ifndef SDL_HAVE_BLIT_SLOW
#define SDL_HAVE_BLIT_SLOW              !SDL_LEAN_AND_MEAN
#endif

/* Run-Length-Encoding
   - SDL_SetColorKey() called with SDL_RLEACCEL flag */
#ifndef SDL_HAVE_RLE
#define SDL_HAVE_RLE !SDL_LEAN_AND_MEAN
#endif

/* Support for blit transformations (modulo/alpha)
   - used in 'SDL_blit_AUTO.c' and in 'SDL_blit_1.c' */
#ifndef SDL_HAVE_BLIT_TRANSFORM
#define SDL_HAVE_BLIT_TRANSFORM         !SDL_LEAN_AND_MEAN
#endif

/* Software SDL_Renderer
   - creation of software renderer
   - *not* general blitting functions
   - {blend,draw}{fillrect,line,point} internal functions */
#ifndef SDL_VIDEO_RENDER_SW
#define SDL_VIDEO_RENDER_SW !SDL_LEAN_AND_MEAN
#endif

/* YUV formats
   - handling of YUV surfaces
   - blitting and conversion functions */
#ifndef SDL_HAVE_YUV
#define SDL_HAVE_YUV !SDL_LEAN_AND_MEAN
#endif

#include <SDL3/SDL.h>
#define SDL_MAIN_NOIMPL /* don't drag in header-only implementation of SDL_main */
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_log.h>

/* Override SDL_*Log functions.
  Necessary to eliminate the calls and the strings as well. */
#if SDL_LOGGING_DISABLED && !SDL_DYNAMIC_API
#define SDL_LogGetPriority(category) SDL_LOG_PRIORITY_CRITICAL
#define SDL_LogResetPriorities()
#define SDL_Log(fmt, ...)
#define SDL_LogVerbose(category, fmt, ...)
#define SDL_LogDebug(category, fmt, ...)
#define SDL_LogInfo(category, fmt, ...)
#define SDL_LogWarn(category, fmt, ...)
#define SDL_LogError(category, fmt, ...)
#define SDL_LogCritical(category, fmt, ...)
#define SDL_LogMessage(category, priority, fmt, ...)
#define SDL_LogMessageV(category, priority, fmt, ap)
#endif

/* Override SDL_*Error functions.
  Necessary to eliminate the calls and the strings as well. */
#if SDL_VERBOSE_ERROR_DISABLED && !SDL_DYNAMIC_API
#define SDL_SetError(fmt, ...) -1
#define SDL_GetError() ""
#define SDL_ClearError()
#define SDL_GetErrorMsg(errstr, maxlen) errstr
#endif

/* The internal implementations of these functions have up to nanosecond precision.
   We can expose these functions as part of the API if we want to later.
*/
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

extern DECLSPEC int SDLCALL SDL_SemWaitTimeoutNS(SDL_sem *sem, Sint64 timeoutNS);
extern DECLSPEC int SDLCALL SDL_CondWaitTimeoutNS(SDL_cond *cond, SDL_mutex *mutex, Sint64 timeoutNS);
extern DECLSPEC int SDLCALL SDL_WaitEventTimeoutNS(SDL_Event *event, Sint64 timeoutNS);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif

#endif /* SDL_internal_h_ */

/* vi: set ts=4 sw=4 expandtab: */
