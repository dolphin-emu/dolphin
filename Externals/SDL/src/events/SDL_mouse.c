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

/* General mouse handling code for SDL */

#include "SDL_events.h"
#include "SDL_events_c.h"
#include "default_cursor.h"


static int SDL_num_mice = 0;
static int SDL_current_mouse = -1;
static SDL_Mouse **SDL_mice = NULL;


/* Public functions */
int
SDL_MouseInit(void)
{
    return (0);
}

SDL_Mouse *
SDL_GetMouse(int index)
{
    if (index < 0 || index >= SDL_num_mice) {
        return NULL;
    }
    return SDL_mice[index];
}

static int
SDL_GetMouseIndexId(int id)
{
    int index;
    SDL_Mouse *mouse;

    for (index = 0; index < SDL_num_mice; ++index) {
        mouse = SDL_GetMouse(index);
        if (mouse->id == id) {
            return index;
        }
    }
    return -1;
}

int
SDL_AddMouse(const SDL_Mouse * mouse, char *name, int pressure_max,
             int pressure_min, int ends)
{
    SDL_Mouse **mice;
    int selected_mouse;
    int index;
    size_t length;

    if (SDL_GetMouseIndexId(mouse->id) != -1) {
        SDL_SetError("Mouse ID already in use");
    }

    /* Add the mouse to the list of mice */
    mice = (SDL_Mouse **) SDL_realloc(SDL_mice,
                                      (SDL_num_mice + 1) * sizeof(*mice));
    if (!mice) {
        SDL_OutOfMemory();
        return -1;
    }

    SDL_mice = mice;
    index = SDL_num_mice++;

    SDL_mice[index] = (SDL_Mouse *) SDL_malloc(sizeof(*SDL_mice[index]));
    if (!SDL_mice[index]) {
        SDL_OutOfMemory();
        return -1;
    }
    *SDL_mice[index] = *mouse;

    /* we're setting the mouse properties */
    length = 0;
    length = SDL_strlen(name);
    SDL_mice[index]->focus = 0;
    SDL_mice[index]->name = SDL_malloc((length + 2) * sizeof(char));
    SDL_strlcpy(SDL_mice[index]->name, name, length + 1);
    SDL_mice[index]->pressure_max = pressure_max;
    SDL_mice[index]->pressure_min = pressure_min;
    SDL_mice[index]->cursor_shown = SDL_TRUE;
    selected_mouse = SDL_SelectMouse(index);
    SDL_mice[index]->cur_cursor = NULL;
    SDL_mice[index]->def_cursor =
        SDL_CreateCursor(default_cdata, default_cmask, DEFAULT_CWIDTH,
                         DEFAULT_CHEIGHT, DEFAULT_CHOTX, DEFAULT_CHOTY);
    SDL_SetCursor(SDL_mice[index]->def_cursor);
    /* we're assuming that all mice are in the computer sensing zone */
    SDL_mice[index]->proximity = SDL_TRUE;
    /* we're assuming that all mice are working in the absolute position mode
       thanx to that, the users that don't want to use many mice don't have to
       worry about anything */
    SDL_mice[index]->relative_mode = SDL_FALSE;
    SDL_mice[index]->current_end = 0;
    SDL_mice[index]->total_ends = ends;
    SDL_SelectMouse(selected_mouse);

    return index;
}

void
SDL_DelMouse(int index)
{
    SDL_Mouse *mouse = SDL_GetMouse(index);

    if (!mouse) {
        return;
    }

    mouse->def_cursor = NULL;
    SDL_free(mouse->name);
    while (mouse->cursors) {
        SDL_FreeCursor(mouse->cursors);
    }

    if (mouse->FreeMouse) {
        mouse->FreeMouse(mouse);
    }
    SDL_free(mouse);

    SDL_mice[index] = NULL;
}

void
SDL_ResetMouse(int index)
{
    SDL_Mouse *mouse = SDL_GetMouse(index);

    if (!mouse) {
        return;
    }

    /* FIXME */
}

void
SDL_MouseQuit(void)
{
    int i;

    for (i = 0; i < SDL_num_mice; ++i) {
        SDL_DelMouse(i);
    }
    SDL_num_mice = 0;
    SDL_current_mouse = -1;

    if (SDL_mice) {
        SDL_free(SDL_mice);
        SDL_mice = NULL;
    }
}

