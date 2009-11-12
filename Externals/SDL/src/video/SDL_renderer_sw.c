/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2009 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

#include "SDL_video.h"
#include "SDL_sysvideo.h"
#include "SDL_pixels_c.h"
#include "SDL_rect_c.h"
#include "SDL_yuv_sw_c.h"


/* SDL surface based renderer implementation */

static SDL_Renderer *SW_CreateRenderer(SDL_Window * window, Uint32 flags);
static int SW_ActivateRenderer(SDL_Renderer * renderer);
static int SW_DisplayModeChanged(SDL_Renderer * renderer);
static int SW_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static int SW_QueryTexturePixels(SDL_Renderer * renderer,
                                 SDL_Texture * texture, void **pixels,
                                 int *pitch);
static int SW_SetTexturePalette(SDL_Renderer * renderer,
                                SDL_Texture * texture,
                                const SDL_Color * colors, int firstcolor,
                                int ncolors);
static int SW_GetTexturePalette(SDL_Renderer * renderer,
                                SDL_Texture * texture, SDL_Color * colors,
                                int firstcolor, int ncolors);
static int SW_SetTextureColorMod(SDL_Renderer * renderer,
                                 SDL_Texture * texture);
static int SW_SetTextureAlphaMod(SDL_Renderer * renderer,
                                 SDL_Texture * texture);
static int SW_SetTextureBlendMode(SDL_Renderer * renderer,
                                  SDL_Texture * texture);
static int SW_SetTextureScaleMode(SDL_Renderer * renderer,
                                  SDL_Texture * texture);
static int SW_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                            const SDL_Rect * rect, const void *pixels,
                            int pitch);
static int SW_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                          const SDL_Rect * rect, int markDirty, void **pixels,
                          int *pitch);
static void SW_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static int SW_RenderPoint(SDL_Renderer * renderer, int x, int y);
static int SW_RenderLine(SDL_Renderer * renderer, int x1, int y1, int x2,
                         int y2);
static int SW_RenderFill(SDL_Renderer * renderer, const SDL_Rect * rect);
static int SW_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                         const SDL_Rect * srcrect, const SDL_Rect * dstrect);
static int SW_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                               void * pixels, int pitch);
static int SW_RenderWritePixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                                const void * pixels, int pitch);
static void SW_RenderPresent(SDL_Renderer * renderer);
static void SW_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static void SW_DestroyRenderer(SDL_Renderer * renderer);


SDL_RenderDriver SW_RenderDriver = {
    SW_CreateRenderer,
    {
     "software",
     (SDL_RENDERER_SINGLEBUFFER | SDL_RENDERER_PRESENTCOPY |
      SDL_RENDERER_PRESENTFLIP2 | SDL_RENDERER_PRESENTFLIP3 |
      SDL_RENDERER_PRESENTDISCARD | SDL_RENDERER_PRESENTVSYNC),
     (SDL_TEXTUREMODULATE_NONE | SDL_TEXTUREMODULATE_COLOR |
      SDL_TEXTUREMODULATE_ALPHA),
     (SDL_BLENDMODE_NONE | SDL_BLENDMODE_MASK |
      SDL_BLENDMODE_BLEND | SDL_BLENDMODE_ADD | SDL_BLENDMODE_MOD),
     (SDL_TEXTURESCALEMODE_NONE | SDL_TEXTURESCALEMODE_FAST),
     14,
     {
      SDL_PIXELFORMAT_INDEX8,
      SDL_PIXELFORMAT_RGB555,
      SDL_PIXELFORMAT_RGB565,
      SDL_PIXELFORMAT_RGB888,
      SDL_PIXELFORMAT_BGR888,
      SDL_PIXELFORMAT_ARGB8888,
      SDL_PIXELFORMAT_RGBA8888,
      SDL_PIXELFORMAT_ABGR8888,
      SDL_PIXELFORMAT_BGRA8888,
      SDL_PIXELFORMAT_YV12,
      SDL_PIXELFORMAT_IYUV,
      SDL_PIXELFORMAT_YUY2,
      SDL_PIXELFORMAT_UYVY,
      SDL_PIXELFORMAT_YVYU},
     0,
     0}
};

typedef struct
{
    Uint32 format;
    SDL_bool updateSize;
    int current_texture;
    SDL_Texture *texture[3];
    SDL_Surface surface;
    SDL_Renderer *renderer;
    SDL_DirtyRectList dirty;
} SW_RenderData;

