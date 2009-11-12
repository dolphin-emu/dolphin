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

    QNX Graphics Framework SDL driver
    Copyright (C) 2009 Mike Gorchak
    (mike@malva.ua, lestat@i.com.ua)
*/

#include "SDL_gf_input.h"

#include "SDL_config.h"
#include "SDL_events.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"

/* Include QNX HIDDI definitions */
#include "SDL_hiddi_keyboard.h"
#include "SDL_hiddi_mouse.h"
#include "SDL_hiddi_joystick.h"

/* Mouse related functions */
SDL_Cursor *gf_createcursor(SDL_Surface * surface, int hot_x, int hot_y);
int gf_showcursor(SDL_Cursor * cursor);
void gf_movecursor(SDL_Cursor * cursor);
void gf_freecursor(SDL_Cursor * cursor);
void gf_warpmouse(SDL_Mouse * mouse, SDL_WindowID windowID, int x, int y);
void gf_freemouse(SDL_Mouse * mouse);

/* HIDDI interacting functions */
static int32_t hiddi_connect_devices();
static int32_t hiddi_disconnect_devices();

int32_t
gf_addinputdevices(_THIS)
{
    SDL_VideoData *gfdata = (SDL_VideoData *) _this->driverdata;
    SDL_DisplayData *didata;
    struct SDL_Mouse gf_mouse;
    SDL_Keyboard gf_keyboard;
    SDLKey keymap[SDL_NUM_SCANCODES];
    SDL_MouseData *mdata;
    uint32_t it;

    for (it = 0; it < _this->num_displays; it++) {
        /* Clear SDL mouse structure */
        SDL_memset(&gf_mouse, 0x00, sizeof(struct SDL_Mouse));

        /* Allocate SDL_MouseData structure */
        mdata = (SDL_MouseData *) SDL_calloc(1, sizeof(SDL_MouseData));
        if (mdata == NULL) {
            SDL_OutOfMemory();
            return -1;
        }

        /* Mark this mouse with ID 0 */
        gf_mouse.id = it;
        gf_mouse.driverdata = (void *) mdata;
        gf_mouse.CreateCursor = gf_createcursor;
        gf_mouse.ShowCursor = gf_showcursor;
        gf_mouse.MoveCursor = gf_movecursor;
        gf_mouse.FreeCursor = gf_freecursor;
        gf_mouse.WarpMouse = gf_warpmouse;
        gf_mouse.FreeMouse = gf_freemouse;

        /* Get display data */
        didata = (SDL_DisplayData *) _this->displays[it].driverdata;

        /* Store SDL_DisplayData pointer in the mouse driver internals */
        mdata->didata = didata;

        /* Set cursor pos to 0,0 to avoid cursor disappearing in some drivers */
        gf_cursor_set_pos(didata->display, 0, 0, 0);

        /* Register mouse cursor in SDL */
        SDL_AddMouse(&gf_mouse, "GF mouse cursor", 0, 0, 1);
    }

    /* Keyboard could be one only */
    SDL_zero(gf_keyboard);
    SDL_AddKeyboard(&gf_keyboard, -1);

    /* Add scancode to key mapping, HIDDI uses USB HID codes, so */
    /* map will be exact one-to-one */
    SDL_GetDefaultKeymap(keymap);
    SDL_SetKeymap(0, 0, keymap, SDL_NUM_SCANCODES);

    /* Connect to HID server and enumerate all input devices */
    hiddi_connect_devices();

    return 0;
}

int32_t
gf_delinputdevices(_THIS)
{
    /* Disconnect from HID server and release input devices */
    hiddi_disconnect_devices();

    /* Delete keyboard */
    SDL_KeyboardQuit();

    /* Destroy all of the mice */
    SDL_MouseQuit();
}

