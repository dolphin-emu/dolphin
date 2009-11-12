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

#include "SDL_syswm.h"
#include "../SDL_sysvideo.h"
#include "../../events/SDL_keyboard_c.h"

#include "SDL_DirectFB_video.h"


int
DirectFB_CreateWindow(_THIS, SDL_Window * window)
{
    SDL_DFB_DEVICEDATA(_this);
    SDL_DFB_DISPLAYDATA(_this, window);
    DFB_WindowData *windata = NULL;
    DFBWindowOptions wopts;
    DFBWindowDescription desc;
    IDirectFBFont *font;
    int ret, x, y;

    SDL_DFB_CALLOC(window->driverdata, 1, sizeof(DFB_WindowData));
    windata = (DFB_WindowData *) window->driverdata;

    windata->is_managed = devdata->has_own_wm;

    SDL_DFB_CHECKERR(devdata->dfb->SetCooperativeLevel(devdata->dfb,
                                                       DFSCL_NORMAL));
    SDL_DFB_CHECKERR(dispdata->layer->SetCooperativeLevel(dispdata->layer,
                                                          DLSCL_ADMINISTRATIVE));

    /* Fill the window description. */
    if (window->x == SDL_WINDOWPOS_CENTERED) {
        x = (dispdata->cw - window->w) / 2;
    } else if (window->x == SDL_WINDOWPOS_UNDEFINED) {
        x = 0;
    } else {
        x = window->x;
    }
    if (window->y == SDL_WINDOWPOS_CENTERED) {
        y = (dispdata->ch - window->h) / 2;
    } else if (window->y == SDL_WINDOWPOS_UNDEFINED) {
        y = 0;
    } else {
        y = window->y;
    }
    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        x = 0;
        y = 0;
    }

    DirectFB_WM_AdjustWindowLayout(window);

    /* Create Window */
    desc.flags =
        DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_PIXELFORMAT | DWDESC_POSX
        | DWDESC_POSY | DWDESC_SURFACE_CAPS;
    desc.posx = x;
    desc.posy = y;
    desc.width = windata->size.w;
    desc.height = windata->size.h;
    desc.pixelformat = dispdata->pixelformat;
    desc.surface_caps = DSCAPS_PREMULTIPLIED;

    /* Create the window. */
    SDL_DFB_CHECKERR(dispdata->layer->CreateWindow(dispdata->layer, &desc,
                                                   &windata->window));

    /* Set Options */
    windata->window->GetOptions(windata->window, &wopts);

    if (window->flags & SDL_WINDOW_RESIZABLE)
        wopts |= DWOP_SCALE;
    else
        wopts |= DWOP_KEEP_SIZE;

    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        wopts |= DWOP_KEEP_POSITION | DWOP_KEEP_STACKING | DWOP_KEEP_SIZE;
        windata->window->SetStackingClass(windata->window, DWSC_UPPER);
    }
    windata->window->SetOptions(windata->window, wopts);

    /* See what we got */
    SDL_DFB_CHECKERR(DirectFB_WM_GetClientSize
                     (_this, window, &window->w, &window->h));

    /* Get the window's surface. */
    SDL_DFB_CHECKERR(windata->window->GetSurface(windata->window,
                                                 &windata->window_surface));
    /* And get a subsurface for rendering */
    SDL_DFB_CHECKERR(windata->window_surface->
                     GetSubSurface(windata->window_surface, &windata->client,
                                   &windata->surface));

    windata->window->SetOpacity(windata->window, 0xFF);

    /* Create Eventbuffer */
    SDL_DFB_CHECKERR(windata->window->CreateEventBuffer(windata->window,
                                                        &windata->
                                                        eventbuffer));
    SDL_DFB_CHECKERR(windata->window->
                     EnableEvents(windata->window, DWET_ALL));

    /* Create a font */
    /* FIXME: once during Video_Init */
    if (windata->is_managed) {
        DFBFontDescription fdesc;

        fdesc.flags = DFDESC_HEIGHT;
        fdesc.height = windata->theme.font_size;
        font = NULL;
        SDL_DFB_CHECK(devdata->
                      dfb->CreateFont(devdata->dfb, windata->theme.font,
                                      &fdesc, &font));
        windata->window_surface->SetFont(windata->window_surface, font);
        SDL_DFB_RELEASE(font);
    }

    /* Make it the top most window. */
    windata->window->RaiseToTop(windata->window);

    /* remember parent */
    windata->sdl_id = window->id;

    /* Add to list ... */

    windata->next = devdata->firstwin;
    windata->opacity = 0xFF;
    devdata->firstwin = windata;

    /* Draw Frame */
    DirectFB_WM_RedrawLayout(window);

    return 0;
  error:
    SDL_DFB_RELEASE(windata->window);
    SDL_DFB_RELEASE(windata->surface);
    return -1;
}

