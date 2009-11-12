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

#include "SDL_blit.h"

/* This code assumes that r, g, b, a are the source color,
 * and in the blend and add case, the RGB values are premultiplied by a.
 */

#define DRAW_MUL(_a, _b) (((unsigned)(_a)*(_b))/255)

#define DRAW_FASTSETPIXEL(x, y, type, bpp, color) \
    *(type *)((Uint8 *)dst->pixels + y * dst->pitch + x * bpp) = (type) color

#define DRAW_FASTSETPIXEL1(x, y) DRAW_FASTSETPIXEL(x, y, Uint8, 1, color);
#define DRAW_FASTSETPIXEL2(x, y) DRAW_FASTSETPIXEL(x, y, Uint16, 2, color);
#define DRAW_FASTSETPIXEL4(x, y) DRAW_FASTSETPIXEL(x, y, Uint32, 4, color);

#define DRAW_SETPIXEL(setpixel) \
do { \
    unsigned sr = r, sg = g, sb = b, sa = a; \
    setpixel; \
} while (0)

#define DRAW_SETPIXEL_BLEND(getpixel, setpixel) \
do { \
    unsigned sr, sg, sb, sa; sa; \
    getpixel; \
    sr = DRAW_MUL(inva, sr) + r; \
    sg = DRAW_MUL(inva, sg) + g; \
    sb = DRAW_MUL(inva, sb) + b; \
    setpixel; \
} while (0)

#define DRAW_SETPIXEL_ADD(getpixel, setpixel) \
do { \
    unsigned sr, sg, sb, sa; sa; \
    getpixel; \
    sr += r; if (sr > 0xff) sr = 0xff; \
    sg += g; if (sg > 0xff) sg = 0xff; \
    sb += b; if (sb > 0xff) sb = 0xff; \
    setpixel; \
} while (0)

#define DRAW_SETPIXEL_MOD(getpixel, setpixel) \
do { \
    unsigned sr, sg, sb, sa; sa; \
    getpixel; \
    sr = DRAW_MUL(sr, r); \
    sg = DRAW_MUL(sg, g); \
    sb = DRAW_MUL(sb, b); \
    setpixel; \
} while (0)

#define DRAW_SETPIXELXY(x, y, type, bpp, op) \
do { \
    type *pixel = (type *)((Uint8 *)dst->pixels + y * dst->pitch + x * bpp); \
    op; \
} while (0)

/*
 * Define draw operators for RGB555
 */

#define DRAW_SETPIXEL_RGB555 \
    DRAW_SETPIXEL(RGB555_FROM_RGB(*pixel, sr, sg, sb))

#define DRAW_SETPIXEL_BLEND_RGB555 \
    DRAW_SETPIXEL_BLEND(RGB_FROM_RGB555(*pixel, sr, sg, sb), \
                        RGB555_FROM_RGB(*pixel, sr, sg, sb))

#define DRAW_SETPIXEL_ADD_RGB555 \
    DRAW_SETPIXEL_ADD(RGB_FROM_RGB555(*pixel, sr, sg, sb), \
                      RGB555_FROM_RGB(*pixel, sr, sg, sb))

#define DRAW_SETPIXEL_MOD_RGB555 \
    DRAW_SETPIXEL_MOD(RGB_FROM_RGB555(*pixel, sr, sg, sb), \
                      RGB555_FROM_RGB(*pixel, sr, sg, sb))

#define DRAW_SETPIXELXY_RGB555(x, y) \
    DRAW_SETPIXELXY(x, y, Uint16, 2, DRAW_SETPIXEL_RGB555)

#define DRAW_SETPIXELXY_BLEND_RGB555(x, y) \
    DRAW_SETPIXELXY(x, y, Uint16, 2, DRAW_SETPIXEL_BLEND_RGB555)

#define DRAW_SETPIXELXY_ADD_RGB555(x, y) \
    DRAW_SETPIXELXY(x, y, Uint16, 2, DRAW_SETPIXEL_ADD_RGB555)

#define DRAW_SETPIXELXY_MOD_RGB555(x, y) \
    DRAW_SETPIXELXY(x, y, Uint16, 2, DRAW_SETPIXEL_MOD_RGB555)

/*
 * Define draw operators for RGB565
 */

