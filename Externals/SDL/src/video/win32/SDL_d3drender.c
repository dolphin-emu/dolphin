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

#if SDL_VIDEO_RENDER_D3D

#include "SDL_win32video.h"
#include "../SDL_yuv_sw_c.h"

/* Direct3D renderer implementation */

#if 1                           /* This takes more memory but you won't lose your texture data */
#define D3DPOOL_SDL    D3DPOOL_MANAGED
#define SDL_MEMORY_POOL_MANAGED
#else
#define D3DPOOL_SDL    D3DPOOL_DEFAULT
#define SDL_MEMORY_POOL_DEFAULT
#endif

static SDL_Renderer *D3D_CreateRenderer(SDL_Window * window, Uint32 flags);
static int D3D_DisplayModeChanged(SDL_Renderer * renderer);
static int D3D_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static int D3D_QueryTexturePixels(SDL_Renderer * renderer,
                                  SDL_Texture * texture, void **pixels,
                                  int *pitch);
static int D3D_SetTexturePalette(SDL_Renderer * renderer,
                                 SDL_Texture * texture,
                                 const SDL_Color * colors, int firstcolor,
                                 int ncolors);
static int D3D_GetTexturePalette(SDL_Renderer * renderer,
                                 SDL_Texture * texture, SDL_Color * colors,
                                 int firstcolor, int ncolors);
static int D3D_SetTextureColorMod(SDL_Renderer * renderer,
                                  SDL_Texture * texture);
static int D3D_SetTextureAlphaMod(SDL_Renderer * renderer,
                                  SDL_Texture * texture);
static int D3D_SetTextureBlendMode(SDL_Renderer * renderer,
                                   SDL_Texture * texture);
static int D3D_SetTextureScaleMode(SDL_Renderer * renderer,
                                   SDL_Texture * texture);
static int D3D_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                             const SDL_Rect * rect, const void *pixels,
                             int pitch);
static int D3D_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                           const SDL_Rect * rect, int markDirty,
                           void **pixels, int *pitch);
static void D3D_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static void D3D_DirtyTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                             int numrects, const SDL_Rect * rects);
static int D3D_RenderPoint(SDL_Renderer * renderer, int x, int y);
static int D3D_RenderLine(SDL_Renderer * renderer, int x1, int y1, int x2,
                          int y2);
static int D3D_RenderFill(SDL_Renderer * renderer, const SDL_Rect * rect);
static int D3D_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                          const SDL_Rect * srcrect, const SDL_Rect * dstrect);
static int D3D_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                                void * pixels, int pitch);
static void D3D_RenderPresent(SDL_Renderer * renderer);
static void D3D_DestroyTexture(SDL_Renderer * renderer,
                               SDL_Texture * texture);
static void D3D_DestroyRenderer(SDL_Renderer * renderer);


SDL_RenderDriver D3D_RenderDriver = {
    D3D_CreateRenderer,
    {
     "d3d",
     (SDL_RENDERER_SINGLEBUFFER | SDL_RENDERER_PRESENTCOPY |
      SDL_RENDERER_PRESENTFLIP2 | SDL_RENDERER_PRESENTFLIP3 |
      SDL_RENDERER_PRESENTDISCARD | SDL_RENDERER_PRESENTVSYNC |
      SDL_RENDERER_ACCELERATED),
     (SDL_TEXTUREMODULATE_NONE | SDL_TEXTUREMODULATE_COLOR |
      SDL_TEXTUREMODULATE_ALPHA),
     (SDL_BLENDMODE_NONE | SDL_BLENDMODE_MASK |
      SDL_BLENDMODE_BLEND | SDL_BLENDMODE_ADD | SDL_BLENDMODE_MOD),
     (SDL_TEXTURESCALEMODE_NONE | SDL_TEXTURESCALEMODE_FAST |
      SDL_TEXTURESCALEMODE_SLOW | SDL_TEXTURESCALEMODE_BEST),
     0,
     {0},
     0,
     0}
};

typedef struct
{
    IDirect3D9 *d3d;
    IDirect3DDevice9 *device;
    D3DPRESENT_PARAMETERS pparams;
    SDL_bool beginScene;
} D3D_RenderData;

typedef struct
{
    SDL_SW_YUVTexture *yuv;
    Uint32 format;
    IDirect3DTexture9 *texture;
} D3D_TextureData;

typedef struct
{
    float x, y, z;
    float rhw;
    DWORD color;
    float u, v;
} Vertex;

