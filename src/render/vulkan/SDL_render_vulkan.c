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

#if SDL_VIDEO_RENDER_VULKAN

#define SDL_VULKAN_FRAME_QUEUE_DEPTH            2
#define SDL_VULKAN_NUM_VERTEX_BUFFERS           256
#define SDL_VULKAN_VERTEX_BUFFER_DEFAULT_SIZE   65536
#define SDL_VULKAN_CONSTANT_BUFFER_DEFAULT_SIZE 65536
#define SDL_VULKAN_NUM_UPLOAD_BUFFERS           32
#define SDL_VULKAN_MAX_DESCRIPTOR_SETS          4096

#define SDL_VULKAN_VALIDATION_LAYER_NAME        "VK_LAYER_KHRONOS_validation"

#define VK_NO_PROTOTYPES

#include "SDL_hints.h"
#include "../../video/SDL_vulkan_internal.h"
#include "../../video/SDL_sysvideo.h"
#include "../SDL_sysrender.h"
#include "../SDL_d3dmath.h"
#include "../../video/SDL_pixels_c.h"
#include "SDL_shaders_vulkan.h"

#define SDL_VULKAN_ERROR_UNKNOWN                -1
SDL_COMPILE_TIME_ASSERT(vk_error, SDL_VULKAN_ERROR_UNKNOWN != VK_SUCCESS);

#define VULKAN_FUNCTIONS()                                              \
    VULKAN_DEVICE_FUNCTION(vkAcquireNextImageKHR)                       \
    VULKAN_DEVICE_FUNCTION(vkAllocateCommandBuffers)                    \
    VULKAN_DEVICE_FUNCTION(vkAllocateDescriptorSets)                    \
    VULKAN_DEVICE_FUNCTION(vkAllocateMemory)                            \
    VULKAN_DEVICE_FUNCTION(vkBeginCommandBuffer)                        \
    VULKAN_DEVICE_FUNCTION(vkBindBufferMemory)                          \
    VULKAN_DEVICE_FUNCTION(vkBindImageMemory)                           \
    VULKAN_DEVICE_FUNCTION(vkCmdBeginRenderPass)                        \
    VULKAN_DEVICE_FUNCTION(vkCmdBindDescriptorSets)                     \
    VULKAN_DEVICE_FUNCTION(vkCmdBindPipeline)                           \
    VULKAN_DEVICE_FUNCTION(vkCmdBindVertexBuffers)                      \
    VULKAN_DEVICE_FUNCTION(vkCmdCopyBufferToImage)                      \
    VULKAN_DEVICE_FUNCTION(vkCmdCopyImageToBuffer)                      \
    VULKAN_DEVICE_FUNCTION(vkCmdDraw)                                   \
    VULKAN_DEVICE_FUNCTION(vkCmdEndRenderPass)                          \
    VULKAN_DEVICE_FUNCTION(vkCmdPipelineBarrier)                        \
    VULKAN_DEVICE_FUNCTION(vkCmdPushConstants)                          \
    VULKAN_DEVICE_FUNCTION(vkCmdSetScissor)                             \
    VULKAN_DEVICE_FUNCTION(vkCmdSetViewport)                            \
    VULKAN_DEVICE_FUNCTION(vkCreateBuffer)                              \
    VULKAN_DEVICE_FUNCTION(vkCreateCommandPool)                         \
    VULKAN_DEVICE_FUNCTION(vkCreateDescriptorPool)                      \
    VULKAN_DEVICE_FUNCTION(vkCreateDescriptorSetLayout)                 \
    VULKAN_DEVICE_FUNCTION(vkCreateFence)                               \
    VULKAN_DEVICE_FUNCTION(vkCreateFramebuffer)                         \
    VULKAN_DEVICE_FUNCTION(vkCreateGraphicsPipelines)                   \
    VULKAN_DEVICE_FUNCTION(vkCreateImage)                               \
    VULKAN_DEVICE_FUNCTION(vkCreateImageView)                           \
    VULKAN_DEVICE_FUNCTION(vkCreatePipelineLayout)                      \
    VULKAN_DEVICE_FUNCTION(vkCreateRenderPass)                          \
    VULKAN_DEVICE_FUNCTION(vkCreateSampler)                             \
    VULKAN_DEVICE_FUNCTION(vkCreateSemaphore)                           \
    VULKAN_DEVICE_FUNCTION(vkCreateShaderModule)                        \
    VULKAN_DEVICE_FUNCTION(vkCreateSwapchainKHR)                        \
    VULKAN_DEVICE_FUNCTION(vkDestroyBuffer)                             \
    VULKAN_DEVICE_FUNCTION(vkDestroyCommandPool)                        \
    VULKAN_DEVICE_FUNCTION(vkDestroyDevice)                             \
    VULKAN_DEVICE_FUNCTION(vkDestroyDescriptorPool)                     \
    VULKAN_DEVICE_FUNCTION(vkDestroyDescriptorSetLayout)                \
    VULKAN_DEVICE_FUNCTION(vkDestroyFence)                              \
    VULKAN_DEVICE_FUNCTION(vkDestroyFramebuffer)                        \
    VULKAN_DEVICE_FUNCTION(vkDestroyImage)                              \
    VULKAN_DEVICE_FUNCTION(vkDestroyImageView)                          \
    VULKAN_DEVICE_FUNCTION(vkDestroyPipeline)                           \
    VULKAN_DEVICE_FUNCTION(vkDestroyPipelineLayout)                     \
    VULKAN_DEVICE_FUNCTION(vkDestroyRenderPass)                         \
    VULKAN_DEVICE_FUNCTION(vkDestroySampler)                            \
    VULKAN_DEVICE_FUNCTION(vkDestroySemaphore)                          \
    VULKAN_DEVICE_FUNCTION(vkDestroyShaderModule)                       \
    VULKAN_DEVICE_FUNCTION(vkDestroySwapchainKHR)                       \
    VULKAN_DEVICE_FUNCTION(vkDeviceWaitIdle)                            \
    VULKAN_DEVICE_FUNCTION(vkEndCommandBuffer)                          \
    VULKAN_DEVICE_FUNCTION(vkFreeCommandBuffers)                        \
    VULKAN_DEVICE_FUNCTION(vkFreeMemory)                                \
    VULKAN_DEVICE_FUNCTION(vkGetBufferMemoryRequirements)               \
    VULKAN_DEVICE_FUNCTION(vkGetImageMemoryRequirements)                \
    VULKAN_DEVICE_FUNCTION(vkGetDeviceQueue)                            \
    VULKAN_DEVICE_FUNCTION(vkGetSwapchainImagesKHR)                     \
    VULKAN_DEVICE_FUNCTION(vkMapMemory)                                 \
    VULKAN_DEVICE_FUNCTION(vkQueuePresentKHR)                           \
    VULKAN_DEVICE_FUNCTION(vkQueueSubmit)                               \
    VULKAN_DEVICE_FUNCTION(vkResetCommandBuffer)                        \
    VULKAN_DEVICE_FUNCTION(vkResetCommandPool)                          \
    VULKAN_DEVICE_FUNCTION(vkResetDescriptorPool)                       \
    VULKAN_DEVICE_FUNCTION(vkResetFences)                               \
    VULKAN_DEVICE_FUNCTION(vkUpdateDescriptorSets)                      \
    VULKAN_DEVICE_FUNCTION(vkWaitForFences)                             \
    VULKAN_GLOBAL_FUNCTION(vkCreateInstance)                            \
    VULKAN_GLOBAL_FUNCTION(vkEnumerateInstanceExtensionProperties)      \
    VULKAN_GLOBAL_FUNCTION(vkEnumerateInstanceLayerProperties)          \
    VULKAN_INSTANCE_FUNCTION(vkCreateDevice)                            \
    VULKAN_INSTANCE_FUNCTION(vkDestroyInstance)                         \
    VULKAN_INSTANCE_FUNCTION(vkDestroySurfaceKHR)                       \
    VULKAN_INSTANCE_FUNCTION(vkEnumerateDeviceExtensionProperties)      \
    VULKAN_INSTANCE_FUNCTION(vkEnumeratePhysicalDevices)                \
    VULKAN_INSTANCE_FUNCTION(vkGetDeviceProcAddr)                       \
    VULKAN_INSTANCE_FUNCTION(vkGetPhysicalDeviceFeatures)               \
    VULKAN_INSTANCE_FUNCTION(vkGetPhysicalDeviceProperties)             \
    VULKAN_INSTANCE_FUNCTION(vkGetPhysicalDeviceMemoryProperties)       \
    VULKAN_INSTANCE_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties)  \
    VULKAN_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) \
    VULKAN_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceFormatsKHR)      \
    VULKAN_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfacePresentModesKHR) \
    VULKAN_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR)      \
    VULKAN_INSTANCE_FUNCTION(vkQueueWaitIdle)                           \
    VULKAN_OPTIONAL_DEVICE_FUNCTION(vkCreateSamplerYcbcrConversionKHR)              \
    VULKAN_OPTIONAL_DEVICE_FUNCTION(vkDestroySamplerYcbcrConversionKHR)             \

#if 0
    VULKAN_DEVICE_FUNCTION(vkCmdClearColorImage)                        \
    VULKAN_DEVICE_FUNCTION(vkGetFenceStatus)                            \
    VULKAN_DEVICE_FUNCTION(vkUnmapMemory)                               \
    VULKAN_OPTIONAL_INSTANCE_FUNCTION(vkGetPhysicalDeviceFeatures2KHR)              \
    VULKAN_OPTIONAL_INSTANCE_FUNCTION(vkGetPhysicalDeviceFormatProperties2KHR)      \
    VULKAN_OPTIONAL_INSTANCE_FUNCTION(vkGetPhysicalDeviceImageFormatProperties2KHR) \
    VULKAN_OPTIONAL_INSTANCE_FUNCTION(vkGetPhysicalDeviceMemoryProperties2KHR)      \
    VULKAN_OPTIONAL_INSTANCE_FUNCTION(vkGetPhysicalDeviceProperties2KHR)            \

#endif

#define VULKAN_DEVICE_FUNCTION(name)            static PFN_##name name = NULL;
#define VULKAN_GLOBAL_FUNCTION(name)            static PFN_##name name = NULL;
#define VULKAN_INSTANCE_FUNCTION(name)          static PFN_##name name = NULL;
#define VULKAN_OPTIONAL_INSTANCE_FUNCTION(name) static PFN_##name name = NULL;
#define VULKAN_OPTIONAL_DEVICE_FUNCTION(name)   static PFN_##name name = NULL;
VULKAN_FUNCTIONS()
#undef VULKAN_DEVICE_FUNCTION
#undef VULKAN_GLOBAL_FUNCTION
#undef VULKAN_INSTANCE_FUNCTION
#undef VULKAN_OPTIONAL_INSTANCE_FUNCTION
#undef VULKAN_OPTIONAL_DEVICE_FUNCTION

/* Renderpass types */
typedef enum {
    SDL_VULKAN_RENDERPASS_LOAD = 0,
    SDL_VULKAN_RENDERPASS_CLEAR = 1,
    SDL_VULKAN_NUM_RENDERPASSES
} SDL_vulkan_renderpass_type;

/* Sampler types */
typedef enum {
    SDL_VULKAN_SAMPLER_NEAREST = 0,
    SDL_VULKAN_SAMPLER_LINEAR = 1,
    SDL_VULKAN_NUM_SAMPLERS
} SDL_vulkan_sampler_type;

/* Vertex shader, common values */
typedef struct
{
    Float4X4 model;
    Float4X4 projectionAndView;
} VertexShaderConstants;

#if 0
/* These should mirror the definitions in VULKAN_PixelShader_Common.hlsli */
//static const float TONEMAP_NONE = 0;
//static const float TONEMAP_LINEAR = 1;
static const float TONEMAP_CHROME = 2;
#endif

static const float INPUTTYPE_UNSPECIFIED = 0;
static const float INPUTTYPE_SRGB = 1;
#if 0
static const float INPUTTYPE_SCRGB = 2;
#endif
#if SDL_HAVE_YUV
static const float INPUTTYPE_HDR10 = 3;
#endif

/* Pixel shader constants, common values */
typedef struct
{
    float scRGB_output;
    float input_type;
    float color_scale;
    float unused_pad0;

    float tonemap_method;
    float tonemap_factor1;
    float tonemap_factor2;
    float sdr_white_point;
} PixelShaderConstants;

/* Per-vertex data */
typedef struct
{
    float pos[2];
    float tex[2];
#if 0
    SDL_FColor color;
#else
    SDL_Color color;
#endif
} VertexPositionColor;

/* Vulkan Buffer */
typedef struct
{
    VkDeviceMemory deviceMemory;
    VkBuffer buffer;
    VkDeviceSize size;
    void *mappedBufferPtr;
} VULKAN_Buffer;

/* Vulkan image */
typedef struct
{
    // SDL_bool allocatedImage;
    VkImage image;
    VkImageView imageView;
    VkDeviceMemory deviceMemory;
    VkImageLayout imageLayout;
    VkFormat format;
} VULKAN_Image;

/* Per-texture data */
typedef struct
{
    VULKAN_Image mainImage;
    VkRenderPass mainRenderpasses[SDL_VULKAN_NUM_RENDERPASSES];
    VkFramebuffer mainFramebuffer;
    VULKAN_Buffer stagingBuffer;
    VkFilter scaleMode;
    SDL_Rect lockedRect;
    int width;
    int height;
    VULKAN_Shader shader;

    /* Object passed to VkImageView and VkSampler for doing Ycbcr -> RGB conversion */
    VkSamplerYcbcrConversion samplerYcbcrConversion;
    /* Sampler created with samplerYcbcrConversion, passed to PSO as immutable sampler */
    VkSampler samplerYcbcr;
    /* Descriptor set layout with samplerYcbcr baked as immutable sampler */
    VkDescriptorSetLayout descriptorSetLayoutYcbcr;
    /* Pipeline layout with immutable sampler descriptor set layout */
    VkPipelineLayout pipelineLayoutYcbcr;

} VULKAN_TextureData;

/* Pipeline State Object data */
typedef struct
{
    VULKAN_Shader shader;
    PixelShaderConstants shader_constants;
    SDL_BlendMode blendMode;
    VkPrimitiveTopology topology;
    VkFormat format;
    VkPipelineLayout pipelineLayout;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipeline pipeline;
} VULKAN_PipelineState;

typedef struct
{
    VkBuffer vertexBuffer;
} VULKAN_DrawStateCache;

/* Private renderer data */
typedef struct
{
    // PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
    SDL_bool vulkan_loaded;
    VkInstance instance;
    // SDL_bool instance_external;
    VkSurfaceKHR surface;
    // SDL_bool surface_external;
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties physicalDeviceProperties;
    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
    VkPhysicalDeviceFeatures physicalDeviceFeatures;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkDevice device;
    // SDL_bool device_external;
    uint32_t graphicsQueueFamilyIndex;
    uint32_t presentQueueFamilyIndex;
    VkSwapchainKHR swapchain;
    VkCommandPool commandPool;
    VkCommandBuffer *commandBuffers;
    uint32_t currentCommandBufferIndex;
    VkCommandBuffer currentCommandBuffer;
    VkFence *fences;
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VkSurfaceFormatKHR *surfaceFormats;
    SDL_bool recreateSwapchain;

    VkFramebuffer *framebuffers;
    VkRenderPass renderPasses[SDL_VULKAN_NUM_RENDERPASSES];
    VkRenderPass currentRenderPass;

    VkShaderModule vertexShaderModules[NUM_SHADERS];
    VkShaderModule fragmentShaderModules[NUM_SHADERS];
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;

    /* Vertex buffer data */
    VULKAN_Buffer vertexBuffers[SDL_VULKAN_NUM_VERTEX_BUFFERS];
    VertexShaderConstants vertexShaderConstantsData;

    /* Data for staging/allocating textures */
    VULKAN_Buffer **uploadBuffers;
    int *currentUploadBuffer;

    /* Data for updating constants */
    VULKAN_Buffer **constantBuffers;
    uint32_t *numConstantBuffers;
    uint32_t currentConstantBufferIndex;
    int32_t currentConstantBufferOffset;

    VkSampler samplers[SDL_VULKAN_NUM_SAMPLERS];
    VkDescriptorPool **descriptorPools;
    uint32_t *numDescriptorPools;
    uint32_t currentDescriptorPoolIndex;
    uint32_t currentDescriptorSetIndex;

    int pipelineStateCount;
    VULKAN_PipelineState *pipelineStates;
    VULKAN_PipelineState *currentPipelineState;

    SDL_bool supportsEXTSwapchainColorspace;
    SDL_bool supportsKHRGetPhysicalDeviceProperties2;
    SDL_bool supportsKHRSamplerYCbCrConversion;
    uint32_t surfaceFormatsAllocatedCount;
    uint32_t surfaceFormatsCount;
    uint32_t swapchainDesiredImageCount;
    VkSurfaceFormatKHR surfaceFormat;
    VkExtent2D swapchainSize;
    uint32_t swapchainImageCount;
    VkImage *swapchainImages;
    VkImageView *swapchainImageViews;
    VkImageLayout *swapchainImageLayouts;
    VkSemaphore *imageAvailableSemaphores;
    VkSemaphore *renderingFinishedSemaphores;
    VkSemaphore currentImageAvailableSemaphore;
    uint32_t currentSwapchainImageIndex;

    VkPipelineStageFlags *waitDestStageMasks;
    VkSemaphore *waitRenderSemaphores;
    uint32_t waitRenderSemaphoreCount;
    uint32_t waitRenderSemaphoreMax;
    VkSemaphore *signalRenderSemaphores;
    uint32_t signalRenderSemaphoreCount;
    uint32_t signalRenderSemaphoreMax;

    /* Cached renderer properties */
    VULKAN_TextureData *textureRenderTarget;
    SDL_bool cliprectDirty;
    SDL_bool currentCliprectEnabled;
    SDL_Rect currentCliprect;
    SDL_Rect currentViewport;
    int currentViewportRotation;
    SDL_bool viewportDirty;
    Float4X4 identity;
    VkComponentMapping identitySwizzle;
    int currentVertexBuffer;
    SDL_bool issueBatch;
} VULKAN_RenderData;

static Uint32 VULKAN_VkFormatToSDLPixelFormat(VkFormat vkFormat)
{
    switch (vkFormat) {
    case VK_FORMAT_B8G8R8A8_UNORM:
        return SDL_PIXELFORMAT_ARGB8888;
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
        return SDL_PIXELFORMAT_XBGR2101010;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return SDL_PIXELFORMAT_RGBA64_FLOAT;
    default:
        return SDL_PIXELFORMAT_UNKNOWN;
    }
}

static int VULKAN_VkFormatGetNumPlanes(VkFormat vkFormat)
{
    switch (vkFormat) {
#if SDL_HAVE_YUV
    case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
        return 3;
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
        return 2;
#endif
    default:
        return 1;
    }
}

static VkDeviceSize VULKAN_GetBytesPerPixel(VkFormat vkFormat)
{
    switch (vkFormat) {
    case VK_FORMAT_R8_UNORM:
        return 1;
    case VK_FORMAT_R8G8_UNORM:
        return 2;
    case VK_FORMAT_R16G16_UNORM:
        return 4;
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
        return 4;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return 8;
    default:
        return 4;
    }
}

static VkFormat SDLPixelFormatToVkTextureFormat(Uint32 format) // , Uint32 colorspace)
{
    switch (format) {
    case SDL_PIXELFORMAT_RGBA64_FLOAT:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case SDL_PIXELFORMAT_XBGR2101010:
        return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case SDL_PIXELFORMAT_ARGB8888:
    case SDL_PIXELFORMAT_XRGB8888:
        // if (colorspace == SDL_COLORSPACE_SRGB_LINEAR) {
        //    return VK_FORMAT_B8G8R8A8_SRGB;
        // }
        return VK_FORMAT_B8G8R8A8_UNORM;
#if SDL_HAVE_YUV
    case SDL_PIXELFORMAT_YUY2:
        return VK_FORMAT_G8B8G8R8_422_UNORM;
    case SDL_PIXELFORMAT_UYVY:
        return VK_FORMAT_B8G8R8G8_422_UNORM;
    case SDL_PIXELFORMAT_YV12:
    case SDL_PIXELFORMAT_IYUV:
        return VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;
    case SDL_PIXELFORMAT_NV12:
    case SDL_PIXELFORMAT_NV21:
        return  VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
    case SDL_PIXELFORMAT_P010:
        return VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
#endif
    default:
        return VK_FORMAT_UNDEFINED;
    }
}

// static void VULKAN_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture);
static void VULKAN_DestroyBuffer(VULKAN_RenderData *rendererData, VULKAN_Buffer *vulkanBuffer);
// static void VULKAN_DestroyImage(VULKAN_RenderData *rendererData, VULKAN_Image *vulkanImage);
static void VULKAN_ResetCommandList(VULKAN_RenderData *rendererData);
static SDL_bool VULKAN_FindMemoryTypeIndex(VULKAN_RenderData *rendererData, uint32_t typeBits, VkMemoryPropertyFlags requiredFlags, VkMemoryPropertyFlags desiredFlags, uint32_t *memoryTypeIndexOut);
static VkResult VULKAN_CreateWindowSizeDependentResources(SDL_Renderer *renderer);
static VkDescriptorPool VULKAN_AllocateDescriptorPool(VULKAN_RenderData *rendererData);
static VkResult VULKAN_CreateDescriptorSetAndPipelineLayout(VULKAN_RenderData *rendererData, VkSampler samplerYcbcr, VkDescriptorSetLayout *descriptorSetLayoutOut, VkPipelineLayout *pipelineLayoutOut);

