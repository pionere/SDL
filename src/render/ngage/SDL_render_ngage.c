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

#if SDL_VIDEO_RENDER_NGAGE

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef Int2Fix
#define Int2Fix(i) ((i) << 16)
#endif

#ifndef Fix2Int
#define Fix2Int(i) ((((unsigned int)(i) > 0xFFFF0000) ? 0 : ((i) >> 16)))
#endif

#ifndef Fix2Real
#define Fix2Real(i) ((i) / 65536.0)
#endif

#ifndef Real2Fix
#define Real2Fix(i) ((int)((i) * 65536.0))
#endif

#include "../SDL_sysrender.h"
#include "SDL_render_ngage_c.h"
#include "../../video/SDL_sysvideo_c.h" /* For SDL_PrivateGetWindowSizeInPixels*/

static void NGAGE_WindowEvent(SDL_Renderer *renderer, const SDL_WindowEvent *event);
static void NGAGE_GetOutputSize(SDL_Renderer *renderer, int *w, int *h);
static SDL_bool NGAGE_SupportsBlendMode(SDL_Renderer *renderer, SDL_BlendMode blendMode);
static int NGAGE_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture);
static int NGAGE_QueueSetViewport(SDL_Renderer *renderer, SDL_RenderCommand *cmd);
static int NGAGE_QueueSetDrawColor(SDL_Renderer *renderer, SDL_RenderCommand *cmd);
static int NGAGE_QueueDrawVertices(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points, int count);
static int NGAGE_QueueFillRects(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FRect *rects, int count);
static int NGAGE_QueueCopy(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_FRect *dstrect);
static int NGAGE_QueueCopyEx(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_Rect *srcquad, const SDL_FRect *dstrect, const double angle, const SDL_FPoint *center, const SDL_RendererFlip flip, float scale_x, float scale_y);
//static SDL_bool NGAGE_QueueGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const float *xy, int xy_stride, const SDL_FColor *color, int color_stride, const float *uv, int uv_stride, int num_vertices, const void *indices, int num_indices, int size_indices, float scale_x, float scale_y);
static int NGAGE_QueueGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const float *xy, int xy_stride, const SDL_Color *color, int color_stride, const float *uv, int uv_stride, int num_vertices, const int *indices, float scale_x, float scale_y);

//static void NGAGE_InvalidateCachedState(SDL_Renderer *renderer);
static int NGAGE_RunCommandQueue(SDL_Renderer *renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize);
static int NGAGE_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect, const void *pixels, int pitch);

static int NGAGE_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect, void **pixels, int *pitch);
static void NGAGE_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture);
static void NGAGE_SetTextureScaleMode(SDL_Renderer *renderer, SDL_Texture *texture, SDL_ScaleMode scaleMode);
static int NGAGE_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture);
static int NGAGE_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect, Uint32 format, void *pixels, int pitch);
static int NGAGE_RenderPresent(SDL_Renderer *renderer);
static void NGAGE_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture);

static void NGAGE_DestroyRenderer(SDL_Renderer *renderer);

static int NGAGE_SetVSync(SDL_Renderer *renderer, int vsync);