/*****************************************************************************/
/* GF Mouse related functions                                                */
/*****************************************************************************/
SDL_Cursor *
gf_createcursor(SDL_Surface * surface, int hot_x, int hot_y)
{
    gf_cursor_t *internal_cursor;
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
        SDL_SetError("GF: Cursor shape is not 24/32bpp.");
        return NULL;
    }

    /* Since GF is not checking data, we must check */
    if ((surface->w == 0) || (surface->h == 0)) {
        SDL_SetError("GF: Cursor shape dimensions are zero");
        return NULL;
    }

    /* Allocate memory for the internal cursor format */
    internal_cursor = (gf_cursor_t *) SDL_calloc(1, sizeof(gf_cursor_t));
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

    /* Allocate two monochrome images */
    image0 = (uint8_t *) SDL_calloc(1, ((surface->w + 7) / 8) * surface->h);
    if (image0 == NULL) {
        SDL_free(sdl_cursor);
        SDL_free(internal_cursor);
        SDL_OutOfMemory();
        return NULL;
    }
    image1 = (uint8_t *) SDL_calloc(1, ((surface->w + 7) >> 3) * surface->h);
    if (image1 == NULL) {
        SDL_free(image0);
        SDL_free(sdl_cursor);
        SDL_free(internal_cursor);
        SDL_OutOfMemory();
        return NULL;
    }

    /* Set driverdata as GF cursor format */
    sdl_cursor->driverdata = (void *) internal_cursor;
    internal_cursor->type = GF_CURSOR_BITMAP;
    internal_cursor->hotspot.x = hot_x;
    internal_cursor->hotspot.y = hot_y;
    internal_cursor->cursor.bitmap.w = surface->w;
    internal_cursor->cursor.bitmap.h = surface->h;
    internal_cursor->cursor.bitmap.color0 = SDL_GF_MOUSE_COLOR_BLACK;
    internal_cursor->cursor.bitmap.color1 = SDL_GF_MOUSE_COLOR_WHITE;

    /* Setup cursor shape images */
    internal_cursor->cursor.bitmap.stride = ((surface->w + 7) >> 3);
    internal_cursor->cursor.bitmap.image0 = image0;
    internal_cursor->cursor.bitmap.image1 = image1;

    /* Convert cursor from 32 bpp */
    for (jt = 0; jt < surface->h; jt++) {
        for (it = 0; it < surface->w; it++) {
            shape_color =
                *((uint32_t *) ((uint8_t *) surface->pixels +
                                jt * surface->pitch +
                                it * surface->format->BytesPerPixel));
            switch (shape_color) {
            case SDL_GF_MOUSE_COLOR_BLACK:
                {
                    *(image0 + jt * (internal_cursor->cursor.bitmap.stride) +
                      (it >> 3)) |= 0x80 >> (it % 8);
                    *(image1 + jt * (internal_cursor->cursor.bitmap.stride) +
                      (it >> 3)) &= ~(0x80 >> (it % 8));
                }
                break;
            case SDL_GF_MOUSE_COLOR_WHITE:
                {
                    *(image0 + jt * (internal_cursor->cursor.bitmap.stride) +
                      (it >> 3)) &= ~(0x80 >> (it % 8));
                    *(image1 + jt * (internal_cursor->cursor.bitmap.stride) +
                      (it >> 3)) |= 0x80 >> (it % 8);
                }
                break;
            case SDL_GF_MOUSE_COLOR_TRANS:
                {
                    *(image0 + jt * (internal_cursor->cursor.bitmap.stride) +
                      (it >> 3)) &= ~(0x80 >> (it % 8));
                    *(image1 + jt * (internal_cursor->cursor.bitmap.stride) +
                      (it >> 3)) &= ~(0x80 >> (it % 8));
                }
                break;
            default:
                {
                    /* The same as transparent color, must not happen */
                    *(image0 + jt * (internal_cursor->cursor.bitmap.stride) +
                      (it >> 3)) &= ~(0x80 >> (it % 8));
                    *(image1 + jt * (internal_cursor->cursor.bitmap.stride) +
                      (it >> 3)) &= ~(0x80 >> (it % 8));
                }
                break;
            }
        }
    }

    return sdl_cursor;
}

