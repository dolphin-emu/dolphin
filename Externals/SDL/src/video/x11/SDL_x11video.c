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

#include "SDL_x11video.h"

#if SDL_VIDEO_DRIVER_PANDORA
#include "SDL_x11opengles.h"
#endif

/* Initialization/Query functions */
static int X11_VideoInit(_THIS);
static void X11_VideoQuit(_THIS);

/* Find out what class name we should use */
static char *
get_classname()
{
    char *spot;
#if defined(__LINUX__) || defined(__FREEBSD__)
    char procfile[1024];
    char linkfile[1024];
    int linksize;
#endif

    /* First allow environment variable override */
    spot = SDL_getenv("SDL_VIDEO_X11_WMCLASS");
    if (spot) {
        return SDL_strdup(spot);
    }

    /* Next look at the application's executable name */
#if defined(__LINUX__) || defined(__FREEBSD__)
#if defined(__LINUX__)
    SDL_snprintf(procfile, SDL_arraysize(procfile), "/proc/%d/exe", getpid());
#elif defined(__FREEBSD__)
    SDL_snprintf(procfile, SDL_arraysize(procfile), "/proc/%d/file",
                 getpid());
#else
#error Where can we find the executable name?
#endif
    linksize = readlink(procfile, linkfile, sizeof(linkfile) - 1);
    if (linksize > 0) {
        linkfile[linksize] = '\0';
        spot = SDL_strrchr(linkfile, '/');
        if (spot) {
            return SDL_strdup(spot + 1);
        } else {
            return SDL_strdup(linkfile);
        }
    }
#endif /* __LINUX__ || __FREEBSD__ */

    /* Finally use the default we've used forever */
    return SDL_strdup("SDL_App");
}

/* X11 driver bootstrap functions */

static int
X11_Available(void)
{
    Display *display = NULL;
    if (SDL_X11_LoadSymbols()) {
        display = XOpenDisplay(NULL);
        if (display != NULL) {
            XCloseDisplay(display);
        }
        SDL_X11_UnloadSymbols();
    }
    return (display != NULL);
}

static void
X11_DeleteDevice(SDL_VideoDevice * device)
{
    SDL_VideoData *data = (SDL_VideoData *) device->driverdata;
    if (data->display) {
        XCloseDisplay(data->display);
    }
    SDL_free(data->windowlist);
    SDL_free(device->driverdata);
#if SDL_VIDEO_DRIVER_PANDORA
    SDL_free(device->gles_data);
#endif
    SDL_free(device);

    SDL_X11_UnloadSymbols();
}

static SDL_VideoDevice *
X11_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;
    SDL_VideoData *data;
    const char *display = NULL; /* Use the DISPLAY environment variable */

    if (!SDL_X11_LoadSymbols()) {
        return NULL;
    }

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return NULL;
    }
    data = (struct SDL_VideoData *) SDL_calloc(1, sizeof(SDL_VideoData));
    if (!data) {
        SDL_OutOfMemory();
        SDL_free(device);
        return NULL;
    }
    device->driverdata = data;

#if SDL_VIDEO_DRIVER_PANDORA
    device->gles_data = (struct SDL_PrivateGLESData *) SDL_calloc(1, sizeof(SDL_PrivateGLESData));
    if (!device->gles_data) {
        SDL_OutOfMemory();
        return NULL;
    }
#endif

    /* FIXME: Do we need this?
       if ( (SDL_strncmp(XDisplayName(display), ":", 1) == 0) ||
       (SDL_strncmp(XDisplayName(display), "unix:", 5) == 0) ) {
       local_X11 = 1;
       } else {
       local_X11 = 0;
       }
     */
    data->display = XOpenDisplay(display);
#if defined(__osf__) && defined(SDL_VIDEO_DRIVER_X11_DYNAMIC)
    /* On Tru64 if linking without -lX11, it fails and you get following message.
     * Xlib: connection to ":0.0" refused by server
     * Xlib: XDM authorization key matches an existing client!
     *
     * It succeeds if retrying 1 second later
     * or if running xhost +localhost on shell.
     */
    if (data->display == NULL) {
        SDL_Delay(1000);
        data->display = XOpenDisplay(display);
    }
#endif
    if (data->display == NULL) {
        SDL_free(device);
        SDL_SetError("Couldn't open X11 display");
        return NULL;
    }
#ifdef X11_DEBUG
    XSynchronize(data->display, True);
