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

#ifdef SDL_VIDEO_DRIVER_WINRT

/* WinRT SDL video driver implementation

   Initial work on this was done by David Ludwig (dludwig@pobox.com), and
   was based off of SDL's "dummy" video driver.
 */

/* Standard C++11 includes */
#include <sstream>
#include <string>

/* Windows includes */
#include <agile.h>
#include <windows.graphics.display.h>
#include <windows.system.display.h>
#include <dxgi.h>
#include <dxgi1_2.h>
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::UI::Core;
using namespace Windows::UI::ViewManagement;

/* [re]declare Windows GUIDs locally, to limit the amount of external lib(s) SDL has to link to */
static const GUID SDL_IID_IDisplayRequest = { 0xe5732044, 0xf49f, 0x4b60, { 0x8d, 0xd4, 0x5e, 0x7e, 0x3a, 0x63, 0x2a, 0xc0 } };
static const GUID SDL_IID_IDXGIFactory2 = { 0x50c83a1c, 0xe072, 0x4c48, { 0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0 } };

/* SDL includes */
extern "C" {
#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"
#include "../../render/SDL_sysrender.h"
#include "SDL_syswm.h"
#include "SDL_winrtopengles.h"
#include "../../core/windows/SDL_windows.h"
}

#include "../../core/winrt/SDL_winrtapp_direct3d.h"
#include "../../core/winrt/SDL_winrtapp_xaml.h"
#include "SDL_winrtvideo_cpp.h"
#include "SDL_winrtvulkan.h"
#include "SDL_winrtevents_c.h"
#include "SDL_winrtgamebar_cpp.h"
#include "SDL_winrtmouse_c.h"
#include "SDL_main.h"
#include "SDL_system.h"
#include "SDL_hints.h"

#ifdef SDL_VIDEO_METAL
#error "Metal is configured, but not implemented for WinRT."
#endif
#if defined(SDL_VIDEO_OPENGL_ANY) && !defined(SDL_VIDEO_OPENGL_EGL)
#error "OpenGL is configured, but not the implemented (EGL) for WinRT."
#endif

/* Initialization/Query functions */
static int WINRT_VideoInit(_THIS);
static int WINRT_InitModes();
static int WINRT_SetDisplayMode(SDL_VideoDisplay *display, SDL_DisplayMode *mode);
static void WINRT_VideoQuit(_THIS);

/* Window functions */
static int WINRT_CreateSDLWindow(_THIS, SDL_Window * window);
static void WINRT_SetWindowSize(SDL_Window * window);
static void WINRT_SetWindowFullscreen(SDL_Window * window, SDL_VideoDisplay * display, SDL_bool fullscreen);
static void WINRT_DestroyWindow(SDL_Window * window);
static SDL_bool WINRT_GetWindowWMInfo(SDL_Window * window, SDL_SysWMinfo * info);


/* Misc functions */
static ABI::Windows::System::Display::IDisplayRequest *WINRT_CreateDisplayRequest();
extern void WINRT_SuspendScreenSaver(_THIS);

/* Instance */
WinRT_VideoData winrtVideoData;
SDL_Window *WINRT_GlobalSDLWindow = NULL;

/* WinRT driver bootstrap functions */

static void WINRT_DeleteDevice(_THIS)
{
    SDL_zero(winrtVideoData);
    SDL_free(_this);
}