static SDL_Texture *
CreateTexture(SDL_Renderer * renderer, Uint32 format, int w, int h)
{
    SDL_Texture *texture;

    texture = (SDL_Texture *) SDL_calloc(1, sizeof(*texture));
    if (!texture) {
        SDL_OutOfMemory();
        return NULL;
    }

    texture->format = format;
    texture->access = SDL_TEXTUREACCESS_STREAMING;
    texture->w = w;
    texture->h = h;
    texture->renderer = renderer;

    if (renderer->CreateTexture(renderer, texture) < 0) {
        SDL_free(texture);
        return NULL;
    }
    return texture;
}

static void
DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    renderer->DestroyTexture(renderer, texture);
    SDL_free(texture);
}

static int
DisplayPaletteChanged(void *userdata, SDL_Palette * palette)
{
    SW_RenderData *data = (SW_RenderData *) userdata;
    int i;

    for (i = 0; i < SDL_arraysize(data->texture); ++i) {
        if (data->texture[i] && data->renderer->SetTexturePalette) {
            data->renderer->SetTexturePalette(data->renderer,
                                              data->texture[i],
                                              palette->colors, 0,
                                              palette->ncolors);
        }
    }
    return 0;
}

void
Setup_SoftwareRenderer(SDL_Renderer * renderer)
{
    renderer->CreateTexture = SW_CreateTexture;
    renderer->QueryTexturePixels = SW_QueryTexturePixels;
    renderer->SetTexturePalette = SW_SetTexturePalette;
    renderer->GetTexturePalette = SW_GetTexturePalette;
    renderer->SetTextureColorMod = SW_SetTextureColorMod;
    renderer->SetTextureAlphaMod = SW_SetTextureAlphaMod;
    renderer->SetTextureBlendMode = SW_SetTextureBlendMode;
    renderer->SetTextureScaleMode = SW_SetTextureScaleMode;
    renderer->UpdateTexture = SW_UpdateTexture;
    renderer->LockTexture = SW_LockTexture;
    renderer->UnlockTexture = SW_UnlockTexture;
    renderer->DestroyTexture = SW_DestroyTexture;

    renderer->info.mod_modes = SW_RenderDriver.info.mod_modes;
    renderer->info.blend_modes = SW_RenderDriver.info.blend_modes;
    renderer->info.scale_modes = SW_RenderDriver.info.scale_modes;
    renderer->info.num_texture_formats =
        SW_RenderDriver.info.num_texture_formats;
    SDL_memcpy(renderer->info.texture_formats,
               SW_RenderDriver.info.texture_formats,
               sizeof(renderer->info.texture_formats));;
    renderer->info.max_texture_width = SW_RenderDriver.info.max_texture_width;
    renderer->info.max_texture_height =
        SW_RenderDriver.info.max_texture_height;
}