static void VULKAN_DestroyAll(SDL_Renderer *renderer)
{
    VULKAN_RenderData *rendererData;

    SDL_assert(renderer != NULL);
    rendererData = (VULKAN_RenderData *)renderer->driverdata;
    SDL_assert(rendererData != NULL);

    if (rendererData->waitDestStageMasks) {
        SDL_free(rendererData->waitDestStageMasks);
        rendererData->waitDestStageMasks = NULL;
    }
    if (rendererData->waitRenderSemaphores) {
        SDL_free(rendererData->waitRenderSemaphores);
        rendererData->waitRenderSemaphores = NULL;
    }
    if (rendererData->signalRenderSemaphores) {
        SDL_free(rendererData->signalRenderSemaphores);
        rendererData->signalRenderSemaphores = NULL;
    }
    if (rendererData->surfaceFormats != NULL) {
        SDL_free(rendererData->surfaceFormats);
        rendererData->surfaceFormats = NULL;
        rendererData->surfaceFormatsAllocatedCount = 0;
    }
    if (rendererData->swapchainImages != NULL) {
        SDL_free(rendererData->swapchainImages);
        rendererData->swapchainImages = NULL;
    }
    if (rendererData->swapchain) {
        vkDestroySwapchainKHR(rendererData->device, rendererData->swapchain, NULL);
        rendererData->swapchain = VK_NULL_HANDLE;
    }
    if (rendererData->fences != NULL) {
        for (uint32_t i = 0; i < rendererData->swapchainImageCount; i++) {
            if (rendererData->fences[i] != VK_NULL_HANDLE) {
                vkDestroyFence(rendererData->device, rendererData->fences[i], NULL);
                rendererData->fences[i] = VK_NULL_HANDLE;
            }
        }
        SDL_free(rendererData->fences);
        rendererData->fences = NULL;
    }
    if (rendererData->swapchainImageViews) {
        for (uint32_t i = 0; i < rendererData->swapchainImageCount; i++) {
            if (rendererData->swapchainImageViews[i] != VK_NULL_HANDLE) {
                vkDestroyImageView(rendererData->device, rendererData->swapchainImageViews[i], NULL);
            }
        }
        SDL_free(rendererData->swapchainImageViews);
        rendererData->swapchainImageViews = NULL;
    }
    if (rendererData->swapchainImageLayouts) {
        SDL_free(rendererData->swapchainImageLayouts);
        rendererData->swapchainImageLayouts = NULL;
    }
    if (rendererData->framebuffers) {
        for (uint32_t i = 0; i < rendererData->swapchainImageCount; i++) {
            if (rendererData->framebuffers[i] != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(rendererData->device, rendererData->framebuffers[i], NULL);
            }
        }
        SDL_free(rendererData->framebuffers);
        rendererData->framebuffers = NULL;
    }
    for (uint32_t i = 0; i < SDL_arraysize(rendererData->samplers); i++) {
        if (rendererData->samplers[i] != VK_NULL_HANDLE) {
            vkDestroySampler(rendererData->device, rendererData->samplers[i], NULL);
            rendererData->samplers[i] = VK_NULL_HANDLE;
        }
    }
    for (uint32_t i = 0; i < SDL_arraysize(rendererData->vertexBuffers); i++ ) {
        VULKAN_DestroyBuffer(rendererData, &rendererData->vertexBuffers[i]);
    }
//    SDL_memset(rendererData->vertexBuffers, 0, sizeof(rendererData->vertexBuffers));
    for (uint32_t i = 0; i < SDL_VULKAN_NUM_RENDERPASSES; i++) {
        if (rendererData->renderPasses[i] != VK_NULL_HANDLE) {
            vkDestroyRenderPass(rendererData->device, rendererData->renderPasses[i], NULL);
            rendererData->renderPasses[i] = VK_NULL_HANDLE;
        }
    }
    if (rendererData->imageAvailableSemaphores) {
        for (uint32_t i = 0; i < rendererData->swapchainImageCount; ++i) {
            if (rendererData->imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(rendererData->device, rendererData->imageAvailableSemaphores[i], NULL);
            }
        }
        SDL_free(rendererData->imageAvailableSemaphores);
        rendererData->imageAvailableSemaphores = NULL;
    }
    if (rendererData->renderingFinishedSemaphores) {
        for (uint32_t i = 0; i < rendererData->swapchainImageCount; ++i) {
            if (rendererData->renderingFinishedSemaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(rendererData->device, rendererData->renderingFinishedSemaphores[i], NULL);
            }
        }
        SDL_free(rendererData->renderingFinishedSemaphores);
        rendererData->renderingFinishedSemaphores = NULL;
    }
    if (rendererData->commandPool) {
        if (rendererData->commandBuffers) {
            vkFreeCommandBuffers(rendererData->device, rendererData->commandPool, rendererData->swapchainImageCount, rendererData->commandBuffers);
            SDL_free(rendererData->commandBuffers);
            rendererData->commandBuffers = NULL;
        }
        vkDestroyCommandPool(rendererData->device, rendererData->commandPool, NULL);
        rendererData->commandPool = VK_NULL_HANDLE;
    }
    if (rendererData->descriptorPools) {
        SDL_assert(rendererData->numDescriptorPools);
        for (uint32_t i = 0; i < rendererData->swapchainImageCount; i++) {
            for (uint32_t j = 0; j < rendererData->numDescriptorPools[i]; j++) {
                if (rendererData->descriptorPools[i][j] != VK_NULL_HANDLE) {
                    vkDestroyDescriptorPool(rendererData->device, rendererData->descriptorPools[i][j], NULL);
                }
            }
            SDL_free(rendererData->descriptorPools[i]);
        }
        SDL_free(rendererData->descriptorPools);
        SDL_free(rendererData->numDescriptorPools);
    }
    for (uint32_t i = 0; i < NUM_SHADERS; i++) {
        if (rendererData->vertexShaderModules[i] != VK_NULL_HANDLE) {
            vkDestroyShaderModule(rendererData->device, rendererData->vertexShaderModules[i], NULL);
            rendererData->vertexShaderModules[i] = VK_NULL_HANDLE;
        }
        if (rendererData->fragmentShaderModules[i] != VK_NULL_HANDLE) {
            vkDestroyShaderModule(rendererData->device, rendererData->fragmentShaderModules[i], NULL);
            rendererData->fragmentShaderModules[i] = VK_NULL_HANDLE;
        }
    }
    if (rendererData->descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rendererData->device, rendererData->descriptorSetLayout, NULL);
        rendererData->descriptorSetLayout = VK_NULL_HANDLE;
    }
    if (rendererData->pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(rendererData->device, rendererData->pipelineLayout, NULL);
        rendererData->pipelineLayout = VK_NULL_HANDLE;
    }
    for (int i = 0; i < rendererData->pipelineStateCount; i++) {
        vkDestroyPipeline(rendererData->device, rendererData->pipelineStates[i].pipeline, NULL);
    }
    SDL_free(rendererData->pipelineStates);
    rendererData->pipelineStateCount = 0;

    if (rendererData->currentUploadBuffer) {
        for (uint32_t i = 0; i < rendererData->swapchainImageCount; ++i) {
            for (int j = 0; j < rendererData->currentUploadBuffer[i]; ++j) {
                VULKAN_DestroyBuffer(rendererData, &rendererData->uploadBuffers[i][j]);
            }
            SDL_free(rendererData->uploadBuffers[i]);
        }
        SDL_free(rendererData->uploadBuffers);
        SDL_free(rendererData->currentUploadBuffer);
    }

    if (rendererData->constantBuffers) {
        SDL_assert(rendererData->numConstantBuffers);
        for (uint32_t i = 0; i < rendererData->swapchainImageCount; ++i) {
            for (uint32_t j = 0; j < rendererData->numConstantBuffers[i]; j++) {
                VULKAN_DestroyBuffer(rendererData, &rendererData->constantBuffers[i][j]);
            }
            SDL_free(rendererData->constantBuffers[i]);
        }
        SDL_free(rendererData->constantBuffers);
        SDL_free(rendererData->numConstantBuffers);
        rendererData->constantBuffers = NULL;
    }

    if (rendererData->device != VK_NULL_HANDLE /*&& !rendererData->device_external*/) {
        vkDestroyDevice(rendererData->device, NULL);
        rendererData->device = VK_NULL_HANDLE;
    }
    if (rendererData->surface != VK_NULL_HANDLE /*&& !rendererData->surface_external*/) {
        vkDestroySurfaceKHR(rendererData->instance, rendererData->surface, NULL);
        rendererData->surface = VK_NULL_HANDLE;
    }
    if (rendererData->instance != VK_NULL_HANDLE /*&& !rendererData->instance_external*/) {
        vkDestroyInstance(rendererData->instance, NULL);
        rendererData->instance = VK_NULL_HANDLE;
    }
}

static void VULKAN_DestroyBuffer(VULKAN_RenderData *rendererData, VULKAN_Buffer *vulkanBuffer)
{
    if (vulkanBuffer->buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(rendererData->device, vulkanBuffer->buffer, NULL);
        // vulkanBuffer->buffer = VK_NULL_HANDLE;
    }
    if (vulkanBuffer->deviceMemory != VK_NULL_HANDLE) {
        vkFreeMemory(rendererData->device, vulkanBuffer->deviceMemory, NULL);
        // vulkanBuffer->deviceMemory = VK_NULL_HANDLE;
    }
    SDL_memset(vulkanBuffer, 0, sizeof(VULKAN_Buffer));
}

static VkResult VULKAN_AllocateBuffer(VULKAN_RenderData *rendererData, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags requiredMemoryProps, VkMemoryPropertyFlags desiredMemoryProps, VULKAN_Buffer *bufferOut)
{
    VkResult result;
    VkMemoryRequirements memoryRequirements;
    VkMemoryAllocateInfo memoryAllocateInfo;
    uint32_t memoryTypeIndex;
    VkBufferCreateInfo bufferCreateInfo;

    SDL_zero(bufferCreateInfo);
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = usage;
    result = vkCreateBuffer(rendererData->device, &bufferCreateInfo, NULL, &bufferOut->buffer);
    if (result != VK_SUCCESS) {
        SDL_Vulkan_SetError("VULKAN_AllocateBuffer failed", "vkCreateBuffer", result);
        return result;
    }

    SDL_zero(memoryRequirements);
    vkGetBufferMemoryRequirements(rendererData->device, bufferOut->buffer, &memoryRequirements);
    if (result != VK_SUCCESS) {
        VULKAN_DestroyBuffer(rendererData, bufferOut);
        SDL_Vulkan_SetError("VULKAN_AllocateBuffer failed", "vkGetBufferMemoryRequirements", result);
        return result;
    }

    memoryTypeIndex = 0;
    if (!VULKAN_FindMemoryTypeIndex(rendererData, memoryRequirements.memoryTypeBits, requiredMemoryProps, desiredMemoryProps, &memoryTypeIndex)) {
        VULKAN_DestroyBuffer(rendererData, bufferOut);
        return SDL_VULKAN_ERROR_UNKNOWN;
    }

    SDL_zero(memoryAllocateInfo);
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;
    result = vkAllocateMemory(rendererData->device, &memoryAllocateInfo, NULL, &bufferOut->deviceMemory);
    if (result != VK_SUCCESS) {
        VULKAN_DestroyBuffer(rendererData, bufferOut);
        SDL_Vulkan_SetError("VULKAN_AllocateBuffer failed", "vkAllocateMemory", result);
        return result;
    }
    result = vkBindBufferMemory(rendererData->device, bufferOut->buffer, bufferOut->deviceMemory, 0);
    if (result != VK_SUCCESS) {
        VULKAN_DestroyBuffer(rendererData, bufferOut);
        SDL_Vulkan_SetError("VULKAN_AllocateBuffer failed", "vkBindBufferMemory", result);
        return result;
    }

    result = vkMapMemory(rendererData->device, bufferOut->deviceMemory, 0, size, 0, &bufferOut->mappedBufferPtr);
    if (result != VK_SUCCESS) {
        VULKAN_DestroyBuffer(rendererData, bufferOut);
        SDL_Vulkan_SetError("VULKAN_AllocateBuffer failed", "vkMapMemory", result);
        return result;
    }
    bufferOut->size = size;
    return result;
}

static void VULKAN_DestroyImage(VULKAN_RenderData *rendererData, VULKAN_Image *vulkanImage)
{
    if (vulkanImage->imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(rendererData->device, vulkanImage->imageView, NULL);
        // vulkanImage->imageView = VK_NULL_HANDLE;
    }
    if (vulkanImage->image != VK_NULL_HANDLE) {
        // if (vulkanImage->allocatedImage) {
            vkDestroyImage(rendererData->device, vulkanImage->image, NULL);
        // }
        // vulkanImage->image = VK_NULL_HANDLE;
    }

    if (vulkanImage->deviceMemory != VK_NULL_HANDLE) {
        // if (vulkanImage->allocatedImage) {
            vkFreeMemory(rendererData->device, vulkanImage->deviceMemory, NULL);
        // }
        // vulkanImage->deviceMemory = VK_NULL_HANDLE;
    }
    SDL_zerop(vulkanImage);
}

