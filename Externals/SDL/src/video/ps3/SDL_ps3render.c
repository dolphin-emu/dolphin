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

#include "SDL_ps3video.h"
#include "SDL_ps3spe_c.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <asm/ps3fb.h>


/* Stores the executable name */
extern spe_program_handle_t yuv2rgb_spu;
extern spe_program_handle_t bilin_scaler_spu;

/* SDL surface based renderer implementation */
static SDL_Renderer *SDL_PS3_CreateRenderer(SDL_Window * window,
                                              Uint32 flags);
static int SDL_PS3_DisplayModeChanged(SDL_Renderer * renderer);
static int SDL_PS3_ActivateRenderer(SDL_Renderer * renderer);
static int SDL_PS3_RenderPoint(SDL_Renderer * renderer, int x, int y);
static int SDL_PS3_RenderLine(SDL_Renderer * renderer, int x1, int y1,
                                int x2, int y2);
static int SDL_PS3_RenderFill(SDL_Renderer * renderer,
                                const SDL_Rect * rect);
static int SDL_PS3_RenderCopy(SDL_Renderer * renderer,
                                SDL_Texture * texture,
                                const SDL_Rect * srcrect,
                                const SDL_Rect * dstrect);
static void SDL_PS3_RenderPresent(SDL_Renderer * renderer);
static void SDL_PS3_DestroyRenderer(SDL_Renderer * renderer);

/* Texture */
static int PS3_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static int PS3_QueryTexturePixels(SDL_Renderer * renderer, SDL_Texture * texture, void **pixels, int *pitch);
static int PS3_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture, const SDL_Rect * rect, const void *pixels, int pitch);
static int PS3_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture, const SDL_Rect * rect, int markDirty, void **pixels, int *pitch);
static void PS3_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static void PS3_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture);


SDL_RenderDriver SDL_PS3_RenderDriver = {
    SDL_PS3_CreateRenderer,
    {
     "ps3",
     (SDL_RENDERER_SINGLEBUFFER | SDL_RENDERER_PRESENTVSYNC |
      SDL_RENDERER_PRESENTFLIP2 | SDL_RENDERER_PRESENTDISCARD |
      SDL_RENDERER_ACCELERATED),
     (SDL_TEXTUREMODULATE_NONE),
     (SDL_BLENDMODE_NONE),
     /* We use bilinear scaling on the SPE for YV12 & IYUV
      * (width and height % 8 = 0) */
     (SDL_TEXTURESCALEMODE_SLOW)
     }
};

typedef struct
{
    int current_screen;
    SDL_Surface *screen;
    SDL_VideoDisplay *display;
    /* adress of the centered image in the framebuffer (double buffered) */
    uint8_t *center[2];

    /* width of input (bounded by writeable width) */
    unsigned int bounded_width;
    /* height of input (bounded by writeable height) */
    unsigned int bounded_height;
    /* offset from the left side (used for centering) */
    unsigned int offset_left;
    /* offset from the upper side (used for centering) */
    unsigned int offset_top;
    /* width of screen which is writeable */
    unsigned int wr_width;
    /* width of screen which is writeable */
    unsigned int wr_height;
    /* size of a screen line: width * bpp/8 */
    unsigned int line_length;

    /* Is the kernels fb size bigger than ~12MB
     * double buffering will work for 1080p */
    unsigned int double_buffering;

    /* SPE threading stuff */
    spu_data_t *converter_thread_data;
    spu_data_t *scaler_thread_data;

    /* YUV converting transfer data */
    volatile struct yuv2rgb_parms_t * converter_parms __attribute__((aligned(128)));
    /* Scaler transfer data */
    volatile struct scale_parms_t * scaler_parms __attribute__((aligned(128)));
} SDL_PS3_RenderData;

typedef struct
{
    int pitch;
    /* Image data */
    volatile void *pixels;
    /* Use software renderer for not supported formats */
    SDL_SW_YUVTexture *yuv;
} PS3_TextureData;

