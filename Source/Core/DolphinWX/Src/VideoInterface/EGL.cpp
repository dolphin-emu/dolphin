// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "VideoConfig.h"
#include "Host.h"
#include "RenderBase.h"

#include "VertexShaderManager.h"
#include "../GLVideoInterface.h"
#include "EGL.h"

void cInterfaceEGL::CreateXWindow(void)
{
	Atom wmProtocols[1];

	// Setup window attributes
	GLWin.attr.colormap = XCreateColormap(GLWin.x_evdpy,
			GLWin.parent, GLWin.vi->visual, AllocNone);
	GLWin.attr.event_mask = KeyPressMask | StructureNotifyMask | FocusChangeMask;
	GLWin.attr.background_pixel = BlackPixel(GLWin.x_evdpy, GLWin.screen);
	GLWin.attr.border_pixel = 0;

	// Create the window
	GLWin.win = XCreateWindow(GLWin.x_evdpy, GLWin.parent,
			GLWin.x, GLWin.y, GLWin.width, GLWin.height, 0,
			GLWin.vi->depth, InputOutput, GLWin.vi->visual,
			CWBorderPixel | CWBackPixel | CWColormap | CWEventMask, &GLWin.attr);
	wmProtocols[0] = XInternAtom(GLWin.x_evdpy, "WM_DELETE_WINDOW", True);
	XSetWMProtocols(GLWin.x_evdpy, GLWin.win, wmProtocols, 1);
	XSetStandardProperties(GLWin.x_evdpy, GLWin.win, "GPU", "GPU", None, NULL, 0, NULL);
	XMapRaised(GLWin.x_evdpy, GLWin.win);
	XSync(GLWin.x_evdpy, True);

	GLWin.xEventThread = std::thread(&cInterfaceEGL::XEventThread, this);
}

void cInterfaceEGL::DestroyXWindow(void)
{
	XUnmapWindow(GLWin.x_dpy, GLWin.win);
	GLWin.win = 0;
	if (GLWin.xEventThread.joinable())
		GLWin.xEventThread.join();
	XFreeColormap(GLWin.x_dpy, GLWin.attr.colormap);
}

