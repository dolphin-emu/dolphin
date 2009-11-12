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

#include "SDL_DirectFB_video.h"

#define DFB_MAX_MODES 200

struct scn_callback_t
{
    int numscreens;
    DFBScreenID screenid[DFB_MAX_SCREENS];
    DFBDisplayLayerID gralayer[DFB_MAX_SCREENS];
    DFBDisplayLayerID vidlayer[DFB_MAX_SCREENS];
    int aux;                    /* auxiliary integer for callbacks */
};

struct modes_callback_t
{
    int nummodes;
    SDL_DisplayMode *modelist;
};

static int
DFBToSDLPixelFormat(DFBSurfacePixelFormat pixelformat, Uint32 * fmt)
{
    switch (pixelformat) {
    case DSPF_ALUT44:
        *fmt = SDL_PIXELFORMAT_INDEX4LSB;
        break;
    case DSPF_LUT8:
        *fmt = SDL_PIXELFORMAT_INDEX8;
        break;
    case DSPF_RGB332:
        *fmt = SDL_PIXELFORMAT_RGB332;
        break;
    case DSPF_ARGB4444:
        *fmt = SDL_PIXELFORMAT_ARGB4444;
        break;
    case SDL_PIXELFORMAT_ARGB1555:
        *fmt = SDL_PIXELFORMAT_ARGB1555;
        break;
    case DSPF_RGB16:
        *fmt = SDL_PIXELFORMAT_RGB565;
        break;
    case DSPF_RGB24:
        *fmt = SDL_PIXELFORMAT_RGB24;
        break;
    case DSPF_RGB32:
        *fmt = SDL_PIXELFORMAT_RGB888;
        break;
    case DSPF_ARGB:
        *fmt = SDL_PIXELFORMAT_ARGB8888;
        break;
    case DSPF_YV12:
        *fmt = SDL_PIXELFORMAT_YV12;
        break;                  /* Planar mode: Y + V + U  (3 planes) */
    case DSPF_I420:
        *fmt = SDL_PIXELFORMAT_IYUV;
        break;                  /* Planar mode: Y + U + V  (3 planes) */
    case DSPF_YUY2:
        *fmt = SDL_PIXELFORMAT_YUY2;
        break;                  /* Packed mode: Y0+U0+Y1+V0 (1 plane) */
    case DSPF_UYVY:
        *fmt = SDL_PIXELFORMAT_UYVY;
        break;                  /* Packed mode: U0+Y0+V0+Y1 (1 plane) */
    default:
        return -1;
    }
    return 0;
}

static DFBSurfacePixelFormat
SDLToDFBPixelFormat(Uint32 format)
{
    switch (format) {
    case SDL_PIXELFORMAT_INDEX4LSB:
        return DSPF_ALUT44;
    case SDL_PIXELFORMAT_INDEX8:
        return DSPF_LUT8;
    case SDL_PIXELFORMAT_RGB332:
        return DSPF_RGB332;
    case SDL_PIXELFORMAT_RGB555:
        return DSPF_ARGB1555;
    case SDL_PIXELFORMAT_ARGB4444:
        return DSPF_ARGB4444;
    case SDL_PIXELFORMAT_ARGB1555:
        return DSPF_ARGB1555;
    case SDL_PIXELFORMAT_RGB565:
        return DSPF_RGB16;
    case SDL_PIXELFORMAT_RGB24:
        return DSPF_RGB24;
    case SDL_PIXELFORMAT_RGB888:
        return DSPF_RGB32;
    case SDL_PIXELFORMAT_ARGB8888:
        return DSPF_ARGB;
    case SDL_PIXELFORMAT_YV12:
        return DSPF_YV12;       /* Planar mode: Y + V + U  (3 planes) */
    case SDL_PIXELFORMAT_IYUV:
        return DSPF_I420;       /* Planar mode: Y + U + V  (3 planes) */
    case SDL_PIXELFORMAT_YUY2:
        return DSPF_YUY2;       /* Packed mode: Y0+U0+Y1+V0 (1 plane) */
    case SDL_PIXELFORMAT_UYVY:
        return DSPF_UYVY;       /* Packed mode: U0+Y0+V0+Y1 (1 plane) */
    case SDL_PIXELFORMAT_YVYU:
        return DSPF_UNKNOWN;    /* Packed mode: Y0+V0+Y1+U0 (1 plane) */
    case SDL_PIXELFORMAT_INDEX1LSB:
        return DSPF_UNKNOWN;
    case SDL_PIXELFORMAT_INDEX1MSB:
        return DSPF_UNKNOWN;
    case SDL_PIXELFORMAT_INDEX4MSB:
        return DSPF_UNKNOWN;
    case SDL_PIXELFORMAT_RGB444:
        return DSPF_UNKNOWN;
    case SDL_PIXELFORMAT_BGR24:
        return DSPF_UNKNOWN;
    case SDL_PIXELFORMAT_BGR888:
        return DSPF_UNKNOWN;
    case SDL_PIXELFORMAT_RGBA8888:
        return DSPF_UNKNOWN;
    case SDL_PIXELFORMAT_ABGR8888:
        return DSPF_UNKNOWN;
    case SDL_PIXELFORMAT_BGRA8888:
        return DSPF_UNKNOWN;
    case SDL_PIXELFORMAT_ARGB2101010:
        return DSPF_UNKNOWN;
    default:
        return DSPF_UNKNOWN;
    }
}

