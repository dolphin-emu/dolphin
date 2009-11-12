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

#if SDL_VIDEO_RENDER_GAPI

#include "SDL_win32video.h"
//#include "../SDL_sysvideo.h"
#include "../SDL_yuv_sw_c.h"
#include "../SDL_renderer_sw.h"

#include "SDL_gapirender_c.h"

#define GAPI_RENDERER_DEBUG 1

/* GAPI renderer implementation */

static SDL_Renderer *GAPI_CreateRenderer(SDL_Window * window, Uint32 flags);
static int GAPI_RenderPoint(SDL_Renderer * renderer, int x, int y);
static int GAPI_RenderLine(SDL_Renderer * renderer, int x1, int y1,
                           int x2, int y2);
static int GAPI_RenderFill(SDL_Renderer * renderer, const SDL_Rect * rect);
static int GAPI_RenderCopy(SDL_Renderer * renderer,
                           SDL_Texture * texture,
                           const SDL_Rect * srcrect,
                           const SDL_Rect * dstrect);
static void GAPI_RenderPresent(SDL_Renderer * renderer);
static void GAPI_DestroyRenderer(SDL_Renderer * renderer);


SDL_RenderDriver GAPI_RenderDriver = {
    GAPI_CreateRenderer,
    {
     "gapi",
     (SDL_RENDERER_SINGLEBUFFER),
     }
};

static HMODULE g_hGapiLib = 0;

// for testing with GapiEmu
#define USE_GAPI_EMU 0
#define EMULATE_AXIM_X30 0

#if 0
#define GAPI_LOG(...) printf(__VA_ARGS__)
#else
#define GAPI_LOG(...)
#endif


#if USE_GAPI_EMU && !REPORT_VIDEO_INFO
#pragma message("Warning: Using GapiEmu in release build. I assume you'd like to set USE_GAPI_EMU to zero.")
#endif


static void
GAPI_SetError(const char *prefix, HRESULT result)
{
    const char *error;

    switch (result) {
    default:
        error = "UNKNOWN";
        break;
    }
    SDL_SetError("%s: %s", prefix, error);
}

void
GAPI_AddRenderDriver(_THIS)
{
    /* TODO: should we check for support of GetRawFramebuffer here?
     */
#if USE_GAPI_EMU
    g_hGapiLib = LoadLibrary(L"GAPI_Emu.dll");
#else
    g_hGapiLib = LoadLibrary(L"\\Windows\\gx.dll");
#endif

    if (g_hGapiLib) {
#define LINK(name,import) gx.name = (PFN##name)GetProcAddress( g_hGapiLib, L##import );

        LINK(GXOpenDisplay, "?GXOpenDisplay@@YAHPAUHWND__@@K@Z")
            LINK(GXCloseDisplay, "?GXCloseDisplay@@YAHXZ")
            LINK(GXBeginDraw, "?GXBeginDraw@@YAPAXXZ")
            LINK(GXEndDraw, "?GXEndDraw@@YAHXZ")
            LINK(GXOpenInput, "?GXOpenInput@@YAHXZ")
            LINK(GXCloseInput, "?GXCloseInput@@YAHXZ")
            LINK(GXGetDisplayProperties,
                 "?GXGetDisplayProperties@@YA?AUGXDisplayProperties@@XZ")
            LINK(GXGetDefaultKeys, "?GXGetDefaultKeys@@YA?AUGXKeyList@@H@Z")
            LINK(GXSuspend, "?GXSuspend@@YAHXZ")
            LINK(GXResume, "?GXResume@@YAHXZ")
            LINK(GXSetViewport, "?GXSetViewport@@YAHKKKK@Z")
            LINK(GXIsDisplayDRAMBuffer, "?GXIsDisplayDRAMBuffer@@YAHXZ")

            /* wrong gapi.dll */
            if (!gx.GXOpenDisplay) {
            FreeLibrary(g_hGapiLib);
            g_hGapiLib = 0;
        }
#undef LINK
    }

    SDL_AddRenderDriver(0, &GAPI_RenderDriver);
}

