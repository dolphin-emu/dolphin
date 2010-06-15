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
    slouken@libsdl.org
*/
#include "SDL_config.h"

/*
	Turbo veille screensaver

	Patrice Mandin
*/

#include <mint/cookie.h>

#include "SDL_xbios.h"
#include "SDL_xbios_tveille.h"

static tveille_t *cookie_veil;
static int status;

int SDL_XBIOS_TveillePresent(_THIS)
{
	return (Getcookie(C_VeiL, (unsigned long *)&cookie_veil) == C_FOUND);
}

void SDL_XBIOS_TveilleDisable(_THIS)
{
	status = cookie_veil->enabled;
	cookie_veil->enabled = 0xff;
}

void SDL_XBIOS_TveilleRestore(_THIS)
{
	cookie_veil->enabled = status;
}
