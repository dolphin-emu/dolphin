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
    
    Stefan Klug
    klug.stefan@gmx.de
*/
#include "SDL_config.h"

#if SDL_VIDEO_RENDER_DDRAW

#include "SDL_win32video.h"
#include "../SDL_yuv_sw_c.h"

#if 0
#define DDRAW_LOG(...) printf(__VA_ARGS__)
#else
#define DDRAW_LOG(...)
#endif


/* DirectDraw renderer implementation */

static SDL_Renderer *DDRAW_CreateRenderer(SDL_Window * window, Uint32 flags);
static int DDRAW_DisplayModeChanged(SDL_Renderer * renderer);
static int DDRAW_CreateTexture(SDL_Renderer * renderer,
                               SDL_Texture * texture);
static int DDRAW_QueryTexturePixels(SDL_Renderer * renderer,
                                    SDL_Texture * texture, void **pixels,
                                    int *pitch);
static int DDRAW_SetTextureColorMod(SDL_Renderer * renderer,
                                    SDL_Texture * texture);
static int DDRAW_SetTextureAlphaMod(SDL_Renderer * renderer,
                                    SDL_Texture * texture);
static int DDRAW_SetTextureBlendMode(SDL_Renderer * renderer,
                                     SDL_Texture * texture);
static int DDRAW_SetTextureScaleMode(SDL_Renderer * renderer,
                                     SDL_Texture * texture);
static int DDRAW_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                               const SDL_Rect * rect, const void *pixels,
                               int pitch);
static int DDRAW_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                             const SDL_Rect * rect, int markDirty,
                             void **pixels, int *pitch);
static void DDRAW_UnlockTexture(SDL_Renderer * renderer,
                                SDL_Texture * texture);
static void DDRAW_DirtyTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                               int numrects, const SDL_Rect * rects);
static int DDRAW_RenderPoint(SDL_Renderer * renderer, int x, int y);
static int DDRAW_RenderLine(SDL_Renderer * renderer, int x1, int y1, int x2,
                            int y2);
static int DDRAW_RenderFill(SDL_Renderer * renderer, const SDL_Rect * rect);
static int DDRAW_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                            const SDL_Rect * srcrect,
                            const SDL_Rect * dstrect);
static void DDRAW_RenderPresent(SDL_Renderer * renderer);
static void DDRAW_DestroyTexture(SDL_Renderer * renderer,
                                 SDL_Texture * texture);
static void DDRAW_DestroyRenderer(SDL_Renderer * renderer);


SDL_RenderDriver DDRAW_RenderDriver = {
    DDRAW_CreateRenderer,
    {
     "ddraw",
     (SDL_RENDERER_SINGLEBUFFER | SDL_RENDERER_PRESENTCOPY |
      SDL_RENDERER_PRESENTFLIP2 | SDL_RENDERER_PRESENTFLIP3 |
      SDL_RENDERER_PRESENTDISCARD | SDL_RENDERER_ACCELERATED),
     (SDL_TEXTUREMODULATE_NONE),
     (SDL_BLENDMODE_NONE),
     (SDL_TEXTURESCALEMODE_NONE),
     0,
     {0},
     0,
     0}
};

typedef struct
{
    IDirectDraw *ddraw;
    IDirectDrawSurface *primary;
} DDRAW_RenderData;

typedef struct
{
    RECT lock;
    IDirectDrawSurface *surface;
} DDRAW_TextureData;


