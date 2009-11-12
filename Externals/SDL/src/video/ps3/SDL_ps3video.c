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

/* SDL PS3 video driver implementation based on dummy video driver
 *
 * Initial work by Ryan C. Gordon (icculus@icculus.org). A good portion
 *  of this was cut-and-pasted from Stephane Peter's work in the AAlib
 *  SDL video driver.  Renamed to "DUMMY" by Sam Lantinga.
 */

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include "SDL_ps3video.h"
#include "SDL_ps3spe_c.h"
#include "SDL_ps3events_c.h"
#include "SDL_ps3render_c.h"
#include "SDL_ps3modes_c.h"

#include <fcntl.h>
#include <linux/fb.h>
#include <asm/ps3fb.h>
#include <sys/mman.h>

#define PS3VID_DRIVER_NAME "ps3"

/* Initialization/Query functions */
static int PS3_VideoInit(_THIS);
static void PS3_VideoQuit(_THIS);

/* Stores the SPE executable name of fb_writer_spu */
extern spe_program_handle_t fb_writer_spu;

/* PS3 driver bootstrap functions */

static int
PS3_Available(void)
{
    deprintf(1, "+PS3_Available()\n");
    const char *envr = SDL_getenv("SDL_VIDEODRIVER");
    if ((envr) && (SDL_strcmp(envr, PS3VID_DRIVER_NAME) == 0)) {
        return (1);
    }

    deprintf(1, "-PS3_Available()\n");
    return (0);
}

static void
PS3_DeleteDevice(SDL_VideoDevice * device)
{
    deprintf(1, "+PS3_DeleteDevice()\n");
    SDL_free(device->driverdata);
    SDL_free(device);
    deprintf(1, "-PS3_DeleteDevice()\n");
}

static SDL_VideoDevice *
PS3_CreateDevice(int devindex)
{
    deprintf(1, "+PS3_CreateDevice()\n");
    SDL_VideoDevice *device;
    SDL_VideoData *data;

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        if (device) {
            SDL_free(device);
        }
        return (0);
    }
    data = (struct SDL_VideoData *) SDL_calloc(1, sizeof(SDL_VideoData));
    if (!data) {
        SDL_OutOfMemory();
        SDL_free(device);
        return (0);
    }
    device->driverdata = data;

    /* Set the function pointers */
    device->VideoInit = PS3_VideoInit;
    device->VideoQuit = PS3_VideoQuit;
    device->SetDisplayMode = PS3_SetDisplayMode;
    device->GetDisplayModes = PS3_GetDisplayModes;
    device->PumpEvents = PS3_PumpEvents;

    device->free = PS3_DeleteDevice;

    deprintf(1, "-PS3_CreateDevice()\n");
    return device;
}

VideoBootStrap PS3_bootstrap = {
    PS3VID_DRIVER_NAME, "SDL PS3 Cell video driver",
    PS3_Available, PS3_CreateDevice
};


int
PS3_VideoInit(_THIS)
{
    deprintf(1, "PS3_VideoInit()\n");

    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
    SDL_DisplayMode mode;

    /* Create SPU fb_parms and thread structure */
    data->fb_parms = (struct fb_writer_parms_t *)
        memalign(16, sizeof(struct fb_writer_parms_t));
    data->fb_thread_data = (spu_data_t *) malloc(sizeof(spu_data_t));
    if (data->fb_parms == NULL || data->fb_thread_data == NULL) {
        SDL_OutOfMemory();
        return -1;
    }
    data->fb_thread_data->program = fb_writer_spu;
    data->fb_thread_data->program_name = "fb_writer_spu";
    data->fb_thread_data->argp = (void *)data->fb_parms;
    data->fb_thread_data->keepalive = 1;
    data->fb_thread_data->booted = 0;

    SPE_Start(data->fb_thread_data);

    /* Open the device */
    data->fbdev = open(PS3DEV, O_RDWR);
    if (data->fbdev < 0) {
        SDL_SetError("[PS3] Unable to open device %s", PS3DEV);
        return -1;
    }

    /* Take control of frame buffer from kernel, for details see
     * http://felter.org/wesley/files/ps3/linux-20061110-docs/ApplicationProgrammingEnvironment.html
     * kernel will no longer flip the screen itself
     */
    ioctl(data->fbdev, PS3FB_IOCTL_ON, 0);

    /* Unblank screen */
    ioctl(data->fbdev, FBIOBLANK, 0);

    struct fb_fix_screeninfo fb_finfo;
    if (ioctl(data->fbdev, FBIOGET_FSCREENINFO, &fb_finfo)) {
        SDL_SetError("[PS3] Can't get fixed screeninfo");
        return (0);
    }

    /* Note: on PS3, fb_finfo.smem_len is enough for double buffering */
    if ((data->frame_buffer = (uint8_t *)mmap(0, fb_finfo.smem_len,
        PROT_READ | PROT_WRITE, MAP_SHARED,
        data->fbdev, 0)) == (uint8_t *) - 1) {
        SDL_SetError("[PS3] Can't mmap for %s", PS3DEV);
        return (0);
    } else {
        /* Enable double buffering */
    }

    /* Blank screen */
    memset(data->frame_buffer, 0x00, fb_finfo.smem_len);

    PS3_InitModes(_this);
    SDL_AddRenderDriver(0, &SDL_PS3_RenderDriver);

    /* We're done! */
    return 0;
}

void
PS3_VideoQuit(_THIS)
{
    deprintf(1, "PS3_VideoQuit()\n");
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;

    PS3_QuitModes(_this);

    /* Unmap framebuffer */
    if (data->frame_buffer) {
        struct fb_fix_screeninfo fb_finfo;
        if (ioctl(data->fbdev, FBIOGET_FSCREENINFO, &fb_finfo) != -1) {
            munmap(data->frame_buffer, fb_finfo.smem_len);
            data->frame_buffer = 0;
        }
    }

    /* Shutdown SPE and related resources */
    if (data->fb_parms)
        free((void *)data->fb_parms);
    if (data->fb_thread_data) {
        SPE_Shutdown(data->fb_thread_data);
        free((void *)data->fb_thread_data);
    }

    /* Close device */
    if (data->fbdev) {
        /* Give control of frame buffer back to kernel */
        ioctl(data->fbdev, PS3FB_IOCTL_OFF, 0);
        close(data->fbdev);
        data->fbdev = -1;
    }
}

/* vi: set ts=4 sw=4 expandtab: */