int
DirectFB_CreateWindowFrom(_THIS, SDL_Window * window, const void *data)
{
    SDL_Unsupported();
    return -1;
}

void
DirectFB_SetWindowTitle(_THIS, SDL_Window * window)
{
    SDL_DFB_WINDOWDATA(window);

    if (windata->is_managed) {
        windata->wm_needs_redraw = 1;
    } else
        SDL_Unsupported();
}

void
DirectFB_SetWindowIcon(_THIS, SDL_Window * window, SDL_Surface * icon)
{
    SDL_DFB_DEVICEDATA(_this);
    SDL_DFB_WINDOWDATA(window);
    SDL_Surface *surface = NULL;
    DFBResult ret;

    if (icon) {
        SDL_PixelFormat format;
        DFBSurfaceDescription dsc;
        Uint32 *dest;
        Uint32 *p;
        int pitch, i;

        /* Convert the icon to ARGB for modern window managers */
        SDL_InitFormat(&format, 32, 0x00FF0000, 0x0000FF00, 0x000000FF,
                       0xFF000000);
        surface = SDL_ConvertSurface(icon, &format, 0);
        if (!surface) {
            return;
        }
        dsc.flags =
            DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT | DSDESC_CAPS;
        dsc.caps = DSCAPS_VIDEOONLY;
        dsc.width = surface->w;
        dsc.height = surface->h;
        dsc.pixelformat = DSPF_ARGB;

        SDL_DFB_CHECKERR(devdata->dfb->CreateSurface(devdata->dfb, &dsc,
                                                     &windata->icon));

        SDL_DFB_CHECKERR(windata->icon->Lock(windata->icon, DSLF_WRITE,
                                             (void *) &dest, &pitch));

        p = surface->pixels;
        for (i = 0; i < surface->h; i++)
            memcpy((char *) dest + i * pitch,
                   (char *) p + i * surface->pitch, 4 * surface->w);

        windata->icon->Unlock(windata->icon);
        SDL_FreeSurface(surface);
    } else {
        SDL_DFB_RELEASE(windata->icon);
    }
    return;
  error:
    if (surface)
        SDL_FreeSurface(surface);
    SDL_DFB_RELEASE(windata->icon);
    return;
}

void
DirectFB_SetWindowPosition(_THIS, SDL_Window * window)
{
    SDL_DFB_WINDOWDATA(window);
    int x, y;

    if (window->y == SDL_WINDOWPOS_UNDEFINED)
        y = 0;
    else
        y = window->y;

    if (window->x == SDL_WINDOWPOS_UNDEFINED)
        x = 0;
    else
        x = window->x;

    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        x = 0;
        y = 0;
    }
    DirectFB_WM_AdjustWindowLayout(window);
    windata->window->MoveTo(windata->window, x, y);
}

void
DirectFB_SetWindowSize(_THIS, SDL_Window * window)
{
    SDL_DFB_DEVICEDATA(_this);
    SDL_DFB_WINDOWDATA(window);
    int ret;

    if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
        int cw;
        int ch;

        /* Make sure all events are disabled for this operation ! */
        SDL_DFB_CHECKERR(windata->window->DisableEvents(windata->window,
                                                        DWET_ALL));

        SDL_DFB_CHECKERR(DirectFB_WM_GetClientSize(_this, window, &cw, &ch));

        if (cw != window->w || ch != window->h) {

            DirectFB_WM_AdjustWindowLayout(window);
            SDL_DFB_CHECKERR(windata->window->Resize(windata->window,
                                                     windata->size.w,
                                                     windata->size.h));
        }

        SDL_DFB_CHECKERR(windata->window->EnableEvents(windata->window,
                                                       DWET_ALL));

        SDL_DFB_CHECKERR(DirectFB_WM_GetClientSize
                         (_this, window, &window->w, &window->h));

        SDL_OnWindowResized(window);
    }
    return;
  error:
    windata->window->EnableEvents(windata->window, DWET_ALL);
    return;
}

void
DirectFB_ShowWindow(_THIS, SDL_Window * window)
{
    SDL_DFB_WINDOWDATA(window);

    windata->window->SetOpacity(windata->window, windata->opacity);

}