static void
DDRAW_SetError(const char *prefix, HRESULT result)
{
    const char *error;

    switch (result) {
    case DDERR_CANTCREATEDC:
        error = "CANTCREATEDC";
        break;
    case DDERR_CANTLOCKSURFACE:
        error = "CANTLOCKSURFACE";
        break;
    case DDERR_CLIPPERISUSINGHWND:
        error = "CLIPPERISUSINGHWND";
        break;
    case DDERR_COLORKEYNOTSET:
        error = "COLORKEYNOTSET";
        break;
    case DDERR_CURRENTLYNOTAVAIL:
        error = "CURRENTLYNOTAVAIL";
        break;
    case DDERR_DCALREADYCREATED:
        error = "DCALREADYCREATED";
        break;
    case DDERR_DEVICEDOESNTOWNSURFACE:
        error = "DEVICEDOESNTOWNSURFACE";
        break;
    case DDERR_DIRECTDRAWALREADYCREATED:
        error = "DIRECTDRAWALREADYCREATED";
        break;
    case DDERR_EXCLUSIVEMODEALREADYSET:
        error = "EXCLUSIVEMODEALREADYSET";
        break;
    case DDERR_GENERIC:
        error = "GENERIC";
        break;
    case DDERR_HEIGHTALIGN:
        error = "HEIGHTALIGN";
        break;
    case DDERR_IMPLICITLYCREATED:
        error = "IMPLICITLYCREATED";
        break;
    case DDERR_INCOMPATIBLEPRIMARY:
        error = "INCOMPATIBLEPRIMARY";
        break;
    case DDERR_INVALIDCAPS:
        error = "INVALIDCAPS";
        break;
    case DDERR_INVALIDCLIPLIST:
        error = "INVALIDCLIPLIST";
        break;
    case DDERR_INVALIDMODE:
        error = "INVALIDMODE";
        break;
    case DDERR_INVALIDOBJECT:
        error = "INVALIDOBJECT";
        break;
    case DDERR_INVALIDPARAMS:
        error = "INVALIDPARAMS";
        break;
    case DDERR_INVALIDPIXELFORMAT:
        error = "INVALIDPIXELFORMAT";
        break;
    case DDERR_INVALIDPOSITION:
        error = "INVALIDPOSITION";
        break;
    case DDERR_INVALIDRECT:
        error = "INVALIDRECT";
        break;
    case DDERR_LOCKEDSURFACES:
        error = "LOCKEDSURFACES";
        break;
    case DDERR_MOREDATA:
        error = "MOREDATA";
        break;
    case DDERR_NOALPHAHW:
        error = "NOALPHAHW";
        break;
    case DDERR_NOBLTHW:
        error = "NOBLTHW";
        break;
    case DDERR_NOCLIPLIST:
        error = "NOCLIPLIST";
        break;
    case DDERR_NOCLIPPERATTACHED:
        error = "NOCLIPPERATTACHED";
        break;
    case DDERR_NOCOLORCONVHW:
        error = "NOCOLORCONVHW";
        break;
    case DDERR_NOCOLORKEYHW:
        error = "NOCOLORKEYHW";
        break;
    case DDERR_NOCOOPERATIVELEVELSET:
        error = "NOCOOPERATIVELEVELSET";
        break;
    case DDERR_NODC:
        error = "NODC";
        break;
    case DDERR_NOFLIPHW:
        error = "NOFLIPHW";
        break;
    case DDERR_NOOVERLAYDEST:
        error = "NOOVERLAYDEST";
        break;
    case DDERR_NOOVERLAYHW:
        error = "NOOVERLAYHW";
        break;
    case DDERR_NOPALETTEATTACHED:
        error = "NOPALETTEATTACHED";
        break;
    case DDERR_NOPALETTEHW:
        error = "NOPALETTEHW";
        break;
    case DDERR_NORASTEROPHW:
        error = "NORASTEROPHW";
        break;
    case DDERR_NOSTRETCHHW:
        error = "NOSTRETCHHW";
        break;
    case DDERR_NOTAOVERLAYSURFACE:
        error = "NOTAOVERLAYSURFACE";
        break;
    case DDERR_NOTFLIPPABLE:
        error = "NOTFLIPPABLE";
        break;
    case DDERR_NOTFOUND:
        error = "NOTFOUND";
        break;
    case DDERR_NOTLOCKED:
        error = "NOTLOCKED";
        break;
    case DDERR_NOTPALETTIZED:
        error = "NOTPALETTIZED";
        break;
    case DDERR_NOVSYNCHW:
        error = "NOVSYNCHW";
        break;
    case DDERR_NOZOVERLAYHW:
        error = "NOZOVERLAYHW";
        break;
    case DDERR_OUTOFCAPS:
        error = "OUTOFCAPS";
        break;
    case DDERR_OUTOFMEMORY:
        error = "OUTOFMEMORY";
        break;
    case DDERR_OUTOFVIDEOMEMORY:
        error = "OUTOFVIDEOMEMORY";
        break;
    case DDERR_OVERLAPPINGRECTS:
        error = "OVERLAPPINGRECTS";
        break;
    case DDERR_OVERLAYNOTVISIBLE:
        error = "OVERLAYNOTVISIBLE";
        break;
    case DDERR_PALETTEBUSY:
        error = "PALETTEBUSY";
        break;
    case DDERR_PRIMARYSURFACEALREADYEXISTS:
        error = "PRIMARYSURFACEALREADYEXISTS";
        break;
    case DDERR_REGIONTOOSMALL:
        error = "REGIONTOOSMALL";
        break;
    case DDERR_SURFACEBUSY:
        error = "SURFACEBUSY";
        break;
    case DDERR_SURFACELOST:
        error = "SURFACELOST";
        break;
    case DDERR_TOOBIGHEIGHT:
        error = "TOOBIGHEIGHT";
        break;
    case DDERR_TOOBIGSIZE:
        error = "TOOBIGSIZE";
        break;
    case DDERR_TOOBIGWIDTH:
        error = "TOOBIGWIDTH";
        break;
    case DDERR_UNSUPPORTED:
        error = "UNSUPPORTED";
        break;
    case DDERR_UNSUPPORTEDFORMAT:
        error = "UNSUPPORTEDFORMAT";
        break;
    case DDERR_VERTICALBLANKINPROGRESS:
        error = "VERTICALBLANKINPROGRESS";
        break;
    case DDERR_VIDEONOTACTIVE:
        error = "VIDEONOTACTIVE";
        break;
    case DDERR_WASSTILLDRAWING:
        error = "WASSTILLDRAWING";
        break;
    case DDERR_WRONGMODE:
        error = "WRONGMODE";
        break;
    default:
        error = "UNKNOWN";
        break;
    }
    SDL_SetError("%s: %s", prefix, error);
}