static DFBEnumerationResult
EnumModesCallback(int width, int height, int bpp, void *data)
{
    struct modes_callback_t *modedata = (struct modes_callback_t *) data;
    SDL_DisplayMode mode;

    mode.w = width;
    mode.h = height;
    mode.refresh_rate = 0;
    mode.driverdata = NULL;
    mode.format = SDL_PIXELFORMAT_UNKNOWN;

    if (modedata->nummodes < DFB_MAX_MODES) {
        modedata->modelist[modedata->nummodes++] = mode;
    }

    SDL_DFB_DEBUG("w %d h %d bpp %d\n", width, height, bpp);
    return DFENUM_OK;
}

static DFBEnumerationResult
cbScreens(DFBScreenID screen_id, DFBScreenDescription desc,
          void *callbackdata)
{
    struct scn_callback_t *devdata = (struct scn_callback_t *) callbackdata;

    devdata->screenid[devdata->numscreens++] = screen_id;
    return DFENUM_OK;
}

DFBEnumerationResult
cbLayers(DFBDisplayLayerID layer_id, DFBDisplayLayerDescription desc,
         void *callbackdata)
{
    struct scn_callback_t *devdata = (struct scn_callback_t *) callbackdata;

    if (desc.caps & DLCAPS_SURFACE) {
        if ((desc.type & DLTF_GRAPHICS) && (desc.type & DLTF_VIDEO)) {
            if (devdata->vidlayer[devdata->aux] == -1)
                devdata->vidlayer[devdata->aux] = layer_id;
        } else if (desc.type & DLTF_GRAPHICS) {
            if (devdata->gralayer[devdata->aux] == -1)
                devdata->gralayer[devdata->aux] = layer_id;
        }
    }
    return DFENUM_OK;
}

static void
CheckSetDisplayMode(_THIS, DFB_DisplayData * data, SDL_DisplayMode * mode)
{
    SDL_DFB_DEVICEDATA(_this);
    DFBDisplayLayerConfig config;
    DFBDisplayLayerConfigFlags failed;
    int ret;

    SDL_DFB_CHECKERR(data->layer->SetCooperativeLevel(data->layer,
                                                      DLSCL_ADMINISTRATIVE));
    config.width = mode->w;
    config.height = mode->h;
    config.pixelformat = SDLToDFBPixelFormat(mode->format);
    config.flags = DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT;
    if (devdata->use_yuv_underlays) {
        config.flags |= DLCONF_OPTIONS;
        config.options = DLOP_ALPHACHANNEL;
    }
    failed = 0;
    data->layer->TestConfiguration(data->layer, &config, &failed);
    SDL_DFB_CHECKERR(data->layer->SetCooperativeLevel(data->layer,
                                                      DLSCL_SHARED));
    if (failed == 0)
        SDL_AddDisplayMode(_this->current_display, mode);
    else
        SDL_DFB_DEBUG("Mode %d x %d not available: %x\n", mode->w,
                      mode->h, failed);

    return;
  error:
    return;
}

