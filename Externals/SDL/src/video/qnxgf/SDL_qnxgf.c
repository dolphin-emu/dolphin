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

    QNX Graphics Framework SDL driver
    Copyright (C) 2009 Mike Gorchak
    (mike@malva.ua, lestat@i.com.ua)
*/

#include "SDL_config.h"

#include "../SDL_sysvideo.h"
#include "SDL_version.h"
#include "SDL_syswm.h"
#include "SDL_loadso.h"

/* Include QNX Graphics Framework declarations */
#include <gf/gf.h>

#include "SDL_qnxgf.h"
#include "SDL_gf_render.h"
#include "SDL_gf_pixelfmt.h"
#include "SDL_gf_opengles.h"
#include "SDL_gf_input.h"

/******************************************************************************/
/* SDL Generic video modes, which could provide GF                            */
/* This is real pain in the ass. GF is just wrapper around a selected driver  */
/* some drivers could support double scan modes, like 320x200, 512x384, etc   */
/* but some drivers are not. Later we can distinguish one driver from another */
/* Feel free to add any new custom graphics mode                              */
/******************************************************************************/
static const SDL_DisplayMode generic_mode[] = {
    {0, 320, 200, 70, NULL},    /* 320x200 modes are 70Hz and 85Hz          */
    {0, 320, 200, 85, NULL},
    {0, 320, 240, 70, NULL},    /* 320x240 modes are 70Hz and 85Hz          */
    {0, 320, 240, 85, NULL},
    {0, 400, 300, 60, NULL},    /* 400x300 mode is 60Hz only                */
    {0, 480, 360, 60, NULL},    /* 480x360 mode is 60Hz only                */
    {0, 512, 384, 60, NULL},    /* 512x384 modes are 60Hz and 70Hz          */
    {0, 512, 384, 70, NULL},
    {0, 640, 480, 60, NULL},    /* 640x480 modes are 60Hz, 75Hz, 85Hz       */
    {0, 640, 480, 75, NULL},
    {0, 640, 480, 85, NULL},
    {0, 800, 600, 60, NULL},    /* 800x600 modes are 60Hz, 75Hz, 85Hz       */
    {0, 800, 600, 75, NULL},
    {0, 800, 600, 85, NULL},
    {0, 800, 480, 60, NULL},    /* 800x480 mode is 60Hz only                */
    {0, 848, 480, 60, NULL},    /* 848x480 mode is 60Hz only                */
    {0, 960, 600, 60, NULL},    /* 960x600 mode is 60Hz only                */
    {0, 1024, 640, 60, NULL},   /* 1024x640 mode is 60Hz only               */
    {0, 1024, 768, 60, NULL},   /* 1024x768 modes are 60Hz, 70Hz, 75Hz      */
    {0, 1024, 768, 70, NULL},
    {0, 1024, 768, 75, NULL},
    {0, 1088, 612, 60, NULL},   /* 1088x612 mode is 60Hz only               */
    {0, 1152, 864, 60, NULL},   /* 1152x864 modes are 60Hz, 70Hz, 72Hz      */
    {0, 1152, 864, 70, NULL},   /* 75Hz and 85Hz                            */
    {0, 1152, 864, 72, NULL},
    {0, 1152, 864, 75, NULL},
    {0, 1152, 864, 85, NULL},
    {0, 1280, 720, 60, NULL},   /* 1280x720 mode is 60Hz only               */
    {0, 1280, 768, 60, NULL},   /* 1280x768 mode is 60Hz only               */
    {0, 1280, 800, 60, NULL},   /* 1280x800 mode is 60Hz only               */
    {0, 1280, 960, 60, NULL},   /* 1280x960 mode is 60Hz only               */
    {0, 1280, 1024, 60, NULL},  /* 1280x1024 modes are 60Hz, 75Hz, 85Hz and */
    {0, 1280, 1024, 75, NULL},  /* 100 Hz                                   */
    {0, 1280, 1024, 85, NULL},  /*                                          */
    {0, 1280, 1024, 100, NULL}, /*                                          */
    {0, 1360, 768, 60, NULL},   /* 1360x768 mode is 60Hz only               */
    {0, 1400, 1050, 60, NULL},  /* 1400x1050 mode is 60Hz only              */
    {0, 1440, 900, 60, NULL},   /* 1440x900 mode is 60Hz only               */
    {0, 1440, 960, 60, NULL},   /* 1440x960 mode is 60Hz only               */
    {0, 1600, 900, 60, NULL},   /* 1600x900 mode is 60Hz only               */
    {0, 1600, 1024, 60, NULL},  /* 1600x1024 mode is 60Hz only              */
    {0, 1600, 1200, 60, NULL},  /* 1600x1200 mode is 60Hz only              */
    {0, 1680, 1050, 60, NULL},  /* 1680x1050 mode is 60Hz only              */
    {0, 1920, 1080, 60, NULL},  /* 1920x1080 mode is 60Hz only              */
    {0, 1920, 1200, 60, NULL},  /* 1920x1200 mode is 60Hz only              */
    {0, 1920, 1440, 60, NULL},  /* 1920x1440 mode is 60Hz only              */
    {0, 2048, 1536, 60, NULL},  /* 2048x1536 mode is 60Hz only              */
    {0, 2048, 1080, 60, NULL},  /* 2048x1080 mode is 60Hz only              */
    {0, 0, 0, 0, NULL}          /* End of generic mode list                 */
};

