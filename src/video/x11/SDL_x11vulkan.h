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

#ifndef SDL_x11vulkan_h_
#define SDL_x11vulkan_h_

#if defined(SDL_VIDEO_VULKAN) && defined(SDL_VIDEO_DRIVER_X11)

#include "../SDL_vulkan_internal.h"
#include "../SDL_sysvideo.h"

/*typedef struct xcb_connection_t xcb_connection_t;*/
typedef xcb_connection_t *(*PFN_XGetXCBConnection)(Display *dpy);

#define X11_Vulkan_GetDrawableSize SDL_PrivateGetWindowSizeInPixels

int X11_Vulkan_LoadLibrary(SDL_VulkanVideo *vulkan_config, const char *path);
void X11_Vulkan_UnloadLibrary(SDL_VulkanVideo *vulkan_config);
SDL_bool X11_Vulkan_GetInstanceExtensions(SDL_Window *window,
                                          unsigned *count,
                                          const char **names);
SDL_bool X11_Vulkan_CreateSurface(SDL_VulkanVideo *vulkan_config,
                                  SDL_Window *window,
                                  VkInstance instance,
                                  VkSurfaceKHR *surface);

#endif

#endif /* SDL_x11vulkan_h_ */

/* vi: set ts=4 sw=4 expandtab: */
