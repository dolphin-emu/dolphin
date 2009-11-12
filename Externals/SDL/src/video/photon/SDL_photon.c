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

    QNX Photon GUI SDL driver
    Copyright (C) 2009 Mike Gorchak
    (mike@malva.ua, lestat@i.com.ua)
*/

/* SDL internals */
#include "SDL_config.h"
#include "../SDL_sysvideo.h"
#include "SDL_version.h"
#include "SDL_syswm.h"
#include "SDL_loadso.h"
#include "SDL_events.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"

/* Photon declarations */
#include "SDL_photon.h"

/* Pixel format conversion routines */
#include "SDL_photon_pixelfmt.h"

/* Use GF's pixel format functions for OpenGL ES context creation */
#if defined(SDL_VIDEO_OPENGL_ES)
#include "../qnxgf/SDL_gf_pixelfmt.h"

/* If GF driver is not compiled in, include some of usefull functions */
#if !defined(SDL_VIDEO_DRIVER_QNXGF)
#include "../qnxgf/SDL_gf_pixelfmt.c"
#endif /* SDL_VIDEO_DRIVER_QNXGF */
#endif /* SDL_VIDEO_OPENGL_ES */

/* Use GF's OpenGL ES 1.1 functions emulation */
#if defined(SDL_VIDEO_OPENGL_ES)
#include "../qnxgf/SDL_gf_opengles.h"

/* If GF driver is not compiled in, include some of usefull functions */
#if !defined(SDL_VIDEO_DRIVER_QNXGF)
#include "../qnxgf/SDL_gf_opengles.c"
#endif /* SDL_VIDEO_DRIVER_QNXGF */
#endif /* SDL_VIDEO_OPENGL_ES */