static SDL_Renderer *NGAGE_CreateRenderer(SDL_Window *window, Uint32 flags)
{
    SDL_Renderer *renderer;
    NGAGE_RendererData *rendererData;
#if 0
    SDL_SetupRendererColorspace(renderer, create_props);

    if (renderer->output_colorspace != SDL_COLORSPACE_RGB_DEFAULT) {
        SDL_SetError("Unsupported output colorspace");
        return NULL;
    }
#endif
    renderer = (SDL_Renderer *)SDL_calloc(1, sizeof(*renderer));
    rendererData = (NGAGE_RendererData *)SDL_calloc(1, sizeof(*rendererData));
    if (!renderer || !rendererData) {
        SDL_free(renderer);
        SDL_free(rendererData);
        return NULL;
    }

    renderer->WindowEvent = NGAGE_WindowEvent;
    renderer->GetOutputSize = NGAGE_GetOutputSize;
    renderer->SupportsBlendMode = NGAGE_SupportsBlendMode;
    renderer->CreateTexture = NGAGE_CreateTexture;
    renderer->UpdateTexture = NGAGE_UpdateTexture;
#if SDL_HAVE_YUV
    // renderer->UpdateTextureYUV = VULKAN_UpdateTextureYUV;
    // renderer->UpdateTextureNV = VULKAN_UpdateTextureNV;
#endif
    renderer->LockTexture = NGAGE_LockTexture;
    renderer->UnlockTexture = NGAGE_UnlockTexture;
    renderer->SetTextureScaleMode = NGAGE_SetTextureScaleMode;
    renderer->SetRenderTarget = NGAGE_SetRenderTarget;
    renderer->QueueSetViewport = NGAGE_QueueSetViewport;
    renderer->QueueSetDrawColor = NGAGE_QueueSetDrawColor;
    renderer->QueueDrawPoints = NGAGE_QueueDrawVertices;
    renderer->QueueDrawLines = NGAGE_QueueDrawVertices;
    renderer->QueueFillRects = NGAGE_QueueFillRects;
    renderer->QueueCopy = NGAGE_QueueCopy;
    renderer->QueueCopyEx = NGAGE_QueueCopyEx;
    renderer->QueueGeometry = NGAGE_QueueGeometry;
    // renderer->InvalidateCachedState = NGAGE_InvalidateCachedState;
    renderer->RunCommandQueue = NGAGE_RunCommandQueue;
    renderer->RenderReadPixels = NGAGE_RenderReadPixels;
    renderer->RenderPresent = NGAGE_RenderPresent;
    renderer->DestroyTexture = NGAGE_DestroyTexture;
    renderer->DestroyRenderer = NGAGE_DestroyRenderer;
    renderer->SetVSync = NGAGE_SetVSync;
    renderer->info = NGAGE_RenderDriver.info;
    renderer->driverdata = rendererData;

    if (flags & SDL_RENDERER_PRESENTVSYNC) {
        SDL_assert(renderer->info.flags & SDL_RENDERER_PRESENTVSYNC);
        // renderer->info.flags |= SDL_RENDERER_PRESENTVSYNC;
    } else {
        SDL_assert(renderer->info.flags == (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC));
        renderer->info.flags = SDL_RENDERER_ACCELERATED;
    }

    renderer->window = window;

    // SDL_SetHintWithPriority(SDL_HINT_RENDER_LINE_METHOD, "2", SDL_HINT_OVERRIDE);

    return renderer;
}

const SDL_RenderDriver NGAGE_RenderDriver = {
    NGAGE_CreateRenderer,
    {
        "N-Gage",
        (SDL_RENDERER_ACCELERATED |
         SDL_RENDERER_PRESENTVSYNC), /* flags.  see SDL_RendererFlags */
        1,                           /* num_texture_formats */
        {                            /* texture_formats */
          SDL_PIXELFORMAT_ARGB4444 },
        256, /* max_texture_width */
        256  /* max_texture_height */
    }
};

static void NGAGE_WindowEvent(SDL_Renderer *renderer, const SDL_WindowEvent *event)
{
    return;
}

static void NGAGE_GetOutputSize(SDL_Renderer *renderer, int *w, int *h)
{
    SDL_PrivateGetWindowSizeInPixels(renderer->window, w, h);
}

static SDL_bool NGAGE_SupportsBlendMode(SDL_Renderer *renderer, SDL_BlendMode blendMode)
{
    switch (blendMode) {
    case SDL_BLENDMODE_NONE:
    case SDL_BLENDMODE_MOD:
        return SDL_TRUE;
    default:
        return SDL_FALSE;
    }
}

static int NGAGE_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    NGAGE_TextureData *data = (NGAGE_TextureData *)SDL_calloc(1, sizeof(*data));
    SDL_Surface *surface;
    if (!data) {
        return SDL_OutOfMemory();
    }

    if (!NGAGE_CreateTextureData(data, texture->w, texture->h)) {
        SDL_free(data);
        return -1;
    }

    surface = SDL_CreateRGBSurfaceWithFormat(0, texture->w, texture->h, 0, texture->format);
    if (!surface) {
        SDL_free(data);
        return -1;
    }

    data->surface = surface;
    texture->driverdata = data;

    return 0;
}

static int NGAGE_QueueSetViewport(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
#if 0
    if (!cmd->data.viewport.rect.w && !cmd->data.viewport.rect.h) {
        SDL_Rect viewport = { 0, 0, NGAGE_SCREEN_WIDTH, NGAGE_SCREEN_HEIGHT };
        SDL_RenderSetViewport(renderer, &viewport);
    }
#endif
    return 0;
}