int
gf_showcursor(SDL_Cursor * cursor)
{
    SDL_VideoDisplay *display;
    SDL_DisplayData *didata;
    SDL_Window *window;
    SDL_WindowID window_id;
    gf_cursor_t *internal_cursor;
    int32_t status;

    /* Get current window id */
    window_id = SDL_GetFocusWindow();
    if (window_id <= 0) {
        SDL_MouseData *mdata = NULL;

        /* If there is no current window, then someone calls this function */
        /* to set global mouse settings during SDL initialization          */
        if (cursor != NULL) {
            mdata = (SDL_MouseData *) cursor->mouse->driverdata;
            didata = (SDL_DisplayData *) mdata->didata;
            if ((didata == NULL) || (mdata == NULL)) {
                return;
            }
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
                if (didata == NULL) {
                    return -1;
                }
            } else {
                return -1;
            }
        } else {
            return -1;
        }
    }

    /* Check if we need to set new shape or disable cursor shape */
    if (cursor != NULL) {
        /* Retrieve GF cursor shape */
        internal_cursor = (gf_cursor_t *) cursor->driverdata;
        if (internal_cursor == NULL) {
            SDL_SetError("GF: Internal cursor data is absent");
            return -1;
        }
        if ((internal_cursor->cursor.bitmap.image0 == NULL) ||
            (internal_cursor->cursor.bitmap.image1 == NULL)) {
            SDL_SetError("GF: Cursor shape is absent");
            return -1;
        }

        /* Store last shown cursor to display data */
        didata->cursor.type = internal_cursor->type;
        didata->cursor.hotspot.x = internal_cursor->hotspot.x;
        didata->cursor.hotspot.y = internal_cursor->hotspot.y;
        if (internal_cursor->cursor.bitmap.w > SDL_VIDEO_GF_MAX_CURSOR_SIZE) {
            didata->cursor.cursor.bitmap.w = SDL_VIDEO_GF_MAX_CURSOR_SIZE;
        } else {
            didata->cursor.cursor.bitmap.w = internal_cursor->cursor.bitmap.w;
        }

        if (didata->cursor.cursor.bitmap.h > SDL_VIDEO_GF_MAX_CURSOR_SIZE) {
            didata->cursor.cursor.bitmap.h = SDL_VIDEO_GF_MAX_CURSOR_SIZE;
        } else {
            didata->cursor.cursor.bitmap.h = internal_cursor->cursor.bitmap.h;
        }

        didata->cursor.cursor.bitmap.color0 =
            internal_cursor->cursor.bitmap.color0;
        didata->cursor.cursor.bitmap.color1 =
            internal_cursor->cursor.bitmap.color1;
        didata->cursor.cursor.bitmap.stride =
            internal_cursor->cursor.bitmap.stride;
        SDL_memcpy(didata->cursor.cursor.bitmap.image0,
                   internal_cursor->cursor.bitmap.image0,
                   ((internal_cursor->cursor.bitmap.w +
                     7) / (sizeof(uint8_t) * 8)) *
                   internal_cursor->cursor.bitmap.h);
        SDL_memcpy(didata->cursor.cursor.bitmap.image1,
                   internal_cursor->cursor.bitmap.image1,
                   ((internal_cursor->cursor.bitmap.w +
                     7) / (sizeof(uint8_t) * 8)) *
                   internal_cursor->cursor.bitmap.h);

        /* Setup cursor shape */
        status = gf_cursor_set(didata->display, 0, internal_cursor);
        if (status != GF_ERR_OK) {
            if (status != GF_ERR_NOSUPPORT) {
                SDL_SetError("GF: Can't set hardware cursor shape");
                return -1;
            }
        }

        /* Enable just set cursor */
        status = gf_cursor_enable(didata->display, 0);
        if (status != GF_ERR_OK) {
            if (status != GF_ERR_NOSUPPORT) {
                SDL_SetError("GF: Can't enable hardware cursor");
                return -1;
            }
        }

        /* Set cursor visible */
        didata->cursor_visible = SDL_TRUE;
    } else {
        /* SDL requests to disable cursor */
        status = gf_cursor_disable(didata->display, 0);
        if (status != GF_ERR_OK) {
            if (status != GF_ERR_NOSUPPORT) {
                SDL_SetError("GF: Can't disable hardware cursor");
                return -1;
            }
        }

        /* Set cursor invisible */
        didata->cursor_visible = SDL_FALSE;
    }

    /* New cursor shape is set */
    return 0;
}

void
gf_movecursor(SDL_Cursor * cursor)
{
    SDL_VideoDisplay *display;
    SDL_DisplayData *didata;
    SDL_Window *window;
    SDL_WindowID window_id;
    int32_t status;
    uint32_t xmax;
    uint32_t ymax;

    /* Get current window id */
    window_id = SDL_GetFocusWindow();
    if (window_id <= 0) {
        didata = (SDL_DisplayData *) cursor->mouse->driverdata;
        if (didata == NULL) {
            return;
        }
    } else {
        /* Sanity checks */
        window = SDL_GetWindowFromID(window_id);
        if (window != NULL) {
            display = SDL_GetDisplayFromWindow(window);
            if (display != NULL) {
                didata = (SDL_DisplayData *) display->driverdata;
                if (didata == NULL) {
                    return;
                }
            } else {
                return;
            }
        } else {
            return;
        }
    }

    /* Add checks for out of screen bounds position */
    if (cursor->mouse->x < 0) {
        cursor->mouse->x = 0;
    }
    if (cursor->mouse->y < 0) {
        cursor->mouse->y = 0;
    }

    /* Get window size to clamp maximum coordinates */
    SDL_GetWindowSize(window_id, &xmax, &ymax);
    if (cursor->mouse->x >= xmax) {
        cursor->mouse->x = xmax - 1;
    }
    if (cursor->mouse->y >= ymax) {
        cursor->mouse->y = ymax - 1;
    }

    status =
        gf_cursor_set_pos(didata->display, 0, cursor->mouse->x,
                          cursor->mouse->y);
    if (status != GF_ERR_OK) {
        if (status != GF_ERR_NOSUPPORT) {
            SDL_SetError("GF: Can't set hardware cursor position");
            return;
        }
    }
}

