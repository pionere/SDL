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
 * @author Mark Callow, www.edgewise-consulting.com. Based on Jacob Lifshay's
 * SDL_x11vulkan.c.
 */

#include "../../SDL_internal.h"

#if defined(SDL_VIDEO_VULKAN) && defined(SDL_VIDEO_DRIVER_UIKIT)

#include "SDL_uikitvideo.h"
#include "SDL_uikitwindow.h"

#include "SDL_loadso.h"
#include "SDL_uikitvulkan.h"
#include "SDL_uikitmetalview.h"
#include "SDL_syswm.h"

#include <dlfcn.h>

const char* defaultPaths[] = {
    "libvulkan.dylib",
};

/* Since libSDL is static, could use RTLD_SELF. Using RTLD_DEFAULT is future
 * proofing. */
#define DEFAULT_HANDLE RTLD_DEFAULT

int UIKit_Vulkan_LoadLibrary(SDL_VulkanVideo *vulkan_config, const char *path)
{
    VkExtensionProperties *extensions = NULL;
    Uint32 extensionCount = 0;
    SDL_bool hasSurfaceExtension = SDL_FALSE;
    SDL_bool hasMetalSurfaceExtension = SDL_FALSE;
    SDL_bool hasIOSSurfaceExtension = SDL_FALSE;
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = NULL;

    SDL_assert(vulkan_config->loader_handle == NULL);

    /* Load the Vulkan loader library */
    if (!path) {
        path = SDL_getenv("SDL_VULKAN_LIBRARY");
    }

    if (!path) {
        /* Handle the case where Vulkan Portability is linked statically. */
        vkGetInstanceProcAddr =
        (PFN_vkGetInstanceProcAddr)dlsym(DEFAULT_HANDLE,
                                         "vkGetInstanceProcAddr");
    }

    if (vkGetInstanceProcAddr) {
        vulkan_config->loader_handle = DEFAULT_HANDLE;
    } else {
        const char** paths;
        const char *foundPath = NULL;
        int numPaths;
        int i;

        if (path) {
            paths = &path;
            numPaths = 1;
        } else {
            /* Look for the .dylib packaged with the application instead. */
            paths = defaultPaths;
            numPaths = SDL_arraysize(defaultPaths);
        }

        for (i = 0; i < numPaths && vulkan_config->loader_handle == NULL; i++) {
            foundPath = paths[i];
            vulkan_config->loader_handle = SDL_LoadObject(foundPath);
        }

        if (!vulkan_config->loader_handle) {
            return -1;
        }

        SDL_strlcpy(vulkan_config->loader_path, path,
                    SDL_arraysize(vulkan_config->loader_path));
        vkGetInstanceProcAddr =
            (PFN_vkGetInstanceProcAddr)SDL_LoadFunction(
                                    vulkan_config->loader_handle,
                                    "vkGetInstanceProcAddr");
    }

    if (!vkGetInstanceProcAddr) {
        SDL_SetError("Failed to find %s in either executable or %s: %s",
                     "vkGetInstanceProcAddr",
                     "linked Vulkan Portability library",
                     (const char *) dlerror());
        goto fail;
    }

    vulkan_config->vkGetInstanceProcAddr = (void *)vkGetInstanceProcAddr;
    vulkan_config->vkEnumerateInstanceExtensionProperties =
        (void *)((PFN_vkGetInstanceProcAddr)vulkan_config->vkGetInstanceProcAddr)(
            VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");

    if (!vulkan_config->vkEnumerateInstanceExtensionProperties) {
        SDL_SetError("No vkEnumerateInstanceExtensionProperties found.");
        goto fail;
    }

    extensions = SDL_Vulkan_CreateInstanceExtensionsList(
        (PFN_vkEnumerateInstanceExtensionProperties)
            vulkan_config->vkEnumerateInstanceExtensionProperties,
        &extensionCount);

    if (!extensions) {
        goto fail;
    }

    for (Uint32 i = 0; i < extensionCount; i++) {
        if (SDL_strcmp(VK_KHR_SURFACE_EXTENSION_NAME, extensions[i].extensionName) == 0) {
            hasSurfaceExtension = SDL_TRUE;
        } else if (SDL_strcmp(VK_EXT_METAL_SURFACE_EXTENSION_NAME, extensions[i].extensionName) == 0) {
            hasMetalSurfaceExtension = SDL_TRUE;
        } else if (SDL_strcmp(VK_MVK_IOS_SURFACE_EXTENSION_NAME, extensions[i].extensionName) == 0) {
            hasIOSSurfaceExtension = SDL_TRUE;
        }
    }

    SDL_free(extensions);

    if (!hasSurfaceExtension) {
        SDL_SetError("Installed Vulkan Portability doesn't implement the "
                     VK_KHR_SURFACE_EXTENSION_NAME " extension");
        goto fail;
    } else if (!hasMetalSurfaceExtension && !hasIOSSurfaceExtension) {
        SDL_SetError("Installed Vulkan Portability doesn't implement the "
                     VK_EXT_METAL_SURFACE_EXTENSION_NAME " or "
                     VK_MVK_IOS_SURFACE_EXTENSION_NAME " extensions");
        goto fail;
    }

    return 0;

fail:
    UIKit_Vulkan_UnloadLibrary(vulkan_config);
    return -1;
}

void UIKit_Vulkan_UnloadLibrary(SDL_VulkanVideo *vulkan_config)
{
    if (vulkan_config->loader_handle != DEFAULT_HANDLE) {
        SDL_UnloadObject(vulkan_config->loader_handle);
    }
    vulkan_config->loader_handle = NULL;
}

SDL_bool UIKit_Vulkan_GetInstanceExtensions(SDL_Window *window,
                                          unsigned *count,
                                          const char **names)
{
    static const char *const extensionsForUIKit[] = {
        VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_METAL_SURFACE_EXTENSION_NAME
    };
    return SDL_Vulkan_GetInstanceExtensions_Helper(
            count, names, SDL_arraysize(extensionsForUIKit),
            extensionsForUIKit);
}

SDL_bool UIKit_Vulkan_CreateSurface(SDL_VulkanVideo *vulkan_config,
                                  SDL_Window *window,
                                  VkInstance instance,
                                  VkSurfaceKHR *surface)
{
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
        (PFN_vkGetInstanceProcAddr)vulkan_config->vkGetInstanceProcAddr;
    PFN_vkCreateMetalSurfaceEXT vkCreateMetalSurfaceEXT =
        (PFN_vkCreateMetalSurfaceEXT)vkGetInstanceProcAddr(
                                            (VkInstance)instance,
                                            "vkCreateMetalSurfaceEXT");
    PFN_vkCreateIOSSurfaceMVK vkCreateIOSSurfaceMVK =
        (PFN_vkCreateIOSSurfaceMVK)vkGetInstanceProcAddr(
                                            (VkInstance)instance,
                                            "vkCreateIOSSurfaceMVK");
    VkResult result;
    SDL_MetalView metalview;

    SDL_assert(vulkan_config->loader_handle != NULL);

    if (!vkCreateMetalSurfaceEXT && !vkCreateIOSSurfaceMVK) {
        SDL_SetError(VK_EXT_METAL_SURFACE_EXTENSION_NAME " or "
                     VK_MVK_IOS_SURFACE_EXTENSION_NAME
                     " extensions are not enabled in the Vulkan instance.");
        return SDL_FALSE;
    }

    metalview = UIKit_Metal_CreateView(window);
    if (metalview == NULL) {
        return SDL_FALSE;
    }

    if (vkCreateMetalSurfaceEXT) {
        VkMetalSurfaceCreateInfoEXT createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
        createInfo.pNext = NULL;
        createInfo.flags = 0;
        createInfo.pLayer = (__bridge const CAMetalLayer *)
                            UIKit_Metal_GetLayer(metalview);
        result = vkCreateMetalSurfaceEXT(instance, &createInfo, NULL, surface);
        if (result != VK_SUCCESS) {
            UIKit_Metal_DestroyView(metalview);
            SDL_SetError("vkCreateMetalSurfaceEXT failed: %s",
                         SDL_Vulkan_GetResultString(result));
            return SDL_FALSE;
        }
    } else {
        VkIOSSurfaceCreateInfoMVK createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK;
        createInfo.pNext = NULL;
        createInfo.flags = 0;
        createInfo.pView = (const void *)metalview;
        result = vkCreateIOSSurfaceMVK(instance, &createInfo,
                                           NULL, surface);
        if (result != VK_SUCCESS) {
            UIKit_Metal_DestroyView(metalview);
            SDL_SetError("vkCreateIOSSurfaceMVK failed: %s",
                         SDL_Vulkan_GetResultString(result));
            return SDL_FALSE;
        }
    }

    /* Unfortunately there's no SDL_Vulkan_DestroySurface function we can call
     * Metal_DestroyView from. Right now the metal view's ref count is +2 (one
     * from returning a new view object in CreateView, and one because it's
     * a subview of the window.) If we release the view here to make it +1, it
     * will be destroyed when the window is destroyed. */
    CFBridgingRelease(metalview);

    return SDL_TRUE;
}

void UIKit_Vulkan_GetDrawableSize(SDL_Window *window, int *w, int *h)
{
    UIKit_Metal_GetDrawableSize(window, w, h);
}

#endif

/* vi: set ts=4 sw=4 expandtab: */
