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
#ifndef _INTERFACEWGL_H_
#define _INTERFACEWGL_H_

#ifdef _WIN32
#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/wglew.h>
#endif

#include "InterfaceBase.h"

class cInterfaceWGL : public cInterfaceBase
{
public:
	void Swap();
	void UpdateFPSDisplay(const char *Text);
	bool Create(void *&window_handle);
	bool MakeCurrent();
	void Shutdown(); 

	void Update();
	bool PeekMessages();
};
#endif

