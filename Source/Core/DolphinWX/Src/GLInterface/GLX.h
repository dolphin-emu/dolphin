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
#ifndef _INTERFACEGLX_H_
#define _INTERFACEGLX_H_

#include <GL/glxew.h>
#include <GL/gl.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "X11_Util.h"
#include "InterfaceBase.h"

class cInterfaceGLX : public cInterfaceBase
{
private:
	cX11Window XWindow;
public:
	friend class cX11Window;
	void SwapInterval(int Interval);
	void Swap();
	void UpdateFPSDisplay(const char *Text);
	bool Create(void *&window_handle);
	bool MakeCurrent();
	bool ClearCurrent();
	void Shutdown(); 
};
#endif