#endif

    /* Set the function pointers */
    device->VideoInit = X11_VideoInit;
    device->VideoQuit = X11_VideoQuit;
    device->GetDisplayModes = X11_GetDisplayModes;
    device->SetDisplayMode = X11_SetDisplayMode;
    device->SetDisplayGammaRamp = X11_SetDisplayGammaRamp;
    device->GetDisplayGammaRamp = X11_GetDisplayGammaRamp;
    device->SuspendScreenSaver = X11_SuspendScreenSaver;
    device->PumpEvents = X11_PumpEvents;

    device->CreateWindow = X11_CreateWindow;
    device->CreateWindowFrom = X11_CreateWindowFrom;
    device->SetWindowTitle = X11_SetWindowTitle;
    device->SetWindowIcon = X11_SetWindowIcon;
    device->SetWindowPosition = X11_SetWindowPosition;
    device->SetWindowSize = X11_SetWindowSize;
    device->ShowWindow = X11_ShowWindow;
    device->HideWindow = X11_HideWindow;
    device->RaiseWindow = X11_RaiseWindow;
    device->MaximizeWindow = X11_MaximizeWindow;
    device->MinimizeWindow = X11_MinimizeWindow;
    device->RestoreWindow = X11_RestoreWindow;
    device->SetWindowGrab = X11_SetWindowGrab;
    device->DestroyWindow = X11_DestroyWindow;
    device->GetWindowWMInfo = X11_GetWindowWMInfo;
#ifdef SDL_VIDEO_OPENGL_GLX
    device->GL_LoadLibrary = X11_GL_LoadLibrary;
    device->GL_GetProcAddress = X11_GL_GetProcAddress;
    device->GL_UnloadLibrary = X11_GL_UnloadLibrary;
    device->GL_CreateContext = X11_GL_CreateContext;
    device->GL_MakeCurrent = X11_GL_MakeCurrent;
    device->GL_SetSwapInterval = X11_GL_SetSwapInterval;
    device->GL_GetSwapInterval = X11_GL_GetSwapInterval;
    device->GL_SwapWindow = X11_GL_SwapWindow;
    device->GL_DeleteContext = X11_GL_DeleteContext;
#endif
#if SDL_VIDEO_DRIVER_PANDORA
    device->GL_LoadLibrary = X11_GLES_LoadLibrary;
    device->GL_GetProcAddress = X11_GLES_GetProcAddress;
    device->GL_UnloadLibrary = X11_GLES_UnloadLibrary;
    device->GL_CreateContext = X11_GLES_CreateContext;
    device->GL_MakeCurrent = X11_GLES_MakeCurrent;
    device->GL_SetSwapInterval = X11_GLES_SetSwapInterval;
    device->GL_GetSwapInterval = X11_GLES_GetSwapInterval;
    device->GL_SwapWindow = X11_GLES_SwapWindow;
    device->GL_DeleteContext = X11_GLES_DeleteContext;
#endif

    device->free = X11_DeleteDevice;

    return device;
}

VideoBootStrap X11_bootstrap = {
    "x11", "SDL X11 video driver",
    X11_Available, X11_CreateDevice
};


int
X11_VideoInit(_THIS)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;

    /* Get the window class name, usually the name of the application */
    data->classname = get_classname();

    /* Open a connection to the X input manager */
#ifdef X_HAVE_UTF8_STRING
    if (SDL_X11_HAVE_UTF8) {
        data->im =
            XOpenIM(data->display, NULL, data->classname, data->classname);
    }
#endif

    /* Look up some useful Atoms */
    data->WM_DELETE_WINDOW =
        XInternAtom(data->display, "WM_DELETE_WINDOW", False);

    X11_InitModes(_this);

#if SDL_VIDEO_RENDER_X11
    X11_AddRenderDriver(_this);
#endif

    if (X11_InitKeyboard(_this) != 0) {
        return -1;
    }
    X11_InitMouse(_this);

    return 0;
}

void
X11_VideoQuit(_THIS)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;

    if (data->classname) {
        SDL_free(data->classname);
    }
#ifdef X_HAVE_UTF8_STRING
    if (data->im) {
        XCloseIM(data->im);
    }
#endif

    X11_QuitModes(_this);
    X11_QuitKeyboard(_this);
    X11_QuitMouse(_this);
}

SDL_bool
X11_UseDirectColorVisuals()
{
    /* Once we implement DirectColor colormaps and gamma ramp support...
       return SDL_getenv("SDL_VIDEO_X11_NODIRECTCOLOR") ? SDL_FALSE : SDL_TRUE;
     */
    return SDL_FALSE;
}

/* vim: set ts=4 sw=4 expandtab: */