void cInterfaceEGL::XEventThread()
{
	// Free look variables
	static bool mouseLookEnabled = false;
	static bool mouseMoveEnabled = false;
	static float lastMouse[2];
	while (GLWin.win)
	{
		XEvent event;
		KeySym key;
		for (int num_events = XPending(GLWin.x_evdpy); num_events > 0; num_events--)
		{
			XNextEvent(GLWin.x_evdpy, &event);
			switch(event.type) {
				case KeyPress:
					key = XLookupKeysym((XKeyEvent*)&event, 0);
					switch (key)
					{
						case XK_3:
							OSDChoice = 1;
							// Toggle native resolution
							g_Config.iEFBScale = g_Config.iEFBScale + 1;
							if (g_Config.iEFBScale > 7) g_Config.iEFBScale = 0;
							break;
						case XK_4:
							OSDChoice = 2;
							// Toggle aspect ratio
							g_Config.iAspectRatio = (g_Config.iAspectRatio + 1) & 3;
							break;
						case XK_5:
							OSDChoice = 3;
							// Toggle EFB copy
							if (!g_Config.bEFBCopyEnable || g_Config.bCopyEFBToTexture)
							{
								g_Config.bEFBCopyEnable ^= true;
								g_Config.bCopyEFBToTexture = false;
							}
							else
							{
								g_Config.bCopyEFBToTexture = !g_Config.bCopyEFBToTexture;
							}
							break;
						case XK_6:
							OSDChoice = 4;
							g_Config.bDisableFog = !g_Config.bDisableFog;
							break;
						default:
							break;
					}
					if (g_Config.bFreeLook)
					{
						static float debugSpeed = 1.0f;
						switch (key)
						{
							case XK_parenleft:
								debugSpeed /= 2.0f;
								break;
							case XK_parenright:
								debugSpeed *= 2.0f;
								break;
							case XK_w:
								VertexShaderManager::TranslateView(0.0f, debugSpeed);
								break;
							case XK_s:
								VertexShaderManager::TranslateView(0.0f, -debugSpeed);
								break;
							case XK_a:
								VertexShaderManager::TranslateView(debugSpeed, 0.0f);
								break;
							case XK_d:
								VertexShaderManager::TranslateView(-debugSpeed, 0.0f);
								break;
							case XK_r:
								VertexShaderManager::ResetView();
								break;
						}
					}
					break;
				case ButtonPress:
					if (g_Config.bFreeLook)
					{
						switch (event.xbutton.button)
						{
							case 2: // Middle button
								lastMouse[0] = event.xbutton.x;
								lastMouse[1] = event.xbutton.y;
								mouseMoveEnabled = true;
								break;
							case 3: // Right button
								lastMouse[0] = event.xbutton.x;
								lastMouse[1] = event.xbutton.y;
								mouseLookEnabled = true;
								break;
						}
					}
					break;
				case ButtonRelease:
					if (g_Config.bFreeLook)
					{
						switch (event.xbutton.button)
						{
							case 2: // Middle button
								mouseMoveEnabled = false;
								break;
							case 3: // Right button
								mouseLookEnabled = false;
								break;
						}
					}
					break;
				case MotionNotify:
					if (g_Config.bFreeLook)
					{
						if (mouseLookEnabled)
						{
							VertexShaderManager::RotateView((event.xmotion.x - lastMouse[0]) / 200.0f,
									(event.xmotion.y - lastMouse[1]) / 200.0f);
							lastMouse[0] = event.xmotion.x;
							lastMouse[1] = event.xmotion.y;
						}

						if (mouseMoveEnabled)
						{
							VertexShaderManager::TranslateView((event.xmotion.x - lastMouse[0]) / 50.0f,
									(event.xmotion.y - lastMouse[1]) / 50.0f);
							lastMouse[0] = event.xmotion.x;
							lastMouse[1] = event.xmotion.y;
						}
					}
					break;
				case ConfigureNotify:
					Window winDummy;
					unsigned int borderDummy, depthDummy;
					XGetGeometry(GLWin.x_dpy, GLWin.win, &winDummy, &GLWin.x, &GLWin.y,
							&GLWin.width, &GLWin.height, &borderDummy, &depthDummy);
					s_backbuffer_width = GLWin.width;
					s_backbuffer_height = GLWin.height;
					break;
				case ClientMessage:
					if ((unsigned long) event.xclient.data.l[0] ==
							XInternAtom(GLWin.x_dpy, "WM_DELETE_WINDOW", False))
						Host_Message(WM_USER_STOP);
					if ((unsigned long) event.xclient.data.l[0] ==
							XInternAtom(GLWin.x_dpy, "RESIZE", False))
						XMoveResizeWindow(GLWin.x_dpy, GLWin.win,
								event.xclient.data.l[1], event.xclient.data.l[2],
								event.xclient.data.l[3], event.xclient.data.l[4]);
					break;
				default:
					break;
			}
		}
		Common::SleepCurrentThread(20);
	}
}

// Show the current FPS
void cInterfaceEGL::UpdateFPSDisplay(const char *text)
{
	XStoreName(GLWin.x_dpy, GLWin.win, text);
}
void cInterfaceEGL::SwapBuffers()
{
	eglSwapBuffers(GLWin.egl_dpy, GLWin.egl_surf);
}

