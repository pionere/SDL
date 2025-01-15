/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_UIKIT

#import <UIKit/UIKit.h>

#include "SDL_mouse.h"
#include "SDL_hints.h"
#include "../../events/SDL_events_c.h"

#include "SDL_uikitvideo.h"
#include "SDL_uikitevents.h"
#include "SDL_uikitmodes.h"
#include "SDL_uikitwindow.h"
#include "SDL_uikitopengles.h"
#include "SDL_uikitclipboard.h"
#include "SDL_uikitvulkan.h"
#include "SDL_uikitmetalview.h"

#if defined(SDL_VIDEO_OPENGL_ANY) && !defined(SDL_VIDEO_OPENGL_ES) && !defined(SDL_VIDEO_OPENGL_ES2)
#error "OpenGL is configured, but not the implemented (GLES/GLES2) for uikit."
#endif

/* Initialization/Query functions */
static int UIKit_VideoInit(_THIS);
static void UIKit_VideoQuit(_THIS);

/* UIKit driver bootstrap functions */

static void UIKit_DeleteDevice(_THIS)
{
}

static SDL_bool UIKit_CreateDevice(SDL_VideoDevice *device)
{
    @autoreleasepool {
        /* Set the function pointers */
        /* Initialization/Query functions */
        device->VideoInit = UIKit_VideoInit;
        device->VideoQuit = UIKit_VideoQuit;
        // device->GetDisplayBounds = UIKit_GetDisplayBounds;
        device->GetDisplayUsableBounds = UIKit_GetDisplayUsableBounds;
        device->GetDisplayDPI = UIKit_GetDisplayDPI;
        device->SetDisplayMode = UIKit_SetDisplayMode;

        /* Window functions */
        device->CreateSDLWindow = UIKit_CreateSDLWindow;
        // device->CreateSDLWindowFrom = UIKit_CreateSDLWindowFrom;
        device->SetWindowTitle = UIKit_SetWindowTitle;
        // device->SetWindowIcon = UIKit_SetWindowIcon;
        // device->SetWindowPosition = UIKit_SetWindowPosition;
        // device->SetWindowSize = UIKit_SetWindowSize;
        // device->SetWindowMinimumSize = UIKit_SetWindowMinimumSize;
        // device->SetWindowMaximumSize = UIKit_SetWindowMaximumSize;
        // device->GetWindowBordersSize = UIKit_GetWindowBordersSize;
        device->GetWindowSizeInPixels = UIKit_GetWindowSizeInPixels;
        // device->SetWindowOpacity = UIKit_SetWindowOpacity;
        // device->SetWindowModalFor = UIKit_SetWindowModalFor;
        // device->SetWindowInputFocus = UIKit_SetWindowInputFocus;
        device->ShowWindow = UIKit_ShowWindow;
        device->HideWindow = UIKit_HideWindow;
        device->RaiseWindow = UIKit_RaiseWindow;
        // device->MaximizeWindow = UIKit_MaximizeWindow;
        // device->MinimizeWindow = UIKit_MinimizeWindow;
        // device->RestoreWindow = UIKit_RestoreWindow;
        device->SetWindowBordered = UIKit_SetWindowBordered;
        // device->SetWindowResizable = UIKit_SetWindowResizable;
        // device->SetWindowAlwaysOnTop = UIKit_SetWindowAlwaysOnTop;
        device->SetWindowFullscreen = UIKit_SetWindowFullscreen;
        // device->SetWindowGammaRamp = UIKit_SetWindowGammaRamp;
        // device->GetWindowGammaRamp = UIKit_GetWindowGammaRamp;
        // device->GetWindowICCProfile = UIKit_GetWindowICCProfile;
        // device->GetWindowDisplayIndex = UIKit_GetWindowDisplayIndex;
        // device->SetWindowMouseRect = UIKit_SetWindowMouseRect;
        device->SetWindowMouseGrab = UIKit_SetWindowMouseGrab;
        // device->SetWindowKeyboardGrab = UIKit_SetWindowKeyboardGrab;
        device->DestroyWindow = UIKit_DestroyWindow;
        // device->CreateWindowFramebuffer = UIKit_CreateWindowFramebuffer;
        // device->UpdateWindowFramebuffer = UIKit_UpdateWindowFramebuffer;
        // device->DestroyWindowFramebuffer = UIKit_DestroyWindowFramebuffer;
        // device->OnWindowEnter = UIKit_OnWindowEnter;
        // device->FlashWindow = UIKit_FlashWindow;
        /* Shaped-window functions */
        // device->CreateShaper = UIKit_CreateShaper;
        // device->SetWindowShape = UIKit_SetWindowShape;
        /* Get some platform dependent window information */
        device->GetWindowWMInfo = UIKit_GetWindowWMInfo;

        /* OpenGL support */
#if defined(SDL_VIDEO_OPENGL_ES) || defined(SDL_VIDEO_OPENGL_ES2)
        device->GL_LoadLibrary = UIKit_GL_LoadLibrary;
        device->GL_GetProcAddress = UIKit_GL_GetProcAddress;
        device->GL_UnloadLibrary = UIKit_GL_UnloadLibrary;
        device->GL_CreateContext = UIKit_GL_CreateContext;
        device->GL_MakeCurrent = UIKit_GL_MakeCurrent;
        device->GL_GetDrawableSize = UIKit_GL_GetDrawableSize;
        device->GL_SetSwapInterval = UIKit_GL_SetSwapInterval;
        device->GL_GetSwapInterval = UIKit_GL_GetSwapInterval;
        device->GL_SwapWindow = UIKit_GL_SwapWindow;
        device->GL_DeleteContext = UIKit_GL_DeleteContext;
#endif

        /* Vulkan support */
#ifdef SDL_VIDEO_VULKAN
        device->Vulkan_LoadLibrary = UIKit_Vulkan_LoadLibrary;
        device->Vulkan_UnloadLibrary = UIKit_Vulkan_UnloadLibrary;
        device->Vulkan_GetInstanceExtensions
                                     = UIKit_Vulkan_GetInstanceExtensions;
        device->Vulkan_CreateSurface = UIKit_Vulkan_CreateSurface;
        device->Vulkan_GetDrawableSize = UIKit_Vulkan_GetDrawableSize;
#endif

        /* Metal support */
#ifdef SDL_VIDEO_METAL
        device->Metal_CreateView = UIKit_Metal_CreateView;
        device->Metal_DestroyView = UIKit_Metal_DestroyView;
        device->Metal_GetLayer = UIKit_Metal_GetLayer;
        device->Metal_GetDrawableSize = UIKit_Metal_GetDrawableSize;
#endif

        /* Event manager functions */
        // device->WaitEventTimeout = UIKit_WaitEventTimeout;
        // device->SendWakeupEvent = UIKit_SendWakeupEvent;
        device->PumpEvents = UIKit_PumpEvents;

        /* Screensaver */
        device->SuspendScreenSaver = UIKit_SuspendScreenSaver;

        /* Text input */
#ifdef SDL_IPHONE_KEYBOARD
        // device->StartTextInput = UIKit_StartTextInput;
        // device->StopTextInput = UIKit_StopTextInput;
        device->SetTextInputRect = UIKit_SetTextInputRect;
        // device->ClearComposition = UIKit_ClearComposition;
        // device->IsTextInputShown = UIKit_IsTextInputShown;

        /* Screen keyboard */
        device->HasScreenKeyboardSupport = UIKit_HasScreenKeyboardSupport;
        device->ShowScreenKeyboard = UIKit_ShowScreenKeyboard;
        device->HideScreenKeyboard = UIKit_HideScreenKeyboard;
        device->IsScreenKeyboardShown = UIKit_IsScreenKeyboardShown;
#endif
        /* Clipboard */
        device->SetClipboardText = UIKit_SetClipboardText;
        device->GetClipboardText = UIKit_GetClipboardText;
        device->HasClipboardText = UIKit_HasClipboardText;
        // device->SetPrimarySelectionText = UIKit_SetPrimarySelectionText;
        // device->GetPrimarySelectionText = UIKit_GetPrimarySelectionText;
        // device->HasPrimarySelectionText = UIKit_HasPrimarySelectionText;

        /* Hit-testing */
        // device->SetWindowHitTest = UIKit_SetWindowHitTest;

        /* Tell window that app enabled drag'n'drop events */
        // device->AcceptDragAndDrop = UIKit_AcceptDragAndDrop;

        device->DeleteDevice = UIKit_DeleteDevice;

        return SDL_TRUE;
    }
}
/* "SDL UIKit video driver" */
const VideoBootStrap UIKit_bootstrap = {
    "uikit", UIKit_CreateDevice
};


