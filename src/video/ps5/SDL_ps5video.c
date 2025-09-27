/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_PS5

#include <errno.h>
#include <pthread.h>

#include "SDL_ps5tilemap.inc"
#include "SDL_ps5video.h"
#include "SDL_ps5keyboard.h"
#include "SDL_ps5osmesa.h"

#define PS5_SURFACE "_PS5_Surface"

#define PS5_THREAD_COUNT 12

#ifdef SDL_VIDEO_VULKAN
#error "Vulkan is configured, but not implemented for PS5."
#endif
#ifdef SDL_VIDEO_METAL
#error "Metal is configured, but not implemented for PS5."
#endif
#if defined(SDL_VIDEO_OPENGL_ANY) && !defined(SDL_VIDEO_OPENGL_OSMESA)
#error "OpenGL is configured, but not the implemented (OSMESA) for PS5."
#endif

/* Instance */
PS5_DeviceData ps5VideoData;

static void* PS5_DrawTileThread(void* arg) {
    const PS5_DrawChunk* chunk = (PS5_DrawChunk*)arg;

    for (int ind = chunk->src_start; ind < chunk->src_end; ind++) {
        int x = ind % chunk->frame_width;
        int y = ind / chunk->frame_width;
        int ty = y / PS5_TILE_HEIGHT;
        int tx = x / PS5_TILE_WIDTH;

        int t = (int)(PS5_TILE_SIZE * (tx + ty * ((double)chunk->frame_width /
                                                  PS5_TILE_WIDTH)));
        int i = PS5_tilemap[y % PS5_TILE_HEIGHT][x % PS5_TILE_WIDTH];
        chunk->dst[t + i] = chunk->src[ind];
    }
    return 0;
}

static void PS5_DrawPixelsAsTiles(uint32_t *src, uint32_t *dst,
                                  int frame_width, int frame_height)
{
    int chunk_size = frame_width * frame_height / PS5_THREAD_COUNT;
    PS5_DrawChunk chunks[PS5_THREAD_COUNT];
    pthread_t threads[PS5_THREAD_COUNT];

    for (int i=0; i<PS5_THREAD_COUNT; i++) {
        chunks[i].src = src;
        chunks[i].dst = dst;
        chunks[i].src_start = i * chunk_size;
        chunks[i].src_end = (i + 1) * chunk_size;
        chunks[i].frame_width = frame_width;
        chunks[i].frame_height = frame_height;

        if(i == PS5_THREAD_COUNT - 1) {
            chunks[i].src_end = frame_width * frame_height;
        }

        pthread_create(&threads[i], 0, &PS5_DrawTileThread, &chunks[i]);
    }

    for (int i=0; i<PS5_THREAD_COUNT; i++) {
        pthread_join(threads[i], 0);
    }
}

static void PS5_DestroyWindowFramebuffer(SDL_Window *window)
{
    SDL_Surface *surface;

    // surface = (SDL_Surface *)SDL_SetWindowData(window, PS5_SURFACE, NULL);
    surface = window->surface;
    SDL_FreeSurface(surface);
}

static int PS5_CreateWindowFramebuffer(SDL_Window *window,
                                       Uint32 *format, void **pixels,
                                       int *pitch)
{
    const Uint32 surface_format = SDL_PIXELFORMAT_ABGR8888;
    SDL_Surface *surface;
    int w, h;

    /* Free the old framebuffer surface */
    // PS5_DestroyWindowFramebuffer(window);
    SDL_assert(window->surface == NULL);

    SDL_PrivateGetWindowSizeInPixels(window, &w, &h);
    surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 0, surface_format);
    if (!surface) {
        return -1;
    }

    /* Save the info and return! */
    window->surface = surface;
    // SDL_SetWindowData(window, PS5_SURFACE, surface);
    // *format = surface_format;
    // *pixels = surface->pixels;
    // *pitch = surface->pitch;
    return 0;
}

static int PS5_UpdateWindowFramebuffer(SDL_Window *window,
                                       const SDL_Rect *rects, int numrects)
{
    PS5_DeviceData *device_data = &ps5VideoData;
    static uint32_t frame_id = 0;
    uint8_t idx = frame_id % 2;
    SDL_Surface *surface;
    struct kevent evt;
    int junk;

    // surface = SDL_GetWindowSurface(window);
    surface = window->surface;
    if (!surface) {
        return SDL_SetError("Couldn't find surface for window");
    }

    if(surface->w == device_data->surface->w &&
       surface->h == device_data->surface->h) {
        PS5_DrawPixelsAsTiles(surface->pixels, device_data->vbuf[idx].data,
                              surface->w, surface->h);
    } else {
        SDL_BlitSurface(surface, NULL, device_data->surface,
                        &(SDL_Rect){(device_data->surface->w - surface->w) / 2,
                                    (device_data->surface->h - surface->h) / 2,
                                    surface->w, surface->h});
        PS5_DrawPixelsAsTiles(device_data->surface->pixels,
                              device_data->vbuf[idx].data,
                              device_data->surface->w,
                              device_data->surface->h);
    }

    if (sceVideoOutSubmitFlip(device_data->handle, idx, 1, frame_id)) {
        return SDL_SetError("sceVideoOutSubmitFlip: %s", strerror(errno));
    }

    if (sceKernelWaitEqueue(device_data->evt_queue, &evt, 1, &junk, 0)) {
        return SDL_SetError("sceKernelWaitEqueue: %s", strerror(errno));
    }
    frame_id++;

    return 0;
}