static SDL_VideoDevice *WINRT_CreateDevice(void)
{
    SDL_VideoDevice *device;
    WinRT_VideoData *data = &winrtVideoData;

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return NULL;
    }

    /* Set the function pointers */
    /* Initialization/Query functions */
    device->VideoInit = WINRT_VideoInit;
    device->VideoQuit = WINRT_VideoQuit;
    // device->GetDisplayBounds = WINRT_GetDisplayBounds;
    // device->GetDisplayUsableBounds = WINRT_GetDisplayUsableBounds;
    // device->GetDisplayDPI = WINRT_GetDisplayDPI;
    device->SetDisplayMode = WINRT_SetDisplayMode;

    /* Window functions */
    device->CreateSDLWindow = WINRT_CreateSDLWindow;
    // device->CreateSDLWindowFrom = WINRT_CreateSDLWindowFrom;
    // device->SetWindowTitle = WINRT_SetWindowTitle;
    // device->SetWindowIcon = WINRT_SetWindowIcon;
    // device->SetWindowPosition = WINRT_SetWindowPosition;
    device->SetWindowSize = WINRT_SetWindowSize;
    // device->SetWindowMinimumSize = WINRT_SetWindowMinimumSize;
    // device->SetWindowMaximumSize = WINRT_SetWindowMaximumSize;
    // device->GetWindowBordersSize = WINRT_GetWindowBordersSize;
    // device->GetWindowSizeInPixels = WINRT_GetWindowSizeInPixels;
    // device->SetWindowOpacity = WINRT_SetWindowOpacity;
    // device->SetWindowModalFor = WINRT_SetWindowModalFor;
    // device->SetWindowInputFocus = WINRT_SetWindowInputFocus;
    // device->ShowWindow = WINRT_ShowWindow;
    // device->HideWindow = WINRT_HideWindow;
    // device->RaiseWindow = WINRT_RaiseWindow;
    // device->MaximizeWindow = WINRT_MaximizeWindow;
    // device->MinimizeWindow = WINRT_MinimizeWindow;
    // device->RestoreWindow = WINRT_RestoreWindow;
    // device->SetWindowBordered = WINRT_SetWindowBordered;
    // device->SetWindowResizable = WINRT_SetWindowResizable;
    // device->SetWindowAlwaysOnTop = WINRT_SetWindowAlwaysOnTop;
    device->SetWindowFullscreen = WINRT_SetWindowFullscreen;
    // device->SetWindowGammaRamp = WINRT_SetWindowGammaRamp;
    // device->GetWindowGammaRamp = WINRT_GetWindowGammaRamp;
    // device->GetWindowICCProfile = WINRT_GetWindowICCProfile;
    // device->GetWindowDisplayIndex = WINRT_GetWindowDisplayIndex;
    // device->SetWindowMouseRect = WINRT_SetWindowMouseRect;
    // device->SetWindowMouseGrab = WINRT_SetWindowMouseGrab;
    // device->SetWindowKeyboardGrab = WINRT_SetWindowKeyboardGrab;
    device->DestroyWindow = WINRT_DestroyWindow;
    // device->CreateWindowFramebuffer = WINRT_CreateWindowFramebuffer;
    // device->UpdateWindowFramebuffer = WINRT_UpdateWindowFramebuffer;
    // device->DestroyWindowFramebuffer = WINRT_DestroyWindowFramebuffer;
    // device->OnWindowEnter = WINRT_OnWindowEnter;
    // device->FlashWindow = WINRT_FlashWindow;
    /* Shaped-window functions */
    // device->CreateShaper = WINRT_CreateShaper;
    // device->SetWindowShape = WINRT_SetWindowShape;
    /* Get some platform dependent window information */
    device->GetWindowWMInfo = WINRT_GetWindowWMInfo;

    /* OpenGL support */
#ifdef SDL_VIDEO_OPENGL_EGL
    device->GL_LoadLibrary = WINRT_GLES_LoadLibrary;
    device->GL_GetProcAddress = WINRT_GLES_GetProcAddress;
    device->GL_UnloadLibrary = WINRT_GLES_UnloadLibrary;
    device->GL_CreateContext = WINRT_GLES_CreateContext;
    device->GL_MakeCurrent = WINRT_GLES_MakeCurrent;
    // device->GL_GetDrawableSize = WINRT_GLES_GetDrawableSize;
    device->GL_SetSwapInterval = WINRT_GLES_SetSwapInterval;
    device->GL_GetSwapInterval = WINRT_GLES_GetSwapInterval;
    device->GL_SwapWindow = WINRT_GLES_SwapWindow;
    device->GL_DeleteContext = WINRT_GLES_DeleteContext;
    // device->GL_DefaultProfileConfig = WINRT_GLES_DefaultProfileConfig;
#endif

    /* Vulkan support */
#ifdef SDL_VIDEO_VULKAN
    device->Vulkan_LoadLibrary = WINRT_Vulkan_LoadLibrary;
    device->Vulkan_UnloadLibrary = WINRT_Vulkan_UnloadLibrary;
    device->Vulkan_GetInstanceExtensions = WINRT_Vulkan_GetInstanceExtensions;
    device->Vulkan_CreateSurface = WINRT_Vulkan_CreateSurface;
    // device->Vulkan_GetDrawableSize = WINRT_Vulkan_GetDrawableSize;
#endif

    /* Metal support */
#ifdef SDL_VIDEO_METAL
    // device->Metal_CreateView = WINRT_Metal_CreateView;
    // device->Metal_DestroyView = WINRT_Metal_DestroyView;
    // device->Metal_GetLayer = WINRT_Metal_GetLayer;
    // device->Metal_GetDrawableSize = WINRT_Metal_GetDrawableSize;
#endif

    /* Event manager functions */
    // device->WaitEventTimeout = WINRT_WaitEventTimeout;
    // device->SendWakeupEvent = WINRT_SendWakeupEvent;
    device->PumpEvents = WINRT_PumpEvents;

    /* Screensaver */
    device->SuspendScreenSaver = WINRT_SuspendScreenSaver;

    /* Text input */
    // device->StartTextInput = WINRT_StartTextInput;
    // device->StopTextInput = WINRT_StopTextInput;
    // device->SetTextInputRect = WINRT_SetTextInputRect;
    // device->ClearComposition = WINRT_ClearComposition;
    // device->IsTextInputShown = WINRT_IsTextInputShown;

    /* Screen keyboard */
