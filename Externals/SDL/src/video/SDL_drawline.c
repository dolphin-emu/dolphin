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

#include "SDL_draw.h"


int
SDL_DrawLine(SDL_Surface * dst, int x1, int y1, int x2, int y2, Uint32 color)
{
    /* This function doesn't work on surfaces < 8 bpp */
    if (dst->format->BitsPerPixel < 8) {
        SDL_SetError("SDL_DrawLine(): Unsupported surface format");
        return (-1);
    }

    /* Perform clipping */
    if (!SDL_IntersectRectAndLine(&dst->clip_rect, &x1, &y1, &x2, &y2)) {
        return (0);
    }

    switch (dst->format->BytesPerPixel) {
    case 1:
        DRAWLINE(x1, y1, x2, y2, DRAW_FASTSETPIXEL1);
        break;
    case 2:
        DRAWLINE(x1, y1, x2, y2, DRAW_FASTSETPIXEL2);
        break;
    case 3:
        SDL_Unsupported();
        return -1;
    case 4:
        DRAWLINE(x1, y1, x2, y2, DRAW_FASTSETPIXEL4);
        break;
    }
    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */
