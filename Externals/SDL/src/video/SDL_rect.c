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

#include "SDL_video.h"
#include "SDL_rect_c.h"

SDL_bool
SDL_HasIntersection(const SDL_Rect * A, const SDL_Rect * B)
{
    int Amin, Amax, Bmin, Bmax;

    /* Horizontal intersection */
    Amin = A->x;
    Amax = Amin + A->w;
    Bmin = B->x;
    Bmax = Bmin + B->w;
    if (Bmin > Amin)
        Amin = Bmin;
    if (Bmax < Amax)
        Amax = Bmax;
    if (Amax <= Amin)
        return SDL_FALSE;

    /* Vertical intersection */
    Amin = A->y;
    Amax = Amin + A->h;
    Bmin = B->y;
    Bmax = Bmin + B->h;
    if (Bmin > Amin)
        Amin = Bmin;
    if (Bmax < Amax)
        Amax = Bmax;
    if (Amax <= Amin)
        return SDL_FALSE;

    return SDL_TRUE;
}

SDL_bool
SDL_IntersectRect(const SDL_Rect * A, const SDL_Rect * B, SDL_Rect * result)
{
    int Amin, Amax, Bmin, Bmax;

    /* Horizontal intersection */
    Amin = A->x;
    Amax = Amin + A->w;
    Bmin = B->x;
    Bmax = Bmin + B->w;
    if (Bmin > Amin)
        Amin = Bmin;
    result->x = Amin;
    if (Bmax < Amax)
        Amax = Bmax;
    result->w = Amax - Amin;

    /* Vertical intersection */
    Amin = A->y;
    Amax = Amin + A->h;
    Bmin = B->y;
    Bmax = Bmin + B->h;
    if (Bmin > Amin)
        Amin = Bmin;
    result->y = Amin;
    if (Bmax < Amax)
        Amax = Bmax;
    result->h = Amax - Amin;

    return !SDL_RectEmpty(result);
}

void
SDL_UnionRect(const SDL_Rect * A, const SDL_Rect * B, SDL_Rect * result)
{
    int Amin, Amax, Bmin, Bmax;

    /* Horizontal union */
    Amin = A->x;
    Amax = Amin + A->w;
    Bmin = B->x;
    Bmax = Bmin + B->w;
    if (Bmin < Amin)
        Amin = Bmin;
    result->x = Amin;
    if (Bmax > Amax)
        Amax = Bmax;
    result->w = Amax - Amin;

    /* Vertical intersection */
    Amin = A->y;
    Amax = Amin + A->h;
    Bmin = B->y;
    Bmax = Bmin + B->h;
    if (Bmin < Amin)
        Amin = Bmin;
    result->y = Amin;
    if (Bmax > Amax)
        Amax = Bmax;
    result->h = Amax - Amin;
}

