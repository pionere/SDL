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
#include "../SDL_internal.h"

#ifndef SDL_sysrender_c_h_
#define SDL_sysrender_c_h_

#include "SDL_sysrender.h"

#define RENDERDRIVER_INSTANCE(x) extern const SDL_RenderDriver x;

#if SDL_VIDEO_RENDER_D3D
#define D3D_RENDERDRIVER       RENDERDRIVER_INSTANCE(D3D_RenderDriver)
#define D3D_RENDERDRIVER_ENUM  SDL_RENDERDRIVER_D3D,
#define D3D_RENDERDRIVER_ENTRY &D3D_RenderDriver,
#else
#define D3D_RENDERDRIVER
#define D3D_RENDERDRIVER_ENUM
#define D3D_RENDERDRIVER_ENTRY
#endif
#if SDL_VIDEO_RENDER_D3D11
#define D3D11_RENDERDRIVER       RENDERDRIVER_INSTANCE(D3D11_RenderDriver)
#define D3D11_RENDERDRIVER_ENUM  SDL_RENDERDRIVER_D3D11,
#define D3D11_RENDERDRIVER_ENTRY &D3D11_RenderDriver,
#else
#define D3D11_RENDERDRIVER
#define D3D11_RENDERDRIVER_ENUM
#define D3D11_RENDERDRIVER_ENTRY
#endif
#if SDL_VIDEO_RENDER_D3D12
#define D3D12_RENDERDRIVER       RENDERDRIVER_INSTANCE(D3D12_RenderDriver)
#define D3D12_RENDERDRIVER_ENUM  SDL_RENDERDRIVER_D3D12,
#define D3D12_RENDERDRIVER_ENTRY &D3D12_RenderDriver,
#else
#define D3D12_RENDERDRIVER
#define D3D12_RENDERDRIVER_ENUM
#define D3D12_RENDERDRIVER_ENTRY
#endif
#if SDL_VIDEO_RENDER_METAL
#define METAL_RENDERDRIVER       RENDERDRIVER_INSTANCE(METAL_RenderDriver)
#define METAL_RENDERDRIVER_ENUM  SDL_RENDERDRIVER_METAL,
#define METAL_RENDERDRIVER_ENTRY &METAL_RenderDriver,
#else
#define METAL_RENDERDRIVER
#define METAL_RENDERDRIVER_ENUM
#define METAL_RENDERDRIVER_ENTRY
#endif
#if SDL_VIDEO_RENDER_OGL
#define GL_RENDERDRIVER       RENDERDRIVER_INSTANCE(GL_RenderDriver)
#define GL_RENDERDRIVER_ENUM  SDL_RENDERDRIVER_GL,
#define GL_RENDERDRIVER_ENTRY &GL_RenderDriver,
#else
#define GL_RENDERDRIVER
#define GL_RENDERDRIVER_ENUM
#define GL_RENDERDRIVER_ENTRY
#endif
#if SDL_VIDEO_RENDER_OGL_ES2
#define GLES2_RENDERDRIVER       RENDERDRIVER_INSTANCE(GLES2_RenderDriver)
#define GLES2_RENDERDRIVER_ENUM  SDL_RENDERDRIVER_GLES2,
#define GLES2_RENDERDRIVER_ENTRY &GLES2_RenderDriver,
#else
#define GLES2_RENDERDRIVER
#define GLES2_RENDERDRIVER_ENUM
#define GLES2_RENDERDRIVER_ENTRY
#endif
#if SDL_VIDEO_RENDER_OGL_ES
#define GLES_RENDERDRIVER       RENDERDRIVER_INSTANCE(GLES_RenderDriver)
#define GLES_RENDERDRIVER_ENUM  SDL_RENDERDRIVER_GLES,
#define GLES_RENDERDRIVER_ENTRY &GLES_RenderDriver,
#else
#define GLES_RENDERDRIVER
#define GLES_RENDERDRIVER_ENUM
#define GLES_RENDERDRIVER_ENTRY
#endif
#if SDL_VIDEO_RENDER_DIRECTFB
#define DirectFB_RENDERDRIVER       RENDERDRIVER_INSTANCE(DirectFB_RenderDriver)
#define DirectFB_RENDERDRIVER_ENUM  SDL_RENDERDRIVER_DirectFB,
#define DirectFB_RENDERDRIVER_ENTRY &DirectFB_RenderDriver,
#else
#define DirectFB_RENDERDRIVER
#define DirectFB_RENDERDRIVER_ENUM
#define DirectFB_RENDERDRIVER_ENTRY
#endif
#if SDL_VIDEO_RENDER_PS2
#define PS2_RENDERDRIVER       RENDERDRIVER_INSTANCE(PS2_RenderDriver)
#define PS2_RENDERDRIVER_ENUM  SDL_RENDERDRIVER_PS2,
#define PS2_RENDERDRIVER_ENTRY &PS2_RenderDriver,
#else
#define PS2_RENDERDRIVER
#define PS2_RENDERDRIVER_ENUM
#define PS2_RENDERDRIVER_ENTRY
#endif
#if SDL_VIDEO_RENDER_PSP
#define PSP_RENDERDRIVER       RENDERDRIVER_INSTANCE(PSP_RenderDriver)
#define PSP_RENDERDRIVER_ENUM  SDL_RENDERDRIVER_PSP,
#define PSP_RENDERDRIVER_ENTRY &PSP_RenderDriver,
#else
#define PSP_RENDERDRIVER
#define PSP_RENDERDRIVER_ENUM
#define PSP_RENDERDRIVER_ENTRY
#endif
#if SDL_VIDEO_RENDER_VITA_GXM
#define VITA_GXM_RENDERDRIVER       RENDERDRIVER_INSTANCE(VITA_GXM_RenderDriver)
#define VITA_GXM_RENDERDRIVER_ENUM  SDL_RENDERDRIVER_VITA_GXM,
#define VITA_GXM_RENDERDRIVER_ENTRY &VITA_GXM_RenderDriver,
#else
#define VITA_GXM_RENDERDRIVER
#define VITA_GXM_RENDERDRIVER_ENUM
#define VITA_GXM_RENDERDRIVER_ENTRY
#endif
#if SDL_VIDEO_RENDER_VULKAN
#define VULKAN_RENDERDRIVER       RENDERDRIVER_INSTANCE(VULKAN_RenderDriver)
#define VULKAN_RENDERDRIVER_ENUM  SDL_RENDERDRIVER_VULKAN,
#define VULKAN_RENDERDRIVER_ENTRY &VULKAN_RenderDriver,
#else
#define VULKAN_RENDERDRIVER
#define VULKAN_RENDERDRIVER_ENUM
#define VULKAN_RENDERDRIVER_ENTRY
#endif
#if SDL_VIDEO_RENDER_SW
#define SW_RENDERDRIVER       RENDERDRIVER_INSTANCE(SW_RenderDriver)
#define SW_RENDERDRIVER_ENUM  SDL_RENDERDRIVER_SW,
#define SW_RENDERDRIVER_ENTRY &SW_RenderDriver,
#else
#define SW_RENDERDRIVER
#define SW_RENDERDRIVER_ENUM
#define SW_RENDERDRIVER_ENTRY
#endif

