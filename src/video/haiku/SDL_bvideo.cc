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
#include "../../main/haiku/SDL_BApp.h"

#ifdef SDL_VIDEO_DRIVER_HAIKU

#include "SDL_BWin.h"
#include <Url.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "SDL_bkeyboard.h"
#include "SDL_bwindow.h"
#include "SDL_bclipboard.h"
#include "SDL_bvideo.h"
#include "SDL_bopengl.h"
#include "SDL_bmodes.h"
#include "SDL_bframebuffer.h"
#include "SDL_bevents.h"

#ifdef SDL_VIDEO_VULKAN
#error "Vulkan is configured, but not implemented for haiku."
#endif
#ifdef SDL_VIDEO_METAL
#error "Metal is configured, but not implemented for haiku."
#endif
#if defined(SDL_VIDEO_OPENGL_ANY) && !defined(SDL_VIDEO_OPENGL)
#error "OpenGL is configured, but not the implemented (GL) for haiku."
#endif

static SDL_INLINE SDL_BWin *_ToBeWin(SDL_Window *window) {
    return (SDL_BWin *)(window->driverdata);
}

static SDL_bool HAIKU_CreateDevice(SDL_VideoDevice *device)
{
/* TODO: Figure out if any initialization needs to go here */

    /* Set the function pointers */
    /* Initialization/Query functions */
    device->VideoInit = HAIKU_VideoInit;
    device->VideoQuit = HAIKU_VideoQuit;
    device->GetDisplayBounds = HAIKU_GetDisplayBounds;
    // device->GetDisplayUsableBounds = HAIKU_GetDisplayUsableBounds;
    // device->GetDisplayDPI = HAIKU_GetDisplayDPI;
    device->SetDisplayMode = HAIKU_SetDisplayMode;

    /* Window functions */
    device->CreateSDLWindow = HAIKU_CreateSDLWindow;
//    device->CreateSDLWindowFrom = HAIKU_CreateSDLWindowFrom;
    device->SetWindowTitle = HAIKU_SetWindowTitle;
    device->SetWindowIcon = HAIKU_SetWindowIcon;
    device->SetWindowPosition = HAIKU_SetWindowPosition;
    device->SetWindowSize = HAIKU_SetWindowSize;
    device->SetWindowMinimumSize = HAIKU_SetWindowMinimumSize;
    device->SetWindowMaximumSize = HAIKU_SetWindowMaximumSize;
    // device->GetWindowBordersSize = HAIKU_GetWindowBordersSize;
    // device->GetWindowSizeInPixels = HAIKU_GetWindowSizeInPixels;
    // device->SetWindowOpacity = HAIKU_SetWindowOpacity;
    // device->SetWindowModalFor = HAIKU_SetWindowModalFor;
    // device->SetWindowInputFocus = HAIKU_SetWindowInputFocus;
    device->ShowWindow = HAIKU_ShowWindow;
    device->HideWindow = HAIKU_HideWindow;
    device->RaiseWindow = HAIKU_RaiseWindow;
    device->MaximizeWindow = HAIKU_MaximizeWindow;
    device->MinimizeWindow = HAIKU_MinimizeWindow;
    device->RestoreWindow = HAIKU_RestoreWindow;
    device->SetWindowBordered = HAIKU_SetWindowBordered;
    device->SetWindowResizable = HAIKU_SetWindowResizable;
    // device->SetWindowAlwaysOnTop = HAIKU_SetWindowAlwaysOnTop;
    device->SetWindowFullscreen = HAIKU_SetWindowFullscreen;
    device->SetWindowGammaRamp = HAIKU_SetWindowGammaRamp;
    device->GetWindowGammaRamp = HAIKU_GetWindowGammaRamp;
    // device->GetWindowICCProfile = HAIKU_GetWindowICCProfile;
    // device->GetWindowDisplayIndex = HAIKU_GetWindowDisplayIndex;
    // device->SetWindowMouseRect = HAIKU_SetWindowMouseRect;
    device->SetWindowMouseGrab = HAIKU_SetWindowMouseGrab;
    // device->SetWindowKeyboardGrab = HAIKU_SetWindowKeyboardGrab;
    device->DestroyWindow = HAIKU_DestroyWindow;
    device->CreateWindowFramebuffer = HAIKU_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = HAIKU_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = HAIKU_DestroyWindowFramebuffer;
    // device->OnWindowEnter = HAIKU_OnWindowEnter;
    // device->FlashWindow = HAIKU_FlashWindow;
    /* Shaped-window functions */
    // device->CreateShaper = HAIKU_CreateShaper;
    // device->SetWindowShape = HAIKU_SetWindowShape;
    /* Get some platform dependent window information */
    device->GetWindowWMInfo = HAIKU_GetWindowWMInfo;

    /* OpenGL support */
#ifdef SDL_VIDEO_OPENGL
    device->GL_LoadLibrary = HAIKU_GL_LoadLibrary;
    device->GL_GetProcAddress = HAIKU_GL_GetProcAddress;
    device->GL_UnloadLibrary = HAIKU_GL_UnloadLibrary;
    device->GL_CreateContext = HAIKU_GL_CreateContext;
    device->GL_MakeCurrent = HAIKU_GL_MakeCurrent;
    device->GL_GetDrawableSize = HAIKU_GL_GetDrawableSize;
    device->GL_SetSwapInterval = HAIKU_GL_SetSwapInterval;
    device->GL_GetSwapInterval = HAIKU_GL_GetSwapInterval;
    device->GL_SwapWindow = HAIKU_GL_SwapWindow;
    device->GL_DeleteContext = HAIKU_GL_DeleteContext;
#endif

    /* Vulkan support */
#ifdef SDL_VIDEO_VULKAN
    // device->Vulkan_LoadLibrary = HAIKU_Vulkan_LoadLibrary;
    // device->Vulkan_UnloadLibrary = HAIKU_Vulkan_UnloadLibrary;
    // device->Vulkan_GetInstanceExtensions = HAIKU_Vulkan_GetInstanceExtensions;
    // device->Vulkan_CreateSurface = HAIKU_Vulkan_CreateSurface;
    // device->Vulkan_GetDrawableSize = HAIKU_Vulkan_GetDrawableSize;
#endif

    /* Metal support */
#ifdef SDL_VIDEO_METAL
    // device->Metal_CreateView = HAIKU_Metal_CreateView;
    // device->Metal_DestroyView = HAIKU_Metal_DestroyView;
    // device->Metal_GetLayer = HAIKU_Metal_GetLayer;
    // device->Metal_GetDrawableSize = HAIKU_Metal_GetDrawableSize;
#endif

    /* Event manager functions */
    // device->WaitEventTimeout = HAIKU_WaitEventTimeout;
    // device->SendWakeupEvent = HAIKU_SendWakeupEvent;
    device->PumpEvents = HAIKU_PumpEvents;

    /* Screensaver */
    // device->SuspendScreenSaver = HAIKU_SuspendScreenSaver;

    /* Text input */
    // device->StartTextInput = HAIKU_StartTextInput;
    // device->StopTextInput = HAIKU_StopTextInput;
    // device->SetTextInputRect = HAIKU_SetTextInputRect;
    // device->ClearComposition = HAIKU_ClearComposition;
    // device->IsTextInputShown = HAIKU_IsTextInputShown;

    /* Screen keyboard */
    // device->HasScreenKeyboardSupport = HAIKU_HasScreenKeyboardSupport;
    // device->ShowScreenKeyboard = HAIKU_ShowScreenKeyboard;
    // device->HideScreenKeyboard = HAIKU_HideScreenKeyboard;
    // device->IsScreenKeyboardShown = HAIKU_IsScreenKeyboardShown;

    /* Clipboard */
    device->SetClipboardText = HAIKU_SetClipboardText;
    device->GetClipboardText = HAIKU_GetClipboardText;
    device->HasClipboardText = HAIKU_HasClipboardText;
    // device->SetPrimarySelectionText = HAIKU_SetPrimarySelectionText;
    // device->GetPrimarySelectionText = HAIKU_GetPrimarySelectionText;
    // device->HasPrimarySelectionText = HAIKU_HasPrimarySelectionText;

    /* Hit-testing */
    // device->SetWindowHitTest = HAIKU_SetWindowHitTest;

    /* Tell window that app enabled drag'n'drop events */
    // device->AcceptDragAndDrop = HAIKU_AcceptDragAndDrop;

    device->DeleteDevice = HAIKU_DeleteDevice;

    return SDL_TRUE;
}
/* "Haiku graphics" */
const VideoBootStrap HAIKU_bootstrap = {
    "haiku", HAIKU_CreateDevice
};

