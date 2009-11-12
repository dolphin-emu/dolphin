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
#include "../SDL_sysvideo.h"
#include "../SDL_yuv_sw_c.h"
#include "../SDL_renderer_sw.h"


/* SDL surface based renderer implementation */

static SDL_Renderer *SDL_DUMMY_CreateRenderer(SDL_Window * window,
                                              Uint32 flags);
static int SDL_DUMMY_RenderPoint(SDL_Renderer * renderer, int x, int y);
static int SDL_DUMMY_RenderLine(SDL_Renderer * renderer, int x1, int y1,
                                int x2, int y2);
static int SDL_DUMMY_RenderFill(SDL_Renderer * renderer,
                                const SDL_Rect * rect);
static int SDL_DUMMY_RenderCopy(SDL_Renderer * renderer,
                                SDL_Texture * texture,
                                const SDL_Rect * srcrect,
                                const SDL_Rect * dstrect);
static void SDL_DUMMY_RenderPresent(SDL_Renderer * renderer);
static void SDL_DUMMY_DestroyRenderer(SDL_Renderer * renderer);


SDL_RenderDriver SDL_DUMMY_RenderDriver = {
    SDL_DUMMY_CreateRenderer,
    {
     "dummy",
     (SDL_RENDERER_SINGLEBUFFER | SDL_RENDERER_PRESENTCOPY |
      SDL_RENDERER_PRESENTFLIP2 | SDL_RENDERER_PRESENTFLIP3 |
      SDL_RENDERER_PRESENTDISCARD),
     }
};

typedef struct
{
    int current_screen;
    SDL_Surface *screens[3];
} SDL_DUMMY_RenderData;

SDL_Renderer *
SDL_DUMMY_CreateRenderer(SDL_Window * window, Uint32 flags)
{
    SDL_VideoDisplay *display = SDL_GetDisplayFromWindow(window);
    SDL_DisplayMode *displayMode = &display->current_mode;
    SDL_Renderer *renderer;
    SDL_DUMMY_RenderData *data;
    int i, n;
    int bpp;
    Uint32 Rmask, Gmask, Bmask, Amask;

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

    data = (SDL_DUMMY_RenderData *) SDL_malloc(sizeof(*data));
    if (!data) {
        SDL_DUMMY_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return NULL;
    }
    SDL_zerop(data);

    renderer->RenderPoint = SDL_DUMMY_RenderPoint;
    renderer->RenderLine = SDL_DUMMY_RenderLine;
    renderer->RenderFill = SDL_DUMMY_RenderFill;
    renderer->RenderCopy = SDL_DUMMY_RenderCopy;
    renderer->RenderPresent = SDL_DUMMY_RenderPresent;
    renderer->DestroyRenderer = SDL_DUMMY_DestroyRenderer;
    renderer->info.name = SDL_DUMMY_RenderDriver.info.name;
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
    for (i = 0; i < n; ++i) {
        data->screens[i] =
            SDL_CreateRGBSurface(0, window->w, window->h, bpp, Rmask, Gmask,
                                 Bmask, Amask);
        if (!data->screens[i]) {
            SDL_DUMMY_DestroyRenderer(renderer);
            return NULL;
        }
        SDL_SetSurfacePalette(data->screens[i], display->palette);
    }
    data->current_screen = 0;

    return renderer;
}

static int
SDL_DUMMY_RenderPoint(SDL_Renderer * renderer, int x, int y)
{
    SDL_DUMMY_RenderData *data =
        (SDL_DUMMY_RenderData *) renderer->driverdata;
    SDL_Surface *target = data->screens[data->current_screen];
    int status;

    if (renderer->blendMode == SDL_BLENDMODE_NONE ||
        renderer->blendMode == SDL_BLENDMODE_MASK) {
        Uint32 color =
            SDL_MapRGBA(target->format, renderer->r, renderer->g, renderer->b,
                        renderer->a);

        status = SDL_DrawPoint(target, x, y, color);
    } else {
        status =
            SDL_BlendPoint(target, x, y, renderer->blendMode, renderer->r,
                           renderer->g, renderer->b, renderer->a);
    }
    return status;
}

