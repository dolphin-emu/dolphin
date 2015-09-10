// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "VideoBackends/OGL/GLInterfaceBase.h"

class cInterfaceWGL : public cInterfaceBase
{
public:
	void SwapInterval(int Interval);
	void Swap();
	void* GetFuncAddress(const std::string& name);
	bool Create(void *window_handle);
	bool CreateOffscreen();
	bool MakeCurrent();
	bool MakeCurrentOffscreen();
	bool ClearCurrent();
	bool ClearCurrentOffscreen();
	void Shutdown();
	void ShutdownOffscreen();

	void Update();
	bool PeekMessages();

	HWND m_window_handle;
	HWND m_offscreen_window_handle = nullptr;
};
