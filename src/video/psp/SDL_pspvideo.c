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

#ifdef SDL_VIDEO_DRIVER_PSP

/* SDL internals */
#include "../SDL_sysvideo.h"
#include "SDL_version.h"
#include "SDL_syswm.h"
#include "SDL_loadso.h"
#include "SDL_events.h"
#include "SDL_render.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../render/SDL_sysrender.h"


/* PSP declarations */
#include "SDL_pspmessagebox.h"
#include "SDL_pspvideo.h"
#include "SDL_pspevents_c.h"
#include "SDL_pspgl_c.h"

#include <psputility.h>
#include <pspgu.h>
#include <pspdisplay.h>
#include <vram.h>
#include "../../render/psp/SDL_render_psp.h"

#ifdef SDL_VIDEO_VULKAN
#error "Vulkan is configured, but not implemented for psp."
#endif
#if defined(SDL_VIDEO_OPENGL_ANY) && !defined(SDL_VIDEO_OPENGL)
#error "OpenGL is configured, but not the implemented (GL) for psp."
#endif

int PSP_CreateWindowFramebuffer(SDL_Window *window, Uint32 *format, void **pixels, int *pitch)
{
#ifndef SDL_RENDER_DISABLED
    void *buffer_pixels;
    int buffer_pitch;
    int bytes_per_pixel;
    int w, h;
    Uint32 texture_format;
    SDL_Renderer *renderer = SDL_GetRenderer(window);

    SDL_PrivateGetWindowSizeInPixels(window, &w, &h);
    if (w != PSP_SCREEN_WIDTH) {
        return SDL_SetError("Unexpected window size");
    }

    if (renderer) {
        if (SDL_strcmp(renderer->info.name, "PSP") != 0) {
            return SDL_SetError("Non-PSP Renderer already associated with window");
        }
    } else {
        renderer = SDL_CreateRenderer(window, SDL_RENDERDRIVER_PSP, 0);
        if (!renderer) {
            return -1;
        }
    }

    /* Find the first format without an alpha channel */
    SDL_INLINE_COMPILE_TIME_ASSERT(framebuffer_format, !SDL_ISPIXELFORMAT_FOURCC(SDL_PIXELFORMAT_UNKNOWN));
    SDL_INLINE_COMPILE_TIME_ASSERT(framebuffer_format, !SDL_ISPIXELFORMAT_ALPHA(SDL_PIXELFORMAT_UNKNOWN));
    texture_format = GetClosestSupportedFormat(renderer, SDL_PIXELFORMAT_UNKNOWN);
    /*info = SDL_PrivateGetRendererInfo(renderer);
    texture_format = info->texture_formats[0];

    for (i = 0; i < (int)info->num_texture_formats; ++i) {
#if SDL_HAVE_YUV
        if (SDL_ISPIXELFORMAT_FOURCC(info->texture_formats[i]))
            continue;
#else
        SDL_assert(!SDL_ISPIXELFORMAT_FOURCC(info->texture_formats[i]));
#endif
        if (!SDL_ISPIXELFORMAT_ALPHA(info->texture_formats[i])) {
            texture_format = info->texture_formats[i];
            break;
        }
    }*/

    /* get the PSP renderer's "private" data */
    SDL_PSP_RenderGetProp(renderer, SDL_PSP_RENDERPROPS_FRONTBUFFER, &buffer_pixels);

    /* Create framebuffer data */
    SDL_assert(!SDL_ISPIXELFORMAT_FOURCC(texture_format));
    bytes_per_pixel = SDL_PIXELBPP(texture_format);
    /* since we point directly to VRAM's frontbuffer, we have to use
       the upscaled pitch of 512 width - since PSP requires all textures to be
       powers of 2. */
    buffer_pitch = 512 * bytes_per_pixel;

    renderer->info.flags |= SDL_RENDERER_DONTFREE;

    /* Make sure we're not double-scaling the viewport */
    SDL_RenderSetViewport(renderer, NULL);

    *format = texture_format;
    *pixels = buffer_pixels;
    *pitch = buffer_pitch;
    return 0;
#else
    return SDL_SetError("No hardware accelerated renderers available");
#endif // SDL_RENDER_DISABLED
}

