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

#include "SDL_ps3video.h"

void
PS3_InitModes(_THIS)
{
    deprintf(1, "+PS3_InitModes()\n");
    SDL_VideoDisplay display;
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
    SDL_DisplayMode mode;
    PS3_DisplayModeData *modedata;
    unsigned long vid = 0;

    modedata = (PS3_DisplayModeData *) SDL_malloc(sizeof(*modedata));
    if (!modedata) {
        return;
    }

    /* Setting up the DisplayMode based on current settings */
    struct ps3fb_ioctl_res res;
    if (ioctl(data->fbdev, PS3FB_IOCTL_SCREENINFO, &res)) {
        SDL_SetError("Can't get PS3FB_IOCTL_SCREENINFO");
    }
    mode.format = SDL_PIXELFORMAT_RGB888;
    mode.refresh_rate = 0;
    mode.w = res.xres;
    mode.h = res.yres;

    /* Setting up driver specific mode data,
     * Get the current ps3 specific videmode number */
    if (ioctl(data->fbdev, PS3FB_IOCTL_GETMODE, (unsigned long)&vid)) {
        SDL_SetError("Can't get PS3FB_IOCTL_GETMODE");
    }
    deprintf(2, "PS3FB_IOCTL_GETMODE = %u\n", vid);
    modedata->mode = vid;
    mode.driverdata = modedata;

    /* Set display's videomode and add it */
    SDL_zero(display);
    display.desktop_mode = mode;
    display.current_mode = mode;

    SDL_AddVideoDisplay(&display);
    deprintf(1, "-PS3_InitModes()\n");
}

/* DisplayModes available on the PS3 */
static SDL_DisplayMode ps3fb_modedb[] = {
    /* VESA */
    {SDL_PIXELFORMAT_RGB888, 1280, 768, 0, NULL}, // WXGA
    {SDL_PIXELFORMAT_RGB888, 1280, 1024, 0, NULL}, // SXGA
    {SDL_PIXELFORMAT_RGB888, 1920, 1200, 0, NULL}, // WUXGA
    /* Native resolutions (progressive, "fullscreen") */
    {SDL_PIXELFORMAT_RGB888, 720, 480, 0, NULL}, // 480p
    {SDL_PIXELFORMAT_RGB888, 1280, 720, 0, NULL}, // 720p
    {SDL_PIXELFORMAT_RGB888, 1920, 1080, 0, NULL} // 1080p
};

/* PS3 videomode number according to ps3fb_modedb */
static PS3_DisplayModeData ps3fb_data[] = {
    {11}, {12}, {13}, {130}, {131}, {133}, 
};

void
PS3_GetDisplayModes(_THIS) {
    deprintf(1, "+PS3_GetDisplayModes()\n");
    SDL_DisplayMode mode;
    unsigned int nummodes;

    nummodes = sizeof(ps3fb_modedb) / sizeof(SDL_DisplayMode);

    int n;
    for (n=0; n<nummodes; ++n) {
        /* Get driver specific mode data */
        ps3fb_modedb[n].driverdata = &ps3fb_data[n];

        /* Add DisplayMode to list */
        deprintf(2, "Adding resolution %u x %u\n", ps3fb_modedb[n].w, ps3fb_modedb[n].h);
        SDL_AddDisplayMode(_this->current_display, &ps3fb_modedb[n]);
    }
    deprintf(1, "-PS3_GetDisplayModes()\n");
}

int
PS3_SetDisplayMode(_THIS, SDL_DisplayMode * mode)
{
    deprintf(1, "+PS3_SetDisplayMode()\n");
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
    PS3_DisplayModeData *dispdata = (PS3_DisplayModeData *) mode->driverdata;

    /* Set the new DisplayMode */
    deprintf(2, "Setting PS3FB_MODE to %u\n", dispdata->mode);
    if (ioctl(data->fbdev, PS3FB_IOCTL_SETMODE, (unsigned long)&dispdata->mode)) {
        deprintf(2, "Could not set PS3FB_MODE\n");
        SDL_SetError("Could not set PS3FB_MODE\n");
        return -1;
    }

    deprintf(1, "-PS3_SetDisplayMode()\n");
    return 0;
}

void
PS3_QuitModes(_THIS) {
    deprintf(1, "+PS3_QuitModes()\n");

    /* There was no mem allocated for driverdata */
    int i, j;
    for (i = _this->num_displays; i--;) {
        SDL_VideoDisplay *display = &_this->displays[i];
        for (j = display->num_display_modes; j--;) {
            display->display_modes[j].driverdata = NULL;
        }
    }

    deprintf(1, "-PS3_QuitModes()\n");
}

/* vi: set ts=4 sw=4 expandtab: */
