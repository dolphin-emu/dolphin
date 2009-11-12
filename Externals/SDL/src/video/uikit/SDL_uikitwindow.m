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

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include "SDL_uikitvideo.h"
#include "SDL_uikitevents.h"
#include "SDL_uikitwindow.h"
#import "SDL_uikitappdelegate.h"

#import "SDL_uikitopenglview.h"
#import "SDL_renderer_sw.h"

#include <UIKit/UIKit.h>
#include <Foundation/Foundation.h>

static int SetupWindowData(_THIS, SDL_Window *window, UIWindow *uiwindow, SDL_bool created) {

    SDL_WindowData *data;
		
    /* Allocate the window data */
    data = (SDL_WindowData *)SDL_malloc(sizeof(*data));
    if (!data) {
        SDL_OutOfMemory();
        return -1;
    }
    data->windowID = window->id;
    data->uiwindow = uiwindow;
	data->view = nil;
		
    /* Fill in the SDL window with the window data */
	{
        window->x = 0;
        window->y = 0;
        window->w = (int)uiwindow.frame.size.width;
        window->h = (int)uiwindow.frame.size.height;
    }
	
	window->driverdata = data;
	
	window->flags &= ~SDL_WINDOW_RESIZABLE;		/* window is NEVER resizeable */
	window->flags |= SDL_WINDOW_OPENGL;			/* window is always OpenGL */
	window->flags |= SDL_WINDOW_FULLSCREEN;		/* window is always fullscreen */
	window->flags |= SDL_WINDOW_SHOWN;			/* only one window on iPod touch, always shown */
	window->flags |= SDL_WINDOW_INPUT_FOCUS;	/* always has input focus */	

	/* SDL_WINDOW_BORDERLESS controls whether status bar is hidden */
	if (window->flags & SDL_WINDOW_BORDERLESS) {
		[UIApplication sharedApplication].statusBarHidden = YES;
	}
	else {
		[UIApplication sharedApplication].statusBarHidden = NO;
	}
	
    return 0;
	
}

int UIKit_CreateWindow(_THIS, SDL_Window *window) {
		
	/* We currently only handle single window applications on iPhone */
	if (nil != [SDLUIKitDelegate sharedAppDelegate].window) {
		SDL_SetError("Window already exists, no multi-window support.");
		return -1;
	}
	
	/* ignore the size user requested, and make a fullscreen window */
	UIWindow *uiwindow = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
	
	if (SetupWindowData(_this, window, uiwindow, SDL_TRUE) < 0) {
        [uiwindow release];
        return -1;
    }	
	
	// This saves the main window in the app delegate so event callbacks can do stuff on the window.
	// This assumes a single window application design and needs to be fixed for multiple windows.
	[SDLUIKitDelegate sharedAppDelegate].window = uiwindow;
	[SDLUIKitDelegate sharedAppDelegate].windowID = window->id;
	[uiwindow release]; /* release the window (the app delegate has retained it) */
	
	return 1;
	
}

void UIKit_DestroyWindow(_THIS, SDL_Window * window) {
	/* don't worry, the delegate will automatically release the window */
	
	SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
	if (data) {
		SDL_free( window->driverdata );
	}

	/* this will also destroy the window */
	[SDLUIKitDelegate sharedAppDelegate].window = nil;
	[SDLUIKitDelegate sharedAppDelegate].windowID = 0;

}

/* vi: set ts=4 sw=4 expandtab: */
