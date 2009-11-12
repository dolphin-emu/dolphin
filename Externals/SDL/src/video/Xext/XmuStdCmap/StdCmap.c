/* $Xorg: StdCmap.c,v 1.4 2001/02/09 02:03:53 xorgcvs Exp $ */

/* 

Copyright 1989, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/
/* $XFree86: xc/lib/Xmu/StdCmap.c,v 1.5 2001/01/17 19:42:56 dawes Exp $ */

/*
 * Author:  Donna Converse, MIT X Consortium
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include "../extensions/StdCmap.h"

#define lowbit(x) ((x) & (~(x) + 1))

/*
 * Prototypes
 */
/* argument restrictions */
static Status valid_args(XVisualInfo *, unsigned long, unsigned long,
                         unsigned long, Atom);

/*
 * To create any one standard colormap, use XmuStandardColormap().
 *
 * Create a standard colormap for the given screen, visualid, and visual
 * depth, with the given red, green, and blue maximum values, with the
 * given standard property name.  Return a pointer to an XStandardColormap
 * structure which describes the newly created colormap, upon success.
 * Upon failure, return NULL.
 * 
 * XmuStandardColormap() calls XmuCreateColormap() to create the map.
 *
 * Resources created by this function are not made permanent; that is the
 * caller's responsibility.
 */

XStandardColormap *
XmuStandardColormap(Display * dpy, int screen, VisualID visualid,
                    unsigned int depth, Atom property, Colormap cmap,
                    unsigned long red_max, unsigned long green_max,
                    unsigned long blue_max)
     /*
      * dpy                             - specifies X server connection
      * screen                          - specifies display screen
      * visualid                        - identifies the visual type
      * depth                           - identifies the visual type
      * property                        - a standard colormap property
      * cmap                            - specifies colormap ID or None
      * red_max, green_max, blue_max    - allocations
      */
{
    XStandardColormap *stdcmap;
    Status status;
    XVisualInfo vinfo_template, *vinfo;
    long vinfo_mask;
    int n;

    /* Match the required visual information to an actual visual */
    vinfo_template.visualid = visualid;
    vinfo_template.screen = screen;
    vinfo_template.depth = depth;
    vinfo_mask = VisualIDMask | VisualScreenMask | VisualDepthMask;
    if ((vinfo =
         XGetVisualInfo(dpy, vinfo_mask, &vinfo_template, &n)) == NULL)
        return 0;

    /* Check the validity of the combination of visual characteristics,
     * allocation, and colormap property.  Create an XStandardColormap
     * structure.
     */

    if (!valid_args(vinfo, red_max, green_max, blue_max, property)
        || ((stdcmap = XAllocStandardColormap()) == NULL)) {
        XFree((char *) vinfo);
        return 0;
    }

    /* Fill in the XStandardColormap structure */

    if (cmap == DefaultColormap(dpy, screen)) {
        /* Allocating out of the default map, cannot use XFreeColormap() */
        Window win = XCreateWindow(dpy, RootWindow(dpy, screen), 1, 1, 1, 1,
                                   0, 0, InputOnly, vinfo->visual,
                                   (unsigned long) 0,
                                   (XSetWindowAttributes *) NULL);
        stdcmap->killid = (XID) XCreatePixmap(dpy, win, 1, 1, depth);
        XDestroyWindow(dpy, win);
        stdcmap->colormap = cmap;
    } else {
        stdcmap->killid = ReleaseByFreeingColormap;
        stdcmap->colormap = XCreateColormap(dpy, RootWindow(dpy, screen),
                                            vinfo->visual, AllocNone);
    }
    stdcmap->red_max = red_max;
    stdcmap->green_max = green_max;
    stdcmap->blue_max = blue_max;
    if (property == XA_RGB_GRAY_MAP)
        stdcmap->red_mult = stdcmap->green_mult = stdcmap->blue_mult = 1;
    else if (vinfo->class == TrueColor || vinfo->class == DirectColor) {
        stdcmap->red_mult = lowbit(vinfo->red_mask);
        stdcmap->green_mult = lowbit(vinfo->green_mask);
        stdcmap->blue_mult = lowbit(vinfo->blue_mask);
    } else {
        stdcmap->red_mult = (red_max > 0)
            ? (green_max + 1) * (blue_max + 1) : 0;
        stdcmap->green_mult = (green_max > 0) ? blue_max + 1 : 0;
        stdcmap->blue_mult = (blue_max > 0) ? 1 : 0;
    }
    stdcmap->base_pixel = 0;    /* base pixel may change */
    stdcmap->visualid = vinfo->visualid;

    /* Make the colormap */

    status = XmuCreateColormap(dpy, stdcmap);

    /* Clean up */

    XFree((char *) vinfo);
    if (!status) {

        /* Free the colormap or the pixmap, if we created one */
        if (stdcmap->killid == ReleaseByFreeingColormap)
            XFreeColormap(dpy, stdcmap->colormap);
        else if (stdcmap->killid != None)
            XFreePixmap(dpy, stdcmap->killid);

        XFree((char *) stdcmap);
        return (XStandardColormap *) NULL;
    }
    return stdcmap;
}

/****************************************************************************/
static Status
valid_args(XVisualInfo * vinfo, unsigned long red_max,
           unsigned long green_max, unsigned long blue_max, Atom property)
     /*
      * vinfo                           - specifies visual
      * red_max, green_max, blue_max    - specifies alloc
      * property                        - specifies property name
      */
{
    unsigned long ncolors;      /* number of colors requested */

    /* Determine that the number of colors requested is <= map size */

    if ((vinfo->class == DirectColor) || (vinfo->class == TrueColor)) {
        unsigned long mask;

        mask = vinfo->red_mask;
        while (!(mask & 1))
            mask >>= 1;
        if (red_max > mask)
            return 0;
        mask = vinfo->green_mask;
        while (!(mask & 1))
            mask >>= 1;
        if (green_max > mask)
            return 0;
        mask = vinfo->blue_mask;
        while (!(mask & 1))
            mask >>= 1;
        if (blue_max > mask)
            return 0;
    } else if (property == XA_RGB_GRAY_MAP) {
        ncolors = red_max + green_max + blue_max + 1;
        if (ncolors > vinfo->colormap_size)
            return 0;
    } else {
        ncolors = (red_max + 1) * (green_max + 1) * (blue_max + 1);
        if (ncolors > vinfo->colormap_size)
            return 0;
    }

    /* Determine that the allocation and visual make sense for the property */

    switch (property) {
    case XA_RGB_DEFAULT_MAP:
        if (red_max == 0 || green_max == 0 || blue_max == 0)
            return 0;
        break;
    case XA_RGB_RED_MAP:
        if (red_max == 0)
            return 0;
        break;
    case XA_RGB_GREEN_MAP:
        if (green_max == 0)
            return 0;
        break;
    case XA_RGB_BLUE_MAP:
        if (blue_max == 0)
            return 0;
        break;
    case XA_RGB_BEST_MAP:
        if (red_max == 0 || green_max == 0 || blue_max == 0)
            return 0;
        break;
    case XA_RGB_GRAY_MAP:
        if (red_max == 0 || blue_max == 0 || green_max == 0)
            return 0;
        break;
    default:
        return 0;
    }
    return 1;
}