#if NTDDI_VERSION >= NTDDI_WIN10
    device->HasScreenKeyboardSupport = WINRT_HasScreenKeyboardSupport;
    device->ShowScreenKeyboard = WINRT_ShowScreenKeyboard;
    device->HideScreenKeyboard = WINRT_HideScreenKeyboard;
    device->IsScreenKeyboardShown = WINRT_IsScreenKeyboardShown;

    WINTRT_InitialiseInputPaneEvents();
#endif

    /* Clipboard */
    // device->SetClipboardText = WINRT_SetClipboardText;
    // device->GetClipboardText = WINRT_GetClipboardText;
    // device->HasClipboardText = WINRT_HasClipboardText;
    // device->SetPrimarySelectionText = WINRT_SetPrimarySelectionText;
    // device->GetPrimarySelectionText = WINRT_GetPrimarySelectionText;
    // device->HasPrimarySelectionText = WINRT_HasPrimarySelectionText;

    /* Hit-testing */
    // device->SetWindowHitTest = WINRT_SetWindowHitTest;

    /* Tell window that app enabled drag'n'drop events */
    // device->AcceptDragAndDrop = WINRT_AcceptDragAndDrop;

    device->free = WINRT_DeleteDevice;

    return device;
}
/* "SDL WinRT video driver" */
const VideoBootStrap WINRT_bootstrap = {
    "winrt", WINRT_CreateDevice
};

static void SDLCALL WINRT_SetDisplayOrientationsPreference(void *userdata, const char *name, const char *oldValue, const char *newValue)
{
    SDL_assert(SDL_strcmp(name, SDL_HINT_ORIENTATIONS) == 0);

    /* HACK: prevent SDL from altering an app's .appxmanifest-set orientation
     * from being changed on startup, by detecting when SDL_HINT_ORIENTATIONS
     * is getting registered.
     *
     * TODO, WinRT: consider reading in an app's .appxmanifest file, and apply its orientation when 'newValue == NULL'.
     */
    if ((!oldValue) && (!newValue)) {
        return;
    }

    // Start with no orientation flags, then add each in as they're parsed
    // from newValue.
    unsigned int orientationFlags = 0;
    if (newValue) {
        std::istringstream tokenizer(newValue);
        while (!tokenizer.eof()) {
            std::string orientationName;
            std::getline(tokenizer, orientationName, ' ');
            if (orientationName == "LandscapeLeft") {
                orientationFlags |= (unsigned int)DisplayOrientations::LandscapeFlipped;
            } else if (orientationName == "LandscapeRight") {
                orientationFlags |= (unsigned int)DisplayOrientations::Landscape;
            } else if (orientationName == "Portrait") {
                orientationFlags |= (unsigned int)DisplayOrientations::Portrait;
            } else if (orientationName == "PortraitUpsideDown") {
                orientationFlags |= (unsigned int)DisplayOrientations::PortraitFlipped;
            }
        }
    }

    // If no valid orientation flags were specified, use a reasonable set of defaults:
    if (!orientationFlags) {
        // TODO, WinRT: consider seeing if an app's default orientation flags can be found out via some API call(s).
        orientationFlags = (unsigned int)(DisplayOrientations::Landscape |
                                          DisplayOrientations::LandscapeFlipped |
                                          DisplayOrientations::Portrait |
                                          DisplayOrientations::PortraitFlipped);
    }

    // Set the orientation/rotation preferences.  Please note that this does
    // not constitute a 100%-certain lock of a given set of possible
    // orientations.  According to Microsoft's documentation on WinRT [1]
    // when a device is not capable of being rotated, Windows may ignore
    // the orientation preferences, and stick to what the device is capable of
    // displaying.
    //
    // [1] Documentation on the 'InitialRotationPreference' setting for a
    // Windows app's manifest file describes how some orientation/rotation
    // preferences may be ignored.  See
    // http://msdn.microsoft.com/en-us/library/windows/apps/hh700343.aspx
    // for details.  Microsoft's "Display orientation sample" also gives an
    // outline of how Windows treats device rotation
    // (http://code.msdn.microsoft.com/Display-Orientation-Sample-19a58e93).
    WINRT_DISPLAY_PROPERTY(AutoRotationPreferences) = (DisplayOrientations)orientationFlags;
}

int WINRT_VideoInit(_THIS)
{
    WinRT_VideoData *driverdata = &winrtVideoData;
    if (WINRT_InitModes() < 0) {
        return -1;
    }

    // Register the hint, SDL_HINT_ORIENTATIONS, with SDL.
    // TODO, WinRT: see if an app's default orientation can be found out via WinRT API(s), then set the initial value of SDL_HINT_ORIENTATIONS accordingly.
    SDL_AddHintCallback(SDL_HINT_ORIENTATIONS, WINRT_SetDisplayOrientationsPreference, NULL);

    WINRT_InitMouse();
    WINRT_InitTouch();
    WINRT_InitGameBar();
    if (driverdata) {
        /* Initialize screensaver-disabling support */
        driverdata->displayRequest = WINRT_CreateDisplayRequest();
    }
    return 0;
}

extern "C" Uint32 D3D11_DXGIFormatToSDLPixelFormat(DXGI_FORMAT dxgiFormat);