void
DirectFB_HideWindow(_THIS, SDL_Window * window)
{
    SDL_DFB_WINDOWDATA(window);

    windata->window->GetOpacity(windata->window, &windata->opacity);
    windata->window->SetOpacity(windata->window, 0);
}

void
DirectFB_RaiseWindow(_THIS, SDL_Window * window)
{
    SDL_DFB_WINDOWDATA(window);

    windata->window->RaiseToTop(windata->window);
    windata->window->RequestFocus(windata->window);
}

void
DirectFB_MaximizeWindow(_THIS, SDL_Window * window)
{
    SDL_DFB_WINDOWDATA(window);

    if (windata->is_managed) {
        DirectFB_WM_MaximizeWindow(_this, window);
    } else
        SDL_Unsupported();
}

void
DirectFB_MinimizeWindow(_THIS, SDL_Window * window)
{
    /* FIXME: Size to 32x32 ? */

    SDL_Unsupported();
}

void
DirectFB_RestoreWindow(_THIS, SDL_Window * window)
{
    SDL_DFB_WINDOWDATA(window);

    if (windata->is_managed) {
        DirectFB_WM_RestoreWindow(_this, window);
    } else
        SDL_Unsupported();
}

void
DirectFB_SetWindowGrab(_THIS, SDL_Window * window)
{
    SDL_DFB_WINDOWDATA(window);

    if ((window->flags & SDL_WINDOW_INPUT_GRABBED)) {
        windata->window->GrabPointer(windata->window);
        windata->window->GrabKeyboard(windata->window);
    } else {
        windata->window->UngrabPointer(windata->window);
        windata->window->UngrabKeyboard(windata->window);
    }
}

void
DirectFB_DestroyWindow(_THIS, SDL_Window * window)
{
    SDL_DFB_DEVICEDATA(_this);
    SDL_DFB_WINDOWDATA(window);
    DFB_WindowData *p;

    SDL_DFB_DEBUG("Trace\n");

    /* Some cleanups */
    windata->window->UngrabPointer(windata->window);
    windata->window->UngrabKeyboard(windata->window);

    windata->window_surface->SetFont(windata->window_surface, NULL);
    SDL_DFB_RELEASE(windata->icon);
    SDL_DFB_RELEASE(windata->eventbuffer);
    SDL_DFB_RELEASE(windata->surface);
    SDL_DFB_RELEASE(windata->window_surface);

    SDL_DFB_RELEASE(windata->window);

    /* Remove from list ... */

    p = devdata->firstwin;
    while (p && p->next != windata)
        p = p->next;
    if (p)
        p->next = windata->next;
    else
        devdata->firstwin = windata->next;
    SDL_free(windata);
    return;
}

SDL_bool
DirectFB_GetWindowWMInfo(_THIS, SDL_Window * window,
                         struct SDL_SysWMinfo * info)
{
    SDL_Unsupported();
    return SDL_FALSE;
}

void
DirectFB_AdjustWindowSurface(SDL_Window * window)
{
    SDL_DFB_WINDOWDATA(window);
    int adjust = windata->wm_needs_redraw;
    int cw, ch;
    int ret;

    DirectFB_WM_AdjustWindowLayout(window);

    SDL_DFB_CHECKERR(windata->
                     window_surface->GetSize(windata->window_surface, &cw,
                                             &ch));
    if (cw != windata->size.w || ch != windata->size.h) {
        adjust = 1;
    }

    if (adjust) {
#if DFB_VERSION_ATLEAST(1,2,1)
        SDL_DFB_CHECKERR(windata->window->ResizeSurface(windata->window,
                                                        windata->size.w,
                                                        windata->size.h));
        SDL_DFB_CHECKERR(windata->surface->MakeSubSurface(windata->surface,
                                                          windata->
                                                          window_surface,
                                                          &windata->client));
#else
        DFBWindowOptions opts;

        SDL_DFB_CHECKERR(windata->window->GetOptions(windata->window, &opts));
        /* recreate subsurface */
        SDL_DFB_RELEASE(windata->surface);

        if (opts & DWOP_SCALE)
            SDL_DFB_CHECKERR(windata->window->ResizeSurface(windata->window,
                                                            windata->size.w,
                                                            windata->size.h));
        SDL_DFB_CHECKERR(windata->window_surface->
                         GetSubSurface(windata->window_surface,
                                       &windata->client, &windata->surface));
#endif
        DirectFB_WM_RedrawLayout(window);
    }
  error:
    return;
}
