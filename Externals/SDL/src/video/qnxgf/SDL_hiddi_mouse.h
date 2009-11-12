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

#ifndef __SDL_HIDDI_MOUSE_H__
#define __SDL_HIDDI_MOUSE_H__

#include <inttypes.h>

/* USB keyboard multimedia keys are generating this packet */
typedef struct mouse_packet2
{
    uint8_t buttons;
    int8_t wheel;
} mouse_packet2;

/* PS/2 mice are generating this packet */
typedef struct mouse_packet4
{
    uint8_t buttons;
    int8_t horizontal;
    int8_t vertical;
    int8_t wheel;
} mouse_packet4;

/* USB keyboard with mice wheel onboard generating this packet */
typedef struct mouse_packet5
{
    uint8_t buttons;
    int8_t horizontal;
    int8_t vertical;
    int8_t wheel;
    uint8_t state;
} mouse_packet5;

/* USB multi-button mice are generating this packet */
typedef struct mouse_packet8
{
    uint8_t buttons;
    int8_t horizontal;
    int8_t vertical;
    int8_t wheel;
    int16_t horizontal_precision;
    int16_t vertical_precision;
} mouse_packet8;

#endif /* __SDL_HIDDI_MOUSE_H__ */