void HAIKU_DeleteDevice(_THIS)
{
}

static SDL_Cursor * HAIKU_CreateSystemCursor(SDL_SystemCursor id)
{
    SDL_Cursor *cursor;
    BCursorID cursorId = B_CURSOR_ID_SYSTEM_DEFAULT;

    switch(id)
    {
    default:
        SDL_assume(!"Unknown system cursor");
        return NULL;
    case SDL_SYSTEM_CURSOR_ARROW:     cursorId = B_CURSOR_ID_SYSTEM_DEFAULT; break;
    case SDL_SYSTEM_CURSOR_IBEAM:     cursorId = B_CURSOR_ID_I_BEAM; break;
    case SDL_SYSTEM_CURSOR_WAIT:      cursorId = B_CURSOR_ID_PROGRESS; break;
    case SDL_SYSTEM_CURSOR_CROSSHAIR: cursorId = B_CURSOR_ID_CROSS_HAIR; break;
    case SDL_SYSTEM_CURSOR_WAITARROW: cursorId = B_CURSOR_ID_PROGRESS; break;
    case SDL_SYSTEM_CURSOR_SIZENWSE:  cursorId = B_CURSOR_ID_RESIZE_NORTH_WEST_SOUTH_EAST; break;
    case SDL_SYSTEM_CURSOR_SIZENESW:  cursorId = B_CURSOR_ID_RESIZE_NORTH_EAST_SOUTH_WEST; break;
    case SDL_SYSTEM_CURSOR_SIZEWE:    cursorId = B_CURSOR_ID_RESIZE_EAST_WEST; break;
    case SDL_SYSTEM_CURSOR_SIZENS:    cursorId = B_CURSOR_ID_RESIZE_NORTH_SOUTH; break;
    case SDL_SYSTEM_CURSOR_SIZEALL:   cursorId = B_CURSOR_ID_MOVE; break;
    case SDL_SYSTEM_CURSOR_NO:        cursorId = B_CURSOR_ID_NOT_ALLOWED; break;
    case SDL_SYSTEM_CURSOR_HAND:      cursorId = B_CURSOR_ID_FOLLOW_LINK; break;
    }

    cursor = (SDL_Cursor *) SDL_calloc(1, sizeof(*cursor));
    if (cursor) {
        cursor->driverdata = (void *)new BCursor(cursorId);
    } else {
        SDL_OutOfMemory();
    }

    return cursor;
}

