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

    QNX Photon GUI SDL driver
    Copyright (C) 2009 Mike Gorchak
    (mike@malva.ua, lestat@i.com.ua)
*/

#include "SDL_photon_input.h"

#include "SDL_config.h"
#include "SDL_events.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"

#include "SDL_photon_keycodes.h"

/* Mouse related functions */
SDL_Cursor *photon_createcursor(SDL_Surface * surface, int hot_x, int hot_y);
int photon_showcursor(SDL_Cursor * cursor);
void photon_movecursor(SDL_Cursor * cursor);
void photon_freecursor(SDL_Cursor * cursor);
void photon_warpmouse(SDL_Mouse * mouse, SDL_WindowID windowID, int x, int y);
void photon_freemouse(SDL_Mouse * mouse);

int32_t
photon_addinputdevices(_THIS)
{
    SDL_VideoData *phdata = (SDL_VideoData *) _this->driverdata;
    SDL_DisplayData *didata = NULL;
    struct SDL_Mouse photon_mouse;
    SDL_MouseData *mdata = NULL;
    SDL_Keyboard photon_keyboard;
    SDLKey keymap[SDL_NUM_SCANCODES];
    uint32_t it;

    for (it = 0; it < _this->num_displays; it++) {
        /* Clear SDL mouse structure */
        SDL_memset(&photon_mouse, 0x00, sizeof(struct SDL_Mouse));

        /* Allocate SDL_MouseData structure */
        mdata = (SDL_MouseData *) SDL_calloc(1, sizeof(SDL_MouseData));
        if (mdata == NULL) {
            SDL_OutOfMemory();
            return -1;
        }

        /* Mark this mouse with ID 0 */
        photon_mouse.id = it;
        photon_mouse.driverdata = (void *) mdata;
        photon_mouse.CreateCursor = photon_createcursor;
        photon_mouse.ShowCursor = photon_showcursor;
        photon_mouse.MoveCursor = photon_movecursor;
        photon_mouse.FreeCursor = photon_freecursor;
        photon_mouse.WarpMouse = photon_warpmouse;
        photon_mouse.FreeMouse = photon_freemouse;

        /* Get display data */
        didata = (SDL_DisplayData *) _this->displays[it].driverdata;

        /* Store SDL_DisplayData pointer in the mouse driver internals */
        mdata->didata = didata;

        /* Register mouse cursor in SDL */
        SDL_AddMouse(&photon_mouse, "Photon mouse cursor", 0, 0, 1);
    }

    /* Photon maps all keyboards to one */
    SDL_zero(photon_keyboard);
    SDL_AddKeyboard(&photon_keyboard, -1);

    /* Add default scancode to key mapping */
    SDL_GetDefaultKeymap(keymap);
    SDL_SetKeymap(0, 0, keymap, SDL_NUM_SCANCODES);

    return 0;
}

int32_t
photon_delinputdevices(_THIS)
{
    /* Destroy all of the mice */
    SDL_MouseQuit();
}

