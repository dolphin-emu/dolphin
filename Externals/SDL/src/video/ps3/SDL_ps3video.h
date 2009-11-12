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

#ifndef _SDL_ps3video_h
#define _SDL_ps3video_h

#include "../SDL_sysvideo.h"
#include "SDL_ps3spe_c.h"

#include <linux/fb.h>
#include <asm/ps3fb.h>

/* Debugging
 * 0: No debug messages
 * 1: Video debug messages
 * 2: SPE debug messages
 * 3: Memory adresses
 */
#define DEBUG_LEVEL 0

#ifdef DEBUG_LEVEL
#define deprintf( level, fmt, args... ) \
    do \
{ \
    if ( (unsigned)(level) <= DEBUG_LEVEL ) \
    { \
        fprintf( stdout, fmt, ##args ); \
        fflush( stdout ); \
    } \
} while ( 0 )
#else
#define deprintf( level, fmt, args... )
#endif

/* Default framebuffer device on PS3 */
#define PS3DEV "/dev/fb0"

/* Private display data */
typedef struct SDL_VideoData
{
    /* Framebuffer device descriptor */
    int fbdev;
    /* mmap'd access to fbdev */
    uint8_t * frame_buffer;
    /* SPE threading stuff of the framebuffer */
    spu_data_t * fb_thread_data;
    /* Framebuffer transfer data */
    volatile struct fb_writer_parms_t * fb_parms __attribute__((aligned(128)));
} SDL_VideoData;

typedef struct SDL_DisplayModeData
{
    unsigned long mode;
    //struct ps3fb_ioctl_res res;
} PS3_DisplayModeData;

#endif /* _SDL_ps3video_h */

/* vi: set ts=4 sw=4 expandtab: */