static void
D3D_SetError(const char *prefix, HRESULT result)
{
    const char *error;

    switch (result) {
    case D3DERR_WRONGTEXTUREFORMAT:
        error = "WRONGTEXTUREFORMAT";
        break;
    case D3DERR_UNSUPPORTEDCOLOROPERATION:
        error = "UNSUPPORTEDCOLOROPERATION";
        break;
    case D3DERR_UNSUPPORTEDCOLORARG:
        error = "UNSUPPORTEDCOLORARG";
        break;
    case D3DERR_UNSUPPORTEDALPHAOPERATION:
        error = "UNSUPPORTEDALPHAOPERATION";
        break;
    case D3DERR_UNSUPPORTEDALPHAARG:
        error = "UNSUPPORTEDALPHAARG";
        break;
    case D3DERR_TOOMANYOPERATIONS:
        error = "TOOMANYOPERATIONS";
        break;
    case D3DERR_CONFLICTINGTEXTUREFILTER:
        error = "CONFLICTINGTEXTUREFILTER";
        break;
    case D3DERR_UNSUPPORTEDFACTORVALUE:
        error = "UNSUPPORTEDFACTORVALUE";
        break;
    case D3DERR_CONFLICTINGRENDERSTATE:
        error = "CONFLICTINGRENDERSTATE";
        break;
    case D3DERR_UNSUPPORTEDTEXTUREFILTER:
        error = "UNSUPPORTEDTEXTUREFILTER";
        break;
    case D3DERR_CONFLICTINGTEXTUREPALETTE:
        error = "CONFLICTINGTEXTUREPALETTE";
        break;
    case D3DERR_DRIVERINTERNALERROR:
        error = "DRIVERINTERNALERROR";
        break;
    case D3DERR_NOTFOUND:
        error = "NOTFOUND";
        break;
    case D3DERR_MOREDATA:
        error = "MOREDATA";
        break;
    case D3DERR_DEVICELOST:
        error = "DEVICELOST";
        break;
    case D3DERR_DEVICENOTRESET:
        error = "DEVICENOTRESET";
        break;
    case D3DERR_NOTAVAILABLE:
        error = "NOTAVAILABLE";
        break;
    case D3DERR_OUTOFVIDEOMEMORY:
        error = "OUTOFVIDEOMEMORY";
        break;
    case D3DERR_INVALIDDEVICE:
        error = "INVALIDDEVICE";
        break;
    case D3DERR_INVALIDCALL:
        error = "INVALIDCALL";
        break;
    case D3DERR_DRIVERINVALIDCALL:
        error = "DRIVERINVALIDCALL";
        break;
    case D3DERR_WASSTILLDRAWING:
        error = "WASSTILLDRAWING";
        break;
    default:
        error = "UNKNOWN";
        break;
    }
    SDL_SetError("%s: %s", prefix, error);
}

static D3DFORMAT
PixelFormatToD3DFMT(Uint32 format)
{
    switch (format) {
    case SDL_PIXELFORMAT_INDEX8:
        return D3DFMT_P8;
    case SDL_PIXELFORMAT_RGB332:
        return D3DFMT_R3G3B2;
    case SDL_PIXELFORMAT_RGB444:
        return D3DFMT_X4R4G4B4;
    case SDL_PIXELFORMAT_RGB555:
        return D3DFMT_X1R5G5B5;
    case SDL_PIXELFORMAT_ARGB4444:
        return D3DFMT_A4R4G4B4;
    case SDL_PIXELFORMAT_ARGB1555:
        return D3DFMT_A1R5G5B5;
    case SDL_PIXELFORMAT_RGB565:
        return D3DFMT_R5G6B5;
    case SDL_PIXELFORMAT_RGB888:
        return D3DFMT_X8R8G8B8;
    case SDL_PIXELFORMAT_ARGB8888:
        return D3DFMT_A8R8G8B8;
    case SDL_PIXELFORMAT_ARGB2101010:
        return D3DFMT_A2R10G10B10;
    case SDL_PIXELFORMAT_UYVY:
        return D3DFMT_UYVY;
    case SDL_PIXELFORMAT_YUY2:
        return D3DFMT_YUY2;
    default:
        return D3DFMT_UNKNOWN;
    }
}

static SDL_bool
D3D_IsTextureFormatAvailable(IDirect3D9 * d3d, Uint32 display_format,
                             Uint32 texture_format)
{
    HRESULT result;

    result = IDirect3D9_CheckDeviceFormat(d3d, D3DADAPTER_DEFAULT,      /* FIXME */
                                          D3DDEVTYPE_HAL,
                                          PixelFormatToD3DFMT(display_format),
                                          0,
                                          D3DRTYPE_TEXTURE,
                                          PixelFormatToD3DFMT
                                          (texture_format));
    return FAILED(result) ? SDL_FALSE : SDL_TRUE;
}

static void
UpdateYUVTextureData(SDL_Texture * texture)
{
    D3D_TextureData *data = (D3D_TextureData *) texture->driverdata;
    SDL_Rect rect;
    RECT d3drect;
    D3DLOCKED_RECT locked;
    HRESULT result;

    d3drect.left = 0;
    d3drect.right = texture->w;
    d3drect.top = 0;
    d3drect.bottom = texture->h;

    result =
        IDirect3DTexture9_LockRect(data->texture, 0, &locked, &d3drect, 0);
    if (FAILED(result)) {
        return;
    }

    rect.x = 0;
    rect.y = 0;
    rect.w = texture->w;
    rect.h = texture->h;
    SDL_SW_CopyYUVToRGB(data->yuv, &rect, data->format, texture->w,
                        texture->h, locked.pBits, locked.Pitch);

    IDirect3DTexture9_UnlockRect(data->texture, 0);
}

void
D3D_AddRenderDriver(_THIS)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
    SDL_RendererInfo *info = &D3D_RenderDriver.info;
    SDL_DisplayMode *mode = &SDL_CurrentDisplay.desktop_mode;

    if (data->d3d) {
        int i;
        int formats[] = {
            SDL_PIXELFORMAT_INDEX8,
            SDL_PIXELFORMAT_RGB332,
            SDL_PIXELFORMAT_RGB444,
            SDL_PIXELFORMAT_RGB555,
            SDL_PIXELFORMAT_ARGB4444,
            SDL_PIXELFORMAT_ARGB1555,
            SDL_PIXELFORMAT_RGB565,
            SDL_PIXELFORMAT_RGB888,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_PIXELFORMAT_ARGB2101010,
        };

        for (i = 0; i < SDL_arraysize(formats); ++i) {
            if (D3D_IsTextureFormatAvailable
                (data->d3d, mode->format, formats[i])) {
                info->texture_formats[info->num_texture_formats++] =
                    formats[i];
            }
        }
        info->texture_formats[info->num_texture_formats++] =
            SDL_PIXELFORMAT_YV12;
        info->texture_formats[info->num_texture_formats++] =
            SDL_PIXELFORMAT_IYUV;
        info->texture_formats[info->num_texture_formats++] =
            SDL_PIXELFORMAT_YUY2;
        info->texture_formats[info->num_texture_formats++] =
            SDL_PIXELFORMAT_UYVY;
        info->texture_formats[info->num_texture_formats++] =
            SDL_PIXELFORMAT_YVYU;

        SDL_AddRenderDriver(0, &D3D_RenderDriver);
    }
}