void
DirectFB_InitModes(_THIS)
{
    SDL_DFB_DEVICEDATA(_this);
    IDirectFBDisplayLayer *layer = NULL;
    SDL_VideoDisplay display;
    DFB_DisplayData *dispdata = NULL;
    SDL_DisplayMode mode;
    DFBGraphicsDeviceDescription caps;
    DFBDisplayLayerConfig dlc;
    struct scn_callback_t *screencbdata;

    int tcw[DFB_MAX_SCREENS];
    int tch[DFB_MAX_SCREENS];
    int i;
    DFBResult ret;

    SDL_DFB_CALLOC(screencbdata, 1, sizeof(*screencbdata));

    screencbdata->numscreens = 0;

    for (i = 0; i < DFB_MAX_SCREENS; i++) {
        screencbdata->gralayer[i] = -1;
        screencbdata->vidlayer[i] = -1;
    }

    SDL_DFB_CHECKERR(devdata->dfb->EnumScreens(devdata->dfb, &cbScreens,
                                               screencbdata));

    for (i = 0; i < screencbdata->numscreens; i++) {
        IDirectFBScreen *screen;

        SDL_DFB_CHECKERR(devdata->dfb->GetScreen(devdata->dfb,
                                                 screencbdata->screenid
                                                 [i], &screen));

        screencbdata->aux = i;
        SDL_DFB_CHECKERR(screen->EnumDisplayLayers(screen, &cbLayers,
                                                   screencbdata));
        screen->GetSize(screen, &tcw[i], &tch[i]);
        screen->Release(screen);
    }

    /* Query card capabilities */

    devdata->dfb->GetDeviceDescription(devdata->dfb, &caps);

    SDL_DFB_DEBUG("SDL directfb video driver - %s %s\n", __DATE__, __TIME__);
    SDL_DFB_DEBUG("Using %s (%s) driver.\n", caps.name, caps.vendor);
    SDL_DFB_DEBUG("Found %d screens\n", screencbdata->numscreens);

    for (i = 0; i < screencbdata->numscreens; i++) {
        SDL_DFB_CHECKERR(devdata->dfb->GetDisplayLayer(devdata->dfb,
                                                       screencbdata->gralayer
                                                       [i], &layer));

        SDL_DFB_CHECKERR(layer->SetCooperativeLevel(layer,
                                                    DLSCL_ADMINISTRATIVE));
        layer->EnableCursor(layer, 1);
        SDL_DFB_CHECKERR(layer->SetCursorOpacity(layer, 0xC0));

        if (devdata->use_yuv_underlays) {
            dlc.flags = DLCONF_PIXELFORMAT | DLCONF_OPTIONS;
            dlc.pixelformat = DSPF_ARGB;
            dlc.options = DLOP_ALPHACHANNEL;

            ret = layer->SetConfiguration(layer, &dlc);
            if (ret) {
                /* try AiRGB if the previous failed */
                dlc.pixelformat = DSPF_AiRGB;
                ret = layer->SetConfiguration(layer, &dlc);
            }
        }

        /* Query layer configuration to determine the current mode and pixelformat */
        dlc.flags = DLCONF_ALL;
        layer->GetConfiguration(layer, &dlc);

        if (DFBToSDLPixelFormat(dlc.pixelformat, &mode.format) != 0) {
            SDL_DFB_ERR("Unknown dfb pixelformat %x !\n", dlc.pixelformat);
            goto error;
        }

        mode.w = dlc.width;
        mode.h = dlc.height;
        mode.refresh_rate = 0;
        mode.driverdata = NULL;

        SDL_DFB_CALLOC(dispdata, 1, sizeof(*dispdata));

        dispdata->layer = layer;
        dispdata->pixelformat = dlc.pixelformat;
        dispdata->cw = tcw[i];
        dispdata->ch = tch[i];

        /* YUV - Video layer */

        dispdata->vidID = screencbdata->vidlayer[i];
        dispdata->vidIDinuse = 0;

        SDL_zero(display);

        display.desktop_mode = mode;
        display.current_mode = mode;
        display.driverdata = dispdata;

#if (DFB_VERSION_ATLEAST(1,2,0))
        dlc.flags =
            DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT |
            DLCONF_OPTIONS;
        ret = layer->SetConfiguration(layer, &dlc);
#endif

        SDL_DFB_CHECKERR(layer->SetCooperativeLevel(layer, DLSCL_SHARED));

        SDL_AddVideoDisplay(&display);
    }
    SDL_DFB_FREE(screencbdata);
    return;
  error:
    /* FIXME: Cleanup not complete, Free existing displays */
    SDL_DFB_FREE(dispdata);
    SDL_DFB_RELEASE(layer);
    return;
}

void
DirectFB_GetDisplayModes(_THIS)
{
    SDL_DFB_DEVICEDATA(_this);
    DFB_DisplayData *dispdata =
        (DFB_DisplayData *) SDL_CurrentDisplay.driverdata;
    SDL_DisplayMode mode;
    struct modes_callback_t data;
    int i;
    int ret;

    data.nummodes = 0;
    /* Enumerate the available fullscreen modes */
    SDL_DFB_CALLOC(data.modelist, DFB_MAX_MODES, sizeof(SDL_DisplayMode));
    SDL_DFB_CHECKERR(devdata->dfb->EnumVideoModes(devdata->dfb,
                                                  EnumModesCallback, &data));

    for (i = 0; i < data.nummodes; ++i) {
        mode = data.modelist[i];

        mode.format = SDL_PIXELFORMAT_ARGB8888;
        CheckSetDisplayMode(_this, dispdata, &mode);
        mode.format = SDL_PIXELFORMAT_RGB888;
        CheckSetDisplayMode(_this, dispdata, &mode);
        mode.format = SDL_PIXELFORMAT_RGB24;
        CheckSetDisplayMode(_this, dispdata, &mode);
        mode.format = SDL_PIXELFORMAT_RGB565;
        CheckSetDisplayMode(_this, dispdata, &mode);
        mode.format = SDL_PIXELFORMAT_INDEX8;
        CheckSetDisplayMode(_this, dispdata, &mode);
    }
    SDL_DFB_FREE(data.modelist);
    return;
  error:
    SDL_DFB_FREE(data.modelist);
    return;
}

