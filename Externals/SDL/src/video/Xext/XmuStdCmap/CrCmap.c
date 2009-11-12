/* $Xorg: CrCmap.c,v 1.4 2001/02/09 02:03:51 xorgcvs Exp $ */

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
/* $XFree86: xc/lib/Xmu/CrCmap.c,v 3.6 2001/01/17 19:42:53 dawes Exp $ */

/*
 * Author:  Donna Converse, MIT X Consortium
 */

/*
 * CreateCmap.c - given a standard colormap description, make the map.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "../extensions/StdCmap.h"

/*
 * Prototypes
 */
/* allocate entire map Read Only */
static int ROmap(Display *, Colormap, unsigned long[], int, int);

/* allocate a cell, prefer Read Only */
static Status ROorRWcell(Display *, Colormap, unsigned long[], int,
                         XColor *, unsigned long);

/* allocate a cell Read Write */
static Status RWcell(Display *, Colormap, XColor *, XColor *,
                     unsigned long *);

/* for quicksort */
static int compare(_Xconst void *, _Xconst void *);

/* find contiguous sequence of cells */
static Status contiguous(unsigned long[], int, int, unsigned long, int *,
                         int *);

/* frees resources before quitting */
static void free_cells(Display *, Colormap, unsigned long[], int, int);

/* create a map in a RO visual type */
static Status readonly_map(Display *, XVisualInfo *, XStandardColormap *);

/* create a map in a RW visual type */
static Status readwrite_map(Display *, XVisualInfo *, XStandardColormap *);

#define lowbit(x) ((x) & (~(x) + 1))
#define TRUEMATCH(mult,max,mask) \
    (colormap->max * colormap->mult <= vinfo->mask && \
     lowbit(vinfo->mask) == colormap->mult)

/*
 * To create any one colormap which is described by an XStandardColormap
 * structure, use XmuCreateColormap().
 *
 * Return 0 on failure, non-zero on success.
 * Resources created by this function are not made permanent.
 * No argument error checking is provided.  Use at your own risk.
 *
 * All colormaps are created with read only allocations, with the exception
 * of read only allocations of colors in the default map or otherwise
 * which fail to return the expected pixel value, and these are individually 
 * defined as read/write allocations.  This is done so that all the cells
 * defined in the default map are contiguous, for use in image processing.
 * This typically happens with White and Black in the default map.
 *
 * Colormaps of static visuals are considered to be successfully created if
 * the map of the static visual matches the definition given in the
 * standard colormap structure.
 */

Status
XmuCreateColormap(Display * dpy, XStandardColormap * colormap)
     /* dpy      - specifies the connection under which the map is created
      * colormap - specifies the map to be created, and returns, particularly
      *            if the map is created as a subset of the default colormap
      *            of the screen, the base_pixel of the map.
      */
{
    XVisualInfo vinfo_template; /* template visual information */
    XVisualInfo *vinfo;         /* matching visual information */
    XVisualInfo *vpointer;      /* for freeing the entire list */
    long vinfo_mask;            /* specifies the visual mask value */
    int n;                      /* number of matching visuals */
    int status;

    vinfo_template.visualid = colormap->visualid;
    vinfo_mask = VisualIDMask;
    if ((vinfo =
         XGetVisualInfo(dpy, vinfo_mask, &vinfo_template, &n)) == NULL)
        return 0;

    /* A visual id may be valid on multiple screens.  Also, there may 
     * be multiple visuals with identical visual ids at different depths.  
     * If the colormap is the Default Colormap, use the Default Visual.
     * Otherwise, arbitrarily, use the deepest visual.
     */
    vpointer = vinfo;
    if (n > 1) {
        register int i;
        register int screen_number;
        Bool def_cmap;

        def_cmap = False;
        for (screen_number = ScreenCount(dpy); --screen_number >= 0;)
            if (colormap->colormap == DefaultColormap(dpy, screen_number)) {
                def_cmap = True;
                break;
            }

        if (def_cmap) {
            for (i = 0; i < n; i++, vinfo++) {
                if (vinfo->visual == DefaultVisual(dpy, screen_number))
                    break;
            }
        } else {
            int maxdepth = 0;
            XVisualInfo *v = NULL;

            for (i = 0; i < n; i++, vinfo++)
                if (vinfo->depth > maxdepth) {
                    maxdepth = vinfo->depth;
                    v = vinfo;
                }
            vinfo = v;
        }
    }

    if (vinfo->class == PseudoColor || vinfo->class == DirectColor ||
        vinfo->class == GrayScale)
        status = readwrite_map(dpy, vinfo, colormap);
    else if (vinfo->class == TrueColor)
        status = TRUEMATCH(red_mult, red_max, red_mask) &&
            TRUEMATCH(green_mult, green_max, green_mask) &&
            TRUEMATCH(blue_mult, blue_max, blue_mask);
    else
        status = readonly_map(dpy, vinfo, colormap);

    XFree((char *) vpointer);
    return status;
}

