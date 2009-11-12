/*
	SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2009 Sam Lantinga

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Library General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

	You should have received a copy of the GNU Library General Public
	License along with this library; if not, write to the Free
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

	Sam Lantinga
	slouken@libsdl.org
*/
#include "SDL_config.h"

#ifndef SDL_JOYSTICK_IOKIT_H


#if MAC_OS_X_VERSION_MIN_REQUIRED == 1030
#include "10.3.9-FIX/IOHIDLib.h"
#else
#include <IOKit/hid/IOHIDLib.h>
#endif
#include <IOKit/hid/IOHIDKeys.h>


struct recElement
{
    IOHIDElementCookie cookie;  /* unique value which identifies element, will NOT change */
    long usagePage, usage;      /* HID usage */
    long min;                   /* reported min value possible */
    long max;                   /* reported max value possible */
#if 0
    /* TODO: maybe should handle the following stuff somehow? */

    long scaledMin;             /* reported scaled min value possible */
    long scaledMax;             /* reported scaled max value possible */
    long size;                  /* size in bits of data return from element */
    Boolean relative;           /* are reports relative to last report (deltas) */
    Boolean wrapping;           /* does element wrap around (one value higher than max is min) */
    Boolean nonLinear;          /* are the values reported non-linear relative to element movement */
    Boolean preferredState;     /* does element have a preferred state (such as a button) */
    Boolean nullState;          /* does element have null state */
#endif                          /* 0 */

    /* runtime variables used for auto-calibration */
    long minReport;             /* min returned value */
    long maxReport;             /* max returned value */

    struct recElement *pNext;   /* next element in list */
};
typedef struct recElement recElement;

struct joystick_hwdata
{
    io_service_t ffservice;     /* Interface for force feedback, 0 = no ff */
    IOHIDDeviceInterface **interface;   /* interface to device, NULL = no interface */

    char product[256];          /* name of product */
    long usage;                 /* usage page from IOUSBHID Parser.h which defines general usage */
    long usagePage;             /* usage within above page from IOUSBHID Parser.h which defines specific usage */

    long axes;                  /* number of axis (calculated, not reported by device) */
    long buttons;               /* number of buttons (calculated, not reported by device) */
    long hats;                  /* number of hat switches (calculated, not reported by device) */
    long elements;              /* number of total elements (shouldbe total of above) (calculated, not reported by device) */

    recElement *firstAxis;
    recElement *firstButton;
    recElement *firstHat;

    int removed;
    int uncentered;

    struct joystick_hwdata *pNext;      /* next device */
};
typedef struct joystick_hwdata recDevice;


#endif /* SDL_JOYSTICK_IOKIT_H */