int PSP_UpdateWindowFramebuffer(SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_Surface *surface = window->surface;
    SDL_Renderer *renderer = SDL_GetRenderer(window);

    SDL_assert(surface != NULL);
    SDL_assert(renderer != NULL);

    renderer->RenderPresent(renderer);
    SDL_PSP_RenderGetProp(renderer, SDL_PSP_RENDERPROPS_BACKBUFFER, &surface->pixels);
    return 0;
}

void PSP_DestroyWindowFramebuffer(SDL_Window *window)
{
    SDL_Renderer *renderer = SDL_GetRenderer(window);
    if (renderer && (renderer->info.flags & SDL_RENDERER_DONTFREE)) {
        renderer->info.flags &= ~SDL_RENDERER_DONTFREE;
        SDL_DestroyRenderer(renderer);
    }
}

#ifdef SDL_VIDEO_OPENGL
/* Instance */
Psp_VideoData pspVideoData; 
#endif

static void PSP_DeleteDevice(_THIS)
{
#ifdef SDL_VIDEO_OPENGL
    SDL_zero(pspVideoData);
#endif
}

static SDL_bool PSP_CreateDevice(SDL_VideoDevice *device)
{
    /* Set the function pointers */
    /* Initialization/Query functions */
    device->VideoInit = PSP_VideoInit;
    device->VideoQuit = PSP_VideoQuit;
    // device->GetDisplayBounds = PSP_GetDisplayBounds;
    // device->GetDisplayUsableBounds = PSP_GetDisplayUsableBounds;
    // device->GetDisplayDPI = PSP_GetDisplayDPI;
    device->SetDisplayMode = PSP_SetDisplayMode;

    /* Window functions */
    device->CreateSDLWindow = PSP_CreateSDLWindow;
    // device->CreateSDLWindowFrom = PSP_CreateSDLWindowFrom;
    // device->SetWindowTitle = PSP_SetWindowTitle;
    // device->SetWindowIcon = PSP_SetWindowIcon;
    // device->SetWindowPosition = PSP_SetWindowPosition;
    // device->SetWindowSize = PSP_SetWindowSize;
    // device->SetWindowMinimumSize = PSP_SetWindowMinimumSize;
    // device->SetWindowMaximumSize = PSP_SetWindowMaximumSize;
    // device->GetWindowBordersSize = PSP_GetWindowBordersSize;
    // device->GetWindowSizeInPixels = PSP_GetWindowSizeInPixels;
    // device->SetWindowOpacity = PSP_SetWindowOpacity;
    // device->SetWindowModalFor = PSP_SetWindowModalFor;
    // device->SetWindowInputFocus = PSP_SetWindowInputFocus;
    // device->ShowWindow = PSP_ShowWindow;
    // device->HideWindow = PSP_HideWindow;
    // device->RaiseWindow = PSP_RaiseWindow;
    // device->MaximizeWindow = PSP_MaximizeWindow;
    device->MinimizeWindow = PSP_MinimizeWindow;
    // device->RestoreWindow = PSP_RestoreWindow;
    // device->SetWindowBordered = PSP_SetWindowBordered;
    // device->SetWindowResizable = PSP_SetWindowResizable;
    // device->SetWindowAlwaysOnTop = PSP_SetWindowAlwaysOnTop;
    // device->SetWindowFullscreen = PSP_SetWindowFullscreen;
    // device->SetWindowGammaRamp = PSP_SetWindowGammaRamp;
    // device->GetWindowGammaRamp = PSP_GetWindowGammaRamp;
    // device->GetWindowICCProfile = PSP_GetWindowICCProfile;
    // device->GetWindowDisplayIndex = PSP_GetWindowDisplayIndex;
    // device->SetWindowMouseRect = PSP_SetWindowMouseRect;
    // device->SetWindowMouseGrab = PSP_SetWindowMouseGrab;
    // device->SetWindowKeyboardGrab = PSP_SetWindowKeyboardGrab;
    device->DestroyWindow = PSP_DestroyWindow;
    /* backend to use VRAM directly as a framebuffer using
       SDL_GetWindowSurface() API. */
    device->CreateWindowFramebuffer = PSP_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = PSP_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = PSP_DestroyWindowFramebuffer;
    // device->OnWindowEnter = PSP_OnWindowEnter;
    // device->FlashWindow = PSP_FlashWindow;
    /* Shaped-window functions */
    // device->CreateShaper = PSP_CreateShaper;
    // device->SetWindowShape = PSP_SetWindowShape;
#if 0
    /* Get some platform dependent window information */
    device->GetWindowWMInfo = PSP_GetWindowWMInfo;
#endif


    /* OpenGL support */
#ifdef SDL_VIDEO_OPENGL
    device->GL_LoadLibrary = PSP_GL_LoadLibrary;
    device->GL_GetProcAddress = PSP_GL_GetProcAddress;
    device->GL_UnloadLibrary = PSP_GL_UnloadLibrary;
    device->GL_CreateContext = PSP_GL_CreateContext;
    device->GL_MakeCurrent = PSP_GL_MakeCurrent;
    device->GL_GetDrawableSize = PSP_GL_GetDrawableSize;
    device->GL_SetSwapInterval = PSP_GL_SetSwapInterval;
    device->GL_GetSwapInterval = PSP_GL_GetSwapInterval;
    device->GL_SwapWindow = PSP_GL_SwapWindow;
    device->GL_DeleteContext = PSP_GL_DeleteContext;
#endif

    /* Vulkan support */
#ifdef SDL_VIDEO_VULKAN
    // device->Vulkan_LoadLibrary = PSP_Vulkan_LoadLibrary;
    // device->Vulkan_UnloadLibrary = PSP_Vulkan_UnloadLibrary;
    // device->Vulkan_GetInstanceExtensions = PSP_Vulkan_GetInstanceExtensions;
    // device->Vulkan_CreateSurface = PSP_Vulkan_CreateSurface;
    // device->Vulkan_GetDrawableSize = PSP_Vulkan_GetDrawableSize;
#endif

    /* Metal support */
#ifdef SDL_VIDEO_METAL
    // device->Metal_CreateView = PSP_Metal_CreateView;
    // device->Metal_DestroyView = PSP_Metal_DestroyView;
    // device->Metal_GetLayer = PSP_Metal_GetLayer;
    // device->Metal_GetDrawableSize = PSP_Metal_GetDrawableSize;
#endif

    /* Event manager functions */
    // device->WaitEventTimeout = PSP_WaitEventTimeout;
    // device->SendWakeupEvent = PSP_SendWakeupEvent;
    device->PumpEvents = PSP_PumpEvents;

    /* Screensaver */
    // device->SuspendScreenSaver = PSP_SuspendScreenSaver;

    /* Text input */
    // device->StartTextInput = PSP_StartTextInput;
    // device->StopTextInput = PSP_StopTextInput;
    // device->SetTextInputRect = PSP_SetTextInputRect;
    // device->ClearComposition = PSP_ClearComposition;
    // device->IsTextInputShown = PSP_IsTextInputShown;

    /* Screen keyboard */
    // device->HasScreenKeyboardSupport = PSP_HasScreenKeyboardSupport;
    // device->ShowScreenKeyboard = PSP_ShowScreenKeyboard;
    // device->HideScreenKeyboard = PSP_HideScreenKeyboard;
    // device->IsScreenKeyboardShown = PSP_IsScreenKeyboardShown;

    /* Clipboard */
    // device->SetClipboardText = PSP_SetClipboardText;
    // device->GetClipboardText = PSP_GetClipboardText;
    // device->HasClipboardText = PSP_HasClipboardText;
    // device->SetPrimarySelectionText = PSP_SetPrimarySelectionText;
    // device->GetPrimarySelectionText = PSP_GetPrimarySelectionText;
    // device->HasPrimarySelectionText = PSP_HasPrimarySelectionText;

    /* Hit-testing */
    // device->SetWindowHitTest = PSP_SetWindowHitTest;

    /* Tell window that app enabled drag'n'drop events */
    // device->AcceptDragAndDrop = PSP_AcceptDragAndDrop;

    device->DeleteDevice = PSP_DeleteDevice;

    return SDL_TRUE;
}