/*****************************************************************************/
/* Photon mouse related functions                                            */
/*****************************************************************************/
SDL_Cursor *
photon_createcursor(SDL_Surface * surface, int hot_x, int hot_y)
{
    PhCursorDef_t *internal_cursor;
    SDL_Cursor *sdl_cursor;
    uint8_t *image0 = NULL;
    uint8_t *image1 = NULL;
    uint32_t it;
    uint32_t jt;
    uint32_t shape_color;

    /* SDL converts monochrome cursor shape to 32bpp cursor shape      */
    /* and we must convert it back to monochrome, this routine handles */
    /* 24/32bpp surfaces only                                          */
    if ((surface->format->BitsPerPixel != 32)
        && (surface->format->BitsPerPixel != 24)) {
        SDL_SetError("Photon: Cursor shape is not 24/32bpp.");
        return NULL;
    }

    /* Checking data parameters */
    if ((surface->w == 0) || (surface->h == 0)) {
        SDL_SetError("Photon: Cursor shape dimensions are zero");
        return NULL;
    }

    /* Allocate memory for the internal cursor format */
    internal_cursor = (PhCursorDef_t *) SDL_calloc(1, sizeof(PhCursorDef_t) +
                                                   ((((surface->w +
                                                       7) >> 3) *
                                                     surface->h) * 2) - 1);
    if (internal_cursor == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    /* Allocate memory for the SDL cursor */
    sdl_cursor = (SDL_Cursor *) SDL_calloc(1, sizeof(SDL_Cursor));
    if (sdl_cursor == NULL) {
        SDL_free(internal_cursor);
        SDL_OutOfMemory();
        return NULL;
    }

    /* Set driverdata as photon cursor format */
    image0 = (uint8_t *) internal_cursor;
    image0 += sizeof(PhCursorDef_t) - 1;
    image1 = image0;
    image1 += ((surface->w + 7) >> 3) * surface->h;
    sdl_cursor->driverdata = (void *) internal_cursor;
    internal_cursor->hdr.len =
        (sizeof(PhCursorDef_t) - sizeof(PhRegionDataHdr_t)) +
        ((((surface->w + 7) >> 3) * surface->h) * 2) - 1;
    internal_cursor->hdr.type = Ph_RDATA_CURSOR;
    internal_cursor->size1.x = surface->w;
    internal_cursor->size1.y = surface->h;
    internal_cursor->size2.x = surface->w;
    internal_cursor->size2.y = surface->h;
    internal_cursor->offset1.x = -hot_x;
    internal_cursor->offset1.y = -hot_y;
    internal_cursor->offset2.x = -hot_x;
    internal_cursor->offset2.y = -hot_y;
    internal_cursor->bytesperline1 = ((surface->w + 7) >> 3);
    internal_cursor->bytesperline2 = ((surface->w + 7) >> 3);
    internal_cursor->color1 = (SDL_PHOTON_MOUSE_COLOR_BLACK) & 0x00FFFFFF;
    internal_cursor->color2 = (SDL_PHOTON_MOUSE_COLOR_WHITE) & 0x00FFFFFF;

    /* Convert cursor from 32 bpp */
    for (jt = 0; jt < surface->h; jt++) {
        for (it = 0; it < surface->w; it++) {
            shape_color =
                *((uint32_t *) ((uint8_t *) surface->pixels +
                                jt * surface->pitch +
                                it * surface->format->BytesPerPixel));
            switch (shape_color) {
            case SDL_PHOTON_MOUSE_COLOR_BLACK:
                {
                    *(image0 + jt * (internal_cursor->bytesperline1) +
                      (it >> 3)) |= 0x80 >> (it % 8);
                    *(image1 + jt * (internal_cursor->bytesperline2) +
                      (it >> 3)) &= ~(0x80 >> (it % 8));
                }
                break;
            case SDL_PHOTON_MOUSE_COLOR_WHITE:
                {
                    *(image0 + jt * (internal_cursor->bytesperline1) +
                      (it >> 3)) &= ~(0x80 >> (it % 8));
                    *(image1 + jt * (internal_cursor->bytesperline2) +
                      (it >> 3)) |= 0x80 >> (it % 8);
                }
                break;
            case SDL_PHOTON_MOUSE_COLOR_TRANS:
                {
                    *(image0 + jt * (internal_cursor->bytesperline1) +
                      (it >> 3)) &= ~(0x80 >> (it % 8));
                    *(image1 + jt * (internal_cursor->bytesperline2) +
                      (it >> 3)) &= ~(0x80 >> (it % 8));
                }
                break;
            default:
                {
                    /* The same as transparent color, must not happen */
                    *(image0 + jt * (internal_cursor->bytesperline1) +
                      (it >> 3)) &= ~(0x80 >> (it % 8));
                    *(image1 + jt * (internal_cursor->bytesperline2) +
                      (it >> 3)) &= ~(0x80 >> (it % 8));
                }
                break;
            }
        }
    }

    return sdl_cursor;
}

int
photon_showcursor(SDL_Cursor * cursor)
{
    SDL_VideoDisplay *display;
    SDL_DisplayData *didata;
    SDL_Window *window;
    SDL_WindowData *wdata;
    SDL_WindowID window_id;
    PhCursorDef_t *internal_cursor;
    int32_t status;

    /* Get current window id */
    window_id = SDL_GetFocusWindow();
    if (window_id <= 0) {
        SDL_MouseData *mdata = NULL;

        /* If there is no current window, then someone calls this function */
        /* to set global mouse settings during SDL initialization          */
        if (cursor != NULL) {
            /* Store cursor for future usage */
            mdata = (SDL_MouseData *) cursor->mouse->driverdata;
            didata = (SDL_DisplayData *) mdata->didata;
            internal_cursor = (PhCursorDef_t *) cursor->driverdata;

            if (didata->cursor_size >=
                (internal_cursor->hdr.len + sizeof(PhRegionDataHdr_t))) {
                SDL_memcpy(didata->cursor, internal_cursor,
                           internal_cursor->hdr.len +
                           sizeof(PhRegionDataHdr_t));
            } else {
                /* Partitial cursor image */
                SDL_memcpy(didata->cursor, internal_cursor,
                           didata->cursor_size);
            }

            didata->cursor_visible = SDL_TRUE;
            return 0;
        } else {
            /* We can't get SDL_DisplayData at this point, return fake success */
            return 0;
        }
    } else {
        /* Sanity checks */
        window = SDL_GetWindowFromID(window_id);
        if (window != NULL) {
            display = SDL_GetDisplayFromWindow(window);
            if (display != NULL) {
                didata = (SDL_DisplayData *) display->driverdata;
                if (didata != NULL) {
                    wdata = (SDL_WindowData *) window->driverdata;
                    if (wdata == NULL) {
                        return -1;
                    }
                } else {
                    return -1;
                }
            } else {
                return -1;
            }
        } else {
            return -1;
        }
    }

    /* return if window widget has been destroyed already */
    if (wdata->window == NULL) {
        return;
    }

    /* Check if we need to set new shape or disable cursor shape */
    if (cursor != NULL) {
        /* Retrieve photon cursor shape */
        internal_cursor = (PhCursorDef_t *) cursor->driverdata;
        if (internal_cursor == NULL) {
            SDL_SetError("Photon: Internal cursor data is absent");
            return -1;
        }

        /* Setup cursor type */
        status =
            PtSetResource(wdata->window, Pt_ARG_CURSOR_TYPE, Ph_CURSOR_BITMAP,
                          0);
        if (status != 0) {
            SDL_SetError("Photon: Failed to set cursor type to bitmap");
            return -1;
        }

        /* Setup cursor color to default */
        status =
            PtSetResource(wdata->window, Pt_ARG_CURSOR_COLOR,
                          Ph_CURSOR_DEFAULT_COLOR, 0);
        if (status != 0) {
            SDL_SetError("Photon: Failed to set cursor color");
            return -1;
        }

        /* Setup cursor shape */
        status =
            PtSetResource(wdata->window, Pt_ARG_BITMAP_CURSOR,
                          internal_cursor,
                          internal_cursor->hdr.len +
                          sizeof(PhRegionDataHdr_t));
        if (status != 0) {
            SDL_SetError("Photon: Failed to set cursor color");
            return -1;
        }

        /* Store current cursor for future usage */
        if (didata->cursor_size >=
            (internal_cursor->hdr.len + sizeof(PhRegionDataHdr_t))) {
            SDL_memcpy(didata->cursor, internal_cursor,
                       internal_cursor->hdr.len + sizeof(PhRegionDataHdr_t));
        } else {
            /* Partitial cursor image */
            SDL_memcpy(didata->cursor, internal_cursor, didata->cursor_size);
        }

        /* Set cursor visible */
        didata->cursor_visible = SDL_TRUE;
    } else {
        /* SDL requests to disable cursor */
        status =
            PtSetResource(wdata->window, Pt_ARG_CURSOR_TYPE, Ph_CURSOR_NONE,
                          0);
        if (status != 0) {
            SDL_SetError("Photon: Can't disable cursor");
            return -1;
        }

        /* Set cursor invisible */
        didata->cursor_visible = SDL_FALSE;
    }

    /* Flush all pending widget data */
    PtFlush();

    /* New cursor shape is set */
    return 0;
}

void
photon_movecursor(SDL_Cursor * cursor)
{
    SDL_VideoDisplay *display;
    SDL_DisplayData *didata;
    SDL_Window *window;
    SDL_WindowData *wdata;
    SDL_WindowID window_id;
    int32_t status;

    /* Get current window id */
    window_id = SDL_GetFocusWindow();
    if (window_id <= 0) {
        didata = (SDL_DisplayData *) cursor->mouse->driverdata;
    } else {
        /* Sanity checks */
        window = SDL_GetWindowFromID(window_id);
        if (window != NULL) {
            display = SDL_GetDisplayFromWindow(window);
            if (display != NULL) {
                didata = (SDL_DisplayData *) display->driverdata;
                if (didata != NULL) {
                    wdata = (SDL_WindowData *) window->driverdata;
                    if (wdata == NULL) {
                        return;
                    }
                } else {
                    return;
                }
            } else {
                return;
            }
        } else {
            return;
        }
    }

    /* No need to move mouse cursor manually in the photon */

    return;
}

void
photon_freecursor(SDL_Cursor * cursor)
{
    PhCursorDef_t *internal_cursor = NULL;

    if (cursor != NULL) {
        internal_cursor = (PhCursorDef_t *) cursor->driverdata;
        if (internal_cursor != NULL) {
            SDL_free(internal_cursor);
            cursor->driverdata = NULL;
        }
    }
}

void
photon_warpmouse(SDL_Mouse * mouse, SDL_WindowID windowID, int x, int y)
{
    SDL_VideoDisplay *display;
    SDL_DisplayData *didata;
    SDL_Window *window;
    SDL_WindowData *wdata;
    int16_t wx;
    int16_t wy;

    /* Sanity checks */
    window = SDL_GetWindowFromID(windowID);
    if (window != NULL) {
        display = SDL_GetDisplayFromWindow(window);
        if (display != NULL) {
            didata = (SDL_DisplayData *) display->driverdata;
            if (didata != NULL) {
                wdata = (SDL_WindowData *) window->driverdata;
                if (wdata == NULL) {
                    return;
                }
            } else {
                return;
            }
        } else {
            return;
        }
    } else {
        return;
    }

    PtGetAbsPosition(wdata->window, &wx, &wy);
    PhMoveCursorAbs(PhInputGroup(NULL), wx + x, wy + y);

    return;
}

void
photon_freemouse(SDL_Mouse * mouse)
{
    if (mouse->driverdata == NULL) {
        return;
    }

    /* Mouse framework doesn't deletes automatically our driverdata */
    SDL_free(mouse->driverdata);
    mouse->driverdata = NULL;

    return;
}

SDL_scancode
photon_to_sdl_keymap(uint32_t key)
{
    SDL_scancode scancode = SDL_SCANCODE_UNKNOWN;

    switch (key & 0x0000007F) {
    case PHOTON_SCANCODE_ESCAPE:
        scancode = SDL_SCANCODE_ESCAPE;
        break;
    case PHOTON_SCANCODE_F1:
        scancode = SDL_SCANCODE_F1;
        break;
    case PHOTON_SCANCODE_F2:
        scancode = SDL_SCANCODE_F2;
        break;
    case PHOTON_SCANCODE_F3:
        scancode = SDL_SCANCODE_F3;
        break;
    case PHOTON_SCANCODE_F4:
        scancode = SDL_SCANCODE_F4;
        break;
    case PHOTON_SCANCODE_F5:
        scancode = SDL_SCANCODE_F5;
        break;
    case PHOTON_SCANCODE_F6:
        scancode = SDL_SCANCODE_F6;
        break;
    case PHOTON_SCANCODE_F7:
        scancode = SDL_SCANCODE_F7;
        break;
    case PHOTON_SCANCODE_F8:
        scancode = SDL_SCANCODE_F8;
        break;
    case PHOTON_SCANCODE_F9:
        scancode = SDL_SCANCODE_F9;
        break;
    case PHOTON_SCANCODE_F10:
        scancode = SDL_SCANCODE_F10;
        break;
    case PHOTON_SCANCODE_F11:
        scancode = SDL_SCANCODE_F11;
        break;
    case PHOTON_SCANCODE_F12:
        scancode = SDL_SCANCODE_F12;
        break;
    case PHOTON_SCANCODE_BACKQOUTE:
        scancode = SDL_SCANCODE_GRAVE;
        break;
    case PHOTON_SCANCODE_1:
        scancode = SDL_SCANCODE_1;
        break;
    case PHOTON_SCANCODE_2:
        scancode = SDL_SCANCODE_2;
        break;
    case PHOTON_SCANCODE_3:
        scancode = SDL_SCANCODE_3;
        break;
    case PHOTON_SCANCODE_4:
        scancode = SDL_SCANCODE_4;
        break;
    case PHOTON_SCANCODE_5:
        scancode = SDL_SCANCODE_5;
        break;
    case PHOTON_SCANCODE_6:
        scancode = SDL_SCANCODE_6;
        break;
    case PHOTON_SCANCODE_7:
        scancode = SDL_SCANCODE_7;
        break;
    case PHOTON_SCANCODE_8:
        scancode = SDL_SCANCODE_8;
        break;
    case PHOTON_SCANCODE_9:
        scancode = SDL_SCANCODE_9;
        break;
    case PHOTON_SCANCODE_0:
        scancode = SDL_SCANCODE_0;
        break;
    case PHOTON_SCANCODE_MINUS:
        scancode = SDL_SCANCODE_MINUS;
        break;
    case PHOTON_SCANCODE_EQUAL:
        scancode = SDL_SCANCODE_EQUALS;
        break;
    case PHOTON_SCANCODE_BACKSPACE:
        scancode = SDL_SCANCODE_BACKSPACE;
        break;
    case PHOTON_SCANCODE_TAB:
        scancode = SDL_SCANCODE_TAB;
        break;
    case PHOTON_SCANCODE_Q:
        scancode = SDL_SCANCODE_Q;
        break;
    case PHOTON_SCANCODE_W:
        scancode = SDL_SCANCODE_W;
        break;
    case PHOTON_SCANCODE_E:
        scancode = SDL_SCANCODE_E;
        break;
    case PHOTON_SCANCODE_R:
        scancode = SDL_SCANCODE_R;
        break;
    case PHOTON_SCANCODE_T:
        scancode = SDL_SCANCODE_T;
        break;
    case PHOTON_SCANCODE_Y:
        scancode = SDL_SCANCODE_Y;
        break;
    case PHOTON_SCANCODE_U:
        scancode = SDL_SCANCODE_U;
        break;
    case PHOTON_SCANCODE_I:
        scancode = SDL_SCANCODE_I;
        break;
    case PHOTON_SCANCODE_O:
        scancode = SDL_SCANCODE_O;
        break;
    case PHOTON_SCANCODE_P:
        scancode = SDL_SCANCODE_P;
        break;
    case PHOTON_SCANCODE_LEFT_SQ_BR:
        scancode = SDL_SCANCODE_LEFTBRACKET;
        break;
    case PHOTON_SCANCODE_RIGHT_SQ_BR:
        scancode = SDL_SCANCODE_RIGHTBRACKET;
        break;
    case PHOTON_SCANCODE_ENTER:
        scancode = SDL_SCANCODE_RETURN;
        break;
    case PHOTON_SCANCODE_CAPSLOCK:
        scancode = SDL_SCANCODE_CAPSLOCK;
        break;
    case PHOTON_SCANCODE_A:
        scancode = SDL_SCANCODE_A;
        break;
    case PHOTON_SCANCODE_S:
        scancode = SDL_SCANCODE_S;
        break;
    case PHOTON_SCANCODE_D:
        scancode = SDL_SCANCODE_D;
        break;
    case PHOTON_SCANCODE_F:
        scancode = SDL_SCANCODE_F;
        break;
    case PHOTON_SCANCODE_G:
        scancode = SDL_SCANCODE_G;
        break;
    case PHOTON_SCANCODE_H:
        scancode = SDL_SCANCODE_H;
        break;
    case PHOTON_SCANCODE_J:
        scancode = SDL_SCANCODE_J;
        break;
    case PHOTON_SCANCODE_K:
        scancode = SDL_SCANCODE_K;
        break;
    case PHOTON_SCANCODE_L:
        scancode = SDL_SCANCODE_L;
        break;
    case PHOTON_SCANCODE_SEMICOLON:
        scancode = SDL_SCANCODE_SEMICOLON;
        break;
    case PHOTON_SCANCODE_QUOTE:
        scancode = SDL_SCANCODE_APOSTROPHE;
        break;
    case PHOTON_SCANCODE_BACKSLASH:
        scancode = SDL_SCANCODE_BACKSLASH;
        break;
    case PHOTON_SCANCODE_LEFT_SHIFT:
        scancode = SDL_SCANCODE_LSHIFT;
        break;
    case PHOTON_SCANCODE_Z:
        scancode = SDL_SCANCODE_Z;
        break;
    case PHOTON_SCANCODE_X:
        scancode = SDL_SCANCODE_X;
        break;
    case PHOTON_SCANCODE_C:
        scancode = SDL_SCANCODE_C;
        break;
    case PHOTON_SCANCODE_V:
        scancode = SDL_SCANCODE_V;
        break;
    case PHOTON_SCANCODE_B:
        scancode = SDL_SCANCODE_B;
        break;
    case PHOTON_SCANCODE_N:
        scancode = SDL_SCANCODE_N;
        break;
    case PHOTON_SCANCODE_M:
        scancode = SDL_SCANCODE_M;
        break;
    case PHOTON_SCANCODE_COMMA:
        scancode = SDL_SCANCODE_COMMA;
        break;
    case PHOTON_SCANCODE_POINT:
        scancode = SDL_SCANCODE_PERIOD;
        break;
    case PHOTON_SCANCODE_SLASH:
        scancode = SDL_SCANCODE_SLASH;
        break;
    case PHOTON_SCANCODE_RIGHT_SHIFT:
        scancode = SDL_SCANCODE_RSHIFT;
        break;
    case PHOTON_SCANCODE_CTRL:
        scancode = SDL_SCANCODE_LCTRL;
        break;
    case PHOTON_SCANCODE_WFLAG:
        scancode = SDL_SCANCODE_LGUI;
        break;
    case PHOTON_SCANCODE_ALT:
        scancode = SDL_SCANCODE_LALT;
        break;
    case PHOTON_SCANCODE_SPACE:
        scancode = SDL_SCANCODE_SPACE;
        break;
    case PHOTON_SCANCODE_MENU:
        scancode = SDL_SCANCODE_MENU;
        break;
    case PHOTON_SCANCODE_PRNSCR:
        scancode = SDL_SCANCODE_PRINTSCREEN;
        break;
    case PHOTON_SCANCODE_SCROLLLOCK:
        scancode = SDL_SCANCODE_SCROLLLOCK;
        break;
    case PHOTON_SCANCODE_INSERT:
        scancode = SDL_SCANCODE_INSERT;
        break;
    case PHOTON_SCANCODE_HOME:
        scancode = SDL_SCANCODE_HOME;
        break;
    case PHOTON_SCANCODE_PAGEUP:
        scancode = SDL_SCANCODE_PAGEUP;
        break;
    case PHOTON_SCANCODE_DELETE:
        scancode = SDL_SCANCODE_DELETE;
        break;
    case PHOTON_SCANCODE_END:
        scancode = SDL_SCANCODE_END;
        break;
    case PHOTON_SCANCODE_PAGEDOWN:
        scancode = SDL_SCANCODE_PAGEDOWN;
        break;
    case PHOTON_SCANCODE_UP:
        scancode = SDL_SCANCODE_UP;
        break;
    case PHOTON_SCANCODE_DOWN:
        scancode = SDL_SCANCODE_DOWN;
        break;
    case PHOTON_SCANCODE_LEFT:
        scancode = SDL_SCANCODE_LEFT;
        break;
    case PHOTON_SCANCODE_RIGHT:
        scancode = SDL_SCANCODE_RIGHT;
        break;
    case PHOTON_SCANCODE_NUMLOCK:
        scancode = SDL_SCANCODE_NUMLOCKCLEAR;
        break;
    default:
        break;
    }

    return scancode;
}