int
SDL_GetNumMice(void)
{
    return SDL_num_mice;
}

int
SDL_SelectMouse(int index)
{
    if (index >= 0 && index < SDL_num_mice) {
        SDL_current_mouse = index;
    }
    return SDL_current_mouse;
}

SDL_WindowID
SDL_GetMouseFocusWindow(int index)
{
    SDL_Mouse *mouse = SDL_GetMouse(index);

    if (!mouse) {
        return 0;
    }
    return mouse->focus;
}

static int SDLCALL
FlushMouseMotion(void *param, SDL_Event * event)
{
    if (event->type == SDL_MOUSEMOTION
        && event->motion.which == (Uint8) SDL_current_mouse) {
        return 0;
    } else {
        return 1;
    }
}

int
SDL_SetRelativeMouseMode(int index, SDL_bool enabled)
{
    SDL_Mouse *mouse = SDL_GetMouse(index);

    if (!mouse) {
        return -1;
    }

    /* Flush pending mouse motion */
    mouse->flush_motion = SDL_TRUE;
    SDL_PumpEvents();
    mouse->flush_motion = SDL_FALSE;
    SDL_FilterEvents(FlushMouseMotion, mouse);

    /* Set the relative mode */
    mouse->relative_mode = enabled;

    /* Update cursor visibility */
    SDL_SetCursor(NULL);

    if (!enabled) {
        /* Restore the expected mouse position */
        SDL_WarpMouseInWindow(mouse->focus, mouse->x, mouse->y);
    }
    return 0;
}

SDL_bool
SDL_GetRelativeMouseMode(int index)
{
    SDL_Mouse *mouse = SDL_GetMouse(index);

    if (!mouse) {
        return SDL_FALSE;
    }
    return mouse->relative_mode;
}

Uint8
SDL_GetMouseState(int index, int *x, int *y)
{
    SDL_Mouse *mouse = SDL_GetMouse(index);

    if (!mouse) {
        if (x) {
            *x = 0;
        }
        if (y) {
            *y = 0;
        }
        return 0;
    }

    if (x) {
        *x = mouse->x;
    }
    if (y) {
        *y = mouse->y;
    }
    return mouse->buttonstate;
}

Uint8
SDL_GetRelativeMouseState(int index, int *x, int *y)
{
    SDL_Mouse *mouse = SDL_GetMouse(index);

    if (!mouse) {
        if (x) {
            *x = 0;
        }
        if (y) {
            *y = 0;
        }
        return 0;
    }

    if (x) {
        *x = mouse->xdelta;
    }
    if (y) {
        *y = mouse->ydelta;
    }
    mouse->xdelta = 0;
    mouse->ydelta = 0;
    return mouse->buttonstate;
}

void
SDL_SetMouseFocus(int id, SDL_WindowID windowID)
{
    int index = SDL_GetMouseIndexId(id);
    SDL_Mouse *mouse = SDL_GetMouse(index);
    int i;
    SDL_bool focus;

    if (!mouse || (mouse->focus == windowID)) {
        return;
    }

    /* See if the current window has lost focus */
    if (mouse->focus) {
        focus = SDL_FALSE;
        for (i = 0; i < SDL_num_mice; ++i) {
            SDL_Mouse *check;
            if (i != index) {
                check = SDL_GetMouse(i);
                if (check && check->focus == mouse->focus) {
                    focus = SDL_TRUE;
                    break;
                }
            }
        }
        if (!focus) {
            SDL_SendWindowEvent(mouse->focus, SDL_WINDOWEVENT_LEAVE, 0, 0);
        }
    }

    mouse->focus = windowID;

    if (mouse->focus) {
        focus = SDL_FALSE;
        for (i = 0; i < SDL_num_mice; ++i) {
            SDL_Mouse *check;
            if (i != index) {
                check = SDL_GetMouse(i);
                if (check && check->focus == mouse->focus) {
                    focus = SDL_TRUE;
                    break;
                }
            }
        }
        if (!focus) {
            SDL_SendWindowEvent(mouse->focus, SDL_WINDOWEVENT_ENTER, 0, 0);
        }
    }
}

