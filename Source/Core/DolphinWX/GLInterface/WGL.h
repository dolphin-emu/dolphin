// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _INTERFACEWGL_H_
#define _INTERFACEWGL_H_

#include "InterfaceBase.h"

class cInterfaceWGL : public cInterfaceBase
{
public:
	void SwapInterval(int Interval);
	void Swap();
	void UpdateFPSDisplay(const char *Text);
	void* GetProcAddress(std::string name);
	bool Create(void *&window_handle);
	bool MakeCurrent();
	bool ClearCurrent();
	void Shutdown();

	void Update();
	bool PeekMessages();
};
#endif

