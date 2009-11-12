/* $Xorg: AllCmap.c,v 1.4 2001/02/09 02:03:51 xorgcvs Exp $ */

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
/* $XFree86: xc/lib/Xmu/AllCmap.c,v 1.7 2001/01/17 19:42:53 dawes Exp $ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include "../extensions/StdCmap.h"

static XVisualInfo *getDeepestVisual(int, XVisualInfo *, int);

/*
 * To create all of the appropriate standard colormaps for every visual of
 * every screen on a given display, use XmuAllStandardColormaps.
 *
 * Define and retain as permanent resources all standard colormaps which are
 * meaningful for the visuals of each screen of the display.  Return 0 on
 * failure, non-zero on success.  If the property of any standard colormap 
 * is already defined, redefine it.
 *
 * This interface is intended to be used by window managers or a client
 * upon start-up of a session.
 *
 * The standard colormaps of a screen are defined by properties associated
 * with the screen's root window.  Each screen has exactly one root window.
 * The property names of standard colormaps are predefined, and each property
 * name may describe at most one colormap.
 * 
 * The standard colormaps are
 *		RGB_BEST_MAP
 *		RGB_RED_MAP
 *		RGB_GREEN_MAP
 *		RGB_BLUE_MAP
 *		RGB_DEFAULT_MAP
 *		RGB_GRAY_MAP
 *
 * Therefore a screen may have at most 6 standard colormap properties defined.
 *
 * A standard colormap is associated with a particular visual of the screen.
 * A screen may have multiple visuals defined, including visuals of the same
 * class at different depths.  Note that a visual id might be repeated for
 * more than one depth, so the visual id and the depth of a visual identify
 * the visual.  The characteristics of the visual will determine which
 * standard colormaps are meaningful under that visual, and will determine
 * how the standard colormap is defined.  Because a standard colormap is
 * associated with a specific visual, there must be a method of determining
 * which visuals take precedence in defining standard colormaps.
 * 
 * The method used here is: for the visual of greatest depth, define all 
 * standard colormaps meaningful to that visual class, according to this
 * order of (descending) precedence:
 *	1. DirectColor
 *	2. PseudoColor
 *	3. TrueColor and GrayScale
 *	4. StaticColor and StaticGray
 *
 * Allows partial success by screenful.  For example, if a map on screen 1
 * fails, the maps on screen 0, created earlier, will remain.  However,
 * none on screen 1 will remain.  If a map on 0 fails, none will remain.
 *
 * See the comments under XmuVisualStandardColormaps() for notes on which
 * standard colormaps are meaningful under these classes of visuals.
 */

Status
XmuAllStandardColormaps(Display * dpy)
{
    int nvisuals, scr;
    Status status;
    long vinfo_mask;
    XVisualInfo template, *vinfo, *v1, *v2;

    status = 0;
    /* for each screen, determine all visuals of this server */
    for (scr = 0; scr < ScreenCount(dpy); scr++) {
        template.screen = scr;
        vinfo_mask = VisualScreenMask;
        vinfo = XGetVisualInfo(dpy, vinfo_mask, &template, &nvisuals);
        if (vinfo == NULL)      /* unexpected: a screen with no visuals */
            continue;

        v1 = getDeepestVisual(DirectColor, vinfo, nvisuals);
        v2 = getDeepestVisual(PseudoColor, vinfo, nvisuals);

        if (v2 &&
            (!v1 || (v2->colormap_size >=
                     ((v1->red_mask | v1->green_mask | v1->blue_mask) + 1))))
            status = XmuVisualStandardColormaps(dpy, scr, v2->visualid,
                                                (unsigned) v2->depth, 1, 1);
        else if (v1)
            status = XmuVisualStandardColormaps(dpy, scr, v1->visualid,
                                                (unsigned) v1->depth, 1, 1);

        else {
            if (((v1 = getDeepestVisual(TrueColor, vinfo, nvisuals)) != NULL)
                || ((v1 = getDeepestVisual(StaticColor, vinfo, nvisuals)) !=
                    NULL))
                status = XmuVisualStandardColormaps(dpy, scr, v1->visualid,
                                                    (unsigned) v1->depth, 1,
                                                    1);
            if (status
                &&
                (((v1 = getDeepestVisual(GrayScale, vinfo, nvisuals)) != NULL)
                 || ((v1 = getDeepestVisual(StaticGray, vinfo, nvisuals)) !=
                     NULL)))
                status =
                    XmuVisualStandardColormaps(dpy, scr, v1->visualid,
                                               (unsigned) v1->depth, 1, 1);
        }
        XFree((char *) vinfo);
        if (!status)
            break;
    }
    return status;
}

static XVisualInfo *
getDeepestVisual(int visual_class, XVisualInfo * vinfo, int nvisuals)
{
    register int i;
    register int maxdepth = 0;
    XVisualInfo *v = NULL;

    for (i = 0; i < nvisuals; i++, vinfo++)
        if (vinfo->class == visual_class && vinfo->depth > maxdepth) {
            maxdepth = vinfo->depth;
            v = vinfo;
        }
    return (v);
}
