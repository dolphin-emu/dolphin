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
#include "SDL_x11video.h"

    /* The size of *all* SDL gamma ramps */
#define SDL_GammaRampSize (3 * 256 * sizeof(Uint16))

static int numCmaps = 0;

typedef struct
{
    Display *display;
    int scrNum;
    Colormap colormap;
    Visual visual;
    Uint16 *ramp;
} cmapTableEntry;

cmapTableEntry *cmapTable = NULL;

/* To reduce the overhead as much as possible lets do as little as
   possible. When we do have to create a colormap keep track of it and
   reuse it. We're going to do this for both DirectColor and
   PseudoColor colormaps. */

Colormap
X11_LookupColormap(Display * display, int scrNum, VisualID vid)
{
    int i;

    for (i = 0; i < numCmaps; i++) {
        if (cmapTable[i].display == display &&
            cmapTable[i].scrNum == scrNum &&
            cmapTable[i].visual.visualid == vid) {
            return cmapTable[i].colormap;
        }
    }

    return 0;
}


void
X11_TrackColormap(Display * display, int scrNum, Colormap colormap,
                  Visual * visual, XColor * ramp)
{
    int i;
    Uint16 *newramp;
    int ncolors;

    /* search the table to find out if we already have this one. We
       only want one entry for each display, screen number, visualid,
       and colormap combination */
    for (i = 0; i < numCmaps; i++) {
        if (cmapTable[i].display == display &&
            cmapTable[i].scrNum == scrNum &&
            cmapTable[i].visual.visualid == visual->visualid &&
            cmapTable[i].colormap == colormap) {
            return;
        }
    }

    /* increase the table by one entry. If the table is NULL create the
       first entrty */
    cmapTable =
        SDL_realloc(cmapTable, (numCmaps + 1) * sizeof(cmapTableEntry));
    if (NULL == cmapTable) {
        SDL_SetError("Out of memory in X11_TrackColormap()");
        return;
    }

    cmapTable[numCmaps].display = display;
    cmapTable[numCmaps].scrNum = scrNum;
    cmapTable[numCmaps].colormap = colormap;
    SDL_memcpy(&cmapTable[numCmaps].visual, visual, sizeof(Visual));
    cmapTable[numCmaps].ramp = NULL;

    if (ramp != NULL) {
        newramp = SDL_malloc(SDL_GammaRampSize);
        if (NULL == newramp) {
            SDL_SetError("Out of memory in X11_TrackColormap()");
            return;
        }
        SDL_memset(newramp, 0, SDL_GammaRampSize);
        cmapTable[numCmaps].ramp = newramp;

        ncolors = cmapTable[numCmaps].visual.map_entries;

        for (i = 0; i < ncolors; i++) {
            newramp[(0 * 256) + i] = ramp[i].red;
            newramp[(1 * 256) + i] = ramp[i].green;
            newramp[(2 * 256) + i] = ramp[i].blue;
        }
    }

    numCmaps++;
}

/* The problem is that you have to have at least one DirectColor
   colormap before you can set the gamma ramps or read the gamma
   ramps. If the application has created a DirectColor window then the
   cmapTable will have at least one colormap in it and everything is
   cool. If not, then we just fail  */

int
X11_SetDisplayGammaRamp(_THIS, Uint16 * ramp)
{
    Visual *visual;
    Display *display;
    Colormap colormap;
    XColor *colorcells;
    int ncolors;
    int rmask, gmask, bmask;
    int rshift, gshift, bshift;
    int i;
    int j;

    for (j = 0; j < numCmaps; j++) {
        if (cmapTable[j].visual.class == DirectColor) {
            display = cmapTable[j].display;
            colormap = cmapTable[j].colormap;
            ncolors = cmapTable[j].visual.map_entries;
            visual = &cmapTable[j].visual;

            colorcells = SDL_malloc(ncolors * sizeof(XColor));
            if (NULL == colorcells) {
                SDL_SetError("out of memory in X11_SetDisplayGammaRamp");
                return -1;
            }
            /* remember the new ramp */
            if (cmapTable[j].ramp == NULL) {
                Uint16 *newramp = SDL_malloc(SDL_GammaRampSize);
                if (NULL == newramp) {
                    SDL_SetError("Out of memory in X11_TrackColormap()");
                    return -1;
                }
                cmapTable[j].ramp = newramp;
            }
            SDL_memcpy(cmapTable[j].ramp, ramp, SDL_GammaRampSize);

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
                Uint32 rbits = (rmask * i) / (ncolors - 1);
                Uint32 gbits = (gmask * i) / (ncolors - 1);
                Uint32 bbits = (bmask * i) / (ncolors - 1);

                Uint32 pix =
                    (rbits << rshift) | (gbits << gshift) | (bbits << bshift);

                colorcells[i].pixel = pix;

                colorcells[i].flags = DoRed | DoGreen | DoBlue;

                colorcells[i].red = ramp[(0 * 256) + i];
                colorcells[i].green = ramp[(1 * 256) + i];
                colorcells[i].blue = ramp[(2 * 256) + i];
            }

            XStoreColors(display, colormap, colorcells, ncolors);
            XFlush(display);
            SDL_free(colorcells);
        }
    }

    return 0;
}

int
X11_GetDisplayGammaRamp(_THIS, Uint16 * ramp)
{
    int i;

    /* find the first DirectColor colormap and use it to get the gamma
       ramp */

    for (i = 0; i < numCmaps; i++) {
        if (cmapTable[i].visual.class == DirectColor) {
            SDL_memcpy(ramp, cmapTable[i].ramp, SDL_GammaRampSize);
            return 0;
        }
    }

    return -1;
}