void
gf_freecursor(SDL_Cursor * cursor)
{
    gf_cursor_t *internal_cursor;

    if (cursor != NULL) {
        internal_cursor = (gf_cursor_t *) cursor->driverdata;
        if (internal_cursor != NULL) {
            if (internal_cursor->cursor.bitmap.image0 != NULL) {
                SDL_free((uint8_t *) internal_cursor->cursor.bitmap.image0);
            }
            if (internal_cursor->cursor.bitmap.image1 != NULL) {
                SDL_free((uint8_t *) internal_cursor->cursor.bitmap.image1);
            }
            SDL_free(internal_cursor);
        }
    }
}

void
gf_warpmouse(SDL_Mouse * mouse, SDL_WindowID windowID, int x, int y)
{
    SDL_VideoDisplay *display;
    SDL_DisplayData *didata;
    SDL_Window *window;
    uint32_t xmax;
    uint32_t ymax;
    int32_t status;

    /* Sanity checks */
    window = SDL_GetWindowFromID(windowID);
    if (window != NULL) {
        display = SDL_GetDisplayFromWindow(window);
        if (display != NULL) {
            didata = (SDL_DisplayData *) display->driverdata;
            if (didata == NULL) {
                return;
            }
        } else {
            return;
        }
    } else {
        return;
    }

    /* Add checks for out of screen bounds position */
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }

    /* Get window size to clamp maximum coordinates */
    SDL_GetWindowSize(windowID, &xmax, &ymax);
    if (x >= xmax) {
        x = xmax - 1;
    }
    if (y >= ymax) {
        y = ymax - 1;
    }

    status = gf_cursor_set_pos(didata->display, 0, x, y);
    if (status != GF_ERR_OK) {
        if (status != GF_ERR_NOSUPPORT) {
            SDL_SetError("GF: Can't set hardware cursor position");
            return;
        }
    }
}

void
gf_freemouse(SDL_Mouse * mouse)
{
    if (mouse->driverdata == NULL) {
        return;
    }

    /* Mouse framework doesn't deletes automatically our driverdata */
    SDL_free(mouse->driverdata);
    mouse->driverdata = NULL;

    return;
}

/*****************************************************************************/
/* HIDDI handlers code                                                       */
/*****************************************************************************/
static key_packet key_last_state[SDL_HIDDI_MAX_DEVICES];