static void WINRT_DXGIModeToSDLDisplayMode(const DXGI_MODE_DESC *dxgiMode, SDL_DisplayMode *sdlMode)
{
    sdlMode->format = D3D11_DXGIFormatToSDLPixelFormat(dxgiMode->Format);
    sdlMode->refresh_rate = dxgiMode->RefreshRate.Numerator / dxgiMode->RefreshRate.Denominator;
    sdlMode->w = dxgiMode->Width;
    sdlMode->h = dxgiMode->Height;
    sdlMode->driverdata = NULL;
}

static int WINRT_AddDisplaysForOutput(IDXGIAdapter1 *dxgiAdapter1, int outputIndex)
{
    HRESULT hr;
    IDXGIOutput *dxgiOutput = NULL;
    DXGI_OUTPUT_DESC dxgiOutputDesc;
    SDL_VideoDisplay display;
    char *displayName = NULL;
    int functionResult = -1; /* -1 for failure, 0 for success */
    DXGI_MODE_DESC modeToMatch, closestMatch;

    SDL_zero(display);

    hr = dxgiAdapter1->EnumOutputs(outputIndex, &dxgiOutput);
    if (FAILED(hr)) {
        if (hr != DXGI_ERROR_NOT_FOUND) {
            WIN_SetErrorFromHRESULT(__FUNCTION__ ", IDXGIAdapter1::EnumOutputs failed", hr);
        }
        goto done;
    }

    hr = dxgiOutput->GetDesc(&dxgiOutputDesc);
    if (FAILED(hr)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", IDXGIOutput::GetDesc failed", hr);
        goto done;
    }

    SDL_zero(modeToMatch);
    modeToMatch.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    modeToMatch.Width = (dxgiOutputDesc.DesktopCoordinates.right - dxgiOutputDesc.DesktopCoordinates.left);
    modeToMatch.Height = (dxgiOutputDesc.DesktopCoordinates.bottom - dxgiOutputDesc.DesktopCoordinates.top);
    hr = dxgiOutput->FindClosestMatchingMode(&modeToMatch, &closestMatch, NULL);
    if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE) {
        /* DXGI_ERROR_NOT_CURRENTLY_AVAILABLE gets returned by IDXGIOutput::FindClosestMatchingMode
           when running under the Windows Simulator, which uses Remote Desktop (formerly known as Terminal
           Services) under the hood.  According to the MSDN docs for the similar function,
           IDXGIOutput::GetDisplayModeList, DXGI_ERROR_NOT_CURRENTLY_AVAILABLE is returned if and
           when an app is run under a Terminal Services session, hence the assumption.

           In this case, just add an SDL display mode, with approximated values.
        */
        SDL_DisplayMode mode;
        display.name = "Windows Simulator / Terminal Services Display";
        mode.format = DXGI_FORMAT_B8G8R8A8_UNORM;
        mode.w = (dxgiOutputDesc.DesktopCoordinates.right - dxgiOutputDesc.DesktopCoordinates.left);
        mode.h = (dxgiOutputDesc.DesktopCoordinates.bottom - dxgiOutputDesc.DesktopCoordinates.top);
        mode.refresh_rate = 0; /* Display mode is unknown, so just fill in zero, as specified by SDL's header files */
        mode.driverdata = NULL;
        display.desktop_mode = mode;
        display.current_mode = mode;
        SDL_AddDisplayMode(&display, &mode);
    } else if (FAILED(hr)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", IDXGIOutput::FindClosestMatchingMode failed", hr);
        goto done;
    } else {
        UINT numModes;
        DXGI_MODE_DESC *dxgiModes = NULL;
        SDL_DisplayMode mode;
        displayName = WIN_StringToUTF8(dxgiOutputDesc.DeviceName);
        display.name = displayName;
        WINRT_DXGIModeToSDLDisplayMode(&closestMatch, &mode);
        display.desktop_mode = mode;
        display.current_mode = mode;
        SDL_AddDisplayMode(&display, &mode);

        while (1) {
            hr = dxgiOutput->GetDisplayModeList(DXGI_FORMAT_B8G8R8A8_UNORM, 0, &numModes, dxgiModes);
            if (SUCCEEDED(hr)) {
                if (dxgiModes == NULL) {
                    dxgiModes = (DXGI_MODE_DESC *)SDL_calloc(numModes, sizeof(DXGI_MODE_DESC));
                    if (!dxgiModes) {
                        SDL_OutOfMemory();
                        goto done;
                    }
                    continue;
                }
                for (UINT i = 0; i < numModes; ++i) {
                    SDL_DisplayMode sdlMode;
                    WINRT_DXGIModeToSDLDisplayMode(&dxgiModes[i], &sdlMode);
                    SDL_AddDisplayMode(&display, &sdlMode);
                }
                SDL_free(dxgiModes);
                break;
            }
            SDL_free(dxgiModes);
            if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE) {
                /* DXGI_ERROR_NOT_CURRENTLY_AVAILABLE gets returned by IDXGIOutput::GetDisplayModeList
                   when using Terminal Services / Windows Simulator

                   In this case, just stick to the previous (closest matched mode).
                */
                break;
            }
            if (hr == DXGI_ERROR_MORE_DATA) {
                /* Rare case the display modes available can change immediately after calling this method
                   (there is not enough room for all the display modes)

                   In this case, try again.
                */
                dxgiModes = NULL;
                continue;
            }
            WIN_SetErrorFromHRESULT(__FUNCTION__ ", IDXGIOutput::GetDisplayModeList failed", hr);
            goto done;
        }
    }

    functionResult = SDL_AddVideoDisplay(&display, SDL_FALSE);
    if (functionResult < 0) {
        SDL_PrivateResetDisplayModes(&display);
    }

