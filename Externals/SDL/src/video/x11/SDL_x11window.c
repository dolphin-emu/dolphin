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

#include "../SDL_sysvideo.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"

#include "SDL_x11video.h"
#include "SDL_x11mouse.h"
#include "SDL_x11gamma.h"
#include "../Xext/extensions/StdCmap.h"

#ifdef SDL_VIDEO_DRIVER_PANDORA
#include "SDL_x11opengles.h"
#endif

#include "SDL_syswm.h"

#define _NET_WM_STATE_REMOVE    0l
#define _NET_WM_STATE_ADD       1l
#define _NET_WM_STATE_TOGGLE    2l

static void
X11_GetDisplaySize(_THIS, SDL_Window * window, int *w, int *h)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
    SDL_DisplayData *displaydata =
        (SDL_DisplayData *) SDL_GetDisplayFromWindow(window)->driverdata;
    XWindowAttributes attr;

    XGetWindowAttributes(data->display, RootWindow(data->display,
                                                   displaydata->screen),
                         &attr);
    if (w) {
        *w = attr.width;
    }
    if (h) {
        *h = attr.height;
    }
}

static int
SetupWindowData(_THIS, SDL_Window * window, Window w, BOOL created)
{
    SDL_VideoData *videodata = (SDL_VideoData *) _this->driverdata;
    SDL_WindowData *data;
    int numwindows = videodata->numwindows;
    int windowlistlength = videodata->windowlistlength;
    SDL_WindowData **windowlist = videodata->windowlist;
    int index;

    /* Allocate the window data */
    data = (SDL_WindowData *) SDL_calloc(1, sizeof(*data));
    if (!data) {
        SDL_OutOfMemory();
        return -1;
    }
    data->windowID = window->id;
    data->window = w;
#ifdef X_HAVE_UTF8_STRING
    if (SDL_X11_HAVE_UTF8) {
        data->ic =
            pXCreateIC(videodata->im, XNClientWindow, w, XNFocusWindow, w,
                       XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                       XNResourceName, videodata->classname, XNResourceClass,
                       videodata->classname, NULL);
    }
#endif
    data->created = created;
    data->videodata = videodata;

    /* Associate the data with the window */

    if (numwindows < windowlistlength) {
        windowlist[numwindows] = data;
        videodata->numwindows++;
    } else {
        windowlist =
            (SDL_WindowData **) SDL_realloc(windowlist,
                                            (numwindows +
                                             1) * sizeof(*windowlist));
        if (!windowlist) {
            SDL_OutOfMemory();
            SDL_free(data);
            return -1;
        }
        windowlist[numwindows] = data;
        videodata->numwindows++;
        videodata->windowlistlength++;
        videodata->windowlist = windowlist;
    }

    /* Fill in the SDL window with the window data */
    {
        XWindowAttributes attrib;

        XGetWindowAttributes(data->videodata->display, w, &attrib);
        window->x = attrib.x;
        window->y = attrib.y;
        window->w = attrib.width;
        window->h = attrib.height;
        if (attrib.map_state != IsUnmapped) {
            window->flags |= SDL_WINDOW_SHOWN;
        } else {
            window->flags &= ~SDL_WINDOW_SHOWN;
        }
    }

    {
        Atom _NET_WM_STATE =
            XInternAtom(data->videodata->display, "_NET_WM_STATE", False);
        Atom _NET_WM_STATE_MAXIMIZED_VERT =
            XInternAtom(data->videodata->display,
                        "_NET_WM_STATE_MAXIMIZED_VERT", False);
        Atom _NET_WM_STATE_MAXIMIZED_HORZ =
            XInternAtom(data->videodata->display,
                        "_NET_WM_STATE_MAXIMIZED_HORZ", False);
        Atom actualType;
        int actualFormat;
        unsigned long i, numItems, bytesAfter;
        unsigned char *propertyValue = NULL;
        long maxLength = 1024;

        if (XGetWindowProperty(data->videodata->display, w, _NET_WM_STATE,
                               0l, maxLength, False, XA_ATOM, &actualType,
                               &actualFormat, &numItems, &bytesAfter,
                               &propertyValue) == Success) {
            Atom *atoms = (Atom *) propertyValue;
            int maximized = 0;

            for (i = 0; i < numItems; ++i) {
                if (atoms[i] == _NET_WM_STATE_MAXIMIZED_VERT) {
                    maximized |= 1;
                } else if (atoms[i] == _NET_WM_STATE_MAXIMIZED_HORZ) {
                    maximized |= 2;
                }
                /* Might also want to check the following properties:
                   _NET_WM_STATE_ABOVE, _NET_WM_STATE_FULLSCREEN
                 */
            }
            if (maximized == 3) {
                window->flags |= SDL_WINDOW_MAXIMIZED;
            }
            XFree(propertyValue);
        }
    }

    /* FIXME: How can I tell?
       {
       DWORD style = GetWindowLong(hwnd, GWL_STYLE);
       if (style & WS_VISIBLE) {
       if (style & (WS_BORDER | WS_THICKFRAME)) {
       window->flags &= ~SDL_WINDOW_BORDERLESS;
       } else {
       window->flags |= SDL_WINDOW_BORDERLESS;
       }
       if (style & WS_THICKFRAME) {
       window->flags |= SDL_WINDOW_RESIZABLE;
       } else {
       window->flags &= ~SDL_WINDOW_RESIZABLE;
       }
       if (style & WS_MAXIMIZE) {
       window->flags |= SDL_WINDOW_MAXIMIZED;
       } else {
       window->flags &= ~SDL_WINDOW_MAXIMIZED;
       }
       if (style & WS_MINIMIZE) {
       window->flags |= SDL_WINDOW_MINIMIZED;
       } else {
       window->flags &= ~SDL_WINDOW_MINIMIZED;
       }
       }
       if (GetFocus() == hwnd) {
       int index = data->videodata->keyboard;
       window->flags |= SDL_WINDOW_INPUT_FOCUS;
       SDL_SetKeyboardFocus(index, data->windowID);

       if (window->flags & SDL_WINDOW_INPUT_GRABBED) {
       RECT rect;
       GetClientRect(hwnd, &rect);
       ClientToScreen(hwnd, (LPPOINT) & rect);
       ClientToScreen(hwnd, (LPPOINT) & rect + 1);
       ClipCursor(&rect);
       }
       }
     */

    /* All done! */
    window->driverdata = data;
    return 0;
}