int UIKit_VideoInit(_THIS)
{
    UIKit_GL_LoadLibrary(_this, NULL);
    _this->gl_config.driver_loaded = 1;

    if (UIKit_InitModes() < 0) {
        return -1;
    }

    SDL_InitGCKeyboard();
    SDL_InitGCMouse();

    return 0;
}

void UIKit_VideoQuit(_THIS)
{
    SDL_QuitGCKeyboard();
    SDL_QuitGCMouse();

    UIKit_QuitModes();

    // UIKit_GL_UnloadLibrary(_this);
}

void UIKit_SuspendScreenSaver(SDL_bool suspend)
{
    @autoreleasepool {
        /* Ignore ScreenSaver API calls if the idle timer hint has been set. */
        /* FIXME: The idle timer hint should be deprecated for SDL 2.1. */
        if (!SDL_GetHintBoolean(SDL_HINT_IDLE_TIMER_DISABLED, SDL_FALSE)) {
            UIApplication *app = [UIApplication sharedApplication];

            /* Prevent the display from dimming and going to sleep. */
            app.idleTimerDisabled = (suspend != SDL_FALSE);
        }
    }
}

SDL_bool UIKit_IsSystemVersionAtLeast(double version)
{
    return [[UIDevice currentDevice].systemVersion doubleValue] >= version;
}

