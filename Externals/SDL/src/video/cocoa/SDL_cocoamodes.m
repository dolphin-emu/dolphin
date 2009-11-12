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

#include "SDL_cocoavideo.h"

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1050
/* 
    Add methods to get at private members of NSScreen. 
    Since there is a bug in Apple's screen switching code
    that does not update this variable when switching
    to fullscreen, we'll set it manually (but only for the
    main screen).
*/
@interface NSScreen (NSScreenAccess)
- (void) setFrame:(NSRect)frame;
@end

@implementation NSScreen (NSScreenAccess)
- (void) setFrame:(NSRect)frame;
{
    _frame = frame;
}
@end
#endif

static void
CG_SetError(const char *prefix, CGDisplayErr result)
{
    const char *error;

    switch (result) {
    case kCGErrorFailure:
        error = "kCGErrorFailure";
        break;
    case kCGErrorIllegalArgument:
        error = "kCGErrorIllegalArgument";
        break;
    case kCGErrorInvalidConnection:
        error = "kCGErrorInvalidConnection";
        break;
    case kCGErrorInvalidContext:
        error = "kCGErrorInvalidContext";
        break;
    case kCGErrorCannotComplete:
        error = "kCGErrorCannotComplete";
        break;
    case kCGErrorNameTooLong:
        error = "kCGErrorNameTooLong";
        break;
    case kCGErrorNotImplemented:
        error = "kCGErrorNotImplemented";
        break;
    case kCGErrorRangeCheck:
        error = "kCGErrorRangeCheck";
        break;
    case kCGErrorTypeCheck:
        error = "kCGErrorTypeCheck";
        break;
    case kCGErrorNoCurrentPoint:
        error = "kCGErrorNoCurrentPoint";
        break;
    case kCGErrorInvalidOperation:
        error = "kCGErrorInvalidOperation";
        break;
    case kCGErrorNoneAvailable:
        error = "kCGErrorNoneAvailable";
        break;
    default:
        error = "Unknown Error";
        break;
    }
    SDL_SetError("%s: %s", prefix, error);
}

static SDL_bool
GetDisplayMode(CFDictionaryRef moderef, SDL_DisplayMode *mode)
{
    SDL_DisplayModeData *data;
    CFNumberRef number;
    long width, height, bpp, refreshRate;

    data = (SDL_DisplayModeData *) SDL_malloc(sizeof(*data));
    if (!data) {
        return SDL_FALSE;
    }
    data->moderef = moderef;

    number = CFDictionaryGetValue(moderef, kCGDisplayWidth);
    CFNumberGetValue(number, kCFNumberLongType, &width);
    number = CFDictionaryGetValue(moderef, kCGDisplayHeight);
    CFNumberGetValue(number, kCFNumberLongType, &height);
    number = CFDictionaryGetValue(moderef, kCGDisplayBitsPerPixel);
    CFNumberGetValue(number, kCFNumberLongType, &bpp);
    number = CFDictionaryGetValue(moderef, kCGDisplayRefreshRate);
    CFNumberGetValue(number, kCFNumberLongType, &refreshRate);

    mode->format = SDL_PIXELFORMAT_UNKNOWN;
    switch (bpp) {
    case 8:
        mode->format = SDL_PIXELFORMAT_INDEX8;
        break;
    case 16:
        mode->format = SDL_PIXELFORMAT_ARGB1555;
        break;
    case 32:
        mode->format = SDL_PIXELFORMAT_ARGB8888;
        break;
    }
    mode->w = width;
    mode->h = height;
    mode->refresh_rate = refreshRate;
    mode->driverdata = data;
    return SDL_TRUE;
}

void
Cocoa_InitModes(_THIS)
{
    CGDisplayErr result;
    CGDirectDisplayID *displays;
    CGDisplayCount numDisplays;
    int i;

    result = CGGetOnlineDisplayList(0, NULL, &numDisplays);
    if (result != kCGErrorSuccess) {
        CG_SetError("CGGetOnlineDisplayList()", result);
        return;
    }
    displays = SDL_stack_alloc(CGDirectDisplayID, numDisplays);
    result = CGGetOnlineDisplayList(numDisplays, displays, &numDisplays);
    if (result != kCGErrorSuccess) {
        CG_SetError("CGGetOnlineDisplayList()", result);
        SDL_stack_free(displays);
        return;
    }

    for (i = 0; i < numDisplays; ++i) {
        SDL_VideoDisplay display;
        SDL_DisplayData *displaydata;
        SDL_DisplayMode mode;
        CFDictionaryRef moderef;

        if (CGDisplayIsInMirrorSet(displays[i])) {
            continue;
        }
        moderef = CGDisplayCurrentMode(displays[i]);
        if (!moderef) {
            continue;
        }

        displaydata = (SDL_DisplayData *) SDL_malloc(sizeof(*displaydata));
        if (!displaydata) {
            continue;
        }
        displaydata->display = displays[i];

        SDL_zero(display);
        if (!GetDisplayMode (moderef, &mode)) {
            SDL_free(displaydata);
            continue;
        }
        display.desktop_mode = mode;
        display.current_mode = mode;
        display.driverdata = displaydata;
        SDL_AddVideoDisplay(&display);
    }
    SDL_stack_free(displays);
}