int
DirectFB_SetDisplayMode(_THIS, SDL_DisplayMode * mode)
{
    /*
     * FIXME: video mode switch is currently broken for 1.2.0
     * 
     */

    SDL_DFB_DEVICEDATA(_this);
    DFB_DisplayData *data = (DFB_DisplayData *) SDL_CurrentDisplay.driverdata;
    DFBDisplayLayerConfig config, rconfig;
    DFBDisplayLayerConfigFlags fail = 0;
    DFBResult ret;

    SDL_DFB_CHECKERR(data->layer->SetCooperativeLevel(data->layer,
                                                      DLSCL_ADMINISTRATIVE));

    SDL_DFB_CHECKERR(data->layer->GetConfiguration(data->layer, &config));
    config.flags = DLCONF_WIDTH | DLCONF_HEIGHT;
    if (mode->format != SDL_PIXELFORMAT_UNKNOWN) {
        config.flags |= DLCONF_PIXELFORMAT;
        config.pixelformat = SDLToDFBPixelFormat(mode->format);
        data->pixelformat = config.pixelformat;
    }
    config.width = mode->w;
    config.height = mode->h;

    if (devdata->use_yuv_underlays) {
        config.flags |= DLCONF_OPTIONS;
        config.options = DLOP_ALPHACHANNEL;
    }

    data->layer->TestConfiguration(data->layer, &config, &fail);

    if (fail &
        (DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT |
         DLCONF_OPTIONS)) {
        SDL_DFB_ERR("Error setting mode %dx%d-%x\n", mode->w, mode->h,
                    mode->format);
        return -1;
    }

    SDL_DFB_DEBUG("Trace\n");
    config.flags &= ~fail;
    SDL_DFB_CHECKERR(data->layer->SetConfiguration(data->layer, &config));
#if (DFB_VERSION_ATLEAST(1,2,0))
    /* Need to call this twice ! */
    SDL_DFB_CHECKERR(data->layer->SetConfiguration(data->layer, &config));
#endif

    /* Double check */
    SDL_DFB_CHECKERR(data->layer->GetConfiguration(data->layer, &rconfig));
    SDL_DFB_CHECKERR(data->
                     layer->SetCooperativeLevel(data->layer, DLSCL_SHARED));

    if ((config.width != rconfig.width) || (config.height != rconfig.height)
        || ((mode->format != SDL_PIXELFORMAT_UNKNOWN)
            && (config.pixelformat != rconfig.pixelformat))) {
        SDL_DFB_ERR("Error setting mode %dx%d-%x\n", mode->w, mode->h,
                    mode->format);
        return -1;
    }

    data->pixelformat = rconfig.pixelformat;
    data->cw = config.width;
    data->ch = config.height;
    SDL_CurrentDisplay.current_mode = *mode;

    return 0;
  error:
    return -1;
}

void
DirectFB_QuitModes(_THIS)
{
    //DFB_DeviceData *devdata = (DFB_DeviceData *) _this->driverdata;
    SDL_DisplayMode tmode;
    DFBResult ret;
    int i;

    SDL_SelectVideoDisplay(0);

    SDL_GetDesktopDisplayMode(&tmode);
    tmode.format = SDL_PIXELFORMAT_UNKNOWN;
    DirectFB_SetDisplayMode(_this, &tmode);

    SDL_GetDesktopDisplayMode(&tmode);
    DirectFB_SetDisplayMode(_this, &tmode);

    for (i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        DFB_DisplayData *dispdata =
            (DFB_DisplayData *) _this->displays[i].driverdata;

        if (dispdata->layer) {
            SDL_DFB_CHECK(dispdata->
                          layer->SetCooperativeLevel(dispdata->layer,
                                                     DLSCL_ADMINISTRATIVE));
            SDL_DFB_CHECK(dispdata->
                          layer->SetCursorOpacity(dispdata->layer, 0x00));
            SDL_DFB_CHECK(dispdata->
                          layer->SetCooperativeLevel(dispdata->layer,
                                                     DLSCL_SHARED));
        }

        SDL_DFB_RELEASE(dispdata->layer);
        SDL_DFB_RELEASE(dispdata->vidlayer);

    }
}

/* vi: set ts=4 sw=4 expandtab: */
