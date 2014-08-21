// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <EGL/egl.h>

#include "Core/ConfigManager.h"
#include "VideoBackends/OGL/GLInterfaceBase.h"

class cInterfaceEGL : public cInterfaceBase
{
protected:
	void DetectMode();
	EGLSurface egl_surf;
	EGLContext egl_ctx;
	EGLDisplay egl_dpy;

	virtual EGLDisplay OpenDisplay() = 0;
	virtual EGLNativeWindowType InitializePlatform(EGLNativeWindowType host_window, EGLConfig config) = 0;
	virtual void ShutdownPlatform() = 0;
public:
	void SwapInterval(int Interval);
	void Swap();
	void SetMode(u32 mode) { s_opengl_mode = mode; }
	void* GetFuncAddress(const std::string& name);
	bool Create(void *window_handle);
	bool MakeCurrent();
	void Shutdown();
};
