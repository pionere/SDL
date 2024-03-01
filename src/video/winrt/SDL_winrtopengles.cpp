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
#include "../../SDL_internal.h"

#if defined(SDL_VIDEO_DRIVER_WINRT) && defined(SDL_VIDEO_OPENGL_EGL)

/* EGL implementation of SDL OpenGL support */

#include "SDL_winrtvideo_cpp.h"
extern "C" {
#include "SDL_winrtopengles.h"
#include "SDL_loadso.h"
#include "../SDL_egl_c.h"
}

/* Windows includes */
#include <wrl/client.h>
using namespace Windows::UI::Core;

/* ANGLE/WinRT constants */
static const int ANGLE_D3D_FEATURE_LEVEL_ANY = 0;
#define EGL_PLATFORM_ANGLE_ANGLE                       0x3202
#define EGL_PLATFORM_ANGLE_TYPE_ANGLE                  0x3203
#define EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE     0x3204
#define EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE     0x3205
#define EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE            0x3208
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE           0x3209
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE      0x320B
#define EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE 0x320F

#define EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER 0x320B

/*
 * SDL/EGL top-level implementation
 */

extern "C" int
WINRT_GLES_LoadLibrary(_THIS, const char *path)
{
    int result;
    SDL_EGL_VideoData *egl_data;

    result = SDL_EGL_LoadLibrary(_this, path, EGL_DEFAULT_DISPLAY, 0);
    if (result < 0) {
        return result;
    }

    egl_data = _this->egl_data;
    /* Load ANGLE/WinRT-specific functions */
    CreateWinrtEglWindow_Old_Function CreateWinrtEglWindow = (CreateWinrtEglWindow_Old_Function)SDL_LoadFunction(egl_data->opengl_dll_handle, "CreateWinrtEglWindow");
    if (CreateWinrtEglWindow) {
        /* 'CreateWinrtEglWindow' was found, which means that an an older
         * version of ANGLE/WinRT is being used.  Continue setting up EGL,
         * as appropriate to this version of ANGLE.
         */

        /* Create an ANGLE/WinRT EGL-window */
        /* TODO, WinRT: check for XAML usage before accessing the CoreWindow, as not doing so could lead to a crash */
        CoreWindow ^ native_win = CoreWindow::GetForCurrentThread();
        Microsoft::WRL::ComPtr<IUnknown> cpp_win = reinterpret_cast<IUnknown *>(native_win);
        HRESULT result = CreateWinrtEglWindow(cpp_win, ANGLE_D3D_FEATURE_LEVEL_ANY, &(egl_data->winrt_egl_addon));
        if (FAILED(result)) {
            SDL_SetError("Could not create an EGL-window");
            goto error;
        }

        /* Call eglGetDisplay and eglInitialize as appropriate.  On other
         * platforms, this would probably get done by SDL_EGL_LoadLibrary,
         * however ANGLE/WinRT's current implementation (as of Mar 22, 2014) of
         * eglGetDisplay requires that a C++ object be passed into it, so the
         * call will be made in this file, a C++ file, instead.
         */
        Microsoft::WRL::ComPtr<IUnknown> cpp_display = egl_data->winrt_egl_addon;
        EGLDisplay display = ((eglGetDisplay_Old_Function)egl_data->eglGetDisplay)(cpp_display);
        if (SDL_EGL_InitializeDisplay(egl_data, display) < 0) { // setup Windows 8.0 EGL display
            goto error;
        }
    } else {
        /* Declare some ANGLE/EGL initialization property-sets, as suggested by
         * MSOpenTech's ANGLE-for-WinRT template apps:
         */
        const EGLint defaultDisplayAttributes[] = {
            EGL_PLATFORM_ANGLE_TYPE_ANGLE,
            EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
            EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER,
            EGL_TRUE,
            EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE,
            EGL_TRUE,
            EGL_NONE,
        };

        const EGLint fl9_3DisplayAttributes[] = {
            EGL_PLATFORM_ANGLE_TYPE_ANGLE,
            EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
            EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE,
            9,
            EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE,
            3,
            EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER,
            EGL_TRUE,
            EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE,
            EGL_TRUE,
            EGL_NONE,
        };

        const EGLint warpDisplayAttributes[] = {
            EGL_PLATFORM_ANGLE_TYPE_ANGLE,
            EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
            EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE,
            EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE,
            EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER,
            EGL_TRUE,
            EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE,
            EGL_TRUE,
            EGL_NONE,
        };

        /* 'CreateWinrtEglWindow' was NOT found, which either means that a
         * newer version of ANGLE/WinRT is being used, or that we don't have
         * a valid copy of ANGLE.
         *
         * Try loading ANGLE as if it were the newer version.
         */
        eglGetPlatformDisplayEXT_Function eglGetPlatformDisplayEXT = (eglGetPlatformDisplayEXT_Function)egl_data->eglGetProcAddress("eglGetPlatformDisplayEXT");
        if (!eglGetPlatformDisplayEXT) {
            SDL_EGL_SetError("Could not retrieve ANGLE/WinRT display function(s)", "eglGetPlatformDisplayEXT");
            goto error;
        }

#if !SDL_WINAPI_FAMILY_PHONE
        /* Try initializing EGL at D3D11 Feature Level 10_0+ (which is not
         * supported on WinPhone 8.x.
         */
        EGLDisplay display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, defaultDisplayAttributes);
        if (SDL_EGL_InitializeDisplay(egl_data, display) < 0) { // Could not get/initialize EGL display for Direct3D 10_0+
#else
        {
#endif
            /* Try initializing EGL at D3D11 Feature Level 9_3, in case the
             * 10_0 init fails, or we're on Windows Phone (which only supports
             * 9_3).
             */
            display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, fl9_3DisplayAttributes);
            if (SDL_EGL_InitializeDisplay(egl_data, display) < 0) { // Could not get/initialize EGL display for Direct3D 9_3
                /* Try initializing EGL at D3D11 Feature Level 11_0 on WARP
                 * (a Windows-provided, software rasterizer) if all else fails.
                 */
                display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, warpDisplayAttributes);
                if (SDL_EGL_InitializeDisplay(egl_data, display) < 0) { // Could not get/initialize EGL display for WinRT 8.x+
                    goto error;
                }
            }
            SDL_ClearError();
        }
    }

    return 0;