done:
    if (dxgiOutput) {
        dxgiOutput->Release();
    }
    if (displayName) {
        SDL_free(displayName);
    }
    return functionResult;
}

static int WINRT_AddDisplaysForAdapter(IDXGIFactory2 *dxgiFactory2, int adapterIndex)
{
    HRESULT hr;
    IDXGIAdapter1 *dxgiAdapter1;

    hr = dxgiFactory2->EnumAdapters1(adapterIndex, &dxgiAdapter1);
    if (FAILED(hr)) {
        if (hr != DXGI_ERROR_NOT_FOUND) {
            WIN_SetErrorFromHRESULT(__FUNCTION__ ", IDXGIFactory1::EnumAdapters1() failed", hr);
        }
        return -1;
    }

    for (int outputIndex = 0;; ++outputIndex) {
        if (WINRT_AddDisplaysForOutput(dxgiAdapter1, outputIndex) < 0) {
            /* HACK: The Windows App Certification Kit 10.0 can fail, when
               running the Store Apps' test, "Direct3D Feature Test".  The
               certification kit's error is:

               "Application App was not running at the end of the test. It likely crashed or was terminated for having become unresponsive."

               This was caused by SDL/WinRT's DXGI failing to report any
               outputs.  Attempts to get the 1st display-output from the
               1st display-adapter can fail, with IDXGIAdapter::EnumOutputs
               returning DXGI_ERROR_NOT_FOUND.  This could be a bug in Windows,
               the Windows App Certification Kit, or possibly in SDL/WinRT's
               display detection code.  Either way, try to detect when this
               happens, and use a hackish means to create a reasonable-as-possible
               'display mode'.  -- DavidL
            */
            if (adapterIndex == 0 && outputIndex == 0) {
                SDL_VideoDisplay display;
                SDL_DisplayMode mode;
#ifdef SDL_WINRT_USE_APPLICATIONVIEW
                ApplicationView ^ appView = ApplicationView::GetForCurrentView();
#endif
                CoreWindow ^ coreWin = CoreWindow::GetForCurrentThread();
                SDL_zero(display);
                display.name = "DXGI Display-detection Workaround";

                /* HACK: ApplicationView's VisibleBounds property, appeared, via testing, to
                   give a better approximation of display-size, than did CoreWindow's
                   Bounds property, insofar that ApplicationView::VisibleBounds seems like
                   it will, at least some of the time, give the full display size (during the
                   failing test), whereas CoreWindow might not.  -- DavidL
                */

#if (NTDDI_VERSION >= NTDDI_WIN10) || (defined(SDL_WINRT_USE_APPLICATIONVIEW) && SDL_WINAPI_FAMILY_PHONE)
                mode.w = WINRT_DIPS_TO_PHYSICAL_PIXELS(appView->VisibleBounds.Width);
                mode.h = WINRT_DIPS_TO_PHYSICAL_PIXELS(appView->VisibleBounds.Height);
#else
                /* On platform(s) that do not support VisibleBounds, such as Windows 8.1,
                   fall back to CoreWindow's Bounds property.
                */
                mode.w = WINRT_DIPS_TO_PHYSICAL_PIXELS(coreWin->Bounds.Width);
                mode.h = WINRT_DIPS_TO_PHYSICAL_PIXELS(coreWin->Bounds.Height);
#endif

                mode.format = DXGI_FORMAT_B8G8R8A8_UNORM;
                mode.refresh_rate = 0; /* Display mode is unknown, so just fill in zero, as specified by SDL's header files */
                mode.driverdata = NULL;
                display.desktop_mode = mode;
                display.current_mode = mode;
                SDL_AddDisplayMode(&display, &mode);
                if (SDL_AddVideoDisplay(&display, SDL_FALSE) < 0) {
                    SDL_PrivateResetDisplayModes(&display);
                    return SDL_SetError("Failed to apply DXGI Display-detection workaround");
                }
            }

            break;
        }
    }

    dxgiAdapter1->Release();
    return 0;
}