int
X11_CreateWindow(_THIS, SDL_Window * window)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
    SDL_DisplayData *displaydata =
        (SDL_DisplayData *) SDL_GetDisplayFromWindow(window)->driverdata;
    Visual *visual;
    int depth;
    XSetWindowAttributes xattr;
    int x, y;
    Window w;
    XSizeHints *sizehints;
    XWMHints *wmhints;
    XClassHint *classhints;

#if SDL_VIDEO_DRIVER_X11_XINERAMA
/* FIXME
    if ( use_xinerama ) {
        x = xinerama_info.x_org;
        y = xinerama_info.y_org;
    }
*/
#endif
#ifdef SDL_VIDEO_OPENGL_GLX
    if (window->flags & SDL_WINDOW_OPENGL) {
        XVisualInfo *vinfo;

        vinfo = X11_GL_GetVisual(_this, data->display, displaydata->screen);
        if (!vinfo) {
            return -1;
        }
        visual = vinfo->visual;
        depth = vinfo->depth;
        XFree(vinfo);
    } else
#endif
#ifdef SDL_VIDEO_DRIVER_PANDORA
    if (window->flags & SDL_WINDOW_OPENGL) {
        XVisualInfo *vinfo;

        vinfo = X11_GLES_GetVisual(_this, data->display, displaydata->screen);
        if (!vinfo) {
            return -1;
        }
        visual = vinfo->visual;
        depth = vinfo->depth;
        XFree(vinfo);
    } else
