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

#ifdef SDL_VIDEO_DRIVER_RISCOS

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include "SDL_riscosvideo.h"
#include "SDL_riscosevents_c.h"
#include "SDL_riscosframebuffer_c.h"
#include "SDL_riscosmouse.h"
#include "SDL_riscosmodes.h"
#include "SDL_riscoswindow.h"

#ifdef SDL_VIDEO_VULKAN
#error "Vulkan is configured, but not implemented for RiscOS."
#endif
#ifdef SDL_VIDEO_METAL
#error "Metal is configured, but not implemented for RiscOS."
#endif
#ifdef SDL_VIDEO_OPENGL_ANY
#error "OpenGL is configured, but not implemented for RiscOS."
#endif

/* Instance */
RiscOS_VideoData riscosVideoData;

/* Initialization/Query functions */
static int RISCOS_VideoInit(_THIS);
static void RISCOS_VideoQuit(_THIS);

/* RISC OS driver bootstrap functions */

static void RISCOS_DeleteDevice(_THIS)
{
    SDL_zero(riscosVideoData);
    SDL_free(_this);
}

static SDL_VideoDevice *RISCOS_CreateDevice(void)
{
    SDL_VideoDevice *device;
    RiscOS_VideoData *phdata = &riscosVideoData;

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return NULL;
    }

    /* Set the function pointers */
    /* Initialization/Query functions */
    device->VideoInit = RISCOS_VideoInit;
    device->VideoQuit = RISCOS_VideoQuit;
    // device->GetDisplayBounds = RISCOS_GetDisplayBounds;
    // device->GetDisplayUsableBounds = RISCOS_GetDisplayUsableBounds;
    // device->GetDisplayDPI = RISCOS_GetDisplayDPI;
    device->SetDisplayMode = RISCOS_SetDisplayMode;

    /* Window functions */
    device->CreateSDLWindow = RISCOS_CreateSDLWindow;
    // device->CreateSDLWindowFrom = RISCOS_CreateSDLWindowFrom;
    // device->SetWindowTitle = RISCOS_SetWindowTitle;
    // device->SetWindowIcon = RISCOS_SetWindowIcon;
    // device->SetWindowPosition = RISCOS_SetWindowPosition;
    // device->SetWindowSize = RISCOS_SetWindowSize;
    // device->SetWindowMinimumSize = RISCOS_SetWindowMinimumSize;
    // device->SetWindowMaximumSize = RISCOS_SetWindowMaximumSize;
    // device->GetWindowBordersSize = RISCOS_GetWindowBordersSize;
    // device->GetWindowSizeInPixels = RISCOS_GetWindowSizeInPixels;
    // device->SetWindowOpacity = RISCOS_SetWindowOpacity;
    // device->SetWindowModalFor = RISCOS_SetWindowModalFor;
    // device->SetWindowInputFocus = RISCOS_SetWindowInputFocus;
    // device->ShowWindow = RISCOS_ShowWindow;
    // device->HideWindow = RISCOS_HideWindow;
    // device->RaiseWindow = RISCOS_RaiseWindow;
    // device->MaximizeWindow = RISCOS_MaximizeWindow;
    // device->MinimizeWindow = RISCOS_MinimizeWindow;
    // device->RestoreWindow = RISCOS_RestoreWindow;
    // device->SetWindowBordered = RISCOS_SetWindowBordered;
    // device->SetWindowResizable = RISCOS_SetWindowResizable;
    // device->SetWindowAlwaysOnTop = RISCOS_SetWindowAlwaysOnTop;
    // device->SetWindowFullscreen = RISCOS_SetWindowFullscreen;
    // device->SetWindowGammaRamp = RISCOS_SetWindowGammaRamp;
    // device->GetWindowGammaRamp = RISCOS_GetWindowGammaRamp;
    // device->GetWindowICCProfile = RISCOS_GetWindowICCProfile;
    // device->GetWindowDisplayIndex = RISCOS_GetWindowDisplayIndex;
    // device->SetWindowMouseRect = RISCOS_SetWindowMouseRect;
    // device->SetWindowMouseGrab = RISCOS_SetWindowMouseGrab;
    // device->SetWindowKeyboardGrab = RISCOS_SetWindowKeyboardGrab;
    device->DestroyWindow = RISCOS_DestroyWindow;
    device->CreateWindowFramebuffer = RISCOS_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = RISCOS_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = RISCOS_DestroyWindowFramebuffer;
    // device->OnWindowEnter = RISCOS_OnWindowEnter;
    // device->FlashWindow = RISCOS_FlashWindow;
    /* Shaped-window functions */
    // device->CreateShaper = RISCOS_CreateShaper;
    // device->SetWindowShape = RISCOS_SetWindowShape;
    /* Get some platform dependent window information */
    device->GetWindowWMInfo = RISCOS_GetWindowWMInfo;

    /* OpenGL support */
