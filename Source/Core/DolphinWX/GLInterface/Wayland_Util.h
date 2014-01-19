// Copyright (C) 2013 Scott Moreau <oreaus@gmail.com>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
#ifndef _WAYLAND_UTIL_H_
#define _WAYLAND_UTIL_H_

#include <wayland-client.h>
#include <wayland-egl.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>

#define MOD_SHIFT_MASK		0x01
#define MOD_ALT_MASK		0x02
#define MOD_CONTROL_MASK	0x04


class cWaylandInterface
{
public:
	bool ServerConnect(void);
	bool Initialize(void *config);
	void *EGLGetDisplay(void);
	void *CreateWindow(void);
	void DestroyWindow(void);
	void UpdateFPSDisplay(const char *text);
	void ToggleFullscreen(bool fullscreen);
	void SwapBuffers();
};

#endif