#endif
    {
        visual = displaydata->visual;
        depth = displaydata->depth;
    }

    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        xattr.override_redirect = True;
    } else {
        xattr.override_redirect = False;
    }
    xattr.background_pixel = 0;
    xattr.border_pixel = 0;

    if (visual->class == PseudoColor) {
        printf("asking for PseudoColor\n");

        Status status;
        XStandardColormap cmap;
        XColor *colorcells;
        Colormap colormap;
        Sint32 pix;
        Sint32 ncolors;
        Sint32 nbits;
        Sint32 rmax, gmax, bmax;
        Sint32 rwidth, gwidth, bwidth;
        Sint32 rmask, gmask, bmask;
        Sint32 rshift, gshift, bshift;
        Sint32 r, g, b;

        /* Is the colormap we need already registered in SDL? */
        if (colormap =
            X11_LookupColormap(data->display,
                               displaydata->screen, visual->visualid)) {
            xattr.colormap = colormap;
/*             printf("found existing colormap\n"); */
        } else {
            /* The colormap is not known to SDL so we will create it */
            colormap = XCreateColormap(data->display,
                                       RootWindow(data->display,
                                                  displaydata->screen),
                                       visual, AllocAll);
/*             printf("colormap = %x\n", colormap); */

            /* If we can't create a colormap, then we must die */
            if (!colormap) {
                SDL_SetError
                    ("Couldn't create window: Could not create writable colormap");
                return -1;
            }

            /* OK, we got a colormap, now fill it in as best as we can */

            colorcells = SDL_malloc(visual->map_entries * sizeof(XColor));
            if (NULL == colorcells) {
                SDL_SetError("out of memory in X11_CreateWindow");
                return -1;
            }

            ncolors = visual->map_entries;
            nbits = visual->bits_per_rgb;

/* 	    printf("ncolors = %d nbits = %d\n", ncolors, nbits); */

            /* what if ncolors != (1 << nbits)? That can happen on a
               true PseudoColor display.  I'm assuming that we will
               always have ncolors == (1 << nbits) */

            /* I'm making a lot of assumptions here. */

            /* Compute the width of each field. If there is one extra
               bit, give it to green. If there are two extra bits give
               them to red and greed.  We can get extra bits when the
               number of bits per pixel is not a multiple of 3. For
               example when we have 16 bits per pixel and need a 5/6/5
               layout for the RGB fields */

            rwidth = (nbits / 3) + (((nbits % 3) == 2) ? 1 : 0);
            gwidth = (nbits / 3) + (((nbits % 3) >= 1) ? 1 : 0);
            bwidth = (nbits / 3);

            rshift = gwidth + bwidth;
            gshift = bwidth;
            bshift = 0;

            rmax = 1 << rwidth;
            gmax = 1 << gwidth;
            bmax = 1 << bwidth;

            rmask = rmax - 1;
            gmask = gmax - 1;
            bmask = bmax - 1;

/*             printf("red   mask = %4x shift = %4d width = %d\n", rmask, rshift, rwidth); */
/*             printf("green mask = %4x shift = %4d width = %d\n", gmask, gshift, gwidth); */
/*             printf("blue  mask = %4x shift = %4d width = %d\n", bmask, bshift, bwidth); */

            /* build the color table pixel values */
            pix = 0;
            for (r = 0; r < rmax; r++) {
                for (g = 0; g < gmax; g++) {
                    for (b = 0; b < bmax; b++) {
                        colorcells[pix].pixel =
                            (r << rshift) | (g << gshift) | (b << bshift);
                        colorcells[pix].red = (0xffff * r) / rmask;
                        colorcells[pix].green = (0xffff * g) / gmask;
                        colorcells[pix].blue = (0xffff * b) / bmask;
/* 		  printf("%4x:%4x [%4x %4x %4x]\n",  */
/* 			 pix,  */
/* 			 colorcells[pix].pixel, */
/* 			 colorcells[pix].red, */
/* 			 colorcells[pix].green, */
/* 			 colorcells[pix].blue); */
                        pix++;
                    }
                }
            }

/*             status = */
/*                 XStoreColors(data->display, colormap, colorcells, ncolors); */

            xattr.colormap = colormap;
            X11_TrackColormap(data->display, displaydata->screen,
                              colormap, visual, NULL);

            SDL_free(colorcells);
        }
    } else if (visual->class == DirectColor) {
        Status status;
        XStandardColormap cmap;
        XColor *colorcells;
        Colormap colormap;
        int i;
        int ncolors;
        int rmax, gmax, bmax;
        int rmask, gmask, bmask;
        int rshift, gshift, bshift;

        /* Is the colormap we need already registered in SDL? */
        if (colormap =
            X11_LookupColormap(data->display,
                               displaydata->screen, visual->visualid)) {
            xattr.colormap = colormap;
/*             printf("found existing colormap\n"); */
        } else {
            /* The colormap is not known to SDL so we will create it */
            colormap = XCreateColormap(data->display,
                                       RootWindow(data->display,
                                                  displaydata->screen),
                                       visual, AllocAll);
/*             printf("colormap = %x\n", colormap); */

            /* If we can't create a colormap, then we must die */
            if (!colormap) {
                SDL_SetError
                    ("Couldn't create window: Could not create writable colormap");
                return -1;
            }

            /* OK, we got a colormap, now fill it in as best as we can */

            colorcells = SDL_malloc(visual->map_entries * sizeof(XColor));
            if (NULL == colorcells) {
                SDL_SetError("out of memory in X11_CreateWindow");
                return -1;
            }
            ncolors = visual->map_entries;
            rmax = 0xffff;
            gmax = 0xffff;
            bmax = 0xffff;

            rshift = 0;
            rmask = visual->red_mask;
            while (0 == (rmask & 1)) {
                rshift++;
                rmask >>= 1;
            }

/*             printf("rmask = %4x rshift = %4d\n", rmask, rshift); */

            gshift = 0;
            gmask = visual->green_mask;
            while (0 == (gmask & 1)) {
                gshift++;
                gmask >>= 1;
            }

/*             printf("gmask = %4x gshift = %4d\n", gmask, gshift); */

            bshift = 0;
            bmask = visual->blue_mask;
            while (0 == (bmask & 1)) {
                bshift++;
                bmask >>= 1;
            }

/*             printf("bmask = %4x bshift = %4d\n", bmask, bshift); */

            /* build the color table pixel values */
            for (i = 0; i < ncolors; i++) {
                Uint32 red = (rmax * i) / (ncolors - 1);
                Uint32 green = (gmax * i) / (ncolors - 1);
                Uint32 blue = (bmax * i) / (ncolors - 1);

                Uint32 rbits = (rmask * i) / (ncolors - 1);
                Uint32 gbits = (gmask * i) / (ncolors - 1);
                Uint32 bbits = (bmask * i) / (ncolors - 1);

                Uint32 pix =
                    (rbits << rshift) | (gbits << gshift) | (bbits << bshift);

                colorcells[i].pixel = pix;

                colorcells[i].red = red;
                colorcells[i].green = green;
                colorcells[i].blue = blue;

                colorcells[i].flags = DoRed | DoGreen | DoBlue;
/* 		printf("%2d:%4x [%4x %4x %4x]\n", i, pix, red, green, blue); */
            }

            status =
                XStoreColors(data->display, colormap, colorcells, ncolors);

            xattr.colormap = colormap;
            X11_TrackColormap(data->display, displaydata->screen,
                              colormap, visual, colorcells);

            SDL_free(colorcells);
        }
    } else {
        xattr.colormap =
            XCreateColormap(data->display,
                            RootWindow(data->display, displaydata->screen),
                            visual, AllocNone);
    }

    if ((window->flags & SDL_WINDOW_FULLSCREEN)
        || window->x == SDL_WINDOWPOS_CENTERED) {
        X11_GetDisplaySize(_this, window, &x, NULL);
        x = (x - window->w) / 2;
    } else if (window->x == SDL_WINDOWPOS_UNDEFINED) {
        x = 0;
    } else {
        x = window->x;
    }
    if ((window->flags & SDL_WINDOW_FULLSCREEN)
        || window->y == SDL_WINDOWPOS_CENTERED) {
        X11_GetDisplaySize(_this, window, NULL, &y);
        y = (y - window->h) / 2;
    } else if (window->y == SDL_WINDOWPOS_UNDEFINED) {
        y = 0;
    } else {
        y = window->y;
    }

    w = XCreateWindow(data->display,
                      RootWindow(data->display, displaydata->screen), x, y,
                      window->w, window->h, 0, depth, InputOutput, visual,
                      (CWOverrideRedirect | CWBackPixel | CWBorderPixel |
                       CWColormap), &xattr);
    if (!w) {
        SDL_SetError("Couldn't create window");
        return -1;
    }
