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

#if SDL_VIDEO_RENDER_GDI

#include "SDL_win32video.h"
#include "../SDL_rect_c.h"
#include "../SDL_yuv_sw_c.h"
#include "../SDL_alphamult.h"

#ifdef _WIN32_WCE
#define NO_GETDIBBITS 1
#endif

/* GDI renderer implementation */

static SDL_Renderer *GDI_CreateRenderer(SDL_Window * window, Uint32 flags);
static int GDI_DisplayModeChanged(SDL_Renderer * renderer);
static int GDI_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static int GDI_QueryTexturePixels(SDL_Renderer * renderer,
                                  SDL_Texture * texture, void **pixels,
                                  int *pitch);
static int GDI_SetTexturePalette(SDL_Renderer * renderer,
                                 SDL_Texture * texture,
                                 const SDL_Color * colors, int firstcolor,
                                 int ncolors);
static int GDI_GetTexturePalette(SDL_Renderer * renderer,
                                 SDL_Texture * texture, SDL_Color * colors,
                                 int firstcolor, int ncolors);
static int GDI_SetTextureAlphaMod(SDL_Renderer * renderer,
                                  SDL_Texture * texture);
static int GDI_SetTextureBlendMode(SDL_Renderer * renderer,
                                   SDL_Texture * texture);
static int GDI_SetTextureScaleMode(SDL_Renderer * renderer,
                                   SDL_Texture * texture);
static int GDI_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                             const SDL_Rect * rect, const void *pixels,
                             int pitch);
static int GDI_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                           const SDL_Rect * rect, int markDirty,
                           void **pixels, int *pitch);
static void GDI_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static int GDI_SetDrawBlendMode(SDL_Renderer * renderer);
static int GDI_RenderPoint(SDL_Renderer * renderer, int x, int y);
static int GDI_RenderLine(SDL_Renderer * renderer, int x1, int y1, int x2,
                          int y2);
static int GDI_RenderFill(SDL_Renderer * renderer, const SDL_Rect * rect);
static int GDI_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                          const SDL_Rect * srcrect, const SDL_Rect * dstrect);
static void GDI_RenderPresent(SDL_Renderer * renderer);
static void GDI_DestroyTexture(SDL_Renderer * renderer,
                               SDL_Texture * texture);
static void GDI_DestroyRenderer(SDL_Renderer * renderer);