/* Low level device graphics driver names, which they are reporting */
static const GF_DeviceCaps gf_devicename[] = {
    /* ATI Rage 128 graphics driver (devg-ati_rage128)      */
    {"ati_rage128", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* Fujitsu Carmine graphics driver (devg-carmine.so)    */
    {"carmine", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_ACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* C&T graphics driver (devg-chips.so)                  */
    {"chips", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* Fujitsu Coral graphics driver (devg-coral.so)        */
    {"coral", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_ACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* Intel integrated graphics driver (devg-extreme2.so)  */
    {"extreme2", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_ACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* Unaccelerated FB driver (devg-flat.so)               */
    {"flat", SDL_GF_UNACCELERATED | SDL_GF_LOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_NOVIDEOMEMORY},
    /* NS Geode graphics driver (devg-geode.so)             */
    {"geode", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* Geode LX graphics driver (devg-geodelx.so)           */
    {"geodelx", SDL_GF_ACCELERATED | SDL_GF_LOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* Intel integrated graphics driver (devg-gma9xx.so)    */
    {"gma", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_ACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* Intel integrated graphics driver (devg-i810.so)      */
    {"i810", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* Intel integrated graphics driver (devg-i830.so)      */
    {"i830", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* Geode LX graphics driver (devg-lx800.so)             */
    {"lx800", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* Matrox Gxx graphics driver (devg-matroxg.so)         */
    {"matroxg", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* Intel Poulsbo graphics driver (devg-poulsbo.so)      */
    {"poulsbo", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_ACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* ATI Radeon driver (devg-radeon.so)                   */
    {"radeon", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* ATI Rage driver (devg-rage.so)                       */
    {"rage", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* S3 Savage graphics driver (devg-s3_savage.so)        */
    {"s3_savage", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* SiS630 integrated graphics driver (devg-sis630.so)   */
    {"sis630", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* PowerVR SGX 535 graphics driver (devg-poulsbo.so)    */
    {"sgx", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_ACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* SM Voyager GX graphics driver (devg-smi5xx.so)       */
    {"smi5xx", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* Silicon Motion graphics driver (devg-smi7xx.so)      */
    {"smi7xx", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* SVGA unaccelerated gfx driver (devg-svga.so)         */
    {"svga", SDL_GF_UNACCELERATED | SDL_GF_LOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_NOVIDEOMEMORY},
    /* nVidia TNT graphics driver (devg-tnt.so)             */
    {"tnt", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* VIA integrated graphics driver (devg-tvia.so)        */
    {"tvia", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* VIA UniChrome graphics driver (devg-unichrome.so)    */
    {"unichrome", SDL_GF_ACCELERATED | SDL_GF_NOLOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* VESA unaccelerated gfx driver (devg-vesa.so)         */
    {"vesa", SDL_GF_UNACCELERATED | SDL_GF_LOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_NOVIDEOMEMORY},
    /* VmWare graphics driver (devg-volari.so)              */
    {"vmware", SDL_GF_ACCELERATED | SDL_GF_LOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_NOVIDEOMEMORY},
    /* XGI XP10 graphics driver (devg-volari.so)            */
    {"volari", SDL_GF_ACCELERATED | SDL_GF_LOWRESOLUTION |
     SDL_GF_UNACCELERATED_3D | SDL_GF_VIDEOMEMORY},
    /* End of list */
    {NULL, 0x00000000}
};

/*****************************************************************************/
/* SDL Video Device initialization functions                                 */
/*****************************************************************************/

static int
qnxgf_available(void)
{
    gf_dev_t gfdev;
    gf_dev_info_t gfdev_info;
    int status;

    /* Try to attach to graphics device driver */
    status = gf_dev_attach(&gfdev, GF_DEVICE_INDEX(0), &gfdev_info);
    if (status != GF_ERR_OK) {
        return 0;
    }

    /* Detach from graphics device driver for now */
    gf_dev_detach(gfdev);

    return 1;
}

static void
qnxgf_destroy(SDL_VideoDevice * device)
{
    SDL_VideoData *gfdata = (SDL_VideoData *) device->driverdata;

    /* Detach from graphics device driver, if it was initialized */
    if (gfdata->gfinitialized != SDL_FALSE) {
        gf_dev_detach(gfdata->gfdev);
        gfdata->gfdev = NULL;
    }

    if (device->driverdata != NULL) {
        device->driverdata = NULL;
    }
}

static SDL_VideoDevice *
qnxgf_create(int devindex)
{
    SDL_VideoDevice *device;
    SDL_VideoData *gfdata;
    int status;

    /* Initialize SDL_VideoDevice structure */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (device == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    /* Initialize internal GF specific data */
    gfdata = (SDL_VideoData *) SDL_calloc(1, sizeof(SDL_VideoData));
    if (gfdata == NULL) {
        SDL_OutOfMemory();
        SDL_free(device);
        return NULL;
    }
    device->driverdata = gfdata;

    /* Try to attach to graphics device driver */
    status =
        gf_dev_attach(&gfdata->gfdev, GF_DEVICE_INDEX(devindex),
                      &gfdata->gfdev_info);
    if (status != GF_ERR_OK) {
        SDL_OutOfMemory();
        SDL_free(gfdata);
        SDL_free(device);
        return NULL;
    }

    if (gfdata->gfdev_info.description == NULL) {
        gf_dev_detach(gfdata->gfdev);
        SDL_SetError("GF: Failed to initialize graphics driver");
        return NULL;
    }

    /* Setup amount of available displays and current display */
    device->num_displays = 0;
    device->current_display = 0;

    /* Setup device shutdown function */
    gfdata->gfinitialized = SDL_TRUE;
    device->free = qnxgf_destroy;

    /* Setup all functions which we can handle */
    device->VideoInit = qnxgf_videoinit;
    device->VideoQuit = qnxgf_videoquit;
    device->GetDisplayModes = qnxgf_getdisplaymodes;
    device->SetDisplayMode = qnxgf_setdisplaymode;
    device->SetDisplayPalette = qnxgf_setdisplaypalette;
    device->GetDisplayPalette = qnxgf_getdisplaypalette;
    device->SetDisplayGammaRamp = qnxgf_setdisplaygammaramp;
    device->GetDisplayGammaRamp = qnxgf_getdisplaygammaramp;
    device->CreateWindow = qnxgf_createwindow;
    device->CreateWindowFrom = qnxgf_createwindowfrom;
    device->SetWindowTitle = qnxgf_setwindowtitle;
    device->SetWindowIcon = qnxgf_setwindowicon;
    device->SetWindowPosition = qnxgf_setwindowposition;
    device->SetWindowSize = qnxgf_setwindowsize;
    device->ShowWindow = qnxgf_showwindow;
    device->HideWindow = qnxgf_hidewindow;
    device->RaiseWindow = qnxgf_raisewindow;
    device->MaximizeWindow = qnxgf_maximizewindow;
    device->MinimizeWindow = qnxgf_minimizewindow;
    device->RestoreWindow = qnxgf_restorewindow;
    device->SetWindowGrab = qnxgf_setwindowgrab;
    device->DestroyWindow = qnxgf_destroywindow;
    device->GetWindowWMInfo = qnxgf_getwindowwminfo;
    device->GL_LoadLibrary = qnxgf_gl_loadlibrary;
    device->GL_GetProcAddress = qnxgf_gl_getprocaddres;
    device->GL_UnloadLibrary = qnxgf_gl_unloadlibrary;
    device->GL_CreateContext = qnxgf_gl_createcontext;
    device->GL_MakeCurrent = qnxgf_gl_makecurrent;
    device->GL_SetSwapInterval = qnxgf_gl_setswapinterval;
    device->GL_GetSwapInterval = qnxgf_gl_getswapinterval;
    device->GL_SwapWindow = qnxgf_gl_swapwindow;
    device->GL_DeleteContext = qnxgf_gl_deletecontext;
    device->PumpEvents = qnxgf_pumpevents;
    device->SuspendScreenSaver = qnxgf_suspendscreensaver;

    return device;
}

VideoBootStrap qnxgf_bootstrap = {
    "qnxgf",
    "SDL QNX Graphics Framework (GF) video driver",
    qnxgf_available,
    qnxgf_create
};

/*****************************************************************************/
/* SDL Video and Display initialization/handling functions                   */
/*****************************************************************************/
int
qnxgf_videoinit(_THIS)
{
    SDL_VideoData *gfdata = (SDL_VideoData *) _this->driverdata;
    uint32_t it;
    uint32_t jt;
    char *override;
    int32_t status;

    /* By default GF uses buffer swap on vsync */
    gfdata->swapinterval = 1;

    /* Add each detected output to SDL */
    for (it = 0; it < gfdata->gfdev_info.ndisplays; it++) {
        SDL_VideoDisplay display;
        SDL_DisplayMode current_mode;
        SDL_DisplayData *didata;

        didata = (SDL_DisplayData *) SDL_calloc(1, sizeof(SDL_DisplayData));
        if (didata == NULL) {
            /* memory allocation problem */
            SDL_OutOfMemory();
            return -1;
        }

        /* Set default cursor settings, maximum 128x128 cursor */
        didata->cursor_visible = SDL_FALSE;
        didata->cursor.type = GF_CURSOR_BITMAP;
        didata->cursor.hotspot.x = 0;
        didata->cursor.hotspot.y = 0;
        didata->cursor.cursor.bitmap.w = SDL_VIDEO_GF_MAX_CURSOR_SIZE;
        didata->cursor.cursor.bitmap.h = SDL_VIDEO_GF_MAX_CURSOR_SIZE;
        didata->cursor.cursor.bitmap.stride =
            (didata->cursor.cursor.bitmap.w + 7) / (sizeof(uint8_t) * 8);
        didata->cursor.cursor.bitmap.color0 = 0x00000000;
        didata->cursor.cursor.bitmap.color1 = 0x00000000;
        didata->cursor.cursor.bitmap.image0 =
            SDL_calloc(sizeof(uint8_t),
                       (didata->cursor.cursor.bitmap.w +
                        7) * didata->cursor.cursor.bitmap.h /
                       (sizeof(uint8_t) * 8));
        if (didata->cursor.cursor.bitmap.image0 == NULL) {
            SDL_free(didata);
            SDL_OutOfMemory();
            return -1;
        }
        didata->cursor.cursor.bitmap.image1 =
            SDL_calloc(sizeof(uint8_t),
                       (didata->cursor.cursor.bitmap.w +
                        7) * didata->cursor.cursor.bitmap.h /
                       (sizeof(uint8_t) * 8));
        if (didata->cursor.cursor.bitmap.image1 == NULL) {
            SDL_OutOfMemory();
            SDL_free((void *) didata->cursor.cursor.bitmap.image0);
            SDL_free(didata);
            return -1;
        }

        /* Query current display settings */
        status = gf_display_query(gfdata->gfdev, it, &didata->display_info);
        if (status == GF_ERR_OK) {
            SDL_zero(current_mode);
            current_mode.w = didata->display_info.xres;
            current_mode.h = didata->display_info.yres;
            current_mode.refresh_rate = didata->display_info.refresh;
            current_mode.format =
                qnxgf_gf_to_sdl_pixelformat(didata->display_info.format);
            current_mode.driverdata = NULL;
        } else {
            /* video initialization problem */
            SDL_free((void *) didata->cursor.cursor.bitmap.image0);
            SDL_free((void *) didata->cursor.cursor.bitmap.image1);
            SDL_free(didata);
            SDL_SetError("GF: Display query failed");
            return -1;
        }

        /* Attach GF to selected display */
        status = gf_display_attach(&didata->display, gfdata->gfdev, it, NULL);
        if (status != GF_ERR_OK) {
            /* video initialization problem */
            SDL_free((void *) didata->cursor.cursor.bitmap.image0);
            SDL_free((void *) didata->cursor.cursor.bitmap.image1);
            SDL_free(didata);
            SDL_SetError("GF: Couldn't attach to display");
            return -1;
        }

        /* Initialize status variables */
        didata->layer_attached = SDL_FALSE;

        /* Attach to main display layer */
        status =
            gf_layer_attach(&didata->layer, didata->display,
                            didata->display_info.main_layer_index, 0);
        if (status != GF_ERR_OK) {
            /* Failed to attach to main layer */
            SDL_free((void *) didata->cursor.cursor.bitmap.image0);
            SDL_free((void *) didata->cursor.cursor.bitmap.image1);
            SDL_free(didata);
            SDL_SetError
                ("GF: Couldn't attach to main layer, it could be busy");
            return -1;
        }

        /* Mark main display layer is attached */
        didata->layer_attached = SDL_TRUE;

        /* Set layer source and destination viewport */
        gf_layer_set_src_viewport(didata->layer, 0, 0, current_mode.w - 1,
                                  current_mode.h - 1);
        gf_layer_set_dst_viewport(didata->layer, 0, 0, current_mode.w - 1,
                                  current_mode.h - 1);

        /* Create main visible on display surface */
        status = gf_surface_create_layer(&didata->surface[0], &didata->layer,
                                         1, 0, current_mode.w, current_mode.h,
                                         qnxgf_sdl_to_gf_pixelformat
                                         (current_mode.format), NULL,
                                         GF_SURFACE_CREATE_2D_ACCESSIBLE |
                                         GF_SURFACE_CREATE_3D_ACCESSIBLE |
                                         GF_SURFACE_CREATE_SHAREABLE);
        if (status != GF_ERR_OK) {
            gf_layer_disable(didata->layer);
            gf_layer_detach(didata->layer);
            didata->layer_attached = SDL_FALSE;
            SDL_free((void *) didata->cursor.cursor.bitmap.image0);
            SDL_free((void *) didata->cursor.cursor.bitmap.image1);
            SDL_free(didata);
            SDL_SetError("GF: Can't create main layer surface at init (%d)\n",
                         status);
            return -1;
        }

        /* Set just created surface as main visible on the layer */
//      gf_layer_set_surfaces(didata->layer, &didata->surface[0], 1);

        /* Update layer parameters */
        status = gf_layer_update(didata->layer, GF_LAYER_UPDATE_NO_WAIT_IDLE);
        if (status != GF_ERR_OK) {
            /* Free allocated surface */
            gf_surface_free(didata->surface[0]);
            didata->surface[0] = NULL;

            /* Disable and detach from layer */
            gf_layer_disable(didata->layer);
            gf_layer_detach(didata->layer);
            didata->layer_attached = SDL_FALSE;
            SDL_free((void *) didata->cursor.cursor.bitmap.image0);
            SDL_free((void *) didata->cursor.cursor.bitmap.image1);
            SDL_free(didata);
            SDL_SetError("GF: Can't update layer parameters\n");
            return -1;
        }

        /* Enable layer in case if hardware supports layer enable/disable */
        gf_layer_enable(didata->layer);

        /* Copy device name for each display */
        SDL_strlcpy(didata->description, gfdata->gfdev_info.description,
                    SDL_VIDEO_GF_DEVICENAME_MAX - 1);

        /* Search device capabilities and possible workarounds */
        jt = 0;
        do {
            if (gf_devicename[jt].name == NULL) {
                break;
            }
            if (SDL_strncmp
                (gf_devicename[jt].name, didata->description,
                 SDL_strlen(gf_devicename[jt].name)) == 0) {
                didata->caps = gf_devicename[jt].caps;
            }
            jt++;
        } while (1);

        /* Initialize display structure */
        SDL_zero(display);
        display.desktop_mode = current_mode;
        display.current_mode = current_mode;
        display.driverdata = didata;
        didata->current_mode = current_mode;
        SDL_AddVideoDisplay(&display);

        /* Check for environment variables which could override some SDL settings */
        didata->custom_refresh = 0;
        override = SDL_getenv("SDL_VIDEO_GF_REFRESH_RATE");
        if (override != NULL) {
            if (SDL_sscanf(override, "%u", &didata->custom_refresh) != 1) {
                didata->custom_refresh = 0;
            }
        }

        /* Get all display modes for this display */
        _this->current_display = it;
        qnxgf_getdisplaymodes(_this);
    }

    /* Restore default display */
    _this->current_display = 0;

    /* Add GF renderer to SDL */
    gf_addrenderdriver(_this);

    /* Add GF input devices */
    status = gf_addinputdevices(_this);
    if (status != 0) {
        /* SDL error is set by gf_addinputdevices() function */
        return -1;
    }

    /* video has been initialized successfully */
    return 1;
}

void
qnxgf_videoquit(_THIS)
{
    SDL_DisplayData *didata = NULL;
    uint32_t it;

    /* Stop collecting mouse events */
    hiddi_disable_mouse();
    /* Delete GF input devices */
    gf_delinputdevices(_this);

    /* SDL will restore old desktop mode on exit */
    for (it = 0; it < _this->num_displays; it++) {
        didata = _this->displays[it].driverdata;

        /* Free cursor image */
        if (didata->cursor.cursor.bitmap.image0 != NULL) {
            SDL_free((void *) didata->cursor.cursor.bitmap.image0);
            didata->cursor.cursor.bitmap.image0 = NULL;
        }
        if (didata->cursor.cursor.bitmap.image1 != NULL) {
            SDL_free((void *) didata->cursor.cursor.bitmap.image1);
            didata->cursor.cursor.bitmap.image1 = NULL;
        }

        /* Free main surface */
        if (didata->surface[0] != NULL) {
            gf_surface_free(didata->surface[0]);
            didata->surface[0] = NULL;
        }

        /* Free back surface */
        if (didata->surface[1] != NULL) {
            gf_surface_free(didata->surface[1]);
            didata->surface[1] = NULL;
        }

        /* Free second back surface */
        if (didata->surface[2] != NULL) {
            gf_surface_free(didata->surface[2]);
            didata->surface[2] = NULL;
        }

        /* Detach layer before quit */
        if (didata->layer_attached == SDL_TRUE) {
            /* Disable layer if hardware supports this */
            gf_layer_disable(didata->layer);

            /* Detach from layer, free it for others */
            gf_layer_detach(didata->layer);
            didata->layer = NULL;

            /* Mark it as detached */
            didata->layer_attached = SDL_FALSE;
        }

        /* Detach from selected display */
        gf_display_detach(didata->display);
        didata->display = NULL;
    }
}

void
qnxgf_getdisplaymodes(_THIS)
{
    SDL_DisplayData *didata =
        (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;
    SDL_DisplayMode mode;
    gf_modeinfo_t modeinfo;
    uint32_t it = 0;
    uint32_t jt = 0;
    uint32_t kt = 0;
    int status;

    do {
        status = gf_display_query_mode(didata->display, it, &modeinfo);
        if (status == GF_ERR_OK) {
            /* Parsing current mode */
            if ((modeinfo.flags & GF_MODE_GENERIC) == GF_MODE_GENERIC) {
                /* This mode is generic, so we can add to SDL our resolutions */
                /* Only pixel format is fixed, refresh rate could be any      */
                jt = 0;
                do {
                    if (generic_mode[jt].w == 0) {
                        break;
                    }

                    /* Check if driver do not supports doublescan video modes */
                    if ((didata->caps & SDL_GF_LOWRESOLUTION) !=
                        SDL_GF_LOWRESOLUTION) {
                        if (generic_mode[jt].w < 640) {
                            jt++;
                            continue;
                        }
                    }

                    mode.w = generic_mode[jt].w;
                    mode.h = generic_mode[jt].h;
                    mode.refresh_rate = generic_mode[jt].refresh_rate;
                    mode.format =
                        qnxgf_gf_to_sdl_pixelformat(modeinfo.primary_format);
                    mode.driverdata = NULL;
                    SDL_AddDisplayMode(_this->current_display, &mode);

                    /* If mode is RGBA8888, add the same mode as RGBx888 */
                    if (modeinfo.primary_format == GF_FORMAT_BGRA8888) {
                        mode.w = generic_mode[jt].w;
                        mode.h = generic_mode[jt].h;
                        mode.refresh_rate = generic_mode[jt].refresh_rate;
                        mode.format = SDL_PIXELFORMAT_RGB888;
                        mode.driverdata = NULL;
                        SDL_AddDisplayMode(_this->current_display, &mode);
                    }
                    /* If mode is RGBA1555, add the same mode as RGBx555 */
                    if (modeinfo.primary_format == GF_FORMAT_PACK_ARGB1555) {
                        mode.w = generic_mode[jt].w;
                        mode.h = generic_mode[jt].h;
                        mode.refresh_rate = generic_mode[jt].refresh_rate;
                        mode.format = SDL_PIXELFORMAT_RGB555;
                        mode.driverdata = NULL;
                        SDL_AddDisplayMode(_this->current_display, &mode);
                    }

                    jt++;
                } while (1);
            } else {
                /* Add this display mode as is in case if it is non-generic */
                /* But go through the each refresh rate, supported by gf    */
                jt = 0;
                do {
                    if (modeinfo.refresh[jt] != 0) {
                        mode.w = modeinfo.xres;
                        mode.h = modeinfo.yres;
                        mode.refresh_rate = modeinfo.refresh[jt];
                        mode.format =
                            qnxgf_gf_to_sdl_pixelformat(modeinfo.
                                                        primary_format);
                        mode.driverdata = NULL;
                        SDL_AddDisplayMode(_this->current_display, &mode);

                        /* If mode is RGBA8888, add the same mode as RGBx888 */
                        if (modeinfo.primary_format == GF_FORMAT_BGRA8888) {
                            mode.w = modeinfo.xres;
                            mode.h = modeinfo.yres;
                            mode.refresh_rate = modeinfo.refresh[jt];
                            mode.format = SDL_PIXELFORMAT_RGB888;
                            mode.driverdata = NULL;
                            SDL_AddDisplayMode(_this->current_display, &mode);
                        }
                        /* If mode is RGBA1555, add the same mode as RGBx555 */
                        if (modeinfo.primary_format ==
                            GF_FORMAT_PACK_ARGB1555) {
                            mode.w = modeinfo.xres;
                            mode.h = modeinfo.yres;
                            mode.refresh_rate = modeinfo.refresh[jt];
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
        } else {
            if (status == GF_ERR_PARM) {
                /* out of available modes, all are listed */
                break;
            }

            /* Error occured during mode listing */
            break;
        }
        it++;
    } while (1);
}

int
qnxgf_setdisplaymode(_THIS, SDL_DisplayMode * mode)
{
    SDL_DisplayData *didata =
        (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;
    uint32_t refresh_rate = 0;
    int status;

    /* Current display dimensions and bpp are no more valid */
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
        uint32_t it;
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
        uint32_t it;
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

    /* Free main surface */
    if (didata->surface[0] != NULL) {
        gf_surface_free(didata->surface[0]);
        didata->surface[0] = NULL;
    }

    /* Free back surface */
    if (didata->surface[1] != NULL) {
        gf_surface_free(didata->surface[1]);
        didata->surface[1] = NULL;
    }

    /* Free second back surface */
    if (didata->surface[2] != NULL) {
        gf_surface_free(didata->surface[2]);
        didata->surface[2] = NULL;
    }

    /* Detach layer before switch to new graphics mode */
    if (didata->layer_attached == SDL_TRUE) {
        /* Disable layer if hardware supports this */
        gf_layer_disable(didata->layer);

        /* Detach from layer, free it for others */
        gf_layer_detach(didata->layer);

        /* Mark it as detached */
        didata->layer_attached = SDL_FALSE;
    }

    /* Set new display video mode */
    status =
        gf_display_set_mode(didata->display, mode->w, mode->h, refresh_rate,
                            qnxgf_sdl_to_gf_pixelformat(mode->format), 0);
    if (status != GF_ERR_OK) {
        /* Display mode/resolution switch has been failed */
        SDL_SetError("GF: Mode is not supported by graphics driver");
        return -1;
    } else {
        didata->current_mode = *mode;
        didata->current_mode.refresh_rate = refresh_rate;
    }

    /* Attach to main display layer */
    status =
        gf_layer_attach(&didata->layer, didata->display,
                        didata->display_info.main_layer_index, 0);
    if (status != GF_ERR_OK) {
        SDL_SetError("GF: Couldn't attach to main layer, it could be busy");

        /* Failed to attach to main displayable layer */
        return -1;
    }

    /* Mark main display layer is attached */
    didata->layer_attached = SDL_TRUE;

    /* Set layer source and destination viewports */
    gf_layer_set_src_viewport(didata->layer, 0, 0, mode->w - 1, mode->h - 1);
    gf_layer_set_dst_viewport(didata->layer, 0, 0, mode->w - 1, mode->h - 1);

    /* Create main visible on display surface */
    status =
        gf_surface_create_layer(&didata->surface[0], &didata->layer, 1, 0,
                                mode->w, mode->h,
                                qnxgf_sdl_to_gf_pixelformat(mode->format),
                                NULL,
                                GF_SURFACE_CREATE_2D_ACCESSIBLE |
                                GF_SURFACE_CREATE_3D_ACCESSIBLE |
                                GF_SURFACE_CREATE_SHAREABLE);
    if (status != GF_ERR_OK) {
        gf_layer_disable(didata->layer);
        gf_layer_detach(didata->layer);
        didata->layer_attached = SDL_FALSE;
        SDL_SetError
            ("GF: Can't create main layer surface at modeswitch (%d)\n",
             status);
        return -1;
    }

    /* Set just created surface as main visible on the layer */
    gf_layer_set_surfaces(didata->layer, &didata->surface[0], 1);

    /* Update layer parameters */
    status = gf_layer_update(didata->layer, GF_LAYER_UPDATE_NO_WAIT_IDLE);
    if (status != GF_ERR_OK) {
        /* Free main surface */
        gf_surface_free(didata->surface[0]);
        didata->surface[0] = NULL;

        /* Detach layer */
        gf_layer_disable(didata->layer);
        gf_layer_detach(didata->layer);
        didata->layer_attached = SDL_FALSE;
        SDL_SetError("GF: Can't update layer parameters\n");
        return -1;
    }

    /* Restore cursor if it was visible */
    if (didata->cursor_visible == SDL_TRUE) {
        gf_cursor_set(didata->display, 0, &didata->cursor);
        gf_cursor_enable(didata->display, 0);
    }

    /* Enable layer in case if hardware supports layer enable/disable */
    gf_layer_enable(didata->layer);

    return 0;
}

int
qnxgf_setdisplaypalette(_THIS, SDL_Palette * palette)
{
    SDL_DisplayData *didata =
        (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;

    /* QNX GF doesn't have support for global palette changing, but we */
    /* could store it for usage in future */

    /* Setting display palette operation has been failed */
    return -1;
}

int
qnxgf_getdisplaypalette(_THIS, SDL_Palette * palette)
{
    SDL_DisplayData *didata =
        (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;

    /* We can't provide current palette settings and looks like SDL          */
    /* do not call this function also, in such case this function returns -1 */

    /* Getting display palette operation has been failed */
    return -1;
}

int
qnxgf_setdisplaygammaramp(_THIS, Uint16 * ramp)
{
    SDL_DisplayData *didata =
        (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;
    int status;

    /* Setup gamma ramp, for each color channel */
    status =
        gf_display_set_color_lut16(didata->display, (uint16_t *) ramp,
                                   (uint16_t *) ramp + 256,
                                   (uint16_t *) ramp + 512);
    if (status != GF_ERR_OK) {
        /* Setting display gamma ramp operation has been failed */
        return -1;
    }

    return 0;
}

int
qnxgf_getdisplaygammaramp(_THIS, Uint16 * ramp)
{
    /* TODO: We need to return previous gamma set           */
    /*       Also we need some initial fake gamma to return */

    /* Getting display gamma ramp operation has been failed */
    return -1;
}

int
qnxgf_createwindow(_THIS, SDL_Window * window)
{
    SDL_VideoData *gfdata = (SDL_VideoData *) _this->driverdata;
    SDL_DisplayData *didata =
        (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;
    SDL_WindowData *wdata;
    int32_t status;

    /* QNX GF supports fullscreen window modes only */
    if ((window->flags & SDL_WINDOW_FULLSCREEN) != SDL_WINDOW_FULLSCREEN) {
        uint32_t it;
        SDL_DisplayMode mode;

        /* Clear display mode structure */
        SDL_memset(&mode, 0x00, sizeof(SDL_DisplayMode));
        mode.refresh_rate = 0x0000FFFF;

        /* Check if window width and height matches one of our modes */
        for (it = 0; it < SDL_CurrentDisplay.num_display_modes; it++) {
            if ((SDL_CurrentDisplay.display_modes[it].w == window->w) &&
                (SDL_CurrentDisplay.display_modes[it].h == window->h) &&
                (SDL_CurrentDisplay.display_modes[it].format ==
                 SDL_CurrentDisplay.desktop_mode.format)) {
                /* Find the lowest refresh rate available */
                if (mode.refresh_rate >
                    SDL_CurrentDisplay.display_modes[it].refresh_rate) {
                    mode = SDL_CurrentDisplay.display_modes[it];
                }
            }
        }

        /* Check if end of display list has been reached */
        if (mode.refresh_rate == 0x0000FFFF) {
            SDL_SetError("GF: Desired video mode is not supported");

            /* Failed to create new window */
            return -1;
        } else {
            /* Tell to the caller that mode will be fullscreen */
            window->flags |= SDL_WINDOW_FULLSCREEN;

            /* Setup fullscreen mode, bpp used from desktop mode in this case */
            status = qnxgf_setdisplaymode(_this, &mode);
            if (status != 0) {
                /* Failed to swith fullscreen video mode */
                return -1;
            }
        }
    }

    /* Setup our own window decorations and states, which are depend on fullscreen mode */
    window->flags |= SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS |
        SDL_WINDOW_MAXIMIZED | SDL_WINDOW_INPUT_GRABBED |
        SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS;
    window->flags &= ~(SDL_WINDOW_RESIZABLE | SDL_WINDOW_MINIMIZED);

    /* Ignore any window position settings */
    window->x = SDL_WINDOWPOS_UNDEFINED;
    window->y = SDL_WINDOWPOS_UNDEFINED;

    /* Allocate window internal data */
    wdata = (SDL_WindowData *) SDL_calloc(1, sizeof(SDL_WindowData));
    if (wdata == NULL) {
        SDL_OutOfMemory();
        return -1;
    }

    /* Setup driver data for this window */
    window->driverdata = wdata;

    /* Check if window must support OpenGL ES rendering */
    if ((window->flags & SDL_WINDOW_OPENGL) == SDL_WINDOW_OPENGL) {
#if defined(SDL_VIDEO_OPENGL_ES)
        EGLBoolean initstatus;

        /* Mark this window as OpenGL ES compatible */
        wdata->uses_gles = SDL_TRUE;

        /* Create connection to OpenGL ES */
        if (gfdata->egldisplay == EGL_NO_DISPLAY) {
            gfdata->egldisplay = eglGetDisplay(gfdata->gfdev);
            if (gfdata->egldisplay == EGL_NO_DISPLAY) {
                SDL_SetError("GF: Can't get connection to OpenGL ES");
                return -1;
            }

            /* Initialize OpenGL ES library, ignore EGL version */
            initstatus = eglInitialize(gfdata->egldisplay, NULL, NULL);
            if (initstatus != EGL_TRUE) {
                SDL_SetError("GF: Can't init OpenGL ES library");
                return -1;
            }
        }

        /* Increment GL ES reference count usage */
        gfdata->egl_refcount++;
#else
        SDL_SetError("GF: OpenGL ES support is not compiled in");
        return -1;
#endif /* SDL_VIDEO_OPENGL_ES */
    }

    /* Enable mouse event collecting */
    hiddi_enable_mouse();

    /* By default last created window got a input focus */
    SDL_SetKeyboardFocus(0, window->id);
    SDL_SetMouseFocus(0, window->id);

    /* Window has been successfully created */
    return 0;
}

int
qnxgf_createwindowfrom(_THIS, SDL_Window * window, const void *data)
{
    /* Failed to create window from another window */
    return -1;
}

void
qnxgf_setwindowtitle(_THIS, SDL_Window * window)
{
}

void
qnxgf_setwindowicon(_THIS, SDL_Window * window, SDL_Surface * icon)
{
}

void
qnxgf_setwindowposition(_THIS, SDL_Window * window)
{
}

void
qnxgf_setwindowsize(_THIS, SDL_Window * window)
{
}

void
qnxgf_showwindow(_THIS, SDL_Window * window)
{
}

void
qnxgf_hidewindow(_THIS, SDL_Window * window)
{
}

void
qnxgf_raisewindow(_THIS, SDL_Window * window)
{
}

void
qnxgf_maximizewindow(_THIS, SDL_Window * window)
{
}

void
qnxgf_minimizewindow(_THIS, SDL_Window * window)
{
}

void
qnxgf_restorewindow(_THIS, SDL_Window * window)
{
}

void
qnxgf_setwindowgrab(_THIS, SDL_Window * window)
{
}

void
qnxgf_destroywindow(_THIS, SDL_Window * window)
{
    SDL_VideoData *gfdata = (SDL_VideoData *) _this->driverdata;
    SDL_DisplayData *didata =
        (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;
    SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;

    if (wdata != NULL) {
#if defined(SDL_VIDEO_OPENGL_ES)
        /* Destroy OpenGL ES surface if it was created */
        if (wdata->gles_surface != EGL_NO_SURFACE) {
            eglDestroySurface(gfdata->egldisplay, wdata->gles_surface);
        }

        /* Free any 3D target if it was created before */
        if (wdata->target_created == SDL_TRUE) {
            gf_3d_target_free(wdata->target);
            wdata->target_created == SDL_FALSE;
        }

        gfdata->egl_refcount--;
        if (gfdata->egl_refcount == 0) {
            /* Terminate connection to OpenGL ES */
            if (gfdata->egldisplay != EGL_NO_DISPLAY) {
                eglTerminate(gfdata->egldisplay);
                gfdata->egldisplay = EGL_NO_DISPLAY;
            }
        }
#endif /* SDL_VIDEO_OPENGL_ES */
    }
}

/*****************************************************************************/
/* SDL Window Manager function                                               */
/*****************************************************************************/
SDL_bool
qnxgf_getwindowwminfo(_THIS, SDL_Window * window, struct SDL_SysWMinfo *info)
{
    /* QNX GF do not operates at window level, this means we are have no */
    /* Window Manager available, no specific data in SDL_SysWMinfo too   */

    if (info->version.major <= SDL_MAJOR_VERSION) {
        return SDL_TRUE;
    } else {
        SDL_SetError("Application not compiled with SDL %d.%d\n",
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
qnxgf_gl_loadlibrary(_THIS, const char *path)
{
#if defined(SDL_VIDEO_OPENGL_ES)
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
        SDL_SetError("GF: Failed to locate OpenGL ES library");
        return -1;
    }

    /* Store OpenGL ES library path and name */
    SDL_strlcpy(_this->gl_config.driver_path, path,
                SDL_arraysize(_this->gl_config.driver_path));

    /* New OpenGL ES library is loaded */
    return 0;
#else
    SDL_SetError("GF: OpenGL ES support is not compiled in");
    return -1;
#endif /* SDL_VIDEO_OPENGL_ES */
}

void *
qnxgf_gl_getprocaddres(_THIS, const char *proc)
{
#if defined(SDL_VIDEO_OPENGL_ES)
    void *function_address;

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
    SDL_SetError("GF: Cannot locate OpenGL ES function name");
    return NULL;
#else
    SDL_SetError("GF: OpenGL ES support is not compiled in");
    return NULL;
#endif /* SDL_VIDEO_OPENGL_ES */
}

void
qnxgf_gl_unloadlibrary(_THIS)
{
#if defined(SDL_VIDEO_OPENGL_ES)
    /* Unload OpenGL ES library */
    if (_this->gl_config.dll_handle) {
        SDL_UnloadObject(_this->gl_config.dll_handle);
        _this->gl_config.dll_handle = NULL;
    }
#else
    SDL_SetError("GF: OpenGL ES support is not compiled in");
    return;
#endif /* SDL_VIDEO_OPENGL_ES */
}

SDL_GLContext
qnxgf_gl_createcontext(_THIS, SDL_Window * window)
{
#if defined(SDL_VIDEO_OPENGL_ES)
    SDL_VideoData *gfdata = (SDL_VideoData *) _this->driverdata;
    SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;
    SDL_DisplayData *didata =
        (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;
    EGLBoolean status;
    int32_t gfstatus;
    EGLint configs;
    uint32_t surfaces;
    uint32_t attr_pos;
    EGLint attr_value;
    EGLint cit;

    /* Choose buffeingr scheme */
    if (!_this->gl_config.double_buffer) {
        surfaces = 1;
    } else {
        surfaces = 2;
    }

    /* If driver has no support of video memory allocation, then */
    /* disable double buffering, use single buffer present copy  */
    if ((didata->caps & SDL_GF_VIDEOMEMORY) != SDL_GF_VIDEOMEMORY) {
        surfaces = 1;
    }

    /* Free main surface */
    if (didata->surface[0] != NULL) {
        gf_surface_free(didata->surface[0]);
        didata->surface[0] = NULL;
    }

    /* Free back surface */
    if (didata->surface[1] != NULL) {
        gf_surface_free(didata->surface[1]);
        didata->surface[1] = NULL;
    }

    /* Free second back surface */
    if (didata->surface[2] != NULL) {
        gf_surface_free(didata->surface[2]);
        didata->surface[2] = NULL;
    }

    /* Detach layer before switch to new graphics mode */
    if (didata->layer_attached == SDL_TRUE) {
        /* Disable layer if hardware supports this */
        gf_layer_disable(didata->layer);

        /* Detach from layer, free it for others */
        gf_layer_detach(didata->layer);

        /* Mark it as detached */
        didata->layer_attached = SDL_FALSE;
    }

    /* Attach to main display layer */
    gfstatus =
        gf_layer_attach(&didata->layer, didata->display,
                        didata->display_info.main_layer_index, 0);
    if (gfstatus != GF_ERR_OK) {
        SDL_SetError("GF: Couldn't attach to main layer, it could be busy");

        /* Failed to attach to main displayable layer */
        return NULL;
    }

    /* Mark main display layer is attached */
    didata->layer_attached = SDL_TRUE;

    /* Set layer source and destination viewport */
    gf_layer_set_src_viewport(didata->layer, 0, 0, didata->current_mode.w - 1,
                              didata->current_mode.h - 1);
    gf_layer_set_dst_viewport(didata->layer, 0, 0, didata->current_mode.w - 1,
                              didata->current_mode.h - 1);

    /* Create main visible on display surface */
    gfstatus =
        gf_surface_create_layer(&didata->surface[0], &didata->layer, 1, 0,
                                didata->current_mode.w,
                                didata->current_mode.h,
                                qnxgf_sdl_to_gf_pixelformat(didata->
                                                            current_mode.
                                                            format), NULL,
                                GF_SURFACE_CREATE_2D_ACCESSIBLE |
                                GF_SURFACE_CREATE_3D_ACCESSIBLE |
                                GF_SURFACE_CREATE_SHAREABLE);
    if (gfstatus != GF_ERR_OK) {
        gf_layer_disable(didata->layer);
        gf_layer_detach(didata->layer);
        didata->layer_attached = SDL_FALSE;
        SDL_SetError("GF: Can't create main layer surface at glctx (%d)\n",
                     gfstatus);
        return NULL;
    }

    /* Set just created surface as main visible on the layer */
//      gf_layer_set_surfaces(didata->layer, &didata->surface[0], 1);

    if (surfaces > 1) {
        /* Create back display surface */
        gfstatus =
            gf_surface_create_layer(&didata->surface[1], &didata->layer, 1, 0,
                                    didata->current_mode.w,
                                    didata->current_mode.h,
                                    qnxgf_sdl_to_gf_pixelformat(didata->
                                                                current_mode.
                                                                format), NULL,
                                    GF_SURFACE_CREATE_2D_ACCESSIBLE |
                                    GF_SURFACE_CREATE_3D_ACCESSIBLE |
                                    GF_SURFACE_CREATE_SHAREABLE);
        if (gfstatus != GF_ERR_OK) {
            gf_surface_free(didata->surface[0]);
            gf_layer_disable(didata->layer);
            gf_layer_detach(didata->layer);
            didata->layer_attached = SDL_FALSE;
            SDL_SetError
                ("GF: Can't create main layer surface at glctx (%d)\n",
                 gfstatus);
            return NULL;
        }
    }

    /* Update layer parameters */
    gfstatus = gf_layer_update(didata->layer, GF_LAYER_UPDATE_NO_WAIT_IDLE);
    if (gfstatus != GF_ERR_OK) {
        /* Free main and back surfaces */
        gf_surface_free(didata->surface[1]);
        didata->surface[1] = NULL;
        gf_surface_free(didata->surface[0]);
        didata->surface[0] = NULL;

        /* Detach layer */
        gf_layer_disable(didata->layer);
        gf_layer_detach(didata->layer);
        didata->layer_attached = SDL_FALSE;
        SDL_SetError("GF: Can't update layer parameters\n");
        return NULL;
    }

    /* Enable layer in case if hardware supports layer enable/disable */
    gf_layer_enable(didata->layer);

    /* Prepare attributes list to pass them to OpenGL ES */
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
    if (_this->gl_config.alpha_size) {
        wdata->gles_attributes[attr_pos++] = _this->gl_config.alpha_size;
    } else {
        wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
    }

    wdata->gles_attributes[attr_pos++] = EGL_DEPTH_SIZE;
    if (_this->gl_config.depth_size) {
        wdata->gles_attributes[attr_pos++] = _this->gl_config.depth_size;
    } else {
        wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
    }

    if (_this->gl_config.buffer_size) {
        wdata->gles_attributes[attr_pos++] = EGL_BUFFER_SIZE;
        wdata->gles_attributes[attr_pos++] = _this->gl_config.buffer_size;
    }
    if (_this->gl_config.stencil_size) {
        wdata->gles_attributes[attr_pos++] = EGL_STENCIL_SIZE;
        wdata->gles_attributes[attr_pos++] = _this->gl_config.stencil_size;
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
    status = eglChooseConfig(gfdata->egldisplay, wdata->gles_attributes,
                             wdata->gles_configs, SDL_VIDEO_GF_OPENGLES_CONFS,
                             &configs);
    if (status != EGL_TRUE) {
        SDL_SetError("GF: Can't find closest configuration for OpenGL ES");
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
                /* Replace previous data set with EGL_DONT_CARE       */
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

                    /* exit from stencil loop */
                    jt = 0;
                }

                /* Don't care about antialiasing */
                wdata->gles_attributes[attr_pos++] = EGL_SAMPLES;
                wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
                wdata->gles_attributes[attr_pos++] = EGL_SAMPLE_BUFFERS;
                wdata->gles_attributes[attr_pos++] = EGL_DONT_CARE;
                wdata->gles_attributes[attr_pos] = EGL_NONE;

                /* Request first suitable framebuffer configuration */
                status =
                    eglChooseConfig(gfdata->egldisplay,
                                    wdata->gles_attributes,
                                    wdata->gles_configs,
                                    SDL_VIDEO_GF_OPENGLES_CONFS, &configs);
                if (status != EGL_TRUE) {
                    SDL_SetError
                        ("GF: Can't find closest configuration for OpenGL ES");
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
            SDL_SetError("GF: Can't find any configuration for OpenGL ES");
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
                eglGetConfigAttrib(gfdata->egldisplay,
                                   wdata->gles_configs[cit], EGL_STENCIL_SIZE,
                                   &cur_stencil);
            if (status == EGL_TRUE) {
                if (attr_value != 0) {
                    stencil_found = 1;
                }
            }
        } else {
            stencil_found = 1;
        }

        if (_this->gl_config.depth_size) {
            status =
                eglGetConfigAttrib(gfdata->egldisplay,
                                   wdata->gles_configs[cit], EGL_DEPTH_SIZE,
                                   &cur_depth);
            if (status == EGL_TRUE) {
                if (attr_value != 0) {
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
        eglCreateContext(gfdata->egldisplay,
                         wdata->gles_configs[wdata->gles_config],
                         EGL_NO_CONTEXT, NULL);
    if (wdata->gles_context == EGL_NO_CONTEXT) {
        SDL_SetError("GF: OpenGL ES context creation has been failed");
        return NULL;
    }

    /* Free any 3D target if it was created before */
    if (wdata->target_created == SDL_TRUE) {
        gf_3d_target_free(wdata->target);
        wdata->target_created == SDL_FALSE;
    }

    /* Create surface(s) target for OpenGL ES */
    gfstatus =
        gf_3d_target_create(&wdata->target, didata->layer,
                            &didata->surface[0], surfaces,
                            didata->current_mode.w, didata->current_mode.h,
                            qnxgf_sdl_to_gf_pixelformat(didata->current_mode.
                                                        format));
    if (gfstatus != GF_ERR_OK) {
        /* Destroy just created context */
        eglDestroyContext(gfdata->egldisplay, wdata->gles_context);
        wdata->gles_context = EGL_NO_CONTEXT;

        /* Mark 3D target as unallocated */
        wdata->target_created = SDL_FALSE;
        SDL_SetError("GF: OpenGL ES target could not be created");
        return NULL;
    } else {
        wdata->target_created = SDL_TRUE;
    }

    /* Create target rendering surface on whole screen */
    wdata->gles_surface =
        eglCreateWindowSurface(gfdata->egldisplay,
                               wdata->gles_configs[wdata->gles_config],
                               wdata->target, NULL);
    if (wdata->gles_surface == EGL_NO_SURFACE) {
        /* Destroy 3d target */
        gf_3d_target_free(wdata->target);
        wdata->target_created = SDL_FALSE;

        /* Destroy OpenGL ES context */
        eglDestroyContext(gfdata->egldisplay, wdata->gles_context);
        wdata->gles_context = EGL_NO_CONTEXT;

        SDL_SetError("GF: OpenGL ES surface could not be created");
        return NULL;
    }

    /* Make just created context current */
    status =
        eglMakeCurrent(gfdata->egldisplay, wdata->gles_surface,
                       wdata->gles_surface, wdata->gles_context);
    if (status != EGL_TRUE) {
        /* Destroy OpenGL ES surface */
        eglDestroySurface(gfdata->egldisplay, wdata->gles_surface);

        /* Destroy 3d target */
        gf_3d_target_free(wdata->target);
        wdata->target_created = SDL_FALSE;

        /* Destroy OpenGL ES context */
        eglDestroyContext(gfdata->egldisplay, wdata->gles_context);
        wdata->gles_context = EGL_NO_CONTEXT;

        /* Failed to set current GL ES context */
        SDL_SetError("GF: Can't set OpenGL ES context on creation");
        return NULL;
    }

    /* Setup into SDL internals state of OpenGL ES:  */
    /* it is accelerated or not                      */
    if ((didata->caps & SDL_GF_ACCELERATED_3D) == SDL_GF_ACCELERATED_3D) {
        _this->gl_config.accelerated = 1;
    } else {
        _this->gl_config.accelerated = 0;
    }

    /* Always clear stereo enable, since OpenGL ES do not supports stereo */
    _this->gl_config.stereo = 0;

    /* Get back samples and samplebuffers configurations. Rest framebuffer */
    /* parameters could be obtained through the OpenGL ES API              */
    status =
        eglGetConfigAttrib(gfdata->egldisplay,
                           wdata->gles_configs[wdata->gles_config],
                           EGL_SAMPLES, &attr_value);
    if (status == EGL_TRUE) {
        _this->gl_config.multisamplesamples = attr_value;
    }
    status =
        eglGetConfigAttrib(gfdata->egldisplay,
                           wdata->gles_configs[wdata->gles_config],
                           EGL_SAMPLE_BUFFERS, &attr_value);
    if (status == EGL_TRUE) {
        _this->gl_config.multisamplebuffers = attr_value;
    }

    /* Get back stencil and depth buffer sizes */
    status =
        eglGetConfigAttrib(gfdata->egldisplay,
                           wdata->gles_configs[wdata->gles_config],
                           EGL_DEPTH_SIZE, &attr_value);
    if (status == EGL_TRUE) {
        _this->gl_config.depth_size = attr_value;
    }
    status =
        eglGetConfigAttrib(gfdata->egldisplay,
                           wdata->gles_configs[wdata->gles_config],
                           EGL_STENCIL_SIZE, &attr_value);
    if (status == EGL_TRUE) {
        _this->gl_config.stencil_size = attr_value;
    }

    /* Restore cursor if it was visible */
    if (didata->cursor_visible == SDL_TRUE) {
        gf_cursor_set(didata->display, 0, &didata->cursor);
        gf_cursor_enable(didata->display, 0);
    }

    /* GL ES context was successfully created */
    return wdata->gles_context;
#else
    SDL_SetError("GF: OpenGL ES support is not compiled in");
    return NULL;
#endif /* SDL_VIDEO_OPENGL_ES */
}

int
qnxgf_gl_makecurrent(_THIS, SDL_Window * window, SDL_GLContext context)
{
#if defined(SDL_VIDEO_OPENGL_ES)
    SDL_VideoData *gfdata = (SDL_VideoData *) _this->driverdata;
    SDL_WindowData *wdata;
    EGLBoolean status;

    if ((window == NULL) && (context == NULL)) {
        status =
            eglMakeCurrent(gfdata->egldisplay, EGL_NO_SURFACE, EGL_NO_SURFACE,
                           EGL_NO_CONTEXT);
        if (status != EGL_TRUE) {
            /* Failed to set current GL ES context */
            SDL_SetError("GF: Can't set OpenGL ES context");
            return -1;
        }
    } else {
        wdata = (SDL_WindowData *) window->driverdata;
        status =
            eglMakeCurrent(gfdata->egldisplay, wdata->gles_surface,
                           wdata->gles_surface, wdata->gles_context);
        if (status != EGL_TRUE) {
            /* Failed to set current GL ES context */
            SDL_SetError("GF: Can't set OpenGL ES context");
            return -1;
        }
    }

    return 0;
#else
    SDL_SetError("GF: OpenGL ES support is not compiled in");
    return -1;
#endif /* SDL_VIDEO_OPENGL_ES */
}

int
qnxgf_gl_setswapinterval(_THIS, int interval)
{
#if defined(SDL_VIDEO_OPENGL_ES)
    SDL_VideoData *gfdata = (SDL_VideoData *) _this->driverdata;
    EGLBoolean status;

    /* Check if OpenGL ES connection has been initialized */
    if (gfdata->egldisplay != EGL_NO_DISPLAY) {
        /* Set swap OpenGL ES interval */
        status = eglSwapInterval(gfdata->egldisplay, interval);
        if (status == EGL_TRUE) {
            /* Return success to upper level */
            gfdata->swapinterval = interval;
            return 0;
        }
    }

    /* Failed to set swap interval */
    SDL_SetError("GF: Cannot set swap interval");
    return -1;
#else
    SDL_SetError("GF: OpenGL ES support is not compiled in");
    return -1;
#endif /* SDL_VIDEO_OPENGL_ES */
}

int
qnxgf_gl_getswapinterval(_THIS)
{
#if defined(SDL_VIDEO_OPENGL_ES)
    SDL_VideoData *gfdata = (SDL_VideoData *) _this->driverdata;

    /* Return default swap interval value */
    return gfdata->swapinterval;
#else
    SDL_SetError("GF: OpenGL ES support is not compiled in");
    return -1;
#endif /* SDL_VIDEO_OPENGL_ES */
}

void
qnxgf_gl_swapwindow(_THIS, SDL_Window * window)
{
#if defined(SDL_VIDEO_OPENGL_ES)
    SDL_VideoData *gfdata = (SDL_VideoData *) _this->driverdata;
    SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;

    /* Finish all drawings */
    glFinish();

    /* Swap buffers */
    eglSwapBuffers(gfdata->egldisplay, wdata->gles_surface);
#else
    SDL_SetError("GF: OpenGL ES support is not compiled in");
    return;
#endif /* SDL_VIDEO_OPENGL_ES */
}

void
qnxgf_gl_deletecontext(_THIS, SDL_GLContext context)
{
#if defined(SDL_VIDEO_OPENGL_ES)
    SDL_VideoData *gfdata = (SDL_VideoData *) _this->driverdata;
    EGLBoolean status;

    /* Check if OpenGL ES connection has been initialized */
    if (gfdata->egldisplay != EGL_NO_DISPLAY) {
        if (context != EGL_NO_CONTEXT) {
            status = eglDestroyContext(gfdata->egldisplay, context);
            if (status != EGL_TRUE) {
                /* Error during OpenGL ES context destroying */
                SDL_SetError("GF: OpenGL ES context destroy error");
                return;
            }
        }
    }

    return;
#else
    SDL_SetError("GF: OpenGL ES support is not compiled in");
    return;
#endif /* SDL_VIDEO_OPENGL_ES */
}

/*****************************************************************************/
/* SDL Event handling function                                               */
/*****************************************************************************/
void
qnxgf_pumpevents(_THIS)
{
}

/*****************************************************************************/
/* SDL screen saver related functions                                        */
/*****************************************************************************/
void
qnxgf_suspendscreensaver(_THIS)
{
    /* There is no screensaver in pure console, it may exist when running */
    /* GF under Photon, but I do not know, how to disable screensaver     */
}

/* vi: set ts=4 sw=4 expandtab: */