static void
hiddi_keyboard_handler(uint32_t devno, uint8_t * report_data,
                       uint32_t report_len)
{
    key_packet *packet;
    uint32_t it;
    uint32_t jt;

    packet = (key_packet *) report_data;

    /* Check for special states */
    switch (report_len) {
    case 8:                    /* 8 bytes of report length */
        {
            for (it = 0; it < 6; it++) {
                /* Check for keyboard overflow, when it can't handle */
                /* many simultaneous pressed keys */
                if (packet->codes[it] == HIDDI_KEY_OVERFLOW) {
                    return;
                }
            }
        }
        break;
    default:
        {
            /* Do not process unknown reports */
            return;
        }
        break;
    }

    /* Check if modifier key was pressed */
    if (packet->modifiers != key_last_state[devno].modifiers) {
        if (((packet->modifiers & HIDDI_MKEY_LEFT_CTRL) ==
             HIDDI_MKEY_LEFT_CTRL)
            && (key_last_state[devno].modifiers & HIDDI_MKEY_LEFT_CTRL) == 0) {
            /* Left Ctrl key was pressed */
            SDL_SendKeyboardKey(0, SDL_PRESSED, SDL_SCANCODE_LCTRL);
        }
        if (((packet->modifiers & HIDDI_MKEY_LEFT_CTRL) == 0) &&
            (key_last_state[devno].modifiers & HIDDI_MKEY_LEFT_CTRL) ==
            HIDDI_MKEY_LEFT_CTRL) {
            /* Left Ctrl key was released */
            SDL_SendKeyboardKey(0, SDL_RELEASED, SDL_SCANCODE_LCTRL);
        }
        if (((packet->modifiers & HIDDI_MKEY_LEFT_SHIFT) ==
             HIDDI_MKEY_LEFT_SHIFT)
            && (key_last_state[devno].modifiers & HIDDI_MKEY_LEFT_SHIFT) == 0) {
            /* Left Shift key was pressed */
            SDL_SendKeyboardKey(0, SDL_PRESSED, SDL_SCANCODE_LSHIFT);
        }
        if (((packet->modifiers & HIDDI_MKEY_LEFT_SHIFT) == 0) &&
            (key_last_state[devno].modifiers & HIDDI_MKEY_LEFT_SHIFT) ==
            HIDDI_MKEY_LEFT_SHIFT) {
            /* Left Shift key was released */
            SDL_SendKeyboardKey(0, SDL_RELEASED, SDL_SCANCODE_LSHIFT);
        }
        if (((packet->modifiers & HIDDI_MKEY_LEFT_ALT) == HIDDI_MKEY_LEFT_ALT)
            && (key_last_state[devno].modifiers & HIDDI_MKEY_LEFT_ALT) == 0) {
            /* Left Alt key was pressed */
            SDL_SendKeyboardKey(0, SDL_PRESSED, SDL_SCANCODE_LALT);
        }
        if (((packet->modifiers & HIDDI_MKEY_LEFT_ALT) == 0) &&
            (key_last_state[devno].modifiers & HIDDI_MKEY_LEFT_ALT) ==
            HIDDI_MKEY_LEFT_ALT) {
            /* Left Alt key was released */
            SDL_SendKeyboardKey(0, SDL_RELEASED, SDL_SCANCODE_LALT);
        }
        if (((packet->modifiers & HIDDI_MKEY_LEFT_WFLAG) ==
             HIDDI_MKEY_LEFT_WFLAG)
            && (key_last_state[devno].modifiers & HIDDI_MKEY_LEFT_WFLAG) == 0) {
            /* Left Windows flag key was pressed */
            SDL_SendKeyboardKey(0, SDL_PRESSED, SDL_SCANCODE_LGUI);
        }
        if (((packet->modifiers & HIDDI_MKEY_LEFT_WFLAG) == 0) &&
            (key_last_state[devno].modifiers & HIDDI_MKEY_LEFT_WFLAG) ==
            HIDDI_MKEY_LEFT_WFLAG) {
            /* Left Windows flag key was released */
            SDL_SendKeyboardKey(0, SDL_RELEASED, SDL_SCANCODE_LGUI);
        }
        if (((packet->modifiers & HIDDI_MKEY_RIGHT_CTRL) ==
             HIDDI_MKEY_RIGHT_CTRL)
            && (key_last_state[devno].modifiers & HIDDI_MKEY_RIGHT_CTRL) == 0) {
            /* Right Ctrl key was pressed */
            SDL_SendKeyboardKey(0, SDL_PRESSED, SDL_SCANCODE_RCTRL);
        }
        if (((packet->modifiers & HIDDI_MKEY_RIGHT_CTRL) == 0) &&
            (key_last_state[devno].modifiers & HIDDI_MKEY_RIGHT_CTRL) ==
            HIDDI_MKEY_RIGHT_CTRL) {
            /* Right Ctrl key was released */
            SDL_SendKeyboardKey(0, SDL_RELEASED, SDL_SCANCODE_RCTRL);
        }
        if (((packet->modifiers & HIDDI_MKEY_RIGHT_SHIFT) ==
             HIDDI_MKEY_RIGHT_SHIFT)
            && (key_last_state[devno].modifiers & HIDDI_MKEY_RIGHT_SHIFT) ==
            0) {
            /* Right Shift key was pressed */
            SDL_SendKeyboardKey(0, SDL_PRESSED, SDL_SCANCODE_RSHIFT);
        }
        if (((packet->modifiers & HIDDI_MKEY_RIGHT_SHIFT) == 0) &&
            (key_last_state[devno].modifiers & HIDDI_MKEY_RIGHT_SHIFT) ==
            HIDDI_MKEY_RIGHT_SHIFT) {
            /* Right Shift key was released */
            SDL_SendKeyboardKey(0, SDL_RELEASED, SDL_SCANCODE_RSHIFT);
        }
        if (((packet->modifiers & HIDDI_MKEY_RIGHT_ALT) ==
             HIDDI_MKEY_RIGHT_ALT)
            && (key_last_state[devno].modifiers & HIDDI_MKEY_RIGHT_ALT) == 0) {
            /* Right Alt key was pressed */
            SDL_SendKeyboardKey(0, SDL_PRESSED, SDL_SCANCODE_RALT);
        }
        if (((packet->modifiers & HIDDI_MKEY_RIGHT_ALT) == 0) &&
            (key_last_state[devno].modifiers & HIDDI_MKEY_RIGHT_ALT) ==
            HIDDI_MKEY_RIGHT_ALT) {
            /* Right Alt key was released */
            SDL_SendKeyboardKey(0, SDL_RELEASED, SDL_SCANCODE_RALT);
        }
        if (((packet->modifiers & HIDDI_MKEY_RIGHT_WFLAG) ==
             HIDDI_MKEY_RIGHT_WFLAG)
            && (key_last_state[devno].modifiers & HIDDI_MKEY_RIGHT_WFLAG) ==
            0) {
            /* Right Windows flag key was pressed */
            SDL_SendKeyboardKey(0, SDL_PRESSED, SDL_SCANCODE_RGUI);
        }
        if (((packet->modifiers & HIDDI_MKEY_RIGHT_WFLAG) == 0) &&
            (key_last_state[devno].modifiers & HIDDI_MKEY_RIGHT_WFLAG) ==
            HIDDI_MKEY_RIGHT_WFLAG) {
            /* Right Windows flag key was released */
            SDL_SendKeyboardKey(0, SDL_RELEASED, SDL_SCANCODE_RGUI);
        }
    }

    /* Check each key in the press/release buffer */
    switch (report_len) {
    case 8:                    /* 8 bytes of report length */
        {
            /* Check if at least one key was unpressed */
            for (it = 0; it < 6; it++) {
                if (key_last_state[devno].codes[it] == HIDDI_KEY_UNPRESSED) {
                    /* if stored keycode is zero, find another */
                    continue;
                }
                for (jt = 0; jt < 6; jt++) {
                    /* Find stored keycode in the current pressed codes */
                    if (packet->codes[jt] == key_last_state[devno].codes[it]) {
                        /* If found then particular key state is not changed */
                        break;
                    }
                }

                /* Check if pressed key can't longer be found */
                if (jt == 6) {
                    SDL_SendKeyboardKey(0, SDL_RELEASED,
                                        key_last_state[devno].codes[it]);
                }
            }

            /* Check if at least one new key was pressed */
            for (it = 0; it < 6; it++) {
                if (packet->codes[it] == HIDDI_KEY_UNPRESSED) {
                    continue;
                }
                for (jt = 0; jt < 6; jt++) {
                    /* Find new keycode it the array of old pressed keys */
                    if (packet->codes[it] == key_last_state[devno].codes[jt]) {
                        break;
                    }
                }

                /* Check if new key was pressed */
                if (jt == 6) {
                    SDL_SendKeyboardKey(0, SDL_PRESSED, packet->codes[it]);
                }
            }
        }
    default:                   /* unknown keyboard report type */
        {
            /* Ignore all unknown reports */
        }
        break;
    }

    /* Store last state */
    key_last_state[devno] = *packet;
}

