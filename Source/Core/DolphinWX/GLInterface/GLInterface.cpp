// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "VideoBackends/OGL/GLInterfaceBase.h"

#if USE_EGL
#include "DolphinWX/GLInterface/EGL.h"
#elif defined(__APPLE__)
#include "DolphinWX/GLInterface/AGL.h"
#elif defined(_WIN32)
#include "DolphinWX/GLInterface/WGL.h"
#elif HAVE_X11
#include "DolphinWX/GLInterface/GLX.h"
#else
#error Platform doesnt have a GLInterface
#endif

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