static void configure_dialog(pspUtilityMsgDialogParams *dialog, size_t dialog_size)
{
	/* clear structure and setup size */
	SDL_memset(dialog, 0, dialog_size);
	dialog->base.size = dialog_size;

	/* set language */
	sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &dialog->base.language);

	/* set X/O swap */
	sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN, &dialog->base.buttonSwap);

	/* set thread priorities */
	/* TODO: understand how these work */
	dialog->base.soundThread = 0x10;
	dialog->base.graphicsThread = 0x11;
	dialog->base.fontThread = 0x12;
	dialog->base.accessThread = 0x13;
}

static void *setup_temporal_gu(void *list)
{
    // Using GU_PSM_8888 for the framebuffer
	int bpp = 4;
	
	void *doublebuffer = vramalloc(PSP_FRAME_BUFFER_SIZE * bpp * 2);
    void *backbuffer = doublebuffer;
    void *frontbuffer = ((uint8_t *)doublebuffer) + PSP_FRAME_BUFFER_SIZE * bpp;

    sceGuInit();

    sceGuStart(GU_DIRECT,list);
	sceGuDrawBuffer(GU_PSM_8888, vrelptr(frontbuffer), PSP_FRAME_BUFFER_WIDTH);
    sceGuDispBuffer(PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, vrelptr(backbuffer), PSP_FRAME_BUFFER_WIDTH);

    sceGuOffset(2048 - (PSP_SCREEN_WIDTH >> 1), 2048 - (PSP_SCREEN_HEIGHT >> 1));
    sceGuViewport(2048, 2048, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);

    sceGuDisable(GU_DEPTH_TEST);

    /* Scissoring */
    sceGuScissor(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);

    sceGuFinish();
    sceGuSync(0,0);
    
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

	return doublebuffer;
}

