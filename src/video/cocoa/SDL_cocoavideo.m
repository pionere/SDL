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

#ifdef SDL_VIDEO_DRIVER_COCOA

#if !__has_feature(objc_arc)
#error SDL must be built with Objective-C ARC (automatic reference counting) enabled
#endif

#include "SDL.h"
#include "SDL_endian.h"
#include "SDL_cocoavideo.h"
#include "SDL_cocoashape.h"
#include "SDL_cocoavulkan.h"
#include "SDL_cocoametalview.h"
#include "SDL_cocoaopengles.h"

#if defined(SDL_VIDEO_OPENGL_ANY) && !defined(SDL_VIDEO_OPENGL_CGL) && !defined(SDL_VIDEO_OPENGL_EGL)
#error "OpenGL is configured, but not the implemented (CGL/EGL) for cocoa."
#endif

@implementation Cocoa_VideoData

@end
/* Instance */
Cocoa_VideoData *cocoaVideoData = nil;

/* Initialization/Query functions */
static int Cocoa_VideoInit(_THIS);
static void Cocoa_VideoQuit(_THIS);

/* Cocoa driver bootstrap functions */

static void Cocoa_DeleteDevice(_THIS)
{ @autoreleasepool
{
    if (_this->wakeup_lock) {
        SDL_DestroyMutex(_this->wakeup_lock);
    }
    CFBridgingRelease((__bridge void *)cocoaVideoData);
    // CFRelease((__bridge CFTypeRef)cocoaVideoData);
    cocoaVideoData = nil;
    SDL_free(_this);
}}