SDL_Renderer *
SW_CreateRenderer(SDL_Window * window, Uint32 flags)
{
    SDL_VideoDisplay *display = SDL_GetDisplayFromWindow(window);
    SDL_DisplayMode *displayMode = &display->current_mode;
    SDL_Renderer *renderer;
    SW_RenderData *data;
    int i, n;
    int bpp;
    Uint32 Rmask, Gmask, Bmask, Amask;
    Uint32 renderer_flags;
    const char *desired_driver;

    if (!SDL_PixelFormatEnumToMasks
        (displayMode->format, &bpp, &Rmask, &Gmask, &Bmask, &Amask)) {
        SDL_SetError("Unknown display format");
        return NULL;
    }

    renderer = (SDL_Renderer *) SDL_calloc(1, sizeof(*renderer));
    if (!renderer) {
        SDL_OutOfMemory();
        return NULL;
    }

    data = (SW_RenderData *) SDL_calloc(1, sizeof(*data));
    if (!data) {
        SW_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return NULL;
    }
    renderer->ActivateRenderer = SW_ActivateRenderer;
    renderer->DisplayModeChanged = SW_DisplayModeChanged;

    renderer->RenderPoint = SW_RenderPoint;
    renderer->RenderLine = SW_RenderLine;
    renderer->RenderFill = SW_RenderFill;
    renderer->RenderCopy = SW_RenderCopy;
    renderer->RenderReadPixels = SW_RenderReadPixels;
    renderer->RenderWritePixels = SW_RenderWritePixels;
    renderer->RenderPresent = SW_RenderPresent;
    renderer->DestroyRenderer = SW_DestroyRenderer;
    renderer->info.name = SW_RenderDriver.info.name;
    renderer->info.flags = 0;
    renderer->window = window->id;
    renderer->driverdata = data;
    Setup_SoftwareRenderer(renderer);

    if (flags & SDL_RENDERER_PRESENTFLIP2) {
        renderer->info.flags |= SDL_RENDERER_PRESENTFLIP2;
        n = 2;
    } else if (flags & SDL_RENDERER_PRESENTFLIP3) {
        renderer->info.flags |= SDL_RENDERER_PRESENTFLIP3;
        n = 3;
    } else {
        renderer->info.flags |= SDL_RENDERER_PRESENTCOPY;
        n = 1;
    }
    data->format = displayMode->format;

    /* Find a render driver that we can use to display data */
    renderer_flags = (SDL_RENDERER_SINGLEBUFFER |
                      SDL_RENDERER_PRESENTDISCARD);
    if (flags & SDL_RENDERER_PRESENTVSYNC) {
        renderer_flags |= SDL_RENDERER_PRESENTVSYNC;
    }
    desired_driver = SDL_getenv("SDL_VIDEO_RENDERER_SWDRIVER");
    for (i = 0; i < display->num_render_drivers; ++i) {
        SDL_RenderDriver *driver = &display->render_drivers[i];
        if (driver->info.name == SW_RenderDriver.info.name) {
            continue;
        }
        if (desired_driver
            && SDL_strcasecmp(desired_driver, driver->info.name) != 0) {
            continue;
        }
        data->renderer = driver->CreateRenderer(window, renderer_flags);
        if (data->renderer) {
            break;
        }
    }
    if (i == display->num_render_drivers) {
        SW_DestroyRenderer(renderer);
        SDL_SetError("Couldn't find display render driver");
        return NULL;
    }
    if (data->renderer->info.flags & SDL_RENDERER_PRESENTVSYNC) {
        renderer->info.flags |= SDL_RENDERER_PRESENTVSYNC;
    }

    /* Create the textures we'll use for display */
    for (i = 0; i < n; ++i) {
        data->texture[i] =
            CreateTexture(data->renderer, data->format, window->w, window->h);
        if (!data->texture[i]) {
            SW_DestroyRenderer(renderer);
            return NULL;
        }
    }
    data->current_texture = 0;

    /* Create a surface we'll use for rendering */
    data->surface.flags = SDL_PREALLOC;
    data->surface.format = SDL_AllocFormat(bpp, Rmask, Gmask, Bmask, Amask);
    if (!data->surface.format) {
        SW_DestroyRenderer(renderer);
        return NULL;
    }
    SDL_SetSurfacePalette(&data->surface, display->palette);

    /* Set up a palette watch on the display palette */
    if (display->palette) {
        SDL_AddPaletteWatch(display->palette, DisplayPaletteChanged, data);
    }

    return renderer;
}

static int
SW_ActivateRenderer(SDL_Renderer * renderer)
{
    SW_RenderData *data = (SW_RenderData *) renderer->driverdata;
    SDL_Window *window = SDL_GetWindowFromID(renderer->window);
    int i, n;

    if (data->renderer && data->renderer->ActivateRenderer) {
        if (data->renderer->ActivateRenderer(data->renderer) < 0) {
            return -1;
        }
    }
    if (data->updateSize) {
        /* Recreate the textures for the new window size */
        if (renderer->info.flags & SDL_RENDERER_PRESENTFLIP2) {
            n = 2;
        } else if (renderer->info.flags & SDL_RENDERER_PRESENTFLIP3) {
            n = 3;
        } else {
            n = 1;
        }
        for (i = 0; i < n; ++i) {
            if (data->texture[i]) {
                DestroyTexture(data->renderer, data->texture[i]);
                data->texture[i] = 0;
            }
        }
        for (i = 0; i < n; ++i) {
            data->texture[i] =
                CreateTexture(data->renderer, data->format, window->w,
                              window->h);
            if (!data->texture[i]) {
                return -1;
            }
        }
        data->updateSize = SDL_FALSE;
    }
    return 0;
}