static void term_temporal_gu(void *guBuffer)
{
	sceGuTerm();
	vfree(guBuffer);
	sceDisplayWaitVblankStart();
}

int PSP_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonid)
{
	unsigned char list[64] __attribute__((aligned(64)));
	pspUtilityMsgDialogParams dialog;
	int status;
	void *guBuffer = NULL;

	/* check if it's possible to use existing video context */
	if (SDL_GetFocusWindow() == NULL) {
		guBuffer = setup_temporal_gu(list);
	}

	/* configure dialog */
	configure_dialog(&dialog, sizeof(dialog));

	/* setup dialog options for text */
	dialog.mode = PSP_UTILITY_MSGDIALOG_MODE_TEXT;
	dialog.options = PSP_UTILITY_MSGDIALOG_OPTION_TEXT;

	/* copy the message in, 512 bytes max */
	SDL_snprintf(dialog.message, sizeof(dialog.message), "%s\r\n\r\n%s", messageboxdata->title, messageboxdata->message);

	/* too many buttons */
	if (messageboxdata->numbuttons > 2)
		return SDL_SetError("messageboxdata->numbuttons valid values are 0, 1, 2");

	/* we only have two options, "yes/no" or "ok" */
	if (messageboxdata->numbuttons == 2)
		dialog.options |= PSP_UTILITY_MSGDIALOG_OPTION_YESNO_BUTTONS | PSP_UTILITY_MSGDIALOG_OPTION_DEFAULT_NO;

	/* start dialog */
	if (sceUtilityMsgDialogInitStart(&dialog) != 0)
		return SDL_SetError("sceUtilityMsgDialogInitStart() failed for some reason");

	/* loop while the dialog is active */
	status = PSP_UTILITY_DIALOG_NONE;
	do
	{
		sceGuStart(GU_DIRECT, list);
		sceGuClearColor(0);
		sceGuClearDepth(0);
		sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
		sceGuFinish();
		sceGuSync(0,0);

		status = sceUtilityMsgDialogGetStatus();

		switch (status)
		{
			case PSP_UTILITY_DIALOG_VISIBLE:
				sceUtilityMsgDialogUpdate(1);
				break;

			case PSP_UTILITY_DIALOG_QUIT:
				sceUtilityMsgDialogShutdownStart();
				break;
		}

		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();

	} while (status != PSP_UTILITY_DIALOG_NONE);

	/* cleanup */
	if (guBuffer)
	{
		term_temporal_gu(guBuffer);
	}

	/* success */
	if (dialog.buttonPressed == PSP_UTILITY_MSGDIALOG_RESULT_YES)
		*buttonid = messageboxdata->buttons[0].buttonid;
	else if (dialog.buttonPressed == PSP_UTILITY_MSGDIALOG_RESULT_NO)
		*buttonid = messageboxdata->buttons[1].buttonid;
	else
		*buttonid = messageboxdata->buttons[0].buttonid;

	return 0;
}

