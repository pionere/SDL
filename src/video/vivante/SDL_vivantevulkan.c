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

/*
 * @author Wladimir J. van der Laan. Based on Jacob Lifshay's
 * SDL_x11vulkan.c, Mark Callow's SDL_androidvulkan.c, and
 * the FSL demo framework.
 */

#include "../../SDL_internal.h"

#if defined(SDL_VIDEO_VULKAN) && defined(SDL_VIDEO_DRIVER_VIVANTE)

#include "SDL_vivantevideo.h"

#include "SDL_loadso.h"
#include "SDL_vivantevulkan.h"
#include "SDL_syswm.h"

int VIVANTE_Vulkan_LoadLibrary(SDL_VulkanVideo *vulkan_config, const char *path)
{
    void *loader_handle;
    VkExtensionProperties *extensions = NULL;
    Uint32 i, extensionCount = 0;
    SDL_bool hasSurfaceExtension = SDL_FALSE;
    SDL_bool hasDisplayExtension = SDL_FALSE;
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = NULL;
    PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;

    SDL_assert(vulkan_config->loader_handle == NULL);

    /* Load the Vulkan loader library */
    if (!path) {
        path = SDL_getenv("SDL_VULKAN_LIBRARY");
    }
    if (!path) {
        /* If no path set, try Vivante fb vulkan driver explicitly */
        path = "libvulkan-fb.so";
        loader_handle = SDL_LoadObject(path);
        if (!loader_handle) {
            /* If that couldn't be loaded, fall back to default name */
            path = "libvulkan.so";
            loader_handle = SDL_LoadObject(path);
        }
    } else {
        loader_handle = SDL_LoadObject(path);
    }
    if (!loader_handle) {
        return -1;
    }
    vulkan_config->loader_handle = loader_handle;

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "vivante: Loaded vulkan driver %s", path);
    vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_LoadFunction(
        vulkan_config->loader_handle, "vkGetInstanceProcAddr");
    if (!vkGetInstanceProcAddr) {
        goto fail;
    }
    vulkan_config->vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vkEnumerateInstanceExtensionProperties =
        (PFN_vkEnumerateInstanceExtensionProperties)vulkan_config->vkGetInstanceProcAddr(
            VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
    if (!vkEnumerateInstanceExtensionProperties) {
        goto fail;
    }
    extensions = SDL_Vulkan_CreateInstanceExtensionsList(
        vkEnumerateInstanceExtensionProperties,
        &extensionCount);
    if (!extensions) {
        goto fail;
    }
    for (i = 0; i < extensionCount; i++) {
        if (SDL_strcmp(VK_KHR_SURFACE_EXTENSION_NAME, extensions[i].extensionName) == 0) {
            hasSurfaceExtension = SDL_TRUE;
        } else if (SDL_strcmp(VK_KHR_DISPLAY_EXTENSION_NAME, extensions[i].extensionName) == 0) {
            hasDisplayExtension = SDL_TRUE;
        }
    }
    SDL_free(extensions);
    if (!hasSurfaceExtension) {
        SDL_SetError("Installed Vulkan doesn't implement the " VK_KHR_SURFACE_EXTENSION_NAME " extension");
        goto fail;
    } else if (!hasDisplayExtension) {
        SDL_SetError("Installed Vulkan doesn't implement the " VK_KHR_DISPLAY_EXTENSION_NAME "extension");
        goto fail;
    }
    return 0;

fail:
    VIVANTE_Vulkan_UnloadLibrary(vulkan_config);
    return -1;
}

void VIVANTE_Vulkan_UnloadLibrary(SDL_VulkanVideo *vulkan_config)
{
    SDL_UnloadObject(vulkan_config->loader_handle);
    vulkan_config->loader_handle = NULL;
}

SDL_bool VIVANTE_Vulkan_GetInstanceExtensions(SDL_Window *window,
                                              unsigned *count,
                                              const char **names)
{
    static const char *const extensionsForVivante[] = {
        VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_DISPLAY_EXTENSION_NAME
    };
    return SDL_Vulkan_GetInstanceExtensions_Helper(
        count, names, SDL_arraysize(extensionsForVivante),
        extensionsForVivante);
}

SDL_bool VIVANTE_Vulkan_CreateSurface(SDL_VulkanVideo *vulkan_config,
                                      SDL_Window *window,
                                      VkInstance instance,
                                      VkSurfaceKHR *surface)
{
    SDL_assert(vulkan_config->loader_handle != NULL);

    return SDL_Vulkan_Display_CreateSurface(vulkan_config->vkGetInstanceProcAddr, instance, surface);
}

#endif

/* vi: set ts=4 sw=4 expandtab: */