static void
AddDisplayMode(const void *moderef, void *context)
{
    SDL_VideoDevice *_this = (SDL_VideoDevice *) context;
    SDL_DisplayMode mode;

    if (GetDisplayMode(moderef, &mode)) {
        SDL_AddDisplayMode(_this->current_display, &mode);
    }
}

void
Cocoa_GetDisplayModes(_THIS)
{
    SDL_DisplayData *data = (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;
    CFArrayRef modes;
    CFRange range;

    modes = CGDisplayAvailableModes(data->display);
    if (!modes) {
        return;
    }
    range.location = 0;
    range.length = CFArrayGetCount(modes);
    CFArrayApplyFunction(modes, range, AddDisplayMode, _this);
}

int
Cocoa_SetDisplayMode(_THIS, SDL_DisplayMode * mode)
{
    SDL_DisplayData *displaydata = (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;
    SDL_DisplayModeData *data = (SDL_DisplayModeData *) mode->driverdata;
    CGDisplayFadeReservationToken fade_token = kCGDisplayFadeReservationInvalidToken;
    CGError result;
    
    /* Fade to black to hide resolution-switching flicker */
    if (CGAcquireDisplayFadeReservation(5, &fade_token) == kCGErrorSuccess) {
        CGDisplayFade(fade_token, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0.0, 0.0, 0.0, TRUE);
    }

    /* Put up the blanking window (a window above all other windows) */
    result = CGDisplayCapture(displaydata->display);
    if (result != kCGErrorSuccess) {
        CG_SetError("CGDisplayCapture()", result);
        goto ERR_NO_CAPTURE;
    }

    /* Do the physical switch */
    result = CGDisplaySwitchToMode(displaydata->display, data->moderef);
    if (result != kCGErrorSuccess) {
        CG_SetError("CGDisplaySwitchToMode()", result);
        goto ERR_NO_SWITCH;
    }

    /* Hide the menu bar so it doesn't intercept events */
    HideMenuBar();

    /* Fade in again (asynchronously) */
    if (fade_token != kCGDisplayFadeReservationInvalidToken) {
        CGDisplayFade(fade_token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, FALSE);
        CGReleaseDisplayFadeReservation(fade_token);
    }

    [[NSApp mainWindow] makeKeyAndOrderFront: nil];

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1050
    /* 
        There is a bug in Cocoa where NSScreen doesn't synchronize
        with CGDirectDisplay, so the main screen's frame is wrong.
        As a result, coordinate translation produces incorrect results.
        We can hack around this bug by setting the screen rect
        ourselves. This hack should be removed if/when the bug is fixed.
    */
    [[NSScreen mainScreen] setFrame:NSMakeRect(0,0,mode->w,mode->h)]; 
#endif

    return 0;

    /* Since the blanking window covers *all* windows (even force quit) correct recovery is crucial */
ERR_NO_SWITCH:
    CGDisplayRelease(displaydata->display);
ERR_NO_CAPTURE:
    if (fade_token != kCGDisplayFadeReservationInvalidToken) {
        CGDisplayFade (fade_token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, FALSE);
        CGReleaseDisplayFadeReservation(fade_token);
    }
    return -1;
}

void
Cocoa_QuitModes(_THIS)
{
    int i, saved_display;

    saved_display = _this->current_display;
    for (i = 0; i < _this->num_displays; ++i) {
        SDL_VideoDisplay *display = &_this->displays[i];

        if (display->current_mode.driverdata != display->desktop_mode.driverdata) {
            _this->current_display = i;
            Cocoa_SetDisplayMode(_this, &display->desktop_mode);
        }
    }
    CGReleaseAllDisplays();
    ShowMenuBar();

    _this->current_display = saved_display;
}

/* vi: set ts=4 sw=4 expandtab: */