int
SDL_SendProximity(int id, int x, int y, int type)
{
    int index = SDL_GetMouseIndexId(id);
    SDL_Mouse *mouse = SDL_GetMouse(index);
    int posted = 0;

    if (!mouse) {
        return 0;
    }

    mouse->last_x = x;
    mouse->last_y = y;
    if (SDL_ProcessEvents[type] == SDL_ENABLE) {
        SDL_Event event;
        event.proximity.which = (Uint8) index;
        event.proximity.x = x;
        event.proximity.y = y;
        event.proximity.cursor = mouse->current_end;
        event.proximity.type = type;
        /* FIXME: is this right? */
        event.proximity.windowID = mouse->focus;
        posted = (SDL_PushEvent(&event) > 0);
        if (type == SDL_PROXIMITYIN) {
            mouse->proximity = SDL_TRUE;
        } else {
            mouse->proximity = SDL_FALSE;
        }
    }
    return posted;
}

int
SDL_SendMouseMotion(int id, int relative, int x, int y, int pressure)
{
    int index = SDL_GetMouseIndexId(id);
    SDL_Mouse *mouse = SDL_GetMouse(index);
    int posted;
    int xrel;
    int yrel;
    int x_max = 0, y_max = 0;

    if (!mouse || mouse->flush_motion) {
        return 0;
    }

    /* if the mouse is out of proximity we don't to want to have any motion from it */
    if (mouse->proximity == SDL_FALSE) {
        mouse->last_x = x;
        mouse->last_y = y;
        return 0;
    }

    /* the relative motion is calculated regarding the system cursor last position */
    if (relative) {
        xrel = x;
        yrel = y;
        x = (mouse->last_x + x);
        y = (mouse->last_y + y);
    } else {
        xrel = x - mouse->last_x;
        yrel = y - mouse->last_y;
    }

    /* Drop events that don't change state */
    if (!xrel && !yrel) {
#if 0
        printf("Mouse event didn't change state - dropped!\n");
#endif
        return 0;
    }

    /* Update internal mouse coordinates */
    if (mouse->relative_mode == SDL_FALSE) {
        mouse->x = x;
        mouse->y = y;
    } else {
        mouse->x += xrel;
        mouse->y += yrel;
    }

    SDL_GetWindowSize(mouse->focus, &x_max, &y_max);

    /* make sure that the pointers find themselves inside the windows */
    /* only check if mouse->xmax is set ! */
    if (x_max && mouse->x > x_max) {
        mouse->x = x_max;
    } else if (mouse->x < 0) {
        mouse->x = 0;
    }

    if (y_max && mouse->y > y_max) {
        mouse->y = y_max;
    } else if (mouse->y < 0) {
        mouse->y = 0;
    }

    mouse->xdelta += xrel;
    mouse->ydelta += yrel;
    mouse->pressure = pressure;

    /* Move the mouse cursor, if needed */
    if (mouse->cursor_shown && !mouse->relative_mode &&
        mouse->MoveCursor && mouse->cur_cursor) {
        mouse->MoveCursor(mouse->cur_cursor);
    }

    /* Post the event, if desired */
    posted = 0;
    if (SDL_ProcessEvents[SDL_MOUSEMOTION] == SDL_ENABLE &&
        mouse->proximity == SDL_TRUE) {
        SDL_Event event;
        event.motion.type = SDL_MOUSEMOTION;
        event.motion.which = (Uint8) index;
        event.motion.state = mouse->buttonstate;
        event.motion.x = mouse->x;
        event.motion.y = mouse->y;
        event.motion.z = mouse->z;
        event.motion.pressure = mouse->pressure;
        event.motion.pressure_max = mouse->pressure_max;
        event.motion.pressure_min = mouse->pressure_min;
        event.motion.rotation = 0;
        event.motion.tilt = 0;
        event.motion.cursor = mouse->current_end;
        event.motion.xrel = xrel;
        event.motion.yrel = yrel;
        event.motion.windowID = mouse->focus;
        posted = (SDL_PushEvent(&event) > 0);
    }
    mouse->last_x = mouse->x;
    mouse->last_y = mouse->y;
    return posted;
}

