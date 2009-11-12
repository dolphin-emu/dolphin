/* $Xorg: CmapAlloc.c,v 1.4 2001/02/09 02:03:51 xorgcvs Exp $ */

/* 

Copyright 1989, 1994, 1998  The Open Group

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
/* $XFree86: xc/lib/Xmu/CmapAlloc.c,v 1.6 2001/01/17 19:42:53 dawes Exp $ */

/*
 * Author:  Donna Converse, MIT X Consortium
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include "../extensions/StdCmap.h"
#include <stdio.h>

#define lowbit(x) ((x) & (~(x) + 1))

/*
 * Prototypes
 */
static void best_allocation(XVisualInfo *, unsigned long *, unsigned long *,
                            unsigned long *);
static int default_allocation(XVisualInfo *, unsigned long *,
                              unsigned long *, unsigned long *);
static void gray_allocation(int, unsigned long *, unsigned long *,
                            unsigned long *);
static int icbrt(int);
static int icbrt_with_bits(int, int);
static int icbrt_with_guess(int, int);

/* To determine the best allocation of reds, greens, and blues in a 
 * standard colormap, use XmuGetColormapAllocation.
 * 	vinfo		specifies visual information for a chosen visual
 *	property	specifies one of the standard colormap property names
 * 	red_max		returns maximum red value 
 *      green_max	returns maximum green value
 * 	blue_max	returns maximum blue value
 *
 * XmuGetColormapAllocation returns 0 on failure, non-zero on success.
 * It is assumed that the visual is appropriate for the colormap property.
 */

Status
XmuGetColormapAllocation(XVisualInfo * vinfo, Atom property,
                         unsigned long *red_max,
                         unsigned long *green_max, unsigned long *blue_max)
{
    Status status = 1;

    if (vinfo->colormap_size <= 2)
        return 0;

    switch (property) {
    case XA_RGB_DEFAULT_MAP:
        status = default_allocation(vinfo, red_max, green_max, blue_max);
        break;
    case XA_RGB_BEST_MAP:
        best_allocation(vinfo, red_max, green_max, blue_max);
        break;
    case XA_RGB_GRAY_MAP:
        gray_allocation(vinfo->colormap_size, red_max, green_max, blue_max);
        break;
    case XA_RGB_RED_MAP:
        *red_max = vinfo->colormap_size - 1;
        *green_max = *blue_max = 0;
        break;
    case XA_RGB_GREEN_MAP:
        *green_max = vinfo->colormap_size - 1;
        *red_max = *blue_max = 0;
        break;
    case XA_RGB_BLUE_MAP:
        *blue_max = vinfo->colormap_size - 1;
        *red_max = *green_max = 0;
        break;
    default:
        status = 0;
    }
    return status;
}

/****************************************************************************/
/* Determine the appropriate color allocations of a gray scale.
 *
 * Keith Packard, MIT X Consortium
 */

static void
gray_allocation(int n, unsigned long *red_max, unsigned long *green_max,
                unsigned long *blue_max)
{
    *red_max = (n * 30) / 100;
    *green_max = (n * 59) / 100;
    *blue_max = (n * 11) / 100;
    *green_max += ((n - 1) - (*red_max + *green_max + *blue_max));
}

/****************************************************************************/
/* Determine an appropriate color allocation for the RGB_DEFAULT_MAP.
 * If a map has less than a minimum number of definable entries, we do not
 * produce an allocation for an RGB_DEFAULT_MAP.  
 *
 * For 16 planes, the default colormap will have 27 each RGB; for 12 planes,
 * 12 each.  For 8 planes, let n = the number of colormap entries, which may
 * be 256 or 254.  Then, maximum red value = floor(cube_root(n - 125)) - 1.
 * Maximum green and maximum blue values are identical to maximum red.
 * This leaves at least 125 cells which clients can allocate.
 *
 * Return 0 if an allocation has been determined, non-zero otherwise.
 */

static int
default_allocation(XVisualInfo * vinfo, unsigned long *red,
                   unsigned long *green, unsigned long *blue)
{
    int ngrays;                 /* number of gray cells */

    switch (vinfo->class) {
    case PseudoColor:

        if (vinfo->colormap_size > 65000)
            /* intended for displays with 16 planes */
            *red = *green = *blue = (unsigned long) 27;
        else if (vinfo->colormap_size > 4000)
            /* intended for displays with 12 planes */
            *red = *green = *blue = (unsigned long) 12;
        else if (vinfo->colormap_size < 250)
            return 0;
        else
            /* intended for displays with 8 planes */
            *red = *green = *blue = (unsigned long)
                (icbrt(vinfo->colormap_size - 125) - 1);
        break;

    case DirectColor:

        if (vinfo->colormap_size < 10)
            return 0;
        *red = *green = *blue = vinfo->colormap_size / 2 - 1;
        break;

    case TrueColor:

        *red = vinfo->red_mask / lowbit(vinfo->red_mask);
        *green = vinfo->green_mask / lowbit(vinfo->green_mask);
        *blue = vinfo->blue_mask / lowbit(vinfo->blue_mask);
        break;

    case GrayScale:

        if (vinfo->colormap_size > 65000)
            ngrays = 4096;
        else if (vinfo->colormap_size > 4000)
            ngrays = 512;
        else if (vinfo->colormap_size < 250)
            return 0;
        else
            ngrays = 12;
        gray_allocation(ngrays, red, green, blue);
        break;

    default:
        return 0;
    }
    return 1;
}

