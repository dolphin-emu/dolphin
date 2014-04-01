// Copyright (C) 2013 Scott Moreau <oreaus@gmail.com>
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>
#include <xkbcommon/xkbcommon.h>

#define MOD_SHIFT_MASK    0x01
#define MOD_ALT_MASK      0x02
#define MOD_CONTROL_MASK  0x04


class cWaylandInterface
{
public:
	bool ServerConnect(void);
	bool Initialize(void *config);
	void *EGLGetDisplay(void);
	void *CreateWindow(void);
	void DestroyWindow(void);
	void UpdateFPSDisplay(const std::string& text);
	void ToggleFullscreen(bool fullscreen);
	void SwapBuffers();
};