static VkResult VULKAN_AllocateImage(VULKAN_RenderData *rendererData/*, SDL_PropertiesID create_props*/, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags imageUsage, VkComponentMapping swizzle, VkSamplerYcbcrConversionKHR samplerYcbcrConversion, VULKAN_Image *imageOut)
{
    VkResult result;
    VkImageCreateInfo imageCreateInfo;
    VkMemoryRequirements memoryRequirements;
    uint32_t memoryTypeIndex;
    VkMemoryAllocateInfo memoryAllocateInfo;
    VkImageViewCreateInfo imageViewCreateInfo;
    VkSamplerYcbcrConversionInfoKHR samplerYcbcrConversionInfo;

    SDL_zerop(imageOut);
    imageOut->format = format;
    imageOut->image = VK_NULL_HANDLE; // (VkImage)SDL_GetNumberProperty(create_props, SDL_PROP_TEXTURE_CREATE_VULKAN_TEXTURE_NUMBER, 0);

    if (imageOut->image == VK_NULL_HANDLE) {
        // imageOut->allocatedImage = VK_TRUE;

        SDL_zero(imageCreateInfo);
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.flags = 0;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.extent.width = width;
        imageCreateInfo.extent.height = height;
        imageCreateInfo.extent.depth = 1;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.usage = imageUsage;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.queueFamilyIndexCount = 0;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        result = vkCreateImage(rendererData->device, &imageCreateInfo, NULL, &imageOut->image);
        if (result != VK_SUCCESS) {
            VULKAN_DestroyImage(rendererData, imageOut);
            SDL_Vulkan_SetError("VULKAN_AllocateImage failed", "vkCreateImage", result);
            return result;
        }

        SDL_zero(memoryRequirements);
        vkGetImageMemoryRequirements(rendererData->device, imageOut->image, &memoryRequirements);
        if (result != VK_SUCCESS) {
            VULKAN_DestroyImage(rendererData, imageOut);
            SDL_Vulkan_SetError("VULKAN_AllocateImage failed", "vkGetImageMemoryRequirements", result);
            return result;
        }

        memoryTypeIndex = 0;
        if (!VULKAN_FindMemoryTypeIndex(rendererData, memoryRequirements.memoryTypeBits, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memoryTypeIndex)) {
            VULKAN_DestroyImage(rendererData, imageOut);
            return SDL_VULKAN_ERROR_UNKNOWN;
        }

        SDL_zero(memoryAllocateInfo);
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.allocationSize = memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;
        result = vkAllocateMemory(rendererData->device, &memoryAllocateInfo, NULL, &imageOut->deviceMemory);
        if (result != VK_SUCCESS) {
            VULKAN_DestroyImage(rendererData, imageOut);
            SDL_Vulkan_SetError("VULKAN_AllocateImage failed", "vkAllocateMemory", result);
            return result;
        }
        result = vkBindImageMemory(rendererData->device, imageOut->image, imageOut->deviceMemory, 0);
        if (result != VK_SUCCESS) {
            VULKAN_DestroyImage(rendererData, imageOut);
            SDL_Vulkan_SetError("VULKAN_AllocateImage failed", "vkBindImageMemory", result);
            return result;
        }
    } else {
        imageOut->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    SDL_zero(imageViewCreateInfo);
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = imageOut->image;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = format;
    imageViewCreateInfo.components = swizzle;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    /* If it's a YCbCr image, we need to pass the conversion info to the VkImageView (and the VkSampler) */
    if (samplerYcbcrConversion != VK_NULL_HANDLE) {
        SDL_zero(samplerYcbcrConversionInfo);
        samplerYcbcrConversionInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO_KHR;
        samplerYcbcrConversionInfo.conversion = samplerYcbcrConversion;
        imageViewCreateInfo.pNext = &samplerYcbcrConversionInfo;
    }

    result = vkCreateImageView(rendererData->device, &imageViewCreateInfo, NULL, &imageOut->imageView);
    if (result != VK_SUCCESS) {
        VULKAN_DestroyImage(rendererData, imageOut);
        SDL_Vulkan_SetError("VULKAN_AllocateImage failed", "vkCreateImageView", result);
        return result;
    }

    return result;
}

static void VULKAN_RecordPipelineImageBarrier(VULKAN_RenderData *rendererData, VkAccessFlags sourceAccessMask, VkAccessFlags destAccessMask,
    VkPipelineStageFlags srcStageFlags, VkPipelineStageFlags dstStageFlags, VkImageLayout destLayout, VkImage image, VkImageLayout *imageLayout)
{
    VkImageMemoryBarrier barrier;

    /* Stop any outstanding renderpass if open */
    if (rendererData->currentRenderPass != VK_NULL_HANDLE) {
        vkCmdEndRenderPass(rendererData->currentCommandBuffer);
        rendererData->currentRenderPass = VK_NULL_HANDLE;
    }

    SDL_zero(barrier);
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = sourceAccessMask;
    barrier.dstAccessMask = destAccessMask;
    barrier.oldLayout = *imageLayout;
    barrier.newLayout = destLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(rendererData->currentCommandBuffer, srcStageFlags, dstStageFlags, 0, 0, NULL, 0, NULL, 1, &barrier);
    *imageLayout = destLayout;
}

static void VULKAN_AcquireNextSwapchainImage(SDL_Renderer *renderer)
{
    VULKAN_RenderData *rendererData = ( VULKAN_RenderData * )renderer->driverdata;
    VkResult result;

    rendererData->currentImageAvailableSemaphore = VK_NULL_HANDLE;
    result = vkAcquireNextImageKHR(rendererData->device, rendererData->swapchain, UINT64_MAX,
        rendererData->imageAvailableSemaphores[rendererData->currentCommandBufferIndex], VK_NULL_HANDLE, &rendererData->currentSwapchainImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_ERROR_SURFACE_LOST_KHR) {
        VULKAN_CreateWindowSizeDependentResources(renderer);
    } else {
        if (result != VK_SUCCESS) {
            if (result != VK_SUBOPTIMAL_KHR) {
                SDL_Vulkan_SetError("VULKAN_AcquireNextSwapchainImage failed", "vkAcquireNextImageKHR", result);
                return;
            }
            /* Suboptimal, but we can contiue */
        }
        rendererData->currentImageAvailableSemaphore = rendererData->imageAvailableSemaphores[rendererData->currentCommandBufferIndex];
    }
}

static void VULKAN_BeginRenderPass(VULKAN_RenderData *rendererData, VkAttachmentLoadOp loadOp, VkClearColorValue *clearColor)
{
    int width = rendererData->swapchainSize.width;
    int height = rendererData->swapchainSize.height;
    VkFramebuffer framebuffer;
    VkRenderPassBeginInfo renderPassBeginInfo;
    VkClearValue clearValue;

    if (rendererData->textureRenderTarget) {
        width = rendererData->textureRenderTarget->width;
        height = rendererData->textureRenderTarget->height;
    }
    switch (loadOp) {
    case VK_ATTACHMENT_LOAD_OP_CLEAR:
        rendererData->currentRenderPass = rendererData->textureRenderTarget ?
            rendererData->textureRenderTarget->mainRenderpasses[SDL_VULKAN_RENDERPASS_CLEAR] :
            rendererData->renderPasses[SDL_VULKAN_RENDERPASS_CLEAR];
        break;

    case VK_ATTACHMENT_LOAD_OP_LOAD:
        rendererData->currentRenderPass = rendererData->textureRenderTarget ?
            rendererData->textureRenderTarget->mainRenderpasses[SDL_VULKAN_RENDERPASS_LOAD] :
            rendererData->renderPasses[SDL_VULKAN_RENDERPASS_LOAD];
        break;
    default:
        SDL_assume(!"Unknown VkAttachmentLoadOp");
        break;
    }

    framebuffer = rendererData->textureRenderTarget ?
        rendererData->textureRenderTarget->mainFramebuffer :
        rendererData->framebuffers[rendererData->currentSwapchainImageIndex];

    SDL_zero(renderPassBeginInfo);
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.pNext = NULL;
    renderPassBeginInfo.renderPass = rendererData->currentRenderPass;
    renderPassBeginInfo.framebuffer = framebuffer;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = width;
    renderPassBeginInfo.renderArea.extent.height = height;
    renderPassBeginInfo.clearValueCount = (clearColor == NULL) ? 0 : 1;
    if (clearColor != NULL) {
        clearValue.color = *clearColor;
        renderPassBeginInfo.pClearValues = &clearValue;
    }
    vkCmdBeginRenderPass(rendererData->currentCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

static void VULKAN_EnsureCommandBuffer(VULKAN_RenderData *rendererData)
{
    if (rendererData->currentCommandBuffer == VK_NULL_HANDLE) {
        rendererData->currentCommandBuffer = rendererData->commandBuffers[rendererData->currentCommandBufferIndex];
        VULKAN_ResetCommandList(rendererData);

        /* Ensure the swapchain is in the correct layout */
        if (rendererData->swapchainImageLayouts[rendererData->currentSwapchainImageIndex] == VK_IMAGE_LAYOUT_UNDEFINED) {
            VULKAN_RecordPipelineImageBarrier(rendererData,
                0,
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                rendererData->swapchainImages[rendererData->currentSwapchainImageIndex],
                &rendererData->swapchainImageLayouts[rendererData->currentSwapchainImageIndex]);
        } else if (rendererData->swapchainImageLayouts[rendererData->currentCommandBufferIndex] != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
            VULKAN_RecordPipelineImageBarrier(rendererData,
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                rendererData->swapchainImages[rendererData->currentSwapchainImageIndex],
                &rendererData->swapchainImageLayouts[rendererData->currentSwapchainImageIndex]);
        }
    }
}

static SDL_bool VULKAN_ActivateCommandBuffer(VULKAN_RenderData *rendererData, VkAttachmentLoadOp loadOp, VkClearColorValue *clearColor, VULKAN_DrawStateCache *stateCache)
{
    VULKAN_EnsureCommandBuffer(rendererData);

    if (rendererData->currentRenderPass == VK_NULL_HANDLE || loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
        if (rendererData->currentRenderPass != VK_NULL_HANDLE) {
            vkCmdEndRenderPass(rendererData->currentCommandBuffer);
            rendererData->currentRenderPass = VK_NULL_HANDLE;
        }
        VULKAN_BeginRenderPass(rendererData, loadOp, clearColor);
    }

    // Bind cached VB now
    if (stateCache->vertexBuffer != VK_NULL_HANDLE) {
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(rendererData->currentCommandBuffer, 0, 1, &stateCache->vertexBuffer, &offset);
    }

    return SDL_TRUE;
}

static void VULKAN_WaitForGPU(VULKAN_RenderData *rendererData)
{
    vkQueueWaitIdle(rendererData->graphicsQueue);
}

static void VULKAN_ResetCommandList(VULKAN_RenderData *rendererData)
{
    VkCommandBufferBeginInfo beginInfo;
    vkResetCommandBuffer(rendererData->currentCommandBuffer, 0);
    for (uint32_t i = 0; i < rendererData->numDescriptorPools[rendererData->currentCommandBufferIndex]; i++) {
        vkResetDescriptorPool(rendererData->device, rendererData->descriptorPools[rendererData->currentCommandBufferIndex][i], 0);
    }

    SDL_zero(beginInfo);
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    vkBeginCommandBuffer(rendererData->currentCommandBuffer, &beginInfo);

    rendererData->currentPipelineState = NULL;
    rendererData->currentVertexBuffer = 0;
    rendererData->issueBatch = SDL_FALSE;
    rendererData->cliprectDirty = SDL_TRUE;
    rendererData->currentDescriptorSetIndex = 0;
    rendererData->currentDescriptorPoolIndex = 0;
    rendererData->currentConstantBufferOffset = -1;
    rendererData->currentConstantBufferIndex = 0;

    /* Release any upload buffers that were inflight */
    for (int i = 0; i < rendererData->currentUploadBuffer[rendererData->currentCommandBufferIndex]; ++i) {
        VULKAN_DestroyBuffer(rendererData, &rendererData->uploadBuffers[rendererData->currentCommandBufferIndex][i]);
    }
    rendererData->currentUploadBuffer[rendererData->currentCommandBufferIndex] = 0;
}

static VkResult VULKAN_IssueBatch(VULKAN_RenderData *rendererData)
{
    VkResult result;
    VkSubmitInfo submitInfo;
    VkPipelineStageFlags waitDestStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    if (rendererData->currentCommandBuffer == VK_NULL_HANDLE) {
        return VK_SUCCESS;
    }

    if (rendererData->currentRenderPass) {
        vkCmdEndRenderPass(rendererData->currentCommandBuffer);
        rendererData->currentRenderPass = VK_NULL_HANDLE;
    }

    rendererData->currentPipelineState = VK_NULL_HANDLE;
    rendererData->viewportDirty = SDL_TRUE;

    vkEndCommandBuffer(rendererData->currentCommandBuffer);

    SDL_zero(submitInfo);
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &rendererData->currentCommandBuffer;
    if (rendererData->waitRenderSemaphoreCount > 0) {
        Uint32 additionalSemaphoreCount = (rendererData->currentImageAvailableSemaphore != VK_NULL_HANDLE) ? 1 : 0;
        submitInfo.waitSemaphoreCount = rendererData->waitRenderSemaphoreCount + additionalSemaphoreCount;
        if (additionalSemaphoreCount > 0) {
            rendererData->waitRenderSemaphores[rendererData->waitRenderSemaphoreCount] = rendererData->currentImageAvailableSemaphore;
            rendererData->waitDestStageMasks[rendererData->waitRenderSemaphoreCount] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }
        submitInfo.pWaitSemaphores = rendererData->waitRenderSemaphores;
        submitInfo.pWaitDstStageMask = rendererData->waitDestStageMasks;
        rendererData->waitRenderSemaphoreCount = 0;
    } else if (rendererData->currentImageAvailableSemaphore != VK_NULL_HANDLE) {
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &rendererData->currentImageAvailableSemaphore;
        submitInfo.pWaitDstStageMask = &waitDestStageMask;
    }

    result = vkQueueSubmit(rendererData->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    rendererData->currentImageAvailableSemaphore = VK_NULL_HANDLE;

    VULKAN_WaitForGPU(rendererData);

    VULKAN_ResetCommandList(rendererData);

    return result;
}

static void VULKAN_DestroyRenderer(SDL_Renderer *renderer)
{
    VULKAN_RenderData *data = (VULKAN_RenderData *)renderer->driverdata;
    SDL_assert(data != NULL);
    if (data->device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(data->device);
    }
    VULKAN_DestroyAll(renderer);
    if (data->vulkan_loaded) {
        SDL_Vulkan_UnloadLibrary();
    }
    SDL_free(data);
    SDL_free(renderer);
}

static void VULKAN_GetOutputSize(SDL_Renderer *renderer, int *w, int *h)
{
    SDL_PrivateGetWindowSizeInPixels(renderer->window, w, h);
}

static VkBlendFactor GetBlendFactor(SDL_BlendFactor factor)
{
    switch (factor) {
    case SDL_BLENDFACTOR_ZERO:
        return VK_BLEND_FACTOR_ZERO;
    case SDL_BLENDFACTOR_ONE:
        return VK_BLEND_FACTOR_ONE;
    case SDL_BLENDFACTOR_SRC_COLOR:
        return VK_BLEND_FACTOR_SRC_COLOR;
    case SDL_BLENDFACTOR_ONE_MINUS_SRC_COLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case SDL_BLENDFACTOR_SRC_ALPHA:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case SDL_BLENDFACTOR_DST_COLOR:
        return VK_BLEND_FACTOR_DST_COLOR;
    case SDL_BLENDFACTOR_ONE_MINUS_DST_COLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case SDL_BLENDFACTOR_DST_ALPHA:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case SDL_BLENDFACTOR_ONE_MINUS_DST_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    default:
        return VK_BLEND_FACTOR_MAX_ENUM;
    }
}

static VkBlendOp GetBlendOp(SDL_BlendOperation operation)
{
    switch (operation) {
    case SDL_BLENDOPERATION_ADD:
        return VK_BLEND_OP_ADD;
    case SDL_BLENDOPERATION_SUBTRACT:
        return VK_BLEND_OP_SUBTRACT;
    case SDL_BLENDOPERATION_REV_SUBTRACT:
        return VK_BLEND_OP_REVERSE_SUBTRACT;
    case SDL_BLENDOPERATION_MINIMUM:
        return VK_BLEND_OP_MIN;
    case SDL_BLENDOPERATION_MAXIMUM:
        return VK_BLEND_OP_MAX;
    default:
        return VK_BLEND_OP_MAX_ENUM;
    }
}


static VULKAN_PipelineState *VULKAN_CreatePipelineState(VULKAN_RenderData *rendererData,
    VULKAN_Shader shader, VkPipelineLayout pipelineLayout, VkDescriptorSetLayout descriptorSetLayout, SDL_BlendMode blendMode, VkPrimitiveTopology topology, VkFormat format)
{
    VULKAN_PipelineState *pipelineStates;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult result = VK_SUCCESS;
    VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = { 0 };
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = { 0 };
    VkVertexInputAttributeDescription attributeDescriptions[3];
    VkVertexInputBindingDescription bindingDescriptions[1];
    VkPipelineShaderStageCreateInfo shaderStageCreateInfo[2];
    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = { 0 };
    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = { 0 };
    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = { 0 };
    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = { 0 };
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = { 0 };
    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = { 0 };
    VkPipelineColorBlendAttachmentState colorBlendAttachment = { 0 };
    VkSampleMask multiSampleMask = 0xFFFFFFFF;
    VkDynamicState dynamicStates[2] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = { 0 };
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.flags = 0;
    pipelineCreateInfo.pStages = shaderStageCreateInfo;
    pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
    pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
    pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
    pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
    pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;

    /* Shaders */
    SDL_zero(shaderStageCreateInfo);
    for (uint32_t i = 0; i < 2; i++) {
        const char *name = "main";
        shaderStageCreateInfo[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageCreateInfo[i].module = (i == 0) ? rendererData->vertexShaderModules[shader] : rendererData->fragmentShaderModules[shader];
        shaderStageCreateInfo[i].stage = (i == 0) ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStageCreateInfo[i].pName = name;
    }
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = &shaderStageCreateInfo[0];

    /* Vertex input */
    vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputCreateInfo.vertexAttributeDescriptionCount = 3;
    vertexInputCreateInfo.pVertexAttributeDescriptions = &attributeDescriptions[0];
    vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
    vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescriptions[0];

    attributeDescriptions[ 0 ].binding = 0;
    attributeDescriptions[ 0 ].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[ 0 ].location = 0;
    attributeDescriptions[ 0 ].offset = 0;
    attributeDescriptions[ 1 ].binding = 0;
    attributeDescriptions[ 1 ].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[ 1 ].location = 1;
    attributeDescriptions[ 1 ].offset = 8;
    attributeDescriptions[ 2 ].binding = 0;
    attributeDescriptions[ 2 ].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[ 2 ].location = 2;
    attributeDescriptions[ 2 ].offset = 16;

    bindingDescriptions[ 0 ].binding = 0;
    bindingDescriptions[ 0 ].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindingDescriptions[ 0 ].stride = 32;

    /* Input assembly */
    inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyStateCreateInfo.topology = ( VkPrimitiveTopology ) topology;
    inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.viewportCount = 1;

    /* Dynamic states */
    dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCreateInfo.dynamicStateCount = SDL_arraysize(dynamicStates);
    dynamicStateCreateInfo.pDynamicStates = dynamicStates;

    /* Rasterization state */
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
    rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
    rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
    rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
    rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
    rasterizationStateCreateInfo.lineWidth = 1.0f;

    /* MSAA state */
    multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCreateInfo.pSampleMask = &multiSampleMask;
    multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    /* Depth Stencil */
    depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    /* Color blend */
    colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = &colorBlendAttachment;
    colorBlendAttachment.blendEnable = blendMode != SDL_BLENDMODE_NONE ? VK_TRUE : VK_FALSE;
    if (colorBlendAttachment.blendEnable != VK_FALSE) {
        SDL_BlendMode longBlendMode = SDL_GetLongBlendMode(blendMode);
        colorBlendAttachment.srcColorBlendFactor = GetBlendFactor(SDL_GetLongBlendModeSrcColorFactor(longBlendMode));
        colorBlendAttachment.srcAlphaBlendFactor = GetBlendFactor(SDL_GetLongBlendModeSrcAlphaFactor(longBlendMode));
        colorBlendAttachment.colorBlendOp = GetBlendOp(SDL_GetLongBlendModeColorOperation(longBlendMode));
        colorBlendAttachment.dstColorBlendFactor = GetBlendFactor(SDL_GetLongBlendModeDstColorFactor(longBlendMode));
        colorBlendAttachment.dstAlphaBlendFactor = GetBlendFactor(SDL_GetLongBlendModeDstAlphaFactor(longBlendMode));
        colorBlendAttachment.alphaBlendOp = GetBlendOp(SDL_GetLongBlendModeAlphaOperation(longBlendMode));
    }
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    /* Renderpass / layout */
    pipelineCreateInfo.renderPass = rendererData->currentRenderPass;
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.layout = pipelineLayout;

    result = vkCreateGraphicsPipelines(rendererData->device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, &pipeline);
    if (result != VK_SUCCESS) {
        SDL_Vulkan_SetError("VULKAN_CreatePipelineState failed", "vkCreateGraphicsPipelines", result);
        return NULL;
    }

    pipelineStates = (VULKAN_PipelineState *)SDL_realloc(rendererData->pipelineStates, (rendererData->pipelineStateCount + 1) * sizeof(*pipelineStates));
    if (!pipelineStates) {
        SDL_OutOfMemory();
        return NULL;
    }
    pipelineStates[rendererData->pipelineStateCount].shader = shader;
    pipelineStates[rendererData->pipelineStateCount].blendMode = blendMode;
    pipelineStates[rendererData->pipelineStateCount].topology = topology;
    pipelineStates[rendererData->pipelineStateCount].format = format;
    pipelineStates[rendererData->pipelineStateCount].pipeline = pipeline;
    pipelineStates[rendererData->pipelineStateCount].descriptorSetLayout = descriptorSetLayout;
    pipelineStates[rendererData->pipelineStateCount].pipelineLayout = pipelineCreateInfo.layout;
    rendererData->pipelineStates = pipelineStates;
    ++rendererData->pipelineStateCount;

    return &pipelineStates[rendererData->pipelineStateCount - 1];
}

static SDL_bool VULKAN_FindMemoryTypeIndex(VULKAN_RenderData *rendererData, uint32_t typeBits, VkMemoryPropertyFlags requiredFlags, VkMemoryPropertyFlags desiredFlags, uint32_t *memoryTypeIndexOut)
{
    uint32_t memoryTypeIndex = 0;
    SDL_bool foundExactMatch = SDL_FALSE;

    /* Desired flags must be a superset of required flags. */
    desiredFlags |= requiredFlags;

    for (memoryTypeIndex = 0; memoryTypeIndex < rendererData->physicalDeviceMemoryProperties.memoryTypeCount; memoryTypeIndex++) {
        if (typeBits & (1 << memoryTypeIndex)) {
            if (rendererData->physicalDeviceMemoryProperties.memoryTypes[memoryTypeIndex].propertyFlags == desiredFlags) {
                foundExactMatch = SDL_TRUE;
                break;
            }
        }
    }
    if (!foundExactMatch) {
        for (memoryTypeIndex = 0; memoryTypeIndex < rendererData->physicalDeviceMemoryProperties.memoryTypeCount; memoryTypeIndex++) {
            if (typeBits & (1 << memoryTypeIndex)) {
                if ((rendererData->physicalDeviceMemoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & requiredFlags) == requiredFlags) {
                    break;
                }
            }
        }
    }

    if (memoryTypeIndex >= rendererData->physicalDeviceMemoryProperties.memoryTypeCount) {
        SDL_SetError("[Vulkan] Unable to find memory type for allocation");
        return SDL_FALSE;
    }
    *memoryTypeIndexOut = memoryTypeIndex;
    return SDL_TRUE;
}

static VkResult VULKAN_CreateVertexBuffer(VULKAN_RenderData *rendererData, size_t vbidx, size_t size)
{
    VkResult result;

    VULKAN_DestroyBuffer(rendererData, &rendererData->vertexBuffers[vbidx]);

    result = VULKAN_AllocateBuffer(rendererData, size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &rendererData->vertexBuffers[vbidx]);

    return result;
}

static VkResult VULKAN_LoadGlobalFunctions()
{
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = SDL_Vulkan_GetVkGetInstanceProcAddr();
    SDL_assert(vkGetInstanceProcAddr != NULL);
#define VULKAN_DEVICE_FUNCTION(name)
#define VULKAN_GLOBAL_FUNCTION(name)                                                   \
    name = (PFN_##name)vkGetInstanceProcAddr(VK_NULL_HANDLE, #name);                   \
    if (!name) {                                                                       \
        SDL_LogError(SDL_LOG_CATEGORY_RENDER,                                          \
                     "vkGetInstanceProcAddr(VK_NULL_HANDLE, \"" #name "\") failed\n"); \
        return SDL_VULKAN_ERROR_UNKNOWN;                                               \
    }
#define VULKAN_INSTANCE_FUNCTION(name)
#define VULKAN_OPTIONAL_INSTANCE_FUNCTION(name)
#define VULKAN_OPTIONAL_DEVICE_FUNCTION(name)
    VULKAN_FUNCTIONS()
#undef VULKAN_DEVICE_FUNCTION
#undef VULKAN_GLOBAL_FUNCTION
#undef VULKAN_INSTANCE_FUNCTION
#undef VULKAN_OPTIONAL_INSTANCE_FUNCTION
#undef VULKAN_OPTIONAL_DEVICE_FUNCTION

    return VK_SUCCESS;
}

static VkResult VULKAN_LoadInstanceFunctions(VULKAN_RenderData *rendererData)
{
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = SDL_Vulkan_GetVkGetInstanceProcAddr();
    SDL_assert(vkGetInstanceProcAddr != NULL);
#define VULKAN_DEVICE_FUNCTION(name)
#define VULKAN_GLOBAL_FUNCTION(name)
#define VULKAN_INSTANCE_FUNCTION(name)                                                      \
    name = (PFN_##name)vkGetInstanceProcAddr(rendererData->instance, #name);  \
    if (!name) {                                                                            \
        SDL_LogError(SDL_LOG_CATEGORY_RENDER,                                               \
                     "vkGetInstanceProcAddr(instance, \"" #name "\") failed\n");            \
        return SDL_VULKAN_ERROR_UNKNOWN;                                                    \
    }
#define VULKAN_OPTIONAL_INSTANCE_FUNCTION(name)                                             \
    name = (PFN_##name)vkGetInstanceProcAddr(rendererData->instance, #name);
#define VULKAN_OPTIONAL_DEVICE_FUNCTION(name)

    VULKAN_FUNCTIONS()
#undef VULKAN_DEVICE_FUNCTION
#undef VULKAN_GLOBAL_FUNCTION
#undef VULKAN_INSTANCE_FUNCTION
#undef VULKAN_OPTIONAL_INSTANCE_FUNCTION
#undef VULKAN_OPTIONAL_DEVICE_FUNCTION

    return VK_SUCCESS;
}

static VkResult VULKAN_LoadDeviceFunctions(VULKAN_RenderData *rendererData)
{
#define VULKAN_DEVICE_FUNCTION(name)                                         \
    name = (PFN_##name)vkGetDeviceProcAddr(rendererData->device, #name);     \
    if (!name) {                                                             \
        SDL_LogError(SDL_LOG_CATEGORY_RENDER,                                \
                     "vkGetDeviceProcAddr(device, \"" #name "\") failed\n"); \
        return SDL_VULKAN_ERROR_UNKNOWN;                                     \
    }
#define VULKAN_GLOBAL_FUNCTION(name)
#define VULKAN_OPTIONAL_DEVICE_FUNCTION(name)                                \
    name = (PFN_##name)vkGetDeviceProcAddr(rendererData->device, #name);
#define VULKAN_INSTANCE_FUNCTION(name)
#define VULKAN_OPTIONAL_INSTANCE_FUNCTION(name)
    VULKAN_FUNCTIONS()
#undef VULKAN_DEVICE_FUNCTION
#undef VULKAN_GLOBAL_FUNCTION
#undef VULKAN_INSTANCE_FUNCTION
#undef VULKAN_OPTIONAL_INSTANCE_FUNCTION
#undef VULKAN_OPTIONAL_DEVICE_FUNCTION
    return VK_SUCCESS;
}

static VkResult VULKAN_FindPhysicalDevice(VULKAN_RenderData *rendererData)
{
    uint32_t physicalDeviceCount = 0;
    VkPhysicalDevice *physicalDevices;
    VkQueueFamilyProperties *queueFamiliesProperties = NULL;
    uint32_t queueFamiliesPropertiesAllocatedSize = 0;
    VkExtensionProperties *deviceExtensions = NULL;
    uint32_t deviceExtensionsAllocatedSize = 0;
    uint32_t physicalDeviceIndex;
    VkResult result;

    result = vkEnumeratePhysicalDevices(rendererData->instance, &physicalDeviceCount, NULL);
    if (result != VK_SUCCESS) {
        SDL_Vulkan_SetError("VULKAN_FindPhysicalDevice query failed", "vkEnumeratePhysicalDevices", result);
        return result;
    }
    if (physicalDeviceCount == 0) {
        SDL_SetError("Vulkan: no physical device found");
        return SDL_VULKAN_ERROR_UNKNOWN;
    }
    physicalDevices = (VkPhysicalDevice *)SDL_malloc(sizeof(VkPhysicalDevice) * physicalDeviceCount);
    if (!physicalDevices) {
        SDL_OutOfMemory();
        return SDL_VULKAN_ERROR_UNKNOWN;
    }
    result = vkEnumeratePhysicalDevices(rendererData->instance, &physicalDeviceCount, physicalDevices);
    if (result != VK_SUCCESS) {
        SDL_free(physicalDevices);
        SDL_Vulkan_SetError("VULKAN_FindPhysicalDevice enumeration failed", "vkEnumeratePhysicalDevices", result);
        return result;
    }
    rendererData->physicalDevice = NULL;
    for (physicalDeviceIndex = 0; physicalDeviceIndex < physicalDeviceCount; physicalDeviceIndex++) {
        uint32_t queueFamiliesCount = 0;
        uint32_t queueFamilyIndex;
        uint32_t deviceExtensionCount = 0;
        SDL_bool hasSwapchainExtension = SDL_FALSE;
        uint32_t i;

        VkPhysicalDevice physicalDevice = physicalDevices[physicalDeviceIndex];
        vkGetPhysicalDeviceProperties(physicalDevice, &rendererData->physicalDeviceProperties);
        if (VK_VERSION_MAJOR(rendererData->physicalDeviceProperties.apiVersion) < 1) {
            continue;
        }
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &rendererData->physicalDeviceMemoryProperties);
        vkGetPhysicalDeviceFeatures(physicalDevice, &rendererData->physicalDeviceFeatures);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, NULL);
        if (queueFamiliesCount == 0) {
            continue;
        }
        if (queueFamiliesPropertiesAllocatedSize < queueFamiliesCount) {
            SDL_free(queueFamiliesProperties);
            queueFamiliesPropertiesAllocatedSize = queueFamiliesCount;
            queueFamiliesProperties = (VkQueueFamilyProperties *)SDL_malloc(sizeof(VkQueueFamilyProperties) * queueFamiliesPropertiesAllocatedSize);
            if (!queueFamiliesProperties) {
                SDL_free(physicalDevices);
                SDL_free(deviceExtensions);
                SDL_OutOfMemory();
                return result;
            }
        }
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, queueFamiliesProperties);
        rendererData->graphicsQueueFamilyIndex = queueFamiliesCount;
        rendererData->presentQueueFamilyIndex = queueFamiliesCount;
        for (queueFamilyIndex = 0; queueFamilyIndex < queueFamiliesCount; queueFamilyIndex++) {
            VkBool32 supported = 0;

            if (queueFamiliesProperties[queueFamilyIndex].queueCount == 0) {
                continue;
            }

            if (queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                rendererData->graphicsQueueFamilyIndex = queueFamilyIndex;
            }

            result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, rendererData->surface, &supported);
            if (result != VK_SUCCESS) {
                SDL_free(physicalDevices);
                SDL_free(queueFamiliesProperties);
                SDL_free(deviceExtensions);
                SDL_Vulkan_SetError("VULKAN_FindPhysicalDevice failed", "vkGetPhysicalDeviceSurfaceSupportKHR", result);
                return result;
            }
            if (supported) {
                rendererData->presentQueueFamilyIndex = queueFamilyIndex;
                if (queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    break; // use this queue because it can present and do graphics
                }
            }
        }

        if (rendererData->graphicsQueueFamilyIndex == queueFamiliesCount) { // no good queues found
            continue;
        }
        if (rendererData->presentQueueFamilyIndex == queueFamiliesCount) { // no good queues found
            continue;
        }
        result = vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &deviceExtensionCount, NULL);
        if (result != VK_SUCCESS) {
            SDL_free(physicalDevices);
            SDL_free(queueFamiliesProperties);
            SDL_free(deviceExtensions);
            SDL_Vulkan_SetError("VULKAN_FindPhysicalDevice query failed", "vkEnumerateDeviceExtensionProperties", result);
            return result;
        }
        if (deviceExtensionCount == 0) {
            continue;
        }
        if (deviceExtensionsAllocatedSize < deviceExtensionCount) {
            SDL_free(deviceExtensions);
            deviceExtensionsAllocatedSize = deviceExtensionCount;
            deviceExtensions = (VkExtensionProperties *)SDL_malloc(sizeof(VkExtensionProperties) * deviceExtensionsAllocatedSize);
            if (!deviceExtensions) {
                SDL_free(physicalDevices);
                SDL_free(queueFamiliesProperties);
                SDL_OutOfMemory();
                return SDL_VULKAN_ERROR_UNKNOWN;
            }
        }
        result = vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &deviceExtensionCount, deviceExtensions);
        if (result != VK_SUCCESS) {
            SDL_free(physicalDevices);
            SDL_free(queueFamiliesProperties);
            SDL_free(deviceExtensions);
            SDL_Vulkan_SetError("VULKAN_FindPhysicalDevice enumeration failed", "vkEnumerateDeviceExtensionProperties", result);
            return result;
        }
        for (i = 0; i < deviceExtensionCount; i++) {
            if (SDL_strcmp(deviceExtensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                hasSwapchainExtension = SDL_TRUE;
                break;
            }
        }
        if (!hasSwapchainExtension) {
            continue;
        }
        rendererData->physicalDevice = physicalDevice;
        break;
    }
    SDL_free(physicalDevices);
    SDL_free(queueFamiliesProperties);
    SDL_free(deviceExtensions);
    if (!rendererData->physicalDevice) {
        SDL_SetError("Vulkan: no viable physical devices found");
        return SDL_VULKAN_ERROR_UNKNOWN;
    }
    return VK_SUCCESS;
}

static int VULKAN_GetSurfaceFormats(VULKAN_RenderData *rendererData)
{
    uint32_t surfaceFormatsCount;
    VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(rendererData->physicalDevice,
                                                           rendererData->surface,
                                                           &surfaceFormatsCount,
                                                           NULL);
    if (result != VK_SUCCESS) {
        SDL_Vulkan_SetError("VULKAN_GetSurfaceFormats query failed", "vkGetPhysicalDeviceSurfaceFormatsKHR", result);
        return 0;
    }
    if (surfaceFormatsCount > rendererData->surfaceFormatsAllocatedCount) {
        VkSurfaceFormatKHR *surfaceFormats = (VkSurfaceFormatKHR *)SDL_realloc(rendererData->surfaceFormats, sizeof(VkSurfaceFormatKHR) * surfaceFormatsCount);
        if (!surfaceFormats) {
            SDL_OutOfMemory();
            return 0;
        }
        rendererData->surfaceFormatsAllocatedCount = surfaceFormatsCount;
        rendererData->surfaceFormats = surfaceFormats;
    }
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(rendererData->physicalDevice,
                                                  rendererData->surface,
                                                  &surfaceFormatsCount,
                                                  rendererData->surfaceFormats);
    if (result != VK_SUCCESS) {
        SDL_Vulkan_SetError("VULKAN_GetSurfaceFormats enumeration failed", "vkGetPhysicalDeviceSurfaceFormatsKHR", result);
        return 0;
    }

    return surfaceFormatsCount;
}

static VkSemaphore VULKAN_CreateSemaphore(VULKAN_RenderData *rendererData)
{
    VkResult result;
    VkSemaphore semaphore = VK_NULL_HANDLE;

    VkSemaphoreCreateInfo semaphoreCreateInfo = { 0 };
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    result = vkCreateSemaphore(rendererData->device, &semaphoreCreateInfo, NULL, &semaphore);
    if (result != VK_SUCCESS) {
        SDL_Vulkan_SetError("VULKAN_CreateSemaphore failed", "vkCreateSemaphore", result);
        return VK_NULL_HANDLE;
    }
    return semaphore;
}

static SDL_bool VULKAN_DeviceExtensionsFound(VULKAN_RenderData *rendererData, uint32_t extensionsToCheck, const char* const* extNames)
{
    VkResult result;
    uint32_t extensionCount;
    VkExtensionProperties *extensionProperties;
    SDL_bool foundExtensions = SDL_TRUE;
    SDL_assert(extensionsToCheck != 0);
    result = vkEnumerateDeviceExtensionProperties(rendererData->physicalDevice, NULL, &extensionCount, NULL);
    if (result != VK_SUCCESS) {
        SDL_Vulkan_SetError("VULKAN_DeviceExtensionsFound query failed", "vkEnumerateInstanceExtensionProperties", result);
        return SDL_FALSE;
    }
    if (extensionCount < extensionsToCheck) {
        return SDL_FALSE;
    }
    extensionProperties = (VkExtensionProperties *)SDL_calloc(sizeof(VkExtensionProperties), extensionCount);
    if (!extensionProperties) {
        SDL_OutOfMemory();
        return SDL_FALSE;
    }
    result = vkEnumerateDeviceExtensionProperties(rendererData->physicalDevice, NULL, &extensionCount, extensionProperties);
    if (result != VK_SUCCESS) {
        SDL_Vulkan_SetError("VULKAN_DeviceExtensionsFound enumeration failed", "vkEnumerateInstanceExtensionProperties", result);
        SDL_free(extensionProperties);
        return SDL_FALSE;
    }
    for (uint32_t ext = 0; ext < extensionsToCheck && foundExtensions; ext++) {
        SDL_bool foundExtension = SDL_FALSE;
        for (uint32_t i = 0; i < extensionCount; i++) {
            if (SDL_strcmp(extensionProperties[i].extensionName, extNames[ext]) == 0) {
                foundExtension = SDL_TRUE;
                break;
            }
        }
        foundExtensions &= foundExtension;
    }

    SDL_free(extensionProperties);

    return foundExtensions;
}

static SDL_bool VULKAN_InstanceExtensionFound(VULKAN_RenderData *rendererData, const char *extName)
{
    uint32_t extensionCount;
    VkResult result = vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);
    if (result != VK_SUCCESS ) {
        SDL_Vulkan_SetError("VULKAN_InstanceExtensionFound query failed", "vkEnumerateInstanceExtensionProperties", result);
        return SDL_FALSE;
    }
    if (extensionCount > 0 ) {
        VkExtensionProperties *extensionProperties = (VkExtensionProperties *)SDL_calloc(extensionCount, sizeof(VkExtensionProperties));
        if (!extensionProperties) {
            SDL_OutOfMemory();
            return SDL_FALSE;
        }
        result = vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, extensionProperties);
        if (result != VK_SUCCESS ) {
            SDL_Vulkan_SetError("VULKAN_InstanceExtensionFound enumeration failed", "vkEnumerateInstanceExtensionProperties", result);
            SDL_free(extensionProperties);
            return SDL_FALSE;
        }
        for (uint32_t i = 0; i < extensionCount; i++) {
            if (SDL_strcmp(extensionProperties[i].extensionName, extName) == 0) {
                SDL_free(extensionProperties);
                return SDL_TRUE;
            }
        }
        SDL_free(extensionProperties);
    }

    return SDL_FALSE;
}

static SDL_bool VULKAN_ValidationLayersFound()
{
    uint32_t instanceLayerCount = 0;
    uint32_t i;
    SDL_bool foundValidation = SDL_FALSE;

    vkEnumerateInstanceLayerProperties(&instanceLayerCount, NULL);
    if (instanceLayerCount > 0) {
        VkLayerProperties *instanceLayers = (VkLayerProperties *)SDL_calloc(instanceLayerCount, sizeof(VkLayerProperties));
        if (!instanceLayers) {
            SDL_OutOfMemory();
            return SDL_FALSE;
        }
        vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayers);
        for (i = 0; i < instanceLayerCount; i++) {
            if (!SDL_strcmp(SDL_VULKAN_VALIDATION_LAYER_NAME, instanceLayers[i].layerName)) {
                foundValidation = SDL_TRUE;
                break;
            }
        }
        SDL_free(instanceLayers);
    }

    return foundValidation;
}

/* Create resources that depend on the device. */
static VkResult VULKAN_CreateDeviceResources(SDL_Renderer *renderer) // , SDL_PropertiesID create_props)
{
    static const char *const deviceExtensionNames[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        /* VK_KHR_sampler_ycbcr_conversion + dependent extensions.
           Note VULKAN_DeviceExtensionsFound() call below, if these get moved in this
           array, update that check too.
       */
        VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
        VK_KHR_MAINTENANCE1_EXTENSION_NAME,
        VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
    };
    VULKAN_RenderData *rendererData = (VULKAN_RenderData *)renderer->driverdata;
    VkResult result = VK_SUCCESS;
    SDL_bool createDebug = SDL_FALSE;
    const char *validationLayerName[] = { SDL_VULKAN_VALIDATION_LAYER_NAME };

    if (SDL_Vulkan_LoadLibrary(NULL) < 0) {
        return SDL_VULKAN_ERROR_UNKNOWN;
    }
    rendererData->vulkan_loaded = SDL_TRUE;

    /* Load global Vulkan functions */
    result = VULKAN_LoadGlobalFunctions();
    if (result != VK_SUCCESS) {
        return result;
    }

    /* Check for colorspace extension */
    rendererData->supportsEXTSwapchainColorspace = VK_FALSE;
#if 0
    if (renderer->output_colorspace == SDL_COLORSPACE_SRGB_LINEAR ||
        renderer->output_colorspace == SDL_COLORSPACE_HDR10) {
        rendererData->supportsEXTSwapchainColorspace = VULKAN_InstanceExtensionFound(rendererData, VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
        if (!rendererData->supportsEXTSwapchainColorspace) {
            SDL_SetError("[Vulkan] Using HDR output but %s not supported", VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
            return SDL_VULKAN_ERROR_UNKNOWN;
        }
    }
#endif
    /* Check for VK_KHR_get_physical_device_properties2 */
    rendererData->supportsKHRGetPhysicalDeviceProperties2 = VULKAN_InstanceExtensionFound(rendererData, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    /* Create VkInstance */
    rendererData->instance = NULL; // (VkInstance)SDL_GetProperty(create_props, SDL_PROP_RENDERER_CREATE_VULKAN_INSTANCE_POINTER, NULL);
    if (rendererData->instance) {
        // rendererData->instance_external = SDL_TRUE;
    } else {
        VkInstanceCreateInfo instanceCreateInfo = { 0 };
        VkApplicationInfo appInfo = { 0 };
        const char *instanceExtensions[2];
        const char **instanceExtensionsCopy;
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.apiVersion = VK_API_VERSION_1_0;
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pApplicationInfo = &appInfo;
        instanceCreateInfo.enabledExtensionCount = 2;
        SDL_Vulkan_GetInstanceExtensions(NULL, &instanceCreateInfo.enabledExtensionCount, instanceExtensions);

        instanceExtensionsCopy = (const char **)SDL_calloc(instanceCreateInfo.enabledExtensionCount + 2, sizeof(const char *));
        if (!instanceExtensionsCopy) {
            SDL_OutOfMemory();
            return SDL_VULKAN_ERROR_UNKNOWN;
        }
        for (uint32_t i = 0; i < instanceCreateInfo.enabledExtensionCount; i++) {
            instanceExtensionsCopy[i] = instanceExtensions[i];
        }
        if (rendererData->supportsEXTSwapchainColorspace) {
            instanceExtensionsCopy[instanceCreateInfo.enabledExtensionCount] = VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME;
            instanceCreateInfo.enabledExtensionCount++;
        }
        if (rendererData->supportsKHRGetPhysicalDeviceProperties2) {
            instanceExtensionsCopy[instanceCreateInfo.enabledExtensionCount] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
            instanceCreateInfo.enabledExtensionCount++;
        }
        instanceCreateInfo.ppEnabledExtensionNames = (const char *const *)instanceExtensionsCopy;
#ifdef DEBUG_RENDER
        createDebug = SDL_GetHintBoolean(SDL_HINT_RENDER_VULKAN_DEBUG, SDL_FALSE);
#endif
        if (createDebug && VULKAN_ValidationLayersFound()) {
            instanceCreateInfo.ppEnabledLayerNames = validationLayerName;
            instanceCreateInfo.enabledLayerCount = 1;
        }
        result = vkCreateInstance(&instanceCreateInfo, NULL, &rendererData->instance);
        SDL_free((void *)instanceExtensionsCopy);
        if (result != VK_SUCCESS) {
            SDL_Vulkan_SetError("VULKAN_CreateDeviceResources failed to create instance", "vkCreateInstance", result);
            return result;
        }
    }

    /* Load instance Vulkan functions */
    result = VULKAN_LoadInstanceFunctions(rendererData);
    if (result != VK_SUCCESS) {
        VULKAN_DestroyAll(renderer);
        return result;
    }

    /* Create Vulkan surface */
    rendererData->surface = 0; // (VkSurfaceKHR)SDL_GetNumberProperty(create_props, SDL_PROP_RENDERER_CREATE_VULKAN_SURFACE_NUMBER, 0);
    if (rendererData->surface) {
        // rendererData->surface_external = SDL_TRUE;
    } else {
        if (!SDL_Vulkan_CreateSurface(renderer->window, rendererData->instance, &rendererData->surface)) {
            return SDL_VULKAN_ERROR_UNKNOWN;
        }
    }

    /* Choose Vulkan physical device */
    rendererData->physicalDevice = NULL; // (VkPhysicalDevice)SDL_GetProperty(create_props, SDL_PROP_RENDERER_CREATE_VULKAN_PHYSICAL_DEVICE_POINTER, NULL);
    if (rendererData->physicalDevice) {
        vkGetPhysicalDeviceMemoryProperties(rendererData->physicalDevice, &rendererData->physicalDeviceMemoryProperties);
        vkGetPhysicalDeviceFeatures(rendererData->physicalDevice, &rendererData->physicalDeviceFeatures);
    } else {
        result = VULKAN_FindPhysicalDevice(rendererData);
        if (result != VK_SUCCESS) {
            return result;
        }
    }
#if 0
    if (SDL_HasProperty(create_props, SDL_PROP_RENDERER_CREATE_VULKAN_GRAPHICS_QUEUE_FAMILY_INDEX_NUMBER)) {
        rendererData->graphicsQueueFamilyIndex = (uint32_t)SDL_GetNumberProperty(create_props, SDL_PROP_RENDERER_CREATE_VULKAN_GRAPHICS_QUEUE_FAMILY_INDEX_NUMBER, 0);
    }
    if (SDL_HasProperty(create_props, SDL_PROP_RENDERER_CREATE_VULKAN_PRESENT_QUEUE_FAMILY_INDEX_NUMBER)) {
        rendererData->presentQueueFamilyIndex = (uint32_t)SDL_GetNumberProperty(create_props, SDL_PROP_RENDERER_CREATE_VULKAN_PRESENT_QUEUE_FAMILY_INDEX_NUMBER, 0);
    }
#endif
    if (rendererData->supportsKHRGetPhysicalDeviceProperties2 &&
        VULKAN_DeviceExtensionsFound(rendererData, 4, &deviceExtensionNames[1])) {
        rendererData->supportsKHRSamplerYCbCrConversion = SDL_TRUE;
    }

    /* Create Vulkan device */
    rendererData->device = NULL; // (VkDevice)SDL_GetProperty(create_props, SDL_PROP_RENDERER_CREATE_VULKAN_DEVICE_POINTER, NULL);
    if (rendererData->device) {
        // rendererData->device_external = SDL_TRUE;
    } else {
        VkPhysicalDeviceSamplerYcbcrConversionFeatures deviceSamplerYcbcrConversionFeatures = { 0 };
        VkDeviceQueueCreateInfo deviceQueueCreateInfo[2] = { { 0 }, { 0 } };
        static const float queuePriority[] = { 1.0f };

        VkDeviceCreateInfo deviceCreateInfo = { 0 };
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.queueCreateInfoCount = 0;
        deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfo;
        deviceCreateInfo.pEnabledFeatures = NULL;
        deviceCreateInfo.enabledExtensionCount = (rendererData->supportsKHRSamplerYCbCrConversion) ? 5 : 1;
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensionNames;

        deviceQueueCreateInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        deviceQueueCreateInfo[0].queueFamilyIndex = rendererData->graphicsQueueFamilyIndex;
        deviceQueueCreateInfo[0].queueCount = 1;
        deviceQueueCreateInfo[0].pQueuePriorities = queuePriority;
        ++deviceCreateInfo.queueCreateInfoCount;

        if (rendererData->presentQueueFamilyIndex != rendererData->graphicsQueueFamilyIndex) {
            deviceQueueCreateInfo[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            deviceQueueCreateInfo[1].queueFamilyIndex = rendererData->presentQueueFamilyIndex;
            deviceQueueCreateInfo[1].queueCount = 1;
            deviceQueueCreateInfo[1].pQueuePriorities = queuePriority;
            ++deviceCreateInfo.queueCreateInfoCount;
        }

        if (rendererData->supportsKHRSamplerYCbCrConversion) {
            deviceSamplerYcbcrConversionFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES;
            deviceSamplerYcbcrConversionFeatures.samplerYcbcrConversion = VK_TRUE;
            deviceSamplerYcbcrConversionFeatures.pNext = (void *)deviceCreateInfo.pNext;
            deviceCreateInfo.pNext = &deviceSamplerYcbcrConversionFeatures;
        }

        result = vkCreateDevice(rendererData->physicalDevice, &deviceCreateInfo, NULL, &rendererData->device);
        if (result != VK_SUCCESS) {
            SDL_Vulkan_SetError("VULKAN_CreateDeviceResources failed to create device", "vkCreateDevice", result);
            return result;
        }
    }

    result = VULKAN_LoadDeviceFunctions(rendererData);
    if (result != VK_SUCCESS) {
        return result;
    }

    /* Get graphics/present queues */
    vkGetDeviceQueue(rendererData->device, rendererData->graphicsQueueFamilyIndex, 0, &rendererData->graphicsQueue);
    if (rendererData->graphicsQueueFamilyIndex != rendererData->presentQueueFamilyIndex) {
        vkGetDeviceQueue(rendererData->device, rendererData->presentQueueFamilyIndex, 0, &rendererData->presentQueue);
    } else {
        rendererData->presentQueue = rendererData->graphicsQueue;
    }

    /* Create command pool/command buffers */
    {
        VkCommandPoolCreateInfo commandPoolCreateInfo = { 0 };
        commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        commandPoolCreateInfo.queueFamilyIndex = rendererData->graphicsQueueFamilyIndex;
        result = vkCreateCommandPool(rendererData->device, &commandPoolCreateInfo, NULL, &rendererData->commandPool);
        if (result != VK_SUCCESS) {
            SDL_Vulkan_SetError("VULKAN_CreateDeviceResources failed to create command pool", "vkCreateCommandPool", result);
            return result;
        }
    }

    rendererData->surfaceFormatsCount = VULKAN_GetSurfaceFormats(rendererData);
    if (!rendererData->surfaceFormatsCount) {
        return SDL_VULKAN_ERROR_UNKNOWN;
    }

    /* Create shaders / layouts */
    for (uint32_t i = 0; i < NUM_SHADERS; i++) {
        VULKAN_Shader shader = (VULKAN_Shader)i;
        VkShaderModuleCreateInfo shaderModuleCreateInfo = { 0 };
        shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        VULKAN_GetVertexShader(shader, &shaderModuleCreateInfo.pCode, &shaderModuleCreateInfo.codeSize);
        result = vkCreateShaderModule(rendererData->device, &shaderModuleCreateInfo, NULL, &rendererData->vertexShaderModules[i]);
        if (result != VK_SUCCESS) {
            SDL_Vulkan_SetError("VULKAN_CreateDeviceResources failed to create vertex shader module", "vkCreateShaderModule", result);
            return result;
        }
        VULKAN_GetPixelShader(shader, &shaderModuleCreateInfo.pCode, &shaderModuleCreateInfo.codeSize);
        result = vkCreateShaderModule(rendererData->device, &shaderModuleCreateInfo, NULL, &rendererData->fragmentShaderModules[i]);
        if (result != VK_SUCCESS) {
            SDL_Vulkan_SetError("VULKAN_CreateDeviceResources failed to create fragment shader module", "vkCreateShaderModule", result);
            return result;
        }
    }

    /* Descriptor set layout / pipeline layout*/
    result = VULKAN_CreateDescriptorSetAndPipelineLayout(rendererData, VK_NULL_HANDLE, &rendererData->descriptorSetLayout, &rendererData->pipelineLayout);
    if (result != VK_SUCCESS) {
        return result;
    }

    /* Create default vertex buffers  */
    for (uint32_t i = 0; i < SDL_VULKAN_NUM_VERTEX_BUFFERS; ++i) {
        result = VULKAN_CreateVertexBuffer(rendererData, i, SDL_VULKAN_VERTEX_BUFFER_DEFAULT_SIZE);
        if (result != VK_SUCCESS) {
            return result;
        }
    }

    /* Create samplers */
    {
        VkSamplerCreateInfo samplerCreateInfo = { 0 };
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
        samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.mipLodBias = 0.0f;
        samplerCreateInfo.anisotropyEnable = VK_FALSE;
        samplerCreateInfo.maxAnisotropy = 1.0f;
        samplerCreateInfo.minLod = 0.0f;
        samplerCreateInfo.maxLod = 1000.0f;
        result = vkCreateSampler(rendererData->device, &samplerCreateInfo, NULL, &rendererData->samplers[SDL_VULKAN_SAMPLER_NEAREST]);
        if (result != VK_SUCCESS) {
            SDL_Vulkan_SetError("VULKAN_CreateDeviceResources failed to create nearest sampler", "vkCreateSampler", result);
            return result;
        }

        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        result = vkCreateSampler(rendererData->device, &samplerCreateInfo, NULL, &rendererData->samplers[SDL_VULKAN_SAMPLER_LINEAR]);
        if (result != VK_SUCCESS) {
            SDL_Vulkan_SetError("VULKAN_CreateDeviceResources failed to create linear sampler", "vkCreateSampler", result);
            return result;
        }
    }
#if 0
    SDL_PropertiesID props = SDL_GetRendererProperties(renderer);
    SDL_SetProperty(props, SDL_PROP_RENDERER_VULKAN_INSTANCE_POINTER, rendererData->instance);
    SDL_SetNumberProperty(props, SDL_PROP_RENDERER_VULKAN_SURFACE_NUMBER, (Sint64)rendererData->surface);
    SDL_SetProperty(props, SDL_PROP_RENDERER_VULKAN_PHYSICAL_DEVICE_POINTER, rendererData->physicalDevice);
    SDL_SetProperty(props, SDL_PROP_RENDERER_VULKAN_DEVICE_POINTER, rendererData->device);
    SDL_SetNumberProperty(props, SDL_PROP_RENDERER_VULKAN_GRAPHICS_QUEUE_FAMILY_INDEX_NUMBER, rendererData->graphicsQueueFamilyIndex);
    SDL_SetNumberProperty(props, SDL_PROP_RENDERER_VULKAN_PRESENT_QUEUE_FAMILY_INDEX_NUMBER, rendererData->presentQueueFamilyIndex);
#endif
    return VK_SUCCESS;
}

static VkResult VULKAN_CreateFramebuffersAndRenderPasses(VULKAN_RenderData *rendererData, int w, int h,
    VkFormat format, int imageViewCount, VkImageView *imageViews, VkFramebuffer *framebuffers, VkRenderPass renderPasses[SDL_VULKAN_NUM_RENDERPASSES])
{
    VkResult result;
    VkAttachmentDescription attachmentDescription;
    VkAttachmentReference colorAttachmentReference;
    VkSubpassDescription subpassDescription;
    VkSubpassDependency subPassDependency;
    VkRenderPassCreateInfo renderPassCreateInfo;
    VkFramebufferCreateInfo framebufferCreateInfo;

    SDL_zero(attachmentDescription);
    attachmentDescription.format = format;
    attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescription.flags = 0;

    SDL_zero(colorAttachmentReference);
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    SDL_zero(subpassDescription);
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.flags = 0;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = NULL;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorAttachmentReference;
    subpassDescription.pResolveAttachments = NULL;
    subpassDescription.pDepthStencilAttachment = NULL;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = NULL;

    SDL_zero(subPassDependency);
    subPassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subPassDependency.dstSubpass = 0;
    subPassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subPassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subPassDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subPassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    subPassDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    SDL_zero(renderPassCreateInfo);
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.flags = 0;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &attachmentDescription;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    renderPassCreateInfo.dependencyCount = 1;
    renderPassCreateInfo.pDependencies = &subPassDependency;

    result = vkCreateRenderPass(rendererData->device, &renderPassCreateInfo, NULL, &renderPasses[SDL_VULKAN_RENDERPASS_LOAD]);
    if (result != VK_SUCCESS) {
        SDL_Vulkan_SetError("VULKAN_CreateFramebuffersAndRenderPasses failed to create the load render-pass", "vkCreateRenderPass", result);
        return result;
    }

    attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    result = vkCreateRenderPass(rendererData->device, &renderPassCreateInfo, NULL, &renderPasses[SDL_VULKAN_RENDERPASS_CLEAR]);
    if (result != VK_SUCCESS) {
        SDL_Vulkan_SetError("VULKAN_CreateFramebuffersAndRenderPasses failed to create the clear render-pass", "vkCreateRenderPass", result);
        return result;
    }

    SDL_zero(framebufferCreateInfo);
    framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCreateInfo.pNext = NULL;
    framebufferCreateInfo.renderPass = rendererData->renderPasses[SDL_VULKAN_RENDERPASS_LOAD];
    framebufferCreateInfo.attachmentCount = 1;
    framebufferCreateInfo.width = w;
    framebufferCreateInfo.height = h;
    framebufferCreateInfo.layers = 1;

    for (int i = 0; i < imageViewCount; i++) {
        framebufferCreateInfo.pAttachments = &imageViews[i];
        result = vkCreateFramebuffer(rendererData->device, &framebufferCreateInfo, NULL, &framebuffers[i]);
        if (result != VK_SUCCESS) {
            SDL_Vulkan_SetError("VULKAN_CreateFramebuffersAndRenderPasses failed to create frame buffers", "vkCreateFramebuffer", result);
            return result;
        }
    }

    return result;
}

static VkResult VULKAN_CreateSwapChain(SDL_Renderer *renderer)
{
    VULKAN_RenderData *rendererData = (VULKAN_RenderData *)renderer->driverdata;
    int w, h;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(rendererData->physicalDevice, rendererData->surface, &rendererData->surfaceCapabilities);
    if (result != VK_SUCCESS) {
        SDL_Vulkan_SetError("VULKAN_CreateSwapChain", "vkGetPhysicalDeviceSurfaceCapabilitiesKHR", result);
        return result;
    }

    // pick an image count
    rendererData->swapchainDesiredImageCount = rendererData->surfaceCapabilities.minImageCount + SDL_VULKAN_FRAME_QUEUE_DEPTH;
    if ((rendererData->swapchainDesiredImageCount > rendererData->surfaceCapabilities.maxImageCount) &&
        (rendererData->surfaceCapabilities.maxImageCount > 0)) {
        rendererData->swapchainDesiredImageCount = rendererData->surfaceCapabilities.maxImageCount;
    }
    // select format
    {
        VkFormat desiredFormat = VK_FORMAT_B8G8R8A8_UNORM;
        VkColorSpaceKHR desiredColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
#if 0
        if (renderer->output_colorspace == SDL_COLORSPACE_SRGB_LINEAR) {
            desiredFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
            desiredColorSpace = VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
        } else if (renderer->output_colorspace == SDL_COLORSPACE_HDR10) {
            desiredFormat = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
            desiredColorSpace = VK_COLOR_SPACE_HDR10_ST2084_EXT;
        }
#endif
        if ((rendererData->surfaceFormatsCount == 1) &&
            (rendererData->surfaceFormats[0].format == VK_FORMAT_UNDEFINED)) {
            // aren't any preferred formats, so we pick
            rendererData->surfaceFormat.colorSpace = desiredColorSpace;
            rendererData->surfaceFormat.format = desiredFormat;
        } else {
            rendererData->surfaceFormat = rendererData->surfaceFormats[0];
//          rendererData->surfaceFormat.colorSpace = rendererData->surfaceFormats[0].colorSpace;
            for (uint32_t i = 0; i < rendererData->surfaceFormatsCount; i++) {
                if (rendererData->surfaceFormats[i].format == desiredFormat &&
                    rendererData->surfaceFormats[i].colorSpace == desiredColorSpace) {
//                  rendererData->surfaceFormat.colorSpace = rendererData->surfaceFormats[i].colorSpace;
                    rendererData->surfaceFormat = rendererData->surfaceFormats[i];
                    break;
                }
            }
        }
    }

    /* The width and height of the swap chain must be based on the display's
     * non-rotated size.
     */
    SDL_GetWindowSizeInPixels(renderer->window, &w, &h);
    rendererData->swapchainSize.width = SDL_clamp((uint32_t)w,
                                          rendererData->surfaceCapabilities.minImageExtent.width,
                                          rendererData->surfaceCapabilities.maxImageExtent.width);

    rendererData->swapchainSize.height = SDL_clamp((uint32_t)h,
                                           rendererData->surfaceCapabilities.minImageExtent.height,
                                           rendererData->surfaceCapabilities.maxImageExtent.height);

    if (rendererData->swapchainSize.width == 0 && rendererData->swapchainSize.height == 0) {
        /* Don't recreate the swapchain if size is (0,0), just fail and continue attempting creation */
        return SDL_VULKAN_ERROR_UNKNOWN; // VK_ERROR_OUT_OF_DATE_KHR;
    }

    /* Choose a present mode. If vsync is requested, then use VK_PRESENT_MODE_FIFO_KHR which is guaranteed to be supported */
    {
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        VkSwapchainCreateInfoKHR swapchainCreateInfo;
        if (!(renderer->info.flags & SDL_RENDERER_PRESENTVSYNC)) {
            uint32_t presentModeCount = 0;
            result = vkGetPhysicalDeviceSurfacePresentModesKHR(rendererData->physicalDevice, rendererData->surface, &presentModeCount, NULL);
            if (result != VK_SUCCESS) {
                SDL_Vulkan_SetError("VULKAN_CreateSwapChain", "vkGetPhysicalDeviceSurfacePresentModesKHR", result);
                return result;
            }
            if (presentModeCount > 0) {
                VkPresentModeKHR *presentModes = (VkPresentModeKHR *)SDL_calloc(presentModeCount, sizeof(VkPresentModeKHR));
                if (!presentModes) {
                    SDL_OutOfMemory();
                    return SDL_VULKAN_ERROR_UNKNOWN;
                }
                result = vkGetPhysicalDeviceSurfacePresentModesKHR(rendererData->physicalDevice, rendererData->surface, &presentModeCount, presentModes);
                if (result != VK_SUCCESS) {
                    SDL_free(presentModes);
                    SDL_Vulkan_SetError("VULKAN_CreateSwapChain", "vkGetPhysicalDeviceSurfacePresentModesKHR", result);
                    return result;
                }

                /* If vsync is not requested, in favor these options in order:
                   VK_PRESENT_MODE_IMMEDIATE_KHR    - no v-sync with tearing
                   VK_PRESENT_MODE_MAILBOX_KHR      - no v-sync without tearing
                   VK_PRESENT_MODE_FIFO_RELAXED_KHR - no v-sync, may tear */
                for (uint32_t i = 0; i < presentModeCount; i++) {
                    if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                        presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
                        break;
                    } else if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                        presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                    } else if ((presentMode != VK_PRESENT_MODE_MAILBOX_KHR) &&
                        (presentModes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR)) {
                        presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
                    }
                }
                SDL_free(presentModes);
            }
        }

        SDL_zero(swapchainCreateInfo);
        swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCreateInfo.surface = rendererData->surface;
        swapchainCreateInfo.minImageCount = rendererData->swapchainDesiredImageCount;
        swapchainCreateInfo.imageFormat = rendererData->surfaceFormat.format;
        swapchainCreateInfo.imageColorSpace = rendererData->surfaceFormat.colorSpace;
        swapchainCreateInfo.imageExtent = rendererData->swapchainSize;
        swapchainCreateInfo.imageArrayLayers = 1;
        swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCreateInfo.preTransform = rendererData->surfaceCapabilities.currentTransform;
        swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainCreateInfo.presentMode = presentMode;
        swapchainCreateInfo.clipped = VK_TRUE;
        swapchainCreateInfo.oldSwapchain = rendererData->swapchain;
        result = vkCreateSwapchainKHR(rendererData->device, &swapchainCreateInfo, NULL, &rendererData->swapchain);

        if (swapchainCreateInfo.oldSwapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(rendererData->device, swapchainCreateInfo.oldSwapchain, NULL);
        }

        if (result != VK_SUCCESS) {
            rendererData->swapchain = VK_NULL_HANDLE;
            SDL_Vulkan_SetError("VULKAN_CreateSwapChain", "vkCreateSwapchainKHR", result);
            return result;
        }
    }
    /* update the swapchain-images */
    {
        uint32_t swapchainImageCount;
        VkImage *swapchainImages;
        SDL_free(rendererData->swapchainImages);
        rendererData->swapchainImages = NULL;
        rendererData->swapchainImageCount = 0;
        result = vkGetSwapchainImagesKHR(rendererData->device, rendererData->swapchain, &swapchainImageCount, NULL);
        if (result != VK_SUCCESS) {
            SDL_Vulkan_SetError("VULKAN_CreateSwapChain", "vkGetSwapchainImagesKHR", result);
            return result;
        }

        swapchainImages = (VkImage *)SDL_malloc(sizeof(VkImage) * swapchainImageCount);
        if (!swapchainImages) {
            SDL_OutOfMemory();
            return SDL_VULKAN_ERROR_UNKNOWN;
        }
        result = vkGetSwapchainImagesKHR(rendererData->device, rendererData->swapchain, &swapchainImageCount, swapchainImages);
        if (result != VK_SUCCESS) {
            SDL_free(swapchainImages);
            SDL_Vulkan_SetError("VULKAN_CreateSwapChain", "vkGetSwapchainImagesKHR", result);
            return result;
        }
        rendererData->swapchainImages = swapchainImages;
        rendererData->swapchainImageCount = swapchainImageCount;
    }

    /* Create VkImageView's for swapchain images */
    {
        VkImageViewCreateInfo imageViewCreateInfo = { 0 };
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.flags = 0;
        imageViewCreateInfo.format = rendererData->surfaceFormat.format;
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        if (rendererData->swapchainImageViews) {
             for (uint32_t i = 0; i < rendererData->swapchainImageCount; i++) {
                 vkDestroyImageView(rendererData->device, rendererData->swapchainImageViews[i], NULL);
             }
             SDL_free(rendererData->swapchainImageViews);
        }
        SDL_free(rendererData->swapchainImageLayouts);
        rendererData->swapchainImageViews = (VkImageView *)SDL_calloc(rendererData->swapchainImageCount, sizeof(VkImageView));
        rendererData->swapchainImageLayouts = (VkImageLayout *)SDL_calloc(rendererData->swapchainImageCount, sizeof(VkImageLayout));
        if (!rendererData->swapchainImageViews || !rendererData->swapchainImageLayouts) {
            SDL_free(rendererData->swapchainImageViews);
            SDL_free(rendererData->swapchainImageLayouts);
            rendererData->swapchainImageViews = NULL;
            rendererData->swapchainImageLayouts = NULL;
            SDL_OutOfMemory();
            return SDL_VULKAN_ERROR_UNKNOWN;
        }

        for (uint32_t i = 0; i < rendererData->swapchainImageCount; i++) {
            imageViewCreateInfo.image = rendererData->swapchainImages[i];
            result = vkCreateImageView(rendererData->device, &imageViewCreateInfo, NULL, &rendererData->swapchainImageViews[i]);
            if (result != VK_SUCCESS) {
                SDL_Vulkan_SetError("VULKAN_CreateSwapChain", "vkCreateImageView", result);
                goto error;
            }
            rendererData->swapchainImageLayouts[i] = VK_IMAGE_LAYOUT_UNDEFINED;
        }

    }

    {
        VkCommandBufferAllocateInfo commandBufferAllocateInfo = { 0 };
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.commandPool = rendererData->commandPool;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount = rendererData->swapchainImageCount;
        if (rendererData->commandBuffers != NULL) {
            vkResetCommandPool(rendererData->device, rendererData->commandPool, 0);
            SDL_free(rendererData->commandBuffers);
            rendererData->currentCommandBuffer = VK_NULL_HANDLE;
            rendererData->currentCommandBufferIndex = 0;
        }
        rendererData->commandBuffers = (VkCommandBuffer *)SDL_calloc(rendererData->swapchainImageCount, sizeof(VkCommandBuffer));
        if (!rendererData->commandBuffers) {
            SDL_OutOfMemory();
            goto error;
        }
        result = vkAllocateCommandBuffers(rendererData->device, &commandBufferAllocateInfo, rendererData->commandBuffers);
        if (result != VK_SUCCESS) {
            SDL_Vulkan_SetError("VULKAN_CreateSwapChain", "vkAllocateCommandBuffers", result);
            goto error;
        }
    }

    /* Create fences */
    if (rendererData->fences) {
        for (uint32_t i = 0; i < rendererData->swapchainImageCount; i++) {
            if (rendererData->fences[i] != VK_NULL_HANDLE) {
                vkDestroyFence(rendererData->device, rendererData->fences[i], NULL);
            }
        }
        SDL_free(rendererData->fences);
    }
    rendererData->fences = (VkFence *)SDL_calloc(rendererData->swapchainImageCount, sizeof(VkFence));
    if (!rendererData->fences) {
        SDL_OutOfMemory();
        goto error;
    }
    for (uint32_t i = 0; i < rendererData->swapchainImageCount; i++) {
        VkFenceCreateInfo fenceCreateInfo = { 0 };
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        result = vkCreateFence(rendererData->device, &fenceCreateInfo, NULL, &rendererData->fences[i]);
        if (result != VK_SUCCESS) {
            SDL_Vulkan_SetError("VULKAN_CreateSwapChain", "vkCreateFence", result);
            goto error;
        }
    }

    /* Create renderpasses and framebuffer */
    if (rendererData->framebuffers) {
        for (uint32_t i = 0; i < rendererData->swapchainImageCount; i++) {
            if (rendererData->framebuffers[i] != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(rendererData->device, rendererData->framebuffers[i], NULL);
            }
        }
        SDL_free(rendererData->framebuffers);
    }
    for (uint32_t i = 0; i < SDL_arraysize(rendererData->renderPasses); i++) {
        if (rendererData->renderPasses[i] != VK_NULL_HANDLE) {
            vkDestroyRenderPass(rendererData->device, rendererData->renderPasses[i], NULL);
            rendererData->renderPasses[i] = VK_NULL_HANDLE;
        }
    }
    rendererData->framebuffers = (VkFramebuffer *)SDL_calloc(rendererData->swapchainImageCount, sizeof(VkFramebuffer));
    if (!rendererData->framebuffers) {
        SDL_OutOfMemory();
        goto error;
    }
    result = VULKAN_CreateFramebuffersAndRenderPasses(rendererData,
        rendererData->swapchainSize.width,
        rendererData->swapchainSize.height,
        rendererData->surfaceFormat.format,
        rendererData->swapchainImageCount,
        rendererData->swapchainImageViews,
        rendererData->framebuffers,
        rendererData->renderPasses);
    if (result != VK_SUCCESS) {
        goto error;
    }

    /* Create descriptor pools - start by allocating one per swapchain image, let it grow if more are needed */
    if (rendererData->descriptorPools) {
        SDL_assert(rendererData->numDescriptorPools);
        for (uint32_t i = 0; i < rendererData->swapchainImageCount; i++) {
            for (uint32_t j = 0; j < rendererData->numDescriptorPools[i]; j++) {
                if (rendererData->descriptorPools[i][j] != VK_NULL_HANDLE) {
                    vkDestroyDescriptorPool(rendererData->device, rendererData->descriptorPools[i][j], NULL);
                }
            }
            SDL_free(rendererData->descriptorPools[i]);
        }
        SDL_free(rendererData->descriptorPools);
        SDL_free(rendererData->numDescriptorPools);
    }
    rendererData->descriptorPools = (VkDescriptorPool **)SDL_calloc(rendererData->swapchainImageCount, sizeof(VkDescriptorPool*));
    rendererData->numDescriptorPools = (uint32_t *)SDL_calloc(rendererData->swapchainImageCount, sizeof(uint32_t));
    for (uint32_t i = 0; i < rendererData->swapchainImageCount; i++) {
        /* Start by just allocating one pool, it will grow if needed */
        VkDescriptorPool descriptorPool;
        VkDescriptorPool *descriptorPoolPtr = (VkDescriptorPool *)SDL_calloc(1, sizeof(VkDescriptorPool));
        if (descriptorPoolPtr == NULL) {
            SDL_OutOfMemory();
            goto error;
        }
        descriptorPool = VULKAN_AllocateDescriptorPool(rendererData);
        if (descriptorPool == VK_NULL_HANDLE) {
            SDL_free(descriptorPoolPtr);
            goto error;
        }
        *descriptorPoolPtr = descriptorPool;
        rendererData->numDescriptorPools[i] = 1;
        rendererData->descriptorPools[i] = descriptorPoolPtr;
    }

    /* Create semaphores */
    if (rendererData->imageAvailableSemaphores) {
        for (uint32_t i = 0; i < rendererData->swapchainImageCount; ++i) {
            if (rendererData->imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(rendererData->device, rendererData->imageAvailableSemaphores[i], NULL);
            }
        }
        SDL_free(rendererData->imageAvailableSemaphores);
    }
    if (rendererData->renderingFinishedSemaphores) {
        for (uint32_t i = 0; i < rendererData->swapchainImageCount; ++i) {
            if (rendererData->renderingFinishedSemaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(rendererData->device, rendererData->renderingFinishedSemaphores[i], NULL);
            }
        }
        SDL_free(rendererData->renderingFinishedSemaphores);
    }
    rendererData->imageAvailableSemaphores = (VkSemaphore *)SDL_calloc(sizeof(VkSemaphore), rendererData->swapchainImageCount);
    rendererData->renderingFinishedSemaphores = (VkSemaphore *)SDL_calloc(sizeof(VkSemaphore), rendererData->swapchainImageCount);
    for (uint32_t i = 0; i < rendererData->swapchainImageCount; i++) {
        rendererData->imageAvailableSemaphores[i] = VULKAN_CreateSemaphore(rendererData);
        if (rendererData->imageAvailableSemaphores[i] == VK_NULL_HANDLE) {
            goto error;
        }
        rendererData->renderingFinishedSemaphores[i] = VULKAN_CreateSemaphore(rendererData);
        if (rendererData->renderingFinishedSemaphores[i] == VK_NULL_HANDLE) {
            goto error;
        }
    }

    /* Upload buffers */
    if (rendererData->uploadBuffers) {
        for (uint32_t i = 0; i < rendererData->swapchainImageCount; i++) {
            for (uint32_t j = 0; j < SDL_VULKAN_NUM_UPLOAD_BUFFERS; j++) {
                VULKAN_DestroyBuffer(rendererData, &rendererData->uploadBuffers[i][j]);
            }
            SDL_free(rendererData->uploadBuffers[i]);
        }
        SDL_free(rendererData->uploadBuffers);
    }
    rendererData->uploadBuffers = (VULKAN_Buffer **)SDL_calloc(rendererData->swapchainImageCount, sizeof(VULKAN_Buffer*));
    if (!rendererData->uploadBuffers) {
        SDL_OutOfMemory();
        goto error;
    }
    for (uint32_t i = 0; i < rendererData->swapchainImageCount; i++) {
        rendererData->uploadBuffers[i] = (VULKAN_Buffer *)SDL_calloc(SDL_VULKAN_NUM_UPLOAD_BUFFERS, sizeof(VULKAN_Buffer));
        if (!rendererData->uploadBuffers[i]) {
            SDL_OutOfMemory();
            goto error;
        }
    }
    SDL_free(rendererData->currentUploadBuffer);
    rendererData->currentUploadBuffer = (int *)SDL_calloc(rendererData->swapchainImageCount, sizeof(int));
    if (!rendererData->currentUploadBuffer) {
        SDL_OutOfMemory();
        goto error;
    }
    /* Constant buffers */
    if (rendererData->constantBuffers) {
        SDL_assert(rendererData->numConstantBuffers);
        for (uint32_t i = 0; i < rendererData->swapchainImageCount; ++i) {
            for (uint32_t j = 0; j < rendererData->numConstantBuffers[i]; j++) {
                VULKAN_DestroyBuffer(rendererData, &rendererData->constantBuffers[i][j]);
            }
            SDL_free(rendererData->constantBuffers[i]);
        }
        SDL_free(rendererData->constantBuffers);
        SDL_free(rendererData->numConstantBuffers);
        rendererData->constantBuffers = NULL;
    }
    rendererData->constantBuffers = (VULKAN_Buffer **)SDL_calloc(rendererData->swapchainImageCount, sizeof(VULKAN_Buffer*));
    rendererData->numConstantBuffers = (uint32_t *)SDL_calloc(rendererData->swapchainImageCount, sizeof(uint32_t));
    for (uint32_t i = 0; i < rendererData->swapchainImageCount; i++) {
        /* Start with just allocating one, will grow if needed */
        rendererData->numConstantBuffers[i] = 1;
        rendererData->constantBuffers[i] = (VULKAN_Buffer *)SDL_calloc(1, sizeof(VULKAN_Buffer));
        if (!rendererData->constantBuffers[i]) {
            SDL_OutOfMemory();
            goto error;
        }
        result = VULKAN_AllocateBuffer(rendererData,
            SDL_VULKAN_CONSTANT_BUFFER_DEFAULT_SIZE,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &rendererData->constantBuffers[i][0]);
        if (result != VK_SUCCESS) {
            goto error;
        }
    }
    rendererData->currentConstantBufferOffset = -1;
    rendererData->currentConstantBufferIndex = 0;

    VULKAN_AcquireNextSwapchainImage(renderer);
#if 0
    SDL_PropertiesID props = SDL_GetRendererProperties(renderer);
    SDL_SetNumberProperty(props, SDL_PROP_RENDERER_VULKAN_SWAPCHAIN_IMAGE_COUNT_NUMBER, rendererData->swapchainImageCount);
#endif
    return 0;
error:
    VULKAN_DestroyAll(renderer);
    return SDL_VULKAN_ERROR_UNKNOWN;
}

/* Initialize all resources that change when the window's size changes. */
static VkResult VULKAN_CreateWindowSizeDependentResources(SDL_Renderer *renderer)
{
    VULKAN_RenderData *rendererData = (VULKAN_RenderData *)renderer->driverdata;
    int result;

    /* Release resources in the current command list */
    VULKAN_IssueBatch(rendererData);
    VULKAN_WaitForGPU(rendererData);

    result = VULKAN_CreateSwapChain(renderer);
    if (result != VK_SUCCESS) {
        rendererData->recreateSwapchain = VK_TRUE;
    }

    rendererData->viewportDirty = SDL_TRUE;

    return result;
}

/* This method is called when the window's size changes. */
static VkResult VULKAN_UpdateForWindowSizeChange(SDL_Renderer *renderer)
{
    VULKAN_RenderData *rendererData = (VULKAN_RenderData *)renderer->driverdata;
    /* If the GPU has previous work, wait for it to be done first */
    VULKAN_WaitForGPU(rendererData);

    return VULKAN_CreateWindowSizeDependentResources(renderer);
}

static void VULKAN_WindowEvent(SDL_Renderer *renderer, const SDL_WindowEvent *event)
{
#if 0
    VULKAN_RenderData *rendererData = (VULKAN_RenderData *)renderer->driverdata;

    if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        rendererData->recreateSwapchain = SDL_TRUE;
    }
#else
    if (event->type == SDL_WINDOWEVENT_SIZE_CHANGED) {
        VULKAN_UpdateForWindowSizeChange(renderer);
    }
#endif
}

static SDL_bool VULKAN_SupportsBlendMode(SDL_Renderer *renderer, SDL_BlendMode blendMode)
{
    SDL_BlendMode longBlendMode = SDL_GetLongBlendMode(blendMode);
    SDL_BlendFactor srcColorFactor = SDL_GetLongBlendModeSrcColorFactor(longBlendMode);
    SDL_BlendFactor srcAlphaFactor = SDL_GetLongBlendModeSrcAlphaFactor(longBlendMode);
    SDL_BlendOperation colorOperation = SDL_GetLongBlendModeColorOperation(longBlendMode);
    SDL_BlendFactor dstColorFactor = SDL_GetLongBlendModeDstColorFactor(longBlendMode);
    SDL_BlendFactor dstAlphaFactor = SDL_GetLongBlendModeDstAlphaFactor(longBlendMode);
    SDL_BlendOperation alphaOperation = SDL_GetLongBlendModeAlphaOperation(longBlendMode);

    if (GetBlendFactor(srcColorFactor) == VK_BLEND_FACTOR_MAX_ENUM ||
        GetBlendFactor(srcAlphaFactor)  == VK_BLEND_FACTOR_MAX_ENUM ||
        GetBlendOp(colorOperation) == VK_BLEND_OP_MAX_ENUM ||
        GetBlendFactor(dstColorFactor) == VK_BLEND_FACTOR_MAX_ENUM ||
        GetBlendFactor(dstAlphaFactor) == VK_BLEND_FACTOR_MAX_ENUM ||
        GetBlendOp(alphaOperation) == VK_BLEND_OP_MAX_ENUM) {
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

static int VULKAN_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture) // , SDL_PropertiesID create_props)
{
    VULKAN_RenderData *rendererData = (VULKAN_RenderData *)renderer->driverdata;
    VULKAN_TextureData *textureData;
    VkResult result;
    VkFormat textureFormat = SDLPixelFormatToVkTextureFormat(texture->format); // , renderer->output_colorspace);
    uint32_t width = texture->w;
    uint32_t height = texture->h;
    VkImageUsageFlags usage;
    VkComponentMapping imageViewSwizzle = rendererData->identitySwizzle;
    if (textureFormat == VK_FORMAT_UNDEFINED) {
        return SDL_SetError("%s, An unsupported SDL pixel format (0x%x) was specified", __FUNCTION__, texture->format);
    }

    textureData = (VULKAN_TextureData *)SDL_calloc(1, sizeof(*textureData));
    if (!textureData) {
        return SDL_OutOfMemory();
    }
    texture->driverdata = textureData;
//    if (SDL_COLORSPACETRANSFER(texture->colorspace) == SDL_TRANSFER_CHARACTERISTICS_SRGB) {
        textureData->shader = SHADER_RGB;
//    } else {
//        textureData->shader = SHADER_ADVANCED;
//    }
    textureData->scaleMode = (texture->scaleMode == SDL_ScaleModeNearest) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;

#if SDL_HAVE_YUV
    /* YUV textures must have even width and height.  Also create Ycbcr conversion */
    if (texture->format == SDL_PIXELFORMAT_YV12 ||
        texture->format == SDL_PIXELFORMAT_IYUV ||
        texture->format == SDL_PIXELFORMAT_NV12 ||
        texture->format == SDL_PIXELFORMAT_NV21 ||
        texture->format == SDL_PIXELFORMAT_P010) {
        const uint32_t YUV_SD_THRESHOLD = 576;
        VkSamplerYcbcrConversionCreateInfoKHR samplerYcbcrConversionCreateInfo;
        VkSamplerCreateInfo samplerCreateInfo;
        VkSamplerYcbcrConversionInfoKHR samplerYcbcrConversionInfo;

        /* Check that we have VK_KHR_sampler_ycbcr_conversion support */
        if (!rendererData->supportsKHRSamplerYCbCrConversion) {
            return SDL_SetError("[Vulkan] YUV textures require a Vulkan device that supports VK_KHR_sampler_ycbcr_conversion");
        }

        SDL_zero(samplerYcbcrConversionCreateInfo);
        samplerYcbcrConversionCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO_KHR;

        /* Pad width/height to multiple of 2 */
        width = (width + 1) & ~1;
        height = (height + 1) & ~1;

        /* Create samplerYcbcrConversion which will be used on the VkImageView and VkSampler */
        samplerYcbcrConversionCreateInfo.format = textureFormat;
#if 0
        switch (SDL_COLORSPACEMATRIX(texture->colorspace)) {
        case SDL_MATRIX_COEFFICIENTS_BT470BG:
        case SDL_MATRIX_COEFFICIENTS_BT601:
            samplerYcbcrConversionCreateInfo.ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601_KHR;
            break;
        case SDL_MATRIX_COEFFICIENTS_BT709:
            samplerYcbcrConversionCreateInfo.ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709_KHR;
            break;
        case SDL_MATRIX_COEFFICIENTS_BT2020_NCL:
            samplerYcbcrConversionCreateInfo.ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020_KHR;
            break;
        case SDL_MATRIX_COEFFICIENTS_UNSPECIFIED:
            if (texture->format == SDL_PIXELFORMAT_P010) {
                samplerYcbcrConversionCreateInfo.ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020_KHR;
            } else if (height > YUV_SD_THRESHOLD) {
                samplerYcbcrConversionCreateInfo.ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709_KHR;
            } else {
                samplerYcbcrConversionCreateInfo.ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601_KHR;
            }
            break;
        default:
            return SDL_SetError("[Vulkan] Unsupported Ycbcr colorspace: %d", SDL_COLORSPACEMATRIX(texture->colorspace));
        }
#else
            if (height > YUV_SD_THRESHOLD) {
                samplerYcbcrConversionCreateInfo.ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709_KHR;
            } else {
                samplerYcbcrConversionCreateInfo.ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601_KHR;
            }
#endif
        samplerYcbcrConversionCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        samplerYcbcrConversionCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        samplerYcbcrConversionCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        samplerYcbcrConversionCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        if (texture->format == SDL_PIXELFORMAT_YV12 ||
            texture->format == SDL_PIXELFORMAT_NV21) {
            samplerYcbcrConversionCreateInfo.components.r = VK_COMPONENT_SWIZZLE_B;
            samplerYcbcrConversionCreateInfo.components.b = VK_COMPONENT_SWIZZLE_R;
        }
#if 0
        switch (SDL_COLORSPACERANGE(texture->colorspace)) {
        case SDL_COLOR_RANGE_LIMITED:
            samplerYcbcrConversionCreateInfo.ycbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_NARROW_KHR;
            break;
        case SDL_COLOR_RANGE_FULL:
        default:
            samplerYcbcrConversionCreateInfo.ycbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_FULL_KHR;
            break;
        }

        switch (SDL_COLORSPACECHROMA(texture->colorspace)) {
        case SDL_CHROMA_LOCATION_LEFT:
            samplerYcbcrConversionCreateInfo.xChromaOffset = VK_CHROMA_LOCATION_COSITED_EVEN_KHR;
            samplerYcbcrConversionCreateInfo.yChromaOffset = VK_CHROMA_LOCATION_MIDPOINT_KHR;
            break;
        case SDL_CHROMA_LOCATION_TOPLEFT:
            samplerYcbcrConversionCreateInfo.xChromaOffset = VK_CHROMA_LOCATION_COSITED_EVEN_KHR;
            samplerYcbcrConversionCreateInfo.yChromaOffset = VK_CHROMA_LOCATION_COSITED_EVEN_KHR;
            break;
        case SDL_CHROMA_LOCATION_NONE:
        case SDL_CHROMA_LOCATION_CENTER:
        default:
            samplerYcbcrConversionCreateInfo.xChromaOffset = VK_CHROMA_LOCATION_MIDPOINT_KHR;
            samplerYcbcrConversionCreateInfo.yChromaOffset = VK_CHROMA_LOCATION_MIDPOINT_KHR;
            break;
        }
#else
            samplerYcbcrConversionCreateInfo.ycbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_FULL_KHR;
            samplerYcbcrConversionCreateInfo.xChromaOffset = VK_CHROMA_LOCATION_MIDPOINT_KHR;
            samplerYcbcrConversionCreateInfo.yChromaOffset = VK_CHROMA_LOCATION_MIDPOINT_KHR;
#endif
        samplerYcbcrConversionCreateInfo.chromaFilter = VK_FILTER_LINEAR;
        samplerYcbcrConversionCreateInfo.forceExplicitReconstruction = VK_FALSE;

        result = vkCreateSamplerYcbcrConversionKHR(rendererData->device, &samplerYcbcrConversionCreateInfo, NULL, &textureData->samplerYcbcrConversion);
        if (result != VK_SUCCESS) {
            return SDL_Vulkan_SetError("VULKAN_CreateTexture failed", "vkCreateSamplerYcbcrConversionKHR", result);
        }

        /* Also create VkSampler object which we will need to pass to the PSO as an immutable sampler */
        SDL_zero(samplerCreateInfo);
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
        samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.mipLodBias = 0.0f;
        samplerCreateInfo.anisotropyEnable = VK_FALSE;
        samplerCreateInfo.maxAnisotropy = 1.0f;
        samplerCreateInfo.minLod = 0.0f;
        samplerCreateInfo.maxLod = 1000.0f;

        SDL_zero(samplerYcbcrConversionInfo);
        samplerYcbcrConversionInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO_KHR;
        samplerYcbcrConversionInfo.conversion = textureData->samplerYcbcrConversion;
        samplerCreateInfo.pNext = &samplerYcbcrConversionInfo;
        result = vkCreateSampler(rendererData->device, &samplerCreateInfo, NULL, &textureData->samplerYcbcr);
        if (result != VK_SUCCESS) {
            return SDL_Vulkan_SetError("VULKAN_CreateTexture failed", "vkCreateSampler", result);
        }

        /* Allocate special descriptor set layout with samplerYcbcr baked as an immutable sampler */
        result = VULKAN_CreateDescriptorSetAndPipelineLayout(rendererData, textureData->samplerYcbcr,
            &textureData->descriptorSetLayoutYcbcr, &textureData->pipelineLayoutYcbcr);
        if (result != VK_SUCCESS) {
            return -1;
        }
    }
#endif
    textureData->width = width;
    textureData->height = height;

    usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (texture->access & SDL_TEXTUREACCESS_TARGET) {
        usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    result = VULKAN_AllocateImage(rendererData/*, create_props*/, width, height, textureFormat, usage, imageViewSwizzle, textureData->samplerYcbcrConversion, &textureData->mainImage);
    if (result != VK_SUCCESS) {
        return -1;
    }
#if 0
    SDL_PropertiesID props = SDL_GetTextureProperties(texture);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_VULKAN_TEXTURE_NUMBER, (Sint64)textureData->mainImage.image);
#endif
    if (texture->access & SDL_TEXTUREACCESS_TARGET) {
        result = VULKAN_CreateFramebuffersAndRenderPasses(rendererData,
            texture->w,
            texture->h,
            textureFormat,
            1,
            &textureData->mainImage.imageView,
            &textureData->mainFramebuffer,
            textureData->mainRenderpasses);
        if (result != VK_SUCCESS) {
            return -1;
        }
    }
    return 0;
}

static void VULKAN_DestroyTexture(SDL_Renderer *renderer,
                                 SDL_Texture *texture)
{
    VULKAN_RenderData *rendererData = (VULKAN_RenderData *)renderer->driverdata;
    VULKAN_TextureData *textureData = (VULKAN_TextureData *)texture->driverdata;

    if (!textureData) {
        return;
    }

    /* Because SDL_DestroyTexture might be called while the data is in-flight, we need to issue the batch first
       Unfortunately, this means that deleting a lot of textures mid-frame will have poor performance. */
    VULKAN_IssueBatch(rendererData);
    VULKAN_WaitForGPU(rendererData);

    VULKAN_DestroyImage(rendererData, &textureData->mainImage);

#if SDL_HAVE_YUV
    if (textureData->samplerYcbcrConversion != VK_NULL_HANDLE) {
        vkDestroySamplerYcbcrConversionKHR(rendererData->device, textureData->samplerYcbcrConversion, NULL);
        textureData->samplerYcbcrConversion = VK_NULL_HANDLE;
    }
    if (textureData->samplerYcbcr != VK_NULL_HANDLE) {
        vkDestroySampler(rendererData->device, textureData->samplerYcbcr, NULL);
        textureData->samplerYcbcr = VK_NULL_HANDLE;
    }
    if (textureData->pipelineLayoutYcbcr != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(rendererData->device, textureData->pipelineLayoutYcbcr, NULL);
        textureData->pipelineLayoutYcbcr = VK_NULL_HANDLE;
    }
    if (textureData->descriptorSetLayoutYcbcr != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rendererData->device, textureData->descriptorSetLayoutYcbcr, NULL);
        textureData->descriptorSetLayoutYcbcr = VK_NULL_HANDLE;
    }
#endif

    VULKAN_DestroyBuffer(rendererData, &textureData->stagingBuffer);
    if (textureData->mainFramebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(rendererData->device, textureData->mainFramebuffer, NULL);
        textureData->mainFramebuffer = VK_NULL_HANDLE;
    }
    for (uint32_t i = 0; i < SDL_arraysize(textureData->mainRenderpasses); i++) {
        if (textureData->mainRenderpasses[i] != VK_NULL_HANDLE) {
            vkDestroyRenderPass(rendererData->device, textureData->mainRenderpasses[i], NULL);
            textureData->mainRenderpasses[i] = VK_NULL_HANDLE;
        }
    }

    SDL_free(textureData);
    texture->driverdata = NULL;
}

static int VULKAN_UpdateTextureInternal(SDL_Renderer *renderer, VULKAN_Image *mainImage, int plane, int x, int y, int w, int h, const void *pixels, int pitch)
{
    VULKAN_RenderData *rendererData = (VULKAN_RenderData *)renderer->driverdata;
    VkFormat format = mainImage->format;
    VkDeviceSize pixelSize = VULKAN_GetBytesPerPixel(format);
    VkDeviceSize length = w * pixelSize;
    VkDeviceSize uploadBufferSize = length * h;
    const Uint8 *src;
    Uint8 *dst;
    VkResult result;
    VkBufferImageCopy region;
    int planeCount = VULKAN_VkFormatGetNumPlanes(format);
    int currentUploadBufferIndex;
    VULKAN_Buffer *uploadBuffer;

    VULKAN_EnsureCommandBuffer(rendererData);

    currentUploadBufferIndex = rendererData->currentUploadBuffer[rendererData->currentCommandBufferIndex];
    uploadBuffer = &rendererData->uploadBuffers[rendererData->currentCommandBufferIndex][currentUploadBufferIndex];

    result = VULKAN_AllocateBuffer(rendererData, uploadBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        uploadBuffer);
    if (result != VK_SUCCESS) {
        return -1;
    }

    src = (const Uint8 *)pixels;
    dst = (Uint8 *)uploadBuffer->mappedBufferPtr;
    if (length == (VkDeviceSize)pitch) {
        SDL_memcpy(dst, src, (size_t)length * h);
    } else {
        if (length > (VkDeviceSize)pitch) {
            length = pitch;
        }
        for (VkDeviceSize row = h; row--; ) {
            SDL_memcpy(dst, src, length);
            src += pitch;
            dst += length;
        }
    }

    /* Make sure the destination is in the correct resource state */
    VULKAN_RecordPipelineImageBarrier(rendererData,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        mainImage->image,
        &mainImage->imageLayout);

    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageSubresource.mipLevel = 0;
    if (planeCount <= 1) {
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    else {
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT << plane;
    }
    region.imageOffset.x = x;
    region.imageOffset.y = y;
    region.imageOffset.z = 0;
    region.imageExtent.width = w;
    region.imageExtent.height = h;
    region.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(rendererData->currentCommandBuffer, uploadBuffer->buffer, mainImage->image, mainImage->imageLayout, 1, &region);

    /* Transition the texture to be shader accessible */
    VULKAN_RecordPipelineImageBarrier(rendererData,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        mainImage->image,
        &mainImage->imageLayout);

    rendererData->currentUploadBuffer[rendererData->currentCommandBufferIndex]++;

    /* If we've used up all the upload buffers, we need to issue the batch */
    if (rendererData->currentUploadBuffer[rendererData->currentCommandBufferIndex] == SDL_VULKAN_NUM_UPLOAD_BUFFERS) {
        VULKAN_IssueBatch(rendererData);
    }

    return 0;
}


static int VULKAN_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture,
                               const SDL_Rect *rect, const void *srcPixels,
                               int srcPitch)
{
    VULKAN_TextureData *textureData = (VULKAN_TextureData *)texture->driverdata;
#if SDL_HAVE_YUV
    Uint32 numPlanes;
#endif
    int retval;
    if (!textureData) {
        return SDL_SetError("Texture is not currently available");
    }

    retval = VULKAN_UpdateTextureInternal(renderer, &textureData->mainImage, 0, rect->x, rect->y, rect->w, rect->h, srcPixels, srcPitch);
    if (retval < 0) {
        return retval;
    }
#if SDL_HAVE_YUV
    numPlanes = VULKAN_VkFormatGetNumPlanes(textureData->mainImage.format);
    /* Skip to the correct offset into the next texture */
    srcPixels = (const void *)((const Uint8 *)srcPixels + rect->h * srcPitch);
    
    if (numPlanes == 3) {
        // YUV data
        for (Uint32 plane = 1; plane < 3; plane++) {
            retval = VULKAN_UpdateTextureInternal(renderer, &textureData->mainImage, plane, rect->x / 2, rect->y / 2, (rect->w + 1) / 2, (rect->h + 1) / 2, srcPixels, (srcPitch + 1) / 2);
            if (retval < 0) {
                break; // return retval;
            }

            /* Skip to the correct offset into the next texture */
            srcPixels = (const void *)((const Uint8 *)srcPixels + ((rect->h + 1) / 2) * ((srcPitch + 1) / 2));
        }
    } else if (numPlanes == 2) {
        // NV12/NV21 data
        if (texture->format == SDL_PIXELFORMAT_P010) {
            srcPitch = (srcPitch + 3) & ~3;
        } else {
            srcPitch = (srcPitch + 1) & ~1;
        }

        retval = VULKAN_UpdateTextureInternal(renderer, &textureData->mainImage, 1, rect->x / 2, rect->y / 2, (rect->w + 1) / 2, (rect->h + 1) / 2, srcPixels, srcPitch);
        // if (retval < 0) {
        //    return retval;
        // }
    }
#endif
    return retval;
}

#if SDL_HAVE_YUV
static int VULKAN_UpdateTextureYUV(SDL_Renderer *renderer, SDL_Texture *texture,
                                  const SDL_Rect *rect,
                                  const Uint8 *Yplane, int Ypitch,
                                  const Uint8 *Uplane, int Upitch,
                                  const Uint8 *Vplane, int Vpitch)
{
    VULKAN_TextureData *textureData = (VULKAN_TextureData *)texture->driverdata;
    int retval;

    if (!textureData) {
        return SDL_SetError("Texture is not currently available");
    }

    retval = VULKAN_UpdateTextureInternal(renderer, &textureData->mainImage, 0, rect->x, rect->y, rect->w, rect->h, Yplane, Ypitch);
    if (retval >= 0) {
        retval = VULKAN_UpdateTextureInternal(renderer, &textureData->mainImage, 1, rect->x / 2, rect->y / 2, rect->w / 2, rect->h / 2, Uplane, Upitch);
        if (retval >= 0) {
            retval = VULKAN_UpdateTextureInternal(renderer, &textureData->mainImage, 2, rect->x / 2, rect->y / 2, rect->w / 2, rect->h / 2, Vplane, Vpitch);
        }
    }
    return retval;
}

static int VULKAN_UpdateTextureNV(SDL_Renderer *renderer, SDL_Texture *texture,
                                 const SDL_Rect *rect,
                                 const Uint8 *Yplane, int Ypitch,
                                 const Uint8 *UVplane, int UVpitch)
{
    VULKAN_TextureData *textureData = (VULKAN_TextureData *)texture->driverdata;
    int retval;

    if (!textureData) {
        return SDL_SetError("Texture is not currently available");
    }

    retval = VULKAN_UpdateTextureInternal(renderer, &textureData->mainImage, 0, rect->x, rect->y, rect->w, rect->h, Yplane, Ypitch);
    if (retval >= 0) {
        retval = VULKAN_UpdateTextureInternal(renderer, &textureData->mainImage, 1, rect->x / 2, rect->y / 2, (rect->w + 1) / 2, (rect->h + 1) / 2, UVplane, UVpitch);
    }
    return retval;
}
#endif

static int VULKAN_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture,
                             const SDL_Rect *rect, void **pixels, int *pitch)
{
    VULKAN_RenderData *rendererData = (VULKAN_RenderData *)renderer->driverdata;
    VULKAN_TextureData *textureData = (VULKAN_TextureData *)texture->driverdata;
    VkDeviceSize pixelSize, length, stagingBufferSize;
    VkResult result;
    if (!textureData) {
        return SDL_SetError("Texture is not currently available");
    }

    if (textureData->stagingBuffer.buffer != VK_NULL_HANDLE) {
        return SDL_SetError("texture is already locked");
    }

    pixelSize = VULKAN_GetBytesPerPixel(textureData->mainImage.format);
    length = rect->w * pixelSize;
    stagingBufferSize = length * rect->h;
    result = VULKAN_AllocateBuffer(rendererData,
        stagingBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &textureData->stagingBuffer);
    if (result != VK_SUCCESS) {
        return -1;
    }

    /* Make note of where the staging texture will be written to
     * (on a call to SDL_UnlockTexture):
     */
    textureData->lockedRect = *rect;

    /* Make sure the caller has information on the texture's pixel buffer,
     * then return:
     */
    *pixels = textureData->stagingBuffer.mappedBufferPtr;
    *pitch = length;
    return 0;

}

static void VULKAN_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    VULKAN_RenderData *rendererData = (VULKAN_RenderData *)renderer->driverdata;
    VULKAN_TextureData *textureData = (VULKAN_TextureData *)texture->driverdata;
    VkBufferImageCopy region;

    if (!textureData) {
        return;
    }

    VULKAN_EnsureCommandBuffer(rendererData);

     /* Make sure the destination is in the correct resource state */
    VULKAN_RecordPipelineImageBarrier(rendererData,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        textureData->mainImage.image,
        &textureData->mainImage.imageLayout);

    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageOffset.x = textureData->lockedRect.x;
    region.imageOffset.y = textureData->lockedRect.y;
    region.imageOffset.z = 0;
    region.imageExtent.width = textureData->lockedRect.w;
    region.imageExtent.height = textureData->lockedRect.h;
    region.imageExtent.depth = 1;
    vkCmdCopyBufferToImage(rendererData->currentCommandBuffer, textureData->stagingBuffer.buffer, textureData->mainImage.image, textureData->mainImage.imageLayout, 1, &region);

    /* Transition the texture to be shader accessible */
    VULKAN_RecordPipelineImageBarrier(rendererData,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        textureData->mainImage.image,
        &textureData->mainImage.imageLayout);

    /* Execute the command list before releasing the staging buffer */
    VULKAN_IssueBatch(rendererData);

    VULKAN_DestroyBuffer(rendererData, &textureData->stagingBuffer);
}

static void VULKAN_SetTextureScaleMode(SDL_Renderer *renderer, SDL_Texture *texture, SDL_ScaleMode scaleMode)
{
    VULKAN_TextureData *textureData = (VULKAN_TextureData *)texture->driverdata;

    if (!textureData) {
        return;
    }

    textureData->scaleMode = (scaleMode == SDL_ScaleModeNearest) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
}

static int VULKAN_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture)
{
    VULKAN_RenderData *rendererData = (VULKAN_RenderData *)renderer->driverdata;
    VULKAN_TextureData *textureData = NULL;

    VULKAN_EnsureCommandBuffer(rendererData);

    if (!texture) {
        if (rendererData->textureRenderTarget) {

            VULKAN_RecordPipelineImageBarrier(rendererData,
                VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                rendererData->textureRenderTarget->mainImage.image,
                &rendererData->textureRenderTarget->mainImage.imageLayout);
        }
        rendererData->textureRenderTarget = NULL;
        return 0;
    }

    textureData = (VULKAN_TextureData *)texture->driverdata;

    if (textureData->mainImage.imageView == VK_NULL_HANDLE) {
        return SDL_SetError("specified texture is not a render target");
    }

    rendererData->textureRenderTarget = textureData;
    VULKAN_RecordPipelineImageBarrier(rendererData,
                VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                rendererData->textureRenderTarget->mainImage.image,
                &rendererData->textureRenderTarget->mainImage.imageLayout);

    return 0;
}

static int VULKAN_QueueNoOp(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    return 0; /* nothing to do in this backend. */
}

static int VULKAN_QueueDrawPoints(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points, int count)
{
    VertexPositionColor *verts = (VertexPositionColor *)SDL_AllocateRenderVertices(renderer, count * sizeof(VertexPositionColor), 0, &cmd->data.draw.first);
    int i;
//    SDL_bool convert_color = SDL_RenderingLinearSpace(renderer);
    SDL_Color color = cmd->data.draw.color;

    if (!verts) {
        return -1;
    }

    cmd->data.draw.count = count;
    for (i = 0; i < count; i++) {
        verts->pos[0] = points[i].x + 0.5f;
        verts->pos[1] = points[i].y + 0.5f;
        verts->tex[0] = 0.0f;
        verts->tex[1] = 0.0f;
        verts->color = color;
#if 0
        if (convert_color) {
            SDL_ConvertToLinear(&verts->color);
        }
#endif
        verts++;
    }
    return 0;
}

static int VULKAN_QueueGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture,
                               const float *xy, int xy_stride, const SDL_Color *color, int color_stride, const float *uv, int uv_stride,
                               int num_vertices, const int *indices,
                               float scale_x, float scale_y)
{
    int i;
    int count = num_vertices;
    VertexPositionColor *verts = (VertexPositionColor *)SDL_AllocateRenderVertices(renderer, count * sizeof(VertexPositionColor), 0, &cmd->data.draw.first);
//    SDL_bool convert_color = SDL_RenderingLinearSpace(renderer);
    VULKAN_TextureData *textureData = texture ? (VULKAN_TextureData *)texture->driverdata : NULL;
    float u_scale = textureData ? (float)texture->w / textureData->width : 0.0f;
    float v_scale = textureData ? (float)texture->h / textureData->height : 0.0f;

    if (!verts) {
        return -1;
    }

    cmd->data.draw.count = count;

    for (i = 0; i < count; i++) {
        int j;
        float *xy_;
        if (indices) {
            j = indices[i];
        } else {
            j = i;
        }

        xy_ = (float *)((char *)xy + j * xy_stride);

        verts->pos[0] = xy_[0] * scale_x;
        verts->pos[1] = xy_[1] * scale_y;
#if 0
        verts->color = *(SDL_FColor *)((char *)color + j * color_stride);
        if (convert_color) {
            SDL_ConvertToLinear(&verts->color);
        }
#else
        verts->color = *(SDL_Color *)((char *)color + j * color_stride);
#endif

        if (texture) {
            float *uv_ = (float *)((char *)uv + j * uv_stride);
            verts->tex[0] = uv_[0] * u_scale;
            verts->tex[1] = uv_[1] * v_scale;
        } else {
            verts->tex[0] = 0.0f;
            verts->tex[1] = 0.0f;
        }

        verts += 1;
    }
    return 0;
}

static SDL_bool VULKAN_UpdateVertexBuffer(VULKAN_RenderData *rendererData,
                                    const void *vertexData, size_t dataSizeInBytes, VULKAN_DrawStateCache *stateCache)
{
    const int vbidx = rendererData->currentVertexBuffer;
    VULKAN_Buffer *vertexBuffer;

    if (dataSizeInBytes == 0) {
        return 0; /* nothing to do. */
    }

    if (rendererData->issueBatch) {
        if (VULKAN_IssueBatch(rendererData) != VK_SUCCESS) {
            SDL_SetError("Failed to issue intermediate batch");
            return SDL_FALSE;
        }
    }
    /* If the existing vertex buffer isn't big enough, we need to recreate a big enough one */
    if (dataSizeInBytes > rendererData->vertexBuffers[vbidx].size) {
        if (VULKAN_CreateVertexBuffer(rendererData, vbidx, dataSizeInBytes) != VK_SUCCESS) {
            return SDL_FALSE;
        }
    }

    vertexBuffer = &rendererData->vertexBuffers[vbidx];
    SDL_memcpy(vertexBuffer->mappedBufferPtr, vertexData, dataSizeInBytes);

    stateCache->vertexBuffer = vertexBuffer->buffer;

    rendererData->currentVertexBuffer++;
    if (rendererData->currentVertexBuffer >= SDL_VULKAN_NUM_VERTEX_BUFFERS) {
        rendererData->currentVertexBuffer = 0;
        rendererData->issueBatch = SDL_TRUE;
    }

    return SDL_TRUE;
}

static int VULKAN_UpdateViewport(VULKAN_RenderData *rendererData)
{
    const SDL_Rect *viewport = &rendererData->currentViewport;
    Float4X4 projection;
    Float4X4 view;
    VkViewport vkViewport;

    if (viewport->w == 0 || viewport->h == 0) {
        /* If the viewport is empty, assume that it is because
         * SDL_CreateRenderer is calling it, and will call it again later
         * with a non-empty viewport.
         */
        /* SDL_Log("%s, no viewport was set!\n", __FUNCTION__); */
        return -1;
    }

    MatrixIdentity(&projection);

    /* Update the view matrix */
    SDL_zero(view);
    view.m[0][0] = 2.0f / viewport->w;
    view.m[1][1] = -2.0f / viewport->h;
    view.m[2][2] = 1.0f;
    view.m[3][0] = -1.0f;
    view.m[3][1] = 1.0f;
    view.m[3][3] = 1.0f;

    MatrixMultiply(&rendererData->vertexShaderConstantsData.projectionAndView, &view, &projection);

    vkViewport.x = viewport->x;
    vkViewport.y = viewport->y;
    vkViewport.width = viewport->w;
    vkViewport.height = viewport->h;
    vkViewport.minDepth = 0.0f;
    vkViewport.maxDepth = 1.0f;
    vkCmdSetViewport(rendererData->currentCommandBuffer, 0, 1, &vkViewport);

    rendererData->viewportDirty = SDL_FALSE;
    return 0;
}

static int VULKAN_UpdateClipRect(VULKAN_RenderData *rendererData)
{
    const SDL_Rect *viewport = &rendererData->currentViewport;

    VkRect2D scissor;
    if (rendererData->currentCliprectEnabled) {
        scissor.offset.x = viewport->x + rendererData->currentCliprect.x;
        scissor.offset.y = viewport->y + rendererData->currentCliprect.y;
        scissor.extent.width = rendererData->currentCliprect.w;
        scissor.extent.height = rendererData->currentCliprect.h;
    } else {
        scissor.offset.x = viewport->x;
        scissor.offset.y = viewport->y;
        scissor.extent.width = viewport->w;
        scissor.extent.height = viewport->h;
    }
    vkCmdSetScissor(rendererData->currentCommandBuffer, 0, 1, &scissor);

    rendererData->cliprectDirty = SDL_FALSE;
    return 0;
}

static void VULKAN_SetupShaderConstants(/*SDL_Renderer *renderer, const SDL_RenderCommand *cmd, */const SDL_Texture *texture, PixelShaderConstants *constants)
{
#if 0
    float output_headroom;
#endif
    SDL_zerop(constants);
#if 0
    constants->scRGB_output = (float)SDL_RenderingLinearSpace(renderer);
    constants->color_scale = cmd->data.draw.color_scale;
#else
    constants->scRGB_output = 0.0;
    constants->color_scale = 0.0;
#endif

    if (texture) {
        switch (texture->format) {
#if SDL_HAVE_YUV
        case SDL_PIXELFORMAT_YV12:
        case SDL_PIXELFORMAT_IYUV:
        case SDL_PIXELFORMAT_NV12:
        case SDL_PIXELFORMAT_NV21:
            constants->input_type = INPUTTYPE_SRGB;
            break;
        case SDL_PIXELFORMAT_P010:
            constants->input_type = INPUTTYPE_HDR10;
            break;
#endif
        default:
#if 0
            if (texture->colorspace == SDL_COLORSPACE_SRGB_LINEAR) {
                constants->input_type = INPUTTYPE_SCRGB;
            } else if (SDL_COLORSPACEPRIMARIES(texture->colorspace) == SDL_COLOR_PRIMARIES_BT2020 &&
                       SDL_COLORSPACETRANSFER(texture->colorspace) == SDL_TRANSFER_CHARACTERISTICS_PQ) {
                constants->input_type = INPUTTYPE_HDR10;
            } else
#endif
            {
                constants->input_type = INPUTTYPE_UNSPECIFIED;
            }
            break;
        }
#if 0
        constants->sdr_white_point = texture->SDR_white_point;

        if (renderer->target) {
            output_headroom = renderer->target->HDR_headroom;
        } else {
            output_headroom = renderer->HDR_headroom;
        }

        if (texture->HDR_headroom > output_headroom) {
            constants->tonemap_method = TONEMAP_CHROME;
            constants->tonemap_factor1 = (output_headroom / (texture->HDR_headroom * texture->HDR_headroom));
            constants->tonemap_factor2 = (1.0f / output_headroom);
        }
#endif
    }
}

static VkDescriptorPool VULKAN_AllocateDescriptorPool(VULKAN_RenderData *rendererData)
{
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorPoolSize descriptorPoolSizes[2];
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
    VkResult result;
    descriptorPoolSizes[0].descriptorCount = SDL_VULKAN_MAX_DESCRIPTOR_SETS;
    descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLER;

    descriptorPoolSizes[1].descriptorCount = SDL_VULKAN_MAX_DESCRIPTOR_SETS;
    descriptorPoolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    SDL_zero(descriptorPoolCreateInfo);
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.poolSizeCount = SDL_arraysize(descriptorPoolSizes);
    descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes;
    descriptorPoolCreateInfo.maxSets = SDL_VULKAN_MAX_DESCRIPTOR_SETS;
    result = vkCreateDescriptorPool(rendererData->device, &descriptorPoolCreateInfo, NULL, &descriptorPool);
    if (result != VK_SUCCESS) {
        SDL_Vulkan_SetError("VULKAN_AllocateDescriptorPool", "vkCreateDescrptorPool", result);
        return VK_NULL_HANDLE;
    }

    return descriptorPool;
}

static VkResult VULKAN_CreateDescriptorSetAndPipelineLayout(VULKAN_RenderData *rendererData, VkSampler samplerYcbcr, VkDescriptorSetLayout *descriptorSetLayoutOut,
    VkPipelineLayout *pipelineLayoutOut)
{
    VkResult result;
    VkDescriptorSetLayoutBinding layoutBindings[2];
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    VkPushConstantRange pushConstantRange;

    /* Descriptor set layout */
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = { 0 };
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.flags = 0;
    /* PixelShaderConstants */
    layoutBindings[0].binding = 1;
    layoutBindings[0].descriptorCount = 1;
    layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBindings[0].pImmutableSamplers = NULL;

    /* Combined image/sampler */
    layoutBindings[1].binding = 0;
    layoutBindings[1].descriptorCount = 1;
    layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBindings[1].pImmutableSamplers = (samplerYcbcr != VK_NULL_HANDLE) ? &samplerYcbcr : NULL;

    descriptorSetLayoutCreateInfo.bindingCount = 2;
    descriptorSetLayoutCreateInfo.pBindings = layoutBindings;
    result = vkCreateDescriptorSetLayout(rendererData->device, &descriptorSetLayoutCreateInfo, NULL, descriptorSetLayoutOut);
    if (result != VK_SUCCESS) {
        SDL_Vulkan_SetError("VULKAN_CreateDescriptorSetAndPipelineLayout", "vkCreateDescriptorSetLayout", result);
        return result;
    }

    /* Pipeline layout */
    SDL_zero(pipelineLayoutCreateInfo);
    pushConstantRange.size = sizeof( VertexShaderConstants );
    pushConstantRange.offset = 0;
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayoutOut;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    result = vkCreatePipelineLayout(rendererData->device, &pipelineLayoutCreateInfo, NULL, pipelineLayoutOut);
    if (result != VK_SUCCESS) {
        SDL_Vulkan_SetError("VULKAN_CreateDescriptorSetAndPipelineLayout", "vkCreatePipelineLayout", result);
        // return result;
    }

    return result;
}

static VkDescriptorSet VULKAN_AllocateDescriptorSet(VULKAN_RenderData *rendererData, VULKAN_Shader shader, VkDescriptorSetLayout descriptorSetLayout,
    VkSampler sampler, VkBuffer constantBuffer, VkDeviceSize constantBufferOffset, VkImageView imageView)
{
    uint32_t currentDescriptorPoolIndex = rendererData->currentDescriptorPoolIndex;
    VkDescriptorPool descriptorPool = rendererData->descriptorPools[rendererData->currentCommandBufferIndex][currentDescriptorPoolIndex];
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkResult result;
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
    VkDescriptorImageInfo combinedImageSamplerDescriptor;
    VkDescriptorBufferInfo bufferDescriptor;
    VkWriteDescriptorSet descriptorWrites[2];
    uint32_t descriptorCount;

    SDL_zero(descriptorSetAllocateInfo);
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool;
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;

    result = (rendererData->currentDescriptorSetIndex >= SDL_VULKAN_MAX_DESCRIPTOR_SETS) ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS;
    if (result == VK_SUCCESS) {
        result = vkAllocateDescriptorSets(rendererData->device, &descriptorSetAllocateInfo, &descriptorSet);
    }
    if (result != VK_SUCCESS) {
        /* Out of descriptor sets in this pool - see if we have more pools allocated */
        currentDescriptorPoolIndex++;
        if (currentDescriptorPoolIndex < rendererData->numDescriptorPools[rendererData->currentCommandBufferIndex]) {
            descriptorPool = rendererData->descriptorPools[rendererData->currentCommandBufferIndex][currentDescriptorPoolIndex];
            descriptorSetAllocateInfo.descriptorPool = descriptorPool;
            result = vkAllocateDescriptorSets(rendererData->device, &descriptorSetAllocateInfo, &descriptorSet);
            if (result != VK_SUCCESS) {
                /* This should not fail - we are allocating from the front of the descriptor set */
                SDL_SetError("[Vulkan] Unable to allocate descriptor set");
                return VK_NULL_HANDLE;
            }
            rendererData->currentDescriptorPoolIndex = currentDescriptorPoolIndex;
            rendererData->currentDescriptorSetIndex = 0;

        }
        /* We are out of pools, create a new one */
        else {
            VkDescriptorPool *descriptorPools;
            descriptorPool = VULKAN_AllocateDescriptorPool(rendererData);
            if (descriptorPool == VK_NULL_HANDLE) {
                /* SDL_SetError called in VULKAN_AllocateDescriptorPool if we failed to allocate a new pool */
                return VK_NULL_HANDLE;
            }
            rendererData->numDescriptorPools[rendererData->currentCommandBufferIndex]++;
            descriptorPools = (VkDescriptorPool *)SDL_realloc(rendererData->descriptorPools[rendererData->currentCommandBufferIndex],
                                                            sizeof(VkDescriptorPool) * rendererData->numDescriptorPools[rendererData->currentCommandBufferIndex]);
            descriptorPools[rendererData->numDescriptorPools[rendererData->currentCommandBufferIndex] - 1] = descriptorPool;
            rendererData->descriptorPools[rendererData->currentCommandBufferIndex] = descriptorPools;
            rendererData->currentDescriptorPoolIndex = currentDescriptorPoolIndex;
            rendererData->currentDescriptorSetIndex = 0;

            /* Call recursively to allocate from the new pool */
            return VULKAN_AllocateDescriptorSet(rendererData, shader, descriptorSetLayout, sampler, constantBuffer, constantBufferOffset, imageView);
        }
    }
    rendererData->currentDescriptorSetIndex++;
    SDL_zero(combinedImageSamplerDescriptor);
    SDL_zero(bufferDescriptor);
    bufferDescriptor.buffer = constantBuffer;
    bufferDescriptor.offset = constantBufferOffset;
    bufferDescriptor.range = sizeof(PixelShaderConstants);

    SDL_zero(descriptorWrites);
    descriptorCount = 1; /* Always have the uniform buffer */

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = descriptorSet;
    descriptorWrites[0].dstBinding = 1;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].pBufferInfo = &bufferDescriptor;

    if (sampler != VK_NULL_HANDLE && imageView != VK_NULL_HANDLE) {
        descriptorCount++;
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSet;
        descriptorWrites[1].dstBinding = 0;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].pImageInfo = &combinedImageSamplerDescriptor;

        /* Ignore the sampler if we're using YcBcCr data since it will be baked in the descriptor set layout */
        if (descriptorSetLayout == rendererData->descriptorSetLayout) {
            combinedImageSamplerDescriptor.sampler = sampler;
        }
        combinedImageSamplerDescriptor.imageView = imageView;
        combinedImageSamplerDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    vkUpdateDescriptorSets(rendererData->device, descriptorCount, descriptorWrites, 0, NULL);

    return descriptorSet;
}

static SDL_bool VULKAN_SetDrawState(VULKAN_RenderData *rendererData, const SDL_RenderCommand *cmd, VULKAN_Shader shader, VkPipelineLayout pipelineLayout, VkDescriptorSetLayout descriptorSetLayout,
    const PixelShaderConstants *shader_constants, VkPrimitiveTopology topology, VkImageView imageView, VkSampler sampler, const Float4X4 *matrix, VULKAN_DrawStateCache *stateCache)

{
    const SDL_BlendMode blendMode = cmd->data.draw.blend;
    VkFormat format = rendererData->surfaceFormat.format;
    const Float4X4 *newmatrix = matrix ? matrix : &rendererData->identity;
    SDL_bool updateConstants = SDL_FALSE;
    PixelShaderConstants solid_constants;
    VkDescriptorSet descriptorSet;
    VkBuffer constantBuffer;
    VkDeviceSize constantBufferOffset;
    int i;
    uint8_t *dst;

    if (!VULKAN_ActivateCommandBuffer(rendererData, VK_ATTACHMENT_LOAD_OP_LOAD, NULL, stateCache)) {
        return SDL_FALSE;
    }

    /* See if we need to change the pipeline state */
    if (!rendererData->currentPipelineState ||
        rendererData->currentPipelineState->shader != shader ||
        rendererData->currentPipelineState->blendMode != blendMode ||
        rendererData->currentPipelineState->topology != topology ||
        rendererData->currentPipelineState->format != format ||
        rendererData->currentPipelineState->pipelineLayout != pipelineLayout ||
        rendererData->currentPipelineState->descriptorSetLayout != descriptorSetLayout) {

        rendererData->currentPipelineState = NULL;
        for (i = 0; i < rendererData->pipelineStateCount; ++i) {
            VULKAN_PipelineState *candidatePiplineState = &rendererData->pipelineStates[i];
            if (candidatePiplineState->shader == shader &&
                candidatePiplineState->blendMode == blendMode &&
                candidatePiplineState->topology == topology &&
                candidatePiplineState->format == format &&
                candidatePiplineState->pipelineLayout == pipelineLayout &&
                candidatePiplineState->descriptorSetLayout == descriptorSetLayout) {
                rendererData->currentPipelineState = candidatePiplineState;
                break;
            }
        }

        /* If we didn't find a match, create a new one -- it must mean the blend mode is non-standard */
        if (!rendererData->currentPipelineState) {
            rendererData->currentPipelineState = VULKAN_CreatePipelineState(rendererData, shader, pipelineLayout, descriptorSetLayout, blendMode, topology, format);
        }

        if (!rendererData->currentPipelineState) {
            return SDL_SetError("[Vulkan] Unable to create required pipeline state");
        }

        vkCmdBindPipeline(rendererData->currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rendererData->currentPipelineState->pipeline);
        updateConstants = SDL_TRUE;
    }

    if (rendererData->viewportDirty) {
        if (VULKAN_UpdateViewport(rendererData) == 0) {
            /* vertexShaderConstantsData.projectionAndView has changed */
            updateConstants = SDL_TRUE;
        }
    }

    if (rendererData->cliprectDirty) {
        VULKAN_UpdateClipRect(rendererData);
    }

    if (updateConstants == SDL_TRUE || SDL_memcmp(&rendererData->vertexShaderConstantsData.model, newmatrix, sizeof(*newmatrix)) != 0) {
        SDL_memcpy(&rendererData->vertexShaderConstantsData.model, newmatrix, sizeof(*newmatrix));
        vkCmdPushConstants(rendererData->currentCommandBuffer, rendererData->currentPipelineState->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
            sizeof(rendererData->vertexShaderConstantsData),
            &rendererData->vertexShaderConstantsData);
    }

    if (!shader_constants) {
        VULKAN_SetupShaderConstants(/*renderer, cmd, */NULL, &solid_constants);
        shader_constants = &solid_constants;
    }

    constantBuffer = rendererData->constantBuffers[rendererData->currentCommandBufferIndex][rendererData->currentConstantBufferIndex].buffer;
    constantBufferOffset = (rendererData->currentConstantBufferOffset < 0) ? 0 : rendererData->currentConstantBufferOffset;
    if (updateConstants ||
        SDL_memcmp(shader_constants, &rendererData->currentPipelineState->shader_constants, sizeof(*shader_constants)) != 0) {

        if (rendererData->currentConstantBufferOffset == -1) {
            /* First time, grab offset 0 */
            rendererData->currentConstantBufferOffset = 0;
            constantBufferOffset = 0;
        } else {
            /* Align the next address to the minUniformBufferOffsetAlignment */
            VkDeviceSize alignment = rendererData->physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
            SDL_assert(rendererData->currentConstantBufferOffset >= 0 );
            rendererData->currentConstantBufferOffset += (int32_t)(sizeof(PixelShaderConstants) + alignment - 1) & ~(alignment - 1);
            constantBufferOffset = rendererData->currentConstantBufferOffset;
        }

        /* If we have run out of size in this constant buffer, create another if needed */
        if (rendererData->currentConstantBufferOffset >= SDL_VULKAN_CONSTANT_BUFFER_DEFAULT_SIZE) {
            uint32_t newConstantBufferIndex = (rendererData->currentConstantBufferIndex + 1);
            /* We need a new constant buffer */
            if (newConstantBufferIndex >= rendererData->numConstantBuffers[rendererData->currentCommandBufferIndex]) {
                VULKAN_Buffer newConstantBuffer;
                VULKAN_Buffer *newConstantBuffers;
                VkResult result = VULKAN_AllocateBuffer(rendererData,
                    SDL_VULKAN_CONSTANT_BUFFER_DEFAULT_SIZE,
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    &newConstantBuffer);

                if (result != VK_SUCCESS) {
                    return SDL_FALSE;
                }

                rendererData->numConstantBuffers[rendererData->currentCommandBufferIndex]++;
                newConstantBuffers = (VULKAN_Buffer *)SDL_realloc(rendererData->constantBuffers[rendererData->currentCommandBufferIndex],
                                                                sizeof(VULKAN_Buffer) * rendererData->numConstantBuffers[rendererData->currentCommandBufferIndex]);
                if (!newConstantBuffers) {
                    SDL_OutOfMemory();
                    return SDL_FALSE;
                }
                newConstantBuffers[rendererData->numConstantBuffers[rendererData->currentCommandBufferIndex] - 1] = newConstantBuffer;
                rendererData->constantBuffers[rendererData->currentCommandBufferIndex] = newConstantBuffers;
            }
            rendererData->currentConstantBufferIndex = newConstantBufferIndex;
            rendererData->currentConstantBufferOffset = 0;
            constantBufferOffset = 0;
            constantBuffer = rendererData->constantBuffers[rendererData->currentCommandBufferIndex][rendererData->currentConstantBufferIndex].buffer;
        }

        SDL_memcpy(&rendererData->currentPipelineState->shader_constants, shader_constants, sizeof(*shader_constants));

        /* Upload constants to persistently mapped buffer */
        dst = (uint8_t *)rendererData->constantBuffers[rendererData->currentCommandBufferIndex][rendererData->currentConstantBufferIndex].mappedBufferPtr;
        dst += constantBufferOffset;
        SDL_memcpy(dst, &rendererData->currentPipelineState->shader_constants, sizeof(PixelShaderConstants));
    }

    /* Allocate/update descriptor set with the bindings */
    descriptorSet = VULKAN_AllocateDescriptorSet(rendererData, shader, descriptorSetLayout, sampler, constantBuffer, constantBufferOffset, imageView);
    if (descriptorSet == VK_NULL_HANDLE) {
        return SDL_FALSE;
    }

    /* Bind the descriptor set with the sampler/UBO/image views */
    vkCmdBindDescriptorSets(rendererData->currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rendererData->currentPipelineState->pipelineLayout,
            0, 1, &descriptorSet, 0, NULL);

    return SDL_TRUE;
}


static SDL_bool VULKAN_SetCopyState(VULKAN_RenderData *rendererData, const SDL_RenderCommand *cmd, const Float4X4 *matrix, VULKAN_DrawStateCache *stateCache)
{
    SDL_Texture *texture = cmd->data.draw.texture;
    VULKAN_TextureData *textureData = (VULKAN_TextureData *)texture->driverdata;
    VkSampler textureSampler = VK_NULL_HANDLE;
    PixelShaderConstants constants;
    VkDescriptorSetLayout descriptorSetLayout = (textureData->descriptorSetLayoutYcbcr != VK_NULL_HANDLE) ? textureData->descriptorSetLayoutYcbcr : rendererData->descriptorSetLayout;
    VkPipelineLayout pipelineLayout = (textureData->pipelineLayoutYcbcr != VK_NULL_HANDLE) ? textureData->pipelineLayoutYcbcr : rendererData->pipelineLayout;

    VULKAN_SetupShaderConstants(/*renderer, cmd, */texture, &constants);

    switch (textureData->scaleMode) {
    case VK_FILTER_NEAREST:
        textureSampler = rendererData->samplers[SDL_VULKAN_SAMPLER_NEAREST];
        break;
    case VK_FILTER_LINEAR:
        textureSampler = rendererData->samplers[SDL_VULKAN_SAMPLER_LINEAR];
        break;
    default:
        return SDL_SetError("Unknown scale mode: %d", textureData->scaleMode);
    }

    if (textureData->mainImage.imageLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        SDL_bool stoppedRenderPass = SDL_FALSE;
        if (rendererData->currentRenderPass != VK_NULL_HANDLE) {
            vkCmdEndRenderPass(rendererData->currentCommandBuffer);
            rendererData->currentRenderPass = VK_NULL_HANDLE;
            stoppedRenderPass = SDL_TRUE;
        }

        VULKAN_RecordPipelineImageBarrier(rendererData,
            VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            textureData->mainImage.image,
            &textureData->mainImage.imageLayout);

        if (stoppedRenderPass) {
            VULKAN_BeginRenderPass(rendererData, VK_ATTACHMENT_LOAD_OP_LOAD, NULL);
        }
    }

    return VULKAN_SetDrawState(rendererData, cmd, textureData->shader, pipelineLayout, descriptorSetLayout, &constants, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, textureData->mainImage.imageView, textureSampler, matrix, stateCache);
}

static void VULKAN_DrawPrimitives(VULKAN_RenderData *rendererData, VkPrimitiveTopology primitiveTopology, const size_t vertexStart, const size_t vertexCount)
{
    vkCmdDraw(rendererData->currentCommandBuffer, (uint32_t)vertexCount, 1, (uint32_t)vertexStart, 0);
}

static void VULKAN_InvalidateCachedState(VULKAN_RenderData *rendererData)
{
    rendererData->currentPipelineState = NULL;
    rendererData->cliprectDirty = SDL_TRUE;
}

static int VULKAN_RunCommandQueue(SDL_Renderer *renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize)
{
    VULKAN_RenderData *rendererData = (VULKAN_RenderData *)renderer->driverdata;
    VULKAN_DrawStateCache stateCache;
    SDL_memset(&stateCache, 0, sizeof(stateCache));

    if (rendererData->recreateSwapchain) {
        if (VULKAN_UpdateForWindowSizeChange(renderer) != VK_SUCCESS) {
            return -1;
        }
        rendererData->recreateSwapchain = SDL_FALSE;
    }

    if (VULKAN_UpdateVertexBuffer(rendererData, vertices, vertsize, &stateCache) < 0) {
        return -1;
    }

    while (cmd) {
        switch (cmd->command) {
        case SDL_RENDERCMD_SETDRAWCOLOR:
        {
            break; /* this isn't currently used in this render backend. */
        }

        case SDL_RENDERCMD_SETVIEWPORT:
        {
            SDL_Rect *viewport = &rendererData->currentViewport;
            if (SDL_memcmp(viewport, &cmd->data.viewport.rect, sizeof(cmd->data.viewport.rect)) != 0) {
                SDL_copyp(viewport, &cmd->data.viewport.rect);
                rendererData->viewportDirty = SDL_TRUE;
                rendererData->cliprectDirty = SDL_TRUE;
            }
            break;
        }

        case SDL_RENDERCMD_SETCLIPRECT:
        {
            const SDL_Rect *rect = &cmd->data.cliprect.rect;
            if (rendererData->currentCliprectEnabled != cmd->data.cliprect.enabled) {
                rendererData->currentCliprectEnabled = cmd->data.cliprect.enabled;
                rendererData->cliprectDirty = SDL_TRUE;
            }
            if (SDL_memcmp(&rendererData->currentCliprect, rect, sizeof(*rect)) != 0) {
                SDL_copyp(&rendererData->currentCliprect, rect);
                rendererData->cliprectDirty = SDL_TRUE;
            }
            break;
        }

        case SDL_RENDERCMD_CLEAR:
        {
#if 0
            SDL_bool convert_color = SDL_RenderingLinearSpace(renderer);
            SDL_FColor color = cmd->data.color.color;
            if (convert_color) {
                SDL_ConvertToLinear(&color);
            }
            color.r *= cmd->data.color.color_scale;
            color.g *= cmd->data.color.color_scale;
            color.b *= cmd->data.color.color_scale;

            VkClearColorValue clearColor;
            clearColor.float32[0] = color.r;
            clearColor.float32[1] = color.g;
            clearColor.float32[2] = color.b;
            clearColor.float32[3] = color.a;
#else
            VkClearColorValue clearColor;
            clearColor.float32[0] = cmd->data.color.color.r / 255.0f;
            clearColor.float32[1] = cmd->data.color.color.g / 255.0f;
            clearColor.float32[2] = cmd->data.color.color.b / 255.0f;
            clearColor.float32[3] = cmd->data.color.color.a / 255.0f;
#endif
            VULKAN_ActivateCommandBuffer(rendererData, VK_ATTACHMENT_LOAD_OP_CLEAR, &clearColor, &stateCache);
            break;
        }

        case SDL_RENDERCMD_DRAW_POINTS:
        {
            const size_t count = cmd->data.draw.count;
            const size_t first = cmd->data.draw.first;
            const size_t start = first / sizeof(VertexPositionColor);
            VULKAN_SetDrawState(rendererData, cmd, SHADER_SOLID, rendererData->pipelineLayout, rendererData->descriptorSetLayout, NULL, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_NULL_HANDLE, VK_NULL_HANDLE, NULL, &stateCache);
            VULKAN_DrawPrimitives(rendererData, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, start, count);
            break;
        }

        case SDL_RENDERCMD_DRAW_LINES:
        {
            const size_t count = cmd->data.draw.count;
            const size_t first = cmd->data.draw.first;
            const size_t start = first / sizeof(VertexPositionColor);
            const VertexPositionColor *verts = (VertexPositionColor *)(((Uint8 *)vertices) + first);
            VULKAN_SetDrawState(rendererData, cmd, SHADER_SOLID, rendererData->pipelineLayout, rendererData->descriptorSetLayout, NULL, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, VK_NULL_HANDLE, VK_NULL_HANDLE, NULL, &stateCache);
            VULKAN_DrawPrimitives(rendererData, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, start, count);
            if (verts[0].pos[0] != verts[count - 1].pos[0] || verts[0].pos[1] != verts[count - 1].pos[1]) {
                VULKAN_SetDrawState(rendererData, cmd, SHADER_SOLID, rendererData->pipelineLayout, rendererData->descriptorSetLayout, NULL, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_NULL_HANDLE, VK_NULL_HANDLE, NULL, &stateCache);
                VULKAN_DrawPrimitives(rendererData, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, start + (count - 1), 1);
            }
            break;
        }

        case SDL_RENDERCMD_FILL_RECTS: /* unused */
            break;

        case SDL_RENDERCMD_COPY: /* unused */
            break;

        case SDL_RENDERCMD_COPY_EX: /* unused */
            break;

        case SDL_RENDERCMD_GEOMETRY:
        {
            SDL_Texture *texture = cmd->data.draw.texture;
            const size_t count = cmd->data.draw.count;
            const size_t first = cmd->data.draw.first;
            const size_t start = first / sizeof(VertexPositionColor);

            if (texture) {
                VULKAN_SetCopyState(rendererData, cmd, NULL, &stateCache);
            } else {
                VULKAN_SetDrawState(rendererData, cmd, SHADER_SOLID, rendererData->pipelineLayout, rendererData->descriptorSetLayout, NULL, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_NULL_HANDLE, VK_NULL_HANDLE, NULL, &stateCache);
            }

            VULKAN_DrawPrimitives(rendererData, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, start, count);
            break;
        }

        case SDL_RENDERCMD_NO_OP:
            break;

        default:
            SDL_assume(!"Unknown vulkanrender-command");
            break;
        }

        cmd = cmd->next;
    }
    return 0;
}

static int VULKAN_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect,
                                  Uint32 format, void *pixels, int pitch)
{
    VULKAN_RenderData *rendererData = (VULKAN_RenderData *)renderer->driverdata;
    VkImage backBuffer;
    VkImageLayout *imageLayout;
    VULKAN_Buffer readbackBuffer;
    VkDeviceSize pixelSize;
    VkDeviceSize length;
    VkDeviceSize readbackBufferSize;
    VkFormat vkFormat;
    VkBufferImageCopy region;
    int status;

    VULKAN_EnsureCommandBuffer(rendererData);

    /* Stop any outstanding renderpass if open */
    if (rendererData->currentRenderPass != VK_NULL_HANDLE) {
        vkCmdEndRenderPass(rendererData->currentCommandBuffer);
        rendererData->currentRenderPass = VK_NULL_HANDLE;
    }

    if (rendererData->textureRenderTarget) {
        backBuffer = rendererData->textureRenderTarget->mainImage.image;
        imageLayout = &rendererData->textureRenderTarget->mainImage.imageLayout;
        vkFormat = rendererData->textureRenderTarget->mainImage.format;
    } else {
        backBuffer = rendererData->swapchainImages[rendererData->currentSwapchainImageIndex];
        imageLayout = &rendererData->swapchainImageLayouts[rendererData->currentSwapchainImageIndex];
        vkFormat = rendererData->surfaceFormat.format;
    }

    pixelSize = VULKAN_GetBytesPerPixel(vkFormat);
    length = rect->w * pixelSize;
    readbackBufferSize = length * rect->h;
    if (VULKAN_AllocateBuffer(rendererData, readbackBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &readbackBuffer) != VK_SUCCESS) {
        return -1;
    }


    /* Make sure the source is in the correct resource state */
    VULKAN_RecordPipelineImageBarrier(rendererData,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        backBuffer,
        imageLayout);

    /* Copy the image to the readback buffer */
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageOffset.x = rect->x;
    region.imageOffset.y = rect->y;
    region.imageOffset.z = 0;
    region.imageExtent.width = rect->w;
    region.imageExtent.height = rect->h;
    region.imageExtent.depth = 1;
    vkCmdCopyImageToBuffer(rendererData->currentCommandBuffer, backBuffer, *imageLayout, readbackBuffer.buffer, 1, &region);

    /* We need to issue the command list for the copy to finish */
    VULKAN_IssueBatch(rendererData);

    /* Transition the render target back to a render target */
     VULKAN_RecordPipelineImageBarrier(rendererData,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        backBuffer,
        imageLayout);

#if 0
    output = SDL_DuplicatePixels(
        rect->w, rect->h,
        VULKAN_VkFormatToSDLPixelFormat(vkFormat),
        renderer->target ? renderer->target->colorspace : renderer->output_colorspace,
        readbackBuffer.mappedBufferPtr,
        length);
#else
    // output = SDL_CreateSurfaceFrom(readbackBuffer.mappedBufferPtr, rect->w, rect->h, length, VULKAN_VkFormatToSDLPixelFormat(vkFormat));

     /* Copy the data into the desired buffer, converting pixels to the
     * desired format at the same time:
     */
    status = SDL_ConvertPixels(
        rect->w, rect->h,
        VULKAN_VkFormatToSDLPixelFormat(vkFormat),
        readbackBuffer.mappedBufferPtr,
        length,
        format,
        pixels,
        pitch);
#endif
    VULKAN_DestroyBuffer(rendererData, &readbackBuffer);

    return status;
}
#if 0
static int VULKAN_AddVulkanRenderSemaphores(SDL_Renderer *renderer, Uint32 wait_stage_mask, Sint64 wait_semaphore, Sint64 signal_semaphore)
{
    VULKAN_RenderData *rendererData = (VULKAN_RenderData *)renderer->driverdata;

    if (wait_semaphore) {
        if (rendererData->waitRenderSemaphoreCount == rendererData->waitRenderSemaphoreMax) {
            /* Allocate an additional one at the end for the normal present wait */
            VkSemaphore *semaphores;
            VkPipelineStageFlags *waitDestStageMasks = (VkPipelineStageFlags *)SDL_realloc(rendererData->waitDestStageMasks, (rendererData->waitRenderSemaphoreMax + 2) * sizeof(*waitDestStageMasks));
            if (!waitDestStageMasks) {
                return SDL_OutOfMemory();
            }
            rendererData->waitDestStageMasks = waitDestStageMasks;

            semaphores = (VkSemaphore *)SDL_realloc(rendererData->waitRenderSemaphores, (rendererData->waitRenderSemaphoreMax + 2) * sizeof(*semaphores));
            if (!semaphores) {
                return SDL_OutOfMemory();
            }
            rendererData->waitRenderSemaphores = semaphores;
            ++rendererData->waitRenderSemaphoreMax;
        }
        rendererData->waitDestStageMasks[rendererData->waitRenderSemaphoreCount] = wait_stage_mask;
        rendererData->waitRenderSemaphores[rendererData->waitRenderSemaphoreCount] = (VkSemaphore)wait_semaphore;
        ++rendererData->waitRenderSemaphoreCount;
    }

    if (signal_semaphore) {
        if (rendererData->signalRenderSemaphoreCount == rendererData->signalRenderSemaphoreMax) {
            /* Allocate an additional one at the end for the normal present signal */
            VkSemaphore *semaphores = (VkSemaphore *)SDL_realloc(rendererData->signalRenderSemaphores, (rendererData->signalRenderSemaphoreMax + 2) * sizeof(*semaphores));
            if (!semaphores) {
                return SDL_OutOfMemory();
            }
            rendererData->signalRenderSemaphores = semaphores;
            ++rendererData->signalRenderSemaphoreMax;
        }
        rendererData->signalRenderSemaphores[rendererData->signalRenderSemaphoreCount] = (VkSemaphore)signal_semaphore;
        ++rendererData->signalRenderSemaphoreCount;
    }

    return 0;
}
#endif
static int VULKAN_RenderPresent(SDL_Renderer *renderer)
{
    VULKAN_RenderData *rendererData = (VULKAN_RenderData *)renderer->driverdata;
    VkResult result = VK_SUCCESS;
    if (rendererData->currentCommandBuffer) {
        VkPipelineStageFlags waitDestStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkSubmitInfo submitInfo;
        VkPresentInfoKHR presentInfo;

        rendererData->currentPipelineState = VK_NULL_HANDLE;
        rendererData->viewportDirty = SDL_TRUE;

        VULKAN_RecordPipelineImageBarrier(rendererData,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            rendererData->swapchainImages[rendererData->currentSwapchainImageIndex],
            &rendererData->swapchainImageLayouts[rendererData->currentSwapchainImageIndex]);

        vkEndCommandBuffer(rendererData->currentCommandBuffer);

        result = vkResetFences(rendererData->device, 1, &rendererData->fences[rendererData->currentCommandBufferIndex]);
        if (result != VK_SUCCESS) {
            return SDL_Vulkan_SetError("VULKAN_RenderPresent", "vkResetFences", result);
        }

        SDL_zero(submitInfo);
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        if (rendererData->waitRenderSemaphoreCount > 0) {
            Uint32 additionalSemaphoreCount = (rendererData->currentImageAvailableSemaphore != VK_NULL_HANDLE) ? 1 : 0;
            submitInfo.waitSemaphoreCount = rendererData->waitRenderSemaphoreCount + additionalSemaphoreCount;
            if (additionalSemaphoreCount > 0) {
                rendererData->waitRenderSemaphores[rendererData->waitRenderSemaphoreCount] = rendererData->currentImageAvailableSemaphore;
                rendererData->waitDestStageMasks[rendererData->waitRenderSemaphoreCount] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            }
            submitInfo.pWaitSemaphores = rendererData->waitRenderSemaphores;
            submitInfo.pWaitDstStageMask = rendererData->waitDestStageMasks;
            rendererData->waitRenderSemaphoreCount = 0;
        } else if (rendererData->currentImageAvailableSemaphore != VK_NULL_HANDLE) {
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &rendererData->currentImageAvailableSemaphore;
            submitInfo.pWaitDstStageMask = &waitDestStageMask;
        }
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &rendererData->currentCommandBuffer;
        if (rendererData->signalRenderSemaphoreCount > 0) {
            submitInfo.signalSemaphoreCount = rendererData->signalRenderSemaphoreCount + 1;
            rendererData->signalRenderSemaphores[rendererData->signalRenderSemaphoreCount] = rendererData->renderingFinishedSemaphores[rendererData->currentCommandBufferIndex];
            submitInfo.pSignalSemaphores = rendererData->signalRenderSemaphores;
            rendererData->signalRenderSemaphoreCount = 0;
        } else {
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &rendererData->renderingFinishedSemaphores[rendererData->currentCommandBufferIndex];
        }
        result = vkQueueSubmit(rendererData->graphicsQueue, 1, &submitInfo, rendererData->fences[rendererData->currentCommandBufferIndex]);
        if (result != VK_SUCCESS) {
            return SDL_Vulkan_SetError("VULKAN_RenderPresent", "vkQueueSubmit", result);
        }
        rendererData->currentCommandBuffer = VK_NULL_HANDLE;
        rendererData->currentImageAvailableSemaphore = VK_NULL_HANDLE;

        SDL_zero(presentInfo);
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &rendererData->renderingFinishedSemaphores[rendererData->currentCommandBufferIndex];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &rendererData->swapchain;
        presentInfo.pImageIndices = &rendererData->currentSwapchainImageIndex;
        result = vkQueuePresentKHR(rendererData->presentQueue, &presentInfo);
        if ((result != VK_SUCCESS) && (result != VK_ERROR_OUT_OF_DATE_KHR) && (result != VK_ERROR_SURFACE_LOST_KHR) && (result != VK_SUBOPTIMAL_KHR )) {
            return SDL_Vulkan_SetError("VULKAN_RenderPresent", "vkQueuePresentKHR", result);
        }

        rendererData->currentCommandBufferIndex = ( rendererData->currentCommandBufferIndex + 1 ) % rendererData->swapchainImageCount;

        /* Wait for previous time this command buffer was submitted, will be N frames ago */
        result = vkWaitForFences(rendererData->device, 1, &rendererData->fences[rendererData->currentCommandBufferIndex], VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS) {
            return SDL_Vulkan_SetError("VULKAN_RenderPresent", "vkWaitForFences", result);
        }

        VULKAN_AcquireNextSwapchainImage(renderer);
    }

    return (result == VK_SUCCESS);
}