typedef enum
{
    USAGE_GX_FUNCS = 0x0001,    /* enable to use GXOpen/GXClose/GXBeginDraw... */
    USAGE_DATA_PTR_CONSTANT = 0x0002    /* the framebuffer is at a constant location, don't use values from GXBeginDraw() */
} GAPI_UsageFlags;



typedef struct
{
    int w;
    int h;
    int xPitch;                 /* bytes to move to go to the next pixel */
    int yPitch;                 /* bytes to move to go to the next line */
    int offset;                 /* data offset, to add to the data returned from GetFramebuffer, before processing */

    void *data;
    Uint32 usageFlags;          /* these flags contain options to define screen handling and to reliably workarounds */

    Uint32 format;              /* pixel format as defined in SDL_pixels.h */

} GAPI_RenderData;


static Uint32
GuessPixelFormatFromBpp(int bpp)
{
    switch (bpp) {
    case 15:
        return SDL_PIXELFORMAT_RGB555;
    case 16:
        return SDL_PIXELFORMAT_RGB565;
    default:
        return SDL_PIXELFORMAT_UNKNOWN;
        break;
    }
}

static GAPI_RenderData *
FillRenderDataRawFramebuffer(SDL_Window * window)
{
    RawFrameBufferInfo rbi;
    GAPI_RenderData *renderdata;
    HDC hdc;

    //TODO should we use the hdc of the window?
    hdc = GetDC(NULL);
    int result = ExtEscape(hdc, GETRAWFRAMEBUFFER, 0, NULL,
                           sizeof(RawFrameBufferInfo),
                           (char *) &rbi);
    ReleaseDC(NULL, hdc);

    if (!(result > 0)) {
        return NULL;
    }

    /* Asus A696 returns wrong results so we do a sanity check
       See:
       http://groups.google.com/group/microsoft.public.smartphone.developer/browse_thread/thread/4fde5bddd477de81
     */
    if (rbi.cxPixels <= 0 ||
        rbi.cyPixels <= 0 ||
        rbi.cxStride == 0 || rbi.cyStride == 0 || rbi.pFramePointer == 0) {
        return NULL;
    }


    renderdata = (GAPI_RenderData *) SDL_calloc(1, sizeof(*renderdata));
    if (!renderdata) {
        SDL_OutOfMemory();
        return NULL;
    }
    //Try to match the window size
    //TODO add rotation support
    if (rbi.cxPixels != window->w || rbi.cyPixels != window->h) {
        SDL_free(renderdata);
        return NULL;
    }
    //Check that the display uses a known display format
    switch (rbi.wFormat) {
    case FORMAT_565:
        renderdata->format = SDL_PIXELFORMAT_RGB565;
        break;
    case FORMAT_555:
        renderdata->format = SDL_PIXELFORMAT_RGB555;
        break;
    default:
        //TODO we should add support for other formats
        SDL_free(renderdata);
        return NULL;
    }

    renderdata->usageFlags = USAGE_DATA_PTR_CONSTANT;
    renderdata->data = rbi.pFramePointer;
    renderdata->w = rbi.cxPixels;
    renderdata->h = rbi.cyPixels;
    renderdata->xPitch = rbi.cxStride;
    renderdata->yPitch = rbi.cyStride;

    return renderdata;

}