static int
SDL_DUMMY_RenderLine(SDL_Renderer * renderer, int x1, int y1, int x2, int y2)
{
    SDL_DUMMY_RenderData *data =
        (SDL_DUMMY_RenderData *) renderer->driverdata;
    SDL_Surface *target = data->screens[data->current_screen];
    int status;

    if (renderer->blendMode == SDL_BLENDMODE_NONE ||
        renderer->blendMode == SDL_BLENDMODE_MASK) {
        Uint32 color =
            SDL_MapRGBA(target->format, renderer->r, renderer->g, renderer->b,
                        renderer->a);

        status = SDL_DrawLine(target, x1, y1, x2, y2, color);
    } else {
        status =
            SDL_BlendLine(target, x1, y1, x2, y2, renderer->blendMode,
                          renderer->r, renderer->g, renderer->b, renderer->a);
    }
    return status;
}

static int
SDL_DUMMY_RenderFill(SDL_Renderer * renderer, const SDL_Rect * rect)
{
    SDL_DUMMY_RenderData *data =
        (SDL_DUMMY_RenderData *) renderer->driverdata;
    SDL_Surface *target = data->screens[data->current_screen];
    SDL_Rect real_rect = *rect;
    int status;

    if (renderer->blendMode == SDL_BLENDMODE_NONE) {
        Uint32 color =
            SDL_MapRGBA(target->format, renderer->r, renderer->g, renderer->b,
                        renderer->a);

        status = SDL_FillRect(target, &real_rect, color);
    } else {
        status =
            SDL_BlendRect(target, &real_rect, renderer->blendMode,
                          renderer->r, renderer->g, renderer->b, renderer->a);
    }
    return status;
}

static int
SDL_DUMMY_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                     const SDL_Rect * srcrect, const SDL_Rect * dstrect)
{
    SDL_DUMMY_RenderData *data =
        (SDL_DUMMY_RenderData *) renderer->driverdata;
    SDL_Window *window = SDL_GetWindowFromID(renderer->window);
    SDL_VideoDisplay *display = SDL_GetDisplayFromWindow(window);

    if (SDL_ISPIXELFORMAT_FOURCC(texture->format)) {
        SDL_Surface *target = data->screens[data->current_screen];
        void *pixels =
            (Uint8 *) target->pixels + dstrect->y * target->pitch +
            dstrect->x * target->format->BytesPerPixel;
        return SDL_SW_CopyYUVToRGB((SDL_SW_YUVTexture *) texture->driverdata,
                                   srcrect, display->current_mode.format,
                                   dstrect->w, dstrect->h, pixels,
                                   target->pitch);
    } else {
        SDL_Surface *surface = (SDL_Surface *) texture->driverdata;
        SDL_Surface *target = data->screens[data->current_screen];
        SDL_Rect real_srcrect = *srcrect;
        SDL_Rect real_dstrect = *dstrect;

        return SDL_LowerBlit(surface, &real_srcrect, target, &real_dstrect);
    }
}

static void
SDL_DUMMY_RenderPresent(SDL_Renderer * renderer)
{
    static int frame_number;
    SDL_DUMMY_RenderData *data =
        (SDL_DUMMY_RenderData *) renderer->driverdata;

    /* Send the data to the display */
    if (SDL_getenv("SDL_VIDEO_DUMMY_SAVE_FRAMES")) {
        char file[128];
        SDL_snprintf(file, sizeof(file), "SDL_window%d-%8.8d.bmp",
                     renderer->window, ++frame_number);
        SDL_SaveBMP(data->screens[data->current_screen], file);
    }

    /* Update the flipping chain, if any */
    if (renderer->info.flags & SDL_RENDERER_PRESENTFLIP2) {
        data->current_screen = (data->current_screen + 1) % 2;
    } else if (renderer->info.flags & SDL_RENDERER_PRESENTFLIP3) {
        data->current_screen = (data->current_screen + 1) % 3;
    }
}

static void
SDL_DUMMY_DestroyRenderer(SDL_Renderer * renderer)
{
    SDL_DUMMY_RenderData *data =
        (SDL_DUMMY_RenderData *) renderer->driverdata;
    int i;

    if (data) {
        for (i = 0; i < SDL_arraysize(data->screens); ++i) {
            if (data->screens[i]) {
                SDL_FreeSurface(data->screens[i]);
            }
        }
        SDL_free(data);
    }
    SDL_free(renderer);
}

/* vi: set ts=4 sw=4 expandtab: */