static SDL_VideoDevice *Cocoa_CreateDevice(void)
{ @autoreleasepool
{
    SDL_VideoDevice *device;
    Cocoa_VideoData *data;

    Cocoa_RegisterApp();

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (device) {
        data = [[Cocoa_VideoData alloc] init];
    } else {
        data = nil;
    }
    if (!data) {
        SDL_OutOfMemory();
        SDL_free(device);
        return NULL;
    }
    cocoaVideoData = (__bridge Cocoa_VideoData *)CFBridgingRetain(data);
    device->wakeup_lock = SDL_CreateMutex();

    /* Set the function pointers */
    /* Initialization/Query functions */
    device->VideoInit = Cocoa_VideoInit;
    device->VideoQuit = Cocoa_VideoQuit;
    device->GetDisplayBounds = Cocoa_GetDisplayBounds;
    device->GetDisplayUsableBounds = Cocoa_GetDisplayUsableBounds;
    device->GetDisplayDPI = Cocoa_GetDisplayDPI;
    device->SetDisplayMode = Cocoa_SetDisplayMode;

    /* Window functions */
    device->CreateSDLWindow = Cocoa_CreateSDLWindow;
    device->CreateSDLWindowFrom = Cocoa_CreateSDLWindowFrom;
    device->SetWindowTitle = Cocoa_SetWindowTitle;
    device->SetWindowIcon = Cocoa_SetWindowIcon;
    device->SetWindowPosition = Cocoa_SetWindowPosition;
    device->SetWindowSize = Cocoa_SetWindowSize;
    device->SetWindowMinimumSize = Cocoa_SetWindowMinimumSize;
    device->SetWindowMaximumSize = Cocoa_SetWindowMaximumSize;
    // device->GetWindowBordersSize = Cocoa_GetWindowBordersSize;
    device->GetWindowSizeInPixels = Cocoa_GetWindowSizeInPixels;
    device->SetWindowOpacity = Cocoa_SetWindowOpacity;
    // device->SetWindowModalFor = Cocoa_SetWindowModalFor;
    // device->SetWindowInputFocus = Cocoa_SetWindowInputFocus;
    device->ShowWindow = Cocoa_ShowWindow;
    device->HideWindow = Cocoa_HideWindow;
    device->RaiseWindow = Cocoa_RaiseWindow;
    device->MaximizeWindow = Cocoa_MaximizeWindow;
    device->MinimizeWindow = Cocoa_MinimizeWindow;
    device->RestoreWindow = Cocoa_RestoreWindow;
    device->SetWindowBordered = Cocoa_SetWindowBordered;
    device->SetWindowResizable = Cocoa_SetWindowResizable;
    device->SetWindowAlwaysOnTop = Cocoa_SetWindowAlwaysOnTop;
    device->SetWindowFullscreen = Cocoa_SetWindowFullscreen;
    device->SetWindowGammaRamp = Cocoa_SetWindowGammaRamp;
    device->GetWindowGammaRamp = Cocoa_GetWindowGammaRamp;
    device->GetWindowICCProfile = Cocoa_GetWindowICCProfile;
    device->GetWindowDisplayIndex = Cocoa_GetWindowDisplayIndex;
    device->SetWindowMouseRect = Cocoa_SetWindowMouseRect;
    device->SetWindowMouseGrab = Cocoa_SetWindowMouseGrab;
    device->SetWindowKeyboardGrab = Cocoa_SetWindowKeyboardGrab;
    device->DestroyWindow = Cocoa_DestroyWindow;
    // device->CreateWindowFramebuffer = Cocoa_CreateWindowFramebuffer;
    // device->UpdateWindowFramebuffer = Cocoa_UpdateWindowFramebuffer;
    // device->DestroyWindowFramebuffer = Cocoa_DestroyWindowFramebuffer;
    // device->OnWindowEnter = Cocoa_OnWindowEnter;
    device->FlashWindow = Cocoa_FlashWindow;
    /* Shaped-window functions */
    device->CreateShaper = Cocoa_CreateShaper;
    device->SetWindowShape = Cocoa_SetWindowShape;
    /* Get some platform dependent window information */
    device->GetWindowWMInfo = Cocoa_GetWindowWMInfo;

    /* OpenGL support */
#ifdef SDL_VIDEO_OPENGL_CGL
    Cocoa_GL_InitDevice(device);
#elif defined(SDL_VIDEO_OPENGL_EGL)
    Cocoa_GLES_InitDevice(device);
#endif

    /* Vulkan support */
#ifdef SDL_VIDEO_VULKAN
    device->Vulkan_LoadLibrary = Cocoa_Vulkan_LoadLibrary;
    device->Vulkan_UnloadLibrary = Cocoa_Vulkan_UnloadLibrary;
    device->Vulkan_GetInstanceExtensions = Cocoa_Vulkan_GetInstanceExtensions;
    device->Vulkan_CreateSurface = Cocoa_Vulkan_CreateSurface;
    device->Vulkan_GetDrawableSize = Cocoa_Vulkan_GetDrawableSize;
#endif

    /* Metal support */
#ifdef SDL_VIDEO_METAL
    device->Metal_CreateView = Cocoa_Metal_CreateView;
    device->Metal_DestroyView = Cocoa_Metal_DestroyView;
    device->Metal_GetLayer = Cocoa_Metal_GetLayer;
    device->Metal_GetDrawableSize = Cocoa_Metal_GetDrawableSize;
#endif
    /* Event manager functions */
    device->WaitEventTimeout = Cocoa_WaitEventTimeout;
    device->SendWakeupEvent = Cocoa_SendWakeupEvent;
    device->PumpEvents = Cocoa_PumpEvents;

    /* Screensaver */
    device->SuspendScreenSaver = Cocoa_SuspendScreenSaver;

    /* Text input */
    device->StartTextInput = Cocoa_StartTextInput;
    device->StopTextInput = Cocoa_StopTextInput;
    device->SetTextInputRect = Cocoa_SetTextInputRect;
    // device->ClearComposition = Cocoa_ClearComposition;
    // device->IsTextInputShown = Cocoa_IsTextInputShown;

    /* Screen keyboard */
    // device->HasScreenKeyboardSupport = Cocoa_HasScreenKeyboardSupport;
    // device->ShowScreenKeyboard = Cocoa_ShowScreenKeyboard;
    // device->HideScreenKeyboard = Cocoa_HideScreenKeyboard;
    // device->IsScreenKeyboardShown = Cocoa_IsScreenKeyboardShown;

    /* Clipboard */
    device->SetClipboardText = Cocoa_SetClipboardText;
    device->GetClipboardText = Cocoa_GetClipboardText;
    device->HasClipboardText = Cocoa_HasClipboardText;
    // device->SetPrimarySelectionText = Cocoa_SetPrimarySelectionText;
    // device->GetPrimarySelectionText = Cocoa_GetPrimarySelectionText;
    // device->HasPrimarySelectionText = Cocoa_HasPrimarySelectionText;

    /* Hit-testing */
    device->SetWindowHitTest = Cocoa_SetWindowHitTest;

    /* Tell window that app enabled drag'n'drop events */
    device->AcceptDragAndDrop = Cocoa_AcceptDragAndDrop;

    device->free = Cocoa_DeleteDevice;

    return device;
}}
/* "SDL Cocoa video driver" */
const VideoBootStrap COCOA_bootstrap = {
    "cocoa", Cocoa_CreateDevice
};


