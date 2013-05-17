// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
	void SwapInterval(int Interval);
	void Swap();
	void UpdateFPSDisplay(const char *Text);
	bool Create(void *&window_handle);
	bool MakeCurrent();
	bool ClearCurrent();
	void Shutdown(); 

	void Update();
	bool PeekMessages();
};
#endif