// Create rendering window.
//		Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceEGL::CreateWindow(void *&window_handle)
{
	int _tx, _ty, _twidth, _theight;
	Host_GetRenderWindowSize(_tx, _ty, _twidth, _theight);

	// Control window size and picture scaling
	s_backbuffer_width = _twidth;
	s_backbuffer_height = _theight;

	const char *s;
   EGLint egl_major, egl_minor;

   GLWin.x_dpy = XOpenDisplay(NULL);

   if (!GLWin.x_dpy) {
      printf("Error: couldn't open display\n");
      return false;
   }

   GLWin.egl_dpy = eglGetDisplay(GLWin.x_dpy);
   if (!GLWin.egl_dpy) {
      printf("Error: eglGetDisplay() failed\n");
      return false;
   }

   if (!eglInitialize(GLWin.egl_dpy, &egl_major, &egl_minor)) {
      printf("Error: eglInitialize() failed\n");
      return false;
   }

   s = eglQueryString(GLWin.egl_dpy, EGL_VERSION);
   printf("EGL_VERSION = %s\n", s);

   s = eglQueryString(GLWin.egl_dpy, EGL_VENDOR);
   printf("EGL_VENDOR = %s\n", s);

   s = eglQueryString(GLWin.egl_dpy, EGL_EXTENSIONS);
   printf("EGL_EXTENSIONS = %s\n", s);

   s = eglQueryString(GLWin.egl_dpy, EGL_CLIENT_APIS);
   printf("EGL_CLIENT_APIS = %s\n", s);

	// attributes for a visual in RGBA format with at least
	// 8 bits per color and a 24 bit depth buffer
	int attribs[] = {
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
#ifdef USE_GLES
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
#else
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
#endif
		EGL_NONE };

	static const EGLint ctx_attribs[] = {
#ifdef USE_GLES
		EGL_CONTEXT_CLIENT_VERSION, 2,
#endif
		EGL_NONE
	};
	
	GLWin.x_evdpy = XOpenDisplay(NULL);
	GLWin.parent = (Window)window_handle;
	GLWin.screen = DefaultScreen(GLWin.x_dpy);
	if (GLWin.parent == 0)
		GLWin.parent = RootWindow(GLWin.x_dpy, GLWin.screen);

   unsigned long mask;
   XVisualInfo  visTemplate;
   int num_visuals;
   EGLConfig config;
   EGLint num_configs;
   EGLint vid;

   if (!eglChooseConfig( GLWin.egl_dpy, attribs, &config, 1, &num_configs)) {
      printf("Error: couldn't get an EGL visual config\n");
      exit(1);
   }

   if (!eglGetConfigAttrib(GLWin.egl_dpy, config, EGL_NATIVE_VISUAL_ID, &vid)) {
      printf("Error: eglGetConfigAttrib() failed\n");
      exit(1);
   }

   /* The X window visual must match the EGL config */
   visTemplate.visualid = vid;
   GLWin.vi = XGetVisualInfo(GLWin.x_dpy, VisualIDMask, &visTemplate, &num_visuals);
   if (!GLWin.vi) {
      printf("Error: couldn't get X visual\n");
      exit(1);
   }
	
	GLWin.x = _tx;
	GLWin.y = _ty;
	GLWin.width = _twidth;
	GLWin.height = _theight;

	CreateXWindow();
#ifdef USE_GLES
	eglBindAPI(EGL_OPENGL_ES_API);
#else
	eglBindAPI(EGL_OPENGL_API);
#endif
	GLWin.egl_ctx = eglCreateContext(GLWin.egl_dpy, config, EGL_NO_CONTEXT, ctx_attribs );
   if (!GLWin.egl_ctx) {
      printf("Error: eglCreateContext failed\n");
      exit(1);
   }

   GLWin.egl_surf = eglCreateWindowSurface(GLWin.egl_dpy, config, GLWin.win, NULL);
   if (!GLWin.egl_surf) {
      printf("Error: eglCreateWindowSurface failed\n");
      exit(1);
   }

   if (!eglMakeCurrent(GLWin.egl_dpy, GLWin.egl_surf, GLWin.egl_surf, GLWin.egl_ctx)) {

      printf("Error: eglMakeCurrent() failed\n");
      return false;
   }
	

	printf("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
	printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
	printf("GL_VERSION: %s\n", glGetString(GL_VERSION));
	printf("GL_EXTENSIONS: %s\n", glGetString(GL_EXTENSIONS));
   /* Set initial projection/viewing transformation.
    * We can't be sure we'll get a ConfigureNotify event when the window
    * first appears.
    */
	glViewport(0, 0, (GLint) _twidth, (GLint) _theight);
	window_handle = (void *)GLWin.win;
	return true;
}

bool cInterfaceEGL::MakeCurrent()
{
	return eglMakeCurrent(GLWin.egl_dpy, GLWin.egl_surf, GLWin.egl_surf, GLWin.egl_ctx);
}
// Close backend
void cInterfaceEGL::Shutdown()
{
	DestroyXWindow();
	if (GLWin.egl_ctx && !eglMakeCurrent(GLWin.egl_dpy, GLWin.egl_surf, GLWin.egl_surf, GLWin.egl_ctx))
		NOTICE_LOG(VIDEO, "Could not release drawing context.");
	if (GLWin.egl_ctx)
	{
		eglDestroyContext(GLWin.egl_dpy, GLWin.egl_ctx);
		eglDestroySurface(GLWin.egl_dpy, GLWin.egl_surf);
		eglTerminate(GLWin.egl_dpy);
		GLWin.egl_ctx = NULL;
	}
}