static int
SW_DisplayModeChanged(SDL_Renderer * renderer)
{
    SW_RenderData *data = (SW_RenderData *) renderer->driverdata;

    if (data->renderer && data->renderer->DisplayModeChanged) {
        if (data->renderer->DisplayModeChanged(data->renderer) < 0) {
            return -1;
        }
    }
    /* Rebind the context to the window area */
    data->updateSize = SDL_TRUE;
    return SW_ActivateRenderer(renderer);
}

static int
SW_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    if (SDL_ISPIXELFORMAT_FOURCC(texture->format)) {
        texture->driverdata =
            SDL_SW_CreateYUVTexture(texture->format, texture->w, texture->h);
    } else {
        int bpp;
        Uint32 Rmask, Gmask, Bmask, Amask;

        if (!SDL_PixelFormatEnumToMasks
            (texture->format, &bpp, &Rmask, &Gmask, &Bmask, &Amask)) {
            SDL_SetError("Unknown texture format");
            return -1;
        }

        texture->driverdata =
            SDL_CreateRGBSurface(0, texture->w, texture->h, bpp, Rmask, Gmask,
                                 Bmask, Amask);
        SDL_SetSurfaceColorMod(texture->driverdata, texture->r, texture->g,
                               texture->b);
        SDL_SetSurfaceAlphaMod(texture->driverdata, texture->a);
        SDL_SetSurfaceBlendMode(texture->driverdata, texture->blendMode);
        SDL_SetSurfaceScaleMode(texture->driverdata, texture->scaleMode);

        if (texture->access == SDL_TEXTUREACCESS_STATIC) {
            SDL_SetSurfaceRLE(texture->driverdata, 1);
        }
    }

    if (!texture->driverdata) {
        return -1;
    }
    return 0;
}

static int
SW_QueryTexturePixels(SDL_Renderer * renderer, SDL_Texture * texture,
                      void **pixels, int *pitch)
{
    if (SDL_ISPIXELFORMAT_FOURCC(texture->format)) {
        return SDL_SW_QueryYUVTexturePixels((SDL_SW_YUVTexture *)
                                            texture->driverdata, pixels,
                                            pitch);
    } else {
        SDL_Surface *surface = (SDL_Surface *) texture->driverdata;

        *pixels = surface->pixels;
        *pitch = surface->pitch;
        return 0;
    }
}

static int
SW_SetTexturePalette(SDL_Renderer * renderer, SDL_Texture * texture,
                     const SDL_Color * colors, int firstcolor, int ncolors)
{
    if (SDL_ISPIXELFORMAT_FOURCC(texture->format)) {
        SDL_SetError("YUV textures don't have a palette");
        return -1;
    } else {
        SDL_Surface *surface = (SDL_Surface *) texture->driverdata;

        return SDL_SetPaletteColors(surface->format->palette, colors,
                                    firstcolor, ncolors);
    }
}

static int
SW_GetTexturePalette(SDL_Renderer * renderer, SDL_Texture * texture,
                     SDL_Color * colors, int firstcolor, int ncolors)
{
    if (SDL_ISPIXELFORMAT_FOURCC(texture->format)) {
        SDL_SetError("YUV textures don't have a palette");
        return -1;
    } else {
        SDL_Surface *surface = (SDL_Surface *) texture->driverdata;

        SDL_memcpy(colors, &surface->format->palette->colors[firstcolor],
                   ncolors * sizeof(*colors));
        return 0;
    }
}

static int
SW_SetTextureColorMod(SDL_Renderer * renderer, SDL_Texture * texture)
{
    SDL_Surface *surface = (SDL_Surface *) texture->driverdata;
    return SDL_SetSurfaceColorMod(surface, texture->r, texture->g,
                                  texture->b);
}

static int
SW_SetTextureAlphaMod(SDL_Renderer * renderer, SDL_Texture * texture)
{
    SDL_Surface *surface = (SDL_Surface *) texture->driverdata;
    return SDL_SetSurfaceAlphaMod(surface, texture->a);
}

static int
SW_SetTextureBlendMode(SDL_Renderer * renderer, SDL_Texture * texture)
{
    SDL_Surface *surface = (SDL_Surface *) texture->driverdata;
    return SDL_SetSurfaceBlendMode(surface, texture->blendMode);
}

static int
SW_SetTextureScaleMode(SDL_Renderer * renderer, SDL_Texture * texture)
{
    SDL_Surface *surface = (SDL_Surface *) texture->driverdata;
    return SDL_SetSurfaceScaleMode(surface, texture->scaleMode);
}