/****************************************************************************/
static Status
readwrite_map(Display * dpy, XVisualInfo * vinfo,
              XStandardColormap * colormap)
{
    register unsigned long i, n;        /* index counters */
    unsigned long ncolors;      /* number of colors to be defined */
    int npixels;                /* number of pixels allocated R/W */
    int first_index;            /* first index of pixels to use */
    int remainder;              /* first index of remainder */
    XColor color;               /* the definition of a color */
    unsigned long *pixels;      /* array of colormap pixels */
    unsigned long delta;


    /* Determine ncolors, the number of colors to be defined.
     * Insure that 1 < ncolors <= the colormap size.
     */
    if (vinfo->class == DirectColor) {
        ncolors = colormap->red_max;
        if (colormap->green_max > ncolors)
            ncolors = colormap->green_max;
        if (colormap->blue_max > ncolors)
            ncolors = colormap->blue_max;
        ncolors++;
        delta = lowbit(vinfo->red_mask) +
            lowbit(vinfo->green_mask) + lowbit(vinfo->blue_mask);
    } else {
        ncolors = colormap->red_max * colormap->red_mult +
            colormap->green_max * colormap->green_mult +
            colormap->blue_max * colormap->blue_mult + 1;
        delta = 1;
    }
    if (ncolors <= 1 || (int) ncolors > vinfo->colormap_size)
        return 0;

    /* Allocate Read/Write as much of the colormap as we can possibly get.
     * Then insure that the pixels we were allocated are given in 
     * monotonically increasing order, using a quicksort.  Next, insure
     * that our allocation includes a subset of contiguous pixels at least
     * as long as the number of colors to be defined.  Now we know that 
     * these conditions are met:
     *  1) There are no free cells in the colormap.
     *  2) We have a contiguous sequence of pixels, monotonically 
     *     increasing, of length >= the number of colors requested.
     *
     * One cell at a time, we will free, compute the next color value, 
     * then allocate read only.  This takes a long time.
     * This is done to insure that cells are allocated read only in the
     * contiguous order which we prefer.  If the server has a choice of
     * cells to grant to an allocation request, the server may give us any
     * cell, so that is why we do these slow gymnastics.
     */

    if ((pixels = (unsigned long *) calloc((unsigned) vinfo->colormap_size,
                                           sizeof(unsigned long))) == NULL)
        return 0;

    if ((npixels = ROmap(dpy, colormap->colormap, pixels,
                         vinfo->colormap_size, ncolors)) == 0) {
        free((char *) pixels);
        return 0;
    }

    qsort((char *) pixels, npixels, sizeof(unsigned long), compare);

    if (!contiguous
        (pixels, npixels, ncolors, delta, &first_index, &remainder)) {
        /* can't find enough contiguous cells, give up */
        XFreeColors(dpy, colormap->colormap, pixels, npixels,
                    (unsigned long) 0);
        free((char *) pixels);
        return 0;
    }
    colormap->base_pixel = pixels[first_index];

    /* construct a gray map */
    if (colormap->red_mult == 1 && colormap->green_mult == 1 &&
        colormap->blue_mult == 1)
        for (n = colormap->base_pixel, i = 0; i < ncolors; i++, n += delta) {
            color.pixel = n;
            color.blue = color.green = color.red =
                (unsigned short) ((i * 65535) / (colormap->red_max +
                                                 colormap->green_max +
                                                 colormap->blue_max));

            if (!ROorRWcell(dpy, colormap->colormap, pixels, npixels, &color,
                            first_index + i))
                return 0;
        }

    /* construct a red ramp map */
    else if (colormap->green_max == 0 && colormap->blue_max == 0)
        for (n = colormap->base_pixel, i = 0; i < ncolors; i++, n += delta) {
            color.pixel = n;
            color.red = (unsigned short) ((i * 65535) / colormap->red_max);
            color.green = color.blue = 0;

            if (!ROorRWcell(dpy, colormap->colormap, pixels, npixels, &color,
                            first_index + i))
                return 0;
        }

    /* construct a green ramp map */
    else if (colormap->red_max == 0 && colormap->blue_max == 0)
        for (n = colormap->base_pixel, i = 0; i < ncolors; i++, n += delta) {
            color.pixel = n;
            color.green =
                (unsigned short) ((i * 65535) / colormap->green_max);
            color.red = color.blue = 0;

            if (!ROorRWcell(dpy, colormap->colormap, pixels, npixels, &color,
                            first_index + i))
                return 0;
        }

    /* construct a blue ramp map */
    else if (colormap->red_max == 0 && colormap->green_max == 0)
        for (n = colormap->base_pixel, i = 0; i < ncolors; i++, n += delta) {
            color.pixel = n;
            color.blue = (unsigned short) ((i * 65535) / colormap->blue_max);
            color.red = color.green = 0;

            if (!ROorRWcell(dpy, colormap->colormap, pixels, npixels, &color,
                            first_index + i))
                return 0;
        }

    /* construct a standard red green blue cube map */
    else {
#define calc(max,mult) (((n / colormap->mult) % \
			 (colormap->max + 1)) * 65535) / colormap->max

        for (n = 0, i = 0; i < ncolors; i++, n += delta) {
            color.pixel = n + colormap->base_pixel;
            color.red = calc(red_max, red_mult);
            color.green = calc(green_max, green_mult);
            color.blue = calc(blue_max, blue_mult);
            if (!ROorRWcell(dpy, colormap->colormap, pixels, npixels, &color,
                            first_index + i))
                return 0;
        }
