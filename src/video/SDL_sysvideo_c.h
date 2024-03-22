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

#ifndef SDL_sysvideo_c_h_
#define SDL_sysvideo_c_h_

#include "SDL_sysvideo.h"

#define BOOTSTRAP_INSTANCE(x) extern const VideoBootStrap x;

#ifdef SDL_VIDEO_DRIVER_COCOA
#define COCOA_BOOTSTRAP       BOOTSTRAP_INSTANCE(COCOA_bootstrap)
#define COCOA_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_COCOA,
#define COCOA_BOOTSTRAP_ENTRY &COCOA_bootstrap,
#include "cocoa/SDL_cocoamessagebox.h"
#define COCOA_MSGBOX_ENTRY    &Cocoa_ShowMessageBox,
#else
#define COCOA_BOOTSTRAP
#define COCOA_BOOTSTRAP_ENUM
#define COCOA_BOOTSTRAP_ENTRY
#define COCOA_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_X11
#define X11_BOOTSTRAP       BOOTSTRAP_INSTANCE(X11_bootstrap)
#define X11_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_X11,
#define X11_BOOTSTRAP_ENTRY &X11_bootstrap,
#include "x11/SDL_x11messagebox.h"
#define X11_MSGBOX_ENTRY    &X11_ShowMessageBox,
#else
#define X11_BOOTSTRAP
#define X11_BOOTSTRAP_ENUM
#define X11_BOOTSTRAP_ENTRY
#define X11_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_WAYLAND
#define Wayland_BOOTSTRAP       BOOTSTRAP_INSTANCE(Wayland_bootstrap)
#define Wayland_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_Wayland,
#define Wayland_BOOTSTRAP_ENTRY &Wayland_bootstrap,
#include "wayland/SDL_waylandmessagebox.h"
#define Wayland_MSGBOX_ENTRY    &Wayland_ShowMessageBox,
#else
#define Wayland_BOOTSTRAP
#define Wayland_BOOTSTRAP_ENUM
#define Wayland_BOOTSTRAP_ENTRY
#define Wayland_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_VIVANTE
#define VIVANTE_BOOTSTRAP       BOOTSTRAP_INSTANCE(VIVANTE_bootstrap)
#define VIVANTE_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_VIVANTE,
#define VIVANTE_BOOTSTRAP_ENTRY &VIVANTE_bootstrap,
#define VIVANTE_MSGBOX_ENTRY    NULL,
#else
#define VIVANTE_BOOTSTRAP
#define VIVANTE_BOOTSTRAP_ENUM
#define VIVANTE_BOOTSTRAP_ENTRY
#define VIVANTE_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_DIRECTFB
#define DirectFB_BOOTSTRAP       BOOTSTRAP_INSTANCE(DirectFB_bootstrap)
#define DirectFB_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_DirectFB,
#define DirectFB_BOOTSTRAP_ENTRY &DirectFB_bootstrap,
#define DirectFB_MSGBOX_ENTRY    NULL,
#else
#define DirectFB_BOOTSTRAP
#define DirectFB_BOOTSTRAP_ENUM
#define DirectFB_BOOTSTRAP_ENTRY
#define DirectFB_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_WINDOWS
#define WIN_BOOTSTRAP       BOOTSTRAP_INSTANCE(WIN_bootstrap)
#define WIN_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_WIN,
#define WIN_BOOTSTRAP_ENTRY &WIN_bootstrap,
#if !defined(__XBOXONE__) && !defined(__XBOXSERIES__)
#include "windows/SDL_windowsmessagebox.h"
#define WIN_MSGBOX_ENTRY    &WIN_ShowMessageBox,
#else
#define WIN_MSGBOX_ENTRY    NULL,
#endif
#else
#define WIN_BOOTSTRAP
#define WIN_BOOTSTRAP_ENUM
#define WIN_BOOTSTRAP_ENTRY
#define WIN_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_WINRT
#define WINRT_BOOTSTRAP       BOOTSTRAP_INSTANCE(WINRT_bootstrap)
#define WINRT_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_WINRT,
#define WINRT_BOOTSTRAP_ENTRY &WINRT_bootstrap,
#include "winrt/SDL_winrtmessagebox.h"
#define WINRT_MSGBOX_ENTRY    &WINRT_ShowMessageBox,
#else
#define WINRT_BOOTSTRAP
#define WINRT_BOOTSTRAP_ENUM
#define WINRT_BOOTSTRAP_ENTRY
#define WINRT_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_HAIKU
#define HAIKU_BOOTSTRAP       BOOTSTRAP_INSTANCE(HAIKU_bootstrap)
#define HAIKU_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_HAIKU,
#define HAIKU_BOOTSTRAP_ENTRY &HAIKU_bootstrap,
#include "haiku/SDL_bmessagebox.h"
#define HAIKU_MSGBOX_ENTRY    &HAIKU_ShowMessageBox,
#else
#define HAIKU_BOOTSTRAP
#define HAIKU_BOOTSTRAP_ENUM
#define HAIKU_BOOTSTRAP_ENTRY
#define HAIKU_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_PANDORA
#define PND_BOOTSTRAP       BOOTSTRAP_INSTANCE(PND_bootstrap)
#define PND_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_PND,
#define PND_BOOTSTRAP_ENTRY &PND_bootstrap,
#define PND_MSGBOX_ENTRY    NULL,
#else
#define PND_BOOTSTRAP
#define PND_BOOTSTRAP_ENUM
#define PND_BOOTSTRAP_ENTRY
#define PND_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_UIKIT
#define UIKit_BOOTSTRAP       BOOTSTRAP_INSTANCE(UIKit_bootstrap)
#define UIKit_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_UIKit,
#define UIKit_BOOTSTRAP_ENTRY &UIKit_bootstrap,
#include "uikit/SDL_uikitmessagebox.h"
#define UIKit_MSGBOX_ENTRY    &UIKit_ShowMessageBox,
#else
#define UIKit_BOOTSTRAP
#define UIKit_BOOTSTRAP_ENUM
#define UIKit_BOOTSTRAP_ENTRY
#define UIKit_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_ANDROID
#define Android_BOOTSTRAP       BOOTSTRAP_INSTANCE(Android_bootstrap)
#define Android_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_Android,
#define Android_BOOTSTRAP_ENTRY &Android_bootstrap,
#include "android/SDL_androidmessagebox.h"
#define Android_MSGBOX_ENTRY    &Android_ShowMessageBox,
#else
#define Android_BOOTSTRAP
#define Android_BOOTSTRAP_ENUM
#define Android_BOOTSTRAP_ENTRY
#define Android_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_PS2
#define PS2_BOOTSTRAP       BOOTSTRAP_INSTANCE(PS2_bootstrap)
#define PS2_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_PS2,
#define PS2_BOOTSTRAP_ENTRY &PS2_bootstrap,
#define PS2_MSGBOX_ENTRY    NULL,
#else
#define PS2_BOOTSTRAP
#define PS2_BOOTSTRAP_ENUM
#define PS2_BOOTSTRAP_ENTRY
#define PS2_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_PSP
#define PSP_BOOTSTRAP       BOOTSTRAP_INSTANCE(PSP_bootstrap)
#define PSP_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_PSP,
#define PSP_BOOTSTRAP_ENTRY &PSP_bootstrap,
#define PSP_MSGBOX_ENTRY    NULL,
#else
#define PSP_BOOTSTRAP
#define PSP_BOOTSTRAP_ENUM
#define PSP_BOOTSTRAP_ENTRY
#define PSP_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_VITA
#define VITA_BOOTSTRAP       BOOTSTRAP_INSTANCE(VITA_bootstrap)
#define VITA_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_VITA,
#define VITA_BOOTSTRAP_ENTRY &VITA_bootstrap,
#include "vita/SDL_vitamessagebox.h"
#define VITA_MSGBOX_ENTRY    &VITA_ShowMessageBox,
#else
#define VITA_BOOTSTRAP
#define VITA_BOOTSTRAP_ENUM
#define VITA_BOOTSTRAP_ENTRY
#define VITA_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_N3DS
#define N3DS_BOOTSTRAP       BOOTSTRAP_INSTANCE(N3DS_bootstrap)
#define N3DS_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_N3DS,
#define N3DS_BOOTSTRAP_ENTRY &N3DS_bootstrap,
#define N3DS_MSGBOX_ENTRY    NULL,
#else
#define N3DS_BOOTSTRAP
#define N3DS_BOOTSTRAP_ENUM
#define N3DS_BOOTSTRAP_ENTRY
#define N3DS_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_KMSDRM
#define KMSDRM_BOOTSTRAP       BOOTSTRAP_INSTANCE(KMSDRM_bootstrap)
#define KMSDRM_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_KMSDRM,
#define KMSDRM_BOOTSTRAP_ENTRY &KMSDRM_bootstrap,
#define KMSDRM_MSGBOX_ENTRY    NULL,
#else
#define KMSDRM_BOOTSTRAP
#define KMSDRM_BOOTSTRAP_ENUM
#define KMSDRM_BOOTSTRAP_ENTRY
#define KMSDRM_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_RISCOS
#define RISCOS_BOOTSTRAP       BOOTSTRAP_INSTANCE(RISCOS_bootstrap)
#define RISCOS_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_RISCOS,
#define RISCOS_BOOTSTRAP_ENTRY &RISCOS_bootstrap,
#include "riscos/SDL_riscosmessagebox.h"
#define RISCOS_MSGBOX_ENTRY    &RISCOS_ShowMessageBox,
#else
#define RISCOS_BOOTSTRAP
#define RISCOS_BOOTSTRAP_ENUM
#define RISCOS_BOOTSTRAP_ENTRY
#define RISCOS_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_RPI
#define RPI_BOOTSTRAP       BOOTSTRAP_INSTANCE(RPI_bootstrap)
#define RPI_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_RPI,
#define RPI_BOOTSTRAP_ENTRY &RPI_bootstrap,
#define RPI_MSGBOX_ENTRY    NULL,
#else
#define RPI_BOOTSTRAP
#define RPI_BOOTSTRAP_ENUM
#define RPI_BOOTSTRAP_ENTRY
#define RPI_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_NACL
#define NACL_BOOTSTRAP       BOOTSTRAP_INSTANCE(NACL_bootstrap)
#define NACL_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_NACL,
#define NACL_BOOTSTRAP_ENTRY &NACL_bootstrap,
#define NACL_MSGBOX_ENTRY    NULL,
#else
#define NACL_BOOTSTRAP
#define NACL_BOOTSTRAP_ENUM
#define NACL_BOOTSTRAP_ENTRY
#define NACL_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_EMSCRIPTEN
#define Emscripten_BOOTSTRAP       BOOTSTRAP_INSTANCE(Emscripten_bootstrap)
#define Emscripten_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_Emscripten,
#define Emscripten_BOOTSTRAP_ENTRY &Emscripten_bootstrap,
#define Emscripten_MSGBOX_ENTRY    NULL,
#else
#define Emscripten_BOOTSTRAP
#define Emscripten_BOOTSTRAP_ENUM
#define Emscripten_BOOTSTRAP_ENTRY
#define Emscripten_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_QNX
#define QNX_BOOTSTRAP       BOOTSTRAP_INSTANCE(QNX_bootstrap)
#define QNX_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_QNX,
#define QNX_BOOTSTRAP_ENTRY &QNX_bootstrap,
#define QNX_MSGBOX_ENTRY    NULL,
#else
#define QNX_BOOTSTRAP
#define QNX_BOOTSTRAP_ENUM
#define QNX_BOOTSTRAP_ENTRY
#define QNX_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_OS2
#define OS2DIVE_BOOTSTRAP       BOOTSTRAP_INSTANCE(OS2DIVE_bootstrap)
#define OS2DIVE_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_OS2DIVE,
#define OS2DIVE_BOOTSTRAP_ENTRY &OS2DIVE_bootstrap,
#include "os2/SDL_os2messagebox.h"
#define OS2DIVE_MSGBOX_ENTRY    &OS2_ShowMessageBox,
#define OS2VMAN_BOOTSTRAP       BOOTSTRAP_INSTANCE(OS2VMAN_bootstrap)
#define OS2VMAN_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_OS2VMAN,
#define OS2VMAN_BOOTSTRAP_ENTRY &OS2VMAN_bootstrap,
#define OS2VMAN_MSGBOX_ENTRY    &OS2_ShowMessageBox,
#else
#define OS2DIVE_BOOTSTRAP
#define OS2DIVE_BOOTSTRAP_ENUM
#define OS2DIVE_BOOTSTRAP_ENTRY
#define OS2DIVE_MSGBOX_ENTRY
#define OS2VMAN_BOOTSTRAP
#define OS2VMAN_BOOTSTRAP_ENUM
#define OS2VMAN_BOOTSTRAP_ENTRY
#define OS2VMAN_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_NGAGE
#define NGAGE_BOOTSTRAP       BOOTSTRAP_INSTANCE(NGAGE_bootstrap)
#define NGAGE_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_NGAGE,
#define NGAGE_BOOTSTRAP_ENTRY &NGAGE_bootstrap,
#define NGAGE_MSGBOX_ENTRY    NULL,
#else
#define NGAGE_BOOTSTRAP
#define NGAGE_BOOTSTRAP_ENUM
#define NGAGE_BOOTSTRAP_ENTRY
#define NGAGE_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_OFFSCREEN
#define OFFSCREEN_BOOTSTRAP       BOOTSTRAP_INSTANCE(OFFSCREEN_bootstrap)
#define OFFSCREEN_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_OFFSCREEN,
#define OFFSCREEN_BOOTSTRAP_ENTRY &OFFSCREEN_bootstrap,
#define OFFSCREEN_MSGBOX_ENTRY    NULL,
#else
#define OFFSCREEN_BOOTSTRAP
#define OFFSCREEN_BOOTSTRAP_ENUM
#define OFFSCREEN_BOOTSTRAP_ENTRY
#define OFFSCREEN_MSGBOX_ENTRY
#endif
#ifdef SDL_VIDEO_DRIVER_DUMMY
#define DUMMY_BOOTSTRAP       BOOTSTRAP_INSTANCE(DUMMY_bootstrap)
#define DUMMY_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_DUMMY,
#define DUMMY_BOOTSTRAP_ENTRY &DUMMY_bootstrap,
#define DUMMY_MSGBOX_ENTRY    NULL,
#ifdef SDL_INPUT_LINUXEV
#define DUMMY_evdev_BOOTSTRAP       BOOTSTRAP_INSTANCE(DUMMY_evdev_bootstrap)
#define DUMMY_evdev_BOOTSTRAP_ENUM  SDL_VIDEODRIVER_DUMMY_evdev,
#define DUMMY_evdev_BOOTSTRAP_ENTRY &DUMMY_evdev_bootstrap,
#define DUMMY_evdev_MSGBOX_ENTRY    NULL,
#else
#define DUMMY_evdev_BOOTSTRAP
#define DUMMY_evdev_BOOTSTRAP_ENUM
#define DUMMY_evdev_BOOTSTRAP_ENTRY
#define DUMMY_evdev_MSGBOX_ENTRY
#endif
#else
#define DUMMY_BOOTSTRAP
#define DUMMY_BOOTSTRAP_ENUM
#define DUMMY_BOOTSTRAP_ENTRY
#define DUMMY_MSGBOX_ENTRY
#define DUMMY_evdev_BOOTSTRAP
#define DUMMY_evdev_BOOTSTRAP_ENUM
#define DUMMY_evdev_BOOTSTRAP_ENTRY
#define DUMMY_evdev_MSGBOX_ENTRY
#endif