SDL_Renderer *
SDL_PS3_CreateRenderer(SDL_Window * window, Uint32 flags)
{
    deprintf(1, "+SDL_PS3_CreateRenderer()\n");
    SDL_VideoDisplay *display = SDL_GetDisplayFromWindow(window);
    SDL_DisplayMode *displayMode = &display->current_mode;
    SDL_VideoData *devdata = display->device->driverdata;
    SDL_Renderer *renderer;
    SDL_PS3_RenderData *data;
    struct ps3fb_ioctl_res res;
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

    data = (SDL_PS3_RenderData *) SDL_malloc(sizeof(*data));
    if (!data) {
        SDL_PS3_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return NULL;
    }
    SDL_zerop(data);

    renderer->CreateTexture = PS3_CreateTexture;
    renderer->DestroyTexture = PS3_DestroyTexture;
    renderer->QueryTexturePixels = PS3_QueryTexturePixels;
    renderer->UpdateTexture = PS3_UpdateTexture;
    renderer->LockTexture = PS3_LockTexture;
    renderer->UnlockTexture = PS3_UnlockTexture;
    renderer->ActivateRenderer = SDL_PS3_ActivateRenderer;
    renderer->DisplayModeChanged = SDL_PS3_DisplayModeChanged;
    renderer->RenderPoint = SDL_PS3_RenderPoint;
    renderer->RenderLine = SDL_PS3_RenderLine;
    renderer->RenderFill = SDL_PS3_RenderFill;
    renderer->RenderCopy = SDL_PS3_RenderCopy;
    renderer->RenderPresent = SDL_PS3_RenderPresent;
    renderer->DestroyRenderer = SDL_PS3_DestroyRenderer;
    renderer->info.name = SDL_PS3_RenderDriver.info.name;
    renderer->info.flags = 0;
    renderer->window = window->id;
    renderer->driverdata = data;

    deprintf(1, "window->w = %u\n", window->w);
    deprintf(1, "window->h = %u\n", window->h);

    data->double_buffering = 0;

    /* Get ps3 screeninfo */
    if (ioctl(devdata->fbdev, PS3FB_IOCTL_SCREENINFO, (unsigned long)&res) < 0) {
        SDL_SetError("[PS3] PS3FB_IOCTL_SCREENINFO failed");
    }
    deprintf(2, "res.num_frames = %d\n", res.num_frames);

    /* Only use double buffering if enough fb memory is available */
    if (res.num_frames > 1) {
        renderer->info.flags |= SDL_RENDERER_PRESENTFLIP2;
        n = 2;
        data->double_buffering = 1;
    } else {
        renderer->info.flags |= SDL_RENDERER_PRESENTCOPY;
        n = 1;
    }

    data->screen =
        SDL_CreateRGBSurface(0, window->w, window->h, bpp, Rmask, Gmask,
                             Bmask, Amask);
    if (!data->screen) {
        SDL_PS3_DestroyRenderer(renderer);
        return NULL;
    }
    /* Allocate aligned memory for pixels */
    SDL_free(data->screen->pixels);
    data->screen->pixels = (void *)memalign(16, data->screen->h * data->screen->pitch);
    if (!data->screen->pixels) {
        SDL_FreeSurface(data->screen);
        SDL_OutOfMemory();
        return NULL;
    }
    SDL_memset(data->screen->pixels, 0, data->screen->h * data->screen->pitch);
    SDL_SetSurfacePalette(data->screen, display->palette);

    data->current_screen = 0;

    /* Create SPU parms structure */
    data->converter_parms = (struct yuv2rgb_parms_t *) memalign(16, sizeof(struct yuv2rgb_parms_t));
    data->scaler_parms = (struct scale_parms_t *) memalign(16, sizeof(struct scale_parms_t));
    if (data->converter_parms == NULL || data->scaler_parms == NULL) {
        SDL_PS3_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return NULL;
    }

    /* Set up the SPE threading data */
    data->converter_thread_data = (spu_data_t *) malloc(sizeof(spu_data_t));
    data->scaler_thread_data = (spu_data_t *) malloc(sizeof(spu_data_t));
    if (data->converter_thread_data == NULL || data->scaler_thread_data == NULL) {
        SDL_PS3_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return NULL;
    }

    /* Set up the SPE scaler (booted) */
    data->scaler_thread_data->program = bilin_scaler_spu;
    data->scaler_thread_data->program_name = "bilin_scaler_spu";
    data->scaler_thread_data->keepalive = 0;
    data->scaler_thread_data->booted = 0;

    /* Set up the SPE converter (always running) */
    data->converter_thread_data->program = yuv2rgb_spu;
    data->converter_thread_data->program_name = "yuv2rgb_spu";
    data->converter_thread_data->keepalive = 1;
    data->converter_thread_data->booted = 0;

    SPE_Start(data->converter_thread_data);

    deprintf(1, "-SDL_PS3_CreateRenderer()\n");
    return renderer;
}

