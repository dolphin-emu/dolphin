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

#include "SDL_win32video.h"

#include "../../events/SDL_keyboard_c.h"
#include "../../events/scancodes_win32.h"

#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC     0
#endif
#ifndef MAPVK_VSC_TO_VK
#define MAPVK_VSC_TO_VK     1
#endif
#ifndef MAPVK_VK_TO_CHAR
#define MAPVK_VK_TO_CHAR    2
#endif

/* Alphabetic scancodes for PC keyboards */
BYTE alpha_scancodes[26] = {
    30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38, 50, 49, 24,
    25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44
};

BYTE keypad_scancodes[10] = {
    82, 79, 80, 81, 75, 76, 77, 71, 72, 73
};

void
WIN_InitKeyboard(_THIS)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
    SDL_Keyboard keyboard;
    int i;

    /* Make sure the alpha scancodes are correct.  T isn't usually remapped */
    if (MapVirtualKey('T', MAPVK_VK_TO_VSC) != alpha_scancodes['T' - 'A']) {
#if 0
        printf
            ("Fixing alpha scancode map, assuming US QWERTY layout!\nPlease send the following 26 lines of output to the SDL mailing list <sdl@libsdl.org>, including a description of your keyboard hardware.\n");
#endif
        for (i = 0; i < SDL_arraysize(alpha_scancodes); ++i) {
            alpha_scancodes[i] = MapVirtualKey('A' + i, MAPVK_VK_TO_VSC);
#if 0
            printf("%d = %d\n", i, alpha_scancodes[i]);
#endif
        }
    }
    if (MapVirtualKey(VK_NUMPAD0, MAPVK_VK_TO_VSC) != keypad_scancodes[0]) {
#if 0
        printf
            ("Fixing keypad scancode map!\nPlease send the following 10 lines of output to the SDL mailing list <sdl@libsdl.org>, including a description of your keyboard hardware.\n");
#endif
        for (i = 0; i < SDL_arraysize(keypad_scancodes); ++i) {
            keypad_scancodes[i] =
                MapVirtualKey(VK_NUMPAD0 + i, MAPVK_VK_TO_VSC);
#if 0
            printf("%d = %d\n", i, keypad_scancodes[i]);
#endif
        }
    }

    data->key_layout = win32_scancode_table;

    SDL_zero(keyboard);
    data->keyboard = SDL_AddKeyboard(&keyboard, -1);
    WIN_UpdateKeymap(data->keyboard);

    SDL_SetScancodeName(SDL_SCANCODE_APPLICATION, "Menu");
    SDL_SetScancodeName(SDL_SCANCODE_LGUI, "Left Windows");
    SDL_SetScancodeName(SDL_SCANCODE_RGUI, "Right Windows");
}

void
WIN_UpdateKeymap(int keyboard)
{
    int i;
    SDL_scancode scancode;
    SDLKey keymap[SDL_NUM_SCANCODES];

    SDL_GetDefaultKeymap(keymap);

    for (i = 0; i < SDL_arraysize(win32_scancode_table); i++) {

        /* Make sure this scancode is a valid character scancode */
        scancode = win32_scancode_table[i];
        if (scancode == SDL_SCANCODE_UNKNOWN ||
            (keymap[scancode] & SDLK_SCANCODE_MASK)) {
            continue;
        }

        /* Alphabetic keys are handled specially, since Windows remaps them */
        if (i >= 'A' && i <= 'Z') {
            BYTE vsc = alpha_scancodes[i - 'A'];
            keymap[scancode] = MapVirtualKey(vsc, MAPVK_VSC_TO_VK) + 0x20;
        } else {
            keymap[scancode] = (MapVirtualKey(i, MAPVK_VK_TO_CHAR) & 0x7FFF);
        }
    }
    SDL_SetKeymap(keyboard, 0, keymap, SDL_NUM_SCANCODES);
}

void
WIN_QuitKeyboard(_THIS)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;

    SDL_DelKeyboard(data->keyboard);
}

/* vi: set ts=4 sw=4 expandtab: */
