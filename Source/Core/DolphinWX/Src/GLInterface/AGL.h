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
#ifndef _INTERFACEAGL_H_
#define _INTERFACEAGL_H_

#ifdef __APPLE__
#include <GL/glew.h>
#import <AppKit/AppKit.h>
#endif

#include "InterfaceBase.h"

class cInterfaceAGL : public cInterfaceBase
{
public:
	void Swap();
	bool Create(void *&window_handle);
	bool MakeCurrent();
	bool ClearCurrent();
	void Shutdown();
	void Update();

};
#endif