static SDL_Cursor * HAIKU_CreateDefaultCursor()
{
    return HAIKU_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
}

static void HAIKU_FreeCursor(SDL_Cursor * cursor)
{
    if (cursor->driverdata) {
        delete (BCursor*) cursor->driverdata;
    }
    SDL_free(cursor);
}

static SDL_Cursor * HAIKU_CreateCursor(SDL_Surface * surface, int hot_x, int hot_y)
{
    SDL_Cursor *cursor;
    SDL_Surface *converted;

    converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
    if (!converted) {
        return NULL;
    }

    BBitmap *cursorBitmap = new BBitmap(BRect(0, 0, surface->w - 1, surface->h - 1), B_RGBA32);
    cursorBitmap->SetBits(converted->pixels, converted->h * converted->pitch, 0, B_RGBA32);
    SDL_FreeSurface(converted);

    cursor = (SDL_Cursor *) SDL_calloc(1, sizeof(*cursor));
    if (cursor) {
        cursor->driverdata = (void *)new BCursor(cursorBitmap, BPoint(hot_x, hot_y));
    } else {
        return NULL;
    }

    return cursor;
}

static int HAIKU_ShowCursor(SDL_Cursor *cursor)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    if (!mouse) {
        return 0;
    }

    if (cursor) {
        BCursor *hCursor = (BCursor*)cursor->driverdata;
        be_app->SetCursor(hCursor);
    } else {
        BCursor *hCursor = new BCursor(B_CURSOR_ID_NO_CURSOR);
        be_app->SetCursor(hCursor);
        delete hCursor;
    }

    return 0;
}

static int HAIKU_SetRelativeMouseMode(SDL_bool enabled)
{
    SDL_Window *window = SDL_GetMouseFocus();
    if (!window) {
        return 0;
    }

    SDL_BWin *bewin = _ToBeWin(window);
    BGLView *_SDL_GLView = bewin->GetGLView();

    bewin->Lock();
    if (enabled)
        _SDL_GLView->SetEventMask(B_POINTER_EVENTS, B_NO_POINTER_HISTORY);
    else
        _SDL_GLView->SetEventMask(0, 0);
    bewin->Unlock();

    return 0;
}

static void HAIKU_MouseInit(void)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    if (!mouse) {
        return;
    }
    mouse->CreateCursor = HAIKU_CreateCursor;
    mouse->CreateSystemCursor = HAIKU_CreateSystemCursor;
    mouse->ShowCursor = HAIKU_ShowCursor;
    mouse->FreeCursor = HAIKU_FreeCursor;
    mouse->SetRelativeMouseMode = HAIKU_SetRelativeMouseMode;

    SDL_SetDefaultCursor(HAIKU_CreateDefaultCursor());
}

int HAIKU_VideoInit(_THIS)
{
    /* Initialize the Be Application for appserver interaction */
    if (SDL_InitBeApp() < 0) {
        return -1;
    }
    
    /* Initialize video modes */
    HAIKU_InitModes();

    /* Init the keymap */
    HAIKU_InitOSKeymap();

    HAIKU_MouseInit();

    /* We're done! */
    return 0;
}

void HAIKU_VideoQuit(_THIS)
{
    HAIKU_QuitModes();

    SDL_QuitBeApp();
}

// just sticking this function in here so it's in a C++ source file.
extern "C" { int HAIKU_OpenURL(const char *url); }
int HAIKU_OpenURL(const char *url)
{
    BUrl burl(url);
    const status_t rc = burl.OpenWithPreferredApplication(false);
    return (rc == B_NO_ERROR) ? 0 : SDL_SetError("URL open failed (err=%d)", (int)rc);
}

#ifdef __cplusplus
}
#endif

#endif /* SDL_VIDEO_DRIVER_HAIKU */

/* vi: set ts=4 sw=4 expandtab: */