/* "PSP Video Driver" */
const VideoBootStrap PSP_bootstrap = {
    "PSP", PSP_CreateDevice
};

/*****************************************************************************/
/* SDL Video and Display initialization/handling functions                   */
/*****************************************************************************/
int PSP_VideoInit(_THIS)
{
    int result;
    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;

    if (PSP_EventInit() < 0) {
        return -1;  /* error string would already be set */
    }

    /* 32 bpp for default */
    current_mode.format = SDL_PIXELFORMAT_ABGR8888;
    current_mode.w = PSP_SCREEN_WIDTH;
    current_mode.h = PSP_SCREEN_HEIGHT;
    current_mode.refresh_rate = 60;
    current_mode.driverdata = NULL;

    SDL_zero(display);
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;
    // display.driverdata = NULL;

    SDL_AddDisplayMode(&display, &current_mode);
    /* 16 bpp secondary mode */
    current_mode.format = SDL_PIXELFORMAT_BGR565;
    // display.desktop_mode = current_mode;
    // display.current_mode = current_mode;
    SDL_AddDisplayMode(&display, &current_mode);
    result = SDL_AddVideoDisplay(&display, SDL_FALSE);
    if (result < 0) {
        SDL_free(display.display_modes);
    }

    return result;
}

void PSP_VideoQuit(_THIS)
{
    PSP_EventQuit();
}

int PSP_SetDisplayMode(SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    return 0;
}
#define EGLCHK(stmt)                           \
    do {                                       \
        EGLint err;                            \
                                               \
        stmt;                                  \
        err = eglGetError();                   \
        if (err != EGL_SUCCESS) {              \
            SDL_SetError("EGL error %d", err); \
            return 0;                          \
        }                                      \
    } while (0)

int PSP_CreateSDLWindow(_THIS, SDL_Window *window)
{
    if (window->flags & SDL_WINDOW_OPENGL) {
#ifdef SDL_VIDEO_OPENGL
        SDL_WindowData *data;

        /* Allocate the window data */
        data = (SDL_WindowData *)SDL_calloc(1, sizeof(*data));
        if (!data) {
            return SDL_OutOfMemory();
        }
        // data->egl_surface = PSP_GL_CreateSurface(_this, (NativeWindowType)windowdata->hwnd);

        // if (data->egl_surface == EGL_NO_SURFACE) {
        //    return -1;
        // }
        window->driverdata = data;
#else
        return SDL_SetError("Could not create GL window (GL support not configured)");
#endif
    }

    SDL_SetKeyboardFocus(window);

    /* Window has been successfully created */
    return 0;
}