/* Video drivers enum */
typedef enum
{
    COCOA_BOOTSTRAP_ENUM
    X11_BOOTSTRAP_ENUM
    Wayland_BOOTSTRAP_ENUM
    VIVANTE_BOOTSTRAP_ENUM
    DirectFB_BOOTSTRAP_ENUM
    WIN_BOOTSTRAP_ENUM
    WINRT_BOOTSTRAP_ENUM
    HAIKU_BOOTSTRAP_ENUM
    PND_BOOTSTRAP_ENUM
    UIKit_BOOTSTRAP_ENUM
    Android_BOOTSTRAP_ENUM
    PS2_BOOTSTRAP_ENUM
    PSP_BOOTSTRAP_ENUM
    VITA_BOOTSTRAP_ENUM
    N3DS_BOOTSTRAP_ENUM
    KMSDRM_BOOTSTRAP_ENUM
    RISCOS_BOOTSTRAP_ENUM
    RPI_BOOTSTRAP_ENUM
    NACL_BOOTSTRAP_ENUM
    Emscripten_BOOTSTRAP_ENUM
    QNX_BOOTSTRAP_ENUM
    OS2DIVE_BOOTSTRAP_ENUM
    OS2VMAN_BOOTSTRAP_ENUM
    NGAGE_BOOTSTRAP_ENUM
    OFFSCREEN_BOOTSTRAP_ENUM
    DUMMY_BOOTSTRAP_ENUM
    DUMMY_evdev_BOOTSTRAP_ENUM
    SDL_VIDEODRIVERS_count
} SDL_VIDEODRIVERS;

/* Video driver instances */
COCOA_BOOTSTRAP
X11_BOOTSTRAP
Wayland_BOOTSTRAP
VIVANTE_BOOTSTRAP
DirectFB_BOOTSTRAP
WIN_BOOTSTRAP
WINRT_BOOTSTRAP
HAIKU_BOOTSTRAP
PND_BOOTSTRAP
UIKit_BOOTSTRAP
Android_BOOTSTRAP
PS2_BOOTSTRAP
PSP_BOOTSTRAP
VITA_BOOTSTRAP
N3DS_BOOTSTRAP
KMSDRM_BOOTSTRAP
RISCOS_BOOTSTRAP
RPI_BOOTSTRAP
NACL_BOOTSTRAP
Emscripten_BOOTSTRAP
QNX_BOOTSTRAP
OS2DIVE_BOOTSTRAP
OS2VMAN_BOOTSTRAP
NGAGE_BOOTSTRAP
OFFSCREEN_BOOTSTRAP
DUMMY_BOOTSTRAP
DUMMY_evdev_BOOTSTRAP

#endif /* SDL_sysvideo_c_h_ */

/* vi: set ts=4 sw=4 expandtab: */