#undef calc
    }
    /* We have a read-only map defined.  Now free unused cells,
     * first those occuring before the contiguous sequence begins,
     * then any following the contiguous sequence.
     */

    if (first_index)
        XFreeColors(dpy, colormap->colormap, pixels, first_index,
                    (unsigned long) 0);
    if (remainder)
        XFreeColors(dpy, colormap->colormap,
                    &(pixels[first_index + ncolors]), remainder,
                    (unsigned long) 0);

    free((char *) pixels);
    return 1;
}


/****************************************************************************/
static int
ROmap(Display * dpy, Colormap cmap, unsigned long pixels[], int m, int n)
     /*
      * dpy     - the X server connection
      * cmap    - specifies colormap ID
      * pixels  - returns pixel allocations
      * m       - specifies colormap size
      * n       - specifies number of colors
      */
{
    register int p;

    /* first try to allocate the entire colormap */
    if (XAllocColorCells(dpy, cmap, 1, (unsigned long *) NULL,
                         (unsigned) 0, pixels, (unsigned) m))
        return m;

    /* Allocate all available cells in the colormap, using a binary
     * algorithm to discover how many cells we can allocate in the colormap.
     */
    m--;
    while (n <= m) {
        p = n + ((m - n + 1) / 2);
        if (XAllocColorCells(dpy, cmap, 1, (unsigned long *) NULL,
                             (unsigned) 0, pixels, (unsigned) p)) {
            if (p == m)
                return p;
            else {
                XFreeColors(dpy, cmap, pixels, p, (unsigned long) 0);
                n = p;
            }
        } else
            m = p - 1;
    }
    return 0;
}


/****************************************************************************/
static Status
contiguous(unsigned long pixels[], int npixels, int ncolors,
           unsigned long delta, int *first, int *rem)
     /* pixels  - specifies allocated pixels
      * npixels - specifies count of alloc'd pixels
      * ncolors - specifies needed sequence length
      * delta   - between pixels
      * first   - returns first index of sequence
      * rem     - returns first index after sequence, or 0, if none follow
      */
{
    register int i = 1;         /* walking index into the pixel array */
    register int count = 1;     /* length of sequence discovered so far */

    *first = 0;
    if (npixels == ncolors) {
        *rem = 0;
        return 1;
    }
    *rem = npixels - 1;
    while (count < ncolors && ncolors - count <= *rem) {
        if (pixels[i - 1] + delta == pixels[i])
            count++;
        else {
            count = 1;
            *first = i;
        }
        i++;
        (*rem)--;
    }
    if (count != ncolors)
        return 0;
    return 1;
}