static int
SW_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                 const SDL_Rect * rect, const void *pixels, int pitch)
{
    if (SDL_ISPIXELFORMAT_FOURCC(texture->format)) {
        return SDL_SW_UpdateYUVTexture((SDL_SW_YUVTexture *)
                                       texture->driverdata, rect, pixels,
                                       pitch);
    } else {
        SDL_Surface *surface = (SDL_Surface *) texture->driverdata;
        Uint8 *src, *dst;
        int row;
        size_t length;

        src = (Uint8 *) pixels;
        dst =
            (Uint8 *) surface->pixels + rect->y * surface->pitch +
            rect->x * surface->format->BytesPerPixel;
        length = rect->w * surface->format->BytesPerPixel;
        for (row = 0; row < rect->h; ++row) {
            SDL_memcpy(dst, src, length);
            src += pitch;
            dst += surface->pitch;
        }
        return 0;
    }
}

static int
SW_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
               const SDL_Rect * rect, int markDirty, void **pixels,
               int *pitch)
{
    if (SDL_ISPIXELFORMAT_FOURCC(texture->format)) {
        return SDL_SW_LockYUVTexture((SDL_SW_YUVTexture *)
                                     texture->driverdata, rect, markDirty,
                                     pixels, pitch);
    } else {
        SDL_Surface *surface = (SDL_Surface *) texture->driverdata;

        *pixels =
            (void *) ((Uint8 *) surface->pixels + rect->y * surface->pitch +
                      rect->x * surface->format->BytesPerPixel);
        *pitch = surface->pitch;
        return 0;
    }
}

static void
SW_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    if (SDL_ISPIXELFORMAT_FOURCC(texture->format)) {
        SDL_SW_UnlockYUVTexture((SDL_SW_YUVTexture *) texture->driverdata);
    }
}

static int
SW_RenderPoint(SDL_Renderer * renderer, int x, int y)
{
    SW_RenderData *data = (SW_RenderData *) renderer->driverdata;
    SDL_Rect rect;
    int status;

    rect.x = x;
    rect.y = y;
    rect.w = 1;
    rect.h = 1;

    if (data->renderer->info.flags & SDL_RENDERER_PRESENTCOPY) {
        SDL_AddDirtyRect(&data->dirty, &rect);
    }

    if (data->renderer->LockTexture(data->renderer,
                                    data->texture[data->current_texture],
                                    &rect, 1,
                                    &data->surface.pixels,
                                    &data->surface.pitch) < 0) {
        return -1;
    }

    data->surface.w = 1;
    data->surface.h = 1;
    data->surface.clip_rect.w = 1;
    data->surface.clip_rect.h = 1;

    if (renderer->blendMode == SDL_BLENDMODE_NONE ||
        renderer->blendMode == SDL_BLENDMODE_MASK) {
        Uint32 color =
            SDL_MapRGBA(data->surface.format, renderer->r, renderer->g,
                        renderer->b, renderer->a);

        status = SDL_DrawPoint(&data->surface, 0, 0, color);
    } else {
        status =
            SDL_BlendPoint(&data->surface, 0, 0, renderer->blendMode,
                           renderer->r, renderer->g, renderer->b,
                           renderer->a);
    }

    data->renderer->UnlockTexture(data->renderer,
                                  data->texture[data->current_texture]);
    return status;
}

static int
SW_RenderLine(SDL_Renderer * renderer, int x1, int y1, int x2, int y2)
{
    SW_RenderData *data = (SW_RenderData *) renderer->driverdata;
    SDL_Rect rect;
    int status;

    if (x1 < x2) {
        rect.x = x1;
        rect.w = (x2 - x1) + 1;
        x2 -= x1;
        x1 = 0;
    } else {
        rect.x = x2;
        rect.w = (x1 - x2) + 1;
        x1 -= x2;
        x2 = 0;
    }
    if (y1 < y2) {
        rect.y = y1;
        rect.h = (y2 - y1) + 1;
        y2 -= y1;
        y1 = 0;
    } else {
        rect.y = y2;
        rect.h = (y1 - y2) + 1;
        y1 -= y2;
        y2 = 0;
    }

    if (data->renderer->info.flags & SDL_RENDERER_PRESENTCOPY) {
        SDL_AddDirtyRect(&data->dirty, &rect);
    }

    if (data->renderer->LockTexture(data->renderer,
                                    data->texture[data->current_texture],
                                    &rect, 1,
                                    &data->surface.pixels,
                                    &data->surface.pitch) < 0) {
        return -1;
    }

    data->surface.w = rect.w;
    data->surface.h = rect.h;
    data->surface.clip_rect.w = rect.w;
    data->surface.clip_rect.h = rect.h;

    if (renderer->blendMode == SDL_BLENDMODE_NONE ||
        renderer->blendMode == SDL_BLENDMODE_MASK) {
        Uint32 color =
            SDL_MapRGBA(data->surface.format, renderer->r, renderer->g,
                        renderer->b, renderer->a);

        status = SDL_DrawLine(&data->surface, x1, y1, x2, y2, color);
    } else {
        status =
            SDL_BlendLine(&data->surface, x1, y1, x2, y2, renderer->blendMode,
                          renderer->r, renderer->g, renderer->b, renderer->a);
    }

    data->renderer->UnlockTexture(data->renderer,
                                  data->texture[data->current_texture]);
    return status;
}