static int NGAGE_QueueSetDrawColor(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    return 0;
}

static int NGAGE_QueueDrawVertices(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points, int count)
{
    const Uint32 color = NGAGE_ConvertColor(cmd->data.draw.color.r, cmd->data.draw.color.g, cmd->data.draw.color.b, cmd->data.draw.color.a/*, cmd->data.draw.color_scale*/);
    NGAGE_Vertex *verts = (NGAGE_Vertex *)SDL_AllocateRenderVertices(renderer, count * sizeof(NGAGE_Vertex), 0, &cmd->data.draw.first);
    int i;
    if (!verts) {
        return -1;
    }

    cmd->data.draw.count = count;

    for (i = 0; i < count; i++, points++) {
        int fixed_x = Real2Fix(points->x);
        int fixed_y = Real2Fix(points->y);

        verts[i].x = Fix2Int(fixed_x);
        verts[i].y = Fix2Int(fixed_y);

        verts[i].color.a = (Uint8)(color >> 24);
        verts[i].color.b = (Uint8)(color >> 16);
        verts[i].color.g = (Uint8)(color >> 8);
        verts[i].color.r = (Uint8)color;
    }

    return 0;
}

static int NGAGE_QueueFillRects(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FRect *rects, int count)
{
    const Uint32 color = NGAGE_ConvertColor(cmd->data.draw.color.r, cmd->data.draw.color.g, cmd->data.draw.color.b, cmd->data.draw.color.a/*, cmd->data.draw.color_scale*/);
    NGAGE_Vertex *verts = (NGAGE_Vertex *)SDL_AllocateRenderVertices(renderer, count * 2 * sizeof(NGAGE_Vertex), 0, &cmd->data.draw.first);
    int i;
    if (!verts) {
        return -1;
    }

    cmd->data.draw.count = count;

    for (i = 0; i < count; i++, rects++) {
        verts[i * 2].x = Real2Fix(rects->x);
        verts[i * 2].y = Real2Fix(rects->y);
        verts[i * 2 + 1].x = Real2Fix(rects->w);
        verts[i * 2 + 1].y = Real2Fix(rects->h);

        verts[i * 2].x = Fix2Int(verts[i * 2].x);
        verts[i * 2].y = Fix2Int(verts[i * 2].y);
        verts[i * 2 + 1].x = Fix2Int(verts[i * 2 + 1].x);
        verts[i * 2 + 1].y = Fix2Int(verts[i * 2 + 1].y);

        verts[i * 2].color.a = (Uint8)(color >> 24);
        verts[i * 2].color.b = (Uint8)(color >> 16);
        verts[i * 2].color.g = (Uint8)(color >> 8);
        verts[i * 2].color.r = (Uint8)color;
    }

    return 0;
}

static int NGAGE_QueueCopy(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_FRect *dstrect)
{
    SDL_Rect *verts = (SDL_Rect *)SDL_AllocateRenderVertices(renderer, 2 * sizeof(SDL_Rect), 0, &cmd->data.draw.first);

    if (!verts) {
        return -1;
    }

    cmd->data.draw.count = 1;

    verts->x = (int)srcrect->x;
    verts->y = (int)srcrect->y;
    verts->w = (int)srcrect->w;
    verts->h = (int)srcrect->h;

    verts++;

    verts->x = (int)dstrect->x;
    verts->y = (int)dstrect->y;
    verts->w = (int)dstrect->w;
    verts->h = (int)dstrect->h;

    return 0;
}

static int NGAGE_QueueCopyEx(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_Rect *srcquad, const SDL_FRect *dstrect, const double angle, const SDL_FPoint *center, const SDL_RendererFlip flip, float scale_x, float scale_y)
{
    NGAGE_CopyExData *verts = (NGAGE_CopyExData *)SDL_AllocateRenderVertices(renderer, sizeof(NGAGE_CopyExData), 0, &cmd->data.draw.first);

    if (!verts) {
        return -1;
    }

    cmd->data.draw.count = 1;

    verts->srcrect.x = (int)srcquad->x;
    verts->srcrect.y = (int)srcquad->y;
    verts->srcrect.w = (int)srcquad->w;
    verts->srcrect.h = (int)srcquad->h;
    verts->dstrect.x = (int)dstrect->x;
    verts->dstrect.y = (int)dstrect->y;
    verts->dstrect.w = (int)dstrect->w;
    verts->dstrect.h = (int)dstrect->h;

    verts->angle = Real2Fix(angle);
    verts->center.x = Real2Fix(center->x);
    verts->center.y = Real2Fix(center->y);
    verts->scale_x = Real2Fix(scale_x);
    verts->scale_y = Real2Fix(scale_y);

    verts->flip = flip;

    return 0;
}