#ifdef SDL_VIDEO_OPENGL_EGL
    // device->GL_LoadLibrary = RISCOS_GL_LoadLibrary;
    // device->GL_GetProcAddress = RISCOS_GL_GetProcAddress;
    // device->GL_UnloadLibrary = RISCOS_GL_UnloadLibrary;
    // device->GL_CreateContext = RISCOS_GL_CreateContext;
    // device->GL_MakeCurrent = RISCOS_GL_MakeCurrent;
    // device->GL_GetDrawableSize = RISCOS_GL_GetDrawableSize;
    // device->GL_SetSwapInterval = RISCOS_GL_SetSwapInterval;
    // device->GL_GetSwapInterval = RISCOS_GL_GetSwapInterval;
    // device->GL_SwapWindow = RISCOS_GL_SwapWindow;
    // device->GL_DeleteContext = RISCOS_GL_DeleteContext;
    // device->GL_DefaultProfileConfig = RISCOS_GL_DefaultProfileConfig;
#endif

    /* Vulkan support */
#ifdef SDL_VIDEO_VULKAN
    // device->Vulkan_LoadLibrary = RISCOS_Vulkan_LoadLibrary;
    // device->Vulkan_UnloadLibrary = RISCOS_Vulkan_UnloadLibrary;
    // device->Vulkan_GetInstanceExtensions = RISCOS_Vulkan_GetInstanceExtensions;
    // device->Vulkan_CreateSurface = RISCOS_Vulkan_CreateSurface;
    // device->Vulkan_GetDrawableSize = RISCOS_Vulkan_GetDrawableSize;
#endif

    /* Metal support */
#ifdef SDL_VIDEO_METAL
    // device->Metal_CreateView = RISCOS_Metal_CreateView;
    // device->Metal_DestroyView = RISCOS_Metal_DestroyView;
    // device->Metal_GetLayer = RISCOS_Metal_GetLayer;
    // device->Metal_GetDrawableSize = RISCOS_Metal_GetDrawableSize;
#endif

    /* Event manager functions */
    // device->WaitEventTimeout = RISCOS_WaitEventTimeout;
    // device->SendWakeupEvent = RISCOS_SendWakeupEvent;
    device->PumpEvents = RISCOS_PumpEvents;

    /* Screensaver */
    // device->SuspendScreenSaver = RISCOS_SuspendScreenSaver;

    /* Text input */
    // device->StartTextInput = RISCOS_StartTextInput;
    // device->StopTextInput = RISCOS_StopTextInput;
    // device->SetTextInputRect = RISCOS_SetTextInputRect;
    // device->ClearComposition = RISCOS_ClearComposition;
    // device->IsTextInputShown = RISCOS_IsTextInputShown;

    /* Screen keyboard */
    // device->HasScreenKeyboardSupport = RISCOS_HasScreenKeyboardSupport;
    // device->ShowScreenKeyboard = RISCOS_ShowScreenKeyboard;
    // device->HideScreenKeyboard = RISCOS_HideScreenKeyboard;
    // device->IsScreenKeyboardShown = RISCOS_IsScreenKeyboardShown;

    /* Clipboard */
    // device->SetClipboardText = RISCOS_SetClipboardText;
    // device->GetClipboardText = RISCOS_GetClipboardText;
    // device->HasClipboardText = RISCOS_HasClipboardText;
    // device->SetPrimarySelectionText = RISCOS_SetPrimarySelectionText;
    // device->GetPrimarySelectionText = RISCOS_GetPrimarySelectionText;
    // device->HasPrimarySelectionText = RISCOS_HasPrimarySelectionText;

    /* Hit-testing */
    // device->SetWindowHitTest = RISCOS_SetWindowHitTest;

    /* Tell window that app enabled drag'n'drop events */
    // device->AcceptDragAndDrop = RISCOS_AcceptDragAndDrop;

    device->free = RISCOS_DeleteDevice;

    return device;
}
/* "SDL RISC OS video driver" */
const VideoBootStrap RISCOS_bootstrap = {
    "riscos", RISCOS_CreateDevice
};

static int RISCOS_VideoInit(_THIS)
{
    if (RISCOS_InitEvents() < 0) {
        return -1;
    }

    if (RISCOS_InitMouse() < 0) {
        return -1;
    }

    if (RISCOS_InitModes() < 0) {
        return -1;
    }

    /* We're done! */
    return 0;
}

static void RISCOS_VideoQuit(_THIS)
{
    RISCOS_QuitEvents();
}

#endif /* SDL_VIDEO_DRIVER_RISCOS */

/* vi: set ts=4 sw=4 expandtab: */