SDL_bool
SDL_IntersectRectAndLine(const SDL_Rect * rect, int *X1, int *Y1, int *X2,
                         int *Y2)
{
    int x1, y1;
    int x2, y2;
    int rectx1;
    int recty1;
    int rectx2;
    int recty2;

    if (!rect || !X1 || !Y1 || !X2 || !Y2) {
        return SDL_FALSE;
    }

    x1 = *X1;
    y1 = *Y1;
    x2 = *X2;
    y2 = *Y2;
    rectx1 = rect->x;
    recty1 = rect->y;
    rectx2 = rect->x + rect->w - 1;
    recty2 = rect->y + rect->h - 1;

    /* Check to see if entire line is inside rect */
    if (x1 >= rectx1 && x1 <= rectx2 && x2 >= rectx1 && x2 <= rectx2 &&
        y1 >= recty1 && y1 <= recty2 && y2 >= recty1 && y2 <= recty2) {
        return SDL_TRUE;
    }

    /* Check to see if entire line is to one side of rect */
    if ((x1 < rectx1 && x2 < rectx1) || (x1 > rectx2 && x2 > rectx2) ||
        (y1 < recty1 && y2 < recty1) || (y1 > recty2 && y2 > recty2)) {
        return SDL_FALSE;
    }

    if (y1 == y2) {
        /* Horizontal line, easy to clip */
        if (x1 < rectx1) {
            *X1 = rectx1;
        } else if (x1 > rectx2) {
            *X1 = rectx2;
        }
        if (x2 < rectx1) {
            *X2 = rectx1;
        } else if (x2 > rectx2) {
            *X2 = rectx2;
        }
        return SDL_TRUE;
    }

    if (x1 == x2) {
        /* Vertical line, easy to clip */
        if (y1 < recty1) {
            *Y1 = recty1;
        } else if (y1 > recty2) {
            *Y1 = recty2;
        }
        if (y2 < recty1) {
            *Y2 = recty1;
        } else if (y2 > recty2) {
            *Y2 = recty2;
        }
        return SDL_TRUE;
    }

    else {
        /* The task of clipping a line with finite slope ratios in a fixed-
         * precision coordinate space is not as immediately simple as it is
         * with coordinates of arbitrary precision. If the ratio of slopes
         * between the input line segment and the result line segment is not
         * a whole number, you have in fact *moved* the line segment a bit,
         * and there can be no avoiding it without more precision
         */
        int *x_result_[] = { X1, X2, NULL }, **x_result = x_result_;
        int *y_result_[] = { Y1, Y2, NULL }, **y_result = y_result_;
        SDL_bool intersection = SDL_FALSE;
        double b, m, left, right, bottom, top;
        int xl, xh, yl, yh;

        /* solve mx+b line formula */
        m = (double) (y1 - y2) / (double) (x1 - x2);
        b = y2 - m * (double) x2;

        /* find some linear intersections */
        left = (m * (double) rectx1) + b;
        right = (m * (double) rectx2) + b;
        top = (recty1 - b) / m;
        bottom = (recty2 - b) / m;

        /* sort end-points' x and y components individually */
        if (x1 < x2) {
            xl = x1;
            xh = x2;
        } else {
            xl = x2;
            xh = x1;
        }
        if (y1 < y2) {
            yl = y1;
            yh = y2;
        } else {
            yl = y2;
            yh = y1;
        }

#define RISING(a, b, c) (((a)<=(b))&&((b)<=(c)))

        /* check for a point that's entirely inside the rect */
        if (RISING(rectx1, x1, rectx2) && RISING(recty1, y1, recty2)) {
            x_result++;
            y_result++;
            intersection = SDL_TRUE;
        } else
            /* it was determined earlier that *both* end-points are not contained */
        if (RISING(rectx1, x2, rectx2) && RISING(recty1, y2, recty2)) {
            **(x_result++) = x2;
            **(y_result++) = y2;
            intersection = SDL_TRUE;
        }

        if (RISING(recty1, left, recty2) && RISING(xl, rectx1, xh)) {
            **(x_result++) = rectx1;
            **(y_result++) = (int) left;
            intersection = SDL_TRUE;
        }

        if (*x_result == NULL)
            return intersection;
        if (RISING(recty1, right, recty2) && RISING(xl, rectx2, xh)) {
            **(x_result++) = rectx2;
            **(y_result++) = (int) right;
            intersection = SDL_TRUE;
        }

        if (*x_result == NULL)
            return intersection;
        if (RISING(rectx1, top, rectx2) && RISING(yl, recty1, yh)) {
            **(x_result++) = (int) top;
            **(y_result++) = recty1;
            intersection = SDL_TRUE;
        }

        if (*x_result == NULL)
            return intersection;
        if (RISING(rectx1, bottom, rectx2) && RISING(yl, recty2, yh)) {
            **(x_result++) = (int) bottom;
            **(y_result++) = recty2;
            intersection = SDL_TRUE;
        }

        return intersection;
    }

    return SDL_FALSE;
}

void
SDL_AddDirtyRect(SDL_DirtyRectList * list, const SDL_Rect * rect)
{
    SDL_DirtyRect *dirty;

    /* FIXME: At what point is this optimization too expensive? */
    for (dirty = list->list; dirty; dirty = dirty->next) {
        if (SDL_HasIntersection(&dirty->rect, rect)) {
            SDL_UnionRect(&dirty->rect, rect, &dirty->rect);
            return;
        }
    }

    if (list->free) {
        dirty = list->free;
        list->free = dirty->next;
    } else {
        dirty = (SDL_DirtyRect *) SDL_malloc(sizeof(*dirty));
        if (!dirty) {
            return;
        }
    }
    dirty->rect = *rect;
    dirty->next = list->list;
    list->list = dirty;
}

void
SDL_ClearDirtyRects(SDL_DirtyRectList * list)
{
    SDL_DirtyRect *prev, *curr;

    /* Skip to the end of the free list */
    prev = NULL;
    for (curr = list->free; curr; curr = curr->next) {
        prev = curr;
    }

    /* Add the list entries to the end */
    if (prev) {
        prev->next = list->list;
    } else {
        list->free = list->list;
    }
    list->list = NULL;
}

void
SDL_FreeDirtyRects(SDL_DirtyRectList * list)
{
    while (list->list) {
        SDL_DirtyRect *elem = list->list;
        list->list = elem->next;
        SDL_free(elem);
    }
    while (list->free) {
        SDL_DirtyRect *elem = list->free;
        list->free = elem->next;
        SDL_free(elem);
    }
}

/* vi: set ts=4 sw=4 expandtab: */
