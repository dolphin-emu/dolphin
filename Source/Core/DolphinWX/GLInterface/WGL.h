// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "DolphinWX/GLInterface/InterfaceBase.h"

class cInterfaceWGL : public cInterfaceBase
{
public:
	void SwapInterval(int Interval);
	void Swap();
	void UpdateFPSDisplay(const char *Text);
	void* GetFuncAddress(std::string name);
	bool Create(void *&window_handle);
	bool MakeCurrent();
	bool ClearCurrent();
	void Shutdown();

	void Update();
	bool PeekMessages();
};
