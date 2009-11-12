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
#include "SDL_x11video.h"
#include "SDL_x11mouse.h"
#include "../../events/SDL_mouse_c.h"

#if SDL_VIDEO_DRIVER_X11_XINPUT
static void
X11_FreeMouse(SDL_Mouse * mouse)
{
    X11_MouseData *data = (X11_MouseData *) mouse->driverdata;

    if (data) {
        XCloseDevice(data->display, data->device);
        SDL_free(data);
    }
}
#endif

void
X11_InitMouse(_THIS)
{
    SDL_Mouse mouse;
#if SDL_VIDEO_DRIVER_X11_XINPUT
    Display *display = ((SDL_VideoData *) _this->driverdata)->display;
    X11_MouseData *data;
    int i, j, n;
    XDeviceInfo *DevList;
    XAnyClassPtr deviceClass;
    int event_code;
    XEventClass xEvent;
#endif
    int num_mice = 0;

    SDL_zero(mouse);

#if SDL_VIDEO_DRIVER_X11_XINPUT
    /* we're getting the list of input devices */
    n = 0;
    if (SDL_X11_HAVE_XINPUT) {
        DevList = XListInputDevices(display, &n);
    }

    /* we're aquiring valuators: mice, tablets, etc. */
    for (i = 0; i < n; ++i) {
        /* if it's the core pointer or core keyborard we don't want it */
        if ((DevList[i].use != IsXPointer && DevList[i].use != IsXKeyboard)) {
            /* we have to check all of the device classes */
            deviceClass = DevList[i].inputclassinfo;
            for (j = 0; j < DevList[i].num_classes; ++j) {
                if (deviceClass->class == ValuatorClass) {      /* bingo ;) */
                    XValuatorInfo *valInfo;

                    data = (X11_MouseData *) SDL_calloc(1, sizeof(*data));
                    if (!data) {
                        continue;
                    }
                    data->display = display;
                    data->device = XOpenDevice(display, DevList[i].id);

                    /* motion events */
                    DeviceMotionNotify(data->device, event_code, xEvent);
                    if (xEvent) {
                        data->xevents[data->num_xevents++] = xEvent;
                        data->motion = event_code;
                    }

                    /* button events */
                    DeviceButtonPress(data->device, event_code, xEvent);
                    if (xEvent) {
                        data->xevents[data->num_xevents++] = xEvent;
                        data->button_pressed = event_code;
                    }
                    DeviceButtonRelease(data->device, event_code, xEvent);
                    if (xEvent) {
                        data->xevents[data->num_xevents++] = xEvent;
                        data->button_released = event_code;
                    }

                    /* proximity events */
                    ProximityIn(data->device, event_code, xEvent);
                    if (xEvent) {
                        data->xevents[data->num_xevents++] = xEvent;
                        data->proximity_in = event_code;
                    }
                    ProximityOut(data->device, event_code, xEvent);
                    if (xEvent) {
                        data->xevents[data->num_xevents++] = xEvent;
                        data->proximity_out = event_code;
                    }

                    SDL_zero(mouse);
                    mouse.id = DevList[i].id;
                    mouse.FreeMouse = X11_FreeMouse;
                    mouse.driverdata = data;

                    /* lets get the device parameters */
                    valInfo = (XValuatorInfo *) deviceClass;
                    /* if the device reports pressure, lets check it parameteres */
                    if (valInfo->num_axes > 2) {
                        SDL_AddMouse(&mouse, DevList[i].name,
                                     valInfo->axes[2].max_value,
                                     valInfo->axes[2].min_value, 1);
                    } else {
                        SDL_AddMouse(&mouse, DevList[i].name, 0, 0, 1);
                    }
#ifndef IsXExtensionPointer
#define IsXExtensionPointer 4
#endif
                    if (DevList[i].use == IsXExtensionPointer) {
                        ++num_mice;
                    }
                    break;
                }
                /* if it's not class we're interested in, lets go further */
                deviceClass =
                    (XAnyClassPtr) ((char *) deviceClass +
                                    deviceClass->length);
            }
        }
    }
    XFreeDeviceList(DevList);
#endif

    if (num_mice == 0) {
        SDL_zero(mouse);
        SDL_AddMouse(&mouse, "CorePointer", 0, 0, 1);
    }
}

void
X11_QuitMouse(_THIS)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;

    /* !!! FIXME: use XCloseDevice()? Or maybe handle under SDL_MouseQuit()? */

    /* let's delete all of the mice */
    SDL_MouseQuit();
}

/* vi: set ts=4 sw=4 expandtab: */