#if SDL_VIDEO_DRIVER_PANDORA
    /* Create the GLES window surface */
    _this->gles_data->egl_surface =
        _this->gles_data->eglCreateWindowSurface(_this->gles_data->
                                                 egl_display,
                                                 _this->gles_data->egl_config,
                                                 (NativeWindowType) w, NULL);

    if (_this->gles_data->egl_surface == EGL_NO_SURFACE) {
        SDL_SetError("Could not create GLES window surface");
        return -1;
    }
#endif

    sizehints = XAllocSizeHints();
    if (sizehints) {
        if (!(window->flags & SDL_WINDOW_RESIZABLE)
            || (window->flags & SDL_WINDOW_FULLSCREEN)) {
            sizehints->min_width = sizehints->max_width = window->w;
            sizehints->min_height = sizehints->max_height = window->h;
            sizehints->flags = PMaxSize | PMinSize;
        }
        if (!(window->flags & SDL_WINDOW_FULLSCREEN)
            && window->x != SDL_WINDOWPOS_UNDEFINED
            && window->y != SDL_WINDOWPOS_UNDEFINED) {
            sizehints->x = x;
            sizehints->y = y;
            sizehints->flags |= USPosition;
        }
        XSetWMNormalHints(data->display, w, sizehints);
        XFree(sizehints);
    }

    if (window->flags & (SDL_WINDOW_BORDERLESS | SDL_WINDOW_FULLSCREEN)) {
        SDL_bool set;
        Atom WM_HINTS;

        /* We haven't modified the window manager hints yet */
        set = SDL_FALSE;

        /* First try to set MWM hints */
        WM_HINTS = XInternAtom(data->display, "_MOTIF_WM_HINTS", True);
        if (WM_HINTS != None) {
            /* Hints used by Motif compliant window managers */
            struct
            {
                unsigned long flags;
                unsigned long functions;
                unsigned long decorations;
                long input_mode;
                unsigned long status;
            } MWMHints = {
            (1L << 1), 0, 0, 0, 0};

            XChangeProperty(data->display, w, WM_HINTS, WM_HINTS, 32,
                            PropModeReplace, (unsigned char *) &MWMHints,
                            sizeof(MWMHints) / sizeof(long));
            set = SDL_TRUE;
        }
        /* Now try to set KWM hints */
        WM_HINTS = XInternAtom(data->display, "KWM_WIN_DECORATION", True);
        if (WM_HINTS != None) {
            long KWMHints = 0;

            XChangeProperty(data->display, w,
                            WM_HINTS, WM_HINTS, 32,
                            PropModeReplace,
                            (unsigned char *) &KWMHints,
                            sizeof(KWMHints) / sizeof(long));
            set = SDL_TRUE;
        }
        /* Now try to set GNOME hints */
        WM_HINTS = XInternAtom(data->display, "_WIN_HINTS", True);
        if (WM_HINTS != None) {
            long GNOMEHints = 0;

            XChangeProperty(data->display, w,
                            WM_HINTS, WM_HINTS, 32,
                            PropModeReplace,
                            (unsigned char *) &GNOMEHints,
                            sizeof(GNOMEHints) / sizeof(long));
            set = SDL_TRUE;
        }
        /* Finally set the transient hints if necessary */
        if (!set) {
            XSetTransientForHint(data->display, w,
                                 RootWindow(data->display,
                                            displaydata->screen));
        }
    } else {
        SDL_bool set;
        Atom WM_HINTS;

        /* We haven't modified the window manager hints yet */
        set = SDL_FALSE;

        /* First try to unset MWM hints */
        WM_HINTS = XInternAtom(data->display, "_MOTIF_WM_HINTS", True);
        if (WM_HINTS != None) {
            XDeleteProperty(data->display, w, WM_HINTS);
            set = SDL_TRUE;
        }
        /* Now try to unset KWM hints */
        WM_HINTS = XInternAtom(data->display, "KWM_WIN_DECORATION", True);
        if (WM_HINTS != None) {
            XDeleteProperty(data->display, w, WM_HINTS);
            set = SDL_TRUE;
        }
        /* Now try to unset GNOME hints */
        WM_HINTS = XInternAtom(data->display, "_WIN_HINTS", True);
        if (WM_HINTS != None) {
            XDeleteProperty(data->display, w, WM_HINTS);
            set = SDL_TRUE;
        }
        /* Finally unset the transient hints if necessary */
        if (!set) {
            /* NOTE: Does this work? */
            XSetTransientForHint(data->display, w, None);
        }
    }

    /* Tell KDE to keep fullscreen windows on top */
    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        XEvent ev;
        long mask;

        SDL_zero(ev);
        ev.xclient.type = ClientMessage;
        ev.xclient.window = RootWindow(data->display, displaydata->screen);
        ev.xclient.message_type =
            XInternAtom(data->display, "KWM_KEEP_ON_TOP", False);
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = w;
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(data->display,
                   RootWindow(data->display, displaydata->screen), False,
                   SubstructureRedirectMask, &ev);
    }

    /* Set the input hints so we get keyboard input */
    wmhints = XAllocWMHints();
    if (wmhints) {
        wmhints->input = True;
        wmhints->flags = InputHint;
        XSetWMHints(data->display, w, wmhints);
        XFree(wmhints);
    }

    /* Set the class hints so we can get an icon (AfterStep) */
    classhints = XAllocClassHint();
    if (classhints != NULL) {
        classhints->res_name = data->classname;
        classhints->res_class = data->classname;
        XSetClassHint(data->display, w, classhints);
        XFree(classhints);
    }

    /* Allow the window to be deleted by the window manager */
    XSetWMProtocols(data->display, w, &data->WM_DELETE_WINDOW, 1);

    if (SetupWindowData(_this, window, w, SDL_TRUE) < 0) {
        XDestroyWindow(data->display, w);
        return -1;
    }