error:
    WINRT_GLES_UnloadLibrary(_this);
    return -1;
}

extern "C" void
WINRT_GLES_UnloadLibrary(_THIS)
{
    /* Release SDL's own COM reference to the ANGLE/WinRT IWinrtEglWindow */
    if (_this->egl_data->winrt_egl_addon) {
        _this->egl_data->winrt_egl_addon->Release();
        _this->egl_data->winrt_egl_addon = nullptr;
    }

    /* Perform the bulk of the unloading */
    SDL_EGL_UnloadLibrary(_this);
}

extern "C" {
SDL_EGL_CreateContext_impl(WINRT)
    SDL_EGL_SwapWindow_impl(WINRT)
        SDL_EGL_MakeCurrent_impl(WINRT)
}

extern "C" EGLSurface
WINRT_GLES_CreateWindowSurface(_THIS, const SDL_Window *window)
{
    const SDL_WindowData *data = (const SDL_WindowData *)window->driverdata;
    SDL_EGL_VideoData *egl_data;
    EGLSurface surface;
    SDL_COMPILE_TIME_ASSERT(no_surface, EGL_NO_SURFACE == NULL);
    /* Call SDL_EGL_ChooseConfig and eglCreateWindowSurface directly,
     * rather than via SDL_EGL_CreateSurface, as older versions of
     * ANGLE/WinRT may require that a C++ object, ComPtr<IUnknown>,
     * be passed into eglCreateWindowSurface.
     */
    if (SDL_EGL_ChooseConfig(_this) != 0) {
        /* SDL_EGL_ChooseConfig failed, SDL_GetError() should have info */
        return EGL_NO_SURFACE;
    }

    egl_data = _this->egl_data;
    if (egl_data->winrt_egl_addon) { /* ... is the 'old' version of ANGLE/WinRT being used? */
        /* Attempt to create a window surface using older versions of
         * ANGLE/WinRT:
         */
        Microsoft::WRL::ComPtr<IUnknown> cpp_winrtEglWindow = egl_data->winrt_egl_addon;
        surface = ((eglCreateWindowSurface_Old_Function)egl_data->eglCreateWindowSurface)(
            egl_data->egl_display,
            egl_data->egl_config,
            cpp_winrtEglWindow, NULL);
    } else if (data->coreWindow.Get() != nullptr) {
        /* Attempt to create a window surface using newer versions of
         * ANGLE/WinRT:
         */
        IInspectable *coreWindowAsIInspectable = reinterpret_cast<IInspectable *>(data->coreWindow.Get());
        surface = egl_data->eglCreateWindowSurface(
            egl_data->egl_display,
            egl_data->egl_config,
            (NativeWindowType)coreWindowAsIInspectable,
            NULL);
    } else {
        SDL_SetError("No supported means to create an EGL window surface are available");
        return EGL_NO_SURFACE;
    }

    if (surface == EGL_NO_SURFACE) {
        SDL_EGL_SetError("unable to create an EGL window surface", "eglCreateWindowSurface");
    }

    return surface;
}

#endif /* SDL_VIDEO_DRIVER_WINRT && SDL_VIDEO_OPENGL_EGL */

/* vi: set ts=4 sw=4 expandtab: */
