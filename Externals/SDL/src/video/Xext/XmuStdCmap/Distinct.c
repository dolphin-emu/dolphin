/* $Xorg: Distinct.c,v 1.4 2001/02/09 02:03:52 xorgcvs Exp $ */

/*

Copyright 1990, 1998  The Open Group

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
/* $XFree86: xc/lib/Xmu/Distinct.c,v 3.5 2001/07/25 15:04:50 dawes Exp $ */

/*
 * Author:  Keith Packard, MIT X Consortium
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/Xlib.h>
#include <stdlib.h>
#include <X11/Xutil.h>
#include "../extensions/StdCmap.h"

/*
 * Distinguishable colors routine.  Determines if two colors are
 * distinguishable or not.  Somewhat arbitrary meaning.
 */

#define MIN_DISTINGUISH	10000.0

Bool
XmuDistinguishableColors(XColor * colors, int count)
{
    double deltaRed, deltaGreen, deltaBlue;
    double dist;
    int i, j;

    for (i = 0; i < count - 1; i++)
        for (j = i + 1; j < count; j++) {
            deltaRed = (double) colors[i].red - (double) colors[j].red;
            deltaGreen = (double) colors[i].green - (double) colors[j].green;
            deltaBlue = (double) colors[i].blue - (double) colors[j].blue;
            dist = deltaRed * deltaRed +
                deltaGreen * deltaGreen + deltaBlue * deltaBlue;
            if (dist <= MIN_DISTINGUISH * MIN_DISTINGUISH)
                return False;
        }
    return True;
}

Bool
XmuDistinguishablePixels(Display * dpy, Colormap cmap,
                         unsigned long *pixels, int count)
{
    XColor *defs;
    int i, j;
    Bool ret;

    for (i = 0; i < count - 1; i++)
        for (j = i + 1; j < count; j++)
            if (pixels[i] == pixels[j])
                return False;
    defs = (XColor *) malloc(count * sizeof(XColor));
    if (!defs)
        return False;
    for (i = 0; i < count; i++)
        defs[i].pixel = pixels[i];
    XQueryColors(dpy, cmap, defs, count);
    ret = XmuDistinguishableColors(defs, count);
    free((char *) defs);
    return ret;
}
