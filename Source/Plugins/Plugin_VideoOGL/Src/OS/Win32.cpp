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

#include <windows.h>

#include <wx/wx.h>
#include <wx/filepicker.h>
#include <wx/notebook.h>
#include <wx/dialog.h>
#include <wx/aboutdlg.h>

#include "../Globals.h"
#include "VideoConfig.h"
#include "main.h"
#include "Win32.h"
#include "OnScreenDisplay.h"
#include "VertexShaderManager.h"
#include "Render.h"

#include "StringUtil.h"


HINSTANCE g_hInstance;

#if defined(HAVE_WX) && HAVE_WX
class wxDLLApp : public wxApp
{
	bool OnInit()
	{
		return true;
	}
};
IMPLEMENT_APP_NO_MAIN(wxDLLApp) 
WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);
#endif
// ------------------

BOOL APIENTRY DllMain(HINSTANCE hinstDLL,	// DLL module handle
					  DWORD dwReason,		// reason called
					  LPVOID lpvReserved)	// reserved
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		{
#if defined(HAVE_WX) && HAVE_WX
			wxSetInstance((HINSTANCE)hinstDLL);
			wxInitialize();
#endif
		}
		break; 

	case DLL_PROCESS_DETACH:
#if defined(HAVE_WX) && HAVE_WX
		wxUninitialize();
#endif
		break;
	}

	g_hInstance = hinstDLL;
	return TRUE;
}

extern bool gShowDebugger;
int OSDChoice = 0 , OSDTime = 0, OSDInternalW = 0, OSDInternalH = 0;


// ---------------------------------------------------------------------
// OSD Menu
// -------------
// Let's begin with 3 since 1 and 2 are default Wii keys
// -------------
void OSDMenu(WPARAM wParam)
{
	switch( LOWORD( wParam ))
	{
	case '3':
		OSDChoice = 1;
		// Toggle native resolution
		if (!(g_Config.bNativeResolution || g_Config.b2xResolution))
			g_Config.bNativeResolution = true;
		else if (g_Config.bNativeResolution && Renderer::AllowCustom())
			{ g_Config.bNativeResolution = false; if (Renderer::Allow2x()) {g_Config.b2xResolution = true;} }
		else if (Renderer::AllowCustom())
			g_Config.b2xResolution = false;
		break;
	case '4':
		OSDChoice = 2;
		// Toggle aspect ratio
		g_Config.iAspectRatio = (g_Config.iAspectRatio + 1) & 3;
		break;
	case '5':
		OSDChoice = 3;
		// Toggle EFB copy
		if (g_Config.bEFBCopyDisable || g_Config.bCopyEFBToTexture)
		{
			g_Config.bEFBCopyDisable = !g_Config.bEFBCopyDisable;
			g_Config.bCopyEFBToTexture = false;
		}
		else
		{
			g_Config.bCopyEFBToTexture = !g_Config.bCopyEFBToTexture;
		}
		break;
	case '6':
		OSDChoice = 4;
		g_Config.bDisableFog = !g_Config.bDisableFog;
		break;
	case '7':
		OSDChoice = 5;
		g_Config.bDisableLighting = !g_Config.bDisableLighting;
		break;
	}
}
// ---------------------------------------------------------------------


// ---------------------------------------------------------------------
// The rendering window
// ----------------------
namespace EmuWindow
{

HWND m_hWnd = NULL; // The new window that is created here
HWND m_hParent = NULL;

HINSTANCE m_hInstance = NULL;
WNDCLASSEX wndClass;
const TCHAR m_szClassName[] = _T("DolphinEmuWnd");
int g_winstyle;

HWND GetWnd()
{
	return m_hWnd;
}
HWND GetParentWnd()
{
	return m_hParent;
}

void FreeLookInput( UINT iMsg, WPARAM wParam )
{
	static float debugSpeed = 1.0f;
	static bool mouseLookEnabled = false;
	static float lastMouse[2];

	switch( iMsg )
	{
	case WM_USER_KEYDOWN:
	case WM_KEYDOWN:
		switch( LOWORD( wParam ))
		{
		case '9':
			debugSpeed /= 2.0f;
			break;
		case '0':
			debugSpeed *= 2.0f;
			break;
		case 'W':
			VertexShaderManager::TranslateView(0.0f, debugSpeed);
			break;
		case 'S':
			VertexShaderManager::TranslateView(0.0f, -debugSpeed);
			break;
		case 'A':
			VertexShaderManager::TranslateView(debugSpeed, 0.0f);
			break;
		case 'D':
			VertexShaderManager::TranslateView(-debugSpeed, 0.0f);
			break;
		case 'R':
			VertexShaderManager::ResetView();
			break;
		}
		break;

	case WM_MOUSEMOVE:
		if (mouseLookEnabled) {
			POINT point;
			GetCursorPos(&point);
			VertexShaderManager::RotateView((point.x - lastMouse[0]) / 200.0f, (point.y - lastMouse[1]) / 200.0f);
			lastMouse[0] = point.x;
			lastMouse[1] = point.y;
		}
		break;

	case WM_RBUTTONDOWN:
		POINT point;	
		GetCursorPos(&point);
		lastMouse[0] = point.x;
		lastMouse[1] = point.y;
		mouseLookEnabled= true;
		break;
	case WM_RBUTTONUP:
		mouseLookEnabled = false;
		break;
	}
}

// ---------------------------------------------------------------------
// KeyDown events
// -------------
void OnKeyDown(WPARAM wParam)
{
	switch (LOWORD( wParam ))
	{
	case '3': // OSD keys
	case '4':
	case '5':
	case '6':
	case '7':
		if (g_Config.bOSDHotKey)
			OSDMenu(wParam);
		break;
	}
}
// ---------------------------------------------------------------------

// Should really take a look at the mouse stuff in here - some of it is weird.
LRESULT CALLBACK WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	if (g_Config.bFreeLook)
		FreeLookInput( iMsg, wParam );