#ifdef X_HAVE_UTF8_STRING
    {
        Uint32 fevent = 0;
        pXGetICValues(((SDL_WindowData *) window->driverdata)->ic,
                      XNFilterEvents, &fevent, NULL);
        XSelectInput(data->display, w,
                     (FocusChangeMask | EnterWindowMask | LeaveWindowMask |
                      ExposureMask | ButtonPressMask | ButtonReleaseMask |
                      PointerMotionMask | KeyPressMask | KeyReleaseMask |
                      PropertyChangeMask | StructureNotifyMask |
                      KeymapStateMask | fevent));
    }
#else
    {
        XSelectInput(data->display, w,
                     (FocusChangeMask | EnterWindowMask | LeaveWindowMask |
                      ExposureMask | ButtonPressMask | ButtonReleaseMask |
                      PointerMotionMask | KeyPressMask | KeyReleaseMask |
                      PropertyChangeMask | StructureNotifyMask |
                      KeymapStateMask));
    }
#endif

#if SDL_VIDEO_DRIVER_X11_XINPUT
    /* we're informing the display what extension events we want to receive from it */
    {
        int i, j, n = 0;
        XEventClass xevents[256];

        for (i = 0; i < SDL_GetNumMice(); ++i) {
            SDL_Mouse *mouse;
            X11_MouseData *data;

            mouse = SDL_GetMouse(i);
            data = (X11_MouseData *) mouse->driverdata;
            if (!data) {
                continue;
            }

            for (j = 0; j < data->num_xevents; ++j) {
                xevents[n++] = data->xevents[j];
            }
        }
        if (n > 0) {
            XSelectExtensionEvent(data->display, w, xevents, n);
        }
    }