int
SDL_SendMouseButton(int id, Uint8 state, Uint8 button)
{
    int index = SDL_GetMouseIndexId(id);
    SDL_Mouse *mouse = SDL_GetMouse(index);
    int posted;
    Uint8 type;

    if (!mouse) {
        return 0;
    }

    /* Figure out which event to perform */
    switch (state) {
    case SDL_PRESSED:
        if (mouse->buttonstate & SDL_BUTTON(button)) {
            /* Ignore this event, no state change */
            return 0;
        }
        type = SDL_MOUSEBUTTONDOWN;
        mouse->buttonstate |= SDL_BUTTON(button);
        break;
    case SDL_RELEASED:
        if (!(mouse->buttonstate & SDL_BUTTON(button))) {
            /* Ignore this event, no state change */
            return 0;
        }
        type = SDL_MOUSEBUTTONUP;
        mouse->buttonstate &= ~SDL_BUTTON(button);
        break;
    default:
        /* Invalid state -- bail */
        return 0;
    }

    /* Post the event, if desired */
    posted = 0;
    if (SDL_ProcessEvents[type] == SDL_ENABLE) {
        SDL_Event event;
        event.type = type;
        event.button.which = (Uint8) index;
        event.button.state = state;
        event.button.button = button;
        event.button.x = mouse->x;
        event.button.y = mouse->y;
        event.button.windowID = mouse->focus;
        posted = (SDL_PushEvent(&event) > 0);
    }
    return posted;
}

int
SDL_SendMouseWheel(int index, int x, int y)
{
    SDL_Mouse *mouse = SDL_GetMouse(index);
    int posted;

    if (!mouse || (!x && !y)) {
        return 0;
    }

    /* Post the event, if desired */
    posted = 0;
    if (SDL_ProcessEvents[SDL_MOUSEWHEEL] == SDL_ENABLE) {
        SDL_Event event;
        event.type = SDL_MOUSEWHEEL;
        event.wheel.which = (Uint8) index;
        event.wheel.x = x;
        event.wheel.y = y;
        event.wheel.windowID = mouse->focus;
        posted = (SDL_PushEvent(&event) > 0);
    }
    return posted;
}

void
SDL_WarpMouseInWindow(SDL_WindowID windowID, int x, int y)
{
    SDL_Mouse *mouse = SDL_GetMouse(SDL_current_mouse);

    if (!mouse) {
        return;
    }

    if (mouse->WarpMouse) {
        mouse->WarpMouse(mouse, windowID, x, y);
    } else {
        SDL_SetMouseFocus(SDL_current_mouse, windowID);
        SDL_SendMouseMotion(SDL_current_mouse, 0, x, y, 0);
    }
}

SDL_Cursor *
SDL_CreateCursor(const Uint8 * data, const Uint8 * mask,
                 int w, int h, int hot_x, int hot_y)
{
    SDL_Mouse *mouse = SDL_GetMouse(SDL_current_mouse);
    SDL_Surface *surface;
    SDL_Cursor *cursor;
    int x, y;
    Uint32 *pixel;
    Uint8 datab, maskb;
    const Uint32 black = 0xFF000000;
    const Uint32 white = 0xFFFFFFFF;
    const Uint32 transparent = 0x00000000;

    if (!mouse) {
        SDL_SetError("No mice are initialized");
        return NULL;
    }

    if (!mouse->CreateCursor) {
        SDL_SetError("Current mouse doesn't have cursor support");
        return NULL;
    }

    /* Sanity check the hot spot */
    if ((hot_x < 0) || (hot_y < 0) || (hot_x >= w) || (hot_y >= h)) {
        SDL_SetError("Cursor hot spot doesn't lie within cursor");
        return NULL;
    }

    /* Make sure the width is a multiple of 8 */
    w = ((w + 7) & ~7);

    /* Create the surface from a bitmap */
    surface =
        SDL_CreateRGBSurface(0, w, h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF,
                             0xFF000000);
    if (!surface) {
        return NULL;
    }
    for (y = 0; y < h; ++y) {
        pixel = (Uint32 *) ((Uint8 *) surface->pixels + y * surface->pitch);
        for (x = 0; x < w; ++x) {
            if ((x % 8) == 0) {
                datab = *data++;
                maskb = *mask++;
            }
            if (maskb & 0x80) {
                *pixel++ = (datab & 0x80) ? black : white;
            } else {
                *pixel++ = (datab & 0x80) ? black : transparent;
            }
            datab <<= 1;
            maskb <<= 1;
        }
    }

    cursor = mouse->CreateCursor(surface, hot_x, hot_y);
    if (cursor) {
        cursor->mouse = mouse;
        cursor->next = mouse->cursors;
        mouse->cursors = cursor;
    }

    SDL_FreeSurface(surface);

    return cursor;
}

