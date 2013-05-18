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

#if USE_GLES
#ifdef USE_GLES3
#include <GLES3/gl3.h>
#else
#include <GLES2/gl2.h>
#endif
#else
#include <GL/glxew.h>
#include <GL/gl.h>
#endif

#include "InterfaceBase.h"

class cPlatform;

class cInterfaceEGL : public cInterfaceBase
{
private:
	cPlatform Platform;
public:
	friend class cPlatform;
	void SwapInterval(int Interval);
	void Swap();
	void UpdateFPSDisplay(const char *Text);
	bool Create(void *&window_handle);
	bool MakeCurrent();
	void Shutdown(); 
};
#endif