#endif

    return 0;
}

int
X11_CreateWindowFrom(_THIS, SDL_Window * window, const void *data)
{
    Window w = (Window) data;

    /* FIXME: Query the title from the existing window */

    if (SetupWindowData(_this, window, w, SDL_FALSE) < 0) {
        return -1;
    }
    return 0;
}

void
X11_SetWindowTitle(_THIS, SDL_Window * window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    Display *display = data->videodata->display;
    XTextProperty titleprop, iconprop;
    Status status;
    const char *title = window->title;
    const char *icon = NULL;

#ifdef X_HAVE_UTF8_STRING
    Atom _NET_WM_NAME = 0;
    Atom _NET_WM_ICON_NAME = 0;

    /* Look up some useful Atoms */
    if (SDL_X11_HAVE_UTF8) {
        _NET_WM_NAME = XInternAtom(display, "_NET_WM_NAME", False);
        _NET_WM_ICON_NAME = XInternAtom(display, "_NET_WM_ICON_NAME", False);
    }
#endif

    if (title != NULL) {
        char *title_locale = SDL_iconv_utf8_locale(title);
        if (!title_locale) {
            SDL_OutOfMemory();
            return;
        }
        status = XStringListToTextProperty(&title_locale, 1, &titleprop);
        SDL_free(title_locale);
        if (status) {
            XSetTextProperty(display, data->window, &titleprop, XA_WM_NAME);
            XFree(titleprop.value);
        }
#ifdef X_HAVE_UTF8_STRING
        if (SDL_X11_HAVE_UTF8) {
            status =
                Xutf8TextListToTextProperty(display, (char **) &title, 1,
                                            XUTF8StringStyle, &titleprop);
            if (status == Success) {
                XSetTextProperty(display, data->window, &titleprop,
                                 _NET_WM_NAME);
                XFree(titleprop.value);
            }
        }
#endif
    }
    if (icon != NULL) {
        char *icon_locale = SDL_iconv_utf8_locale(icon);
        if (!icon_locale) {
            SDL_OutOfMemory();
            return;
        }
        status = XStringListToTextProperty(&icon_locale, 1, &iconprop);
        SDL_free(icon_locale);
        if (status) {
            XSetTextProperty(display, data->window, &iconprop,
                             XA_WM_ICON_NAME);
            XFree(iconprop.value);
        }
#ifdef X_HAVE_UTF8_STRING
        if (SDL_X11_HAVE_UTF8) {
            status =
                Xutf8TextListToTextProperty(display, (char **) &icon, 1,
                                            XUTF8StringStyle, &iconprop);
            if (status == Success) {
                XSetTextProperty(display, data->window, &iconprop,
                                 _NET_WM_ICON_NAME);
                XFree(iconprop.value);
            }
        }
#endif
    }
}