static GAPI_RenderData *
FillRenderDataGAPI(SDL_Window * window)
{
    GAPI_RenderData *renderdata;
    struct GXDisplayProperties gxdp;
    int tmp;

#ifdef _ARM_
    WCHAR oemstr[100];
#endif

    if (!g_hGapiLib) {
        return NULL;
    }

    renderdata = (GAPI_RenderData *) SDL_calloc(1, sizeof(GAPI_RenderData));
    if (!renderdata) {
        SDL_OutOfMemory();
        return NULL;
    }

    gxdp = gx.GXGetDisplayProperties();
    renderdata->usageFlags = USAGE_GX_FUNCS;
    renderdata->w = gxdp.cxWidth;
    renderdata->h = gxdp.cyHeight;
    renderdata->xPitch = gxdp.cbxPitch;
    renderdata->yPitch = gxdp.cbyPitch;

    //Check that the display uses a known display format
    if (gxdp.ffFormat & kfDirect565) {
        renderdata->format = SDL_PIXELFORMAT_RGB565;
    } else if (gxdp.ffFormat & kfDirect555) {
        renderdata->format = SDL_PIXELFORMAT_RGB555;
    } else {
        renderdata->format = SDL_PIXELFORMAT_UNKNOWN;
    }

    /* apply some device specific corrections */
#ifdef _ARM_
    SystemParametersInfo(SPI_GETOEMINFO, sizeof(oemstr), oemstr, 0);

    // buggy iPaq38xx
    if ((oemstr[12] == 'H') && (oemstr[13] == '3')
        && (oemstr[14] == '8')
        && (gxdp.cbxPitch > 0)) {
        renderdata->data = (void *) 0xac0755a0;
        renderdata->xPitch = -640;
        renderdata->yPitch = 2;
    }
#if (EMULATE_AXIM_X30 == 0)
    // buggy Dell Axim X30
    if (_tcsncmp(oemstr, L"Dell Axim X30", 13) == 0)
#endif
    {
        GXDeviceInfo gxInfo = { 0 };
        HDC hdc = GetDC(NULL);
        int result;

        gxInfo.Version = 100;
        result =
            ExtEscape(hdc, GETGXINFO, 0, NULL, sizeof(gxInfo),
                      (char *) &gxInfo);
        if (result > 0) {
            renderdata->usageFlags = USAGE_DATA_PTR_CONSTANT;   /* no more GAPI usage from now */
            renderdata->data = gxInfo.pvFrameBuffer;
            this->hidden->needUpdate = 0;
            renderdata->xPitch = 2;
            renderdata->yPitch = 480;
            renderdata->w = gxInfo.cxWidth;
            renderdata->h = gxInfo.cyHeight;

            //Check that the display uses a known display format
            switch (rbi->wFormat) {
            case FORMAT_565:
                renderdata->format = SDL_PIXELFORMAT_RGB565;
                break;
            case FORMAT_555:
                renderdata->format = SDL_PIXELFORMAT_RGB555;
                break;
            default:
                //TODO we should add support for other formats
                SDL_free(renderdata);
                return NULL;
            }
        }
    }
#endif


    if (renderdata->format == SDL_PIXELFORMAT_UNKNOWN) {
        SDL_SetError("Gapi Pixelformat is unknown");
        SDL_free(renderdata);
        return NULL;
    }

    /* Gapi always returns values in standard orientation, so we manually apply
       the current orientation 
     */

    DEVMODE settings;
    SDL_memset(&settings, 0, sizeof(DEVMODE));
    settings.dmSize = sizeof(DEVMODE);

    settings.dmFields = DM_DISPLAYORIENTATION;
    ChangeDisplaySettingsEx(NULL, &settings, NULL, CDS_TEST, NULL);

    if (settings.dmDisplayOrientation == DMDO_90) {

        tmp = renderdata->w;
        renderdata->w = renderdata->h;
        renderdata->h = tmp;

        tmp = renderdata->xPitch;
        renderdata->xPitch = -renderdata->yPitch;
        renderdata->yPitch = tmp;

        renderdata->offset = -renderdata->w * renderdata->xPitch;

    } else if (settings.dmDisplayOrientation == DMDO_180) {

        renderdata->xPitch = -renderdata->xPitch;
        renderdata->yPitch = -renderdata->yPitch;

        renderdata->offset = -renderdata->h * renderdata->yPitch
            - renderdata->w * renderdata->xPitch;

    } else if (settings.dmDisplayOrientation == DMDO_270) {

        tmp = renderdata->w;
        renderdata->w = renderdata->h;
        renderdata->h = tmp;

        tmp = renderdata->xPitch;
        renderdata->xPitch = renderdata->yPitch;
        renderdata->yPitch = -tmp;

        renderdata->offset = -renderdata->h * renderdata->yPitch;

    }

    if (renderdata->w != window->w || renderdata->h != window->h) {
        GAPI_LOG("GAPI open failed, wrong size %i %i %i %i\n", renderdata->w,
                 renderdata->h, renderdata->xPitch, renderdata->yPitch);
        SDL_free(renderdata);
        return NULL;
    }

    return renderdata;

}


