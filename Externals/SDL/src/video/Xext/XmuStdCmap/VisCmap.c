/* $Xorg: VisCmap.c,v 1.4 2001/02/09 02:03:53 xorgcvs Exp $ */

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
/* $XFree86: xc/lib/Xmu/VisCmap.c,v 1.6 2001/01/17 19:42:57 dawes Exp $ */

/*
 * Author:  Donna Converse, MIT X Consortium
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <math.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include "../extensions/StdCmap.h"

/*
 * To create all of the appropriate standard colormaps for a given visual on
 * a given screen, use XmuVisualStandardColormaps.
 * 
 * Define all appropriate standard colormap properties for the given visual.
 * If replace is true, any previous definition will be removed.
 * If retain is true, new properties will be retained for the duration of
 * the server session.  Return 0 on failure, non-zero on success.
 * On failure, no new properties will be defined, and, old ones may have
 * been removed if replace was True.
 *
 * Not all standard colormaps are meaningful to all visual classes.  This
 * routine will check and define the following properties for the following
 * classes, provided that the size of the colormap is not too small.
 *
 *	DirectColor and PseudoColor
 *	    RGB_DEFAULT_MAP
 *	    RGB_BEST_MAP
 *	    RGB_RED_MAP
 *	    RGB_GREEN_MAP
 * 	    RGB_BLUE_MAP
 *          RGB_GRAY_MAP
 *
 *	TrueColor and StaticColor
 *	    RGB_BEST_MAP
 *
 *	GrayScale and StaticGray
 *	    RGB_GRAY_MAP
 */

Status
XmuVisualStandardColormaps(Display * dpy, int screen, VisualID visualid,
                           unsigned int depth, Bool replace, Bool retain)
     /*
      * dpy                     - specifies server connection
      * screen                  - specifies screen number
      * visualid                - specifies the visual
      * depth                   - specifies the visual
      * replace specifies       - whether to replace
      * retain                  - specifies whether to retain
      */
{
    Status status;
    int n;
    long vinfo_mask;
    XVisualInfo vinfo_template, *vinfo;

    status = 0;
    vinfo_template.screen = screen;
    vinfo_template.visualid = visualid;
    vinfo_template.depth = depth;
    vinfo_mask = VisualScreenMask | VisualIDMask | VisualDepthMask;
    if ((vinfo =
         XGetVisualInfo(dpy, vinfo_mask, &vinfo_template, &n)) == NULL)
        return 0;

    if (vinfo->colormap_size <= 2) {
        /* Monochrome visuals have no standard maps; considered successful */
        XFree((char *) vinfo);
        return 1;
    }

    switch (vinfo->class) {
    case PseudoColor:
    case DirectColor:
        status = XmuLookupStandardColormap(dpy, screen, visualid, depth,
                                           XA_RGB_DEFAULT_MAP, replace,
                                           retain);
        if (!status)
            break;

        status = XmuLookupStandardColormap(dpy, screen, visualid, depth,
                                           XA_RGB_GRAY_MAP, replace, retain);
        if (!status) {
            XmuDeleteStandardColormap(dpy, screen, XA_RGB_DEFAULT_MAP);
            break;
        }

        status = XmuLookupStandardColormap(dpy, screen, visualid, depth,
                                           XA_RGB_RED_MAP, replace, retain);
        if (!status) {
            XmuDeleteStandardColormap(dpy, screen, XA_RGB_DEFAULT_MAP);
            XmuDeleteStandardColormap(dpy, screen, XA_RGB_GRAY_MAP);
            break;
        }

        status = XmuLookupStandardColormap(dpy, screen, visualid, depth,
                                           XA_RGB_GREEN_MAP, replace, retain);
        if (!status) {
            XmuDeleteStandardColormap(dpy, screen, XA_RGB_DEFAULT_MAP);
            XmuDeleteStandardColormap(dpy, screen, XA_RGB_GRAY_MAP);
            XmuDeleteStandardColormap(dpy, screen, XA_RGB_RED_MAP);
            break;
        }

        status = XmuLookupStandardColormap(dpy, screen, visualid, depth,
                                           XA_RGB_BLUE_MAP, replace, retain);
        if (!status) {
            XmuDeleteStandardColormap(dpy, screen, XA_RGB_DEFAULT_MAP);
            XmuDeleteStandardColormap(dpy, screen, XA_RGB_GRAY_MAP);
            XmuDeleteStandardColormap(dpy, screen, XA_RGB_RED_MAP);
            XmuDeleteStandardColormap(dpy, screen, XA_RGB_GREEN_MAP);
            break;
        }
        /* fall through */

    case StaticColor:
    case TrueColor:

        status = XmuLookupStandardColormap(dpy, screen, visualid, depth,
                                           XA_RGB_BEST_MAP, replace, retain);
        if (!status && (vinfo->class == PseudoColor ||
                        vinfo->class == DirectColor)) {
            XmuDeleteStandardColormap(dpy, screen, XA_RGB_DEFAULT_MAP);
            XmuDeleteStandardColormap(dpy, screen, XA_RGB_GRAY_MAP);
            XmuDeleteStandardColormap(dpy, screen, XA_RGB_RED_MAP);
            XmuDeleteStandardColormap(dpy, screen, XA_RGB_GREEN_MAP);
            XmuDeleteStandardColormap(dpy, screen, XA_RGB_BLUE_MAP);
        }
        break;
        /* the end for PseudoColor, DirectColor, StaticColor, and TrueColor */

    case GrayScale:
        status = XmuLookupStandardColormap(dpy, screen, visualid, depth,
                                           XA_RGB_DEFAULT_MAP, replace,
                                           retain);
        if (!status)
            break;
     /*FALLTHROUGH*/ case StaticGray:

        status = XmuLookupStandardColormap(dpy, screen, visualid, depth,
                                           XA_RGB_GRAY_MAP, replace, retain);
        if (!status && vinfo->class == GrayScale) {
            XmuDeleteStandardColormap(dpy, screen, XA_RGB_DEFAULT_MAP);
            break;
        }
    }

    XFree((char *) vinfo);
    return status;
}
