// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "VideoConfig.h"
#include "Host.h"
#include "RenderBase.h"

#include "VertexShaderManager.h"
#include "../GLInterface.h"
#include "WGL.h"

#include "EmuWindow.h"
static HDC hDC = NULL;       // Private GDI Device Context
static HGLRC hRC = NULL;     // Permanent Rendering Context

void cInterfaceWGL::SwapInterval(int Interval)
{
	if (WGLEW_EXT_swap_control)
		wglSwapIntervalEXT(Interval);
	else
		ERROR_LOG(VIDEO, "No support for SwapInterval (framerate clamped to monitor refresh rate).");
}
void cInterfaceWGL::Swap()
{
	SwapBuffers(hDC);
}

// Draw messages on top of the screen
bool cInterfaceWGL::PeekMessages()
{
	// TODO: peekmessage
	MSG msg;
	while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
			return FALSE;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return TRUE;
}

// Show the current FPS
void cInterfaceWGL::UpdateFPSDisplay(const char *text)
{
	TCHAR temp[512];
	swprintf_s(temp, sizeof(temp)/sizeof(TCHAR), _T("%hs"), text);
	EmuWindow::SetWindowText(temp);
}

// Create rendering window.
//		Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceWGL::Create(void *&window_handle)
{
	int _tx, _ty, _twidth, _theight;
	Host_GetRenderWindowSize(_tx, _ty, _twidth, _theight);

	// Control window size and picture scaling
	s_backbuffer_width = _twidth;
	s_backbuffer_height = _theight;

	window_handle = (void*)EmuWindow::Create((HWND)window_handle, GetModuleHandle(0), _T("Please wait..."));
	if (window_handle == NULL)
	{
		Host_SysMessage("failed to create window");
		return false;
	}

	// Show the window
	EmuWindow::Show();

	PIXELFORMATDESCRIPTOR pfd =         // pfd Tells Windows How We Want Things To Be
	{
		sizeof(PIXELFORMATDESCRIPTOR),  // Size Of This Pixel Format Descriptor
		1,                              // Version Number
		PFD_DRAW_TO_WINDOW |            // Format Must Support Window
			PFD_SUPPORT_OPENGL |        // Format Must Support OpenGL
			PFD_DOUBLEBUFFER,           // Must Support Double Buffering
		PFD_TYPE_RGBA,                  // Request An RGBA Format
		32,                             // Select Our Color Depth
		0, 0, 0, 0, 0, 0,               // Color Bits Ignored
		0,                              // 8bit Alpha Buffer
		0,                              // Shift Bit Ignored
		0,                              // No Accumulation Buffer
		0, 0, 0, 0,                     // Accumulation Bits Ignored
		24,                             // 24Bit Z-Buffer (Depth Buffer)  
		8,                              // 8bit Stencil Buffer
		0,                              // No Auxiliary Buffer
		PFD_MAIN_PLANE,                 // Main Drawing Layer
		0,                              // Reserved
		0, 0, 0                         // Layer Masks Ignored
	};

	GLuint      PixelFormat;            // Holds The Results After Searching For A Match

	if (!(hDC=GetDC(EmuWindow::GetWnd()))) {
		PanicAlert("(1) Can't create an OpenGL Device context. Fail.");
		return false;
	}
	if (!(PixelFormat = ChoosePixelFormat(hDC, &pfd))) {
		PanicAlert("(2) Can't find a suitable PixelFormat.");
		return false;
	}
	if (!SetPixelFormat(hDC, PixelFormat, &pfd)) {
		PanicAlert("(3) Can't set the PixelFormat.");
		return false;
	}
	if (!(hRC = wglCreateContext(hDC))) {
		PanicAlert("(4) Can't create an OpenGL rendering context.");
		return false;
	}
	return true;
}

bool cInterfaceWGL::MakeCurrent()
{
	return wglMakeCurrent(hDC, hRC) ? true : false;
}

bool cInterfaceWGL::ClearCurrent()
{
	return wglMakeCurrent(hDC, NULL) ? true : false;
}

// Update window width, size and etc. Called from Render.cpp
void cInterfaceWGL::Update()
{
	RECT rcWindow;
	if (!EmuWindow::GetParentWnd())
	{
		// We are not rendering to a child window - use client size.
		GetClientRect(EmuWindow::GetWnd(), &rcWindow);
	}
	else
	{
		// We are rendering to a child window - use parent size.
		GetWindowRect(EmuWindow::GetParentWnd(), &rcWindow);
	}

	// Get the new window width and height
	// See below for documentation
	int width = rcWindow.right - rcWindow.left;
	int height = rcWindow.bottom - rcWindow.top;

	// If we are rendering to a child window
	if (EmuWindow::GetParentWnd() != 0 &&
			(s_backbuffer_width != width || s_backbuffer_height != height) &&
			width >= 4 && height >= 4)
	{
		::MoveWindow(EmuWindow::GetWnd(), 0, 0, width, height, FALSE);
		s_backbuffer_width = width;
		s_backbuffer_height = height;
	}
}

// Close backend
void cInterfaceWGL::Shutdown()
{
	if (hRC)
	{
		if (!wglMakeCurrent(NULL, NULL))
			NOTICE_LOG(VIDEO, "Could not release drawing context.");

		if (!wglDeleteContext(hRC))
			ERROR_LOG(VIDEO, "Attempt to release rendering context failed.");

		hRC = NULL;
	}

	if (hDC && !ReleaseDC(EmuWindow::GetWnd(), hDC))
	{
		ERROR_LOG(VIDEO, "Attempt to release device context failed.");
		hDC = NULL;
	}
}