/*static void PS5_GetDisplayModes(SDL_VideoDisplay * display)
{
    SDL_DisplayMode mode;

    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_ABGR8888;
    mode.w = 3840;
    mode.h = 2160;
    mode.refresh_rate = 60;

    SDL_AddDisplayMode(display, &display->current_mode);
    //SDL_AddDisplayMode(display, &mode);
}*/

static int PS5_SetDisplayMode(SDL_VideoDisplay * display,
                              SDL_DisplayMode * mode)
{
    PS5_DeviceData *device_data = &ps5VideoData;
    PS5_VideoAttr vattr = {0};

    if(device_data->evt_queue) {
        sceVideoOutDeleteFlipEvent(device_data->evt_queue, device_data->handle);
        sceKernelDeleteEqueue(device_data->evt_queue);
    }

    if(device_data->handle >= 0) {
        sceVideoOutClose(device_data->handle);
    }
    device_data->handle = sceVideoOutOpen(0xff, 0, 0, NULL);

    if (sceKernelCreateEqueue(&device_data->evt_queue, "flip queue")) {
        return SDL_SetError("sceKernelCreateEqueue: %s", strerror(errno));
    }
    if (sceVideoOutAddFlipEvent(device_data->evt_queue, device_data->handle, 0)) {
        return SDL_SetError("sceVideoOutAddFlipEvent: %s", strerror(errno));
    }
    if (sceVideoOutSetFlipRate(device_data->handle, 0)) {
        return SDL_SetError("sceVideoOutSetFlipRate: %s", strerror(errno));
    }

    sceVideoOutSetBufferAttribute2(&vattr, 0x8000000022000000UL, 0,
                                   mode->w, mode->h, 0, 0, 0);

    if (sceVideoOutRegisterBuffers2(device_data->handle, 0, 0,
                                    device_data->vbuf, 2, &vattr, 0, NULL)) {
        return SDL_SetError("sceVideoOutRegisterBuffers2: %s", strerror(errno));
    }

    return 0;
}

static int PS5_VideoInit(_THIS)
{
    PS5_DeviceData *device_data = &ps5VideoData;
    SDL_VideoDisplay display;
    SDL_DisplayMode mode;
    PS5_VideoAttr vattr;
    void *vaddr = 0;
    int result;

    mode.format = SDL_PIXELFORMAT_ABGR8888;
    mode.w = 1920;
    mode.h = 1080;
    mode.refresh_rate = 60;
    mode.driverdata = NULL;

    memset(device_data->vbuf, 0, sizeof(device_data->vbuf));
    memset(&vattr, 0, sizeof(vattr));

    sceSystemServiceHideSplashScreen();
    device_data->handle = sceVideoOutOpen(0xff, 0, 0, NULL);
    if (device_data->handle < 0) {
        return SDL_SetError("sceVideoOutOpen: %s", strerror(errno));
    }
    device_data->memsize = 0x20000000;
    if (sceKernelAllocateMainDirectMemory(device_data->memsize, 0x20000, 3,
                                          &device_data->paddr)) {
        return SDL_SetError("sceKernelAllocateMainDirectMemory: %s",
                            strerror(errno));
    }

    if (sceKernelMapDirectMemory(&vaddr, device_data->memsize, 0x33, 0,
                                 device_data->paddr, 0x20000)) {
        return SDL_SetError("sceKernelMapDirectMemory: %s", strerror(errno));
    }

    device_data->vbuf[0].data = vaddr;
    device_data->vbuf[1].data = vaddr + (device_data->memsize / 2);

    if (sceKernelCreateEqueue(&device_data->evt_queue, "flip queue")) {
        return SDL_SetError("sceKernelCreateEqueue: %s", strerror(errno));
    }

    if (sceVideoOutAddFlipEvent(device_data->evt_queue, device_data->handle, 0)) {
        return SDL_SetError("sceVideoOutAddFlipEvent: %s", strerror(errno));
    }
    if (sceVideoOutSetFlipRate(device_data->handle, 0)) {
        return SDL_SetError("sceVideoOutSetFlipRate: %s", strerror(errno));
    }

    sceVideoOutSetBufferAttribute2(&vattr, 0x8000000022000000UL, 0,
                                   mode.w, mode.h, 0, 0, 0);

    if (sceVideoOutRegisterBuffers2(device_data->handle, 0, 0,
                                    device_data->vbuf, 2, &vattr, 0, NULL)) {
        return SDL_SetError("sceVideoOutRegisterBuffers2: %s", strerror(errno));
    }

    device_data->surface = SDL_CreateRGBSurfaceWithFormat(0, mode.w, mode.h, 32,
                                                          mode.format);
    SDL_zero(display);
    display.desktop_mode = mode;
    display.current_mode = mode;
    // display.driverdata = NULL;

    SDL_AddDisplayMode(&display, &mode);

    result = SDL_AddVideoDisplay(&display, SDL_FALSE);
    if (result < 0) {
        SDL_free(display.display_modes);
    }

    return result;
}