static int
SW_RenderFill(SDL_Renderer * renderer, const SDL_Rect * rect)
{
    SW_RenderData *data = (SW_RenderData *) renderer->driverdata;
    SDL_Rect real_rect;
    int status;

    if (data->renderer->info.flags & SDL_RENDERER_PRESENTCOPY) {
        SDL_AddDirtyRect(&data->dirty, rect);
    }

    if (data->renderer->LockTexture(data->renderer,
                                    data->texture[data->current_texture],
                                    rect, 1, &data->surface.pixels,
                                    &data->surface.pitch) < 0) {
        return -1;
    }

    data->surface.w = rect->w;
    data->surface.h = rect->h;
    data->surface.clip_rect.w = rect->w;
    data->surface.clip_rect.h = rect->h;
    real_rect = data->surface.clip_rect;

    if (renderer->blendMode == SDL_BLENDMODE_NONE) {
        Uint32 color =
            SDL_MapRGBA(data->surface.format, renderer->r, renderer->g,
                        renderer->b, renderer->a);

        status = SDL_FillRect(&data->surface, &real_rect, color);
    } else {
        status =
            SDL_BlendRect(&data->surface, &real_rect, renderer->blendMode,
                          renderer->r, renderer->g, renderer->b, renderer->a);
    }

    data->renderer->UnlockTexture(data->renderer,
                                  data->texture[data->current_texture]);
    return status;
}

static int
SW_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
              const SDL_Rect * srcrect, const SDL_Rect * dstrect)
{
    SW_RenderData *data = (SW_RenderData *) renderer->driverdata;
    SDL_Window *window = SDL_GetWindowFromID(renderer->window);
    int status;

    if (data->renderer->info.flags & SDL_RENDERER_PRESENTCOPY) {
        SDL_AddDirtyRect(&data->dirty, dstrect);
    }

    if (data->renderer->LockTexture(data->renderer,
                                    data->texture[data->current_texture],
                                    dstrect, 1, &data->surface.pixels,
                                    &data->surface.pitch) < 0) {
        return -1;
    }

    if (SDL_ISPIXELFORMAT_FOURCC(texture->format)) {
        status =
            SDL_SW_CopyYUVToRGB((SDL_SW_YUVTexture *) texture->driverdata,
                                srcrect, data->format, dstrect->w, dstrect->h,
                                data->surface.pixels, data->surface.pitch);
    } else {
        SDL_Surface *surface = (SDL_Surface *) texture->driverdata;
        SDL_Rect real_srcrect = *srcrect;
        SDL_Rect real_dstrect;

        data->surface.w = dstrect->w;
        data->surface.h = dstrect->h;
        data->surface.clip_rect.w = dstrect->w;
        data->surface.clip_rect.h = dstrect->h;
        real_dstrect = data->surface.clip_rect;

        status =
            SDL_LowerBlit(surface, &real_srcrect, &data->surface,
                          &real_dstrect);
    }
    data->renderer->UnlockTexture(data->renderer,
                                  data->texture[data->current_texture]);
    return status;
}