static int WINRT_InitModes()
{
    /* HACK: Initialize a single display, for whatever screen the app's
         CoreApplicationView is on.
       TODO, WinRT: Try initializing multiple displays, one for each monitor.
         Appropriate WinRT APIs for this seem elusive, though.  -- DavidL
    */

    HRESULT hr;
    IDXGIFactory2 *dxgiFactory2 = NULL;

    hr = CreateDXGIFactory1(SDL_IID_IDXGIFactory2, (void **)&dxgiFactory2);
    if (FAILED(hr)) {
        return WIN_SetErrorFromHRESULT(__FUNCTION__ ", CreateDXGIFactory1() failed", hr);
    }

    for (int adapterIndex = 0;; ++adapterIndex) {
        if (WINRT_AddDisplaysForAdapter(dxgiFactory2, adapterIndex) < 0) {
            break;
        }
    }

    return 0;
}

static int WINRT_SetDisplayMode(SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    return 0;
}

void WINRT_VideoQuit(_THIS)
{
    WinRT_VideoData *driverdata = &winrtVideoData;
    if (driverdata->displayRequest) {
        driverdata->displayRequest->Release();
        driverdata->displayRequest = NULL;
    }
    WINRT_QuitGameBar();
    WINRT_QuitMouse();
}

static const Uint32 WINRT_DetectableFlags = SDL_WINDOW_MAXIMIZED | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN | SDL_WINDOW_HIDDEN | SDL_WINDOW_MOUSE_FOCUS;

extern "C" Uint32
WINRT_DetectWindowFlags(SDL_Window *window)
{
    Uint32 latestFlags = 0;
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    bool is_fullscreen = false;

#ifdef SDL_WINRT_USE_APPLICATIONVIEW
    if (data->appView) {
        is_fullscreen = data->appView->IsFullScreenMode;
    }
#elif SDL_WINAPI_FAMILY_PHONE || (NTDDI_VERSION == NTDDI_WIN8)
    is_fullscreen = true;
#endif

    if (data->coreWindow.Get()) {
        if (is_fullscreen) {
            SDL_VideoDisplay *display = SDL_GetDisplayForWindow(window);
            int w = WINRT_DIPS_TO_PHYSICAL_PIXELS(data->coreWindow->Bounds.Width);
            int h = WINRT_DIPS_TO_PHYSICAL_PIXELS(data->coreWindow->Bounds.Height);

#if !SDL_WINAPI_FAMILY_PHONE || (NTDDI_VERSION > NTDDI_WIN8)
            // On all WinRT platforms, except for WinPhone 8.0, rotate the
            // window size.  This is needed to properly calculate
            // fullscreen vs. maximized.
            const DisplayOrientations currentOrientation = WINRT_DISPLAY_PROPERTY(CurrentOrientation);
            switch (currentOrientation) {
#if SDL_WINAPI_FAMILY_PHONE
            case DisplayOrientations::Landscape:
            case DisplayOrientations::LandscapeFlipped:
#else
            case DisplayOrientations::Portrait:
            case DisplayOrientations::PortraitFlipped:
#endif
            {
                int tmp = w;
                w = h;
                h = tmp;
            } break;
            }
#endif

            if (display->desktop_mode.w != w || display->desktop_mode.h != h) {
                latestFlags |= SDL_WINDOW_MAXIMIZED;
            } else {
                latestFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
            }
        }

        if (data->coreWindow->Visible) {
            latestFlags |= SDL_WINDOW_SHOWN;
        } else {
            latestFlags |= SDL_WINDOW_HIDDEN;
        }

#if SDL_WINAPI_FAMILY_PHONE && (NTDDI_VERSION < NTDDI_WINBLUE)
        // data->coreWindow->PointerPosition is not supported on WinPhone 8.0
        latestFlags |= SDL_WINDOW_MOUSE_FOCUS;
#else
        if (data->coreWindow->Visible && data->coreWindow->Bounds.Contains(data->coreWindow->PointerPosition)) {
            latestFlags |= SDL_WINDOW_MOUSE_FOCUS;
        }
#endif
    }

    return latestFlags;
}

// TODO, WinRT: consider removing WINRT_UpdateWindowFlags, and just calling WINRT_DetectWindowFlags as-appropriate (with appropriate calls to SDL_SendWindowEvent)
void WINRT_UpdateWindowFlags(SDL_Window *window, Uint32 mask)
{
    mask &= WINRT_DetectableFlags;
    if (window) {
        Uint32 apply = WINRT_DetectWindowFlags(window);
        if ((apply & mask) & SDL_WINDOW_FULLSCREEN) {
            window->last_fullscreen_flags = window->flags; // seems necessary to programmatically un-fullscreen, via SDL APIs
        }
        window->flags = (window->flags & ~mask) | (apply & mask);
    }
}