/****************************************************************************/
static Status
ROorRWcell(Display * dpy, Colormap cmap, unsigned long pixels[],
           int npixels, XColor * color, unsigned long p)
{
    unsigned long pixel;
    XColor request;

    /* Free the read/write allocation of one cell in the colormap.
     * Request a read only allocation of one cell in the colormap.
     * If the read only allocation cannot be granted, give up, because
     * there must be no free cells in the colormap.
     * If the read only allocation is granted, but gives us a cell which
     * is not the one that we just freed, it is probably the case that
     * we are trying allocate White or Black or some other color which
     * already has a read-only allocation in the map.  So we try to 
     * allocate the previously freed cell with a read/write allocation,
     * because we want contiguous cells for image processing algorithms.
     */

    pixel = color->pixel;
    request.red = color->red;
    request.green = color->green;
    request.blue = color->blue;

    XFreeColors(dpy, cmap, &pixel, 1, (unsigned long) 0);
    if (!XAllocColor(dpy, cmap, color)
        || (color->pixel != pixel &&
            (!RWcell(dpy, cmap, color, &request, &pixel)))) {
        free_cells(dpy, cmap, pixels, npixels, (int) p);
        return 0;
    }
    return 1;
}


/****************************************************************************/
static void
free_cells(Display * dpy, Colormap cmap, unsigned long pixels[],
           int npixels, int p)
     /*
      * pixels  - to be freed
      * npixels - original number allocated
      */
{
    /* One of the npixels allocated has already been freed.
     * p is the index of the freed pixel.
     * First free the pixels preceeding p, and there are p of them;
     * then free the pixels following p, there are npixels - p - 1 of them.
     */
    XFreeColors(dpy, cmap, pixels, p, (unsigned long) 0);
    XFreeColors(dpy, cmap, &(pixels[p + 1]), npixels - p - 1,
                (unsigned long) 0);
    free((char *) pixels);
}


/****************************************************************************/
static Status
RWcell(Display * dpy, Colormap cmap, XColor * color, XColor * request,
       unsigned long *pixel)
{
    unsigned long n = *pixel;

    XFreeColors(dpy, cmap, &(color->pixel), 1, (unsigned long) 0);
    if (!XAllocColorCells(dpy, cmap, (Bool) 0, (unsigned long *) NULL,
                          (unsigned) 0, pixel, (unsigned) 1))
        return 0;
    if (*pixel != n) {
        XFreeColors(dpy, cmap, pixel, 1, (unsigned long) 0);
        return 0;
    }
    color->pixel = *pixel;
    color->flags = DoRed | DoGreen | DoBlue;
    color->red = request->red;
    color->green = request->green;
    color->blue = request->blue;
    XStoreColors(dpy, cmap, color, 1);
    return 1;
}


/****************************************************************************/
static int
compare(_Xconst void *e1, _Xconst void *e2)
{
    return ((int) (*(long *) e1 - *(long *) e2));
}


/****************************************************************************/
static Status
readonly_map(Display * dpy, XVisualInfo * vinfo, XStandardColormap * colormap)
{
    int i, last_pixel;
    XColor color;

    last_pixel = (colormap->red_max + 1) * (colormap->green_max + 1) *
        (colormap->blue_max + 1) + colormap->base_pixel - 1;

    for (i = colormap->base_pixel; i <= last_pixel; i++) {

        color.pixel = (unsigned long) i;
        color.red = (unsigned short)
            (((i / colormap->red_mult) * 65535) / colormap->red_max);

        if (vinfo->class == StaticColor) {
            color.green = (unsigned short)
                ((((i / colormap->green_mult) % (colormap->green_max + 1)) *
                  65535) / colormap->green_max);
            color.blue = (unsigned short)
                (((i % colormap->green_mult) * 65535) / colormap->blue_max);
        } else                  /* vinfo->class == GrayScale, old style allocation XXX */
            color.green = color.blue = color.red;

        XAllocColor(dpy, colormap->colormap, &color);
        if (color.pixel != (unsigned long) i)
            return 0;
    }
    return 1;
}