static void PS5_VideoQuit(_THIS)
{
    PS5_DeviceData *device_data = &ps5VideoData;

    if (device_data->handle != 0) {
        sceVideoOutClose(device_data->handle);
        device_data->handle = 0;
    }

    if (device_data->paddr) {
        sceKernelReleaseDirectMemory(device_data->paddr, device_data->memsize);
        device_data->paddr = 0;
        device_data->memsize = 0;
    }
    if (device_data->evt_queue) {
        sceKernelDeleteEqueue(device_data->evt_queue);
    }
}

static int PS5_CreateSDLWindow(_THIS, SDL_Window *window)
{
    return 0;
}

static void PS5_DeleteDevice(_THIS)
{
    SDL_zero(ps5VideoData);
}

static void PS5_DestroyWindow(SDL_Window *window)
{
}

static void PS5_PumpEvents(void)
{
    PS5_Keyboard_PumpEvents();
}

static SDL_bool PS5_CreateDevice(SDL_VideoDevice *device)
{
    PS5_Keyboard_Init();
    PS5_Keyboard_Open();

    /* Set the function pointers */
    /* Initialization/Query functions */
    device->VideoInit = PS5_VideoInit;
    device->VideoQuit = PS5_VideoQuit;
    // device->GetDisplayBounds = PS5_GetDisplayBounds;
    // device->GetDisplayUsableBounds = PS5_GetDisplayUsableBounds;
    // device->GetDisplayDPI = PS5_GetDisplayDPI;
    device->SetDisplayMode = PS5_SetDisplayMode;

    /* Window functions */
    device->CreateSDLWindow = PS5_CreateSDLWindow;
    // device->CreateSDLWindowFrom = PS5_CreateSDLWindowFrom;
    // device->SetWindowTitle = PS5_SetWindowTitle;
    // device->SetWindowIcon = PS5_SetWindowIcon;
    // device->SetWindowPosition = PS5_SetWindowPosition;
    // device->SetWindowSize = PS5_SetWindowSize;
    // device->SetWindowMinimumSize = PS5_SetWindowMinimumSize;
    // device->SetWindowMaximumSize = PS5_SetWindowMaximumSize;
    // device->GetWindowBordersSize = PS5_GetWindowBordersSize;
    // device->GetWindowSizeInPixels = PS5_GetWindowSizeInPixels;
    // device->SetWindowOpacity = PS5_SetWindowOpacity;
    // device->SetWindowModalFor = PS5_SetWindowModalFor;
    // device->SetWindowInputFocus = PS5_SetWindowInputFocus;
    // device->ShowWindow = PS5_ShowWindow;
    // device->HideWindow = PS5_HideWindow;
    // device->RaiseWindow = PS5_RaiseWindow;
    // device->MaximizeWindow = PS5_MaximizeWindow;
    // device->MinimizeWindow = PS5_MinimizeWindow;
    // device->RestoreWindow = PS5_RestoreWindow;
    // device->SetWindowBordered = PS5_SetWindowBordered;
    // device->SetWindowResizable = PS5_SetWindowResizable;
    // device->SetWindowAlwaysOnTop = PS5_SetWindowAlwaysOnTop;
    // device->SetWindowFullscreen = PS5_SetWindowFullscreen;
    // device->SetWindowGammaRamp = PS5_SetWindowGammaRamp;
    // device->GetWindowGammaRamp = PS5_GetWindowGammaRamp;
    // device->GetWindowICCProfile = PS5_GetWindowICCProfile;
    // device->GetWindowDisplayIndex = PS5_GetWindowDisplayIndex;
    // device->SetWindowMouseRect = PS5_SetWindowMouseRect;
    // device->SetWindowMouseGrab = PS5_SetWindowMouseGrab;
    // device->SetWindowKeyboardGrab = PS5_SetWindowKeyboardGrab;
    device->DestroyWindow = PS5_DestroyWindow;
    // * Framebuffer disabled, causes issues on high-framerate updates. SDL still emulates this.
    device->CreateWindowFramebuffer = PS5_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = PS5_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = PS5_DestroyWindowFramebuffer;
    // device->OnWindowEnter = PS5_OnWindowEnter;
    // device->FlashWindow = PS5_FlashWindow;
    /* Shaped-window functions */
    // device->CreateShaper = PS5_CreateShaper;
    // device->SetWindowShape = PS5_SetWindowShape;
    /* Get some platform dependent window information */
    // device->GetWindowWMInfo = PS5_GetWindowWMInfo;

    /* OpenGL support */
#ifdef SDL_VIDEO_OPENGL_OSMESA
    PS5_OSMesa_InitDevice(device);
    // device->GL_LoadLibrary = PS5_GL_LoadLibrary;
    // device->GL_GetProcAddress = PS5_GL_GetProcAddress;
    // device->GL_CreateContext = PS5_GL_CreateContext;
    // device->GL_UnloadLibrary = PS5_GLES_UnloadLibrary;
    // device->GL_MakeCurrent = PS5_GLES_MakeCurrent;
    // device->GL_GetDrawableSize = PS5_GLES_GetDrawableSize;
    // device->GL_SetSwapInterval = PS5_GLES_SetSwapInterval;
    // device->GL_GetSwapInterval = PS5_GLES_GetSwapInterval;
    // device->GL_SwapWindow = PS5_GLES_SwapWindow;
    // device->GL_DeleteContext = PS5_GLES_DeleteContext;
#endif

    /* Vulkan support */
#ifdef SDL_VIDEO_VULKAN
    // device->Vulkan_LoadLibrary = PS5_Vulkan_LoadLibrary;
    // device->Vulkan_UnloadLibrary = PS5_Vulkan_UnloadLibrary;
    // device->Vulkan_GetInstanceExtensions = PS5_Vulkan_GetInstanceExtensions;
    // device->Vulkan_CreateSurface = PS5_Vulkan_CreateSurface;
    // device->Vulkan_GetDrawableSize = PS5_Vulkan_GetDrawableSize;
#endif

    /* Metal support */
#ifdef SDL_VIDEO_METAL
    // device->Metal_CreateView = PS5_Metal_CreateView;
    // device->Metal_DestroyView = PS5_Metal_DestroyView;
    // device->Metal_GetLayer = PS5_Metal_GetLayer;
    // device->Metal_GetDrawableSize = PS5_Metal_GetDrawableSize;
#endif

    /* Event manager functions */
    // device->WaitEventTimeout = PS5_WaitEventTimeout;
    // device->SendWakeupEvent = PS5_SendWakeupEvent;
    device->PumpEvents = PS5_PumpEvents;

    /* Screensaver */
    // device->SuspendScreenSaver = PS5_SuspendScreenSaver;

    /* Text input */
    // device->StartTextInput = PS5_StartTextInput;
    // device->StopTextInput = PS5_StopTextInput;
    // device->SetTextInputRect = PS5_SetTextInputRect;
    // device->ClearComposition = PS5_ClearComposition;
    // device->IsTextInputShown = PS5_IsTextInputShown;

    /* Screen keyboard */
    device->HasScreenKeyboardSupport = PS5_HasScreenKeyboardSupport;
    device->ShowScreenKeyboard = PS5_ShowScreenKeyboard;
    device->HideScreenKeyboard = PS5_HideScreenKeyboard;
    device->IsScreenKeyboardShown = PS5_IsScreenKeyboardShown;

    /* Clipboard */
    // device->SetClipboardText = PS5_SetClipboardText;
    // device->GetClipboardText = PS5_GetClipboardText;
    // device->SetPrimarySelectionText = PS5_SetPrimarySelectionText;
    // device->GetPrimarySelectionText = PS5_GetPrimarySelectionText;

    /* Hit-testing */
    // device->SetWindowHitTest = PS5_SetWindowHitTest;

    /* Tell window that app enabled drag'n'drop events */
    // device->AcceptDragAndDrop = PS5_AcceptDragAndDrop;

    device->DeleteDevice = PS5_DeleteDevice;

    return SDL_TRUE;
}

const VideoBootStrap PS5_bootstrap = { "ps5", PS5_CreateDevice };

#endif /* SDL_VIDEO_DRIVER_PS5 */

/* vi: set ts=4 sw=4 expandtab: */

/* emacs: */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* indent-tabs-mode: nil */
/* End: */
