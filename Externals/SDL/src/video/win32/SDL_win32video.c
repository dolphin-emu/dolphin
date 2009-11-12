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

#include "SDL_main.h"
#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"

#include "SDL_win32video.h"
#include "SDL_d3drender.h"
#include "SDL_gdirender.h"

/* Initialization/Query functions */
static int WIN_VideoInit(_THIS);
static void WIN_VideoQuit(_THIS);

int total_mice = 0;             /* total mouse count */
HANDLE *mice = NULL;            /* the handles to the detected mice */
HCTX *g_hCtx = NULL;            /* handles to tablet contexts */
int tablet = -1;                /* we're assuming that there is no tablet */

/* WIN32 driver bootstrap functions */

static int
WIN_Available(void)
{
    return (1);
}

static void
WIN_DeleteDevice(SDL_VideoDevice * device)
{
    SDL_VideoData *data = (SDL_VideoData *) device->driverdata;

    SDL_UnregisterApp();
#if SDL_VIDEO_RENDER_D3D
    if (data->d3d) {
        IDirect3D9_Release(data->d3d);
        FreeLibrary(data->d3dDLL);
    }
#endif
#if SDL_VIDEO_RENDER_DDRAW
    if (data->ddraw) {
        data->ddraw->lpVtbl->Release(data->ddraw);
        FreeLibrary(data->ddrawDLL);
    }
#endif
    if (data->wintabDLL) {
        FreeLibrary(data->wintabDLL);
    }
    SDL_free(device->driverdata);
    SDL_free(device);
}

static SDL_VideoDevice *
WIN_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;
    SDL_VideoData *data;

    SDL_RegisterApp(NULL, 0, NULL);

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

#if SDL_VIDEO_RENDER_D3D
    data->d3dDLL = LoadLibrary(TEXT("D3D9.DLL"));
    if (data->d3dDLL) {
        IDirect3D9 *(WINAPI * D3DCreate) (UINT SDKVersion);

        D3DCreate =
            (IDirect3D9 * (WINAPI *) (UINT)) GetProcAddress(data->d3dDLL,
                                                            "Direct3DCreate9");
        if (D3DCreate) {
            data->d3d = D3DCreate(D3D_SDK_VERSION);
        }
        if (!data->d3d) {
            FreeLibrary(data->d3dDLL);
            data->d3dDLL = NULL;
        }
    }
#endif /* SDL_VIDEO_RENDER_D3D */
#if SDL_VIDEO_RENDER_DDRAW
    data->ddrawDLL = LoadLibrary(TEXT("ddraw.dll"));
    if (data->ddrawDLL) {
        IDirectDraw *(WINAPI * DDCreate) (GUID FAR * lpGUID,
                                          LPDIRECTDRAW FAR * lplpDD,
                                          IUnknown FAR * pUnkOuter);

        DDCreate =
            (IDirectDraw *
             (WINAPI *) (GUID FAR *, LPDIRECTDRAW FAR *, IUnknown FAR *))
            GetProcAddress(data->ddrawDLL, TEXT("DirectDrawCreate"));
        if (!DDCreate || DDCreate(NULL, &data->ddraw, NULL) != DD_OK) {
            FreeLibrary(data->ddrawDLL);
            data->ddrawDLL = NULL;
            data->ddraw = NULL;
        }
    }
