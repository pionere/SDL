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

#ifndef SDL_config_ngage_h_
#define SDL_config_ngage_h_
#define SDL_config_h_

#include "SDL_platform.h"

#define SIZEOF_VOIDP 4

/* Useful headers */
#define HAVE_STDARG_H    1
#define HAVE_STDDEF_H    1
#define HAVE_STDIO_H     1
#define HAVE_STDLIB_H    1
#define HAVE_SYS_STAT_H  1
#define HAVE_MATH_H      1
#define HAVE_CEIL        1
#define HAVE_COPYSIGN    1
#define HAVE_COS         1
#define HAVE_EXP         1
#define HAVE_FABS        1
#define HAVE_FLOOR       1
#define HAVE_LOG         1
#define HAVE_LOG10       1
#define HAVE_SCALBN      1
#define HAVE_SIN         1
#define HAVE_SQRT        1
#define HAVE_TAN         1
#define HAVE_MALLOC      1
#define SDL_MAIN_NEEDED  1
#define LACKS_SYS_MMAN_H 1

/* Enable the N-Gage thread support (src/thread/ngage/\*.c) */
//#define SDL_THREAD_NGAGE 1

/* Enable the N-Gage timer support (src/timer/ngage/\*.c) */
#ifndef SDL_TIMER_NGAGE
#define SDL_TIMER_NGAGE  1
#endif

/* Enable the N-Gage video driver (src/video/ngage/\*.cpp) */
#define SDL_VIDEO_DRIVER_NGAGE 1

/* Enable appropriate renderer(s) */
#ifndef SDL_VIDEO_RENDER_NGAGE
#define SDL_VIDEO_RENDER_NGAGE    1
#endif

/* Enable the N-Gage audio driver (src/audio/ngage/\*.c and src/audio/ngage/\*.cpp) */
#ifndef SDL_AUDIO_DRIVER_NGAGE
#define SDL_AUDIO_DRIVER_NGAGE  1
#endif
/* Enable the dummy audio driver (src/audio/dummy/\*.c) */
//#ifndef SDL_AUDIO_DRIVER_DUMMY
//#define SDL_AUDIO_DRIVER_DUMMY  1
//#endif

/* Enable the stub joystick driver (src/joystick/dummy/\*.c) */
#define SDL_JOYSTICK_DISABLED   1

/* Enable the stub haptic driver (src/haptic/dummy/\*.c) */
#define SDL_HAPTIC_DISABLED 1

/* Enable the stub HIDAPI */
#define SDL_HIDAPI_DISABLED 1

/* Turn off the power driver (src/power/\*.c) */
#define SDL_POWER_DISABLED 1

/* Enable the stub sensor driver (src/sensor/dummy/\*.c) */
#define SDL_SENSOR_DISABLED 1

/* Enable the stub shared object loader (src/loadso/dummy/\*.c) */
#define SDL_LOADSO_DISABLED 1

/* Enable the N-Gage filesystem driver (src/filesystem/ngage/\*.c and src/filesystem/ngage/\*.cpp) */
#ifndef SDL_FILESYSTEM_NGAGE
#define SDL_FILESYSTEM_NGAGE  1
#endif
/* Enable the dummy filesystem driver (src/filesystem/dummy/\*.c) */
//#ifndef SDL_FILESYSTEM_DUMMY
//#define SDL_FILESYSTEM_DUMMY  1
//#endif

#endif /* SDL_config_ngage_h_ */