static int
SDL_PS3_ActivateRenderer(SDL_Renderer * renderer)
{
    deprintf(1, "+PS3_ActivateRenderer()\n");
    SDL_PS3_RenderData *data = (SDL_PS3_RenderData *) renderer->driverdata;

    deprintf(1, "-PS3_ActivateRenderer()\n");
    return 0;
}

static int SDL_PS3_DisplayModeChanged(SDL_Renderer * renderer) {
    deprintf(1, "+PS3_DisplayModeChanged()\n");
    SDL_PS3_RenderData *data = (SDL_PS3_RenderData *) renderer->driverdata;

    deprintf(1, "-PS3_DisplayModeChanged()\n");
    return 0;
}

static int
PS3_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture) {
    deprintf(1, "+PS3_CreateTexture()\n");
    PS3_TextureData *data;
    data = (PS3_TextureData *) SDL_calloc(1, sizeof(*data));
    if (!data) {
        SDL_OutOfMemory();
        return -1;
    }
    data->pitch = (texture->w * SDL_BYTESPERPIXEL(texture->format));

    if (SDL_ISPIXELFORMAT_FOURCC(texture->format)) {
        /* Use SDLs SW_YUVTexture */
        data->yuv =
            SDL_SW_CreateYUVTexture(texture->format, texture->w, texture->h);
        if (!data->yuv) {
            SDL_OutOfMemory();
            return -1;
        }
        /* but align pixels */
        SDL_free(data->yuv->pixels);
        data->yuv->pixels = (Uint8 *)memalign(16, texture->w * texture->h * 2);
        if (!data->yuv->pixels) {
            SDL_OutOfMemory();
            return -1;
        }

        /* Redo: Find the pitch and offset values for the overlay */
        SDL_SW_YUVTexture *swdata = (SDL_SW_YUVTexture *) data->yuv;
        switch (texture->format) {
            case SDL_PIXELFORMAT_YV12:
            case SDL_PIXELFORMAT_IYUV:
                swdata->pitches[0] = texture->w;
                swdata->pitches[1] = swdata->pitches[0] / 2;
                swdata->pitches[2] = swdata->pitches[0] / 2;
                swdata->planes[0] = swdata->pixels;
                swdata->planes[1] = swdata->planes[0] + swdata->pitches[0] * texture->h;
                swdata->planes[2] = swdata->planes[1] + swdata->pitches[1] * texture->h / 2;
                break;
            case SDL_PIXELFORMAT_YUY2:
            case SDL_PIXELFORMAT_UYVY:
            case SDL_PIXELFORMAT_YVYU:
                swdata->pitches[0] = texture->w * 2;
                swdata->planes[0] = swdata->pixels;
                break;
            default:
                /* We should never get here (caught above) */
                break;
        }
    } else {
        data->pixels = NULL;
        data->pixels = SDL_malloc(texture->h * data->pitch);
        if (!data->pixels) {
            PS3_DestroyTexture(renderer, texture);
            SDL_OutOfMemory();
            return -1;
        }
    }
    texture->driverdata = data;
    deprintf(1, "-PS3_CreateTexture()\n");
    return 0;
}