static uint32_t mouse_last_state_button[SDL_HIDDI_MAX_DEVICES];
static uint32_t collect_reports = 0;

static void
hiddi_mouse_handler(uint32_t devno, uint8_t * report_data,
                    uint32_t report_len)
{
    uint32_t it;
    uint32_t sdlbutton;

    /* We do not want to collect stored events */
    if (collect_reports == 0) {
        return;
    }

    /* Check for special states */
    switch (report_len) {
    case 8:                    /* 8 bytes of report length, usually multi-button USB mice */
        {
            mouse_packet8 *packet;
            packet = (mouse_packet8 *) report_data;

            /* Send motion event if motion really was */
            if ((packet->horizontal_precision != 0)
                || (packet->vertical_precision != 0)) {
                SDL_SendMouseMotion(0, 1, packet->horizontal_precision,
                                    packet->vertical_precision, 0);
            }

            /* Send mouse button press/release events */
            if (mouse_last_state_button[devno] != packet->buttons) {
                /* Cycle all buttons status */
                for (it = 0; it < 8; it++) {
                    /* convert hiddi button id to sdl button id */
                    switch (it) {
                    case 0:
                        {
                            sdlbutton = SDL_BUTTON_LEFT;
                        }
                        break;
                    case 1:
                        {
                            sdlbutton = SDL_BUTTON_RIGHT;
                        }
                        break;
                    case 2:
                        {
                            sdlbutton = SDL_BUTTON_MIDDLE;
                        }
                        break;
                    default:
                        {
                            sdlbutton = it + 1;
                        }
                        break;
                    }

                    /* Button pressed */
                    if (((packet->buttons & (0x01 << it)) == (0x01 << it)) &&
                        ((mouse_last_state_button[devno] & (0x01 << it)) ==
                         0x00)) {
                        SDL_SendMouseButton(0, SDL_PRESSED, sdlbutton);
                    }
                    /* Button released */
                    if (((packet->buttons & (0x01 << it)) == 0x00) &&
                        ((mouse_last_state_button[devno] & (0x01 << it)) ==
                         (0x01 << it))) {
                        SDL_SendMouseButton(0, SDL_RELEASED, sdlbutton);
                    }
                }
                mouse_last_state_button[devno] = packet->buttons;
            }

            /* Send mouse wheel events */
            if (packet->wheel != 0) {
                /* Send vertical wheel event only */
                SDL_SendMouseWheel(0, 0, packet->wheel);
            }
        }
        break;
    case 4:                    /* 4 bytes of report length, usually PS/2 mice */
        {
            mouse_packet4 *packet;
            packet = (mouse_packet4 *) report_data;

            /* Send motion event if motion really was */
            if ((packet->horizontal != 0) || (packet->vertical != 0)) {
                SDL_SendMouseMotion(0, 1, packet->horizontal,
                                    packet->vertical, 0);
            }

            /* Send mouse button press/release events */
            if (mouse_last_state_button[devno] != packet->buttons) {
                /* Cycle all buttons status */
                for (it = 0; it < 8; it++) {
                    /* convert hiddi button id to sdl button id */
                    switch (it) {
                    case 0:
                        {
                            sdlbutton = SDL_BUTTON_LEFT;
                        }
                        break;
                    case 1:
                        {
                            sdlbutton = SDL_BUTTON_RIGHT;
                        }
                        break;
                    case 2:
                        {
                            sdlbutton = SDL_BUTTON_MIDDLE;
                        }
                        break;
                    default:
                        {
                            sdlbutton = it + 1;
                        }
                        break;
                    }

                    /* Button pressed */
                    if (((packet->buttons & (0x01 << it)) == (0x01 << it)) &&
                        ((mouse_last_state_button[devno] & (0x01 << it)) ==
                         0x00)) {
                        SDL_SendMouseButton(0, SDL_PRESSED, sdlbutton);
                    }
                    /* Button released */
                    if (((packet->buttons & (0x01 << it)) == 0x00) &&
                        ((mouse_last_state_button[devno] & (0x01 << it)) ==
                         (0x01 << it))) {
                        SDL_SendMouseButton(0, SDL_RELEASED, sdlbutton);
                    }
                }
                mouse_last_state_button[devno] = packet->buttons;
            }

            /* Send mouse wheel events */
            if (packet->wheel != 0) {
                /* Send vertical wheel event only */
                SDL_SendMouseWheel(0, 0, packet->wheel);
            }
        }
        break;
    }
}

