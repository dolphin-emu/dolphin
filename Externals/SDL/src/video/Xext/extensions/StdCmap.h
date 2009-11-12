/* $Xorg: StdCmap.h,v 1.5 2001/02/09 02:03:53 xorgcvs Exp $ */

/*
 
Copyright 1988, 1998  The Open Group

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
/* $XFree86: xc/lib/Xmu/StdCmap.h,v 1.8 2001/01/23 17:38:14 keithp Exp $ */

/*
 * The interfaces described by this header file are for miscellaneous utilities
 * and are not part of the Xlib standard.
 */

#ifndef _XMU_STDCMAP_H_
#define _XMU_STDCMAP_H_

#include <X11/Xfuncproto.h>
#include "SDL_name.h"

_XFUNCPROTOBEGIN extern Status XmuAllStandardColormaps(Display * dpy);

extern Status XmuCreateColormap(Display * dpy, XStandardColormap * colormap);

extern void XmuDeleteStandardColormap
    (Display * dpy, int screen, Atom property);

extern Status XmuGetColormapAllocation
    (XVisualInfo * vinfo,
     Atom property,
     unsigned long *red_max_return,
     unsigned long *green_max_return, unsigned long *blue_max_return);

extern Status XmuLookupStandardColormap
    (Display * dpy,
     int screen,
     VisualID visualid,
     unsigned int depth, Atom property, Bool replace, Bool retain);

extern XStandardColormap *XmuStandardColormap
    (Display * dpy,
     int screen,
     VisualID visualid,
     unsigned int depth,
     Atom property,
     Colormap cmap,
     unsigned long red_max, unsigned long green_max, unsigned long blue_max);

extern Status XmuVisualStandardColormaps
    (Display * dpy,
     int screen,
     VisualID visualid, unsigned int depth, Bool replace, Bool retain);

extern Bool XmuDistinguishableColors(XColor * colors, int count);

extern Bool XmuDistinguishablePixels
    (Display * dpy, Colormap cmap, unsigned long *pixels, int count);

_XFUNCPROTOEND
#endif /* _XMU_STDCMAP_H_ */