SDL_RenderDriver GDI_RenderDriver = {
    GDI_CreateRenderer,
    {
     "gdi",
     (SDL_RENDERER_SINGLEBUFFER | SDL_RENDERER_PRESENTCOPY |
      SDL_RENDERER_PRESENTFLIP2 | SDL_RENDERER_PRESENTFLIP3 |
      SDL_RENDERER_PRESENTDISCARD | SDL_RENDERER_ACCELERATED),
     (SDL_TEXTUREMODULATE_NONE | SDL_TEXTUREMODULATE_ALPHA),
     (SDL_BLENDMODE_NONE | SDL_BLENDMODE_MASK),
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
    HWND hwnd;
    HDC window_hdc;
    HDC render_hdc;
    HDC memory_hdc;
    HDC current_hdc;
#ifndef NO_GETDIBBITS
    LPBITMAPINFO bmi;
#endif
    HBITMAP hbm[3];
    int current_hbm;
    SDL_DirtyRectList dirty;
    SDL_bool makedirty;
} GDI_RenderData;

typedef struct
{
    SDL_SW_YUVTexture *yuv;
    Uint32 format;
    HPALETTE hpal;
    HBITMAP hbm;
    void *pixels;
    int pitch;
    SDL_bool premultiplied;
} GDI_TextureData;

static void
UpdateYUVTextureData(SDL_Texture * texture)
{
    GDI_TextureData *data = (GDI_TextureData *) texture->driverdata;
    SDL_Rect rect;

    rect.x = 0;
    rect.y = 0;
    rect.w = texture->w;
    rect.h = texture->h;
    SDL_SW_CopyYUVToRGB(data->yuv, &rect, data->format, texture->w,
                        texture->h, data->pixels, data->pitch);
}

void
GDI_AddRenderDriver(_THIS)
{
    SDL_AddRenderDriver(0, &GDI_RenderDriver);
}

SDL_Renderer *
GDI_CreateRenderer(SDL_Window * window, Uint32 flags)
{
    SDL_WindowData *windowdata = (SDL_WindowData *) window->driverdata;
    SDL_Renderer *renderer;
    GDI_RenderData *data;
    int bmi_size;
    HBITMAP hbm;
    int i, n;

    renderer = (SDL_Renderer *) SDL_calloc(1, sizeof(*renderer));
    if (!renderer) {
        SDL_OutOfMemory();
        return NULL;
    }

    data = (GDI_RenderData *) SDL_calloc(1, sizeof(*data));
    if (!data) {
        GDI_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return NULL;
    }

    renderer->DisplayModeChanged = GDI_DisplayModeChanged;
    renderer->CreateTexture = GDI_CreateTexture;
    renderer->QueryTexturePixels = GDI_QueryTexturePixels;
    renderer->SetTexturePalette = GDI_SetTexturePalette;
    renderer->GetTexturePalette = GDI_GetTexturePalette;
    renderer->SetTextureAlphaMod = GDI_SetTextureAlphaMod;
    renderer->SetTextureBlendMode = GDI_SetTextureBlendMode;
    renderer->SetTextureScaleMode = GDI_SetTextureScaleMode;
    renderer->UpdateTexture = GDI_UpdateTexture;
    renderer->LockTexture = GDI_LockTexture;
    renderer->UnlockTexture = GDI_UnlockTexture;
    renderer->SetDrawBlendMode = GDI_SetDrawBlendMode;
    renderer->RenderPoint = GDI_RenderPoint;
    renderer->RenderLine = GDI_RenderLine;
    renderer->RenderFill = GDI_RenderFill;
    renderer->RenderCopy = GDI_RenderCopy;
    renderer->RenderPresent = GDI_RenderPresent;
    renderer->DestroyTexture = GDI_DestroyTexture;
    renderer->DestroyRenderer = GDI_DestroyRenderer;
    renderer->info = GDI_RenderDriver.info;
    renderer->window = window->id;
    renderer->driverdata = data;

    renderer->info.flags = SDL_RENDERER_ACCELERATED;

    data->hwnd = windowdata->hwnd;
    data->window_hdc = windowdata->hdc;
    data->render_hdc = CreateCompatibleDC(data->window_hdc);
    data->memory_hdc = CreateCompatibleDC(data->window_hdc);

#ifndef NO_GETDIBBITS
    /* Fill in the compatible bitmap info */
    bmi_size = sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD);
    data->bmi = (LPBITMAPINFO) SDL_calloc(1, bmi_size);
    if (!data->bmi) {
        GDI_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return NULL;
    }
    data->bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

    hbm = CreateCompatibleBitmap(data->window_hdc, 1, 1);
    GetDIBits(data->window_hdc, hbm, 0, 1, NULL, data->bmi, DIB_RGB_COLORS);
    GetDIBits(data->window_hdc, hbm, 0, 1, NULL, data->bmi, DIB_RGB_COLORS);
    DeleteObject(hbm);
#endif

    if (flags & SDL_RENDERER_SINGLEBUFFER) {
        renderer->info.flags |=
            (SDL_RENDERER_SINGLEBUFFER | SDL_RENDERER_PRESENTCOPY);
        n = 0;
    } else if (flags & SDL_RENDERER_PRESENTFLIP2) {
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
        data->hbm[i] =
            CreateCompatibleBitmap(data->window_hdc, window->w, window->h);
        if (!data->hbm[i]) {
            GDI_DestroyRenderer(renderer);
            WIN_SetError("CreateCompatibleBitmap()");
            return NULL;
        }
    }
    if (n > 0) {
        SelectObject(data->render_hdc, data->hbm[0]);
        data->current_hdc = data->render_hdc;
        data->makedirty = SDL_TRUE;
    } else {
        data->current_hdc = data->window_hdc;
        data->makedirty = SDL_FALSE;
    }
    data->current_hbm = 0;

    return renderer;
}