#endif /* SDL_VIDEO_RENDER_DDRAW */

    data->wintabDLL = LoadLibrary(TEXT("WINTAB32.DLL"));
    if (data->wintabDLL) {
#define PROCNAME(X) #X
        data->WTInfoA =
            (UINT(*)(UINT, UINT, LPVOID)) GetProcAddress(data->wintabDLL,
                                                         PROCNAME(WTInfoA));
        data->WTOpenA =
            (HCTX(*)(HWND, LPLOGCONTEXTA, BOOL)) GetProcAddress(data->
                                                                wintabDLL,
                                                                PROCNAME
                                                                (WTOpenA));
        data->WTPacket =
            (int (*)(HCTX, UINT, LPVOID)) GetProcAddress(data->wintabDLL,
                                                         PROCNAME(WTPacket));
        data->WTClose =
            (BOOL(*)(HCTX)) GetProcAddress(data->wintabDLL,
                                           PROCNAME(WTClose));
#undef PROCNAME

        if (!data->WTInfoA || !data->WTOpenA || !data->WTPacket
            || !data->WTClose) {
            FreeLibrary(data->wintabDLL);
            data->wintabDLL = NULL;
        }
    }

    /* Set the function pointers */
    device->VideoInit = WIN_VideoInit;
    device->VideoQuit = WIN_VideoQuit;
    device->GetDisplayModes = WIN_GetDisplayModes;
    device->SetDisplayMode = WIN_SetDisplayMode;
    device->SetDisplayGammaRamp = WIN_SetDisplayGammaRamp;
    device->GetDisplayGammaRamp = WIN_GetDisplayGammaRamp;
    device->PumpEvents = WIN_PumpEvents;

#undef CreateWindow
    device->CreateWindow = WIN_CreateWindow;
    device->CreateWindowFrom = WIN_CreateWindowFrom;
    device->SetWindowTitle = WIN_SetWindowTitle;
    device->SetWindowIcon = WIN_SetWindowIcon;
    device->SetWindowPosition = WIN_SetWindowPosition;
    device->SetWindowSize = WIN_SetWindowSize;
    device->ShowWindow = WIN_ShowWindow;
    device->HideWindow = WIN_HideWindow;
    device->RaiseWindow = WIN_RaiseWindow;
    device->MaximizeWindow = WIN_MaximizeWindow;
    device->MinimizeWindow = WIN_MinimizeWindow;
    device->RestoreWindow = WIN_RestoreWindow;
    device->SetWindowGrab = WIN_SetWindowGrab;
    device->DestroyWindow = WIN_DestroyWindow;
    device->GetWindowWMInfo = WIN_GetWindowWMInfo;
#ifdef SDL_VIDEO_OPENGL_WGL
    device->GL_LoadLibrary = WIN_GL_LoadLibrary;
    device->GL_GetProcAddress = WIN_GL_GetProcAddress;
    device->GL_UnloadLibrary = WIN_GL_UnloadLibrary;
    device->GL_CreateContext = WIN_GL_CreateContext;
    device->GL_MakeCurrent = WIN_GL_MakeCurrent;
    device->GL_SetSwapInterval = WIN_GL_SetSwapInterval;
    device->GL_GetSwapInterval = WIN_GL_GetSwapInterval;
    device->GL_SwapWindow = WIN_GL_SwapWindow;
    device->GL_DeleteContext = WIN_GL_DeleteContext;
#endif

    device->free = WIN_DeleteDevice;

    return device;
}

VideoBootStrap WIN32_bootstrap = {
    "win32", "SDL Win32/64 video driver", WIN_Available, WIN_CreateDevice
};


int
WIN_VideoInit(_THIS)
{
    WIN_InitModes(_this);

#if SDL_VIDEO_RENDER_D3D
    D3D_AddRenderDriver(_this);
#endif
#if SDL_VIDEO_RENDER_DDRAW
    DDRAW_AddRenderDriver(_this);
#endif
#if SDL_VIDEO_RENDER_GDI
    GDI_AddRenderDriver(_this);
#endif
#if SDL_VIDEO_RENDER_GAPI
    GAPI_AddRenderDriver(_this);
#endif

    g_hCtx = SDL_malloc(sizeof(HCTX));
    WIN_InitKeyboard(_this);
    WIN_InitMouse(_this);

    return 0;
}

void
WIN_VideoQuit(_THIS)
{
    WIN_QuitModes(_this);
    WIN_QuitKeyboard(_this);
    WIN_QuitMouse(_this);
    SDL_free(g_hCtx);
}

/* vim: set ts=4 sw=4 expandtab: */