static bool WINRT_IsCoreWindowActive(CoreWindow ^ coreWindow)
{
    /* WinRT does not appear to offer API(s) to determine window-activation state,
       at least not that I am aware of in Win8 - Win10.  As such, SDL tracks this
       itself, via window-activation events.

       If there *is* an API to track this, it should probably get used instead
       of the following hack (that uses "SDLHelperWindowActivationState").
         -- DavidL.
    */
    if (coreWindow->CustomProperties->HasKey("SDLHelperWindowActivationState")) {
        CoreWindowActivationState activationState =
            safe_cast<CoreWindowActivationState>(coreWindow->CustomProperties->Lookup("SDLHelperWindowActivationState"));
        return activationState != CoreWindowActivationState::Deactivated;
    }

    /* Assume that non-SDL tracked windows are active, although this should
       probably be avoided, if possible.

       This might not even be possible, in normal SDL use, at least as of
       this writing (Dec 22, 2015; via latest hg.libsdl.org/SDL clone)  -- DavidL
    */
    return true;
}

int WINRT_CreateSDLWindow(_THIS, SDL_Window *window)
{
    // Make sure that only one window gets created, at least until multimonitor
    // support is added.
    if (WINRT_GlobalSDLWindow != NULL) {
        return SDL_SetError("WinRT only supports one window");
    }

    SDL_WindowData *data = new SDL_WindowData; /* use 'new' here as SDL_WindowData may use WinRT/C++ types */
    if (!data) {
        return SDL_OutOfMemory();
    }
    window->driverdata = data;
    data->sdlWindow = window;

    /* To note, when XAML support is enabled, access to the CoreWindow will not
       be possible, at least not via the SDL/XAML thread.  Attempts to access it
       from there will throw exceptions.  As such, the SDL_WindowData's
       'coreWindow' field will only be set (to a non-null value) if XAML isn't
       enabled.
    */
    if (!WINRT_XAMLWasEnabled) {
        data->coreWindow = CoreWindow::GetForCurrentThread();
#ifdef SDL_WINRT_USE_APPLICATIONVIEW
        data->appView = ApplicationView::GetForCurrentView();
#endif
    }

#ifdef SDL_VIDEO_OPENGL_EGL
    /* Setup the EGL surface, but only if OpenGL ES 2 was requested. */
    if (!(window->flags & SDL_WINDOW_OPENGL)) {
        /* OpenGL ES 2 wasn't requested.  Don't set up an EGL surface. */
        data->egl_surface = EGL_NO_SURFACE;
    } else {
        /* OpenGL ES 2 was requested.  Set up an EGL surface. */
        data->egl_surface = WINRT_GLES_CreateWindowSurface(_this, window);
        if (data->egl_surface == EGL_NO_SURFACE) {
            return -1;
        }
    }
#endif

    /* Make note of the requested window flags, before they start getting changed. */
    const Uint32 requestedFlags = window->flags;

    /* Determine as many flags dynamically, as possible. */
    window->flags =
        SDL_WINDOW_BORDERLESS |
        SDL_WINDOW_RESIZABLE;

#ifdef SDL_VIDEO_OPENGL_EGL
    if (data->egl_surface != EGL_NO_SURFACE) {
        window->flags |= SDL_WINDOW_OPENGL;
    }
#endif

    if (WINRT_XAMLWasEnabled) {
        /* TODO, WinRT: set SDL_Window size, maybe position too, from XAML control */
        window->x = 0;
        window->y = 0;
        window->flags |= SDL_WINDOW_SHOWN;
        SDL_SetMouseFocus(NULL);    // TODO: detect this
        SDL_SetKeyboardFocus(NULL); // TODO: detect this
    } else {
        /* WinRT 8.x apps seem to live in an environment where the OS controls the
           app's window size, with some apps being fullscreen, depending on
           user choice of various things.  For now, just adapt the SDL_Window to
           whatever Windows set-up as the native-window's geometry.
        */
        window->x = WINRT_DIPS_TO_PHYSICAL_PIXELS(data->coreWindow->Bounds.Left);
        window->y = WINRT_DIPS_TO_PHYSICAL_PIXELS(data->coreWindow->Bounds.Top);
#if NTDDI_VERSION < NTDDI_WIN10
        /* On WinRT 8.x / pre-Win10, just use the size we were given. */
        window->w = WINRT_DIPS_TO_PHYSICAL_PIXELS(data->coreWindow->Bounds.Width);
        window->h = WINRT_DIPS_TO_PHYSICAL_PIXELS(data->coreWindow->Bounds.Height);
#else
        /* On Windows 10, we occasionally get control over window size.  For windowed
           mode apps, try this.
        */
        bool didSetSize = false;
        if (!(requestedFlags & SDL_WINDOW_FULLSCREEN)) {
            const Windows::Foundation::Size size(WINRT_PHYSICAL_PIXELS_TO_DIPS(window->w),
                                                 WINRT_PHYSICAL_PIXELS_TO_DIPS(window->h));
            didSetSize = data->appView->TryResizeView(size);
        }
        if (!didSetSize) {
            /* We either weren't able to set the window size, or a request for
               fullscreen was made.  Get window-size info from the OS.
            */
            window->w = WINRT_DIPS_TO_PHYSICAL_PIXELS(data->coreWindow->Bounds.Width);
            window->h = WINRT_DIPS_TO_PHYSICAL_PIXELS(data->coreWindow->Bounds.Height);
        }
#endif

        WINRT_UpdateWindowFlags(
            window,
            0xffffffff /* Update any window flag(s) that WINRT_UpdateWindow can handle */
        );

        /* Try detecting if the window is active */
        bool isWindowActive = WINRT_IsCoreWindowActive(data->coreWindow.Get());
        if (isWindowActive) {
            SDL_SetKeyboardFocus(window);
        }
    }

    /* Make sure the WinRT app's IFramworkView can post events on
       behalf of SDL:
    */
    WINRT_GlobalSDLWindow = window;

    /* All done! */
    return 0;
}