#define DRAW_SETPIXEL_RGB565 \
    DRAW_SETPIXEL(RGB565_FROM_RGB(*pixel, sr, sg, sb))

#define DRAW_SETPIXEL_BLEND_RGB565 \
    DRAW_SETPIXEL_BLEND(RGB_FROM_RGB565(*pixel, sr, sg, sb), \
                        RGB565_FROM_RGB(*pixel, sr, sg, sb))

#define DRAW_SETPIXEL_ADD_RGB565 \
    DRAW_SETPIXEL_ADD(RGB_FROM_RGB565(*pixel, sr, sg, sb), \
                      RGB565_FROM_RGB(*pixel, sr, sg, sb))

#define DRAW_SETPIXEL_MOD_RGB565 \
    DRAW_SETPIXEL_MOD(RGB_FROM_RGB565(*pixel, sr, sg, sb), \
                      RGB565_FROM_RGB(*pixel, sr, sg, sb))

#define DRAW_SETPIXELXY_RGB565(x, y) \
    DRAW_SETPIXELXY(x, y, Uint16, 2, DRAW_SETPIXEL_RGB565)

#define DRAW_SETPIXELXY_BLEND_RGB565(x, y) \
    DRAW_SETPIXELXY(x, y, Uint16, 2, DRAW_SETPIXEL_BLEND_RGB565)

#define DRAW_SETPIXELXY_ADD_RGB565(x, y) \
    DRAW_SETPIXELXY(x, y, Uint16, 2, DRAW_SETPIXEL_ADD_RGB565)

#define DRAW_SETPIXELXY_MOD_RGB565(x, y) \
    DRAW_SETPIXELXY(x, y, Uint16, 2, DRAW_SETPIXEL_MOD_RGB565)

/*
 * Define draw operators for RGB888
 */

#define DRAW_SETPIXEL_RGB888 \
    DRAW_SETPIXEL(RGB888_FROM_RGB(*pixel, sr, sg, sb))

#define DRAW_SETPIXEL_BLEND_RGB888 \
    DRAW_SETPIXEL_BLEND(RGB_FROM_RGB888(*pixel, sr, sg, sb), \
                        RGB888_FROM_RGB(*pixel, sr, sg, sb))

#define DRAW_SETPIXEL_ADD_RGB888 \
    DRAW_SETPIXEL_ADD(RGB_FROM_RGB888(*pixel, sr, sg, sb), \
                      RGB888_FROM_RGB(*pixel, sr, sg, sb))

#define DRAW_SETPIXEL_MOD_RGB888 \
    DRAW_SETPIXEL_MOD(RGB_FROM_RGB888(*pixel, sr, sg, sb), \
                      RGB888_FROM_RGB(*pixel, sr, sg, sb))

#define DRAW_SETPIXELXY_RGB888(x, y) \
    DRAW_SETPIXELXY(x, y, Uint32, 4, DRAW_SETPIXEL_RGB888)

#define DRAW_SETPIXELXY_BLEND_RGB888(x, y) \
    DRAW_SETPIXELXY(x, y, Uint32, 4, DRAW_SETPIXEL_BLEND_RGB888)

#define DRAW_SETPIXELXY_ADD_RGB888(x, y) \
    DRAW_SETPIXELXY(x, y, Uint32, 4, DRAW_SETPIXEL_ADD_RGB888)

#define DRAW_SETPIXELXY_MOD_RGB888(x, y) \
    DRAW_SETPIXELXY(x, y, Uint32, 4, DRAW_SETPIXEL_MOD_RGB888)

/*
 * Define draw operators for ARGB8888
 */

#define DRAW_SETPIXEL_ARGB8888 \
    DRAW_SETPIXEL(ARGB8888_FROM_RGBA(*pixel, sr, sg, sb, sa))

#define DRAW_SETPIXEL_BLEND_ARGB8888 \
    DRAW_SETPIXEL_BLEND(RGBA_FROM_ARGB8888(*pixel, sr, sg, sb, sa), \
                        ARGB8888_FROM_RGBA(*pixel, sr, sg, sb, sa))

#define DRAW_SETPIXEL_ADD_ARGB8888 \
    DRAW_SETPIXEL_ADD(RGBA_FROM_ARGB8888(*pixel, sr, sg, sb, sa), \
                      ARGB8888_FROM_RGBA(*pixel, sr, sg, sb, sa))