static int
GDI_DisplayModeChanged(SDL_Renderer * renderer)
{
    GDI_RenderData *data = (GDI_RenderData *) renderer->driverdata;
    SDL_Window *window = SDL_GetWindowFromID(renderer->window);
    int i, n;

    if (renderer->info.flags & SDL_RENDERER_SINGLEBUFFER) {
        n = 0;
    } else if (renderer->info.flags & SDL_RENDERER_PRESENTFLIP2) {
        n = 2;
    } else if (renderer->info.flags & SDL_RENDERER_PRESENTFLIP3) {
        n = 3;
    } else {
        n = 1;
    }
    for (i = 0; i < n; ++i) {
        if (data->hbm[i]) {
            DeleteObject(data->hbm[i]);
            data->hbm[i] = NULL;
        }
    }
    for (i = 0; i < n; ++i) {
        data->hbm[i] =
            CreateCompatibleBitmap(data->window_hdc, window->w, window->h);
        if (!data->hbm[i]) {
            WIN_SetError("CreateCompatibleBitmap()");
            return -1;
        }
    }
    if (n > 0) {
        SelectObject(data->render_hdc, data->hbm[0]);
    }
    data->current_hbm = 0;

    return 0;
}

static int
GDI_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    GDI_RenderData *renderdata = (GDI_RenderData *) renderer->driverdata;
    SDL_Window *window = SDL_GetWindowFromID(renderer->window);
    SDL_VideoDisplay *display = SDL_GetDisplayFromWindow(window);
    GDI_TextureData *data;

    data = (GDI_TextureData *) SDL_calloc(1, sizeof(*data));
    if (!data) {
        SDL_OutOfMemory();
        return -1;
    }

    texture->driverdata = data;

    if (SDL_ISPIXELFORMAT_FOURCC(texture->format)) {
        data->yuv =
            SDL_SW_CreateYUVTexture(texture->format, texture->w, texture->h);
        if (!data->yuv) {
            return -1;
        }
        data->format = display->current_mode.format;
    } else {
        data->format = texture->format;
    }
    data->pitch = (texture->w * SDL_BYTESPERPIXEL(data->format));

    if (data->yuv || texture->access == SDL_TEXTUREACCESS_STREAMING
        || texture->format != display->current_mode.format) {
        int bmi_size;
        LPBITMAPINFO bmi;

        bmi_size = sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD);
        bmi = (LPBITMAPINFO) SDL_calloc(1, bmi_size);
        if (!bmi) {
            SDL_OutOfMemory();
            return -1;
        }
        bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi->bmiHeader.biWidth = texture->w;
        bmi->bmiHeader.biHeight = -texture->h;  /* topdown bitmap */
        bmi->bmiHeader.biPlanes = 1;
        bmi->bmiHeader.biSizeImage = texture->h * data->pitch;
        bmi->bmiHeader.biXPelsPerMeter = 0;
        bmi->bmiHeader.biYPelsPerMeter = 0;
        bmi->bmiHeader.biClrUsed = 0;
        bmi->bmiHeader.biClrImportant = 0;
        bmi->bmiHeader.biBitCount = SDL_BYTESPERPIXEL(data->format) * 8;
        if (SDL_ISPIXELFORMAT_INDEXED(data->format)) {
            int i, ncolors;
            LOGPALETTE *palette;

            bmi->bmiHeader.biCompression = BI_RGB;
            ncolors = (1 << SDL_BITSPERPIXEL(data->format));
            palette =
                (LOGPALETTE *) SDL_malloc(sizeof(*palette) +
                                          ncolors * sizeof(PALETTEENTRY));
            if (!palette) {
                SDL_free(bmi);
                SDL_OutOfMemory();
                return -1;
            }
            palette->palVersion = 0x300;
            palette->palNumEntries = ncolors;
            for (i = 0; i < ncolors; ++i) {
                palette->palPalEntry[i].peRed = 0xFF;
                palette->palPalEntry[i].peGreen = 0xFF;
                palette->palPalEntry[i].peBlue = 0xFF;
                palette->palPalEntry[i].peFlags = 0;
            }
            data->hpal = CreatePalette(palette);
            SDL_free(palette);
        } else {
            int bpp;
            Uint32 Rmask, Gmask, Bmask, Amask;

            bmi->bmiHeader.biCompression = BI_BITFIELDS;
            SDL_PixelFormatEnumToMasks(data->format, &bpp, &Rmask, &Gmask,
                                       &Bmask, &Amask);
            ((Uint32 *) bmi->bmiColors)[0] = Rmask;
            ((Uint32 *) bmi->bmiColors)[1] = Gmask;
            ((Uint32 *) bmi->bmiColors)[2] = Bmask;
            data->hpal = NULL;
        }
        data->hbm =
            CreateDIBSection(renderdata->memory_hdc, bmi, DIB_RGB_COLORS,
                             &data->pixels, NULL, 0);
    } else {
        data->hbm =
            CreateCompatibleBitmap(renderdata->window_hdc, texture->w,
                                   texture->h);
        data->pixels = NULL;
    }
    if (!data->hbm) {
        WIN_SetError("Couldn't create bitmap");
        return -1;
    }
    return 0;
}