//static int NGAGE_QueueGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const float *xy, int xy_stride, const SDL_FColor *color, int color_stride, const float *uv, int uv_stride, int num_vertices, const void *indices, int num_indices, int size_indices, float scale_x, float scale_y)
static int NGAGE_QueueGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const float *xy, int xy_stride, const SDL_Color *color, int color_stride, const float *uv, int uv_stride, int num_vertices, const int *indices, float scale_x, float scale_y)
{
    return 0;
}

/*static void NGAGE_InvalidateCachedState(SDL_Renderer *renderer)
{
    return;
}*/

static int NGAGE_RunCommandQueue(SDL_Renderer *renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize)
{
    NGAGE_RendererData *phdata = (NGAGE_RendererData *)renderer->driverdata;
    if (!phdata) {
        return -1;
    }
    phdata->viewport = 0;

    while (cmd) {
        switch (cmd->command) {
        case SDL_RENDERCMD_NO_OP:
            break;
        case SDL_RENDERCMD_SETVIEWPORT:
            phdata->viewport = &cmd->data.viewport.rect;
            break;

        case SDL_RENDERCMD_SETCLIPRECT:
        {
            const SDL_Rect *rect = &cmd->data.cliprect.rect;

            if (cmd->data.cliprect.enabled) {
                NGAGE_SetClipRect(rect);
            }

            break;
        }

        case SDL_RENDERCMD_SETDRAWCOLOR:
        {
            break;
        }

        case SDL_RENDERCMD_CLEAR:
        {
            Uint32 color = NGAGE_ConvertColor(cmd->data.color.color.r, cmd->data.color.color.g, cmd->data.color.color.b, cmd->data.color.color.a/*, cmd->data.color.color_scale*/);

            NGAGE_Clear(color);
            break;
        }

        case SDL_RENDERCMD_DRAW_POINTS:
        {
            NGAGE_Vertex *verts = (NGAGE_Vertex *)(((Uint8 *)vertices) + cmd->data.draw.first);
            const int count = cmd->data.draw.count;

            // Apply viewport.
            if (phdata->viewport && (phdata->viewport->x || phdata->viewport->y)) {
                int i;
                for (i = 0; i < count; i++) {
                    verts[i].x += phdata->viewport->x;
                    verts[i].y += phdata->viewport->y;
                }
            }

            NGAGE_DrawPoints(verts, count);
            break;
        }
        case SDL_RENDERCMD_DRAW_LINES:
        {
            NGAGE_Vertex *verts = (NGAGE_Vertex *)(((Uint8 *)vertices) + cmd->data.draw.first);
            const int count = cmd->data.draw.count;

            // Apply viewport.
            if (phdata->viewport && (phdata->viewport->x || phdata->viewport->y)) {
                int i;
                for (i = 0; i < count; i++) {
                    verts[i].x += phdata->viewport->x;
                    verts[i].y += phdata->viewport->y;
                }
            }

            NGAGE_DrawLines(verts, count);
            break;
        }

        case SDL_RENDERCMD_FILL_RECTS:
        {
            NGAGE_Vertex *verts = (NGAGE_Vertex *)(((Uint8 *)vertices) + cmd->data.draw.first);
            const int count = cmd->data.draw.count;

            // Apply viewport.
            if (phdata->viewport && (phdata->viewport->x || phdata->viewport->y)) {
                int i;
                for (i = 0; i < count; i++) {
                    verts[i].x += phdata->viewport->x;
                    verts[i].y += phdata->viewport->y;
                }
            }

            NGAGE_FillRects(verts, count);
            break;
        }

        case SDL_RENDERCMD_COPY:
        {
            SDL_Rect *verts = (SDL_Rect *)(((Uint8 *)vertices) + cmd->data.draw.first);
            SDL_Rect *srcrect = verts;
            SDL_Rect *dstrect = verts + 1;
            SDL_Texture *texture = cmd->data.draw.texture;

            // Apply viewport.
            if (phdata->viewport && (phdata->viewport->x || phdata->viewport->y)) {
                dstrect->x += phdata->viewport->x;
                dstrect->y += phdata->viewport->y;
            }

            NGAGE_Copy(renderer, texture, srcrect, dstrect);
            break;
        }

        case SDL_RENDERCMD_COPY_EX:
        {
            NGAGE_CopyExData *copydata = (NGAGE_CopyExData *)(((Uint8 *)vertices) + cmd->data.draw.first);
            SDL_Texture *texture = cmd->data.draw.texture;

            // Apply viewport.
            if (phdata->viewport && (phdata->viewport->x || phdata->viewport->y)) {
                copydata->dstrect.x += phdata->viewport->x;
                copydata->dstrect.y += phdata->viewport->y;
            }

            NGAGE_CopyEx(renderer, texture, copydata);
            break;
        }

        case SDL_RENDERCMD_GEOMETRY:
        {
            break;
        }
        }
        cmd = cmd->next;
    }

    return 0;
}

