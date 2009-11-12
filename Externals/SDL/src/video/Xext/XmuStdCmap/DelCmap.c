/* $Xorg: DelCmap.c,v 1.4 2001/02/09 02:03:52 xorgcvs Exp $ */

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
/* $XFree86: xc/lib/Xmu/DelCmap.c,v 1.6 2001/01/17 19:42:54 dawes Exp $ */

/*
 * Author:  Donna Converse, MIT X Consortium
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "../extensions/StdCmap.h"

/* To remove any standard colormap property, use XmuDeleteStandardColormap().
 * XmuDeleteStandardColormap() will remove the specified property from the
 * specified screen, releasing any resources used by the colormap(s) of the
 * property if possible.
 */

void
XmuDeleteStandardColormap(Display * dpy, int screen, Atom property)
     /* dpy;            - specifies the X server to connect to
      * screen          - specifies the screen of the display
      * property        - specifies the standard colormap property
      */
{
    XStandardColormap *stdcmaps, *s;
    int count = 0;

    if (XGetRGBColormaps(dpy, RootWindow(dpy, screen), &stdcmaps, &count,
                         property)) {
        for (s = stdcmaps; count > 0; count--, s++) {
            if ((s->killid == ReleaseByFreeingColormap) &&
                (s->colormap != None) &&
                (s->colormap != DefaultColormap(dpy, screen)))
                XFreeColormap(dpy, s->colormap);
            else if (s->killid != None)
                XKillClient(dpy, s->killid);
        }
        XDeleteProperty(dpy, RootWindow(dpy, screen), property);
        XFree((char *) stdcmaps);
        XSync(dpy, False);
    }
}
