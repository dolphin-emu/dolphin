// Copyright (C) 2003 Dolphin Project.

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
#ifndef _INTERFACEEGL_H_
#define _INTERFACEEGL_H_

#include <EGL/egl.h>
#include "InterfaceBase.h"
#include "ConfigManager.h"


class cPlatform
{
private:
#if HAVE_X11
	cXInterface XInterface;
#endif
#if HAVE_WAYLAND
	cWaylandInterface WaylandInterface;
#endif
public:
	enum egl_platform platform;
	bool SelectDisplay(void);
	bool Init(EGLConfig config, void *window_handle);
	EGLDisplay EGLGetDisplay(void);
	EGLNativeWindowType CreateWindow(void);
	void DestroyWindow(void);
	void UpdateFPSDisplay(const char *text);
	void ToggleFullscreen(bool fullscreen);
	void SwapBuffers();
};

class cInterfaceEGL : public cInterfaceBase
{
private:
	cPlatform Platform;
	void DetectMode();
public:
	friend class cPlatform;
	void SwapInterval(int Interval);
	void Swap();
	void SetMode(u32 mode) { s_opengl_mode = GLInterfaceMode::MODE_DETECT; }
	void UpdateFPSDisplay(const char *Text);
	void* GetFuncAddress(std::string name);
	bool Create(void *&window_handle);
	bool MakeCurrent();
	void Shutdown();
};
#endif

