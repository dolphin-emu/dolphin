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

#ifndef __SDL_HIDDI_KEYBOARD_H__
#define __SDL_HIDDI_KEYBOARD_H__

#include <inttypes.h>

/* PS/2 and USB keyboards are generating this packet */
typedef struct key_packet
{
    uint8_t modifiers;
    uint8_t data;
    uint8_t codes[6];
} key_packet;

/* Key modifier codes */
#define HIDDI_MKEY_LEFT_CTRL            0x00000001
#define HIDDI_MKEY_LEFT_SHIFT           0x00000002
#define HIDDI_MKEY_LEFT_ALT             0x00000004
#define HIDDI_MKEY_LEFT_WFLAG           0x00000008
#define HIDDI_MKEY_RIGHT_CTRL           0x00000010
#define HIDDI_MKEY_RIGHT_SHIFT          0x00000020
#define HIDDI_MKEY_RIGHT_ALT            0x00000040
#define HIDDI_MKEY_RIGHT_WFLAG          0x00000080

/* Key codes */
#define HIDDI_KEY_UNPRESSED             0x00000000
#define HIDDI_KEY_OVERFLOW              0x00000001
#define HIDDI_KEY_ESC                   0x00000029
#define HIDDI_KEY_F1                    0x0000003A
#define HIDDI_KEY_F2                    0x0000003B
#define HIDDI_KEY_F3                    0x0000003C
#define HIDDI_KEY_F4                    0x0000003D
#define HIDDI_KEY_F5                    0x0000003E
#define HIDDI_KEY_F6                    0x0000003F
#define HIDDI_KEY_F7                    0x00000040
#define HIDDI_KEY_F8                    0x00000041
#define HIDDI_KEY_F9                    0x00000042
#define HIDDI_KEY_F10                   0x00000043
#define HIDDI_KEY_F11                   0x00000044
#define HIDDI_KEY_F12                   0x00000045

#define HIDDI_KEY_BACKQUOTE             0x00000035
#define HIDDI_KEY_1                     0x0000001E
#define HIDDI_KEY_2                     0x0000001F
#define HIDDI_KEY_3                     0x00000020
#define HIDDI_KEY_4                     0x00000021
#define HIDDI_KEY_5                     0x00000022
#define HIDDI_KEY_6                     0x00000023
#define HIDDI_KEY_7                     0x00000024
#define HIDDI_KEY_8                     0x00000025
#define HIDDI_KEY_9                     0x00000026
#define HIDDI_KEY_0                     0x00000027
#define HIDDI_KEY_MINUS                 0x0000002D
#define HIDDI_KEY_EQUAL                 0x0000002E
#define HIDDI_KEY_BACKSPACE             0x0000002A

#define HIDDI_KEY_TAB                   0x0000002B
#define HIDDI_KEY_Q                     0x00000014
#define HIDDI_KEY_W                     0x0000001A
#define HIDDI_KEY_E                     0x00000008
#define HIDDI_KEY_R                     0x00000015
#define HIDDI_KEY_T                     0x00000017
#define HIDDI_KEY_Y                     0x0000001C
#define HIDDI_KEY_U                     0x00000018
#define HIDDI_KEY_I                     0x0000000C
#define HIDDI_KEY_O                     0x00000012
#define HIDDI_KEY_P                     0x00000013
#define HIDDI_KEY_LEFT_SQ_BRACKET       0x0000002F
#define HIDDI_KEY_RIGHT_SQ_BRACKET      0x00000030
#define HIDDI_KEY_BACKSLASH             0x00000031

#define HIDDI_KEY_CAPSLOCK              0x00000039
#define HIDDI_KEY_A                     0x00000004
#define HIDDI_KEY_S                     0x00000016
#define HIDDI_KEY_D                     0x00000007
#define HIDDI_KEY_F                     0x00000009
#define HIDDI_KEY_G                     0x0000000A
#define HIDDI_KEY_H                     0x0000000B
#define HIDDI_KEY_J                     0x0000000D
#define HIDDI_KEY_K                     0x0000000E
#define HIDDI_KEY_L                     0x0000000F
#define HIDDI_KEY_SEMICOLON             0x00000033
#define HIDDI_KEY_QUOTE                 0x00000034
#define HIDDI_KEY_ENTER                 0x00000028

#define HIDDI_KEY_Z                     0x0000001D
#define HIDDI_KEY_X                     0x0000001B
#define HIDDI_KEY_C                     0x00000006
#define HIDDI_KEY_V                     0x00000019
#define HIDDI_KEY_B                     0x00000005
#define HIDDI_KEY_N                     0x00000011
#define HIDDI_KEY_M                     0x00000010
#define HIDDI_KEY_COMMA                 0x00000036
#define HIDDI_KEY_POINT                 0x00000037
#define HIDDI_KEY_SLASH                 0x00000038

#define HIDDI_KEY_SPACE                 0x0000002C
#define HIDDI_KEY_MENU                  0x00000065

#define HIDDI_KEY_PRINTSCREEN           0x00000046
#define HIDDI_KEY_SCROLLLOCK            0x00000047
#define HIDDI_KEY_PAUSE                 0x00000048

#define HIDDI_KEY_INSERT                0x00000049
#define HIDDI_KEY_HOME                  0x0000004A
#define HIDDI_KEY_PAGEUP                0x0000004B
#define HIDDI_KEY_DELETE                0x0000004C
#define HIDDI_KEY_END                   0x0000004D
#define HIDDI_KEY_PAGEDOWN              0x0000004E

#define HIDDI_KEY_UP                    0x00000052
#define HIDDI_KEY_LEFT                  0x00000050
#define HIDDI_KEY_DOWN                  0x00000051
#define HIDDI_KEY_RIGHT                 0x0000004F

#define HIDDI_KEY_NUMLOCK               0x00000053
#define HIDDI_KEY_GR_SLASH              0x00000054
#define HIDDI_KEY_GR_ASTERISK           0x00000055
#define HIDDI_KEY_GR_MINUS              0x00000056
#define HIDDI_KEY_GR_7                  0x0000005F
#define HIDDI_KEY_GR_8                  0x00000060
#define HIDDI_KEY_GR_9                  0x00000061
#define HIDDI_KEY_GR_PLUS               0x00000057
#define HIDDI_KEY_GR_4                  0x0000005C
#define HIDDI_KEY_GR_5                  0x0000005D
#define HIDDI_KEY_GR_6                  0x0000005E
#define HIDDI_KEY_GR_1                  0x00000059
#define HIDDI_KEY_GR_2                  0x0000005A
#define HIDDI_KEY_GR_3                  0x0000005B
#define HIDDI_KEY_GR_ENTER              0x00000058
#define HIDDI_KEY_GR_0                  0x00000062
#define HIDDI_KEY_GR_DELETE             0x00000063

#endif /* __SDL_HIDDI_KEYBOARD_H__ */
