// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "DolphinWX/GLInterface/InterfaceBase.h"
#include "DolphinWX/GLInterface/X11_Util.h"

class cInterfaceGLX : public cInterfaceBase
{
private:
	cX11Window XWindow;
public:
	friend class cX11Window;
	void SwapInterval(int Interval) override;
	void Swap() override;
	void UpdateFPSDisplay(const std::string& text) override;
	void* GetFuncAddress(const std::string& name) override;
	bool Create(void *&window_handle) override;
	bool MakeCurrent() override;
	bool ClearCurrent() override;
	void Shutdown() override;
};