/****************************************************************************/
/* Determine an appropriate color allocation for the RGB_BEST_MAP.
 *
 * For a DirectColor or TrueColor visual, the allocation is determined
 * by the red_mask, green_mask, and blue_mask members of the visual info.
 *
 * Otherwise, if the colormap size is an integral power of 2, determine
 * the allocation according to the number of bits given to each color,
 * with green getting more than red, and red more than blue, if there
 * are to be inequities in the distribution.  If the colormap size is
 * not an integral power of 2, let n = the number of colormap entries.
 * Then maximum red value = floor(cube_root(n)) - 1;
 * 	maximum blue value = floor(cube_root(n)) - 1;
 *	maximum green value = n / ((# red values) * (# blue values)) - 1;
 * Which, on a GPX, allows for 252 entries in the best map, out of 254
 * defineable colormap entries.
 */

static void
best_allocation(XVisualInfo * vinfo, unsigned long *red, unsigned long *green,
                unsigned long *blue)
{

    if (vinfo->class == DirectColor || vinfo->class == TrueColor) {
        *red = vinfo->red_mask;
        while ((*red & 01) == 0)
            *red >>= 1;
        *green = vinfo->green_mask;
        while ((*green & 01) == 0)
            *green >>= 1;
        *blue = vinfo->blue_mask;
        while ((*blue & 01) == 0)
            *blue >>= 1;
    } else {
        register int bits, n;

        /* Determine n such that n is the least integral power of 2 which is
         * greater than or equal to the number of entries in the colormap.
         */
        n = 1;
        bits = 0;
        while (vinfo->colormap_size > n) {
            n = n << 1;
            bits++;
        }

        /* If the number of entries in the colormap is a power of 2, determine
         * the allocation by "dealing" the bits, first to green, then red, then
         * blue.  If not, find the maximum integral red, green, and blue values
         * which, when multiplied together, do not exceed the number of 

         * colormap entries.
         */
        if (n == vinfo->colormap_size) {
            register int r, g, b;
            b = bits / 3;
            g = b + ((bits % 3) ? 1 : 0);
            r = b + (((bits % 3) == 2) ? 1 : 0);
            *red = 1 << r;
            *green = 1 << g;
            *blue = 1 << b;
        } else {
            *red = icbrt_with_bits(vinfo->colormap_size, bits);
            *blue = *red;
            *green = (vinfo->colormap_size / ((*red) * (*blue)));
        }
        (*red)--;
        (*green)--;
        (*blue)--;
    }
    return;
}

/*
 * integer cube roots by Newton's method
 *
 * Stephen Gildea, MIT X Consortium, July 1991
 */

static int
icbrt(int a)
{
    register int bits = 0;
    register unsigned n = a;

    while (n) {
        bits++;
        n >>= 1;
    }
    return icbrt_with_bits(a, bits);
}


static int
icbrt_with_bits(int a, int bits)
     /* bits - log 2 of a */
{
    return icbrt_with_guess(a, a >> 2 * bits / 3);
}

#ifdef _X_ROOT_STATS
int icbrt_loopcount;
#endif

/* Newton's Method:  x_n+1 = x_n - ( f(x_n) / f'(x_n) ) */

/* for cube roots, x^3 - a = 0,  x_new = x - 1/3 (x - a/x^2) */

/*
 * Quick and dirty cube roots.  Nothing fancy here, just Newton's method.
 * Only works for positive integers (since that's all we need).
 * We actually return floor(cbrt(a)) because that's what we need here, too.
 */

static int
icbrt_with_guess(int a, int guess)
{
    register int delta;

#ifdef _X_ROOT_STATS
    icbrt_loopcount = 0;
#endif
    if (a <= 0)
        return 0;
    if (guess < 1)
        guess = 1;

    do {
#ifdef _X_ROOT_STATS
        icbrt_loopcount++;
#endif
        delta = (guess - a / (guess * guess)) / 3;
#ifdef DEBUG
        printf("pass %d: guess=%d, delta=%d\n", icbrt_loopcount, guess,
               delta);
#endif
        guess -= delta;
    } while (delta != 0);

    if (guess * guess * guess > a)
        guess--;

    return guess;
}