/* Video drivers enum */
typedef enum
{
    D3D_RENDERDRIVER_ENUM
    D3D11_RENDERDRIVER_ENUM
    D3D12_RENDERDRIVER_ENUM
    METAL_RENDERDRIVER_ENUM
    GL_RENDERDRIVER_ENUM
    GLES2_RENDERDRIVER_ENUM
    GLES_RENDERDRIVER_ENUM
    DirectFB_RENDERDRIVER_ENUM
    PS2_RENDERDRIVER_ENUM
    PSP_RENDERDRIVER_ENUM
    VITA_GXM_RENDERDRIVER_ENUM
    VULKAN_RENDERDRIVER_ENUM
    SW_RENDERDRIVER_ENUM
    SDL_RENDERDRIVERS_count
} SDL_RENDERDRIVERS;

/* Render driver instances */
D3D_RENDERDRIVER
D3D11_RENDERDRIVER
D3D12_RENDERDRIVER
METAL_RENDERDRIVER
GL_RENDERDRIVER
GLES2_RENDERDRIVER
GLES_RENDERDRIVER
DirectFB_RENDERDRIVER
PS2_RENDERDRIVER
PSP_RENDERDRIVER
VITA_GXM_RENDERDRIVER
VULKAN_RENDERDRIVER
SW_RENDERDRIVER

#endif /* SDL_sysrender_c_h_ */

/* vi: set ts=4 sw=4 expandtab: */
