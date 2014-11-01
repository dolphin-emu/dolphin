// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "VideoBackends/OGL/GLInterfaceBase.h"
#include "VideoBackends/OGL/GLInterface/EGL.h"
#include "VideoCommon/RenderBase.h"

// Show the current FPS
void cInterfaceEGL::Swap()
{
	eglSwapBuffers(egl_dpy, egl_surf);
}
void cInterfaceEGL::SwapInterval(int Interval)
{
	eglSwapInterval(egl_dpy, Interval);
}

void* cInterfaceEGL::GetFuncAddress(const std::string& name)
{
	return (void*)eglGetProcAddress(name.c_str());
}

void cInterfaceEGL::DetectMode()
{
	if (s_opengl_mode != MODE_DETECT)
		return;

	EGLint num_configs;
	EGLConfig *config = nullptr;
	bool supportsGL = false, supportsGLES2 = false, supportsGLES3 = false;

	// attributes for a visual in RGBA format with at least
	// 8 bits per color
	int attribs[] = {
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_NONE };

	// Get how many configs there are
	if (!eglChooseConfig( egl_dpy, attribs, nullptr, 0, &num_configs))
	{
		INFO_LOG(VIDEO, "Error: couldn't get an EGL visual config\n");
		goto err_exit;
	}

	config = new EGLConfig[num_configs];

	// Get all the configurations
	if (!eglChooseConfig(egl_dpy, attribs, config, num_configs, &num_configs))
	{
		INFO_LOG(VIDEO, "Error: couldn't get an EGL visual config\n");
		goto err_exit;
	}

	for (int i = 0; i < num_configs; ++i)
	{
		EGLint attribVal;
		bool ret;
		ret = eglGetConfigAttrib(egl_dpy, config[i], EGL_RENDERABLE_TYPE, &attribVal);
		if (ret)
		{
			if (attribVal & EGL_OPENGL_BIT)
				supportsGL = true;
			if (attribVal & (1 << 6)) /* EGL_OPENGL_ES3_BIT_KHR */
				supportsGLES3 = true;
			if (attribVal & EGL_OPENGL_ES2_BIT)
				supportsGLES2 = true;
		}
	}
	if (supportsGL)
		s_opengl_mode = GLInterfaceMode::MODE_OPENGL;
	else if (supportsGLES3)
		s_opengl_mode = GLInterfaceMode::MODE_OPENGLES3;
	else if (supportsGLES2)
		s_opengl_mode = GLInterfaceMode::MODE_OPENGLES2;
err_exit:
	if (s_opengl_mode == GLInterfaceMode::MODE_DETECT) // Errored before we found a mode
		s_opengl_mode = GLInterfaceMode::MODE_OPENGL; // Fall back to OpenGL
	if (config)
		delete[] config;
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceEGL::Create(void *window_handle)
{
	const char *s;
	EGLint egl_major, egl_minor;

	egl_dpy = OpenDisplay();

	if (!egl_dpy)
	{
		INFO_LOG(VIDEO, "Error: eglGetDisplay() failed\n");
		return false;
	}

	if (!eglInitialize(egl_dpy, &egl_major, &egl_minor))
	{
		INFO_LOG(VIDEO, "Error: eglInitialize() failed\n");
		return false;
	}

	/* Detection code */
	EGLConfig config;
	EGLint num_configs;

	DetectMode();

	// attributes for a visual in RGBA format with at least
	// 8 bits per color
	int attribs[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_NONE };

	EGLint ctx_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	switch (s_opengl_mode)
	{
		case MODE_OPENGL:
			attribs[1] = EGL_OPENGL_BIT;
			ctx_attribs[0] = EGL_NONE;
		break;
		case MODE_OPENGLES2:
			attribs[1] = EGL_OPENGL_ES2_BIT;
			ctx_attribs[1] = 2;
		break;
		case MODE_OPENGLES3:
			attribs[1] = (1 << 6); /* EGL_OPENGL_ES3_BIT_KHR */
			ctx_attribs[1] = 3;
		break;
		default:
			ERROR_LOG(VIDEO, "Unknown opengl mode set\n");
			return false;
		break;
	}

	if (!eglChooseConfig( egl_dpy, attribs, &config, 1, &num_configs))
	{
		INFO_LOG(VIDEO, "Error: couldn't get an EGL visual config\n");
		exit(1);
	}

	if (s_opengl_mode == MODE_OPENGL)
		eglBindAPI(EGL_OPENGL_API);
	else
		eglBindAPI(EGL_OPENGL_ES_API);

	EGLNativeWindowType host_window = (EGLNativeWindowType) window_handle;
	EGLNativeWindowType native_window = InitializePlatform(host_window, config);

	s = eglQueryString(egl_dpy, EGL_VERSION);
	INFO_LOG(VIDEO, "EGL_VERSION = %s\n", s);

	s = eglQueryString(egl_dpy, EGL_VENDOR);
	INFO_LOG(VIDEO, "EGL_VENDOR = %s\n", s);

	s = eglQueryString(egl_dpy, EGL_EXTENSIONS);
	INFO_LOG(VIDEO, "EGL_EXTENSIONS = %s\n", s);

	s = eglQueryString(egl_dpy, EGL_CLIENT_APIS);
	INFO_LOG(VIDEO, "EGL_CLIENT_APIS = %s\n", s);

	egl_ctx = eglCreateContext(egl_dpy, config, EGL_NO_CONTEXT, ctx_attribs );
	if (!egl_ctx)
	{
		INFO_LOG(VIDEO, "Error: eglCreateContext failed\n");
		exit(1);
	}

	egl_surf = eglCreateWindowSurface(egl_dpy, config, native_window, nullptr);
	if (!egl_surf)
	{
		INFO_LOG(VIDEO, "Error: eglCreateWindowSurface failed\n");
		exit(1);
	}

	return true;
}

bool cInterfaceEGL::MakeCurrent()
{
	return eglMakeCurrent(egl_dpy, egl_surf, egl_surf, egl_ctx);
}
// Close backend
void cInterfaceEGL::Shutdown()
{
	ShutdownPlatform();
	if (egl_ctx && !eglMakeCurrent(egl_dpy, egl_surf, egl_surf, egl_ctx))
		NOTICE_LOG(VIDEO, "Could not release drawing context.");
	if (egl_ctx)
	{
		eglMakeCurrent(egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (!eglDestroyContext(egl_dpy, egl_ctx))
			NOTICE_LOG(VIDEO, "Could not destroy drawing context.");
		if (!eglDestroySurface(egl_dpy, egl_surf))
			NOTICE_LOG(VIDEO, "Could not destroy window surface.");
		if (!eglTerminate(egl_dpy))
			NOTICE_LOG(VIDEO, "Could not destroy display connection.");
		egl_ctx = nullptr;
	}
}