static int NGAGE_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect, const void *pixels, int pitch)
{
    NGAGE_TextureData *phdata = (NGAGE_TextureData *)texture->driverdata;

    SDL_Surface *surface = phdata->surface;
    Uint8 *src, *dst;
    int row;
    size_t length;

    if (SDL_MUSTLOCK(surface)) {
        if (!SDL_LockSurface(surface)) {
            return -1;
        }
    }
    src = (Uint8 *)pixels;
    dst = (Uint8 *)surface->pixels +
          rect->y * surface->pitch +
          rect->x * surface->format->BytesPerPixel;

    length = (size_t)rect->w * surface->format->BytesPerPixel;
    for (row = 0; row < rect->h; ++row) {
        SDL_memcpy(dst, src, length);
        src += pitch;
        dst += surface->pitch;
    }
    if (SDL_MUSTLOCK(surface)) {
        SDL_UnlockSurface(surface);
    }

    return 0;
}

static int NGAGE_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect, void **pixels, int *pitch)
{
    NGAGE_TextureData *phdata = (NGAGE_TextureData *)texture->driverdata;
    SDL_Surface *surface = phdata->surface;

    *pixels =
        (void *)((Uint8 *)surface->pixels + rect->y * surface->pitch +
                 rect->x * surface->format->BytesPerPixel);
    *pitch = surface->pitch;
    return 0;
}

static void NGAGE_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
}

static void NGAGE_SetTextureScaleMode(SDL_Renderer *renderer, SDL_Texture *texture, SDL_ScaleMode scaleMode)
{
}

static int NGAGE_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture)
{
    return 0;
}

static int NGAGE_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect, Uint32 format, void *pixels, int pitch)
{
    return -1;
}

static int NGAGE_RenderPresent(SDL_Renderer *renderer)
{
    NGAGE_Flip();

    return 0;
}

static void NGAGE_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    NGAGE_TextureData *data = (NGAGE_TextureData *)texture->driverdata;
    if (data) {
        SDL_FreeSurface(data->surface);
        NGAGE_DestroyTextureData(data);
        SDL_free(data);
        texture->driverdata = NULL;
    }
}

static void NGAGE_DestroyRenderer(SDL_Renderer *renderer)
{
    NGAGE_RendererData *rendererData = (NGAGE_RendererData *)renderer->driverdata;
    SDL_assert(rendererData != NULL);
    SDL_free(rendererData);
    SDL_free(renderer);
}

static int NGAGE_SetVSync(SDL_Renderer *renderer, int vsync)
{
    /* NGAGE_RendererData *rendererData = (NGAGE_RendererData *)renderer->driverdata;

    Uint32 prevFlags = renderer->info.flags;
    if (vsync) {
        renderer->info.flags |= SDL_RENDERER_PRESENTVSYNC;
    } else {
        renderer->info.flags &= ~SDL_RENDERER_PRESENTVSYNC;
    }
    if (prevFlags != renderer->info.flags) {
        rendererData->recreateSwapchain = SDL_TRUE;
    }*/
    return 0;
}

#endif // SDL_VIDEO_RENDER_NGAGE