/*****************************************************************************/
/* HIDDI interacting code                                                    */
/*****************************************************************************/
static hidd_device_ident_t hiddevice = {
    HIDD_CONNECT_WILDCARD,      /* vendor id:  any */
    HIDD_CONNECT_WILDCARD,      /* product id: any */
    HIDD_CONNECT_WILDCARD,      /* version:    any */
};

static hidd_connect_parm_t hidparams =
    { NULL, HID_VERSION, HIDD_VERSION, 0, 0, &hiddevice, NULL, 0 };

static void hiddi_insertion(struct hidd_connection *connection,
                            hidd_device_instance_t * device_instance);
static void hiddi_removal(struct hidd_connection *connection,
                          hidd_device_instance_t * instance);
static void hiddi_report(struct hidd_connection *connection,
                         struct hidd_report *report, void *report_data,
                         uint32_t report_len, uint32_t flags, void *user);

static hidd_funcs_t hidfuncs =
    { _HIDDI_NFUNCS, hiddi_insertion, hiddi_removal, hiddi_report, NULL };

/* HID handle, singletone */
struct hidd_connection *connection = NULL;

/* SDL detected input device types, singletone */
static uint32_t sdl_input_devices[SDL_HIDDI_MAX_DEVICES];

static int
hiddi_register_for_reports(struct hidd_collection *col,
                           hidd_device_instance_t * device_instance)
{
    int it;
    uint16_t num_col;
    struct hidd_collection **hidd_collections;
    struct hidd_report_instance *report_instance;
    struct hidd_report *report;
    int status = 0;
    hidview_device_t *device;

    for (it = 0; it < 10 && !status; it++) {
        status =
            hidd_get_report_instance(col, it, HID_INPUT_REPORT,
                                     &report_instance);
        if (status == EOK) {
            status =
                hidd_report_attach(connection, device_instance,
                                   report_instance, 0,
                                   sizeof(hidview_device_t), &report);
            if (status == EOK) {
                device = hidd_report_extra(report);
                device->report = report;
                device->instance = report_instance;
            }
        }
    }
    hidd_get_collections(NULL, col, &hidd_collections, &num_col);

    for (it = 0; it < num_col; it++) {
        hiddi_register_for_reports(hidd_collections[it], device_instance);
    }

    return EOK;
}

static void
hiddi_insertion(struct hidd_connection *connection,
                hidd_device_instance_t * device_instance)
{
    uint32_t it;
    struct hidd_collection **hidd_collections;
    uint16_t num_col;

    /* get root level collections */
    hidd_get_collections(device_instance, NULL, &hidd_collections, &num_col);
    for (it = 0; it < num_col; it++) {
        hiddi_register_for_reports(hidd_collections[it], device_instance);
    }
}

static void
hiddi_removal(struct hidd_connection *connection,
              hidd_device_instance_t * instance)
{
    hidd_reports_detach(connection, instance);
}