static int
GDI_QueryTexturePixels(SDL_Renderer * renderer, SDL_Texture * texture,
                       void **pixels, int *pitch)
{
    GDI_TextureData *data = (GDI_TextureData *) texture->driverdata;

    if (data->yuv) {
        return SDL_SW_QueryYUVTexturePixels(data->yuv, pixels, pitch);
    } else {
        *pixels = data->pixels;
        *pitch = data->pitch;
        return 0;
    }
}

static int
GDI_SetTexturePalette(SDL_Renderer * renderer, SDL_Texture * texture,
                      const SDL_Color * colors, int firstcolor, int ncolors)
{
    GDI_RenderData *renderdata = (GDI_RenderData *) renderer->driverdata;
    GDI_TextureData *data = (GDI_TextureData *) texture->driverdata;

    if (data->yuv) {
        SDL_SetError("YUV textures don't have a palette");
        return -1;
    } else {
        PALETTEENTRY entries[256];
        int i;

        for (i = 0; i < ncolors; ++i) {
            entries[i].peRed = colors[i].r;
            entries[i].peGreen = colors[i].g;
            entries[i].peBlue = colors[i].b;
            entries[i].peFlags = 0;
        }
        if (!SetPaletteEntries(data->hpal, firstcolor, ncolors, entries)) {
            WIN_SetError("SetPaletteEntries()");
            return -1;
        }
        return 0;
    }
}

static int
GDI_GetTexturePalette(SDL_Renderer * renderer, SDL_Texture * texture,
                      SDL_Color * colors, int firstcolor, int ncolors)
{
    GDI_TextureData *data = (GDI_TextureData *) texture->driverdata;

    if (data->yuv) {
        SDL_SetError("YUV textures don't have a palette");
        return -1;
    } else {
        PALETTEENTRY entries[256];
        int i;

        if (!GetPaletteEntries(data->hpal, firstcolor, ncolors, entries)) {
            WIN_SetError("GetPaletteEntries()");
            return -1;
        }
        for (i = 0; i < ncolors; ++i) {
            colors[i].r = entries[i].peRed;
            colors[i].g = entries[i].peGreen;
            colors[i].b = entries[i].peBlue;
        }
        return 0;
    }
}

static int
GDI_SetTextureAlphaMod(SDL_Renderer * renderer, SDL_Texture * texture)
{
    return 0;
}

static int
GDI_SetTextureBlendMode(SDL_Renderer * renderer, SDL_Texture * texture)
{
    GDI_TextureData *data = (GDI_TextureData *) texture->driverdata;

    switch (texture->blendMode) {
    case SDL_BLENDMODE_NONE:
        if (data->premultiplied) {
            /* Crap, we've lost the original pixel data... *sigh* */
        }
        return 0;
#ifndef _WIN32_WCE              /* WinCE has no alphablend */
    case SDL_BLENDMODE_MASK:
    case SDL_BLENDMODE_BLEND:
        if (!data->premultiplied && data->pixels) {
            switch (texture->format) {
            case SDL_PIXELFORMAT_ARGB8888:
                SDL_PreMultiplyAlphaARGB8888(texture->w, texture->h,
                                             (Uint32 *) data->pixels,
                                             data->pitch);
                data->premultiplied = SDL_TRUE;
                break;
            case SDL_PIXELFORMAT_RGBA8888:
                SDL_PreMultiplyAlphaRGBA8888(texture->w, texture->h,
                                             (Uint32 *) data->pixels,
                                             data->pitch);
                data->premultiplied = SDL_TRUE;
                break;
            case SDL_PIXELFORMAT_ABGR8888:
                SDL_PreMultiplyAlphaABGR8888(texture->w, texture->h,
                                             (Uint32 *) data->pixels,
                                             data->pitch);
                data->premultiplied = SDL_TRUE;
                break;
            case SDL_PIXELFORMAT_BGRA8888:
                SDL_PreMultiplyAlphaBGRA8888(texture->w, texture->h,
                                             (Uint32 *) data->pixels,
                                             data->pitch);
                data->premultiplied = SDL_TRUE;
                break;
            }
        }
        return 0;
#endif
    default:
        SDL_Unsupported();
        texture->blendMode = SDL_BLENDMODE_NONE;
        return -1;
    }
}