	switch (iMsg)
	{
	case WM_PAINT:
		{
			HDC hdc;
			PAINTSTRUCT ps;
			hdc = BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
		}
		break;

	case WM_KEYDOWN:
		break;

	/* Post these mouse events to the main window, it's nessesary becase in difference to the
	   keyboard inputs these events only appear here, not in the parent window or any other WndProc()*/
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_LBUTTONDBLCLK:
		PostMessage(GetParentWnd(), iMsg, wParam, lParam);
		break;

	case WM_USER:
		if (wParam == WM_USER_KEYDOWN)
		{
			OnKeyDown(lParam);
			FreeLookInput(wParam, lParam);
		}
		else if (wParam == WIIMOTE_DISCONNECT)
		{
			PostMessage(m_hParent, WM_USER, wParam, lParam);
		}
		break;

	// This is called when we close the window when we render to a separate window
	case WM_CLOSE:
		if (m_hParent == NULL)
		{
			// Stop the game
			PostMessage(m_hParent, WM_USER, WM_USER_STOP, 0);
		}
		break;

	case WM_DESTROY:
		Shutdown();
		break;

	// Called when a screensaver wants to show up while this window is active
	case WM_SYSCOMMAND:
		switch (wParam) 
		{
		case SC_SCREENSAVE:
		case SC_MONITORPOWER:
			break;
		default:
			return DefWindowProc(hWnd, iMsg, wParam, lParam);
		}
		break;
	case WM_SETCURSOR:
		PostMessage(m_hParent, WM_USER, WM_USER_SETCURSOR, 0);
		return true;
		break;
	default:
		return DefWindowProc(hWnd, iMsg, wParam, lParam);
	}
	return 0;
	
}


// This is called from Create()
HWND OpenWindow(HWND parent, HINSTANCE hInstance, int width, int height, const TCHAR *title)
{
	wndClass.cbSize = sizeof( wndClass );
	wndClass.style  = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = WndProc;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = hInstance;
	wndClass.hIcon = LoadIcon( NULL, IDI_APPLICATION );
	wndClass.hCursor = NULL;
	wndClass.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = m_szClassName;
	wndClass.hIconSm = LoadIcon( NULL, IDI_APPLICATION );

	m_hInstance = hInstance;
	RegisterClassEx( &wndClass );

	// Create child window
	m_hParent = parent;

	m_hWnd = CreateWindow(m_szClassName, title, WS_CHILD,
		0, 0, width, height, parent, NULL, hInstance, NULL);

	return m_hWnd;
}

void Show()
{
	ShowWindow(m_hWnd, SW_SHOW);
	BringWindowToTop(m_hWnd);
	UpdateWindow(m_hWnd);
	SetFocus(m_hParent);

	// gShowDebugger from main.cpp is forgotten between the Dolphin-Debugger is opened and a game is
	// started so we have to use an ini file setting here
	/*
	bool bVideoWindow = false;
	IniFile ini;
	ini.Load(File::GetUserPath(F_DEBUGGERCONFIG_IDX));
	ini.Get("ShowOnStart", "VideoWindow", &bVideoWindow, false);
	if(bVideoWindow) DoDllDebugger();
	*/
}

HWND Create(HWND hParent, HINSTANCE hInstance, const TCHAR *title)
{
	int x=0, y=0, width=640, height=480;
	g_VideoInitialize.pRequestWindowSize(x, y, width, height);
	return OpenWindow(hParent, hInstance, width, height, title);
}

void Close()
{
	if (m_hParent == NULL)
		DestroyWindow(m_hWnd);
	UnregisterClass(m_szClassName, m_hInstance);
}

// ------------------------------------------
// Set the size of the child or main window
// ------------------------------------------
void SetSize(int width, int height)
{
	RECT rc = {0, 0, width, height};
	DWORD dwStyle = GetWindowLong(m_hWnd, GWL_STYLE);
	AdjustWindowRect(&rc, dwStyle, false);

	int w = rc.right - rc.left;
	int h = rc.bottom - rc.top;

	// Move and resize the window
	rc.left = (1280 - w)/2;
	rc.right = rc.left + w;
	rc.top = (1024 - h)/2;
	rc.bottom = rc.top + h;
	MoveWindow(m_hWnd, rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top, TRUE);
}

} // EmuWindow
