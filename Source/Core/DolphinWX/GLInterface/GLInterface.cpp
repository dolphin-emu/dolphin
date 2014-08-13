// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "DolphinWX/GLInterface/GLInterface.h"
#include "VideoBackends/OGL/GLInterfaceBase.h"

GLWindow GLWin;

cInterfaceBase* HostGL_CreateGLInterface()
{
	#if defined(USE_EGL) && USE_EGL
		return new cInterfaceEGL;
	#elif defined(__APPLE__)
		return new cInterfaceAGL;
	#elif defined(_WIN32)
		return new cInterfaceWGL;
	#elif defined(HAVE_X11) && HAVE_X11
		return new cInterfaceGLX;
	#else
		return nullptr;
	#endif
}