static int
GDI_SetTextureScaleMode(SDL_Renderer * renderer, SDL_Texture * texture)
{
    switch (texture->scaleMode) {
    case SDL_TEXTURESCALEMODE_NONE:
    case SDL_TEXTURESCALEMODE_FAST:
        return 0;
    case SDL_TEXTURESCALEMODE_SLOW:
    case SDL_TEXTURESCALEMODE_BEST:
        SDL_Unsupported();
        texture->scaleMode = SDL_TEXTURESCALEMODE_FAST;
        return -1;
    default:
        SDL_Unsupported();
        texture->scaleMode = SDL_TEXTURESCALEMODE_NONE;
        return -1;
    }
    return 0;
}

static int
GDI_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                  const SDL_Rect * rect, const void *pixels, int pitch)
{
    GDI_TextureData *data = (GDI_TextureData *) texture->driverdata;

    if (data->yuv) {
        if (SDL_SW_UpdateYUVTexture(data->yuv, rect, pixels, pitch) < 0) {
            return -1;
        }
        UpdateYUVTextureData(texture);
        return 0;
    } else {
        GDI_RenderData *renderdata = (GDI_RenderData *) renderer->driverdata;

        if (data->pixels) {
            Uint8 *src, *dst;
            int row;
            size_t length;

            src = (Uint8 *) pixels;
            dst =
                (Uint8 *) data->pixels + rect->y * data->pitch +
                rect->x * SDL_BYTESPERPIXEL(texture->format);
            length = rect->w * SDL_BYTESPERPIXEL(texture->format);
            for (row = 0; row < rect->h; ++row) {
                SDL_memcpy(dst, src, length);
                src += pitch;
                dst += data->pitch;
            }
            if (data->premultiplied) {
                Uint32 *pixels =
                    (Uint32 *) data->pixels + rect->y * (data->pitch / 4) +
                    rect->x;
                switch (texture->format) {
                case SDL_PIXELFORMAT_ARGB8888:
                    SDL_PreMultiplyAlphaARGB8888(rect->w, rect->h, pixels,
                                                 data->pitch);
                    break;
                case SDL_PIXELFORMAT_RGBA8888:
                    SDL_PreMultiplyAlphaRGBA8888(rect->w, rect->h, pixels,
                                                 data->pitch);
                    break;
                case SDL_PIXELFORMAT_ABGR8888:
                    SDL_PreMultiplyAlphaABGR8888(rect->w, rect->h, pixels,
                                                 data->pitch);
                    break;
                case SDL_PIXELFORMAT_BGRA8888:
                    SDL_PreMultiplyAlphaBGRA8888(rect->w, rect->h, pixels,
                                                 data->pitch);
                    break;
                }
            }
        } else if (rect->w == texture->w && pitch == data->pitch) {
#ifndef NO_GETDIBBITS
            if (!SetDIBits
                (renderdata->window_hdc, data->hbm, rect->y, rect->h, pixels,
                 renderdata->bmi, DIB_RGB_COLORS)) {
                WIN_SetError("SetDIBits()");
                return -1;
            }
#else
            SDL_SetError("FIXME: Update Texture");
            return -1;
#endif
        } else {
            SDL_SetError
                ("FIXME: Need to allocate temporary memory and do GetDIBits() followed by SetDIBits(), since we can only set blocks of scanlines at a time");
            return -1;
        }
        return 0;
    }
}

