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

#ifdef SDL_VIDEO_DRIVER_NACL

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi_simple/ps.h"
#include "ppapi_simple/ps_interface.h"
#include "ppapi_simple/ps_event.h"
#include "nacl_io/nacl_io.h"

#include "SDL_naclvideo.h"
#include "SDL_naclwindow.h"
#include "SDL_naclevents_c.h"
#include "SDL_naclopengles.h"
#include "SDL_video.h"
#include "../SDL_sysvideo.h"
#include "../../events/SDL_events_c.h"

#ifdef SDL_VIDEO_VULKAN
#error "Vulkan is configured, but not implemented for nacl."
#endif
#ifdef SDL_VIDEO_METAL
#error "Metal is configured, but not implemented for nacl."
#endif
#ifndef SDL_VIDEO_OPENGL_ANY
#error "OpenGL must be configured for nacl."
#endif

/* Static init required because NACL_SetScreenResolution
 * may appear even before SDL starts and we want to remember
 * the window width and height
 */
NACL_VideoData naclVideoData;

void NACL_SetScreenResolution(int width, int height, Uint32 format)
{
    PP_Resource context;

    naclVideoData.w = width;
    naclVideoData.h = height;
    naclVideoData.format = format;

    if (naclVideoData.window) {
        naclVideoData.window->w = width;
        naclVideoData.window->h = height;
        SDL_SendWindowEvent(naclVideoData.window, SDL_WINDOWEVENT_RESIZED, width, height);
    }

    /* FIXME: Check threading issues...otherwise use a hardcoded _this->context across all threads */
    context = (PP_Resource) SDL_GL_GetCurrentContext();
    if (context) {
        PSInterfaceGraphics3D()->ResizeBuffers(context, width, height);
    }

}



/* Initialization/Query functions */
static int NACL_VideoInit(_THIS);
static void NACL_VideoQuit(_THIS);

static int NACL_Available(void) {
    return PSGetInstanceId() != 0;
}

static void NACL_DeleteDevice(_THIS)
{
    NACL_VideoData *driverdata = &naclVideoData;
    driverdata->ppb_core->ReleaseResource((PP_Resource) driverdata->ppb_message_loop);
    // SDL_zero(naclVideoData); -- do not clear, to remember the window width and height
}

static int NACL_SetDisplayMode(SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    return 0;
}

