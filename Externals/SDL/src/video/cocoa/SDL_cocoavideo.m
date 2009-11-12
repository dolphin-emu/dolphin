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

/* Initialization/Query functions */
static int Cocoa_VideoInit(_THIS);
static void Cocoa_VideoQuit(_THIS);

/* Cocoa driver bootstrap functions */

static int
Cocoa_Available(void)
{
    return (1);
}

static void
Cocoa_DeleteDevice(SDL_VideoDevice * device)
{
    SDL_VideoData *data = (SDL_VideoData *) device->driverdata;

    SDL_free(device->driverdata);
    SDL_free(device);
}

static SDL_VideoDevice *
Cocoa_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;
    SDL_VideoData *data;

    Cocoa_RegisterApp();

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (device) {
        data = (struct SDL_VideoData *) SDL_calloc(1, sizeof(SDL_VideoData));
    }
    if (!device || !data) {
        SDL_OutOfMemory();
        if (device) {
            SDL_free(device);
        }
        return NULL;
    }
    device->driverdata = data;

    /* Find out what version of Mac OS X we're running */
    Gestalt(gestaltSystemVersion, &data->osversion);

    /* Set the function pointers */
    device->VideoInit = Cocoa_VideoInit;
    device->VideoQuit = Cocoa_VideoQuit;
    device->GetDisplayModes = Cocoa_GetDisplayModes;
    device->SetDisplayMode = Cocoa_SetDisplayMode;
    device->PumpEvents = Cocoa_PumpEvents;

    device->CreateWindow = Cocoa_CreateWindow;
    device->CreateWindowFrom = Cocoa_CreateWindowFrom;
    device->SetWindowTitle = Cocoa_SetWindowTitle;
    device->SetWindowPosition = Cocoa_SetWindowPosition;
    device->SetWindowSize = Cocoa_SetWindowSize;
    device->ShowWindow = Cocoa_ShowWindow;
    device->HideWindow = Cocoa_HideWindow;
    device->RaiseWindow = Cocoa_RaiseWindow;
    device->MaximizeWindow = Cocoa_MaximizeWindow;
    device->MinimizeWindow = Cocoa_MinimizeWindow;
    device->RestoreWindow = Cocoa_RestoreWindow;
    device->SetWindowGrab = Cocoa_SetWindowGrab;
    device->DestroyWindow = Cocoa_DestroyWindow;
    device->GetWindowWMInfo = Cocoa_GetWindowWMInfo;
#ifdef SDL_VIDEO_OPENGL_CGL
    device->GL_LoadLibrary = Cocoa_GL_LoadLibrary;
    device->GL_GetProcAddress = Cocoa_GL_GetProcAddress;
    device->GL_UnloadLibrary = Cocoa_GL_UnloadLibrary;
    device->GL_CreateContext = Cocoa_GL_CreateContext;
    device->GL_MakeCurrent = Cocoa_GL_MakeCurrent;
    device->GL_SetSwapInterval = Cocoa_GL_SetSwapInterval;
    device->GL_GetSwapInterval = Cocoa_GL_GetSwapInterval;
    device->GL_SwapWindow = Cocoa_GL_SwapWindow;
    device->GL_DeleteContext = Cocoa_GL_DeleteContext;
#endif

    device->StartTextInput = Cocoa_StartTextInput;
    device->StopTextInput = Cocoa_StopTextInput;
    device->SetTextInputRect = Cocoa_SetTextInputRect;

    device->free = Cocoa_DeleteDevice;

    return device;
}

VideoBootStrap COCOA_bootstrap = {
    "cocoa", "SDL Cocoa video driver",
    Cocoa_Available, Cocoa_CreateDevice
};


int
Cocoa_VideoInit(_THIS)
{
    Cocoa_InitModes(_this);
    Cocoa_InitKeyboard(_this);
    Cocoa_InitMouse(_this);
    return 0;
}

void
Cocoa_VideoQuit(_THIS)
{
    Cocoa_QuitModes(_this);
    Cocoa_QuitKeyboard(_this);
    Cocoa_QuitMouse(_this);
}

/* vim: set ts=4 sw=4 expandtab: */