static int
GDI_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                const SDL_Rect * rect, int markDirty, void **pixels,
                int *pitch)
{
    GDI_TextureData *data = (GDI_TextureData *) texture->driverdata;

    if (data->yuv) {
        return SDL_SW_LockYUVTexture(data->yuv, rect, markDirty, pixels,
                                     pitch);
    } else if (data->pixels) {
#ifndef _WIN32_WCE
        /* WinCE has no GdiFlush */
        GdiFlush();
#endif
        *pixels =
            (void *) ((Uint8 *) data->pixels + rect->y * data->pitch +
                      rect->x * SDL_BYTESPERPIXEL(texture->format));
        *pitch = data->pitch;
        return 0;
    } else {
        SDL_SetError("No pixels available");
        return -1;
    }
}

static void
GDI_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    GDI_TextureData *data = (GDI_TextureData *) texture->driverdata;

    if (data->yuv) {
        SDL_SW_UnlockYUVTexture(data->yuv);
        UpdateYUVTextureData(texture);
    }
}

static int
GDI_SetDrawBlendMode(SDL_Renderer * renderer)
{
    switch (renderer->blendMode) {
    case SDL_BLENDMODE_NONE:
        return 0;
    default:
        SDL_Unsupported();
        renderer->blendMode = SDL_BLENDMODE_NONE;
        return -1;
    }
}

static int
GDI_RenderPoint(SDL_Renderer * renderer, int x, int y)
{
    GDI_RenderData *data = (GDI_RenderData *) renderer->driverdata;

    if (data->makedirty) {
        SDL_Rect rect;

        rect.x = x;
        rect.y = y;
        rect.w = 1;
        rect.h = 1;

        SDL_AddDirtyRect(&data->dirty, &rect);
    }

    SetPixel(data->current_hdc, x, y,
             RGB(renderer->r, renderer->g, renderer->b));
    return 0;
}

static int
GDI_RenderLine(SDL_Renderer * renderer, int x1, int y1, int x2, int y2)
{
    GDI_RenderData *data = (GDI_RenderData *) renderer->driverdata;
    POINT points[2];
    HPEN pen;
    BOOL status;

    if (data->makedirty) {
        SDL_Rect rect;

        if (x1 < x2) {
            rect.x = x1;
            rect.w = (x2 - x1) + 1;
        } else {
            rect.x = x2;
            rect.w = (x1 - x2) + 1;
        }
        if (y1 < y2) {
            rect.y = y1;
            rect.h = (y2 - y1) + 1;
        } else {
            rect.y = y2;
            rect.h = (y1 - y2) + 1;
        }
        SDL_AddDirtyRect(&data->dirty, &rect);
    }

    /* Should we cache the pen? .. it looks like GDI does for us. :) */
    pen = CreatePen(PS_SOLID, 1, RGB(renderer->r, renderer->g, renderer->b));
    SelectObject(data->current_hdc, pen);
    points[0].x = x1;
    points[0].y = y1;
    points[1].x = x2;
    points[1].y = y2;
    status = Polyline(data->current_hdc, points, 2);
    DeleteObject(pen);

    if (!status) {
        WIN_SetError("FillRect()");
        return -1;
    }
    return 0;
}

static int
GDI_RenderFill(SDL_Renderer * renderer, const SDL_Rect * rect)
{
    GDI_RenderData *data = (GDI_RenderData *) renderer->driverdata;
    RECT rc;
    HBRUSH brush;
    int status;

    if (data->makedirty) {
        SDL_AddDirtyRect(&data->dirty, rect);
    }

    rc.left = rect->x;
    rc.top = rect->y;
    rc.right = rect->x + rect->w + 1;
    rc.bottom = rect->y + rect->h + 1;

    /* Should we cache the brushes? .. it looks like GDI does for us. :) */
    brush = CreateSolidBrush(RGB(renderer->r, renderer->g, renderer->b));
    SelectObject(data->current_hdc, brush);
    status = FillRect(data->current_hdc, &rc, brush);
    DeleteObject(brush);

    if (!status) {
        WIN_SetError("FillRect()");
        return -1;
    }
    return 0;
}