/* This function does the whole encapsulation of Gapi/RAWFRAMEBUFFER
   it should handle all the device dependent details and fill the device INDEPENDENT
   RenderData structure.
 */
GAPI_RenderData *
FillRenderData(SDL_Window * window)
{
    /* We try to match the requested window to the modes available by GAPI and RAWFRAMEBUFFER.
       First RAWFRAMEBUFFER is tried, as it is the most reliable one 
       Look here for detailed discussions:
       http://pdaphonehome.com/forums/samsung-i700/28087-just-saw.html
       http://blogs.msdn.com/windowsmobile/archive/2007/08/13/have-you-migrated-to-directdraw-yet.aspx
     */

    GAPI_RenderData *res;

    res = FillRenderDataRawFramebuffer(window);
    GAPI_LOG("FillRenderDataRawFramebuffer: %p\n", res);
    if (res) {
        return res;
    }
    //Now we try gapi
    res = FillRenderDataGAPI(window);
    GAPI_LOG("FillRenderDataGAPI: %p\n", res);

    return res;
}

void *
GetFramebuffer()
{

}


SDL_Renderer *
GAPI_CreateRenderer(SDL_Window * window, Uint32 flags)
{
    SDL_VideoDisplay *display = SDL_GetDisplayFromWindow(window);
    SDL_WindowData *windowdata = (SDL_WindowData *) window->driverdata;
    SDL_DisplayMode *displayMode = &display->current_mode;
    SDL_Renderer *renderer;
    GAPI_RenderData *data;
    int i, n;
    int bpp;
    Uint32 Rmask, Gmask, Bmask, Amask;

    if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
        SDL_SetError("Gapi supports only fullscreen windows");
        return NULL;
    }

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

    data = FillRenderData(window);
    if (!data) {
        GAPI_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return NULL;
    }

    renderer->RenderPoint = GAPI_RenderPoint;
    renderer->RenderLine = GAPI_RenderLine;
    renderer->RenderFill = GAPI_RenderFill;
    renderer->RenderCopy = GAPI_RenderCopy;
    renderer->RenderPresent = GAPI_RenderPresent;
    renderer->DestroyRenderer = GAPI_DestroyRenderer;
    renderer->info.name = GAPI_RenderDriver.info.name;
    renderer->info.flags = 0;
    renderer->window = window->id;
    renderer->driverdata = data;

    /* Gapi provides only a framebuffer so lets use software implementation */
    Setup_SoftwareRenderer(renderer);

#ifdef GAPI_RENDERER_DEBUG
    printf("Created gapi renderer\n");
    printf("use GX functions: %i\n", data->usageFlags & USAGE_GX_FUNCS);
    printf("framebuffer is constant: %i\n",
           data->usageFlags & USAGE_DATA_PTR_CONSTANT);
    printf("w: %i h: %i\n", data->w, data->h);
    printf("data ptr: %p\n", data->data);       /* this can be 0 in case of GAPI usage */
    printf("xPitch: %i\n", data->xPitch);
    printf("yPitch: %i\n", data->yPitch);
    printf("offset: %i\n", data->offset);
    printf("format: %x\n", data->format);
#endif

    if (data->usageFlags & USAGE_GX_FUNCS) {
        if (gx.GXOpenDisplay(windowdata->hwnd, GX_FULLSCREEN) == 0) {
            GAPI_DestroyRenderer(renderer);
            return NULL;
        }
    }

    return renderer;
}

static int
GAPI_RenderPoint(SDL_Renderer * renderer, int x, int y)
{
    //TODO implement
    return -1;
}

static int
GAPI_RenderLine(SDL_Renderer * renderer, int x1, int y1, int x2, int y2)
{
    //TODO implement
    return -11;
}

static int
GAPI_RenderFill(SDL_Renderer * renderer, const SDL_Rect * rect)
{
    //TODO implement
    return -1;
}

