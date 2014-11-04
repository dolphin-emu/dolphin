// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <GL/glx.h>

#include "DolphinWX/GLInterface/X11_Util.h"
#include "VideoBackends/OGL/GLInterfaceBase.h"

class cInterfaceGLX : public cInterfaceBase
{
private:
	cX11Window XWindow;
	Display *dpy, *dpy_offscreen;
	Window win, win_offscreen;
	GLXContext ctx, ctx_offscreen;
	XVisualInfo *vi;
public:
	friend class cX11Window;
	void SwapInterval(int Interval) override;
	void Swap() override;
	void* GetFuncAddress(const std::string& name) override;
	bool Create(void *window_handle);
	bool CreateOffscreen();
	bool MakeCurrent() override;
	bool MakeCurrentOffscreen();
	bool ClearCurrent() override;
	bool ClearCurrentOffscreen();

	void Shutdown() override;
	void ShutdownOffscreen();
};