static int
GDI_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
               const SDL_Rect * srcrect, const SDL_Rect * dstrect)
{
    GDI_RenderData *data = (GDI_RenderData *) renderer->driverdata;
    GDI_TextureData *texturedata = (GDI_TextureData *) texture->driverdata;

    if (data->makedirty) {
        SDL_AddDirtyRect(&data->dirty, dstrect);
    }

    SelectObject(data->memory_hdc, texturedata->hbm);
    if (texturedata->hpal) {
        SelectPalette(data->memory_hdc, texturedata->hpal, TRUE);
        RealizePalette(data->memory_hdc);
    }
    if (texture->blendMode & (SDL_BLENDMODE_MASK | SDL_BLENDMODE_BLEND)) {
#ifdef _WIN32_WCE
        SDL_SetError("Texture has blendmode not supported under WinCE");
        return -1;
#else
        BLENDFUNCTION blendFunc = {
            AC_SRC_OVER,
            0,
            texture->a,
            AC_SRC_ALPHA
        };
        if (!AlphaBlend
            (data->current_hdc, dstrect->x, dstrect->y, dstrect->w,
             dstrect->h, data->memory_hdc, srcrect->x, srcrect->y, srcrect->w,
             srcrect->h, blendFunc)) {
            WIN_SetError("AlphaBlend()");
            return -1;
        }
#endif
    } else {
        if (srcrect->w == dstrect->w && srcrect->h == dstrect->h) {
            if (!BitBlt
                (data->current_hdc, dstrect->x, dstrect->y, dstrect->w,
                 srcrect->h, data->memory_hdc, srcrect->x, srcrect->y,
                 SRCCOPY)) {
                WIN_SetError("BitBlt()");
                return -1;
            }
        } else {
            if (!StretchBlt
                (data->current_hdc, dstrect->x, dstrect->y, dstrect->w,
                 dstrect->h, data->memory_hdc, srcrect->x, srcrect->y,
                 srcrect->w, srcrect->h, SRCCOPY)) {
                WIN_SetError("StretchBlt()");
                return -1;
            }
        }
    }
    return 0;
}

static void
GDI_RenderPresent(SDL_Renderer * renderer)
{
    GDI_RenderData *data = (GDI_RenderData *) renderer->driverdata;
    SDL_DirtyRect *dirty;

    /* Send the data to the display */
    if (!(renderer->info.flags & SDL_RENDERER_SINGLEBUFFER)) {
        for (dirty = data->dirty.list; dirty; dirty = dirty->next) {
            const SDL_Rect *rect = &dirty->rect;
            BitBlt(data->window_hdc, rect->x, rect->y, rect->w, rect->h,
                   data->render_hdc, rect->x, rect->y, SRCCOPY);
        }
        SDL_ClearDirtyRects(&data->dirty);
    }

    /* Update the flipping chain, if any */
    if (renderer->info.flags & SDL_RENDERER_PRESENTFLIP2) {
        data->current_hbm = (data->current_hbm + 1) % 2;
        SelectObject(data->render_hdc, data->hbm[data->current_hbm]);
    } else if (renderer->info.flags & SDL_RENDERER_PRESENTFLIP3) {
        data->current_hbm = (data->current_hbm + 1) % 3;
        SelectObject(data->render_hdc, data->hbm[data->current_hbm]);
    }
}

static void
GDI_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    GDI_TextureData *data = (GDI_TextureData *) texture->driverdata;

    if (!data) {
        return;
    }
    if (data->yuv) {
        SDL_SW_DestroyYUVTexture(data->yuv);
    }
    if (data->hpal) {
        DeleteObject(data->hpal);
    }
    if (data->hbm) {
        DeleteObject(data->hbm);
    }
    SDL_free(data);
    texture->driverdata = NULL;
}

static void
GDI_DestroyRenderer(SDL_Renderer * renderer)
{
    GDI_RenderData *data = (GDI_RenderData *) renderer->driverdata;
    int i;

    if (data) {
        DeleteDC(data->render_hdc);
        DeleteDC(data->memory_hdc);
#ifndef NO_GETDIBBITS
        if (data->bmi) {
            SDL_free(data->bmi);
        }
#endif
        for (i = 0; i < SDL_arraysize(data->hbm); ++i) {
            if (data->hbm[i]) {
                DeleteObject(data->hbm[i]);
            }
        }
        SDL_FreeDirtyRects(&data->dirty);
        SDL_free(data);
    }
    SDL_free(renderer);
}

#endif /* SDL_VIDEO_RENDER_GDI */

/* vi: set ts=4 sw=4 expandtab: */