/* Low level device graphics driver names, which they are reporting */
static const Photon_DeviceCaps photon_devicename[] = {
    /* ATI Rage 128 graphics driver (devg-ati_rage128)      */
    {"ati_rage128", SDL_PHOTON_ACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* Fujitsu Carmine graphics driver (devg-carmine.so)    */
    {"carmine", SDL_PHOTON_ACCELERATED | SDL_PHOTON_ACCELERATED_3D}
    ,
    /* C&T graphics driver (devg-chips.so)                  */
    {"chips", SDL_PHOTON_ACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* Fujitsu Coral graphics driver (devg-coral.so)        */
    {"coral", SDL_PHOTON_ACCELERATED | SDL_PHOTON_ACCELERATED_3D}
    ,
    /* Intel integrated graphics driver (devg-extreme2.so)  */
    {"extreme2", SDL_PHOTON_ACCELERATED | SDL_PHOTON_ACCELERATED_3D}
    ,
    /* Unaccelerated FB driver (devg-flat.so)               */
    {"flat", SDL_PHOTON_UNACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* NS Geode graphics driver (devg-geode.so)             */
    {"geode", SDL_PHOTON_ACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* Geode LX graphics driver (devg-geodelx.so)           */
    {"geodelx", SDL_PHOTON_ACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* Intel integrated graphics driver (devg-gma9xx.so)    */
    {"gma", SDL_PHOTON_ACCELERATED | SDL_PHOTON_ACCELERATED_3D}
    ,
    /* Intel integrated graphics driver (devg-i810.so)      */
    {"i810", SDL_PHOTON_ACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* Intel integrated graphics driver (devg-i830.so)      */
    {"i830", SDL_PHOTON_ACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* Geode LX graphics driver (devg-lx800.so)             */
    {"lx800", SDL_PHOTON_ACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* Matrox Gxx graphics driver (devg-matroxg.so)         */
    {"matroxg", SDL_PHOTON_ACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* Intel Poulsbo graphics driver (devg-poulsbo.so)      */
    {"poulsbo", SDL_PHOTON_ACCELERATED | SDL_PHOTON_ACCELERATED_3D}
    ,
    /* ATI Radeon driver (devg-radeon.so)                   */
    {"radeon", SDL_PHOTON_ACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* ATI Rage driver (devg-rage.so)                       */
    {"rage", SDL_PHOTON_ACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* S3 Savage graphics driver (devg-s3_savage.so)        */
    {"s3_savage", SDL_PHOTON_ACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* SiS630 integrated graphics driver (devg-sis630.so)   */
    {"sis630", SDL_PHOTON_ACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* PowerVR SGX 535 graphics driver (devg-poulsbo.so)    */
    {"sgx", SDL_PHOTON_ACCELERATED | SDL_PHOTON_ACCELERATED_3D}
    ,
    /* SM Voyager GX graphics driver (devg-smi5xx.so)       */
    {"smi5xx", SDL_PHOTON_ACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* Silicon Motion graphics driver (devg-smi7xx.so)      */
    {"smi7xx", SDL_PHOTON_ACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* SVGA unaccelerated gfx driver (devg-svga.so)         */
    {"svga", SDL_PHOTON_UNACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* nVidia TNT graphics driver (devg-tnt.so)             */
    {"tnt", SDL_PHOTON_ACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* VIA integrated graphics driver (devg-tvia.so)        */
    {"tvia", SDL_PHOTON_ACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* VIA UniChrome graphics driver (devg-unichrome.so)    */
    {"unichrome", SDL_PHOTON_ACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* VESA unaccelerated gfx driver (devg-vesabios.so)     */
    {"vesa", SDL_PHOTON_UNACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* VmWare graphics driver (devg-volari.so)              */
    {"vmware", SDL_PHOTON_ACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* XGI XP10 graphics driver (devg-volari.so), OpenGL 1.5 */
    {"volari", SDL_PHOTON_ACCELERATED | SDL_PHOTON_UNACCELERATED_3D}
    ,
    /* End of list */
    {NULL, 0x00000000}
};

static SDL_bool photon_initialized = SDL_FALSE;

static int
photon_available(void)
{
    int32_t status;

    /* Check if Photon was initialized before */
    if (photon_initialized == SDL_FALSE) {
        /* Initialize Photon widget library and open channel to Photon */
        status = PtInit(NULL);
        if (status == 0) {
            photon_initialized = SDL_TRUE;
            return 1;
        } else {
            photon_initialized = SDL_FALSE;
            return 0;
        }
    }

    return 1;
}

static void
photon_destroy(SDL_VideoDevice * device)
{
    SDL_VideoData *phdata = (SDL_VideoData *) device->driverdata;

#if defined(SDL_VIDEO_OPENGL_ES)
    if (phdata->gfinitialized != SDL_FALSE) {
        gf_dev_detach(phdata->gfdev);
    }
#endif /* SDL_VIDEO_OPENGL_ES */

    if (device->driverdata != NULL) {
        device->driverdata = NULL;
    }
}

static SDL_VideoDevice *
photon_create(int devindex)
{
    SDL_VideoDevice *device;
    SDL_VideoData *phdata;
    int32_t status;

    /* Check if photon could be initialized */
    status = photon_available();
    if (status == 0) {
        /* Photon could not be used */
        return NULL;
    }

    /* Photon agregates all video devices to one with multiple heads */
    if (devindex != 0) {
        /* devindex could be zero only in Photon SDL driver */
        return NULL;
    }

    /* Initialize SDL_VideoDevice structure */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (device == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    /* Initialize internal photon specific data */
    phdata = (SDL_VideoData *) SDL_calloc(1, sizeof(SDL_VideoData));
    if (phdata == NULL) {
        SDL_OutOfMemory();
        SDL_free(device);
        return NULL;
    }
    device->driverdata = phdata;

    /* Get all photon display devices */
    phdata->avail_rids =
        PdGetDevices(&phdata->rid[0], SDL_VIDEO_PHOTON_MAX_RIDS);
    if (phdata->avail_rids > SDL_VIDEO_PHOTON_MAX_RIDS) {
        phdata->avail_rids = SDL_VIDEO_PHOTON_MAX_RIDS;
    }
#if defined(SDL_VIDEO_OPENGL_ES)
    /* TODO: add real device detection versus multiple heads */
    status =
        gf_dev_attach(&phdata->gfdev, GF_DEVICE_INDEX(0),
                      &phdata->gfdev_info);
    if (status != GF_ERR_OK) {
        /* Do not fail right now, if GF can't be attached */
        phdata->gfinitialized = SDL_FALSE;
    } else {
        phdata->gfinitialized = SDL_TRUE;
    }
#endif /* SDL_VIDEO_OPENGL_ES */

    /* Set default target device */
    status = PdSetTargetDevice(NULL, phdata->rid[0]);
    if (status == -1) {
        SDL_SetError("Photon: Can't set default target device");
#if defined(SDL_VIDEO_OPENGL_ES)
        gf_dev_detach(phdata->gfdev);
#endif /* SDL_VIDEO_OPENGL_ES */
        SDL_free(phdata);
        SDL_free(device);
        return NULL;
    }
    phdata->current_device_id = 0;

    /* Setup amount of available displays and current display */
    device->num_displays = 0;
    device->current_display = 0;

    /* Set device free function */
    device->free = photon_destroy;

    /* Setup all functions which we can handle */
    device->VideoInit = photon_videoinit;
    device->VideoQuit = photon_videoquit;
    device->GetDisplayModes = photon_getdisplaymodes;
    device->SetDisplayMode = photon_setdisplaymode;
    device->SetDisplayPalette = photon_setdisplaypalette;
    device->GetDisplayPalette = photon_getdisplaypalette;
    device->SetDisplayGammaRamp = photon_setdisplaygammaramp;
    device->GetDisplayGammaRamp = photon_getdisplaygammaramp;
    device->CreateWindow = photon_createwindow;
    device->CreateWindowFrom = photon_createwindowfrom;
    device->SetWindowTitle = photon_setwindowtitle;
    device->SetWindowIcon = photon_setwindowicon;
    device->SetWindowPosition = photon_setwindowposition;
    device->SetWindowSize = photon_setwindowsize;
    device->ShowWindow = photon_showwindow;
    device->HideWindow = photon_hidewindow;
    device->RaiseWindow = photon_raisewindow;
    device->MaximizeWindow = photon_maximizewindow;
    device->MinimizeWindow = photon_minimizewindow;
    device->RestoreWindow = photon_restorewindow;
    device->SetWindowGrab = photon_setwindowgrab;
    device->DestroyWindow = photon_destroywindow;
    device->GetWindowWMInfo = photon_getwindowwminfo;
    device->GL_LoadLibrary = photon_gl_loadlibrary;
    device->GL_GetProcAddress = photon_gl_getprocaddres;
    device->GL_UnloadLibrary = photon_gl_unloadlibrary;
    device->GL_CreateContext = photon_gl_createcontext;
    device->GL_MakeCurrent = photon_gl_makecurrent;
    device->GL_SetSwapInterval = photon_gl_setswapinterval;
    device->GL_GetSwapInterval = photon_gl_getswapinterval;
    device->GL_SwapWindow = photon_gl_swapwindow;
    device->GL_DeleteContext = photon_gl_deletecontext;
    device->PumpEvents = photon_pumpevents;
    device->SuspendScreenSaver = photon_suspendscreensaver;

    return device;
}

VideoBootStrap photon_bootstrap = {
    "photon",
    "SDL QNX Photon video driver",
    photon_available,
    photon_create
};

/*****************************************************************************/
/* SDL Video and Display initialization/handling functions                   */
/*****************************************************************************/
int
photon_videoinit(_THIS)
{
    SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
    PgHWCaps_t hwcaps;
    PgVideoModeInfo_t modeinfo;
    int32_t status;
    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;
    SDL_DisplayData *didata;
    uint32_t it;
    uint32_t jt;
    char *override;

    /* By default Photon do not uses swap on VSYNC */
#if defined(SDL_VIDEO_OPENGL_ES)
    phdata->swapinterval = 0;
#endif /* SDL_VIDEO_OPENGL_ES */

    for (it = 0; it < phdata->avail_rids; it++) {
        didata = (SDL_DisplayData *) SDL_calloc(1, sizeof(SDL_DisplayData));
        if (didata == NULL) {
            /* memory allocation error */
            SDL_OutOfMemory();
            return -1;
        }

        /* Allocate two cursors with SDL_VIDEO_PHOTON_MAX_CURSOR_SIZE size */
        /* and 128 bytes of spare place                                    */
        didata->cursor_size =
            ((SDL_VIDEO_PHOTON_MAX_CURSOR_SIZE * 4) >> 3) + 128;
        didata->cursor = (PhCursorDef_t *) SDL_calloc(1, didata->cursor_size);
        if (didata->cursor == NULL) {
            /* memory allocation error */
            SDL_OutOfMemory();
            SDL_free(didata);
            return -1;
        }

        /* Initialize GF in case of OpenGL ES support is compiled in */
#if defined(SDL_VIDEO_OPENGL_ES)
        /* TODO: add real device detection versus multiple heads */
        if (phdata->gfinitialized == SDL_TRUE) {
            status =
                gf_display_attach(&didata->display, phdata->gfdev, it,
                                  &didata->display_info);
            if (status != GF_ERR_OK) {
                /* Just shutdown GF, do not fail */
                gf_dev_detach(phdata->gfdev);
                phdata->gfinitialized = SDL_FALSE;
            }
        }
#endif /* SDL_VIDEO_OPENGL_ES */

        /* Check if current device is not the same as target */
        if (phdata->current_device_id != it) {
            /* Set target device as default for Pd and Pg functions */
            status = PdSetTargetDevice(NULL, phdata->rid[it]);
            if (status != 0) {
                SDL_SetError("Photon: Can't set default target device\n");
                SDL_free(didata->cursor);
                SDL_free(didata);
                return -1;
            }
            phdata->current_device_id = it;
        }

        /* Store device id */
        didata->device_id = it;

        /* Query photon about graphics hardware caps and current video mode */
        SDL_memset(&hwcaps, 0x00, sizeof(PgHWCaps_t));
        status = PgGetGraphicsHWCaps(&hwcaps);
        if (status != 0) {
            PhRect_t extent;
            PdOffscreenContext_t* curctx;

            /* If error happens, this also could mean, that photon is working */
            /* under custom (not listed by photon) video mode                 */
            status=PhWindowQueryVisible(Ph_QUERY_GRAPHICS, 0, 0, &extent);
            if (status != 0) {
                SDL_SetError("Photon: Can't get graphics driver region");
                SDL_free(didata->cursor);
                SDL_free(didata);
                return -1;
            }
            modeinfo.width=extent.lr.x+1;
            modeinfo.height=extent.lr.y+1;
            /* Hardcode 60Hz, as the base refresh rate frequency */
            hwcaps.current_rrate=60;
            /* Clear current video driver name, no way to get it somehow */
            hwcaps.chip_name[0]=0x00;

            /* Create offscreen context from video memory, which is currently */
            /* displayed on the screen                                        */
            curctx=PdCreateOffscreenContext(0, 0, 0, Pg_OSC_MAIN_DISPLAY);
            if (curctx==NULL)
            {
                SDL_SetError("Photon: Can't get display area capabilities");
                SDL_free(didata->cursor);
                SDL_free(didata);
                return -1;
            }
            /* Retrieve current bpp */
            modeinfo.type=curctx->format;
            PhDCRelease(curctx);
        } else {
            /* Get current video mode details */
            status = PgGetVideoModeInfo(hwcaps.current_video_mode, &modeinfo);
            if (status != 0) {
                SDL_SetError("Photon: Can't get current video mode information");
                SDL_free(didata->cursor);
                SDL_free(didata);
                return -1;
            }

            /* Get current video mode 2D capabilities for the renderer */
            didata->mode_2dcaps=0;
            if ((modeinfo.mode_capabilities2 & PgVM_MODE_CAP2_ALPHA_BLEND)==PgVM_MODE_CAP2_ALPHA_BLEND)
            {
               didata->mode_2dcaps|=SDL_VIDEO_PHOTON_CAP_ALPHA_BLEND;
            }
            if ((modeinfo.mode_capabilities2 & PgVM_MODE_CAP2_SCALED_BLIT)==PgVM_MODE_CAP2_SCALED_BLIT)
            {
               didata->mode_2dcaps|=SDL_VIDEO_PHOTON_CAP_SCALED_BLIT;
            }
        }

        /* Setup current desktop mode for SDL */
        SDL_zero(current_mode);
        current_mode.w = modeinfo.width;
        current_mode.h = modeinfo.height;
        current_mode.refresh_rate = hwcaps.current_rrate;
        current_mode.format = photon_image_to_sdl_pixelformat(modeinfo.type);
        current_mode.driverdata = NULL;

        /* Copy device name */
        SDL_strlcpy(didata->description, hwcaps.chip_name,
                    SDL_VIDEO_PHOTON_DEVICENAME_MAX - 1);

        /* Search device capabilities and possible workarounds */
        jt = 0;
        do {
            if (photon_devicename[jt].name == NULL) {
                break;
            }
            if (SDL_strncmp
                (photon_devicename[jt].name, didata->description,
                 SDL_strlen(photon_devicename[jt].name)) == 0) {
                didata->caps = photon_devicename[jt].caps;
            }
            jt++;
        } while (1);

        /* Initialize display structure */
        SDL_zero(display);
        display.desktop_mode = current_mode;
        display.current_mode = current_mode;
        display.fullscreen_mode = current_mode;
        display.driverdata = didata;
        didata->current_mode = current_mode;
        SDL_AddVideoDisplay(&display);

        /* Check for environment variables which could override some SDL settings */
        didata->custom_refresh = 0;
        override = SDL_getenv("SDL_VIDEO_PHOTON_REFRESH_RATE");
        if (override != NULL) {
            if (SDL_sscanf(override, "%u", &didata->custom_refresh) != 1) {
                didata->custom_refresh = 0;
            }
        }
    }

    /* Add photon input devices */
    status = photon_addinputdevices(_this);
    if (status != 0) {
        /* SDL error is set by photon_addinputdevices() function */
        return -1;
    }

    /* Add photon renderer to SDL */
    photon_addrenderdriver(_this);

    /* video has been initialized successfully */
    return 1;
}

void
photon_videoquit(_THIS)
{
    SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
    SDL_DisplayData *didata;
    uint32_t it;

    /* SDL will restore our desktop mode on exit */
    for (it = 0; it < _this->num_displays; it++) {
        didata = _this->displays[it].driverdata;

        if (didata->cursor != NULL) {
            SDL_free(didata->cursor);
        }
#if defined(SDL_VIDEO_OPENGL_ES)
        if (phdata->gfinitialized == SDL_TRUE) {
            /* Detach GF display */
            gf_display_detach(didata->display);
        }
#endif /* SDL_VIDEO_OPENGL_ES */
    }
}

void
photon_getdisplaymodes(_THIS)
{
    SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
    SDL_DisplayData *didata =
        (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;
    SDL_DisplayMode mode;
    PgVideoModes_t modes;
    PgVideoModeInfo_t modeinfo;
    int32_t status;
    uint32_t it;
    uint32_t jt;

    /* Check if current device is not the same as target */
    if (phdata->current_device_id != didata->device_id) {
        /* Set target device as default for Pd and Pg functions */
        status = PdSetTargetDevice(NULL, phdata->rid[didata->device_id]);
        if (status != 0) {
            SDL_SetError("Photon: Can't set default target device\n");
            return;
        }
        phdata->current_device_id = didata->device_id;
    }

    /* Get array of available video modes */
    status = PgGetVideoModeList(&modes);
    if (status != 0) {
        SDL_SetError("Photon: Can't get video mode list");
        return;
    }

    for (it = 0; it < modes.num_modes; it++) {
        status = PgGetVideoModeInfo(modes.modes[it], &modeinfo);
        if (status != 0) {
            /* Maybe something wrong with this particular mode, skip it */
            continue;
        }

        jt = 0;
        do {
            if (modeinfo.refresh_rates[jt] != 0) {
                mode.w = modeinfo.width;
                mode.h = modeinfo.height;
                mode.refresh_rate = modeinfo.refresh_rates[jt];
                mode.format = photon_image_to_sdl_pixelformat(modeinfo.type);
                mode.driverdata = NULL;
                SDL_AddDisplayMode(_this->current_display, &mode);

                /* If mode is RGBA8888, add the same mode as RGBx888 */
                if (modeinfo.type == Pg_IMAGE_DIRECT_8888) {
                    mode.w = modeinfo.width;
                    mode.h = modeinfo.height;
                    mode.refresh_rate = modeinfo.refresh_rates[jt];
                    mode.format = SDL_PIXELFORMAT_RGB888;
                    mode.driverdata = NULL;
                    SDL_AddDisplayMode(_this->current_display, &mode);
                }

                /* If mode is RGBA1555, add the same mode as RGBx555 */
                if (modeinfo.type == Pg_IMAGE_DIRECT_1555) {
                    mode.w = modeinfo.width;
                    mode.h = modeinfo.height;
                    mode.refresh_rate = modeinfo.refresh_rates[jt];
                    mode.format = SDL_PIXELFORMAT_RGB555;
                    mode.driverdata = NULL;
                    SDL_AddDisplayMode(_this->current_display, &mode);
                }

                jt++;
            } else {
                break;
            }
        } while (1);
    }
}

int
photon_setdisplaymode(_THIS, SDL_DisplayMode * mode)
{
    SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
    SDL_DisplayData *didata =
        (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;
    PgVideoModes_t modes;
    PgVideoModeInfo_t modeinfo;
    PgDisplaySettings_t modesettings;
    uint32_t refresh_rate = 0;
    int32_t status;
    uint32_t it;

    /* Check if current device is not the same as target */
    if (phdata->current_device_id != didata->device_id) {
        /* Set target device as default for Pd and Pg functions */
        status = PdSetTargetDevice(NULL, phdata->rid[didata->device_id]);
        if (status != 0) {
            SDL_SetError("Photon: Can't set default target device\n");
            return -1;
        }
        phdata->current_device_id = didata->device_id;
    }

    /* Get array of available video modes */
    status = PgGetVideoModeList(&modes);
    if (status != 0) {
        SDL_SetError("Photon: Can't get video mode list");
        return -1;
    }

    /* Current display dimension and bpp are no more valid */
    didata->current_mode.format = SDL_PIXELFORMAT_UNKNOWN;
    didata->current_mode.w = 0;
    didata->current_mode.h = 0;

    /* Check if custom refresh rate requested */
    if (didata->custom_refresh != 0) {
        refresh_rate = didata->custom_refresh;
    } else {
        refresh_rate = mode->refresh_rate;
    }

    /* Check if SDL GF driver needs to find appropriate refresh rate itself */
    if (refresh_rate == 0) {
        SDL_DisplayMode tempmode;

        /* Clear display mode structure */
        SDL_memset(&tempmode, 0x00, sizeof(SDL_DisplayMode));
        tempmode.refresh_rate = 0x0000FFFF;

        /* Check if window width and height matches one of our modes */
        for (it = 0; it < SDL_CurrentDisplay.num_display_modes; it++) {
            if ((SDL_CurrentDisplay.display_modes[it].w == mode->w) &&
                (SDL_CurrentDisplay.display_modes[it].h == mode->h) &&
                (SDL_CurrentDisplay.display_modes[it].format == mode->format))
            {
                /* Find the lowest refresh rate available */
                if (tempmode.refresh_rate >
                    SDL_CurrentDisplay.display_modes[it].refresh_rate) {
                    tempmode = SDL_CurrentDisplay.display_modes[it];
                }
            }
        }
        if (tempmode.refresh_rate != 0x0000FFFF) {
            refresh_rate = tempmode.refresh_rate;
        } else {
            /* Let video driver decide what to do with this */
            refresh_rate = 0;
        }
    }

    /* Check if SDL GF driver needs to check custom refresh rate */
    if (didata->custom_refresh != 0) {
        SDL_DisplayMode tempmode;

        /* Clear display mode structure */
        SDL_memset(&tempmode, 0x00, sizeof(SDL_DisplayMode));
        tempmode.refresh_rate = 0x0000FFFF;

        /* Check if window width and height matches one of our modes */
        for (it = 0; it < SDL_CurrentDisplay.num_display_modes; it++) {
            if ((SDL_CurrentDisplay.display_modes[it].w == mode->w) &&
                (SDL_CurrentDisplay.display_modes[it].h == mode->h) &&
                (SDL_CurrentDisplay.display_modes[it].format == mode->format))
            {
                /* Find the lowest refresh rate available */
                if (tempmode.refresh_rate >
                    SDL_CurrentDisplay.display_modes[it].refresh_rate) {
                    tempmode = SDL_CurrentDisplay.display_modes[it];
                }

                /* Check if requested refresh rate found */
                if (refresh_rate ==
                    SDL_CurrentDisplay.display_modes[it].refresh_rate) {
                    tempmode = SDL_CurrentDisplay.display_modes[it];
                    break;
                }
            }
        }
        if (tempmode.refresh_rate != 0x0000FFFF) {
            refresh_rate = tempmode.refresh_rate;
        } else {
            /* Let video driver decide what to do with this */
            refresh_rate = 0;
        }
    }

    /* Find photon's video mode number */
    for (it = 0; it < modes.num_modes; it++) {
        uint32_t jt;
        uint32_t foundrefresh;

        /* Get current video mode details */
        status = PgGetVideoModeInfo(modes.modes[it], &modeinfo);
        if (status != 0) {
            /* Maybe something wrong with this particular mode, skip it */
            continue;
        }
        if ((modeinfo.width == mode->w) && (modeinfo.height == mode->h) &&
            (modeinfo.type == photon_sdl_to_image_pixelformat(mode->format)))
        {
            /* Mode is found, find requested refresh rate, this case is for */
            /* video drivers, which provide non-generic video modes.        */
            jt = 0;
            foundrefresh = 0;
            do {
                if (modeinfo.refresh_rates[jt] != 0) {
                    if (modeinfo.refresh_rates[jt] == refresh_rate) {
                        foundrefresh = 1;
                        break;
                    }
                    jt++;
                } else {
                    break;
                }
            } while (1);
            if (foundrefresh != 0) {
                break;
            }
        }
    }

    /* Check if video mode has not been found */
    if (it == modes.num_modes) {
        SDL_SetError("Photon: Can't find appropriate video mode");
        return -1;
    }

    /* Fill mode settings */
    modesettings.flags = 0x00000000;
    modesettings.mode = modes.modes[it];
    modesettings.refresh = refresh_rate;

    /* Finally set new video mode */
    status = PgSetVideoMode(&modesettings);
    if (status != 0) {
        SDL_SetError("Photon: Can't set new video mode");
        return -1;
    }

    /* Store new video mode parameters */
    didata->current_mode = *mode;
    didata->current_mode.refresh_rate = refresh_rate;

    /* Get current video mode 2D capabilities for the renderer */
    didata->mode_2dcaps=0;
    if ((modeinfo.mode_capabilities2 & PgVM_MODE_CAP2_ALPHA_BLEND)==PgVM_MODE_CAP2_ALPHA_BLEND)
    {
       didata->mode_2dcaps|=SDL_VIDEO_PHOTON_CAP_ALPHA_BLEND;
    }
    if ((modeinfo.mode_capabilities2 & PgVM_MODE_CAP2_SCALED_BLIT)==PgVM_MODE_CAP2_SCALED_BLIT)
    {
       didata->mode_2dcaps|=SDL_VIDEO_PHOTON_CAP_SCALED_BLIT;
    }

    return 0;
}

int
photon_setdisplaypalette(_THIS, SDL_Palette * palette)
{
    SDL_DisplayData *didata =
        (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;

    /* Setting display palette operation has been failed */
    return -1;
}

int
photon_getdisplaypalette(_THIS, SDL_Palette * palette)
{
    SDL_DisplayData *didata =
        (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;

    /* Getting display palette operation has been failed */
    return -1;
}

int
photon_setdisplaygammaramp(_THIS, Uint16 * ramp)
{
    SDL_DisplayData *didata =
        (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;

    /* Setting display gamma ramp operation has been failed */
    return -1;
}

int
photon_getdisplaygammaramp(_THIS, Uint16 * ramp)
{
    /* Getting display gamma ramp operation has been failed */
    return -1;
}

int
photon_createwindow(_THIS, SDL_Window * window)
{
    SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
    SDL_DisplayData *didata =
        (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;
    SDL_WindowData *wdata;
    PhDim_t winsize;
    PhPoint_t winpos;
    PtArg_t winargs[32];
    uint32_t winargc = 0;
    int32_t status;
    PhRegion_t wregion;

    /* Allocate window internal data */
    wdata = (SDL_WindowData *) SDL_calloc(1, sizeof(SDL_WindowData));
    if (wdata == NULL) {
        SDL_OutOfMemory();
        return -1;
    }

    /* Setup driver data for this window */
    window->driverdata = wdata;

    /* Set initial window title */
    if (window->title != NULL) {
        PtSetArg(&winargs[winargc++], Pt_ARG_WINDOW_TITLE, window->title, 0);
    } else {
        PtSetArg(&winargs[winargc++], Pt_ARG_WINDOW_TITLE, "SDL unnamed application", 0);
    }

    /* TODO: handle SDL_WINDOW_INPUT_GRABBED */

    /* Disable default window filling on redraw */
    PtSetArg(&winargs[winargc++], Pt_ARG_BASIC_FLAGS, Pt_TRUE,
             Pt_BASIC_PREVENT_FILL);

    /* Reset default managed flags to disabled state */
    PtSetArg(&winargs[winargc++], Pt_ARG_WINDOW_MANAGED_FLAGS, Pt_FALSE,
             Ph_WM_APP_DEF_MANAGED);
    /* Set which events we will not handle, let WM to handle them */
    PtSetArg(&winargs[winargc++], Pt_ARG_WINDOW_MANAGED_FLAGS, Pt_TRUE,
             Ph_WM_BACKDROP | Ph_WM_TOFRONT | Ph_WM_COLLAPSE | Ph_WM_FFRONT |
             Ph_WM_FOCUS | Ph_WM_HELP | Ph_WM_HIDE | Ph_WM_MAX |
             Ph_WM_MENU | Ph_WM_MOVE | Ph_WM_RESTORE | Ph_WM_TASKBAR |
             Ph_WM_TOBACK | Ph_WM_RESIZE);

    /* Reset default notify events to disable */
    PtSetArg(&winargs[winargc++], Pt_ARG_WINDOW_NOTIFY_FLAGS, Pt_FALSE,
             Ph_WM_RESIZE | Ph_WM_CLOSE | Ph_WM_HELP);
    /* Set which events we will handle */
    PtSetArg(&winargs[winargc++], Pt_ARG_WINDOW_NOTIFY_FLAGS, Pt_TRUE,
             Ph_WM_CLOSE | Ph_WM_COLLAPSE | Ph_WM_FOCUS | Ph_WM_MAX |
             Ph_WM_MOVE | Ph_WM_RESIZE | Ph_WM_RESTORE | Ph_WM_HIDE);

    /* Borderless window can't be resizeable */
    if ((window->flags & SDL_WINDOW_BORDERLESS) == SDL_WINDOW_BORDERLESS) {
        window->flags &= ~(SDL_WINDOW_RESIZABLE);
    }

    /* Reset all default decorations */
    PtSetArg(&winargs[winargc++], Pt_ARG_WINDOW_RENDER_FLAGS, Pt_FALSE,
             Ph_WM_APP_DEF_RENDER);

    /* Fullscreen window has its own decorations */
    if ((window->flags & SDL_WINDOW_FULLSCREEN) == SDL_WINDOW_FULLSCREEN) {
        window->flags |= SDL_WINDOW_BORDERLESS;
        window->flags &= ~(SDL_WINDOW_RESIZABLE);
        PtSetArg(&winargs[winargc++], Pt_ARG_WINDOW_RENDER_FLAGS, Pt_TRUE,
                 Ph_WM_RENDER_CLOSE | Ph_WM_RENDER_TITLE);
    } else {
        uint32_t decorations =
            Ph_WM_RENDER_CLOSE | Ph_WM_RENDER_MENU | Ph_WM_RENDER_MIN |
            Ph_WM_RENDER_TITLE | Ph_WM_RENDER_MOVE;

        if ((window->flags & SDL_WINDOW_RESIZABLE) == SDL_WINDOW_RESIZABLE) {
            decorations |=
                Ph_WM_RENDER_BORDER | Ph_WM_RENDER_RESIZE | Ph_WM_RENDER_MAX;
            PtSetArg(&winargs[winargc++], Pt_ARG_RESIZE_FLAGS, Pt_TRUE,
                     Pt_RESIZE_XY_AS_REQUIRED);
        }
        if ((window->flags & SDL_WINDOW_BORDERLESS) != SDL_WINDOW_BORDERLESS) {
            decorations |= Ph_WM_RENDER_BORDER;
        }
        PtSetArg(&winargs[winargc++], Pt_ARG_WINDOW_RENDER_FLAGS, Pt_TRUE,
                 decorations);
    }

    /* Set initial window state */
    PtSetArg(&winargs[winargc++], Pt_ARG_WINDOW_STATE, Pt_FALSE,
             Ph_WM_STATE_ISFOCUS);
    PtSetArg(&winargs[winargc++], Pt_ARG_WINDOW_STATE, Pt_TRUE,
             Ph_WM_STATE_ISALTKEY);

    /* Special case, do not handle maximize events, if window can't be resized */
    if ((window->flags & SDL_WINDOW_RESIZABLE) != SDL_WINDOW_RESIZABLE)
    {
       PtSetArg(&winargs[winargc++], Pt_ARG_WINDOW_MANAGED_FLAGS, Pt_FALSE,
                Ph_WM_MAX | Ph_WM_RESTORE | Ph_WM_RESIZE);
    }

    /* Set window dimension */
    winsize.w = window->w;
    winsize.h = window->h;
    PtSetArg(&winargs[winargc++], Pt_ARG_DIM, &winsize, 0);

    /* Check if upper level requests WM to position window */
    if ((window->x == SDL_WINDOWPOS_UNDEFINED)
        && (window->y == SDL_WINDOWPOS_UNDEFINED)) {
        /* Do not set widget position, let WM to set it */
    } else {
        if (window->x == SDL_WINDOWPOS_UNDEFINED) {
            window->x = 0;
        }
        if (window->y == SDL_WINDOWPOS_UNDEFINED) {
            window->y = 0;
        }
        if (window->x == SDL_WINDOWPOS_CENTERED) {
            window->x = (didata->current_mode.w - window->w) / 2;
        }
        if (window->y == SDL_WINDOWPOS_CENTERED) {
            window->y = (didata->current_mode.h - window->h) / 2;
        }

        /* Now set window position */
        winpos.x = window->x;
        winpos.y = window->y;
        PtSetArg(&winargs[winargc++], Pt_ARG_POS, &winpos, 0);
    }

    /* Check if window must support OpenGL ES rendering */
    if ((window->flags & SDL_WINDOW_OPENGL) == SDL_WINDOW_OPENGL) {
#if defined(SDL_VIDEO_OPENGL_ES)
        EGLBoolean initstatus;

        /* Check if GF was initialized correctly */
        if (phdata->gfinitialized == SDL_FALSE) {
            SDL_SetError
                ("Photon: GF initialization failed, no OpenGL ES support");
            return -1;
        }

        /* Mark this window as OpenGL ES compatible */
        wdata->uses_gles = SDL_TRUE;

        /* Create connection to OpenGL ES */
        if (phdata->egldisplay == EGL_NO_DISPLAY) {
            phdata->egldisplay = eglGetDisplay(phdata->gfdev);
            if (phdata->egldisplay == EGL_NO_DISPLAY) {
                SDL_SetError("Photon: Can't get connection to OpenGL ES");
                return -1;
            }

            /* Initialize OpenGL ES library, ignore EGL version */
            initstatus = eglInitialize(phdata->egldisplay, NULL, NULL);
            if (initstatus != EGL_TRUE) {
                SDL_SetError("Photon: Can't init OpenGL ES library");
                return -1;
            }
        }

        /* Increment GL ES reference count usage */
        phdata->egl_refcount++;
#else
        SDL_SetError("Photon: OpenGL ES support is not compiled in");
        return -1;
#endif /* SDL_VIDEO_OPENGL_ES */
    }

    /* Finally create the window */
    wdata->window = PtCreateWidget(PtWindow, Pt_NO_PARENT, winargc, winargs);
    if (wdata->window == NULL) {
        SDL_SetError("Photon: Can't create window widget");
        SDL_free(wdata);
        return -1;
    }

    /* Show widget */
    status = PtRealizeWidget(wdata->window);
    if (status != 0) {
        SDL_SetError("Photon: Can't realize window widget");
        PtDestroyWidget(wdata->window);
        SDL_free(wdata);
        return;
    }

    /* Just created SDL window always gets focus */
    window->flags |= SDL_WINDOW_INPUT_FOCUS;

    /* Create window-specific cursor after creation */
    if (didata->cursor_visible == SDL_TRUE) {
        /* Setup cursor type. shape and default color */
        PtSetResource(wdata->window, Pt_ARG_CURSOR_COLOR,
                      Ph_CURSOR_DEFAULT_COLOR, 0);
        PtSetResource(wdata->window, Pt_ARG_CURSOR_TYPE, Ph_CURSOR_BITMAP, 0);
        PtSetResource(wdata->window, Pt_ARG_BITMAP_CURSOR, didata->cursor,
                      didata->cursor->hdr.len + sizeof(PhRegionDataHdr_t));
    } else {
        PtSetResource(wdata->window, Pt_ARG_CURSOR_TYPE, Ph_CURSOR_NONE, 0);
    }

    /* Set window region sensible to mouse motion events */
    status =
        PhRegionQuery(PtWidgetRid(wdata->window), &wregion, NULL, NULL, 0);
    if (status != 0) {
        SDL_SetError
            ("Photon: Can't set region sensivity to mouse motion events");
        PtDestroyWidget(wdata->window);
        SDL_free(wdata);
        return -1;
    }
    wregion.events_sense |=
        Ph_EV_PTR_MOTION_BUTTON | Ph_EV_PTR_MOTION_NOBUTTON;
    status = PhRegionChange(Ph_REGION_EV_SENSE, 0, &wregion, NULL, NULL);
    if (status < 0) {
        SDL_SetError("Photon: Can't change region sensivity");
        PtDestroyWidget(wdata->window);
        SDL_free(wdata);
        return -1;
    }

    /* Flush all widget operations again */
    PtFlush();

    /* By default last created window got a input focus */
    SDL_SetKeyboardFocus(0, window->id);

    /* Emit focus gained event, because photon is not sending it */
    SDL_OnWindowFocusGained(window);

    /* Window has been successfully created */
    return 0;
}

int
photon_createwindowfrom(_THIS, SDL_Window * window, const void *data)
{
    /* TODO: it is possible */

    /* Failed to create window from another window */
    return -1;
}

void
photon_setwindowtitle(_THIS, SDL_Window * window)
{
    SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;
    int32_t status;

    /* Set window title */
    if (window->title != NULL) {
        status =
            PtSetResource(wdata->window, Pt_ARG_WINDOW_TITLE, window->title,
                          0);
    } else {
        status = PtSetResource(wdata->window, Pt_ARG_WINDOW_TITLE, "", 0);
    }

    if (status != 0) {
        SDL_SetError("Photon: Can't set window title");
    }

    /* Flush all widget operations */
    PtFlush();
}

void
photon_setwindowicon(_THIS, SDL_Window * window, SDL_Surface * icon)
{
    SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;
    int32_t status;

    /* TODO: Use iconify ? */

    /* Flush all widget operations */
    PtFlush();
}

void
photon_setwindowposition(_THIS, SDL_Window * window)
{
    SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;
    SDL_DisplayData *didata =
        (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;
    PhPoint_t winpos;
    int32_t status;

    /* Check if upper level requests WM to position window */
    if ((window->x == SDL_WINDOWPOS_UNDEFINED)
        && (window->y == SDL_WINDOWPOS_UNDEFINED)) {
        /* Do not set widget position, let WM to set it */
    } else {
        if (window->x == SDL_WINDOWPOS_UNDEFINED) {
            window->x = 0;
        }
        if (window->y == SDL_WINDOWPOS_UNDEFINED) {
            window->y = 0;
        }
        if (window->x == SDL_WINDOWPOS_CENTERED) {
            window->x = (didata->current_mode.w - window->w) / 2;
        }
        if (window->y == SDL_WINDOWPOS_CENTERED) {
            window->y = (didata->current_mode.h - window->h) / 2;
        }

        /* Now set window position */
        winpos.x = window->x;
        winpos.y = window->y;
        status = PtSetResource(wdata->window, Pt_ARG_POS, &winpos, 0);
        if (status != 0) {
            SDL_SetError("Photon: Can't set window position");
        }
    }

    /* Flush all widget operations */
    PtFlush();
}

void
photon_setwindowsize(_THIS, SDL_Window * window)
{
    SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;
    PhDim_t winsize;
    int32_t status;

    winsize.w = window->w;
    winsize.h = window->h;

    status = PtSetResource(wdata->window, Pt_ARG_DIM, &winsize, 0);
    if (status != 0) {
        SDL_SetError("Photon: Can't set window size");
    }

    /* Flush all widget operations */
    PtFlush();
}

void
photon_showwindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;
    int32_t status;

    /* Bring focus to window and put it in front of others */
    PtWindowToFront(wdata->window);

    /* Flush all widget operations */
    PtFlush();
}

void
photon_hidewindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;
    PhWindowEvent_t winevent;
    int32_t status;

    SDL_memset(&winevent, 0x00, sizeof(PhWindowEvent_t));
    winevent.event_f = Ph_WM_HIDE;
    winevent.rid = PtWidgetRid(wdata->window);
    winevent.event_state = Ph_WM_EVSTATE_HIDE;

    status = PtForwardWindowEvent(&winevent);
    if (status != 0) {
        SDL_SetError("Photon: Can't hide window");
    }

    /* Flush all widget operations */
    PtFlush();
}

void
photon_raisewindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;
    PhWindowEvent_t winevent;
    int32_t status;

    SDL_memset(&winevent, 0x00, sizeof(PhWindowEvent_t));
    winevent.event_f = Ph_WM_HIDE;
    winevent.rid = PtWidgetRid(wdata->window);
    winevent.event_state = Ph_WM_EVSTATE_UNHIDE;

    status = PtForwardWindowEvent(&winevent);
    if (status != 0) {
        SDL_SetError("Photon: Can't hide window");
    }

    /* Flush all widget operations */
    PtFlush();
}

void
photon_maximizewindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;
    PhWindowEvent_t winevent;
    int32_t status;

    /* Flush all widget operations */
    PtFlush();
}

void
photon_minimizewindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;
    PhWindowEvent_t winevent;
    int32_t status;

    SDL_memset(&winevent, 0x00, sizeof(PhWindowEvent_t));
    winevent.event_f = Ph_WM_HIDE;
    winevent.rid = PtWidgetRid(wdata->window);
    winevent.event_state = Ph_WM_EVSTATE_HIDE;

    status = PtForwardWindowEvent(&winevent);
    if (status != 0) {
        SDL_SetError("Photon: Can't hide window");
    }

    /* Flush all widget operations */
    PtFlush();
}

void
photon_restorewindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;
    PhWindowEvent_t winevent;
    int32_t status;

    /* Flush all widget operations */
    PtFlush();
}

void
photon_setwindowgrab(_THIS, SDL_Window * window)
{
    SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;
    PhWindowEvent_t winevent;
    int32_t status;

    /* Flush all widget operations */
    PtFlush();
}

void
photon_destroywindow(_THIS, SDL_Window * window)
{
    SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
    SDL_DisplayData *didata =
        (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;
    SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;
    int32_t status;

    if (wdata != NULL) {
        status = PtDestroyWidget(wdata->window);
        if (status != 0) {
            SDL_SetError("Photon: Can't destroy window widget");
        }
        wdata->window = NULL;

#if defined(SDL_VIDEO_OPENGL_ES)
        if (phdata->gfinitialized == SDL_TRUE) {
            /* Destroy photon handle to GF surface */
            if (wdata->phsurface != NULL) {
                PhDCRelease(wdata->phsurface);
                wdata->phsurface=NULL;
            }

            /* Destroy OpenGL ES surface if it was created */
            if (wdata->gles_surface != EGL_NO_SURFACE) {
                eglDestroySurface(phdata->egldisplay, wdata->gles_surface);
            }

            /* Free OpenGL ES target surface */
            if (wdata->gfsurface != NULL) {
                gf_surface_free(wdata->gfsurface);
                wdata->gfsurface=NULL;
            }

            phdata->egl_refcount--;
            if (phdata->egl_refcount == 0) {
                /* Terminate connection to OpenGL ES */
                if (phdata->egldisplay != EGL_NO_DISPLAY) {
                    eglTerminate(phdata->egldisplay);
                    phdata->egldisplay = EGL_NO_DISPLAY;
                }
            }
        }
#endif /* SDL_VIDEO_OPENGL_ES */
    }

    /* Flush all widget operations */
    PtFlush();
}

/*****************************************************************************/
/* SDL Window Manager function                                               */
/*****************************************************************************/
SDL_bool
photon_getwindowwminfo(_THIS, SDL_Window * window, struct SDL_SysWMinfo *info)
{
    if (info->version.major <= SDL_MAJOR_VERSION) {
        return SDL_TRUE;
    } else {
        SDL_SetError("application not compiled with SDL %d.%d\n",
                     SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
        return SDL_FALSE;
    }

    /* Failed to get window manager information */
    return SDL_FALSE;
}

/*****************************************************************************/
/* SDL OpenGL/OpenGL ES functions                                            */
/*****************************************************************************/
int
photon_gl_loadlibrary(_THIS, const char *path)
{
#if defined(SDL_VIDEO_OPENGL_ES)
    SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;

    if (phdata->gfinitialized != SDL_TRUE) {
        SDL_SetError
            ("Photon: GF initialization failed, no OpenGL ES support");
        return -1;
    }

    /* Check if OpenGL ES library is specified for GF driver */
    if (path == NULL) {
        path = SDL_getenv("SDL_OPENGL_LIBRARY");
        if (path == NULL) {
            path = SDL_getenv("SDL_OPENGLES_LIBRARY");
        }
    }

    /* Check if default library loading requested */
    if (path == NULL) {
        /* Already linked with GF library which provides egl* subset of  */
        /* functions, use Common profile of OpenGL ES library by default */
        path = "/usr/lib/libGLES_CM.so.1";
    }

    /* Load dynamic library */
    _this->gl_config.dll_handle = SDL_LoadObject(path);
    if (!_this->gl_config.dll_handle) {
        /* Failed to load new GL ES library */
        SDL_SetError("Photon: Failed to locate OpenGL ES library");
        return -1;
    }

    /* Store OpenGL ES library path and name */
    SDL_strlcpy(_this->gl_config.driver_path, path,
                SDL_arraysize(_this->gl_config.driver_path));

    /* New OpenGL ES library is loaded */
    return 0;
#else
    SDL_SetError("Photon: OpenGL ES support is not compiled in");
    return -1;
#endif /* SDL_VIDEO_OPENGL_ES */
}

void *
photon_gl_getprocaddres(_THIS, const char *proc)
{
#if defined(SDL_VIDEO_OPENGL_ES)
    SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
    void *function_address;

    if (phdata->gfinitialized != SDL_TRUE) {
        SDL_SetError
            ("Photon: GF initialization failed, no OpenGL ES support");
        return NULL;
    }

    /* Try to get function address through the egl interface */
    function_address = eglGetProcAddress(proc);
    if (function_address != NULL) {
        return function_address;
    }

    /* Then try to get function in the OpenGL ES library */
    if (_this->gl_config.dll_handle) {
        function_address =
            SDL_LoadFunction(_this->gl_config.dll_handle, proc);
        if (function_address != NULL) {
            return function_address;
        }
    }

    /* Add emulated OpenGL ES 1.1 functions */
    if (SDL_strcmp(proc, "glTexParameteri") == 0) {
        return glTexParameteri;
    }
    if (SDL_strcmp(proc, "glTexParameteriv") == 0) {
        return glTexParameteriv;
    }
    if (SDL_strcmp(proc, "glColor4ub") == 0) {
        return glColor4ub;
    }

    /* Failed to get GL ES function address pointer */
    SDL_SetError("Photon: Cannot locate OpenGL ES function name");
    return NULL;
#else
    SDL_SetError("Photon: OpenGL ES support is not compiled in");
    return NULL;
#endif /* SDL_VIDEO_OPENGL_ES */
}

void
photon_gl_unloadlibrary(_THIS)
{
#if defined(SDL_VIDEO_OPENGL_ES)
    SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;

    if (phdata->gfinitialized == SDL_TRUE) {
        /* Unload OpenGL ES library */
        if (_this->gl_config.dll_handle) {
            SDL_UnloadObject(_this->gl_config.dll_handle);
            _this->gl_config.dll_handle = NULL;
        }
    } else {
        SDL_SetError
            ("Photon: GF initialization failed, no OpenGL ES support");
    }
#else
    SDL_SetError("Photon: OpenGL ES support is not compiled in");
    return;
#endif /* SDL_VIDEO_OPENGL_ES */
}

SDL_GLContext
photon_gl_createcontext(_THIS, SDL_Window * window)
{
#if defined(SDL_VIDEO_OPENGL_ES)
    SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
    SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;
    SDL_DisplayData *didata =
        (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;
    EGLBoolean status;
    int32_t gfstatus;
    EGLint configs;
    uint32_t attr_pos;
    EGLint attr_value;
    EGLint cit;

    /* Check if GF was initialized */
    if (phdata->gfinitialized != SDL_TRUE) {
        SDL_SetError
            ("Photon: GF initialization failed, no OpenGL ES support");
        return NULL;
    }

    /* Prepare attributes list to pass them to OpenGL ES egl interface */
    attr_pos = 0;
    wdata->gles_attributes[attr_pos++] = EGL_NATIVE_VISUAL_ID;
    wdata->gles_attributes[attr_pos++] =
        qnxgf_sdl_to_gf_pixelformat(didata->current_mode.format);
    wdata->gles_attributes[attr_pos++] = EGL_RED_SIZE;
    wdata->gles_attributes[attr_pos++] = _this->gl_config.red_size;
    wdata->gles_attributes[attr_pos++] = EGL_GREEN_SIZE;
    wdata->gles_attributes[attr_pos++] = _this->gl_config.green_size;
    wdata->gles_attributes[attr_pos++] = EGL_BLUE_SIZE;
    wdata->gles_attributes[attr_pos++] = _this->gl_config.blue_size;
    wdata->gles_attributes[attr_pos++] = EGL_ALPHA_SIZE;

    /* Setup alpha size in bits */
    if (_this->gl_config.alpha_size) {
        wdata->gles_attributes[attr_pos++] = _this->gl_config.alpha_size;
    } else {
        wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
    }

    /* Setup color buffer size */
    if (_this->gl_config.buffer_size) {
        wdata->gles_attributes[attr_pos++] = EGL_BUFFER_SIZE;
        wdata->gles_attributes[attr_pos++] = _this->gl_config.buffer_size;
    } else {
        wdata->gles_attributes[attr_pos++] = EGL_BUFFER_SIZE;
        wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
    }

    /* Setup depth buffer bits */
    wdata->gles_attributes[attr_pos++] = EGL_DEPTH_SIZE;
    if (_this->gl_config.depth_size)
    {
        wdata->gles_attributes[attr_pos++] = _this->gl_config.depth_size;
    }
    else
    {
        wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
    }

    /* Setup stencil bits */
    if (_this->gl_config.stencil_size) {
        wdata->gles_attributes[attr_pos++] = EGL_STENCIL_SIZE;
        wdata->gles_attributes[attr_pos++] = _this->gl_config.stencil_size;
    } else {
        wdata->gles_attributes[attr_pos++] = EGL_STENCIL_SIZE;
        wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
    }

    /* Set number of samples in multisampling */
    if (_this->gl_config.multisamplesamples) {
        wdata->gles_attributes[attr_pos++] = EGL_SAMPLES;
        wdata->gles_attributes[attr_pos++] =
            _this->gl_config.multisamplesamples;
    }

    /* Multisample buffers, OpenGL ES 1.0 spec defines 0 or 1 buffer */
    if (_this->gl_config.multisamplebuffers) {
        wdata->gles_attributes[attr_pos++] = EGL_SAMPLE_BUFFERS;
        wdata->gles_attributes[attr_pos++] =
            _this->gl_config.multisamplebuffers;
    }

    /* Finish attributes list */
    wdata->gles_attributes[attr_pos] = EGL_NONE;

    /* Request first suitable framebuffer configuration */
    status = eglChooseConfig(phdata->egldisplay, wdata->gles_attributes,
                             wdata->gles_configs, SDL_VIDEO_GF_OPENGLES_CONFS,
                             &configs);
    if (status != EGL_TRUE) {
        SDL_SetError
            ("Photon: Can't find closest configuration for OpenGL ES");
        return NULL;
    }

    /* Check if nothing has been found, try "don't care" settings */
    if (configs == 0) {
        int32_t it;
        int32_t jt;
        static const GLint depthbits[4] = { 32, 24, 16, EGL_DONT_CARE };

        for (it = 0; it < 4; it++) {
            for (jt = 16; jt >= 0; jt--) {
                /* Don't care about color buffer bits, use what exist */
                /* Replace previous set data with EGL_DONT_CARE       */
                attr_pos = 0;
                wdata->gles_attributes[attr_pos++] = EGL_NATIVE_VISUAL_ID;
                wdata->gles_attributes[attr_pos++] =
                    qnxgf_sdl_to_gf_pixelformat(didata->current_mode.format);
                wdata->gles_attributes[attr_pos++] = EGL_RED_SIZE;
                wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
                wdata->gles_attributes[attr_pos++] = EGL_GREEN_SIZE;
                wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
                wdata->gles_attributes[attr_pos++] = EGL_BLUE_SIZE;
                wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
                wdata->gles_attributes[attr_pos++] = EGL_ALPHA_SIZE;
                wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
                wdata->gles_attributes[attr_pos++] = EGL_BUFFER_SIZE;
                wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;

                /* Try to find requested or smallest depth */
                if (_this->gl_config.depth_size) {
                    wdata->gles_attributes[attr_pos++] = EGL_DEPTH_SIZE;
                    wdata->gles_attributes[attr_pos++] = depthbits[it];
                } else {
                    wdata->gles_attributes[attr_pos++] = EGL_DEPTH_SIZE;
                    wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
                }

                if (_this->gl_config.stencil_size) {
                    wdata->gles_attributes[attr_pos++] = EGL_STENCIL_SIZE;
                    wdata->gles_attributes[attr_pos++] = jt;
                } else {
                    wdata->gles_attributes[attr_pos++] = EGL_STENCIL_SIZE;
                    wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
                }

                wdata->gles_attributes[attr_pos++] = EGL_SAMPLES;
                wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
                wdata->gles_attributes[attr_pos++] = EGL_SAMPLE_BUFFERS;
                wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
                wdata->gles_attributes[attr_pos] = EGL_NONE;

                /* Request first suitable framebuffer configuration */
                status =
                    eglChooseConfig(phdata->egldisplay,
                                    wdata->gles_attributes,
                                    wdata->gles_configs,
                                    SDL_VIDEO_GF_OPENGLES_CONFS, &configs);
                if (status != EGL_TRUE) {
                    SDL_SetError
                        ("Photon: Can't find closest configuration for OpenGL ES");
                    return NULL;
                }
                if (configs != 0) {
                    break;
                }
            }
            if (configs != 0) {
                break;
            }
        }

        /* No available configs */
        if (configs == 0) {
            SDL_SetError
                ("Photon: Can't find any configuration for OpenGL ES");
            return NULL;
        }
    }

    /* Initialize config index */
    wdata->gles_config = 0;

    /* Now check each configuration to find out the best */
    for (cit = 0; cit < configs; cit++) {
        uint32_t stencil_found;
        uint32_t depth_found;
        EGLint   cur_depth;
        EGLint   cur_stencil;

        stencil_found = 0;
        depth_found = 0;

        if (_this->gl_config.stencil_size) {
            status =
                eglGetConfigAttrib(phdata->egldisplay,
                                   wdata->gles_configs[cit], EGL_STENCIL_SIZE,
                                   &cur_stencil);
            if (status == EGL_TRUE) {
                if (cur_stencil != 0) {
                    stencil_found = 1;
                }
            }
        } else {
            stencil_found = 1;
        }

        if (_this->gl_config.depth_size) {
            status =
                eglGetConfigAttrib(phdata->egldisplay,
                                   wdata->gles_configs[cit], EGL_DEPTH_SIZE,
                                   &cur_depth);
            if (status == EGL_TRUE) {
                if (cur_depth != 0) {
                    depth_found = 1;
                }
            }
        } else {
            depth_found = 1;
        }

        /* Exit from loop if found appropriate configuration */
        if ((depth_found != 0) && (stencil_found != 0)) {
            /* Store last satisfied configuration id */
            wdata->gles_config = cit;

            if (cur_depth==_this->gl_config.depth_size)
            {
                /* Exact match on depth bits */
                if (!_this->gl_config.stencil_size)
                {
                    /* Stencil is not required */
                    break;
                }
                else
                {
                    if (cur_stencil==_this->gl_config.stencil_size)
                    {
                        /* Exact match on stencil bits */
                        break;
                    }
                }
            }
        }
    }

    /* If best could not be found, use first or last satisfied */
    if ((cit == configs) && (wdata->gles_config==0)) {
        cit = 0;
        wdata->gles_config = cit;
    }

    /* Create OpenGL ES context */
    wdata->gles_context =
        eglCreateContext(phdata->egldisplay,
                         wdata->gles_configs[wdata->gles_config],
                         EGL_NO_CONTEXT, NULL);
    if (wdata->gles_context == EGL_NO_CONTEXT) {
        SDL_SetError("Photon: OpenGL ES context creation has been failed");
        return NULL;
    }

    /* Check if surface is exist */
    if (wdata->gfsurface != NULL) {
        gf_surface_free(wdata->gfsurface);
        wdata->gfsurface = NULL;
    }

    /* Create GF surface */
    gfstatus =
        gf_surface_create(&wdata->gfsurface, phdata->gfdev, window->w,
                          window->h,
                          qnxgf_sdl_to_gf_pixelformat(didata->current_mode.
                                                      format), NULL,
                          GF_SURFACE_CREATE_2D_ACCESSIBLE |
                          GF_SURFACE_CREATE_3D_ACCESSIBLE |
                          GF_SURFACE_CREATE_SHAREABLE);
    if (gfstatus != GF_ERR_OK) {
        eglDestroyContext(phdata->egldisplay, wdata->gles_context);
        wdata->gles_context = EGL_NO_CONTEXT;
        SDL_SetError("Photon: Can't create GF 3D surface (%08X)", gfstatus);
        return NULL;
    }

    /* Create pixmap 3D target surface */
    wdata->gles_surface =
        eglCreatePixmapSurface(phdata->egldisplay,
                               wdata->gles_configs[wdata->gles_config],
                               wdata->gfsurface, NULL);
    if (wdata->gles_surface == EGL_NO_SURFACE) {
        gf_surface_free(wdata->gfsurface);
        wdata->gfsurface=NULL;
        eglDestroyContext(phdata->egldisplay, wdata->gles_context);
        wdata->gles_context = EGL_NO_CONTEXT;
        SDL_SetError("Photon: Can't create EGL pixmap surface");
        return NULL;
    }

    /* Store last used context and surface */
    phdata->lgles_surface=wdata->gles_surface;
    phdata->lgles_context=wdata->gles_context;

    /* Make just created context current */
    status =
        eglMakeCurrent(phdata->egldisplay, wdata->gles_surface,
                       wdata->gles_surface, wdata->gles_context);
    if (status != EGL_TRUE) {
        /* Reset last used context and surface */
        phdata->lgles_surface=EGL_NO_SURFACE;
        phdata->lgles_context=EGL_NO_CONTEXT;

        /* Destroy OpenGL ES surface */
        eglDestroySurface(phdata->egldisplay, wdata->gles_surface);
        wdata->gles_surface=EGL_NO_SURFACE;
        gf_surface_free(wdata->gfsurface);
        wdata->gfsurface=NULL;
        eglDestroyContext(phdata->egldisplay, wdata->gles_context);
        wdata->gles_context = EGL_NO_CONTEXT;
        SDL_SetError("Photon: Can't set OpenGL ES context on creation");
        return NULL;
    }

    /* Setup into SDL internals state of OpenGL ES:  */
    /* it is accelerated or not                      */
    if ((didata->caps & SDL_PHOTON_ACCELERATED_3D) ==
        SDL_PHOTON_ACCELERATED_3D) {
        _this->gl_config.accelerated = 1;
    } else {
        _this->gl_config.accelerated = 0;
    }

    /* Always clear stereo enable, since OpenGL ES do not supports stereo */
    _this->gl_config.stereo = 0;

    /* Get back samples and samplebuffers configurations. Rest framebuffer */
    /* parameters could be obtained through the OpenGL ES API              */
    status =
        eglGetConfigAttrib(phdata->egldisplay,
                           wdata->gles_configs[wdata->gles_config],
                           EGL_SAMPLES, &attr_value);
    if (status == EGL_TRUE) {
        _this->gl_config.multisamplesamples = attr_value;
    }
    status =
        eglGetConfigAttrib(phdata->egldisplay,
                           wdata->gles_configs[wdata->gles_config],
                           EGL_SAMPLE_BUFFERS, &attr_value);
    if (status == EGL_TRUE) {
        _this->gl_config.multisamplebuffers = attr_value;
    }

    /* Get back stencil and depth buffer sizes */
    status =
        eglGetConfigAttrib(phdata->egldisplay,
                           wdata->gles_configs[wdata->gles_config],
                           EGL_DEPTH_SIZE, &attr_value);
    if (status == EGL_TRUE) {
        _this->gl_config.depth_size = attr_value;
    }
    status =
        eglGetConfigAttrib(phdata->egldisplay,
                           wdata->gles_configs[wdata->gles_config],
                           EGL_STENCIL_SIZE, &attr_value);
    if (status == EGL_TRUE) {
        _this->gl_config.stencil_size = attr_value;
    }

    /* Under Photon OpenGL ES output can't be double buffered */
    _this->gl_config.double_buffer = 0;

    /* Check if current device is not the same as target */
    if (phdata->current_device_id != didata->device_id) {
        /* Set target device as default for Pd and Pg functions */
        status = PdSetTargetDevice(NULL, phdata->rid[didata->device_id]);
        if (status != 0) {
            /* Destroy OpenGL ES surface */
            eglDestroySurface(phdata->egldisplay, wdata->gles_surface);
            wdata->gles_surface=EGL_NO_SURFACE;
            gf_surface_free(wdata->gfsurface);
            wdata->gfsurface=NULL;
            eglDestroyContext(phdata->egldisplay, wdata->gles_context);
            wdata->gles_context = EGL_NO_CONTEXT;
            SDL_SetError("Photon: Can't set default target device\n");
            return NULL;
        }
        phdata->current_device_id = didata->device_id;
    }

    wdata->phsurface = PdCreateOffscreenContextGF(wdata->gfsurface);
    if (wdata->phsurface == NULL) {
        /* Destroy OpenGL ES surface */
        eglDestroySurface(phdata->egldisplay, wdata->gles_surface);
        wdata->gles_surface=EGL_NO_SURFACE;
        gf_surface_free(wdata->gfsurface);
        wdata->gfsurface=NULL;
        eglDestroyContext(phdata->egldisplay, wdata->gles_context);
        wdata->gles_context = EGL_NO_CONTEXT;
        SDL_SetError("Photon: Can't bind GF surface to Photon\n");
        return NULL;
    }

    /* GL ES context was successfully created */
    return wdata->gles_context;
#else
    SDL_SetError("Photon: OpenGL ES support is not compiled in");
    return NULL;
#endif /* SDL_VIDEO_OPENGL_ES */
}

int
photon_gl_makecurrent(_THIS, SDL_Window * window, SDL_GLContext context)
{
#if defined(SDL_VIDEO_OPENGL_ES)
    SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
    SDL_WindowData *wdata;
    EGLBoolean status;

    if (phdata->gfinitialized != SDL_TRUE) {
        SDL_SetError
            ("Photon: GF initialization failed, no OpenGL ES support");
        return -1;
    }

    if ((window == NULL) && (context == NULL)) {
        /* Reset last used context and surface */
        phdata->lgles_surface=EGL_NO_SURFACE;
        phdata->lgles_context=EGL_NO_CONTEXT;

        /* Unset current context */
        status =
            eglMakeCurrent(phdata->egldisplay, EGL_NO_SURFACE, EGL_NO_SURFACE,
                           EGL_NO_CONTEXT);
        if (status != EGL_TRUE) {
            /* Failed to set current GL ES context */
            SDL_SetError("Photon: Can't empty current OpenGL ES context");
            return -1;
        }
    } else {
        wdata = (SDL_WindowData *) window->driverdata;
        if (wdata->gles_surface == EGL_NO_SURFACE) {
            SDL_SetError
                ("Photon: OpenGL ES surface is not initialized for this window");
            return -1;
        }
        if (wdata->gles_context == EGL_NO_CONTEXT) {
            SDL_SetError
                ("Photon: OpenGL ES context is not initialized for this window");
            return -1;
        }
        if (wdata->gles_context != context) {
            SDL_SetError
                ("Photon: OpenGL ES context is not belong to this window");
            return -1;
        }

        /* Store last set surface and context */
        phdata->lgles_surface=wdata->gles_surface;
        phdata->lgles_context=wdata->gles_context;

        /* Set new current context */
        status =
            eglMakeCurrent(phdata->egldisplay, wdata->gles_surface,
                           wdata->gles_surface, wdata->gles_context);
        if (status != EGL_TRUE) {
            /* Reset last used context and surface */
            phdata->lgles_surface=EGL_NO_SURFACE;
            phdata->lgles_context=EGL_NO_CONTEXT;
            /* Failed to set current GL ES context */
            SDL_SetError("Photon: Can't set OpenGL ES context");
            return -1;
        }
    }

    return 0;
#else
    SDL_SetError("Photon: OpenGL ES support is not compiled in");
    return -1;
#endif /* SDL_VIDEO_OPENGL_ES */
}

int
photon_gl_setswapinterval(_THIS, int interval)
{
#if defined(SDL_VIDEO_OPENGL_ES)
    SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
    EGLBoolean status;

    if (phdata->gfinitialized != SDL_TRUE) {
        SDL_SetError
            ("Photon: GF initialization failed, no OpenGL ES support");
        return -1;
    }

    /* Check if OpenGL ES connection has been initialized */
    if (phdata->egldisplay != EGL_NO_DISPLAY) {
        /* Set swap OpenGL ES interval */
        status = eglSwapInterval(phdata->egldisplay, interval);
        if (status == EGL_TRUE) {
            /* Return success to upper level */
            phdata->swapinterval = interval;
            return 0;
        }
    }

    /* Failed to set swap interval */
    SDL_SetError("Photon: Cannot set swap interval");
    return -1;
#else
    SDL_SetError("Photon: OpenGL ES support is not compiled in");
    return -1;
#endif /* SDL_VIDEO_OPENGL_ES */
}

int
photon_gl_getswapinterval(_THIS)
{
#if defined(SDL_VIDEO_OPENGL_ES)
    SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;

    if (phdata->gfinitialized != SDL_TRUE) {
        SDL_SetError
            ("Photon: GF initialization failed, no OpenGL ES support");
        return -1;
    }

    /* Return default swap interval value */
    return phdata->swapinterval;
#else
    SDL_SetError("Photon: OpenGL ES support is not compiled in");
    return -1;
#endif /* SDL_VIDEO_OPENGL_ES */
}

void
photon_gl_swapwindow(_THIS, SDL_Window * window)
{
#if defined(SDL_VIDEO_OPENGL_ES)
    SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
    SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;
    SDL_DisplayData *didata =
        (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;
    PhRect_t dst_rect;
    PhRect_t src_rect;
    int32_t status;

    if (phdata->gfinitialized != SDL_TRUE) {
        SDL_SetError
            ("Photon: GF initialization failed, no OpenGL ES support");
        return;
    }

    if (wdata->phsurface==NULL) {
        SDL_SetError
            ("Photon: Photon OpenGL ES surface is not initialized");
        return;
    }

    /* Many applications do not uses glFinish(), so we call it for them */
    glFinish();

    /* Wait until OpenGL ES rendering is completed */
    eglWaitGL();

    /* Wait for VSYNC manually, if it was enabled */
    if (phdata->swapinterval != 0) {
        /* Wait for VSYNC, we use GF function, since Photon requires */
        /* to enter to the Direct mode to call PgWaitVSync()         */
        gf_display_wait_vsync(didata->display);
    }

    /* Set blit area */
    dst_rect = *PtGetCanvas(wdata->window);
    src_rect.ul.x = 0;
    src_rect.ul.y = 0;
    src_rect.lr.x = window->w - 1;
    src_rect.lr.y = window->h - 1;

    /* Check if current device is not the same as target */
    if (phdata->current_device_id != didata->device_id) {
        /* Set target device as default for Pd and Pg functions */
        status = PdSetTargetDevice(NULL, phdata->rid[didata->device_id]);
        if (status != 0) {
            SDL_SetError("Photon: Can't set default target device\n");
            return;
        }
        phdata->current_device_id = didata->device_id;
    }

    /* Blit OpenGL ES pixmap surface directly to window region */
    PgFFlush(Ph_START_DRAW);
    PgSetRegionCx(PhDCGetCurrent(), PtWidgetRid(wdata->window));
    PgClearTranslationCx(PgGetGCCx(PhDCGetCurrent()));
    PgContextBlit(wdata->phsurface, &src_rect, NULL, &dst_rect);
    PgFFlush(Ph_DONE_DRAW);
    PgWaitHWIdle();

    eglSwapBuffers(phdata->egldisplay, wdata->gles_surface);
#else
    SDL_SetError("Photon: OpenGL ES support is not compiled in");
    return;
#endif /* SDL_VIDEO_OPENGL_ES */
}

void
photon_gl_deletecontext(_THIS, SDL_GLContext context)
{
#if defined(SDL_VIDEO_OPENGL_ES)
    SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
    EGLBoolean status;

    if (phdata->gfinitialized != SDL_TRUE) {
        SDL_SetError
            ("Photon: GF initialization failed, no OpenGL ES support");
        return;
    }

    /* Check if OpenGL ES connection has been initialized */
    if (phdata->egldisplay != EGL_NO_DISPLAY) {
        if (context != EGL_NO_CONTEXT) {
            /* Check if we are destroying current active context */
            if (phdata->lgles_context==context) {
                /* Release current context */
                eglMakeCurrent(phdata->egldisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                phdata->lgles_context=EGL_NO_CONTEXT;
                phdata->lgles_surface=EGL_NO_SURFACE;
            }
            status = eglDestroyContext(phdata->egldisplay, context);
            if (status != EGL_TRUE) {
                /* Error during OpenGL ES context destroying */
                SDL_SetError("Photon: OpenGL ES context destroy error");
                return;
            }
        }
    }

    return;
#else
    SDL_SetError("Photon: OpenGL ES support is not compiled in");
    return;
#endif /* SDL_VIDEO_OPENGL_ES */
}

/* Helper function, which re-creates surface, not an API */
int photon_gl_recreatesurface(_THIS, SDL_Window * window, uint32_t width, uint32_t height)
{
#if defined(SDL_VIDEO_OPENGL_ES)
    SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
    SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;
    SDL_DisplayData *didata =
        (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;
    SDL_bool makecurrent=SDL_FALSE;
    int32_t gfstatus;

    /* Check if context has been initialized */
    if (wdata->gles_context == EGL_NO_CONTEXT) {
        /* If no, abort surface re-creating */
        return -1;
    }

    /* Check if last used surface the same as one which must be re-created */
    if (phdata->lgles_surface == wdata->gles_surface) {
        makecurrent=SDL_TRUE;
        /* Flush all current drawings */
        glFinish();
        /* Wait until OpenGL ES rendering is completed */
        eglWaitGL();
        /* Release current context */
        eglMakeCurrent(phdata->egldisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        phdata->lgles_surface=EGL_NO_SURFACE;
    }

    /* Check if we need to destroy previous surface */
    if (wdata->gles_surface != EGL_NO_SURFACE) {
        /* Destroy photon handle to GF surface */
        if (wdata->phsurface != NULL) {
            PhDCRelease(wdata->phsurface);
            wdata->phsurface=NULL;
        }

        /* Destroy previous surface */
        eglDestroySurface(phdata->egldisplay, wdata->gles_surface);

        /* Set surface to uninitialized */
        wdata->gles_surface = EGL_NO_SURFACE;

        if (wdata->gfsurface!=NULL) {
           gf_surface_free(wdata->gfsurface);
           wdata->gfsurface=NULL;
        }
    }

    /* Create new GF surface */
    gfstatus =
        gf_surface_create(&wdata->gfsurface, phdata->gfdev, width,
                          height,
                          qnxgf_sdl_to_gf_pixelformat(didata->current_mode.
                                                      format), NULL,
                          GF_SURFACE_CREATE_2D_ACCESSIBLE |
                          GF_SURFACE_CREATE_3D_ACCESSIBLE |
                          GF_SURFACE_CREATE_SHAREABLE);
    if (gfstatus != GF_ERR_OK) {
        SDL_SetError("Photon: Can't create GF 3D surface (%08X)", gfstatus);
        return -1;
    }

    /* Create new pixmap 3D target surface */
    wdata->gles_surface =
        eglCreatePixmapSurface(phdata->egldisplay,
                               wdata->gles_configs[wdata->gles_config],
                               wdata->gfsurface, NULL);
    if (wdata->gles_surface == EGL_NO_SURFACE) {
        gf_surface_free(wdata->gfsurface);
        wdata->gfsurface=NULL;
        SDL_SetError("Photon: Can't create EGL pixmap surface");
        return -1;
    }

    wdata->phsurface = PdCreateOffscreenContextGF(wdata->gfsurface);
    if (wdata->phsurface == NULL) {
        /* Destroy OpenGL ES surface */
        eglDestroySurface(phdata->egldisplay, wdata->gles_surface);
        wdata->gles_surface=EGL_NO_SURFACE;
        gf_surface_free(wdata->gfsurface);
        wdata->gfsurface=NULL;
        SDL_SetError("Photon: Can't bind GF surface to Photon\n");
        return -1;
    }

    /* Check if we need to set this surface and context as current */
    if (makecurrent == SDL_TRUE) {
        return photon_gl_makecurrent(_this, window, wdata->gles_context);
    } else {
        return 0;
    }
#else
    /* Do nothing, if OpenGL ES support is not compiled in */
    return 0;
#endif /* SDL_VIDEO_OPENGL_ES */
}

/*****************************************************************************/
/* SDL Event handling function                                               */
/*****************************************************************************/
void
photon_pumpevents(_THIS)
{
    uint8_t eventbuffer[SDL_VIDEO_PHOTON_EVENT_SIZE];
    PhEvent_t *event = (PhEvent_t *) eventbuffer;
    int32_t status;
    uint32_t finish = 0;
    uint32_t it;
    SDL_Window *window;
    SDL_WindowData *wdata;

    do {
        status = PhEventPeek(event, SDL_VIDEO_PHOTON_EVENT_SIZE);
        switch (status) {
        case Ph_RESIZE_MSG:
            {
                SDL_SetError("Photon: Event size too much for buffer");
                return;
            }
            break;
        case Ph_EVENT_MSG:
            {
                /* Find a window, to which this handle destinated */
                status = 0;
                for (it = 0; it < SDL_CurrentDisplay.num_windows; it++) {
                    wdata =
                        (SDL_WindowData *) SDL_CurrentDisplay.windows[it].
                        driverdata;

                    /* Find the proper window */
                    if (wdata->window != NULL) {
                        if (PtWidgetRid(wdata->window) ==
                            event->collector.rid) {
                            window =
                                (SDL_Window *) & SDL_CurrentDisplay.
                                windows[it];
                            status = 1;
                            break;
                        }
                    } else {
                        continue;
                    }
                }
                if (status == 0) {
                    window = NULL;
                    wdata = NULL;
                }

                /* Event is ready */
                switch (event->type) {
                case Ph_EV_BOUNDARY:
                    {
                        switch (event->subtype) {
                        case Ph_EV_PTR_ENTER:
                            {
                                /* Mouse cursor over handled window */
                                if (window != NULL) {
                                    SDL_SendWindowEvent(window->id,
                                                        SDL_WINDOWEVENT_ENTER,
                                                        0, 0);
                                    SDL_SetMouseFocus(0, window->id);
                                }
                            }
                            break;
                        case Ph_EV_PTR_LEAVE:
                            {
                                /* Mouse cursor out of handled window */
                                if (window != NULL) {
                                    SDL_SendWindowEvent(window->id,
                                                        SDL_WINDOWEVENT_LEAVE,
                                                        0, 0);
                                }
                            }
                            break;
                        }
                    }
                    break;
                case Ph_EV_PTR_MOTION_BUTTON:
                case Ph_EV_PTR_MOTION_NOBUTTON:
                    {
                        PhPointerEvent_t *pevent = NULL;
                        PhRect_t *prects = NULL;

                        /* Get event data */
                        pevent = PhGetData(event);
                        /* Get associated event rectangles */
                        prects = PhGetRects(event);
                        if ((pevent != NULL) && (prects != NULL)) {
                            SDL_SendMouseMotion(0, 0, prects->ul.x,
                                                prects->ul.y, 0);
                        }
                    }
                    break;
                case Ph_EV_BUT_PRESS:
                    {
                        /* Button press event */
                        PhPointerEvent_t *pevent = NULL;
                        uint32_t sdlbutton = 0x00000000;

                        /* Get event data */
                        pevent = PhGetData(event);
                        if (pevent != NULL) {
                            for (it = 0; it < sizeof(pevent->buttons) * 8;
                                 it++) {
                                if ((pevent->buttons & (0x0001 << it)) ==
                                    (0x0001 << it)) {
                                    switch (it) {
                                    case 0:
                                        {
                                            sdlbutton = SDL_BUTTON_RIGHT;
                                        }
                                        break;
                                    case 1:
                                        {
                                            sdlbutton = SDL_BUTTON_MIDDLE;
                                        }
                                        break;
                                    case 2:
                                        {
                                            sdlbutton = SDL_BUTTON_LEFT;
                                        }
                                        break;
                                    default:
                                        {
                                            sdlbutton = it + 1;
                                        }
                                        break;
                                    }
                                    SDL_SendMouseButton(0, SDL_PRESSED,
                                                        sdlbutton);
                                }
                            }
                        }
                    }
                    break;
                case Ph_EV_BUT_RELEASE:
                    {
                        /* Button press event */
                        PhPointerEvent_t *pevent = NULL;
                        uint32_t sdlbutton = 0x00000000;

                        /* Get event data */
                        pevent = PhGetData(event);
                        if (pevent != NULL) {
                            for (it = 0; it < sizeof(pevent->buttons) * 8;
                                 it++) {
                                if ((pevent->buttons & (0x0001 << it)) ==
                                    (0x0001 << it)) {
                                    switch (it) {
                                    case 0:
                                        {
                                            sdlbutton = SDL_BUTTON_RIGHT;
                                        }
                                        break;
                                    case 1:
                                        {
                                            sdlbutton = SDL_BUTTON_MIDDLE;
                                        }
                                        break;
                                    case 2:
                                        {
                                            sdlbutton = SDL_BUTTON_LEFT;
                                        }
                                        break;
                                    default:
                                        {
                                            sdlbutton = it + 1;
                                        }
                                        break;
                                    }
                                }
                            }
                        }

                        switch (event->subtype) {
                        case Ph_EV_RELEASE_REAL:
                            {
                                /* Real release button event */
                                SDL_SendMouseButton(0, SDL_RELEASED,
                                                    sdlbutton);
                            }
                            break;
                        case Ph_EV_RELEASE_PHANTOM:
                            {
                                /* We will get phantom button release */
                                /* event in case if it was unpressed  */
                                /* outside of window                  */
                                if (window != NULL) {
                                    if ((window->
                                         flags & SDL_WINDOW_MOUSE_FOCUS) !=
                                        SDL_WINDOW_MOUSE_FOCUS) {
                                        /* Send phantom button release */
                                        SDL_SendMouseButton(0, SDL_RELEASED,
                                                            sdlbutton);
                                    }
                                }
                            }
                            break;
                        }
                    }
                    break;
                case Ph_EV_EXPOSE:
                    {
                        switch (event->subtype) {
                        case Ph_NORMAL_EXPOSE:
                            {
                                PhRect_t *rects = NULL;

                                /* Get array of rectangles to be updated */
                                rects = PhGetRects(event);
                                if (rects == NULL) {
                                    break;
                                }

                                /* Check if expose come to one of the our windows */
                                if ((wdata != NULL) && (window != NULL)) {
                                    /* Check if window uses OpenGL ES */
                                    if (wdata->uses_gles == SDL_TRUE) {
                                        #if defined(SDL_VIDEO_OPENGL_ES)
                                        /* Cycle through each rectangle */
                                        for (it = 0; it < event->num_rects; it++) {
                                            /* Blit OpenGL ES pixmap surface directly to window region */
                                            PgFFlush(Ph_START_DRAW);
                                            PgSetRegionCx(PhDCGetCurrent(),
                                                          PtWidgetRid(wdata->
                                                                      window));
                                            PgClearTranslationCx(PgGetGCCx
                                                                 (PhDCGetCurrent
                                                                  ()));
                                            PgContextBlit(wdata->phsurface,
                                                          &rects[it], NULL,
                                                          &rects[it]);
                                            PgFFlush(Ph_DONE_DRAW);
                                            PgWaitHWIdle();
                                        }
                                        #endif /* SDL_VIDEO_OPENGL_ES */
                                    } else {
                                        /* Cycle through each rectangle */
                                        for (it = 0; it < event->num_rects;
                                             it++) {
                                            /* Blit 2D pixmap surface directly to window region */
                                            _photon_update_rectangles(window->renderer, &rects[it]);
                                        }
                                        PgFlush();
                                        PgWaitHWIdle();
                                    }
                                }

                                /* Flush all blittings */
                                PgFlush();
                            }
                            break;
                        case Ph_CAPTURE_EXPOSE:
                            {
                                /* Check if expose come to one of the our windows */
                                if ((wdata != NULL) && (window != NULL)) {
                                    /* Check if window uses OpenGL ES */
                                    if (wdata->uses_gles == SDL_TRUE) {
                                        PhRect_t dst_rect;
                                        PhRect_t src_rect;

                                        /* Set blit area */
                                        dst_rect =
                                            *PtGetCanvas(wdata->window);
                                        src_rect.ul.x = 0;
                                        src_rect.ul.y = 0;
                                        src_rect.lr.x = window->w - 1;
                                        src_rect.lr.y = window->h - 1;

                                        #if defined(SDL_VIDEO_OPENGL_ES)
                                        /* We need to redraw entire window */
                                        PgFFlush(Ph_START_DRAW);
                                        PgSetRegionCx(PhDCGetCurrent(),
                                                      PtWidgetRid(wdata->
                                                                  window));
                                        PgClearTranslationCx(PgGetGCCx
                                                             (PhDCGetCurrent
                                                              ()));
                                        PgContextBlit(wdata->phsurface,
                                                      &src_rect, NULL,
                                                      &dst_rect);
                                        PgFFlush(Ph_DONE_DRAW);
                                        PgWaitHWIdle();
                                        #endif /* SDL_VIDEO_OPENGL_ES */
                                    } else {
                                        PhRect_t rect;

                                        /* We need to redraw entire window */
                                        rect.ul.x = 0;
                                        rect.ul.y = 0;
                                        rect.lr.x = window->w - 1;
                                        rect.lr.y = window->h - 1;

                                        /* Blit 2D pixmap surface directly to window region */
                                        PgFFlush(Ph_START_DRAW);
                                        _photon_update_rectangles(window->renderer, &rect);
                                        PgFFlush(Ph_DONE_DRAW);
                                        PgWaitHWIdle();
                                    }
                                }
                            }
                            break;
                        case Ph_GRAPHIC_EXPOSE:
                            {
                                /* TODO: What this event means ? */
                            }
                            break;
                        }
                    }
                    break;
                case Ph_EV_INFO:
                    {
                        switch (event->subtype)
                        {
                           case Ph_OFFSCREEN_INVALID:
                                {
                                   uint32_t* type;

                                   type = PhGetData(event);
                                   switch (*type)
                                   {
                                      case Pg_VIDEO_MODE_SWITCHED:
                                      case Pg_ENTERED_DIRECT:
                                      case Pg_EXITED_DIRECT:
                                      case Pg_DRIVER_STARTED:
                                           {
                                               /* TODO: */
                                               /* We must tell the renderer, that it have */
                                               /* to recreate all surfaces                */
                                           }
                                           break;
                                      default:
                                           {
                                           }
                                           break;
                                   }
                                }
                                break;
                           default:
                                break;
                        }
                    }
                    break;
                case Ph_EV_KEY:
                    {
                        PhKeyEvent_t *keyevent = NULL;
                        SDL_scancode scancode = SDL_SCANCODE_UNKNOWN;
                        SDL_bool pressed = SDL_FALSE;

                        keyevent = PhGetData(event);
                        if (keyevent == NULL) {
                            break;
                        }

                        /* Check if key is repeated */
                        if ((keyevent->key_flags & Pk_KF_Key_Repeat) ==
                            Pk_KF_Key_Repeat) {
                            /* Ignore such events */
                            break;
                        }

                        /* Check if key has its own scancode */
                        if ((keyevent->key_flags & Pk_KF_Scan_Valid) ==
                            Pk_KF_Scan_Valid) {
                            if ((keyevent->key_flags & Pk_KF_Key_Down) ==
                                Pk_KF_Key_Down) {
                                pressed = SDL_TRUE;
                            } else {
                                pressed = SDL_FALSE;
                            }
                            scancode =
                                photon_to_sdl_keymap(keyevent->key_scan);

                            /* Add details for the pressed key */
                            if ((keyevent->key_flags & Pk_KF_Cap_Valid) ==
                                Pk_KF_Cap_Valid) {
                                switch (keyevent->key_cap) {
                                case Pk_Hyper_R:       /* Right windows flag key */
                                    scancode = SDL_SCANCODE_RGUI;
                                    break;
                                case Pk_Control_R:     /* Right Ctrl key */
                                    scancode = SDL_SCANCODE_RCTRL;
                                    break;
                                case Pk_Alt_R: /* Right Alt key */
                                    scancode = SDL_SCANCODE_RALT;
                                    break;
                                case Pk_Up:    /* Up key but with invalid scan */
                                    if (scancode != SDL_SCANCODE_UP) {
                                        /* This is a mouse wheel event */
                                        SDL_SendMouseWheel(0, 0, 1);
                                        return;
                                    }
                                    break;
                                case Pk_KP_8:  /* Up arrow or 8 on keypad */
                                    scancode = SDL_SCANCODE_KP_8;
                                    break;
                                case Pk_Down:  /* Down key but with invalid scan */
                                    if (scancode != SDL_SCANCODE_DOWN) {
                                        /* This is a mouse wheel event */
                                        SDL_SendMouseWheel(0, 0, -1);
                                        return;
                                    }
                                    break;
                                case Pk_KP_2:  /* Down arrow or 2 on keypad */
                                    scancode = SDL_SCANCODE_KP_2;
                                    break;
                                case Pk_Left:  /* Left arrow key */
                                    scancode = SDL_SCANCODE_LEFT;
                                    break;
                                case Pk_KP_4:  /* Left arrow or 4 on keypad */
                                    scancode = SDL_SCANCODE_KP_4;
                                    break;
                                case Pk_Right: /* Right arrow key */
                                    scancode = SDL_SCANCODE_RIGHT;
                                    break;
                                case Pk_KP_6:  /* Right arrow or 6 on keypad */
                                    scancode = SDL_SCANCODE_KP_6;
                                    break;
                                case Pk_Insert:        /* Insert key */
                                    scancode = SDL_SCANCODE_INSERT;
                                    break;
                                case Pk_KP_0:  /* Insert or 0 on keypad */
                                    scancode = SDL_SCANCODE_KP_0;
                                    break;
                                case Pk_Home:  /* Home key */
                                    scancode = SDL_SCANCODE_HOME;
                                    break;
                                case Pk_KP_7:  /* Home or 7 on keypad */
                                    scancode = SDL_SCANCODE_KP_7;
                                    break;
                                case Pk_Pg_Up: /* PageUp key */
                                    scancode = SDL_SCANCODE_PAGEUP;
                                    break;
                                case Pk_KP_9:  /* PgUp or 9 on keypad */
                                    scancode = SDL_SCANCODE_KP_9;
                                    break;
                                case Pk_Delete:        /* Delete key */
                                    scancode = SDL_SCANCODE_DELETE;
                                    break;
                                case Pk_KP_Decimal:    /* Del or . on keypad */
                                    scancode = SDL_SCANCODE_KP_PERIOD;
                                    break;
                                case Pk_End:   /* End key */
                                    scancode = SDL_SCANCODE_END;
                                    break;
                                case Pk_KP_1:  /* End or 1 on keypad */
                                    scancode = SDL_SCANCODE_KP_1;
                                    break;
                                case Pk_Pg_Down:       /* PageDown key */
                                    scancode = SDL_SCANCODE_PAGEDOWN;
                                    break;
                                case Pk_KP_3:  /* PgDn or 3 on keypad */
                                    scancode = SDL_SCANCODE_KP_3;
                                    break;
                                case Pk_KP_5:  /* 5 on keypad */
                                    scancode = SDL_SCANCODE_KP_5;
                                    break;
                                case Pk_KP_Enter:
                                    scancode = SDL_SCANCODE_KP_ENTER;
                                    break;
                                case Pk_KP_Add:
                                    scancode = SDL_SCANCODE_KP_PLUS;
                                    break;
                                case Pk_KP_Subtract:
                                    scancode = SDL_SCANCODE_KP_MINUS;
                                    break;
                                case Pk_KP_Multiply:
                                    scancode = SDL_SCANCODE_KP_MULTIPLY;
                                    break;
                                case Pk_KP_Divide:
                                    scancode = SDL_SCANCODE_KP_DIVIDE;
                                    break;
                                case Pk_Pause:
                                    scancode = SDL_SCANCODE_PAUSE;
                                    break;
                                }
                            }

                            /* Finally check if scancode has been decoded */
                            if (scancode == SDL_SCANCODE_UNKNOWN) {
                                /* Something was pressed, which is not supported */
                                break;
                            }

                            /* Report pressed/released key to SDL */
                            if (pressed == SDL_TRUE) {
                                SDL_SendKeyboardKey(0, SDL_PRESSED, scancode);
                            } else {
                                SDL_SendKeyboardKey(0, SDL_RELEASED,
                                                    scancode);
                            }

                            /* Photon doesn't send a release event for PrnScr key */
                            if ((scancode == SDL_SCANCODE_PRINTSCREEN)
                                && (pressed)) {
                                SDL_SendKeyboardKey(0, SDL_RELEASED,
                                                    scancode);
                            }
                        }
                    }
                    break;
                case Ph_EV_SERVICE:
                    {
                    }
                    break;
                case Ph_EV_SYSTEM:
                    {
                    }
                    break;
                case Ph_EV_WM:
                    {
                        PhWindowEvent_t *wmevent = NULL;

                        /* Get associated event data */
                        wmevent = PhGetData(event);
                        if (wmevent == NULL) {
                            break;
                        }

                        switch (wmevent->event_f) {
                        case Ph_WM_CLOSE:
                            {
                                if (window != NULL) {
                                    SDL_SendWindowEvent(window->id,
                                                        SDL_WINDOWEVENT_CLOSE,
                                                        0, 0);
                                }
                            }
                            break;
                        case Ph_WM_FOCUS:
                            {
                                if (wmevent->event_state ==
                                    Ph_WM_EVSTATE_FOCUS) {
                                    if (window != NULL) {
                                        PhRegion_t wregion;

                                        SDL_SendWindowEvent(window->id,
                                                            SDL_WINDOWEVENT_FOCUS_GAINED,
                                                            0, 0);
                                        SDL_SetKeyboardFocus(0, window->id);

                                        /* Set window region sensible to mouse motion events */
                                        PhRegionQuery(PtWidgetRid
                                                      (wdata->window),
                                                      &wregion, NULL, NULL,
                                                      0);
                                        wregion.events_sense |=
                                            Ph_EV_PTR_MOTION_BUTTON |
                                            Ph_EV_PTR_MOTION_NOBUTTON;
                                        PhRegionChange(Ph_REGION_EV_SENSE, 0,
                                                       &wregion, NULL, NULL);

                                        /* If window got a focus, then it is visible */
                                        SDL_SendWindowEvent(window->id,
                                                            SDL_WINDOWEVENT_SHOWN,
                                                            0, 0);
                                    }
                                }
                                if (wmevent->event_state ==
                                    Ph_WM_EVSTATE_FOCUSLOST) {
                                    if (window != NULL) {
                                        PhRegion_t wregion;

                                        SDL_SendWindowEvent(window->id,
                                                            SDL_WINDOWEVENT_FOCUS_LOST,
                                                            0, 0);

                                        /* Set window region ignore mouse motion events */
                                        PhRegionQuery(PtWidgetRid
                                                      (wdata->window),
                                                      &wregion, NULL, NULL,
                                                      0);
                                        wregion.events_sense &=
                                            ~(Ph_EV_PTR_MOTION_BUTTON |
                                              Ph_EV_PTR_MOTION_NOBUTTON);
                                        PhRegionChange(Ph_REGION_EV_SENSE, 0,
                                                       &wregion, NULL, NULL);
                                    }
                                }
                            }
                            break;
                        case Ph_WM_MOVE:
                            {
                                if (window != NULL) {
                                    SDL_SendWindowEvent(window->id,
                                                        SDL_WINDOWEVENT_MOVED,
                                                        wmevent->pos.x,
                                                        wmevent->pos.y);
                                }
                            }
                            break;
                        case Ph_WM_RESIZE:
                            {
                                if (window != NULL) {
                                    /* Set new window position after resize */
                                    SDL_SendWindowEvent(window->id,
                                                        SDL_WINDOWEVENT_MOVED,
                                                        wmevent->pos.x,
                                                        wmevent->pos.y);

                                    /* Check if this window uses OpenGL ES */
                                    if (wdata->uses_gles == SDL_TRUE) {
                                        /* If so, recreate surface with new dimensions */
                                        photon_gl_recreatesurface(_this, window, wmevent->size.w, wmevent->size.h);
                                    }

                                    /* Set new window size after resize */
                                    SDL_SendWindowEvent(window->id,
                                                        SDL_WINDOWEVENT_RESIZED,
                                                        wmevent->size.w,
                                                        wmevent->size.h);
                                }
                            }
                            break;
                        case Ph_WM_HIDE:
                            {
                                if (window != NULL) {
                                    /* Send new window state: minimized */
                                    SDL_SendWindowEvent(window->id,
                                                        SDL_WINDOWEVENT_MINIMIZED,
                                                        0, 0);
                                    /* In case window is minimized, then it is hidden */
                                    SDL_SendWindowEvent(window->id,
                                                        SDL_WINDOWEVENT_HIDDEN,
                                                        0, 0);
                                }
                            }
                            break;
                        case Ph_WM_MAX:
                            {
                                if (window != NULL) {
                                    if ((window->flags & SDL_WINDOW_RESIZABLE)==SDL_WINDOW_RESIZABLE)
                                    {
                                       SDL_SendWindowEvent(window->id,
                                                           SDL_WINDOWEVENT_MAXIMIZED,
                                                           0, 0);
                                    }
                                    else
                                    {
                                       /* otherwise ignor the resize events */
                                    }
                                }
                            }
                            break;
                        case Ph_WM_RESTORE:
                            {
                                if (window != NULL) {
                                    SDL_SendWindowEvent(window->id,
                                                        SDL_WINDOWEVENT_RESTORED,
                                                        0, 0);
                                }
                            }
                            break;
                        }
                    }
                    break;
                }
                PtEventHandler(event);
            }
            break;
        case 0:
            {
                /* All events are read */
                finish = 1;
                break;
            }
        case -1:
            {
                /* Error occured in event reading */
                SDL_SetError("Photon: Can't read event");
                return;
            }
            break;
        }
        if (finish != 0) {
            break;
        }
    } while (1);
}

/*****************************************************************************/
/* SDL screen saver related functions                                        */
/*****************************************************************************/
void
photon_suspendscreensaver(_THIS)
{
    /* There is no screensaver in pure console, it may exist when running */
    /* GF under Photon, but I do not know, how to disable screensaver     */
}

/* vi: set ts=4 sw=4 expandtab: */