/* SDL_SetCursor(NULL) can be used to force the cursor redraw,
   if this is desired for any reason.  This is used when setting
   the video mode and when the SDL window gains the mouse focus.
 */
void
SDL_SetCursor(SDL_Cursor * cursor)
{
    SDL_Mouse *mouse = SDL_GetMouse(SDL_current_mouse);

    if (!mouse) {
        SDL_SetError("No mice are initialized");
        return;
    }

    /* Set the new cursor */
    if (cursor) {
        /* Make sure the cursor is still valid for this mouse */
        SDL_Cursor *found;
        for (found = mouse->cursors; found; found = found->next) {
            if (found == cursor) {
                break;
            }
        }
        if (!found) {
            SDL_SetError("Cursor not associated with the current mouse");
            return;
        }
        mouse->cur_cursor = cursor;
    } else {
        cursor = mouse->cur_cursor;
    }

    if (cursor && mouse->cursor_shown && !mouse->relative_mode) {
        if (mouse->ShowCursor) {
            mouse->ShowCursor(cursor);
        }
    } else {
        if (mouse->ShowCursor) {
            mouse->ShowCursor(NULL);
        }
    }
}

SDL_Cursor *
SDL_GetCursor(void)
{
    SDL_Mouse *mouse = SDL_GetMouse(SDL_current_mouse);

    if (!mouse) {
        return NULL;
    }
    return mouse->cur_cursor;
}

void
SDL_FreeCursor(SDL_Cursor * cursor)
{
    SDL_Mouse *mouse;
    SDL_Cursor *curr, *prev;

    if (!cursor) {
        return;
    }
    mouse = cursor->mouse;

    if (cursor == mouse->def_cursor) {
        return;
    }
    if (cursor == mouse->cur_cursor) {
        SDL_SetCursor(mouse->def_cursor);
    }

    for (prev = NULL, curr = mouse->cursors; curr;
         prev = curr, curr = curr->next) {
        if (curr == cursor) {
            if (prev) {
                prev->next = curr->next;
            } else {
                mouse->cursors = curr->next;
            }

            if (mouse->FreeCursor) {
                mouse->FreeCursor(curr);
            }
            return;
        }
    }
}

int
SDL_ShowCursor(int toggle)
{
    SDL_Mouse *mouse = SDL_GetMouse(SDL_current_mouse);
    SDL_bool shown;

    if (!mouse) {
        return 0;
    }

    shown = mouse->cursor_shown;
    if (toggle >= 0) {
        if (toggle) {
            mouse->cursor_shown = SDL_TRUE;
        } else {
            mouse->cursor_shown = SDL_FALSE;
        }
        if (mouse->cursor_shown != shown) {
            SDL_SetCursor(NULL);
        }
    }
    return shown;
}

char *
SDL_GetMouseName(int index)
{
    SDL_Mouse *mouse = SDL_GetMouse(index);
    if (!mouse) {
        return NULL;
    }
    return mouse->name;
}

void
SDL_ChangeEnd(int id, int end)
{
    int index = SDL_GetMouseIndexId(id);
    SDL_Mouse *mouse = SDL_GetMouse(index);

    if (mouse) {
        mouse->current_end = end;
    }
}

int
SDL_GetCursorsNumber(int index)
{
    SDL_Mouse *mouse = SDL_GetMouse(index);

    if (!mouse) {
        return -1;
    }
    return mouse->total_ends;
}

int
SDL_GetCurrentCursor(int index)
{
    SDL_Mouse *mouse = SDL_GetMouse(index);

    if (!mouse) {
        return -1;
    }
    return mouse->current_end;
}

/* vi: set ts=4 sw=4 expandtab: */