int Cocoa_VideoInit(_THIS)
{ @autoreleasepool
{
    Cocoa_VideoData *data = cocoaVideoData;

    Cocoa_InitModes();
    Cocoa_InitKeyboard();
    if (Cocoa_InitMouse() < 0) {
        return -1;
    }

    data.allow_spaces = SDL_GetHintBoolean(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, SDL_TRUE);
    data.trackpad_is_touch_only = SDL_GetHintBoolean(SDL_HINT_TRACKPAD_IS_TOUCH_ONLY, SDL_FALSE);

    data.swaplock = SDL_CreateMutex();
    if (!data.swaplock) {
        return -1;
    }

    return 0;
}}

void Cocoa_VideoQuit(_THIS)
{ @autoreleasepool
{
    Cocoa_VideoData *data = cocoaVideoData;
    Cocoa_QuitModes();
    Cocoa_QuitKeyboard();
    Cocoa_QuitMouse();
    SDL_DestroyMutex(data.swaplock);
    data.swaplock = NULL;
}}

/* This function assumes that it's called from within an autorelease pool */
NSImage *Cocoa_CreateImage(SDL_Surface * surface)
{
    SDL_Surface *converted;
    NSBitmapImageRep *imgrep;
    Uint8 *pixels;
    int i;
    NSImage *img;

    converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    if (!converted) {
        return nil;
    }

    imgrep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes: NULL
                    pixelsWide: converted->w
                    pixelsHigh: converted->h
                    bitsPerSample: 8
                    samplesPerPixel: 4
                    hasAlpha: YES
                    isPlanar: NO
                    colorSpaceName: NSDeviceRGBColorSpace
                    bytesPerRow: converted->pitch
                    bitsPerPixel: converted->format->BitsPerPixel];
    if (imgrep == nil) {
        SDL_FreeSurface(converted);
        return nil;
    }

    /* Copy the pixels */
    pixels = [imgrep bitmapData];
    SDL_memcpy(pixels, converted->pixels, converted->h * converted->pitch);
    SDL_FreeSurface(converted);

    /* Premultiply the alpha channel */
    for (i = (surface->h * surface->w); i--; ) {
        Uint8 alpha = pixels[3];
        pixels[0] = (Uint8)(((Uint16)pixels[0] * alpha) / 255);
        pixels[1] = (Uint8)(((Uint16)pixels[1] * alpha) / 255);
        pixels[2] = (Uint8)(((Uint16)pixels[2] * alpha) / 255);
        pixels += 4;
    }

    img = [[NSImage alloc] initWithSize: NSMakeSize(surface->w, surface->h)];
    if (img != nil) {
        [img addRepresentation: imgrep];
    }
    return img;
}

/*
 * Mac OS X log support.
 *
 * This doesn't really have aything to do with the interfaces of the SDL video
 *  subsystem, but we need to stuff this into an Objective-C source code file.
 *
 * NOTE: This is copypasted in src/video/uikit/SDL_uikitvideo.m! Be sure both
 *  versions remain identical!
 */

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

/* vim: set ts=4 sw=4 expandtab: */