SDL_Renderer *
D3D_CreateRenderer(SDL_Window * window, Uint32 flags)
{
    SDL_VideoDisplay *display = SDL_GetDisplayFromWindow(window);
    SDL_VideoData *videodata = (SDL_VideoData *) display->device->driverdata;
    SDL_WindowData *windowdata = (SDL_WindowData *) window->driverdata;
    SDL_Renderer *renderer;
    D3D_RenderData *data;
    HRESULT result;
    D3DPRESENT_PARAMETERS pparams;
    IDirect3DSwapChain9 *chain;
    D3DCAPS9 caps;

    renderer = (SDL_Renderer *) SDL_calloc(1, sizeof(*renderer));
    if (!renderer) {
        SDL_OutOfMemory();
        return NULL;
    }

    data = (D3D_RenderData *) SDL_calloc(1, sizeof(*data));
    if (!data) {
        D3D_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return NULL;
    }
    data->d3d = videodata->d3d;

    renderer->DisplayModeChanged = D3D_DisplayModeChanged;
    renderer->CreateTexture = D3D_CreateTexture;
    renderer->QueryTexturePixels = D3D_QueryTexturePixels;
    renderer->SetTexturePalette = D3D_SetTexturePalette;
    renderer->GetTexturePalette = D3D_GetTexturePalette;
    renderer->SetTextureColorMod = D3D_SetTextureColorMod;
    renderer->SetTextureAlphaMod = D3D_SetTextureAlphaMod;
    renderer->SetTextureBlendMode = D3D_SetTextureBlendMode;
    renderer->SetTextureScaleMode = D3D_SetTextureScaleMode;
    renderer->UpdateTexture = D3D_UpdateTexture;
    renderer->LockTexture = D3D_LockTexture;
    renderer->UnlockTexture = D3D_UnlockTexture;
    renderer->DirtyTexture = D3D_DirtyTexture;
    renderer->RenderPoint = D3D_RenderPoint;
    renderer->RenderLine = D3D_RenderLine;
    renderer->RenderFill = D3D_RenderFill;
    renderer->RenderCopy = D3D_RenderCopy;
    renderer->RenderReadPixels = D3D_RenderReadPixels;
    renderer->RenderPresent = D3D_RenderPresent;
    renderer->DestroyTexture = D3D_DestroyTexture;
    renderer->DestroyRenderer = D3D_DestroyRenderer;
    renderer->info = D3D_RenderDriver.info;
    renderer->window = window->id;
    renderer->driverdata = data;

    renderer->info.flags = SDL_RENDERER_ACCELERATED;

    SDL_zero(pparams);
    pparams.BackBufferWidth = window->w;
    pparams.BackBufferHeight = window->h;
    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        pparams.BackBufferFormat =
            PixelFormatToD3DFMT(display->fullscreen_mode.format);
    } else {
        pparams.BackBufferFormat = D3DFMT_UNKNOWN;
    }
    if (flags & SDL_RENDERER_PRESENTFLIP2) {
        pparams.BackBufferCount = 2;
        pparams.SwapEffect = D3DSWAPEFFECT_FLIP;
    } else if (flags & SDL_RENDERER_PRESENTFLIP3) {
        pparams.BackBufferCount = 3;
        pparams.SwapEffect = D3DSWAPEFFECT_FLIP;
    } else if (flags & SDL_RENDERER_PRESENTCOPY) {
        pparams.BackBufferCount = 1;
        pparams.SwapEffect = D3DSWAPEFFECT_COPY;
    } else {
        pparams.BackBufferCount = 1;
        pparams.SwapEffect = D3DSWAPEFFECT_DISCARD;
    }
    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        pparams.Windowed = FALSE;
        pparams.FullScreen_RefreshRateInHz =
            display->fullscreen_mode.refresh_rate;
    } else {
        pparams.Windowed = TRUE;
        pparams.FullScreen_RefreshRateInHz = 0;
    }
    if (flags & SDL_RENDERER_PRESENTVSYNC) {
        pparams.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    } else {
        pparams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    }

    IDirect3D9_GetDeviceCaps(videodata->d3d, D3DADAPTER_DEFAULT,
                             D3DDEVTYPE_HAL, &caps);

    result = IDirect3D9_CreateDevice(videodata->d3d, D3DADAPTER_DEFAULT,        /* FIXME */
                                     D3DDEVTYPE_HAL,
                                     windowdata->hwnd,
                                     (caps.
                                      DevCaps &
                                      D3DDEVCAPS_HWTRANSFORMANDLIGHT) ?
                                     D3DCREATE_HARDWARE_VERTEXPROCESSING :
                                     D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                     &pparams, &data->device);
    if (FAILED(result)) {
        D3D_DestroyRenderer(renderer);
        D3D_SetError("CreateDevice()", result);
        return NULL;
    }
    data->beginScene = SDL_TRUE;

    /* Get presentation parameters to fill info */
    result = IDirect3DDevice9_GetSwapChain(data->device, 0, &chain);
    if (FAILED(result)) {
        D3D_DestroyRenderer(renderer);
        D3D_SetError("GetSwapChain()", result);
        return NULL;
    }
    result = IDirect3DSwapChain9_GetPresentParameters(chain, &pparams);
    if (FAILED(result)) {
        IDirect3DSwapChain9_Release(chain);
        D3D_DestroyRenderer(renderer);
        D3D_SetError("GetPresentParameters()", result);
        return NULL;
    }
    IDirect3DSwapChain9_Release(chain);
    switch (pparams.SwapEffect) {
    case D3DSWAPEFFECT_COPY:
        renderer->info.flags |= SDL_RENDERER_PRESENTCOPY;
        break;
    case D3DSWAPEFFECT_FLIP:
        switch (pparams.BackBufferCount) {
        case 2:
            renderer->info.flags |= SDL_RENDERER_PRESENTFLIP2;
            break;
        case 3:
            renderer->info.flags |= SDL_RENDERER_PRESENTFLIP3;
            break;
        }
        break;
    case D3DSWAPEFFECT_DISCARD:
        renderer->info.flags |= SDL_RENDERER_PRESENTDISCARD;
        break;
    }
    if (pparams.PresentationInterval == D3DPRESENT_INTERVAL_ONE) {
        renderer->info.flags |= SDL_RENDERER_PRESENTVSYNC;
    }
    data->pparams = pparams;

    IDirect3DDevice9_GetDeviceCaps(data->device, &caps);
    renderer->info.max_texture_width = caps.MaxTextureWidth;
    renderer->info.max_texture_height = caps.MaxTextureHeight;

    /* Set up parameters for rendering */
    IDirect3DDevice9_SetVertexShader(data->device, NULL);
    IDirect3DDevice9_SetFVF(data->device,
                            D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    IDirect3DDevice9_SetRenderState(data->device, D3DRS_ZENABLE, D3DZB_FALSE);
    IDirect3DDevice9_SetRenderState(data->device, D3DRS_CULLMODE,
                                    D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(data->device, D3DRS_LIGHTING, FALSE);
    /* Enable color modulation by diffuse color */
    IDirect3DDevice9_SetTextureStageState(data->device, 0, D3DTSS_COLOROP,
                                          D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(data->device, 0, D3DTSS_COLORARG1,
                                          D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(data->device, 0, D3DTSS_COLORARG2,
                                          D3DTA_DIFFUSE);
    /* Enable alpha modulation by diffuse alpha */
    IDirect3DDevice9_SetTextureStageState(data->device, 0, D3DTSS_ALPHAOP,
                                          D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(data->device, 0, D3DTSS_ALPHAARG1,
                                          D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(data->device, 0, D3DTSS_ALPHAARG2,
                                          D3DTA_DIFFUSE);
    /* Disable second texture stage, since we're done */
    IDirect3DDevice9_SetTextureStageState(data->device, 1, D3DTSS_COLOROP,
                                          D3DTOP_DISABLE);
    IDirect3DDevice9_SetTextureStageState(data->device, 1, D3DTSS_ALPHAOP,
                                          D3DTOP_DISABLE);

    return renderer;
}

static int
D3D_Reset(SDL_Renderer * renderer)
{
    D3D_RenderData *data = (D3D_RenderData *) renderer->driverdata;
    HRESULT result;

    result = IDirect3DDevice9_Reset(data->device, &data->pparams);
    if (FAILED(result)) {
        if (result == D3DERR_DEVICELOST) {
            /* Don't worry about it, we'll reset later... */
            return 0;
        } else {
            D3D_SetError("Reset()", result);
            return -1;
        }
    }
    IDirect3DDevice9_SetVertexShader(data->device, NULL);
    IDirect3DDevice9_SetFVF(data->device,
                            D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    IDirect3DDevice9_SetRenderState(data->device, D3DRS_CULLMODE,
                                    D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(data->device, D3DRS_LIGHTING, FALSE);
    return 0;
}

static int
D3D_DisplayModeChanged(SDL_Renderer * renderer)
{
    D3D_RenderData *data = (D3D_RenderData *) renderer->driverdata;
    SDL_Window *window = SDL_GetWindowFromID(renderer->window);
    SDL_VideoDisplay *display = SDL_GetDisplayFromWindow(window);

    data->pparams.BackBufferWidth = window->w;
    data->pparams.BackBufferHeight = window->h;
    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        data->pparams.BackBufferFormat =
            PixelFormatToD3DFMT(display->fullscreen_mode.format);
    } else {
        data->pparams.BackBufferFormat = D3DFMT_UNKNOWN;
    }
    return D3D_Reset(renderer);
}

static int
D3D_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    D3D_RenderData *renderdata = (D3D_RenderData *) renderer->driverdata;
    SDL_Window *window = SDL_GetWindowFromID(renderer->window);
    SDL_VideoDisplay *display = SDL_GetDisplayFromWindow(window);
    Uint32 display_format = display->current_mode.format;
    D3D_TextureData *data;
    HRESULT result;

    data = (D3D_TextureData *) SDL_calloc(1, sizeof(*data));
    if (!data) {
        SDL_OutOfMemory();
        return -1;
    }

    texture->driverdata = data;

    if (SDL_ISPIXELFORMAT_FOURCC(texture->format) &&
        (texture->format != SDL_PIXELFORMAT_YUY2 ||
         !D3D_IsTextureFormatAvailable(renderdata->d3d, display_format,
                                       texture->format))
        && (texture->format != SDL_PIXELFORMAT_YVYU
            || !D3D_IsTextureFormatAvailable(renderdata->d3d, display_format,
                                             texture->format))) {
        data->yuv =
            SDL_SW_CreateYUVTexture(texture->format, texture->w, texture->h);
        if (!data->yuv) {
            return -1;
        }
        data->format = display->current_mode.format;
    } else {
        data->format = texture->format;
    }

    result =
        IDirect3DDevice9_CreateTexture(renderdata->device, texture->w,
                                       texture->h, 1, 0,
                                       PixelFormatToD3DFMT(data->format),
                                       D3DPOOL_SDL, &data->texture, NULL);
    if (FAILED(result)) {
        D3D_SetError("CreateTexture()", result);
        return -1;
    }

    return 0;
}

static int
D3D_QueryTexturePixels(SDL_Renderer * renderer, SDL_Texture * texture,
                       void **pixels, int *pitch)
{
    D3D_TextureData *data = (D3D_TextureData *) texture->driverdata;

    if (data->yuv) {
        return SDL_SW_QueryYUVTexturePixels(data->yuv, pixels, pitch);
    } else {
        /* D3D textures don't have their pixels hanging out */
        return -1;
    }
}

static int
D3D_SetTexturePalette(SDL_Renderer * renderer, SDL_Texture * texture,
                      const SDL_Color * colors, int firstcolor, int ncolors)
{
    D3D_RenderData *renderdata = (D3D_RenderData *) renderer->driverdata;
    D3D_TextureData *data = (D3D_TextureData *) texture->driverdata;

    return 0;
}

static int
D3D_GetTexturePalette(SDL_Renderer * renderer, SDL_Texture * texture,
                      SDL_Color * colors, int firstcolor, int ncolors)
{
    D3D_TextureData *data = (D3D_TextureData *) texture->driverdata;

    return 0;
}

static int
D3D_SetTextureColorMod(SDL_Renderer * renderer, SDL_Texture * texture)
{
    return 0;
}

static int
D3D_SetTextureAlphaMod(SDL_Renderer * renderer, SDL_Texture * texture)
{
    return 0;
}

static int
D3D_SetTextureBlendMode(SDL_Renderer * renderer, SDL_Texture * texture)
{
    switch (texture->blendMode) {
    case SDL_BLENDMODE_NONE:
    case SDL_BLENDMODE_MASK:
    case SDL_BLENDMODE_BLEND:
    case SDL_BLENDMODE_ADD:
    case SDL_BLENDMODE_MOD:
        return 0;
    default:
        SDL_Unsupported();
        texture->blendMode = SDL_BLENDMODE_NONE;
        return -1;
    }
}

static int
D3D_SetTextureScaleMode(SDL_Renderer * renderer, SDL_Texture * texture)
{
    switch (texture->scaleMode) {
    case SDL_TEXTURESCALEMODE_NONE:
    case SDL_TEXTURESCALEMODE_FAST:
    case SDL_TEXTURESCALEMODE_SLOW:
    case SDL_TEXTURESCALEMODE_BEST:
        return 0;
    default:
        SDL_Unsupported();
        texture->scaleMode = SDL_TEXTURESCALEMODE_NONE;
        return -1;
    }
    return 0;
}

static int
D3D_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                  const SDL_Rect * rect, const void *pixels, int pitch)
{
    D3D_TextureData *data = (D3D_TextureData *) texture->driverdata;
    D3D_RenderData *renderdata = (D3D_RenderData *) renderer->driverdata;

    if (data->yuv) {
        if (SDL_SW_UpdateYUVTexture(data->yuv, rect, pixels, pitch) < 0) {
            return -1;
        }
        UpdateYUVTextureData(texture);
        return 0;
    } else {
#ifdef SDL_MEMORY_POOL_DEFAULT
        IDirect3DTexture9 *temp;
        RECT d3drect;
        D3DLOCKED_RECT locked;
        const Uint8 *src;
        Uint8 *dst;
        int row, length;
        HRESULT result;

        result =
            IDirect3DDevice9_CreateTexture(renderdata->device, texture->w,
                                           texture->h, 1, 0,
                                           PixelFormatToD3DFMT(texture->
                                                               format),
                                           D3DPOOL_SYSTEMMEM, &temp, NULL);
        if (FAILED(result)) {
            D3D_SetError("CreateTexture()", result);
            return -1;
        }

        d3drect.left = rect->x;
        d3drect.right = rect->x + rect->w;
        d3drect.top = rect->y;
        d3drect.bottom = rect->y + rect->h;

        result = IDirect3DTexture9_LockRect(temp, 0, &locked, &d3drect, 0);
        if (FAILED(result)) {
            IDirect3DTexture9_Release(temp);
            D3D_SetError("LockRect()", result);
            return -1;
        }

        src = pixels;
        dst = locked.pBits;
        length = rect->w * SDL_BYTESPERPIXEL(texture->format);
        for (row = 0; row < rect->h; ++row) {
            SDL_memcpy(dst, src, length);
            src += pitch;
            dst += locked.Pitch;
        }
        IDirect3DTexture9_UnlockRect(temp, 0);

        result =
            IDirect3DDevice9_UpdateTexture(renderdata->device,
                                           (IDirect3DBaseTexture9 *) temp,
                                           (IDirect3DBaseTexture9 *)
                                           data->texture);
        IDirect3DTexture9_Release(temp);
        if (FAILED(result)) {
            D3D_SetError("UpdateTexture()", result);
            return -1;
        }
#else
        RECT d3drect;
        D3DLOCKED_RECT locked;
        const Uint8 *src;
        Uint8 *dst;
        int row, length;
        HRESULT result;

        d3drect.left = rect->x;
        d3drect.right = rect->x + rect->w;
        d3drect.top = rect->y;
        d3drect.bottom = rect->y + rect->h;

        result =
            IDirect3DTexture9_LockRect(data->texture, 0, &locked, &d3drect,
                                       0);
        if (FAILED(result)) {
            D3D_SetError("LockRect()", result);
            return -1;
        }

        src = pixels;
        dst = locked.pBits;
        length = rect->w * SDL_BYTESPERPIXEL(texture->format);
        for (row = 0; row < rect->h; ++row) {
            SDL_memcpy(dst, src, length);
            src += pitch;
            dst += locked.Pitch;
        }
        IDirect3DTexture9_UnlockRect(data->texture, 0);
#endif // SDL_MEMORY_POOL_DEFAULT

        return 0;
    }
}

static int
D3D_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                const SDL_Rect * rect, int markDirty, void **pixels,
                int *pitch)
{
    D3D_TextureData *data = (D3D_TextureData *) texture->driverdata;

    if (data->yuv) {
        return SDL_SW_LockYUVTexture(data->yuv, rect, markDirty, pixels,
                                     pitch);
    } else {
        RECT d3drect;
        D3DLOCKED_RECT locked;
        HRESULT result;

        d3drect.left = rect->x;
        d3drect.right = rect->x + rect->w;
        d3drect.top = rect->y;
        d3drect.bottom = rect->y + rect->h;

        result =
            IDirect3DTexture9_LockRect(data->texture, 0, &locked, &d3drect,
                                       markDirty ? 0 :
                                       D3DLOCK_NO_DIRTY_UPDATE);
        if (FAILED(result)) {
            D3D_SetError("LockRect()", result);
            return -1;
        }
        *pixels = locked.pBits;
        *pitch = locked.Pitch;
        return 0;
    }
}

static void
D3D_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    D3D_TextureData *data = (D3D_TextureData *) texture->driverdata;

    if (data->yuv) {
        SDL_SW_UnlockYUVTexture(data->yuv);
        UpdateYUVTextureData(texture);
    } else {
        IDirect3DTexture9_UnlockRect(data->texture, 0);
    }
}

static void
D3D_DirtyTexture(SDL_Renderer * renderer, SDL_Texture * texture, int numrects,
                 const SDL_Rect * rects)
{
    D3D_TextureData *data = (D3D_TextureData *) texture->driverdata;
    RECT d3drect;
    int i;

    for (i = 0; i < numrects; ++i) {
        const SDL_Rect *rect = &rects[i];

        d3drect.left = rect->x;
        d3drect.right = rect->x + rect->w;
        d3drect.top = rect->y;
        d3drect.bottom = rect->y + rect->h;

        IDirect3DTexture9_AddDirtyRect(data->texture, &d3drect);
    }
}

static void
D3D_SetBlendMode(D3D_RenderData * data, int blendMode)
{
    switch (blendMode) {
    case SDL_BLENDMODE_NONE:
        IDirect3DDevice9_SetRenderState(data->device, D3DRS_ALPHABLENDENABLE,
                                        FALSE);
        break;
    case SDL_BLENDMODE_MASK:
    case SDL_BLENDMODE_BLEND:
        IDirect3DDevice9_SetRenderState(data->device, D3DRS_ALPHABLENDENABLE,
                                        TRUE);
        IDirect3DDevice9_SetRenderState(data->device, D3DRS_SRCBLEND,
                                        D3DBLEND_SRCALPHA);
        IDirect3DDevice9_SetRenderState(data->device, D3DRS_DESTBLEND,
                                        D3DBLEND_INVSRCALPHA);
        break;
    case SDL_BLENDMODE_ADD:
        IDirect3DDevice9_SetRenderState(data->device, D3DRS_ALPHABLENDENABLE,
                                        TRUE);
        IDirect3DDevice9_SetRenderState(data->device, D3DRS_SRCBLEND,
                                        D3DBLEND_SRCALPHA);
        IDirect3DDevice9_SetRenderState(data->device, D3DRS_DESTBLEND,
                                        D3DBLEND_ONE);
        break;
    case SDL_BLENDMODE_MOD:
        IDirect3DDevice9_SetRenderState(data->device, D3DRS_ALPHABLENDENABLE,
                                        TRUE);
        IDirect3DDevice9_SetRenderState(data->device, D3DRS_SRCBLEND,
                                        D3DBLEND_ZERO);
        IDirect3DDevice9_SetRenderState(data->device, D3DRS_DESTBLEND,
                                        D3DBLEND_SRCCOLOR);
        break;
    }
}

static int
D3D_RenderPoint(SDL_Renderer * renderer, int x, int y)
{
    D3D_RenderData *data = (D3D_RenderData *) renderer->driverdata;
    DWORD color;
    Vertex vertices[1];
    HRESULT result;

    if (data->beginScene) {
        IDirect3DDevice9_BeginScene(data->device);
        data->beginScene = SDL_FALSE;
    }

    color = D3DCOLOR_ARGB(renderer->a, renderer->r, renderer->g, renderer->b);

    vertices[0].x = (float) x;
    vertices[0].y = (float) y;
    vertices[0].z = 0.0f;
    vertices[0].rhw = 1.0f;
    vertices[0].color = color;
    vertices[0].u = 0.0f;
    vertices[0].v = 0.0f;

    D3D_SetBlendMode(data, renderer->blendMode);

    result =
        IDirect3DDevice9_SetTexture(data->device, 0,
                                    (IDirect3DBaseTexture9 *) 0);
    if (FAILED(result)) {
        D3D_SetError("SetTexture()", result);
        return -1;
    }
    result =
        IDirect3DDevice9_DrawPrimitiveUP(data->device, D3DPT_POINTLIST, 1,
                                         vertices, sizeof(*vertices));
    if (FAILED(result)) {
        D3D_SetError("DrawPrimitiveUP()", result);
        return -1;
    }
    return 0;
}

static int
D3D_RenderLine(SDL_Renderer * renderer, int x1, int y1, int x2, int y2)
{
    D3D_RenderData *data = (D3D_RenderData *) renderer->driverdata;
    DWORD color;
    Vertex vertices[2];
    HRESULT result;

    if (data->beginScene) {
        IDirect3DDevice9_BeginScene(data->device);
        data->beginScene = SDL_FALSE;
    }

    color = D3DCOLOR_ARGB(renderer->a, renderer->r, renderer->g, renderer->b);

    vertices[0].x = (float) x1;
    vertices[0].y = (float) y1;
    vertices[0].z = 0.0f;
    vertices[0].rhw = 1.0f;
    vertices[0].color = color;
    vertices[0].u = 0.0f;
    vertices[0].v = 0.0f;

    vertices[1].x = (float) x2;
    vertices[1].y = (float) y2;
    vertices[1].z = 0.0f;
    vertices[1].rhw = 1.0f;
    vertices[1].color = color;
    vertices[1].u = 0.0f;
    vertices[1].v = 0.0f;

    D3D_SetBlendMode(data, renderer->blendMode);

    result =
        IDirect3DDevice9_SetTexture(data->device, 0,
                                    (IDirect3DBaseTexture9 *) 0);
    if (FAILED(result)) {
        D3D_SetError("SetTexture()", result);
        return -1;
    }
    result =
        IDirect3DDevice9_DrawPrimitiveUP(data->device, D3DPT_LINELIST, 1,
                                         vertices, sizeof(*vertices));
    if (FAILED(result)) {
        D3D_SetError("DrawPrimitiveUP()", result);
        return -1;
    }
    return 0;
}

static int
D3D_RenderFill(SDL_Renderer * renderer, const SDL_Rect * rect)
{
    D3D_RenderData *data = (D3D_RenderData *) renderer->driverdata;
    float minx, miny, maxx, maxy;
    DWORD color;
    Vertex vertices[4];
    HRESULT result;

    if (data->beginScene) {
        IDirect3DDevice9_BeginScene(data->device);
        data->beginScene = SDL_FALSE;
    }

    minx = (float) rect->x;
    miny = (float) rect->y;
    maxx = (float) rect->x + rect->w;
    maxy = (float) rect->y + rect->h;

    color = D3DCOLOR_ARGB(renderer->a, renderer->r, renderer->g, renderer->b);

    vertices[0].x = minx;
    vertices[0].y = miny;
    vertices[0].z = 0.0f;
    vertices[0].rhw = 1.0f;
    vertices[0].color = color;
    vertices[0].u = 0.0f;
    vertices[0].v = 0.0f;

    vertices[1].x = maxx;
    vertices[1].y = miny;
    vertices[1].z = 0.0f;
    vertices[1].rhw = 1.0f;
    vertices[1].color = color;
    vertices[1].u = 0.0f;
    vertices[1].v = 0.0f;

    vertices[2].x = maxx;
    vertices[2].y = maxy;
    vertices[2].z = 0.0f;
    vertices[2].rhw = 1.0f;
    vertices[2].color = color;
    vertices[2].u = 0.0f;
    vertices[2].v = 0.0f;

    vertices[3].x = minx;
    vertices[3].y = maxy;
    vertices[3].z = 0.0f;
    vertices[3].rhw = 1.0f;
    vertices[3].color = color;
    vertices[3].u = 0.0f;
    vertices[3].v = 0.0f;

    D3D_SetBlendMode(data, renderer->blendMode);

    result =
        IDirect3DDevice9_SetTexture(data->device, 0,
                                    (IDirect3DBaseTexture9 *) 0);
    if (FAILED(result)) {
        D3D_SetError("SetTexture()", result);
        return -1;
    }
    result =
        IDirect3DDevice9_DrawPrimitiveUP(data->device, D3DPT_TRIANGLEFAN, 2,
                                         vertices, sizeof(*vertices));
    if (FAILED(result)) {
        D3D_SetError("DrawPrimitiveUP()", result);
        return -1;
    }
    return 0;
}

static int
D3D_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
               const SDL_Rect * srcrect, const SDL_Rect * dstrect)
{
    D3D_RenderData *data = (D3D_RenderData *) renderer->driverdata;
    D3D_TextureData *texturedata = (D3D_TextureData *) texture->driverdata;
    float minx, miny, maxx, maxy;
    float minu, maxu, minv, maxv;
    DWORD color;
    Vertex vertices[4];
    HRESULT result;

    if (data->beginScene) {
        IDirect3DDevice9_BeginScene(data->device);
        data->beginScene = SDL_FALSE;
    }

    minx = (float) dstrect->x - 0.5f;
    miny = (float) dstrect->y - 0.5f;
    maxx = (float) dstrect->x + dstrect->w - 0.5f;
    maxy = (float) dstrect->y + dstrect->h - 0.5f;

    minu = (float) srcrect->x / texture->w;
    maxu = (float) (srcrect->x + srcrect->w) / texture->w;
    minv = (float) srcrect->y / texture->h;
    maxv = (float) (srcrect->y + srcrect->h) / texture->h;

    color = D3DCOLOR_ARGB(texture->a, texture->r, texture->g, texture->b);

    vertices[0].x = minx;
    vertices[0].y = miny;
    vertices[0].z = 0.0f;
    vertices[0].rhw = 1.0f;
    vertices[0].color = color;
    vertices[0].u = minu;
    vertices[0].v = minv;

    vertices[1].x = maxx;
    vertices[1].y = miny;
    vertices[1].z = 0.0f;
    vertices[1].rhw = 1.0f;
    vertices[1].color = color;
    vertices[1].u = maxu;
    vertices[1].v = minv;

    vertices[2].x = maxx;
    vertices[2].y = maxy;
    vertices[2].z = 0.0f;
    vertices[2].rhw = 1.0f;
    vertices[2].color = color;
    vertices[2].u = maxu;
    vertices[2].v = maxv;

    vertices[3].x = minx;
    vertices[3].y = maxy;
    vertices[3].z = 0.0f;
    vertices[3].rhw = 1.0f;
    vertices[3].color = color;
    vertices[3].u = minu;
    vertices[3].v = maxv;

    D3D_SetBlendMode(data, texture->blendMode);

    switch (texture->scaleMode) {
    case SDL_TEXTURESCALEMODE_NONE:
    case SDL_TEXTURESCALEMODE_FAST:
        IDirect3DDevice9_SetSamplerState(data->device, 0, D3DSAMP_MINFILTER,
                                         D3DTEXF_POINT);
        IDirect3DDevice9_SetSamplerState(data->device, 0, D3DSAMP_MAGFILTER,
                                         D3DTEXF_POINT);
        break;
    case SDL_TEXTURESCALEMODE_SLOW:
        IDirect3DDevice9_SetSamplerState(data->device, 0, D3DSAMP_MINFILTER,
                                         D3DTEXF_LINEAR);
        IDirect3DDevice9_SetSamplerState(data->device, 0, D3DSAMP_MAGFILTER,
                                         D3DTEXF_LINEAR);
        break;
    case SDL_TEXTURESCALEMODE_BEST:
        IDirect3DDevice9_SetSamplerState(data->device, 0, D3DSAMP_MINFILTER,
                                         D3DTEXF_GAUSSIANQUAD);
        IDirect3DDevice9_SetSamplerState(data->device, 0, D3DSAMP_MAGFILTER,
                                         D3DTEXF_GAUSSIANQUAD);
        break;
    }

    result =
        IDirect3DDevice9_SetTexture(data->device, 0, (IDirect3DBaseTexture9 *)
                                    texturedata->texture);
    if (FAILED(result)) {
        D3D_SetError("SetTexture()", result);
        return -1;
    }
    result =
        IDirect3DDevice9_DrawPrimitiveUP(data->device, D3DPT_TRIANGLEFAN, 2,
                                         vertices, sizeof(*vertices));
    if (FAILED(result)) {
        D3D_SetError("DrawPrimitiveUP()", result);
        return -1;
    }
    return 0;
}

static int
D3D_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                     void * pixels, int pitch)
{
    BYTE * pBytes;
    D3DLOCKED_RECT lockedRect;
    BYTE b, g, r, a;
    unsigned long index;
    int cur_mouse;
    int x, y;

    LPDIRECT3DSURFACE9 backBuffer;
    LPDIRECT3DSURFACE9 pickOffscreenSurface;
    D3DSURFACE_DESC desc;

    D3D_RenderData * data = (D3D_RenderData *) renderer->driverdata;
    
    IDirect3DDevice9_GetBackBuffer(data->device, 0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
    
    
    IDirect3DSurface9_GetDesc(backBuffer, &desc);

    IDirect3DDevice9_CreateOffscreenPlainSurface(data->device, desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &pickOffscreenSurface, NULL);

    IDirect3DDevice9_GetRenderTargetData(data->device, backBuffer, pickOffscreenSurface);

    IDirect3DSurface9_LockRect(pickOffscreenSurface, &lockedRect, NULL, D3DLOCK_READONLY);
    pBytes = (BYTE*)lockedRect.pBits;
    IDirect3DSurface9_UnlockRect(pickOffscreenSurface);

    // just to debug -->
    cur_mouse = SDL_SelectMouse(-1);
    SDL_GetMouseState(cur_mouse, &x, &y);
    index = (x * 4 + (y * lockedRect.Pitch));

    b = pBytes[index];
    g = pBytes[index+1];
    r = pBytes[index+2];
    a = pBytes[index+3];
    // <--
    
    return -1;
}

static void
D3D_RenderPresent(SDL_Renderer * renderer)
{
    D3D_RenderData *data = (D3D_RenderData *) renderer->driverdata;
    HRESULT result;

    if (!data->beginScene) {
        IDirect3DDevice9_EndScene(data->device);
        data->beginScene = SDL_TRUE;
    }

    result = IDirect3DDevice9_TestCooperativeLevel(data->device);
    if (result == D3DERR_DEVICELOST) {
        /* We'll reset later */
        return;
    }
    if (result == D3DERR_DEVICENOTRESET) {
        D3D_Reset(renderer);
    }
    result = IDirect3DDevice9_Present(data->device, NULL, NULL, NULL, NULL);
    if (FAILED(result)) {
        D3D_SetError("Present()", result);
    }
}

static void
D3D_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    D3D_TextureData *data = (D3D_TextureData *) texture->driverdata;

    if (!data) {
        return;
    }
    if (data->yuv) {
        SDL_SW_DestroyYUVTexture(data->yuv);
    }
    if (data->texture) {
        IDirect3DTexture9_Release(data->texture);
    }
    SDL_free(data);
    texture->driverdata = NULL;
}

static void
D3D_DestroyRenderer(SDL_Renderer * renderer)
{
    D3D_RenderData *data = (D3D_RenderData *) renderer->driverdata;

    if (data) {
        if (data->device) {
            IDirect3DDevice9_Release(data->device);
        }
        SDL_free(data);
    }
    SDL_free(renderer);
}

#endif /* SDL_VIDEO_RENDER_D3D */

/* vi: set ts=4 sw=4 expandtab: */