static void
hiddi_report(struct hidd_connection *connection, hidd_report_t * report,
             void *report_data, uint32_t report_len, uint32_t flags,
             void *user)
{
    if (report->dev_inst->devno >= SDL_HIDDI_MAX_DEVICES) {
        /* Unknown HID device, with devno number out of supported range */
        return;
    }

    /* Check device type which generates event */
    switch (sdl_input_devices[report->dev_inst->devno]) {
    case SDL_GF_HIDDI_NONE:
        {
            /* We do not handle other devices type */
            return;
        }
        break;
    case SDL_GF_HIDDI_MOUSE:
        {
            /* Call mouse handler */
            hiddi_mouse_handler(report->dev_inst->devno, report_data,
                                report_len);
        }
        break;
    case SDL_GF_HIDDI_KEYBOARD:
        {
            /* Call keyboard handler */
            hiddi_keyboard_handler(report->dev_inst->devno, report_data,
                                   report_len);
        }
        break;
    case SDL_GF_HIDDI_JOYSTICK:
        {
            /* Call joystick handler */
        }
        break;
    }
}

static
hiddi_get_device_type(uint8_t * report_data, uint16_t report_length)
{
    hid_byte_t byte;
    uint16_t usage_page = 0;
    uint16_t usage = 0;
    uint16_t data = 0;

    while (report_length && !(usage_page && usage)) {
        if (hidp_analyse_byte(*report_data, &byte)) {
            /* Error in parser, do nothing */
        }
        data = hidp_get_data((report_data + 1), &byte);
        switch (byte.HIDB_Type) {
        case HID_TYPE_GLOBAL:
            if (!usage_page && byte.HIDB_Tag == HID_GLOBAL_USAGE_PAGE) {
                usage_page = data;
            }
            break;
        case HID_TYPE_LOCAL:
            if (!usage && byte.HIDB_Tag == HID_LOCAL_USAGE) {
                usage = data;
            }
            break;
        }
        report_data += byte.HIDB_Length + 1;
        report_length -= byte.HIDB_Length + 1;
    }

    switch (usage_page) {
    case HIDD_PAGE_DESKTOP:
        {
            switch (usage) {
            case HIDD_USAGE_MOUSE:
                {
                    return SDL_GF_HIDDI_MOUSE;
                }
                break;
            case HIDD_USAGE_JOYSTICK:
                {
                    return SDL_GF_HIDDI_JOYSTICK;
                }
                break;
            case HIDD_USAGE_KEYBOARD:
                {
                    return SDL_GF_HIDDI_KEYBOARD;
                }
                break;
            }
        }
        break;
    case HIDD_PAGE_DIGITIZER:
        {
            /* Do not handle digitizers */
        }
        break;
    case HIDD_PAGE_CONSUMER:
        {
            /* Do not handle consumer input devices */
        }
        break;
    }

    return SDL_GF_HIDDI_NONE;
}

static int32_t
hiddi_connect_devices()
{
    int32_t status;
    uint32_t it;
    uint8_t *report_data;
    uint16_t report_length;
    hidd_device_instance_t instance;

    /* Cleanup initial keys and mice state */
    SDL_memset(key_last_state, 0x00,
               sizeof(key_packet) * SDL_HIDDI_MAX_DEVICES);
    SDL_memset(mouse_last_state_button, 0x00,
               sizeof(uint32_t) * SDL_HIDDI_MAX_DEVICES);

    status = hidd_connect(&hidparams, &connection);
    if (status != EOK) {
        return -1;
    }

    for (it = 0; it < SDL_HIDDI_MAX_DEVICES; it++) {
        /* Get device instance */
        status = hidd_get_device_instance(connection, it, &instance);
        if (status != EOK) {
            continue;
        }

        status =
            hidd_get_report_desc(connection, &instance, &report_data,
                                 &report_length);
        if (status != EOK) {
            continue;
        }

        status = hiddi_get_device_type(report_data, report_length);
        sdl_input_devices[it] = status;

        free(report_data);
    }

    /* Disconnect from HID server */
    status = hidd_disconnect(connection);
    if (status != EOK) {
        return -1;
    }

    /* Add handlers for HID devices */
    hidparams.funcs = &hidfuncs;

    status = hidd_connect(&hidparams, &connection);
    if (status != EOK) {
        return -1;
    }

    return 0;
}

static int32_t
hiddi_disconnect_devices()
{
    int32_t status;

    hiddi_disable_mouse();

    /* Disconnect from HID server */
    status = hidd_disconnect(connection);
    if (status != EOK) {
        return -1;
    }
}

void
hiddi_enable_mouse()
{
    collect_reports = 1;
}

void
hiddi_disable_mouse()
{
    collect_reports = 0;
}