#define DRAW_SETPIXEL_MOD_ARGB8888 \
    DRAW_SETPIXEL_MOD(RGBA_FROM_ARGB8888(*pixel, sr, sg, sb, sa), \
                      ARGB8888_FROM_RGBA(*pixel, sr, sg, sb, sa))

#define DRAW_SETPIXELXY_ARGB8888(x, y) \
    DRAW_SETPIXELXY(x, y, Uint32, 4, DRAW_SETPIXEL_ARGB8888)

#define DRAW_SETPIXELXY_BLEND_ARGB8888(x, y) \
    DRAW_SETPIXELXY(x, y, Uint32, 4, DRAW_SETPIXEL_BLEND_ARGB8888)

#define DRAW_SETPIXELXY_ADD_ARGB8888(x, y) \
    DRAW_SETPIXELXY(x, y, Uint32, 4, DRAW_SETPIXEL_ADD_ARGB8888)

#define DRAW_SETPIXELXY_MOD_ARGB8888(x, y) \
    DRAW_SETPIXELXY(x, y, Uint32, 4, DRAW_SETPIXEL_MOD_ARGB8888)

/*
 * Define draw operators for general RGB
 */

#define DRAW_SETPIXEL_RGB \
    DRAW_SETPIXEL(PIXEL_FROM_RGB(*pixel, fmt, sr, sg, sb))

#define DRAW_SETPIXEL_BLEND_RGB \
    DRAW_SETPIXEL_BLEND(RGB_FROM_PIXEL(*pixel, fmt, sr, sg, sb), \
                        PIXEL_FROM_RGB(*pixel, fmt, sr, sg, sb))

#define DRAW_SETPIXEL_ADD_RGB \
    DRAW_SETPIXEL_ADD(RGB_FROM_PIXEL(*pixel, fmt, sr, sg, sb), \
                      PIXEL_FROM_RGB(*pixel, fmt, sr, sg, sb))

#define DRAW_SETPIXEL_MOD_RGB \
    DRAW_SETPIXEL_MOD(RGB_FROM_PIXEL(*pixel, fmt, sr, sg, sb), \
                      PIXEL_FROM_RGB(*pixel, fmt, sr, sg, sb))

#define DRAW_SETPIXELXY2_RGB(x, y) \
    DRAW_SETPIXELXY(x, y, Uint16, 2, DRAW_SETPIXEL_RGB)

#define DRAW_SETPIXELXY4_RGB(x, y) \
    DRAW_SETPIXELXY(x, y, Uint32, 4, DRAW_SETPIXEL_RGB)

#define DRAW_SETPIXELXY2_BLEND_RGB(x, y) \
    DRAW_SETPIXELXY(x, y, Uint16, 2, DRAW_SETPIXEL_BLEND_RGB)

#define DRAW_SETPIXELXY4_BLEND_RGB(x, y) \
    DRAW_SETPIXELXY(x, y, Uint32, 4, DRAW_SETPIXEL_BLEND_RGB)

#define DRAW_SETPIXELXY2_ADD_RGB(x, y) \
    DRAW_SETPIXELXY(x, y, Uint16, 2, DRAW_SETPIXEL_ADD_RGB)

#define DRAW_SETPIXELXY4_ADD_RGB(x, y) \
    DRAW_SETPIXELXY(x, y, Uint32, 4, DRAW_SETPIXEL_ADD_RGB)

#define DRAW_SETPIXELXY2_MOD_RGB(x, y) \
    DRAW_SETPIXELXY(x, y, Uint16, 2, DRAW_SETPIXEL_MOD_RGB)

#define DRAW_SETPIXELXY4_MOD_RGB(x, y) \
    DRAW_SETPIXELXY(x, y, Uint32, 4, DRAW_SETPIXEL_MOD_RGB)


/*
 * Define draw operators for general RGBA
 */

#define DRAW_SETPIXEL_RGBA \
    DRAW_SETPIXEL(PIXEL_FROM_RGBA(*pixel, fmt, sr, sg, sb, sa))

