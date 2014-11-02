// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <GL/glx.h>

#include "VideoBackends/OGL/GLInterfaceBase.h"
#include "VideoBackends/OGL/GLInterface/X11_Util.h"

class cInterfaceGLX : public cInterfaceBase
{
private:
	cX11Window XWindow;
	Display *dpy;
	Window win;
	GLXContext ctx;
	XVisualInfo *vi;
public:
	friend class cX11Window;
	void SwapInterval(int Interval) override;
	void Swap() override;
	void* GetFuncAddress(const std::string& name) override;
	bool Create(void *window_handle);
	bool MakeCurrent() override;
	bool ClearCurrent() override;
	void Shutdown() override;
};