static SDL_bool
PixelFormatToDDPIXELFORMAT(Uint32 format, LPDDPIXELFORMAT dst)
{
    SDL_zerop(dst);
    dst->dwSize = sizeof(*dst);

    if (SDL_ISPIXELFORMAT_FOURCC(format)) {
        dst->dwFlags = DDPF_FOURCC;
        dst->dwFourCC = format;
    } else if (SDL_ISPIXELFORMAT_INDEXED(format)) {
        SDL_SetError("Indexed pixelformats are not supported.");
        return SDL_FALSE;
    } else {
        int bpp;
        Uint32 Rmask, Gmask, Bmask, Amask;
        if (!SDL_PixelFormatEnumToMasks
            (format, &bpp, &Rmask, &Gmask, &Bmask, &Amask)) {
            SDL_SetError("pixelformat not supported");
            return SDL_FALSE;
        }

        if (!Rmask && !Gmask && !Bmask) {
            dst->dwFlags = DDPF_ALPHA;
            dst->dwAlphaBitDepth = bpp;
        } else {
            dst->dwFlags = DDPF_RGB;
            dst->dwRGBBitCount = bpp;
            dst->dwRBitMask = Rmask;
            dst->dwGBitMask = Gmask;
            dst->dwBBitMask = Bmask;

            if (Amask) {
                dst->dwFlags |= DDPF_ALPHAPIXELS;
                dst->dwRGBAlphaBitMask = Amask;
            }
        }
    }

    return SDL_TRUE;
}

static SDL_bool
DDRAW_IsTextureFormatAvailable(IDirectDraw * ddraw, Uint32 display_format,
                               Uint32 texture_format)
{
    int bpp;
    Uint32 Rmask, Gmask, Bmask, Amask;

    if (SDL_ISPIXELFORMAT_FOURCC(texture_format)) {
        //TODO I don't expect DDRAW to support all 4CC formats, but I don't know which ones
        return SDL_TRUE;
    }
    //These are only basic checks
    if (SDL_ISPIXELFORMAT_INDEXED(texture_format)) {
        return SDL_FALSE;
    }

    if (!SDL_PixelFormatEnumToMasks
        (texture_format, &bpp, &Rmask, &Gmask, &Bmask, &Amask)) {
        return SDL_FALSE;
    }

    switch (bpp) {
    case 4:
    case 8:
    case 16:
    case 24:
    case 32:
        break;
    default:
        return SDL_FALSE;
    }

    return SDL_TRUE;
}

