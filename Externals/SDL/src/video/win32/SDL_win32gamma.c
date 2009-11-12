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

#include "SDL_win32video.h"


int
WIN_SetDisplayGammaRamp(_THIS, Uint16 * ramp)
{
#ifdef _WIN32_WCE
    return -1;
#else
    SDL_DisplayData *data = (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;
    HDC hdc;
    BOOL succeeded = FALSE;

    hdc = CreateDC(data->DeviceName, NULL, NULL, NULL);
    if (hdc) {
        succeeded = SetDeviceGammaRamp(hdc, ramp);
        if (!succeeded) {
            WIN_SetError("SetDeviceGammaRamp()");
        }
        DeleteDC(hdc);
    }
    return succeeded ? 0 : -1;
#endif
}

int
WIN_GetDisplayGammaRamp(_THIS, Uint16 * ramp)
{
#ifdef _WIN32_WCE
    return -1;
#else
    SDL_DisplayData *data = (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;
    HDC hdc;
    BOOL succeeded = FALSE;

    hdc = CreateDC(data->DeviceName, NULL, NULL, NULL);
    if (hdc) {
        succeeded = GetDeviceGammaRamp(hdc, ramp);
        if (!succeeded) {
            WIN_SetError("GetDeviceGammaRamp()");
        }
        DeleteDC(hdc);
    }
    return succeeded ? 0 : -1;
#endif
}

/* vi: set ts=4 sw=4 expandtab: */