static int
SW_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                    void * pixels, int pitch)
{
    SW_RenderData *data = (SW_RenderData *) renderer->driverdata;
    const Uint8 *src;
    Uint8 *dst;
    int src_pitch, dst_pitch, w, h;

    if (data->renderer->LockTexture(data->renderer,
                                    data->texture[data->current_texture],
                                    rect, 0, &data->surface.pixels,
                                    &data->surface.pitch) < 0) {
        return -1;
    }

    src = data->surface.pixels;
    src_pitch = data->surface.pitch;
    dst = pixels;
    dst_pitch = pitch;
    h = rect->h;
    w = rect->w * data->surface.format->BytesPerPixel;
    while (h--) {
        SDL_memcpy(dst, src, w);
        src += src_pitch;
        dst += dst_pitch;
    }

    data->renderer->UnlockTexture(data->renderer,
                                  data->texture[data->current_texture]);
    return 0;
}

static int
SW_RenderWritePixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                     const void * pixels, int pitch)
{
    SW_RenderData *data = (SW_RenderData *) renderer->driverdata;
    const Uint8 *src;
    Uint8 *dst;
    int src_pitch, dst_pitch, w, h;

    if (data->renderer->info.flags & SDL_RENDERER_PRESENTCOPY) {
        SDL_AddDirtyRect(&data->dirty, rect);
    }

    if (data->renderer->LockTexture(data->renderer,
                                    data->texture[data->current_texture],
                                    rect, 1, &data->surface.pixels,
                                    &data->surface.pitch) < 0) {
        return -1;
    }

    src = pixels;
    src_pitch = pitch;
    dst = data->surface.pixels;
    dst_pitch = data->surface.pitch;
    h = rect->h;
    w = rect->w * data->surface.format->BytesPerPixel;
    while (h--) {
        SDL_memcpy(dst, src, w);
        src += src_pitch;
        dst += dst_pitch;
    }

    data->renderer->UnlockTexture(data->renderer,
                                  data->texture[data->current_texture]);
    return 0;
}

static void
SW_RenderPresent(SDL_Renderer * renderer)
{
    SW_RenderData *data = (SW_RenderData *) renderer->driverdata;
    SDL_Texture *texture = data->texture[data->current_texture];

    /* Send the data to the display */
    if (data->renderer->info.flags & SDL_RENDERER_PRESENTCOPY) {
        SDL_DirtyRect *dirty;
        for (dirty = data->dirty.list; dirty; dirty = dirty->next) {
            data->renderer->RenderCopy(data->renderer, texture, &dirty->rect,
                                       &dirty->rect);
        }
        SDL_ClearDirtyRects(&data->dirty);
    } else {
        SDL_Rect rect;
        rect.x = 0;
        rect.y = 0;
        rect.w = texture->w;
        rect.h = texture->h;
        data->renderer->RenderCopy(data->renderer, texture, &rect, &rect);
    }
    data->renderer->RenderPresent(data->renderer);

    /* Update the flipping chain, if any */
    if (renderer->info.flags & SDL_RENDERER_PRESENTFLIP2) {
        data->current_texture = (data->current_texture + 1) % 2;
    } else if (renderer->info.flags & SDL_RENDERER_PRESENTFLIP3) {
        data->current_texture = (data->current_texture + 1) % 3;
    }
}

static void
SW_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    if (SDL_ISPIXELFORMAT_FOURCC(texture->format)) {
        SDL_SW_DestroyYUVTexture((SDL_SW_YUVTexture *) texture->driverdata);
    } else {
        SDL_Surface *surface = (SDL_Surface *) texture->driverdata;

        SDL_FreeSurface(surface);
    }
}

static void
SW_DestroyRenderer(SDL_Renderer * renderer)
{
    SW_RenderData *data = (SW_RenderData *) renderer->driverdata;
    SDL_Window *window = SDL_GetWindowFromID(renderer->window);
    SDL_VideoDisplay *display = SDL_GetDisplayFromWindow(window);
    int i;

    if (data) {
        for (i = 0; i < SDL_arraysize(data->texture); ++i) {
            if (data->texture[i]) {
                DestroyTexture(data->renderer, data->texture[i]);
            }
        }
        if (data->surface.format) {
            SDL_SetSurfacePalette(&data->surface, NULL);
            SDL_FreeFormat(data->surface.format);
        }
        if (display->palette) {
            SDL_DelPaletteWatch(display->palette, DisplayPaletteChanged,
                                data);
        }
        if (data->renderer) {
            data->renderer->DestroyRenderer(data->renderer);
        }
        SDL_FreeDirtyRects(&data->dirty);
        SDL_free(data);
    }
    SDL_free(renderer);
}

/* vi: set ts=4 sw=4 expandtab: */