/*int PSP_CreateSDLWindowFrom(_THIS, SDL_Window *window, const void *data)
{
    return SDL_Unsupported();
}

void PSP_SetWindowTitle(SDL_Window *window)
{
}
void PSP_SetWindowIcon(SDL_Window *window, SDL_Surface *icon)
{
}
void PSP_SetWindowPosition(SDL_Window *window)
{
}
void PSP_SetWindowSize(SDL_Window *window)
{
}
void PSP_ShowWindow(SDL_Window *window)
{
}
void PSP_HideWindow(SDL_Window *window)
{
}
void PSP_RaiseWindow(SDL_Window *window)
{
}
void PSP_MaximizeWindow(SDL_Window *window)
{
}*/
void PSP_MinimizeWindow(SDL_Window *window)
{
}
/*void PSP_RestoreWindow(SDL_Window *window)
{
}*/
void PSP_DestroyWindow(SDL_Window *window)
{
#ifdef SDL_VIDEO_OPENGL
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    if (data) {
        PSP_GL_DestroySurface(data->egl_surface);
        SDL_free(data);
        window->driverdata = NULL;
    }
#endif
}

/*****************************************************************************/
/* SDL Window Manager function                                               */
/*****************************************************************************/
#if 0
SDL_bool PSP_GetWindowWMInfo(SDL_Window * window, struct SDL_SysWMinfo *info)
{
    if (info->version.major <= SDL_MAJOR_VERSION) {
        return SDL_TRUE;
    } else {
        SDL_SetError("Application not compiled with SDL %d",
                     SDL_MAJOR_VERSION);
        return SDL_FALSE;
    }

    /* Failed to get window manager information */
    return SDL_FALSE;
}
#endif



SDL_bool PSP_HasScreenKeyboardSupport(void)
{
    return SDL_TRUE;
}

void PSP_ShowScreenKeyboard(SDL_Window *window)
{
    char list[0x20000] __attribute__((aligned(64)));  // Needed for sceGuStart to work
    int i;
    int done = 0;
    int input_text_length = 32; // SDL_SendKeyboardText supports up to 32 characters per event
    unsigned short outtext[input_text_length];
    char text_string[input_text_length];

    SceUtilityOskData data;
    SceUtilityOskParams params;

    SDL_memset(outtext, 0, input_text_length * sizeof(unsigned short));

    data.language = PSP_UTILITY_OSK_LANGUAGE_DEFAULT;
    data.lines = 1;
    data.unk_24 = 1;
    data.inputtype = PSP_UTILITY_OSK_INPUTTYPE_ALL;
    data.desc = NULL;
    data.intext = NULL;
    data.outtextlength = input_text_length;
    data.outtextlimit = input_text_length;
    data.outtext = outtext;

    params.base.size = sizeof(params);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &params.base.language);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN, &params.base.buttonSwap);
    params.base.graphicsThread = 17;
    params.base.accessThread = 19;
    params.base.fontThread = 18;
    params.base.soundThread = 16;
    params.datacount = 1;
    params.data = &data;

    sceUtilityOskInitStart(&params);

    while(!done) {
        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(0);
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
        sceGuFinish();
        sceGuSync(0,0);

        switch(sceUtilityOskGetStatus())
        {
            case PSP_UTILITY_DIALOG_VISIBLE:
                sceUtilityOskUpdate(1);
                break;
            case PSP_UTILITY_DIALOG_QUIT:
                sceUtilityOskShutdownStart();
                break;
            case PSP_UTILITY_DIALOG_NONE:
                done = 1;
                break;
            default :
                break;
        }
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    // Convert input list to string
    for (i = 0; i < input_text_length; i++) {
        text_string[i] = outtext[i];
    }
    SDL_SendKeyboardText((const char *) text_string);
}
void PSP_HideScreenKeyboard(SDL_Window *window)
{
}
SDL_bool PSP_IsScreenKeyboardShown(SDL_Window *window)
{
    return SDL_FALSE;
}*/

#endif /* SDL_VIDEO_DRIVER_PSP */

/* vi: set ts=4 sw=4 expandtab: */