void
DDRAW_AddRenderDriver(_THIS)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
    SDL_RendererInfo *info = &DDRAW_RenderDriver.info;
    SDL_DisplayMode *mode = &SDL_CurrentDisplay.desktop_mode;

    if (data->ddraw) {
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
            if (DDRAW_IsTextureFormatAvailable
                (data->ddraw, mode->format, formats[i])) {
                info->texture_formats[info->num_texture_formats++] =
                    formats[i];
            }
        }

        //TODO the fourcc formats should get fetched from IDirectDraw::GetFourCCCodes
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

        SDL_AddRenderDriver(0, &DDRAW_RenderDriver);
    }
}

SDL_Renderer *
DDRAW_CreateRenderer(SDL_Window * window, Uint32 flags)
{
    SDL_VideoDisplay *display = SDL_GetDisplayFromWindow(window);
    SDL_VideoData *videodata = (SDL_VideoData *) display->device->driverdata;
    SDL_WindowData *windowdata = (SDL_WindowData *) window->driverdata;
    SDL_Renderer *renderer;
    DDRAW_RenderData *data;
    HRESULT result;
    DDSURFACEDESC ddsd;

    renderer = (SDL_Renderer *) SDL_calloc(1, sizeof(*renderer));
    if (!renderer) {
        SDL_OutOfMemory();
        return NULL;
    }

    data = (DDRAW_RenderData *) SDL_calloc(1, sizeof(*data));
    if (!data) {
        DDRAW_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return NULL;
    }
    data->ddraw = videodata->ddraw;

    renderer->DisplayModeChanged = DDRAW_DisplayModeChanged;
    renderer->CreateTexture = DDRAW_CreateTexture;
    renderer->QueryTexturePixels = DDRAW_QueryTexturePixels;

    renderer->SetTextureColorMod = DDRAW_SetTextureColorMod;
    renderer->SetTextureAlphaMod = DDRAW_SetTextureAlphaMod;
    renderer->SetTextureBlendMode = DDRAW_SetTextureBlendMode;
    renderer->SetTextureScaleMode = DDRAW_SetTextureScaleMode;
    renderer->UpdateTexture = DDRAW_UpdateTexture;
    renderer->LockTexture = DDRAW_LockTexture;
    renderer->UnlockTexture = DDRAW_UnlockTexture;
    renderer->DirtyTexture = DDRAW_DirtyTexture;
    renderer->RenderPoint = DDRAW_RenderPoint;
    renderer->RenderLine = DDRAW_RenderLine;
    renderer->RenderFill = DDRAW_RenderFill;
    renderer->RenderCopy = DDRAW_RenderCopy;
    renderer->RenderPresent = DDRAW_RenderPresent;
    renderer->DestroyTexture = DDRAW_DestroyTexture;
    renderer->DestroyRenderer = DDRAW_DestroyRenderer;
    renderer->info = DDRAW_RenderDriver.info;
    renderer->window = window->id;
    renderer->driverdata = data;

    renderer->info.flags = SDL_RENDERER_ACCELERATED;

    SDL_zero(ddsd);
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS;

    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    } else {
        //TODO handle non fullscreen
        SDL_SetError("DirectDraw renderer has only fullscreen implemented");
        DDRAW_DestroyRenderer(renderer);
        return NULL;
    }

    if (flags & SDL_RENDERER_PRESENTFLIP2) {
        ddsd.dwFlags |= DDSD_BACKBUFFERCOUNT;
        ddsd.dwBackBufferCount = 2;
    } else if (flags & SDL_RENDERER_PRESENTFLIP3) {
        ddsd.dwFlags |= DDSD_BACKBUFFERCOUNT;
        ddsd.dwBackBufferCount = 3;
    } else if (flags & SDL_RENDERER_PRESENTCOPY) {
        //TODO what is the best approximation to this mode
    } else {

    }

    if (flags & SDL_RENDERER_PRESENTVSYNC) {
        SDL_SetError("DirectDraw renderer with v-sync is not implemented");
        DDRAW_DestroyRenderer(renderer);
        return NULL;
    }

    result =
        data->ddraw->lpVtbl->SetCooperativeLevel(data->ddraw,
                                                 windowdata->hwnd,
                                                 DDSCL_NORMAL);
    if (result != DD_OK) {
        DDRAW_SetError("CreateDevice()", result);
        DDRAW_DestroyRenderer(renderer);
        return NULL;
    }

    result =
        data->ddraw->lpVtbl->CreateSurface(data->ddraw, &ddsd, &data->primary,
                                           NULL);
    if (result != DD_OK) {
        DDRAW_SetError("CreateDevice()", result);
        DDRAW_DestroyRenderer(renderer);
        return NULL;
    }

    return renderer;
}