void
X11_SetWindowIcon(_THIS, SDL_Window * window, SDL_Surface * icon)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    Display *display = data->videodata->display;
    Atom _NET_WM_ICON = XInternAtom(display, "_NET_WM_ICON", False);

    if (icon) {
        SDL_PixelFormat format;
        SDL_Surface *surface;
        int propsize;
        Uint32 *propdata;

        /* Convert the icon to ARGB for modern window managers */
        SDL_InitFormat(&format, 32, 0x00FF0000, 0x0000FF00, 0x000000FF,
                       0xFF000000);
        surface = SDL_ConvertSurface(icon, &format, 0);
        if (!surface) {
            return;
        }

        /* Set the _NET_WM_ICON property */
        propsize = 2 + (icon->w * icon->h);
        propdata = SDL_malloc(propsize * sizeof(Uint32));
        if (propdata) {
            propdata[0] = icon->w;
            propdata[1] = icon->h;
            SDL_memcpy(&propdata[2], surface->pixels,
                       surface->h * surface->pitch);
            XChangeProperty(display, data->window, _NET_WM_ICON, XA_CARDINAL,
                            32, PropModeReplace, (unsigned char *) propdata,
                            propsize);
        }
        SDL_FreeSurface(surface);
    } else {
        XDeleteProperty(display, data->window, _NET_WM_ICON);
    }
}

void
X11_SetWindowPosition(_THIS, SDL_Window * window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    SDL_DisplayData *displaydata =
        (SDL_DisplayData *) SDL_GetDisplayFromWindow(window)->driverdata;
    Display *display = data->videodata->display;
    int x, y;

    if ((window->flags & SDL_WINDOW_FULLSCREEN)
        || window->x == SDL_WINDOWPOS_CENTERED) {
        X11_GetDisplaySize(_this, window, &x, NULL);
        x = (x - window->w) / 2;
    } else {
        x = window->x;
    }
    if ((window->flags & SDL_WINDOW_FULLSCREEN)
        || window->y == SDL_WINDOWPOS_CENTERED) {
        X11_GetDisplaySize(_this, window, NULL, &y);
        y = (y - window->h) / 2;
    } else {
        y = window->y;
    }
    XMoveWindow(display, data->window, x, y);
}

void
X11_SetWindowSize(_THIS, SDL_Window * window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    Display *display = data->videodata->display;

    XResizeWindow(display, data->window, window->w, window->h);
}

void
X11_ShowWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    Display *display = data->videodata->display;

    XMapRaised(display, data->window);
}

