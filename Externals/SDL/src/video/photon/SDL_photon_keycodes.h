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

#ifndef __SDL_PHOTON_KEYCODES_H__
#define __SDL_PHOTON_KEYCODES_H__

#define PHOTON_SCANCODE_ESCAPE      0x01
#define PHOTON_SCANCODE_F1          0x3B
#define PHOTON_SCANCODE_F2          0x3C
#define PHOTON_SCANCODE_F3          0x3D
#define PHOTON_SCANCODE_F4          0x3E
#define PHOTON_SCANCODE_F5          0x3F
#define PHOTON_SCANCODE_F6          0x40
#define PHOTON_SCANCODE_F7          0x41
#define PHOTON_SCANCODE_F8          0x42
#define PHOTON_SCANCODE_F9          0x43
#define PHOTON_SCANCODE_F10         0x44
#define PHOTON_SCANCODE_F11         0x57
#define PHOTON_SCANCODE_F12         0x58

#define PHOTON_SCANCODE_BACKQOUTE   0x29
#define PHOTON_SCANCODE_1           0x02
#define PHOTON_SCANCODE_2           0x03
#define PHOTON_SCANCODE_3           0x04
#define PHOTON_SCANCODE_4           0x05
#define PHOTON_SCANCODE_5           0x06
#define PHOTON_SCANCODE_6           0x07
#define PHOTON_SCANCODE_7           0x08
#define PHOTON_SCANCODE_8           0x09
#define PHOTON_SCANCODE_9           0x0A
#define PHOTON_SCANCODE_0           0x0B
#define PHOTON_SCANCODE_MINUS       0x0C
#define PHOTON_SCANCODE_EQUAL       0x0D
#define PHOTON_SCANCODE_BACKSPACE   0x0E

#define PHOTON_SCANCODE_TAB         0x0F
#define PHOTON_SCANCODE_Q           0x10
#define PHOTON_SCANCODE_W           0x11
#define PHOTON_SCANCODE_E           0x12
#define PHOTON_SCANCODE_R           0x13
#define PHOTON_SCANCODE_T           0x14
#define PHOTON_SCANCODE_Y           0x15
#define PHOTON_SCANCODE_U           0x16
#define PHOTON_SCANCODE_I           0x17
#define PHOTON_SCANCODE_O           0x18
#define PHOTON_SCANCODE_P           0x19
#define PHOTON_SCANCODE_LEFT_SQ_BR  0x1A
#define PHOTON_SCANCODE_RIGHT_SQ_BR 0x1B
#define PHOTON_SCANCODE_ENTER       0x1C

#define PHOTON_SCANCODE_CAPSLOCK    0x3A
#define PHOTON_SCANCODE_A           0x1E
#define PHOTON_SCANCODE_S           0x1F
#define PHOTON_SCANCODE_D           0x20
#define PHOTON_SCANCODE_F           0x21
#define PHOTON_SCANCODE_G           0x22
#define PHOTON_SCANCODE_H           0x23
#define PHOTON_SCANCODE_J           0x24
#define PHOTON_SCANCODE_K           0x25
#define PHOTON_SCANCODE_L           0x26
#define PHOTON_SCANCODE_SEMICOLON   0x27
#define PHOTON_SCANCODE_QUOTE       0x28
#define PHOTON_SCANCODE_BACKSLASH   0x2B

#define PHOTON_SCANCODE_LEFT_SHIFT  0x2A
#define PHOTON_SCANCODE_Z           0x2C
#define PHOTON_SCANCODE_X           0x2D
#define PHOTON_SCANCODE_C           0x2E
#define PHOTON_SCANCODE_V           0x2F
#define PHOTON_SCANCODE_B           0x30
#define PHOTON_SCANCODE_N           0x31
#define PHOTON_SCANCODE_M           0x32
#define PHOTON_SCANCODE_COMMA       0x33
#define PHOTON_SCANCODE_POINT       0x34
#define PHOTON_SCANCODE_SLASH       0x35
#define PHOTON_SCANCODE_RIGHT_SHIFT 0x36

#define PHOTON_SCANCODE_CTRL        0x1D
#define PHOTON_SCANCODE_WFLAG       0x5B
#define PHOTON_SCANCODE_ALT         0x38
#define PHOTON_SCANCODE_SPACE       0x39
#define PHOTON_SCANCODE_MENU        0x5D

#define PHOTON_SCANCODE_PRNSCR      0x54        /* only key pressed event, no release */
#define PHOTON_SCANCODE_SCROLLLOCK  0x46
#if 0                                           /* pause doesn't generates a scancode */
#define PHOTON_SCANCODE_PAUSE       0x??
#endif
#define PHOTON_SCANCODE_INSERT      0x52
#define PHOTON_SCANCODE_HOME        0x47
#define PHOTON_SCANCODE_PAGEUP      0x49
#define PHOTON_SCANCODE_DELETE      0x53
#define PHOTON_SCANCODE_END         0x4F
#define PHOTON_SCANCODE_PAGEDOWN    0x51
#define PHOTON_SCANCODE_UP          0x48
#define PHOTON_SCANCODE_DOWN        0x50
#define PHOTON_SCANCODE_LEFT        0x4B
#define PHOTON_SCANCODE_RIGHT       0x4D

#define PHOTON_SCANCODE_NUMLOCK     0x45

#endif /* __SDL_PHOTON_KEYCODES_H__ */