static int
DDRAW_Reset(SDL_Renderer * renderer)
{
    //TODO implement
    /*D3D_RenderData *data = (D3D_RenderData *) renderer->driverdata;
       HRESULT result;

       result = IDirect3DDevice9_Reset(data->device, &data->pparams);
       if (FAILED(result)) {
       if (result == D3DERR_DEVICELOST) {
       /* Don't worry about it, we'll reset later... *
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
       IDirect3DDevice9_SetRenderState(data->device, D3DRS_LIGHTING, FALSE); */
    return 0;
}

static int
DDRAW_DisplayModeChanged(SDL_Renderer * renderer)
{
    //TODO implement
    /*D3D_RenderData *data = (D3D_RenderData *) renderer->driverdata;
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
       return D3D_Reset(renderer); */
    return 0;
}

static int
DDRAW_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    DDRAW_RenderData *renderdata = (DDRAW_RenderData *) renderer->driverdata;
    SDL_Window *window = SDL_GetWindowFromID(renderer->window);
    SDL_VideoDisplay *display = SDL_GetDisplayFromWindow(window);
    Uint32 display_format = display->current_mode.format;
    DDRAW_TextureData *data;
    DDSURFACEDESC ddsd;
    HRESULT result;

    data = (DDRAW_TextureData *) SDL_calloc(1, sizeof(*data));
    if (!data) {
        SDL_OutOfMemory();
        return -1;
    }

    SDL_zero(ddsd);
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_PIXELFORMAT | DDSD_HEIGHT | DDSD_WIDTH;
    ddsd.dwWidth = texture->w;
    ddsd.dwHeight = texture->h;


    if (!PixelFormatToDDPIXELFORMAT(texture->format, &ddsd.ddpfPixelFormat)) {
        SDL_free(data);
        return -1;
    }

    texture->driverdata = data;

    result =
        renderdata->ddraw->lpVtbl->CreateSurface(renderdata->ddraw, &ddsd,
                                                 &data->surface, NULL);
    if (result != DD_OK) {
        SDL_free(data);
        DDRAW_SetError("CreateTexture", result);
        return -1;
    }

    return 0;
}

static int
DDRAW_QueryTexturePixels(SDL_Renderer * renderer, SDL_Texture * texture,
                         void **pixels, int *pitch)
{
    //TODO implement
    SDL_SetError("QueryTexturePixels is not implemented");
    return -1;
}

static int
DDRAW_SetTextureColorMod(SDL_Renderer * renderer, SDL_Texture * texture)
{
    return 0;
}

static int
DDRAW_SetTextureAlphaMod(SDL_Renderer * renderer, SDL_Texture * texture)
{
    return 0;
}

static int
DDRAW_SetTextureBlendMode(SDL_Renderer * renderer, SDL_Texture * texture)
{
    switch (texture->blendMode) {
    case SDL_BLENDMODE_NONE:
        return 0;
    default:
        SDL_Unsupported();
        texture->blendMode = SDL_BLENDMODE_NONE;
        return -1;
    }
}

static int
DDRAW_SetTextureScaleMode(SDL_Renderer * renderer, SDL_Texture * texture)
{
    switch (texture->scaleMode) {
    case SDL_TEXTURESCALEMODE_NONE:
    default:
        SDL_Unsupported();
        texture->scaleMode = SDL_TEXTURESCALEMODE_NONE;
        return -1;
    }
    return 0;
}

static int
DDRAW_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                    const SDL_Rect * rect, const void *pixels, int pitch)
{
    DDRAW_TextureData *data = (DDRAW_TextureData *) texture->driverdata;

    //TODO implement
    SDL_SetError("UpdateTexture is not implemented");
    return 0;
}