CGRect UIKit_ComputeViewFrame(SDL_Window *window, UIScreen *screen)
{
    SDL_WindowData *data = (__bridge SDL_WindowData *) window->driverdata;
    CGRect frame = screen.bounds;

    /* Use the UIWindow bounds instead of the UIScreen bounds, when possible.
     * The uiwindow bounds may be smaller than the screen bounds when Split View
     * is used on an iPad. */
    if (data != nil && data.uiwindow != nil) {
        frame = data.uiwindow.bounds;
    }

#if !TARGET_OS_TV
    /* iOS 10 seems to have a bug where, in certain conditions, putting the
     * device to sleep with the a landscape-only app open, re-orienting the
     * device to portrait, and turning it back on will result in the screen
     * bounds returning portrait orientation despite the app being in landscape.
     * This is a workaround until a better solution can be found.
     * https://bugzilla.libsdl.org/show_bug.cgi?id=3505
     * https://bugzilla.libsdl.org/show_bug.cgi?id=3465
     * https://forums.developer.apple.com/thread/65337 */
    {
    UIInterfaceOrientation orient = [UIApplication sharedApplication].statusBarOrientation;
    BOOL landscape = UIInterfaceOrientationIsLandscape(orient) ||
                    !(UIKit_GetSupportedOrientations(window) & (UIInterfaceOrientationMaskPortrait | UIInterfaceOrientationMaskPortraitUpsideDown));
    BOOL fullscreen = CGRectEqualToRect(screen.bounds, frame);

    /* The orientation flip doesn't make sense when the window is smaller
     * than the screen (iPad Split View, for example). */
    if (fullscreen && (landscape != (frame.size.width > frame.size.height))) {
        float height = frame.size.width;
        frame.size.width = frame.size.height;
        frame.size.height = height;
    }
    }
#endif

    return frame;
}

void UIKit_ForceUpdateHomeIndicator(void)
{
#if !TARGET_OS_TV
    /* Force the main SDL window to re-evaluate home indicator state */
    SDL_Window *focus = SDL_GetFocusWindow();
    if (focus) {
        SDL_WindowData *data = (__bridge SDL_WindowData *) focus->driverdata;
        if (data != nil) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
            if ([data.viewcontroller respondsToSelector:@selector(setNeedsUpdateOfHomeIndicatorAutoHidden)]) {
                [data.viewcontroller performSelectorOnMainThread:@selector(setNeedsUpdateOfHomeIndicatorAutoHidden) withObject:nil waitUntilDone:NO];
                [data.viewcontroller performSelectorOnMainThread:@selector(setNeedsUpdateOfScreenEdgesDeferringSystemGestures) withObject:nil waitUntilDone:NO];
            }
#pragma clang diagnostic pop
        }
    }
#endif /* !TARGET_OS_TV */
}

/*
 * iOS log support.
 *
 * This doesn't really have aything to do with the interfaces of the SDL video
 *  subsystem, but we need to stuff this into an Objective-C source code file.
 *
 * NOTE: This is copypasted from src/video/cocoa/SDL_cocoavideo.m! Thus, if
 *  Cocoa is supported, we use that one instead. Be sure both versions remain
 *  identical!
 */

#if !defined(SDL_VIDEO_DRIVER_COCOA)
void SDL_NSLog(const char *prefix, const char *text)
{
    @autoreleasepool {
        NSString *nsText = [NSString stringWithUTF8String:text];
        if (prefix) {
            NSString *nsPrefix = [NSString stringWithUTF8String:prefix];
            NSLog(@"%@: %@", nsPrefix, nsText);
        } else {
            NSLog(@"%@", nsText);
        }
    }
}
#endif /* SDL_VIDEO_DRIVER_COCOA */

/*
 * iOS Tablet detection
 *
 * This doesn't really have aything to do with the interfaces of the SDL video
 * subsystem, but we need to stuff this into an Objective-C source code file.
 */
SDL_bool SDL_IsIPad(void)
{
    return ([UIDevice currentDevice].userInterfaceIdiom == UIUserInterfaceIdiomPad);
}

#endif /* SDL_VIDEO_DRIVER_UIKIT */

/* vi: set ts=4 sw=4 expandtab: */