static int
PS3_QueryTexturePixels(SDL_Renderer * renderer, SDL_Texture * texture,
                      void **pixels, int *pitch)
{
    deprintf(1, "+PS3_QueryTexturePixels()\n");
    PS3_TextureData *data = (PS3_TextureData *) texture->driverdata;

    if (SDL_ISPIXELFORMAT_FOURCC(texture->format)) {
        return SDL_SW_QueryYUVTexturePixels(data->yuv, pixels, pitch);
    } else {
        *pixels = (void *)data->pixels;
        *pitch = data->pitch;
    }

    deprintf(1, "-PS3_QueryTexturePixels()\n");
    return 0;
}

static int
PS3_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                       const SDL_Rect * rect, const void *pixels, int pitch)
{
    deprintf(1, "+PS3_UpdateTexture()\n");
    PS3_TextureData *data = (PS3_TextureData *) texture->driverdata;

    if (SDL_ISPIXELFORMAT_FOURCC(texture->format)) {
        return SDL_SW_UpdateYUVTexture(data->yuv, rect, pixels, pitch);
    } else {
        Uint8 *src, *dst;
        int row;
        size_t length;
        Uint8 *dstpixels;

        src = (Uint8 *) pixels;
        dst = (Uint8 *) dstpixels + rect->y * data->pitch + rect->x
                        * SDL_BYTESPERPIXEL(texture->format);
        length = rect->w * SDL_BYTESPERPIXEL(texture->format);
        /* Update the texture */
        for (row = 0; row < rect->h; ++row) {
            SDL_memcpy(dst, src, length);
            src += pitch;
            dst += data->pitch;
        }
    }
    deprintf(1, "-PS3_UpdateTexture()\n");
    return 0;
}

static int
PS3_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
               const SDL_Rect * rect, int markDirty, void **pixels,
               int *pitch)
{
    deprintf(1, "+PS3_LockTexture()\n");
    PS3_TextureData *data = (PS3_TextureData *) texture->driverdata;

    if (SDL_ISPIXELFORMAT_FOURCC(texture->format)) {
        deprintf(1, "-PS3_LockTexture()\n");
        return SDL_SW_LockYUVTexture(data->yuv, rect, markDirty, pixels, pitch);
    } else {
        *pixels =
            (void *) ((Uint8 *) data->pixels + rect->y * data->pitch +
                      rect->x * SDL_BYTESPERPIXEL(texture->format));
        *pitch = data->pitch;
        deprintf(1, "-PS3_LockTexture()\n");
        return 0;
    }
}

static void
PS3_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    deprintf(1, "+PS3_UnlockTexture()\n");
    PS3_TextureData *data = (PS3_TextureData *) texture->driverdata;

    if (SDL_ISPIXELFORMAT_FOURCC(texture->format)) {
        SDL_SW_UnlockYUVTexture(data->yuv);
    }
    deprintf(1, "-PS3_UnlockTexture()\n");
}

static void
PS3_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    deprintf(1, "+PS3_DestroyTexture()\n");
    PS3_TextureData *data = (PS3_TextureData *) texture->driverdata;

    if (!data) {
        return;
    }
    if (data->yuv) {
        SDL_SW_DestroyYUVTexture(data->yuv);
    }
    if (data->pixels) {
        SDL_free((void *)data->pixels);
    }
    deprintf(1, "-PS3_DestroyTexture()\n");
}

