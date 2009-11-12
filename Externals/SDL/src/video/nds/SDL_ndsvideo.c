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

/* SDL Nintendo DS video driver implementation
 * based on dummy driver:
 *  Initial work by Ryan C. Gordon (icculus@icculus.org). A good portion
 *  of this was cut-and-pasted from Stephane Peter's work in the AAlib
 *  SDL video driver.  Renamed to "DUMMY" by Sam Lantinga.
 */

#include <stdio.h>
#include <stdlib.h>
#include <nds.h>
#include <nds/arm9/video.h>

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include "SDL_ndsvideo.h"
#include "SDL_ndsevents_c.h"
#include "SDL_ndsrender_c.h"

#define NDSVID_DRIVER_NAME "nds"

/* Initialization/Query functions */
static int NDS_VideoInit(_THIS);
static int NDS_SetDisplayMode(_THIS, SDL_DisplayMode * mode);
static void NDS_VideoQuit(_THIS);


/* SDL NDS driver bootstrap functions */
static int
NDS_Available(void)
{
    return (1);                 /* always here */
}

static void
NDS_DeleteDevice(SDL_VideoDevice * device)
{
    SDL_free(device);
}

static SDL_VideoDevice *
NDS_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        if (device) {
            SDL_free(device);
        }
        return (0);
    }

    /* Set the function pointers */
    device->VideoInit = NDS_VideoInit;
    device->VideoQuit = NDS_VideoQuit;
    device->SetDisplayMode = NDS_SetDisplayMode;
    device->PumpEvents = NDS_PumpEvents;

    device->num_displays = 2;   /* DS = dual screens */

    device->free = NDS_DeleteDevice;

    return device;
}

VideoBootStrap NDS_bootstrap = {
    NDSVID_DRIVER_NAME, "SDL NDS video driver",
    NDS_Available, NDS_CreateDevice
};

int
NDS_VideoInit(_THIS)
{
    SDL_DisplayMode mode;
    int i;

    /* simple 256x192x16x60 for now */
    mode.w = 256;
    mode.h = 192;
    mode.format = SDL_PIXELFORMAT_ABGR1555;
    mode.refresh_rate = 60;
    mode.driverdata = NULL;

    SDL_AddBasicVideoDisplay(&mode);
    SDL_AddRenderDriver(0, &NDS_RenderDriver);

    SDL_zero(mode);
    SDL_AddDisplayMode(0, &mode);

    powerON(POWER_ALL_2D);
    irqInit();
    irqEnable(IRQ_VBLANK);
    NDS_SetDisplayMode(_this, &mode);

    return 0;
}

static int
NDS_SetDisplayMode(_THIS, SDL_DisplayMode * mode)
{
    /* right now this function is just hard-coded for 256x192 ABGR1555 */
    videoSetMode(MODE_5_2D | DISPLAY_BG2_ACTIVE | DISPLAY_BG3_ACTIVE | DISPLAY_BG_EXT_PALETTE | DISPLAY_SPR_1D_LAYOUT | DISPLAY_SPR_1D_BMP | DISPLAY_SPR_1D_BMP_SIZE_256 |      /* (try 128 if 256 is trouble.) */
                 DISPLAY_SPR_ACTIVE | DISPLAY_SPR_EXT_PALETTE); /* display on main core
                                                                   with lots of flags set for
                                                                   flexibility/capacity to render */

    /* hopefully these cover all the various things we might need to do */
    vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
    vramSetBankB(VRAM_B_MAIN_BG_0x06020000);
    vramSetBankC(VRAM_C_SUB_BG_0x06200000);
    vramSetBankD(VRAM_D_MAIN_BG_0x06040000);    /* not a typo. vram d can't sub */
    vramSetBankE(VRAM_E_MAIN_SPRITE);
    vramSetBankF(VRAM_F_OBJ_EXT_PALETTE);
    vramSetBankG(VRAM_G_BG_EXT_PALETTE);
    vramSetBankH(VRAM_H_SUB_BG_EXT_PALETTE);
    vramSetBankI(VRAM_I_SUB_SPRITE);

    videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE);    /* debug text on sub
                                                           TODO: this will change
                                                           when multi-head is
                                                           introduced in render */

    return 0;
}

void
NDS_VideoQuit(_THIS)
{
    videoSetMode(DISPLAY_SCREEN_OFF);
    videoSetModeSub(DISPLAY_SCREEN_OFF);
    vramSetMainBanks(VRAM_A_LCD, VRAM_B_LCD, VRAM_C_LCD, VRAM_D_LCD);
    vramSetBankE(VRAM_E_LCD);
    vramSetBankF(VRAM_F_LCD);
    vramSetBankG(VRAM_G_LCD);
    vramSetBankH(VRAM_H_LCD);
    vramSetBankI(VRAM_I_LCD);
}

/* vi: set ts=4 sw=4 expandtab: */