void
X11_HideWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    Display *display = data->videodata->display;

    XUnmapWindow(display, data->window);
}

void
X11_RaiseWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    Display *display = data->videodata->display;

    XRaiseWindow(display, data->window);
}

static void
X11_SetWindowMaximized(_THIS, SDL_Window * window, SDL_bool maximized)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    SDL_DisplayData *displaydata =
        (SDL_DisplayData *) SDL_GetDisplayFromWindow(window)->driverdata;
    Display *display = data->videodata->display;
    Atom _NET_WM_STATE = XInternAtom(display, "_NET_WM_STATE", False);
    Atom _NET_WM_STATE_MAXIMIZED_VERT =
        XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    Atom _NET_WM_STATE_MAXIMIZED_HORZ =
        XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    XEvent e;

    e.xany.type = ClientMessage;
    e.xany.window = data->window;
    e.xclient.message_type = _NET_WM_STATE;
    e.xclient.format = 32;
    e.xclient.data.l[0] =
        maximized ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
    e.xclient.data.l[1] = _NET_WM_STATE_MAXIMIZED_VERT;
    e.xclient.data.l[2] = _NET_WM_STATE_MAXIMIZED_HORZ;
    e.xclient.data.l[3] = 0l;
    e.xclient.data.l[4] = 0l;

    XSendEvent(display, RootWindow(display, displaydata->screen), 0,
               SubstructureNotifyMask | SubstructureRedirectMask, &e);
}

void
X11_MaximizeWindow(_THIS, SDL_Window * window)
{
    X11_SetWindowMaximized(_this, window, SDL_TRUE);
}

void
X11_MinimizeWindow(_THIS, SDL_Window * window)
{
    X11_HideWindow(_this, window);
}

void
X11_RestoreWindow(_THIS, SDL_Window * window)
{
    X11_SetWindowMaximized(_this, window, SDL_FALSE);
    X11_ShowWindow(_this, window);
}

void
X11_SetWindowGrab(_THIS, SDL_Window * window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    Display *display = data->videodata->display;

    if ((window->flags & (SDL_WINDOW_INPUT_GRABBED | SDL_WINDOW_FULLSCREEN))
        && (window->flags & SDL_WINDOW_INPUT_FOCUS)) {
        /* Try to grab the mouse */
        for (;;) {
            int result =
                XGrabPointer(display, data->window, True, 0, GrabModeAsync,
                             GrabModeAsync, data->window, None, CurrentTime);
            if (result == GrabSuccess) {
                break;
            }
            SDL_Delay(100);
        }

        /* Raise the window if we grab the mouse */
        XRaiseWindow(display, data->window);

        /* Now grab the keyboard */
        XGrabKeyboard(display, data->window, True, GrabModeAsync,
                      GrabModeAsync, CurrentTime);
    } else {
        XUngrabPointer(display, CurrentTime);
        XUngrabKeyboard(display, CurrentTime);
    }
}

void
X11_DestroyWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    window->driverdata = NULL;

    if (data) {
        SDL_VideoData *videodata = (SDL_VideoData *) data->videodata;
        Display *display = videodata->display;
        int numwindows = videodata->numwindows;
        SDL_WindowData **windowlist = videodata->windowlist;
        int i;

        if (windowlist) {
            for (i = 0; i < numwindows; ++i) {
                if (windowlist[i] && (windowlist[i]->windowID == window->id)) {
                    windowlist[i] = windowlist[numwindows - 1];
                    windowlist[numwindows - 1] = NULL;
                    videodata->numwindows--;
                    break;
                }
            }
        }
#ifdef X_HAVE_UTF8_STRING
        if (data->ic) {
            XDestroyIC(data->ic);
        }
#endif
        if (data->created) {
            XDestroyWindow(display, data->window);
        }
        SDL_free(data);
    }
}

SDL_bool
X11_GetWindowWMInfo(_THIS, SDL_Window * window, SDL_SysWMinfo * info)
{
    if (info->version.major <= SDL_MAJOR_VERSION) {
        /* FIXME! */
        return SDL_TRUE;
    } else {
        SDL_SetError("Application not compiled with SDL %d.%d\n",
                     SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
        return SDL_FALSE;
    }
}

/* vi: set ts=4 sw=4 expandtab: */