static int
SDL_PS3_RenderPoint(SDL_Renderer * renderer, int x, int y)
{
    SDL_PS3_RenderData *data =
        (SDL_PS3_RenderData *) renderer->driverdata;
    SDL_Surface *target = data->screen;
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
SDL_PS3_RenderLine(SDL_Renderer * renderer, int x1, int y1, int x2, int y2)
{
    SDL_PS3_RenderData *data =
        (SDL_PS3_RenderData *) renderer->driverdata;
    SDL_Surface *target = data->screen;
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
SDL_PS3_RenderFill(SDL_Renderer * renderer, const SDL_Rect * rect)
{
    deprintf(1, "SDL_PS3_RenderFill()\n");
    SDL_PS3_RenderData *data =
        (SDL_PS3_RenderData *) renderer->driverdata;
    SDL_Surface *target = data->screen;
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
SDL_PS3_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                     const SDL_Rect * srcrect, const SDL_Rect * dstrect)
{
    deprintf(1, "+SDL_PS3_RenderCopy()\n");
    SDL_PS3_RenderData *data =
        (SDL_PS3_RenderData *) renderer->driverdata;
    SDL_Window *window = SDL_GetWindowFromID(renderer->window);
    SDL_VideoDisplay *display = SDL_GetDisplayFromWindow(window);
    PS3_TextureData *txdata = (PS3_TextureData *) texture->driverdata;
    SDL_VideoData *devdata = display->device->driverdata;

    if (SDL_ISPIXELFORMAT_FOURCC(texture->format)) {
        deprintf(1, "Texture is in a FOURCC format\n");
        if ((texture->format == SDL_PIXELFORMAT_YV12 || texture->format == SDL_PIXELFORMAT_IYUV)
                && texture->w % 8 == 0 && texture->h % 8 == 0
                && dstrect->w % 8 == 0 && dstrect->h % 8 == 0) {
            deprintf(1, "Use SPE for scaling/converting\n");

            SDL_SW_YUVTexture *swdata = (SDL_SW_YUVTexture *) txdata->yuv;
            Uint8 *lum, *Cr, *Cb;
            Uint8 *scaler_out = NULL;
            Uint8 *dstpixels;
            switch (texture->format) {
                case SDL_PIXELFORMAT_YV12:
                    lum = swdata->planes[0];
                    Cr = swdata->planes[1];
                    Cb = swdata->planes[2];
                    break;
                case SDL_PIXELFORMAT_IYUV:
                    lum = swdata->planes[0];
                    Cr = swdata->planes[2];
                    Cb = swdata->planes[1];
                    break;
                default:
                    /* We should never get here (caught above) */
                    return -1;
            }

            if (srcrect->w != dstrect->w || srcrect->h != dstrect->h) {
                deprintf(1, "We need to scale the texture from %u x %u to %u x %u\n",
                        srcrect->w, srcrect->h, dstrect->w, dstrect->h);
                /* Alloc mem for scaled YUV picture */
                scaler_out = (Uint8 *) memalign(16, dstrect->w * dstrect->h + ((dstrect->w * dstrect->h) >> 1));
                if (scaler_out == NULL) {
                    SDL_OutOfMemory();
                    return -1;
                }

                /* Set parms for scaling */
                data->scaler_parms->src_pixel_width = srcrect->w;
                data->scaler_parms->src_pixel_height = srcrect->h;
                data->scaler_parms->dst_pixel_width = dstrect->w;
                data->scaler_parms->dst_pixel_height = dstrect->h;
                data->scaler_parms->y_plane = lum;
                data->scaler_parms->v_plane = Cr;
                data->scaler_parms->u_plane = Cb;
                data->scaler_parms->dstBuffer = scaler_out;
                data->scaler_thread_data->argp = (void *)data->scaler_parms;

                /* Scale the YUV overlay to given size */
                SPE_Start(data->scaler_thread_data);
                SPE_Stop(data->scaler_thread_data);

                /* Set parms for converting after scaling */
                data->converter_parms->y_plane = scaler_out;
                data->converter_parms->v_plane = scaler_out + dstrect->w * dstrect->h;
                data->converter_parms->u_plane = scaler_out + dstrect->w * dstrect->h + ((dstrect->w * dstrect->h) >> 2);
            } else {
                data->converter_parms->y_plane = lum;
                data->converter_parms->v_plane = Cr;
                data->converter_parms->u_plane = Cb;
            }

            dstpixels = (Uint8 *) data->screen->pixels + dstrect->y * data->screen->pitch + dstrect->x
                            * SDL_BYTESPERPIXEL(texture->format);
            data->converter_parms->src_pixel_width = dstrect->w;
            data->converter_parms->src_pixel_height = dstrect->h;
            data->converter_parms->dstBuffer = dstpixels/*(Uint8 *)data->screen->pixels*/;
            data->converter_thread_data->argp = (void *)data->converter_parms;

            /* Convert YUV texture to RGB */
            SPE_SendMsg(data->converter_thread_data, SPU_START);
            SPE_SendMsg(data->converter_thread_data, (unsigned int)data->converter_thread_data->argp);

            /* We can probably move that to RenderPresent() */
            SPE_WaitForMsg(data->converter_thread_data, SPU_FIN);
            if (scaler_out) {
                free(scaler_out);
            }
        } else {
            deprintf(1, "Use software for scaling/converting\n");
            Uint8 *dst;
            /* FIXME: Not good */
            dst = (Uint8 *) data->screen->pixels + dstrect->y * data->screen->pitch + dstrect->x
                            * SDL_BYTESPERPIXEL(texture->format);
            return SDL_SW_CopyYUVToRGB(txdata->yuv, srcrect, display->current_mode.format,
                                   dstrect->w, dstrect->h, dst/*data->screen->pixels*/,
                                   data->screen->pitch);
        }
    } else {
        deprintf(1, "SDL_ISPIXELFORMAT_FOURCC = false\n");

        Uint8 *src, *dst;
        int row;
        size_t length;
        Uint8 *dstpixels;

        src = (Uint8 *) txdata->pixels;
        dst = (Uint8 *) data->screen->pixels + dstrect->y * data->screen->pitch + dstrect->x
                        * SDL_BYTESPERPIXEL(texture->format);
        length = dstrect->w * SDL_BYTESPERPIXEL(texture->format);
        for (row = 0; row < dstrect->h; ++row) {
            SDL_memcpy(dst, src, length);
            src += txdata->pitch;
            dst += data->screen->pitch;
        }
    }

    deprintf(1, "-SDL_PS3_RenderCopy()\n");
    return 0;
}

static void
SDL_PS3_RenderPresent(SDL_Renderer * renderer)
{
    deprintf(1, "+SDL_PS3_RenderPresent()\n");
    SDL_PS3_RenderData *data =
        (SDL_PS3_RenderData *) renderer->driverdata;
    SDL_Window *window = SDL_GetWindowFromID(renderer->window);
    SDL_VideoDisplay *display = SDL_GetDisplayFromWindow(window);
    SDL_VideoData *devdata = display->device->driverdata;

    /* Send the data to the screen */
    /* Get screeninfo */
    struct fb_fix_screeninfo fb_finfo;
    if (ioctl(devdata->fbdev, FBIOGET_FSCREENINFO, &fb_finfo)) {
        SDL_SetError("[PS3] Can't get fixed screeninfo");
    }
    struct fb_var_screeninfo fb_vinfo;
    if (ioctl(devdata->fbdev, FBIOGET_VSCREENINFO, &fb_vinfo)) {
        SDL_SetError("[PS3] Can't get VSCREENINFO");
    }

    /* 16 and 15 bpp is reported as 16 bpp */
    //txdata->bpp = fb_vinfo.bits_per_pixel;
    //if (txdata->bpp == 16)
    //    txdata->bpp = fb_vinfo.red.length + fb_vinfo.green.length + fb_vinfo.blue.length;

    /* Adjust centering */
    data->bounded_width = window->w < fb_vinfo.xres ? window->w : fb_vinfo.xres;
    data->bounded_height = window->h < fb_vinfo.yres ? window->h : fb_vinfo.yres;
    /* We could use SDL's CENTERED flag for centering */
    data->offset_left = (fb_vinfo.xres - data->bounded_width) >> 1;
    data->offset_top = (fb_vinfo.yres - data->bounded_height) >> 1;
    data->center[0] = devdata->frame_buffer + data->offset_left * /*txdata->bpp/8*/ 4 +
                data->offset_top * fb_finfo.line_length;
    data->center[1] = data->center[0] + fb_vinfo.yres * fb_finfo.line_length;

    deprintf(1, "offset_left = %u\n", data->offset_left);
    deprintf(1, "offset_top = %u\n", data->offset_top);

    /* Set SPU parms for copying the surface to framebuffer */
    devdata->fb_parms->data = (unsigned char *)data->screen->pixels;
    devdata->fb_parms->center = data->center[data->current_screen];
    devdata->fb_parms->out_line_stride = fb_finfo.line_length;
    devdata->fb_parms->in_line_stride = window->w * /*txdata->bpp / 8*/4;
    devdata->fb_parms->bounded_input_height = data->bounded_height;
    devdata->fb_parms->bounded_input_width = data->bounded_width;
    //devdata->fb_parms->fb_pixel_size = txdata->bpp / 8;
    devdata->fb_parms->fb_pixel_size = 4;//SDL_BYTESPERPIXEL(window->format);

    deprintf(3, "[PS3->SPU] fb_thread_data->argp = 0x%x\n", devdata->fb_thread_data->argp);

    /* Copying.. */
    SPE_SendMsg(devdata->fb_thread_data, SPU_START);
    SPE_SendMsg(devdata->fb_thread_data, (unsigned int)devdata->fb_thread_data->argp);

    SPE_WaitForMsg(devdata->fb_thread_data, SPU_FIN);

    /* Wait for vsync */
    if (renderer->info.flags & SDL_RENDERER_PRESENTVSYNC) {
        unsigned long crt = 0;
        deprintf(1, "[PS3] Wait for vsync\n");
        ioctl(devdata->fbdev, FBIO_WAITFORVSYNC, &crt);
    }

    /* Page flip */
    deprintf(1, "[PS3] Page flip to buffer #%u 0x%x\n", data->current_screen, data->center[data->current_screen]);
    ioctl(devdata->fbdev, PS3FB_IOCTL_FSEL, (unsigned long)&data->current_screen);

    /* Update the flipping chain, if any */
    if (data->double_buffering) {
        data->current_screen = (data->current_screen + 1) % 2;
    }
    deprintf(1, "-SDL_PS3_RenderPresent()\n");
}

static void
SDL_PS3_DestroyRenderer(SDL_Renderer * renderer)
{
    deprintf(1, "+SDL_PS3_DestroyRenderer()\n");
    SDL_PS3_RenderData *data =
        (SDL_PS3_RenderData *) renderer->driverdata;
    int i;

    if (data) {
        for (i = 0; i < SDL_arraysize(data->screen); ++i) {
            if (data->screen) {
                SDL_FreeSurface(data->screen);
            }
        }

        /* Shutdown SPE and release related resources */
        if (data->scaler_thread_data) {
            free((void *)data->scaler_thread_data);
        }
        if (data->scaler_parms) {
            free((void *)data->scaler_parms);
        }
        if (data->converter_thread_data) {
            SPE_Shutdown(data->converter_thread_data);
            free((void *)data->converter_thread_data);
        }
        if (data->converter_parms) {
            free((void *)data->converter_parms);
        }

        SDL_free(data);
    }
    SDL_free(renderer);
    deprintf(1, "-SDL_PS3_DestroyRenderer()\n");
}

/* vi: set ts=4 sw=4 expandtab: */