static int
DDRAW_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                  const SDL_Rect * rect, int markDirty, void **pixels,
                  int *pitch)
{
    DDRAW_TextureData *data = (DDRAW_TextureData *) texture->driverdata;
    HRESULT result;
    DDSURFACEDESC ddsd;

    SDL_zero(ddsd);
    ddsd.dwSize = sizeof(ddsd);

    /**
     * On a Axim x51v locking a subrect returns the startaddress of the whole surface,
     * wheras on my ASUS MyPal 696 the startaddress of the locked area is returned,
     * thats why I always lock the whole surface and calculate the pixels pointer by hand.
     * This shouldn't be a speed problem, as multiple locks aren't available on DDraw Mobile
     * see http://msdn.microsoft.com/en-us/library/ms858221.aspx
     */

    result = data->surface->lpVtbl->Lock(data->surface, NULL, &ddsd, 0, NULL);
    if (result != DD_OK) {
        DDRAW_SetError("LockRect()", result);
        return -1;
    }

    *pixels = ddsd.lpSurface + rect->y * ddsd.lPitch + rect->x * ddsd.lXPitch;
    *pitch = ddsd.lPitch;
    return 0;
}

static void
DDRAW_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    DDRAW_TextureData *data = (DDRAW_TextureData *) texture->driverdata;

    data->surface->lpVtbl->Unlock(data->surface, NULL);
}

static void
DDRAW_DirtyTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                   int numrects, const SDL_Rect * rects)
{
}

static void
DDRAW_SetBlendMode(DDRAW_RenderData * data, int blendMode)
{
    switch (blendMode) {

    }
}

static int
DDRAW_RenderPoint(SDL_Renderer * renderer, int x, int y)
{
    return -1;
}

static int
DDRAW_RenderLine(SDL_Renderer * renderer, int x1, int y1, int x2, int y2)
{
    return -1;
}

static int
DDRAW_RenderFill(SDL_Renderer * renderer, const SDL_Rect * rect)
{
    return -1;
}

static int
DDRAW_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                 const SDL_Rect * srcrect, const SDL_Rect * dstrect)
{
    DDRAW_RenderData *data = (DDRAW_RenderData *) renderer->driverdata;
    DDRAW_TextureData *texturedata =
        (DDRAW_TextureData *) texture->driverdata;
    HRESULT result;
    RECT srcr;
    RECT dstr;
    DDBLTFX bltfx;

    srcr.left = srcrect->x;
    srcr.top = srcrect->y;
    srcr.right = srcrect->x + srcrect->w;
    srcr.bottom = srcrect->y + srcrect->h;

    dstr.left = dstrect->x;
    dstr.top = dstrect->y;
    dstr.right = dstrect->x + dstrect->w;
    dstr.bottom = dstrect->y + dstrect->h;

    SDL_zero(bltfx);
    bltfx.dwSize = sizeof(bltfx);
    bltfx.dwROP = SRCCOPY;

    data->primary->lpVtbl->Blt(data->primary, &dstr, texturedata->surface,
                               &srcr, DDBLT_ROP, &bltfx);

    return 0;
}

static void
DDRAW_RenderPresent(SDL_Renderer * renderer)
{
    DDRAW_RenderData *data = (DDRAW_RenderData *) renderer->driverdata;
    HRESULT result;

    return;

    result =
        data->primary->lpVtbl->Flip(data->primary, NULL, DDFLIP_INTERVAL1);
    if (result != DD_OK) {
        DDRAW_SetError("Present()", result);
    }
}

static void
DDRAW_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    DDRAW_TextureData *data = (DDRAW_TextureData *) texture->driverdata;

    if (!data) {
        return;
    }

    data->surface->lpVtbl->Release(data->surface);
    SDL_free(data);
    texture->driverdata = NULL;
}

static void
DDRAW_DestroyRenderer(SDL_Renderer * renderer)
{
    DDRAW_RenderData *data = (DDRAW_RenderData *) renderer->driverdata;

    if (data) {
        data->primary->lpVtbl->Release(data->primary);
        SDL_free(data);
    }
    SDL_free(renderer);
}

#endif /* SDL_VIDEO_RENDER_DDRAW */

/* vi: set ts=4 sw=4 expandtab: */