#define DRAW_SETPIXEL_BLEND_RGBA \
    DRAW_SETPIXEL_BLEND(RGBA_FROM_PIXEL(*pixel, fmt, sr, sg, sb, sa), \
                        PIXEL_FROM_RGBA(*pixel, fmt, sr, sg, sb, sa))

#define DRAW_SETPIXEL_ADD_RGBA \
    DRAW_SETPIXEL_ADD(RGBA_FROM_PIXEL(*pixel, fmt, sr, sg, sb, sa), \
                      PIXEL_FROM_RGBA(*pixel, fmt, sr, sg, sb, sa))

#define DRAW_SETPIXEL_MOD_RGBA \
    DRAW_SETPIXEL_MOD(RGBA_FROM_PIXEL(*pixel, fmt, sr, sg, sb, sa), \
                      PIXEL_FROM_RGBA(*pixel, fmt, sr, sg, sb, sa))

#define DRAW_SETPIXELXY4_RGBA(x, y) \
    DRAW_SETPIXELXY(x, y, Uint32, 4, DRAW_SETPIXEL_RGBA)

#define DRAW_SETPIXELXY4_BLEND_RGBA(x, y) \
    DRAW_SETPIXELXY(x, y, Uint32, 4, DRAW_SETPIXEL_BLEND_RGBA)

#define DRAW_SETPIXELXY4_ADD_RGBA(x, y) \
    DRAW_SETPIXELXY(x, y, Uint32, 4, DRAW_SETPIXEL_ADD_RGBA)

#define DRAW_SETPIXELXY4_MOD_RGBA(x, y) \
    DRAW_SETPIXELXY(x, y, Uint32, 4, DRAW_SETPIXEL_MOD_RGBA)

/*
 * Define line drawing macro
 */

#define ABS(_x) ((_x) < 0 ? -(_x) : (_x))

#define BRESENHAM(x1, y1, x2, y2, op) \
{ \
    int i, deltax, deltay, numpixels; \
    int d, dinc1, dinc2; \
    int x, xinc1, xinc2; \
    int y, yinc1, yinc2; \
 \
    deltax = ABS(x2 - x1); \
    deltay = ABS(y2 - y1); \
 \
    if (deltax >= deltay) { \
        numpixels = deltax + 1; \
        d = (2 * deltay) - deltax; \
        dinc1 = deltay * 2; \
        dinc2 = (deltay - deltax) * 2; \
        xinc1 = 1; \
        xinc2 = 1; \
        yinc1 = 0; \
        yinc2 = 1; \
    } else { \
        numpixels = deltay + 1; \
        d = (2 * deltax) - deltay; \
        dinc1 = deltax * 2; \
        dinc2 = (deltax - deltay) * 2; \
        xinc1 = 0; \
        xinc2 = 1; \
        yinc1 = 1; \
        yinc2 = 1; \
    } \
 \
    if (x1 > x2) { \
        xinc1 = -xinc1; \
        xinc2 = -xinc2; \
    } \
    if (y1 > y2) { \
        yinc1 = -yinc1; \
        yinc2 = -yinc2; \
    } \
 \
    x = x1; \
    y = y1; \
 \
    for (i = 0; i < numpixels; ++i) { \
        op(x, y); \
        if (d < 0) { \
            d += dinc1; \
            x += xinc1; \
            y += yinc1; \
        } else { \
            d += dinc2; \
            x += xinc2; \
            y += yinc2; \
        } \
    } \
}
#define DRAWLINE(x0, y0, x1, y1, op)	BRESENHAM(x0, y0, x1, y1, op)

/*
 * Define blend fill macro
 */

#define FILLRECT(type, op) \
do { \
    int width = dstrect->w; \
    int height = dstrect->h; \
    int pitch = (dst->pitch / dst->format->BytesPerPixel); \
    int skip = pitch - width; \
    type *pixel = (type *)dst->pixels + dstrect->y * pitch + dstrect->x; \
    while (height--) { \
        { int n = (width+3)/4; \
            switch (width & 3) { \
            case 0: do {   op; pixel++; \
            case 3:        op; pixel++; \
            case 2:        op; pixel++; \
            case 1:        op; pixel++; \
                    } while ( --n > 0 ); \
            } \
        } \
        pixel += skip; \
    } \
} while (0)

/*
 * Define blend line macro
 */

/* vi: set ts=4 sw=4 expandtab: */
