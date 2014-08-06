// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <EGL/egl.h>

#include "Core/ConfigManager.h"
#include "VideoBackends/OGL/GLInterfaceBase.h"


class cPlatform
{
private:
#if HAVE_X11
	cXInterface XInterface;
#endif
public:
	bool Init(EGLConfig config, void *window_handle);
	EGLDisplay EGLGetDisplay(void);
	EGLNativeWindowType CreateWindow(void);
	void DestroyWindow(void);
	void UpdateFPSDisplay(const std::string& text);
	void SwapBuffers();
};

class cInterfaceEGL : public cInterfaceBase
{
private:
	cPlatform Platform;
	void DetectMode();
public:
	friend class cPlatform;
	void SwapInterval(int Interval);
	void Swap();
	void SetMode(u32 mode) { s_opengl_mode = mode; }
	void UpdateFPSDisplay(const std::string& text);
	void* GetFuncAddress(const std::string& name);
	bool Create(void *&window_handle);
	bool MakeCurrent();
	void Shutdown();
};
