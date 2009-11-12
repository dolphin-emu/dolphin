/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2009 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    slouken@devolution.com
*/

#ifdef SAVE_RCSID
static char rcsid =
    "@(#) $Id: SDL_systhread.c,v 1.2 2001/04/26 16:50:18 hercules Exp $";
#endif

/* Thread management routines for SDL */

#include "SDL_error.h"
#include "SDL_thread.h"
#include "SDL_systhread.h"

int
SDL_SYS_CreateThread(SDL_Thread * thread, void *args)
{
    SDL_SetError("Threads are not supported on this platform");
    return (-1);
}

void
SDL_SYS_SetupThread(void)
{
    return;
}

Uint32
SDL_ThreadID(void)
{
    return (0);
}

void
SDL_SYS_WaitThread(SDL_Thread * thread)
{
    return;
}

void
SDL_SYS_KillThread(SDL_Thread * thread)
{
    return;
}
