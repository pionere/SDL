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

#if defined(SDL_VIDEO_VULKAN) && defined(SDL_VIDEO_DRIVER_DIRECTFB)

#include "SDL_DirectFB_window.h"

#include "SDL_loadso.h"
#include "SDL_DirectFB_vulkan.h"

int DirectFB_Vulkan_LoadLibrary(SDL_VulkanVideo *vulkan_config, const char *path)
{
    VkExtensionProperties *extensions = NULL;
    Uint32 i, extensionCount = 0;
    SDL_bool hasSurfaceExtension = SDL_FALSE;
    SDL_bool hasDirectFBSurfaceExtension = SDL_FALSE;
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = NULL;
    PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;

    SDL_assert(vulkan_config->loader_handle == NULL);

    /* Load the Vulkan loader library */
    if (!path)
        path = SDL_getenv("SDL_VULKAN_LIBRARY");
    if (!path)
        path = "libvulkan.so.1";
    vulkan_config->loader_handle = SDL_LoadObject(path);
    if (!vulkan_config->loader_handle)
        return -1;
    vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_LoadFunction(
        vulkan_config->loader_handle, "vkGetInstanceProcAddr");
    if (!vkGetInstanceProcAddr)
        goto fail;
    vulkan_config->vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vkEnumerateInstanceExtensionProperties =
        (PFN_vkEnumerateInstanceExtensionProperties)vulkan_config->vkGetInstanceProcAddr(
            VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
    if (!vkEnumerateInstanceExtensionProperties)
        goto fail;
    extensions = SDL_Vulkan_CreateInstanceExtensionsList(
        vkEnumerateInstanceExtensionProperties,
        &extensionCount);
    if (!extensions)
        goto fail;
    for (i = 0; i < extensionCount; i++) {
        if(SDL_strcmp(VK_KHR_SURFACE_EXTENSION_NAME, extensions[i].extensionName) == 0)
            hasSurfaceExtension = SDL_TRUE;
        else if(SDL_strcmp(VK_EXT_DIRECTFB_SURFACE_EXTENSION_NAME, extensions[i].extensionName) == 0)
            hasDirectFBSurfaceExtension = SDL_TRUE;
    }
    SDL_free(extensions);
    if (!hasSurfaceExtension) {
        SDL_SetError("Installed Vulkan doesn't implement the "
                     VK_KHR_SURFACE_EXTENSION_NAME " extension");
        goto fail;
    } else if(!hasDirectFBSurfaceExtension) {
        SDL_SetError("Installed Vulkan doesn't implement the "
                     VK_EXT_DIRECTFB_SURFACE_EXTENSION_NAME "extension");
        goto fail;
    }
    return 0;

fail:
    DirectFB_Vulkan_UnloadLibrary(vulkan_config);
    return -1;
}

void DirectFB_Vulkan_UnloadLibrary(SDL_VulkanVideo *vulkan_config)
{
    SDL_UnloadObject(vulkan_config->loader_handle);
    vulkan_config->loader_handle = NULL;
}

SDL_bool DirectFB_Vulkan_GetInstanceExtensions(SDL_Window *window,
                                          unsigned *count,
                                          const char **names)
{
    static const char *const extensionsForDirectFB[] = {
        VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_DIRECTFB_SURFACE_EXTENSION_NAME
    };
    return SDL_Vulkan_GetInstanceExtensions_Helper(
            count, names, SDL_arraysize(extensionsForDirectFB),
            extensionsForDirectFB);
}

SDL_bool DirectFB_Vulkan_CreateSurface(SDL_VulkanVideo *vulkan_config,
                                  SDL_Window *window,
                                  VkInstance instance,
                                  VkSurfaceKHR *surface)
{
    DFB_VideoData *devdata = &dfbVideoData;
    DFB_WindowData *windata;
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = vulkan_config->vkGetInstanceProcAddr;
    PFN_vkCreateDirectFBSurfaceEXT vkCreateDirectFBSurfaceEXT =
        (PFN_vkCreateDirectFBSurfaceEXT)vkGetInstanceProcAddr(
                                            instance,
                                            "vkCreateDirectFBSurfaceEXT");
    VkDirectFBSurfaceCreateInfoEXT createInfo;
    VkResult result;

    SDL_assert(vulkan_config->loader_handle != NULL);

    if (!vkCreateDirectFBSurfaceEXT) {
        SDL_SetError(VK_EXT_DIRECTFB_SURFACE_EXTENSION_NAME
                     " extension is not enabled in the Vulkan instance.");
        return SDL_FALSE;
    }
    windata = (DFB_WindowData *)window->driverdata;
    SDL_zero(createInfo);
    createInfo.sType = VK_STRUCTURE_TYPE_DIRECTFB_SURFACE_CREATE_INFO_EXT;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.dfb = devdata->dfb;
    createInfo.surface =  windata->surface;
    result = vkCreateDirectFBSurfaceEXT(instance, &createInfo,
                                        NULL, surface);
    if (result != VK_SUCCESS) {
        SDL_Vulkan_SetError("unable to create a Vulkan window surface", "vkCreateDirectFBSurfaceEXT", result);
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

#endif

/* vim: set ts=4 sw=4 expandtab: */