/* Video memory is very slow so lets optimize as much as possible */
static void
updateLine16to16(char *src, int srcXPitch, int srcYPitch,
                 char *dst, int dstXPitch, int dstYPitch, int width,
                 int height)
{
    char *srcLine, *dstLine;
    char *srcPix, *dstPix;

    int x, y;

    //First dumb solution
    if (srcXPitch == 2 && dstXPitch == 2) {
        srcLine = src;
        dstLine = dst;
        y = height;
        while (y--) {
            SDL_memcpy(dstLine, srcLine, width * sizeof(Uint16));
            srcLine += srcYPitch;
            dstLine += dstYPitch;
        }
    } else {
        //printf("GAPI uses slow blit path %i, %i\n", dstXPitch, dstYPitch);
        srcLine = src;
        dstLine = dst;
        y = height;
        while (y--) {
            srcPix = srcLine;
            dstPix = dstLine;
            x = width;
            while (x--) {
                *((Uint16 *) dstPix) = *((Uint16 *) srcPix);
                dstPix += dstXPitch;
                srcPix += srcXPitch;
            }
            srcLine += srcYPitch;
            dstLine += dstYPitch;
        }
    }
}

static int
GAPI_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                const SDL_Rect * srcrect, const SDL_Rect * dstrect)
{
    GAPI_RenderData *data = (GAPI_RenderData *) renderer->driverdata;
    int bpp;
    int bytespp;
    int status;
    Uint32 Rmask, Gmask, Bmask, Amask;

    if (texture->format != data->format) {
        SDL_SetError("Gapi got wrong texture");
        return -1;
    }

    GAPI_LOG("GAPI_RenderCopy\n");

    if (data->usageFlags & USAGE_GX_FUNCS) {
        char *buffer;
        buffer = gx.GXBeginDraw();
        if (!(data->usageFlags & USAGE_DATA_PTR_CONSTANT)) {
            data->data = buffer;
        }
    }

    GAPI_LOG("GAPI_RenderCopy blit\n");
    /* If our framebuffer has an xPitch which matches the pixelsize, we
       can convert the framebuffer to a SDL_surface and blit there,
       otherwise, we have to use our own blitting routine
     */
    SDL_PixelFormatEnumToMasks(data->format, &bpp, &Rmask, &Gmask, &Bmask,
                               &Amask);
    bytespp = bpp >> 3;
    if (data->xPitch == bytespp && 0) {
        SDL_Surface *screen =
            SDL_CreateRGBSurfaceFrom(data->data, data->w, data->h,
                                     bpp, data->yPitch, Rmask, Gmask, Bmask,
                                     Amask);
        status =
            SDL_UpperBlit((SDL_Surface *) texture->driverdata, srcrect,
                          screen, dstrect);
        SDL_FreeSurface(screen);
    } else {                    /* screen is rotated, we have to blit on our own */
        SDL_Surface *surface = (SDL_Surface *) texture->driverdata;

        char *src, *dst;
        src = surface->pixels;
        src += srcrect->y * surface->pitch + srcrect->x * 2;

        dst = data->data + data->offset;
        dst += dstrect->y * data->yPitch + dstrect->x * data->xPitch;

        updateLine16to16(src, 2, surface->pitch,
                         dst, data->xPitch, data->yPitch,
                         srcrect->w, srcrect->h);

    }

    Uint32 ticks = SDL_GetTicks();
    if (data->usageFlags & USAGE_GX_FUNCS) {
        gx.GXEndDraw();
    }
    GAPI_LOG("GAPI_RenderCopy %i\n", SDL_GetTicks() - ticks);
    return status;
}

static void
GAPI_RenderPresent(SDL_Renderer * renderer)
{
    /* Nothing todo as we rendered directly to the screen on RenderCopy */
}

static void
GAPI_DestroyRenderer(SDL_Renderer * renderer)
{
    GAPI_RenderData *data = (GAPI_RenderData *) renderer->driverdata;

    if (data->usageFlags & USAGE_GX_FUNCS) {
        gx.GXCloseDisplay();
    }

    if (data) {
        SDL_free(data);
    }
    SDL_free(renderer);
}

#endif /* SDL_VIDEO_RENDER_GAPI */

/* vi: set ts=4 sw=4 expandtab: */