static SDL_bool NACL_CreateDevice(SDL_VideoDevice *device)
{
    if (!NACL_Available()) {
        return SDL_FALSE;
    }

    /* Set the function pointers */
    /* Initialization/Query functions */
    device->VideoInit = NACL_VideoInit;
    device->VideoQuit = NACL_VideoQuit;
    // device->GetDisplayBounds = NACL_GetDisplayBounds;
    // device->GetDisplayUsableBounds = NACL_GetDisplayUsableBounds;
    // device->GetDisplayDPI = NACL_GetDisplayDPI;
    device->SetDisplayMode = NACL_SetDisplayMode;

    /* Window functions */
    device->CreateSDLWindow = NACL_CreateSDLWindow;
    // device->CreateSDLWindowFrom = NACL_CreateSDLWindowFrom;
    // device->SetWindowTitle = NACL_SetWindowTitle;
    // device->SetWindowIcon = NACL_SetWindowIcon;
    // device->SetWindowPosition = NACL_SetWindowPosition;
    // device->SetWindowSize = NACL_SetWindowSize;
    // device->SetWindowMinimumSize = NACL_SetWindowMinimumSize;
    // device->SetWindowMaximumSize = NACL_SetWindowMaximumSize;
    // device->GetWindowBordersSize = NACL_GetWindowBordersSize;
    // device->GetWindowSizeInPixels = NACL_GetWindowSizeInPixels;
    // device->SetWindowOpacity = NACL_SetWindowOpacity;
    // device->SetWindowModalFor = NACL_SetWindowModalFor;
    // device->SetWindowInputFocus = NACL_SetWindowInputFocus;
    // device->ShowWindow = NACL_ShowWindow;
    // device->HideWindow = NACL_HideWindow;
    // device->RaiseWindow = NACL_RaiseWindow;
    // device->MaximizeWindow = NACL_MaximizeWindow;
    // device->MinimizeWindow = NACL_MinimizeWindow;
    // device->RestoreWindow = NACL_RestoreWindow;
    // device->SetWindowBordered = NACL_SetWindowBordered;
    // device->SetWindowResizable = NACL_SetWindowResizable;
    // device->SetWindowAlwaysOnTop = NACL_SetWindowAlwaysOnTop;
    // device->SetWindowFullscreen = NACL_SetWindowFullscreen;
    // device->SetWindowGammaRamp = NACL_SetWindowGammaRamp;
    // device->GetWindowGammaRamp = NACL_GetWindowGammaRamp;
    // device->GetWindowICCProfile = NACL_GetWindowICCProfile;
    // device->GetWindowDisplayIndex = NACL_GetWindowDisplayIndex;
    // device->SetWindowMouseRect = NACL_SetWindowMouseRect;
    // device->SetWindowMouseGrab = NACL_SetWindowMouseGrab;
    // device->SetWindowKeyboardGrab = NACL_SetWindowKeyboardGrab;
    device->DestroyWindow = NACL_DestroyWindow;
    // device->CreateWindowFramebuffer = NACL_CreateWindowFramebuffer;
    // device->UpdateWindowFramebuffer = NACL_UpdateWindowFramebuffer;
    // device->DestroyWindowFramebuffer = NACL_DestroyWindowFramebuffer;
    // device->OnWindowEnter = NACL_OnWindowEnter;
    // device->FlashWindow = NACL_FlashWindow;
    /* Shaped-window functions */
    // device->CreateShaper = NACL_CreateShaper;
    // device->SetWindowShape = NACL_SetWindowShape;
    /* Get some platform dependent window information */
    // device->GetWindowWMInfo = NACL_GetWindowWMInfo;

    /* OpenGL support */
    device->GL_LoadLibrary = NACL_GLES_LoadLibrary;
    device->GL_GetProcAddress = NACL_GLES_GetProcAddress;
    device->GL_UnloadLibrary = NACL_GLES_UnloadLibrary;
    device->GL_CreateContext = NACL_GLES_CreateContext;
    device->GL_MakeCurrent = NACL_GLES_MakeCurrent;
    device->GL_GetDrawableSize = NACL_GLES_GetDrawableSize;
    device->GL_SetSwapInterval = NACL_GLES_SetSwapInterval;
    device->GL_GetSwapInterval = NACL_GLES_GetSwapInterval;
    device->GL_SwapWindow = NACL_GLES_SwapWindow;
    device->GL_DeleteContext = NACL_GLES_DeleteContext;
    // device->GL_DefaultProfileConfig = NACL_GLES_DefaultProfileConfig;

    /* Vulkan support */
#ifdef SDL_VIDEO_VULKAN
    // device->Vulkan_LoadLibrary = NACL_Vulkan_LoadLibrary;
    // device->Vulkan_UnloadLibrary = NACL_Vulkan_UnloadLibrary;
    // device->Vulkan_GetInstanceExtensions = NACL_Vulkan_GetInstanceExtensions;
    // device->Vulkan_CreateSurface = NACL_Vulkan_CreateSurface;
    // device->Vulkan_GetDrawableSize = NACL_Vulkan_GetDrawableSize;
#endif

    /* Metal support */
#ifdef SDL_VIDEO_METAL
    // device->Metal_CreateView = NACL_Metal_CreateView;
    // device->Metal_DestroyView = NACL_Metal_DestroyView;
    // device->Metal_GetLayer = NACL_Metal_GetLayer;
    // device->Metal_GetDrawableSize = NACL_Metal_GetDrawableSize;
#endif

    /* Event manager functions */
    // device->WaitEventTimeout = NACL_WaitEventTimeout;
    // device->SendWakeupEvent = NACL_SendWakeupEvent;
    device->PumpEvents = NACL_PumpEvents;

    /* Screensaver */
    // device->SuspendScreenSaver = NACL_SuspendScreenSaver;

    /* Text input */
    // device->StartTextInput = NACL_StartTextInput;
    // device->StopTextInput = NACL_StopTextInput;
    // device->SetTextInputRect = NACL_SetTextInputRect;
    // device->ClearComposition = NACL_ClearComposition;
    // device->IsTextInputShown = NACL_IsTextInputShown;

    /* Screen keyboard */
    // device->HasScreenKeyboardSupport = NACL_HasScreenKeyboardSupport;
    // device->ShowScreenKeyboard = NACL_ShowScreenKeyboard;
    // device->HideScreenKeyboard = NACL_HideScreenKeyboard;
    // device->IsScreenKeyboardShown = NACL_IsScreenKeyboardShown;

    /* Clipboard */
    // device->SetClipboardText = NACL_SetClipboardText;
    // device->GetClipboardText = NACL_GetClipboardText;
    // device->HasClipboardText = NACL_HasClipboardText;
    // device->SetPrimarySelectionText = NACL_SetPrimarySelectionText;
    // device->GetPrimarySelectionText = NACL_GetPrimarySelectionText;
    // device->HasPrimarySelectionText = NACL_HasPrimarySelectionText;

    /* Hit-testing */
    // device->SetWindowHitTest = NACL_SetWindowHitTest;

    /* Tell window that app enabled drag'n'drop events */
    // device->AcceptDragAndDrop = NACL_AcceptDragAndDrop;

    device->DeleteDevice = NACL_DeleteDevice;


    return SDL_TRUE;
}
/* "SDL Native Client Video Driver" */
const VideoBootStrap NACL_bootstrap = {
    "nacl", NACL_CreateDevice
};