void WINRT_SetWindowSize(SDL_Window *window)
{
#if NTDDI_VERSION >= NTDDI_WIN10
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    const Windows::Foundation::Size size(WINRT_PHYSICAL_PIXELS_TO_DIPS(window->w),
                                         WINRT_PHYSICAL_PIXELS_TO_DIPS(window->h));
    data->appView->TryResizeView(size); // TODO, WinRT: return failure (to caller?) from TryResizeView()
#endif
}

void WINRT_SetWindowFullscreen(SDL_Window *window, SDL_VideoDisplay *display, SDL_bool fullscreen)
{
#if NTDDI_VERSION >= NTDDI_WIN10
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    bool isWindowActive = WINRT_IsCoreWindowActive(data->coreWindow.Get());
    if (isWindowActive) {
        if (fullscreen) {
            if (!data->appView->IsFullScreenMode) {
                data->appView->TryEnterFullScreenMode(); // TODO, WinRT: return failure (to caller?) from TryEnterFullScreenMode()
            }
        } else {
            if (data->appView->IsFullScreenMode) {
                data->appView->ExitFullScreenMode();
            }
        }
    }
#endif
}

void WINRT_DestroyWindow(SDL_Window *window)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;

    if (WINRT_GlobalSDLWindow == window) {
        WINRT_GlobalSDLWindow = NULL;
    }

    if (data) {
#ifdef SDL_VIDEO_OPENGL_EGL
        SDL_EGL_DestroySurface(data->egl_surface);
#endif
        // Delete the internal window data:
        delete data;
        data = NULL;
        window->driverdata = NULL;
    }
}

SDL_bool WINRT_GetWindowWMInfo(SDL_Window * window, SDL_SysWMinfo * info)
{
    SDL_WindowData * data = (SDL_WindowData *) window->driverdata;

    if (info->version.major <= SDL_MAJOR_VERSION) {
        info->subsystem = SDL_SYSWM_WINRT;
        info->info.winrt.window = reinterpret_cast<IInspectable *>(data->coreWindow.Get());
        return SDL_TRUE;
    } else {
        SDL_SetError("Application not compiled with SDL %d",
                     SDL_MAJOR_VERSION);
        return SDL_FALSE;
    }
    return SDL_FALSE;
}

static ABI::Windows::System::Display::IDisplayRequest *WINRT_CreateDisplayRequest()
{
    /* Setup a WinRT DisplayRequest object, usable for enabling/disabling screensaver requests */
    wchar_t *wClassName = L"Windows.System.Display.DisplayRequest";
    HSTRING hClassName;
    IActivationFactory *pActivationFactory = NULL;
    IInspectable *pDisplayRequestRaw = nullptr;
    ABI::Windows::System::Display::IDisplayRequest *pDisplayRequest = nullptr;
    HRESULT hr;

    hr = ::WindowsCreateString(wClassName, (UINT32)SDL_wcslen(wClassName), &hClassName);
    if (FAILED(hr)) {
        goto done;
    }

    hr = Windows::Foundation::GetActivationFactory(hClassName, &pActivationFactory);
    if (FAILED(hr)) {
        goto done;
    }

    hr = pActivationFactory->ActivateInstance(&pDisplayRequestRaw);
    if (FAILED(hr)) {
        goto done;
    }

    hr = pDisplayRequestRaw->QueryInterface(SDL_IID_IDisplayRequest, (void **)&pDisplayRequest);
    if (FAILED(hr)) {
        goto done;
    }

done:
    if (pDisplayRequestRaw) {
        pDisplayRequestRaw->Release();
    }
    if (pActivationFactory) {
        pActivationFactory->Release();
    }
    if (hClassName) {
        ::WindowsDeleteString(hClassName);
    }

    return pDisplayRequest;
}

void WINRT_SuspendScreenSaver(_THIS)
{
    WinRT_VideoData *driverdata = &winrtVideoData;
    if (driverdata->displayRequest) {
        ABI::Windows::System::Display::IDisplayRequest *displayRequest = (ABI::Windows::System::Display::IDisplayRequest *)driverdata->displayRequest;
        if (_this->suspend_screensaver) {
            displayRequest->RequestActive();
        } else {
            displayRequest->RequestRelease();
        }
    }
}

#endif /* SDL_VIDEO_DRIVER_WINRT */

/* vi: set ts=4 sw=4 expandtab: */
