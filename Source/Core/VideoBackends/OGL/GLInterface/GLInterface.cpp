// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "VideoBackends/OGL/GLInterfaceBase.h"

#ifdef ANDROID
#include "VideoBackends/OGL/GLInterface/EGLAndroid.h"
#elif defined(__APPLE__)
#include "VideoBackends/OGL/GLInterface/AGL.h"
#elif defined(_WIN32)
#include "VideoBackends/OGL/GLInterface/WGL.h"
#elif HAVE_X11
#if defined(USE_EGL) && USE_EGL
#include "VideoBackends/OGL/GLInterface/EGLX11.h"
#else
#include "VideoBackends/OGL/GLInterface/GLX.h"
#endif
#else
#error Platform doesnt have a GLInterface
#endif

cInterfaceBase* HostGL_CreateGLInterface()
{
	#ifdef ANDROID
		return new cInterfaceEGLAndroid;
	#elif defined(__APPLE__)
		return new cInterfaceAGL;
	#elif defined(_WIN32)
		return new cInterfaceWGL;
	#elif defined(HAVE_X11) && HAVE_X11
	#if defined(USE_EGL) && USE_EGL
		return new cInterfaceEGLX11;
	#else
		return new cInterfaceGLX;
	#endif
	#else
		return nullptr;
	#endif
}