int NACL_VideoInit(_THIS)
{
    NACL_VideoData *driverdata = &naclVideoData;
    int result;
    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;

    current_mode.format = driverdata->format;
    current_mode.w = driverdata->w;
    current_mode.h = driverdata->h;
    current_mode.refresh_rate = 0;
    current_mode.driverdata = NULL;

    SDL_zero(display);
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;
    // display.driverdata = NULL;

    SDL_AddDisplayMode(&display, &current_mode);
    result = SDL_AddVideoDisplay(&display, SDL_FALSE);
    if (result < 0) {
        SDL_free(display.display_modes);
    }

    PSInterfaceInit();
    driverdata->instance = PSGetInstanceId();
    driverdata->ppb_graphics = PSInterfaceGraphics3D();
    driverdata->ppb_message_loop = PSInterfaceMessageLoop();
    driverdata->ppb_core = PSInterfaceCore();
    driverdata->ppb_fullscreen = PSInterfaceFullscreen();
    driverdata->ppb_instance = PSInterfaceInstance();
    driverdata->ppb_image_data = PSInterfaceImageData();
    driverdata->ppb_view = PSInterfaceView();
    driverdata->ppb_var = PSInterfaceVar();
    driverdata->ppb_input_event = (PPB_InputEvent*) PSGetInterface(PPB_INPUT_EVENT_INTERFACE);
    driverdata->ppb_keyboard_input_event = (PPB_KeyboardInputEvent*) PSGetInterface(PPB_KEYBOARD_INPUT_EVENT_INTERFACE);
    driverdata->ppb_mouse_input_event = (PPB_MouseInputEvent*) PSGetInterface(PPB_MOUSE_INPUT_EVENT_INTERFACE);
    driverdata->ppb_wheel_input_event = (PPB_WheelInputEvent*) PSGetInterface(PPB_WHEEL_INPUT_EVENT_INTERFACE);
    driverdata->ppb_touch_input_event = (PPB_TouchInputEvent*) PSGetInterface(PPB_TOUCH_INPUT_EVENT_INTERFACE);


    driverdata->message_loop = driverdata->ppb_message_loop->Create(driverdata->instance);

    PSEventSetFilter(PSE_ALL);

    return result;
}

void NACL_VideoQuit(_THIS) {
}

#endif /* SDL_VIDEO_DRIVER_NACL */
/* vi: set ts=4 sw=4 expandtab: */
