// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/OGL/GLInterface/EGL.h"

class cInterfaceEGLAndroid : public cInterfaceEGL
{
protected:
	EGLDisplay OpenDisplay() override;
	EGLNativeWindowType InitializePlatform(EGLNativeWindowType host_window, EGLConfig config) override;
	void ShutdownPlatform() override;
};