static int VULKAN_SetVSync(SDL_Renderer *renderer, const int vsync)
{
    VULKAN_RenderData *rendererData = (VULKAN_RenderData *)renderer->driverdata;

    Uint32 prevFlags = renderer->info.flags;
    if (vsync) {
        renderer->info.flags |= SDL_RENDERER_PRESENTVSYNC;
    } else {
        renderer->info.flags &= ~SDL_RENDERER_PRESENTVSYNC;
    }
    if (prevFlags != renderer->info.flags) {
        rendererData->recreateSwapchain = SDL_TRUE;
    }
    return 0;
}

SDL_Renderer *VULKAN_CreateRenderer(SDL_Window *window, Uint32 flags)
{
    SDL_Renderer *renderer;
    VULKAN_RenderData *rendererData;
#if 0
    SDL_Colorspace output_colorspace = (SDL_Colorspace)SDL_GetNumberProperty(props, SDL_PROP_RENDERER_CREATE_OUTPUT_COLORSPACE_NUMBER, SDL_COLORSPACE_SRGB);

    if (output_colorspace != SDL_COLORSPACE_SRGB &&
        output_colorspace != SDL_COLORSPACE_SRGB_LINEAR &&
        output_colorspace != SDL_COLORSPACE_HDR10) {
        SDL_SetError("Unsupported output colorspace");
        return NULL;
    }
#endif

    renderer = (SDL_Renderer *)SDL_calloc(1, sizeof(*renderer));
    rendererData = (VULKAN_RenderData *)SDL_calloc(1, sizeof(*rendererData));
    if (!renderer || !rendererData) {
        SDL_free(renderer);
        SDL_free(rendererData);
        return NULL;
    }

#if 0
    renderer->output_colorspace = output_colorspace;
#endif
    MatrixIdentity(&rendererData->identity);
    rendererData->identitySwizzle.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    rendererData->identitySwizzle.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    rendererData->identitySwizzle.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    rendererData->identitySwizzle.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    renderer->WindowEvent = VULKAN_WindowEvent;
    renderer->GetOutputSize = VULKAN_GetOutputSize;
    renderer->SupportsBlendMode = VULKAN_SupportsBlendMode;
    renderer->CreateTexture = VULKAN_CreateTexture;
    renderer->UpdateTexture = VULKAN_UpdateTexture;
#if SDL_HAVE_YUV
    renderer->UpdateTextureYUV = VULKAN_UpdateTextureYUV;
    renderer->UpdateTextureNV = VULKAN_UpdateTextureNV;
#endif
    renderer->LockTexture = VULKAN_LockTexture;
    renderer->UnlockTexture = VULKAN_UnlockTexture;
    renderer->SetTextureScaleMode = VULKAN_SetTextureScaleMode;
    renderer->SetRenderTarget = VULKAN_SetRenderTarget;
    renderer->QueueSetViewport = VULKAN_QueueNoOp;
    renderer->QueueSetDrawColor = VULKAN_QueueNoOp;
    renderer->QueueDrawPoints = VULKAN_QueueDrawPoints;
    renderer->QueueDrawLines = VULKAN_QueueDrawPoints; /* lines and points queue vertices the same way. */
    renderer->QueueGeometry = VULKAN_QueueGeometry;
    // renderer->InvalidateCachedState = VULKAN_InvalidateCachedState;
    renderer->RunCommandQueue = VULKAN_RunCommandQueue;
    renderer->RenderReadPixels = VULKAN_RenderReadPixels;
    // renderer->AddVulkanRenderSemaphores = VULKAN_AddVulkanRenderSemaphores;
    renderer->RenderPresent = VULKAN_RenderPresent;
    renderer->DestroyTexture = VULKAN_DestroyTexture;
    renderer->DestroyRenderer = VULKAN_DestroyRenderer;
    renderer->SetVSync = VULKAN_SetVSync;
    renderer->info = VULKAN_RenderDriver.info;
    renderer->driverdata = rendererData;
    VULKAN_InvalidateCachedState(rendererData);

    if (flags & SDL_RENDERER_PRESENTVSYNC) {
        SDL_assert(renderer->info.flags & SDL_RENDERER_PRESENTVSYNC);
        // renderer->info.flags |= SDL_RENDERER_PRESENTVSYNC;
    } else {
        SDL_assert(renderer->info.flags == (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC));
        renderer->info.flags = SDL_RENDERER_ACCELERATED;
    }

    /* HACK: make sure the SDL_Renderer references the SDL_Window data now, in
     * order to give init functions access to the underlying window handle:
     */
    renderer->window = window;

    /* Initialize Vulkan resources */
    if (VULKAN_CreateDeviceResources(renderer /*, create_props*/) != VK_SUCCESS
     || VULKAN_CreateWindowSizeDependentResources(renderer) != VK_SUCCESS) {
        VULKAN_DestroyRenderer(renderer);
        return NULL;
    }

#if SDL_HAVE_YUV
    if (rendererData->supportsKHRSamplerYCbCrConversion) {
        renderer->info.texture_formats[renderer->info.num_texture_formats++] = SDL_PIXELFORMAT_YV12;
        renderer->info.texture_formats[renderer->info.num_texture_formats++] = SDL_PIXELFORMAT_IYUV;
        renderer->info.texture_formats[renderer->info.num_texture_formats++] = SDL_PIXELFORMAT_NV12;
        renderer->info.texture_formats[renderer->info.num_texture_formats++] = SDL_PIXELFORMAT_NV21;
        renderer->info.texture_formats[renderer->info.num_texture_formats++] = SDL_PIXELFORMAT_P010;
    }
#endif

    return renderer;
}

const SDL_RenderDriver VULKAN_RenderDriver = {
    VULKAN_CreateRenderer,
    {
        "vulkan",
        (SDL_RENDERER_ACCELERATED |
         SDL_RENDERER_PRESENTVSYNC), /* flags.  see SDL_RendererFlags */
        4,                           /* num_texture_formats */
        {                            /* texture_formats */
          SDL_PIXELFORMAT_ARGB8888,
          SDL_PIXELFORMAT_XRGB8888,
          SDL_PIXELFORMAT_XBGR2101010,
          SDL_PIXELFORMAT_RGBA64_FLOAT },
        16384, /* max_texture_width */
        16384  /* max_texture_height */
    }
};

#endif /* SDL_VIDEO_RENDER_VULKAN */
