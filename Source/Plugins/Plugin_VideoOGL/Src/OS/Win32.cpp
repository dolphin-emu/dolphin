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
#include "../Config.h"
#include "main.h"
#include "Win32.h"
#include "OnScreenDisplay.h"
#include "VertexShaderManager.h"

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
			// Use wxInitialize() if you don't want GUI instead of the following 12 lines
			wxSetInstance((HINSTANCE)hinstDLL);
			int argc = 0;
			char **argv = NULL;
			wxEntryStart(argc, argv);
			if (!wxTheApp || !wxTheApp->CallOnInit())
				return FALSE;
			#endif
		}
		break; 

	case DLL_PROCESS_DETACH:
		#if defined(HAVE_WX) && HAVE_WX
			// This causes a "stop hang", if the gfx config dialog has been opened.
			// Old comment: "Use wxUninitialize() if you don't want GUI"
			wxEntryCleanup();
		#endif
		break;
	default:
		break;
	}

	g_hInstance = hinstDLL;
	return TRUE;
}

extern bool gShowDebugger;
int OSDChoice = 0, OSDTime = 0, OSDInternalW = 0, OSDInternalH = 0;

// ---------------------------------------------------------------------
// OSD Menu
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
// Let's begin with 3 since 1 and 2 are default Wii keys
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
void OSDMenu(WPARAM wParam)
{
	switch( LOWORD( wParam ))
	{
	case '3':
		OSDChoice = 1;
		// Toggle native resolution
		if (!(g_Config.bNativeResolution || g_Config.b2xResolution))
			g_Config.bNativeResolution = true;
		else if (g_Config.bNativeResolution)
			{ g_Config.bNativeResolution = false; g_Config.b2xResolution = true; }
		else
			g_Config.b2xResolution = false;
		break;
	case '4':
		OSDChoice = 2;
		// Toggle aspect ratio
		if (!(g_Config.bKeepAR43 || g_Config.bKeepAR169))
			{ g_Config.bKeepAR43 = true; g_Config.bCrop = false; }
		else if (g_Config.bKeepAR43 && !g_Config.bCrop)
			g_Config.bCrop = true;
		else if (g_Config.bKeepAR43)
			{ g_Config.bKeepAR43 = false; g_Config.bCrop = false; g_Config.bKeepAR169 = true; }
		else if (g_Config.bKeepAR169 && !g_Config.bCrop)
			g_Config.bCrop = true;
		else
			{ g_Config.bKeepAR43 = false; g_Config.bKeepAR169 = false; g_Config.bCrop = false; }
		break;
	case '5':
		OSDChoice = 3;
		// Toggle EFB copy
		g_Config.bEFBCopyDisable = !g_Config.bEFBCopyDisable;
		break;
	case '6':
		//OSDChoice = 4;
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
HWND m_hMain = NULL; // The main CPanel

HINSTANCE m_hInstance = NULL;
WNDCLASSEX wndClass;
const TCHAR m_szClassName[] = "DolphinEmuWnd";
int g_winstyle;

// ---------------------------------------------------------------------
/* Invisible cursor option. In the lack of a predefined IDC_BLANK we make
   an empty transparent cursor */
// ------------------
HCURSOR hCursor = NULL, hCursorBlank = NULL;
void CreateCursors(HINSTANCE hInstance)
{
	BYTE ANDmaskCursor[] = { 0xff };
	BYTE XORmaskCursor[] = { 0x00 };
	hCursorBlank = CreateCursor(hInstance, 0,0, 1,1, ANDmaskCursor,XORmaskCursor);

	hCursor = LoadCursor(NULL, IDC_ARROW);
}

HWND GetWnd()
{
	return m_hWnd;
}

HWND GetParentWnd()
{
    return m_hParent;
}

HWND GetChildParentWnd()
{
    return m_hMain;
}

void FreeLookInput( UINT iMsg, WPARAM wParam )
{
    static float debugSpeed = 1.0f;
    static bool mouseLookEnabled = false;
    static float lastMouse[2];

	switch( iMsg )
	{
	
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

LRESULT CALLBACK WndProc( HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam )
{		
	HDC         hdc;
	PAINTSTRUCT ps;
	switch( iMsg )
	{
	case WM_CREATE:
		PostMessage(m_hMain, WM_USER, OPENGL_WM_USER_CREATE, (int)m_hParent);
		break;

	case WM_PAINT:
		hdc = BeginPaint( hWnd, &ps );
		EndPaint( hWnd, &ps );
		return 0;

	case WM_SYSKEYDOWN:
		switch( LOWORD( wParam ))
		{
		case VK_RETURN:
			// Pressing Alt+Enter switch FullScreen/Windowed
			if (m_hParent == NULL && !g_Config.renderToMainframe)
			{
				ToggleFullscreen(hWnd);
				return 0;
			}				
			break;
		}
		break;

	case WM_KEYDOWN:
		switch( LOWORD( wParam ))
		{
		case VK_ESCAPE:
			if (g_Config.bFullscreen)
			{
				// Pressing Esc switch to Windowed in Fullscreen mode
				ToggleFullscreen(hWnd);
				return 0;
			}
			else if (!g_Config.renderToMainframe)
			{
				// And stops the emulation when already in Windowed mode
				PostMessage(m_hMain, WM_USER, OPENGL_WM_USER_STOP, 0);
			}
			break;
		case '3': // OSD keys
		case '4':
		case '5':
		case '6':
			OSDMenu(wParam);
			break;
		}
		g_VideoInitialize.pKeyPress(LOWORD(wParam), GetAsyncKeyState(VK_SHIFT) != 0, GetAsyncKeyState(VK_CONTROL) != 0);
		break;

	/* The reason we pick up the WM_MOUSEMOVE is to be able to change this option
	   during gameplay. The alternative is to load one of the cursors when the plugin
	   is loaded and go with that. This should only produce a minimal performance hit
	   because SetCursor is not supposed to actually change the cursor if it's the
	   same as the one before. */
	case WM_MOUSEMOVE:
		/* Check rendering mode; child or parent. Then post the mouse moves to the main window
		   it's nessesary for both the chil dwindow and separate rendering window because
		   moves over the rendering window do not reach the main program then. */
		if (GetParentWnd() == NULL) // Separate rendering window
			PostMessage(m_hMain, iMsg, wParam, -1);			
		else
			PostMessage(GetParentWnd(), iMsg, wParam, lParam);
		break;

	/* To support the separate window rendering we get the message back here. So we basically
	    only let it pass through Dolphin > Frame.cpp to determine if it should be on or off
		and coordinate it with the other settings if nessesary */
	case WM_USER:
		/* I set wParam to 10 just in case there are other WM_USER events. If we want more
		   WM_USER cases we would start making wParam or lParam cases */
		if (wParam == 10)
		{
			if (lParam)
				SetCursor(hCursor);
			else
				SetCursor(hCursorBlank);
		}
		break;

	/* Post thes mouse events to the main window, it's nessesary becase in difference to the
	   keyboard inputs these events only appear here, not in the main WndProc() */
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_LBUTTONDBLCLK:
		PostMessage(GetParentWnd(), iMsg, wParam, lParam);
	break;

	// This is called when we close the window when we render to a separate window
	case WM_CLOSE:	
		if (m_hParent == NULL)
		{
			// Simple hack to easily exit without stopping. Hope to fix the stopping errors soon.
			ExitProcess(0);
			return 0;
		}

	case WM_DESTROY:
		Shutdown();
		break;

	// Called when a screensaver wants to show up while this window is active
	case WM_SYSCOMMAND:
		switch (wParam) 
		{
		case SC_SCREENSAVE:
		case SC_MONITORPOWER:
			return 0;
		}
		break;
	}

    if (g_Config.bFreeLook) {
        FreeLookInput( iMsg, wParam );
    }

	return DefWindowProc(hWnd, iMsg, wParam, lParam);
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
	// To interfer less with SetCursor() later we set this to NULL
	//wndClass.hCursor = LoadCursor( NULL, IDC_ARROW );
	wndClass.hCursor = NULL;
	wndClass.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = m_szClassName;
	wndClass.hIconSm = LoadIcon( NULL, IDI_APPLICATION );

	m_hInstance = hInstance;
	RegisterClassEx( &wndClass );

	CreateCursors(m_hInstance);

	// Create child window
    if (parent)
    {
		m_hParent = m_hMain = parent;

        m_hWnd = CreateWindow(m_szClassName, title,
            WS_CHILD,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            parent, NULL, hInstance, NULL);

        ShowWindow(m_hWnd, SW_SHOWMAXIMIZED);
    }

	// Create new separate window
    else
    {
		DWORD style = g_Config.bFullscreen ? WS_POPUP : WS_OVERLAPPEDWINDOW;

        RECT rc = {0, 0, width, height};
        AdjustWindowRect(&rc, style, false);

        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        rc.left = (1280 - w)/2;
        rc.right = rc.left + w;
        rc.top = (1024 - h)/2;
        rc.bottom = rc.top + h;

		// I save this to m_hMain instead of m_hParent because it casused problems otherwise
		m_hMain = (HWND)g_VideoInitialize.pWindowHandle;

        m_hWnd = CreateWindow(m_szClassName, title,
            style,
            rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top,
            parent, NULL, hInstance, NULL );

        g_winstyle = GetWindowLong( m_hWnd, GWL_STYLE );
        g_winstyle &= ~WS_MAXIMIZE & ~WS_MINIMIZE; // remove minimize/maximize style
    }

	return m_hWnd;
}

void ToggleFullscreen(HWND hParent)
{
	if (m_hParent == NULL)
	{ 
		int	w_fs = 640, h_fs = 480;
		if (g_Config.bFullscreen)
		{
			// Get out of fullscreen
			g_Config.bFullscreen = false;
			RECT rc = {0, 0, w_fs, h_fs};

			if (strlen(g_Config.iInternalRes) > 1)
				sscanf(g_Config.iInternalRes, "%dx%d", &w_fs, &h_fs);
			// FullScreen -> Desktop
			ChangeDisplaySettings(NULL, 0);

			RECT rcdesktop;	// Get desktop resolution
			GetWindowRect(GetDesktopWindow(), &rcdesktop);

			ShowCursor(TRUE);

			// SetWindowPos to the center of the screen
			int X = (rcdesktop.right-rcdesktop.left)/2 - (rc.right-rc.left)/2;
			int Y = (rcdesktop.bottom-rcdesktop.top)/2 - (rc.bottom-rc.top)/2;

			// Note: we now use the same res for fullscreen and windowed, so we need to check if the window
			// is not too big here
			if (w_fs == rcdesktop.right-rcdesktop.left)
				SetWindowPos(hParent, NULL, X*0.75, Y*0.75, w_fs*0.75, h_fs*0.75, SWP_NOREPOSITION | SWP_NOZORDER);
			else
				SetWindowPos(hParent, NULL, X, Y, w_fs, h_fs, SWP_NOREPOSITION | SWP_NOZORDER);

			// Set new window style FS -> Windowed
			SetWindowLong(hParent, GWL_STYLE, WS_OVERLAPPEDWINDOW);

			// Eventually show the window!
			EmuWindow::Show();
		}
		else
		{
			// Get into fullscreen
			DEVMODE dmScreenSettings;
			memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));

			if (strlen(g_Config.iFSResolution) > 1)
				sscanf(g_Config.iFSResolution, "%dx%d", &w_fs, &h_fs);

			// Desktop -> FullScreen
			dmScreenSettings.dmSize			= sizeof(dmScreenSettings);
			dmScreenSettings.dmPelsWidth	= w_fs;
			dmScreenSettings.dmPelsHeight	= h_fs;
			dmScreenSettings.dmBitsPerPel	= 32;
			dmScreenSettings.dmFields = DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT;
			if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
				return;
			
			// Set new window style -> PopUp
			SetWindowLong(hParent, GWL_STYLE, WS_POPUP);
			g_Config.bFullscreen = true;
			ShowCursor(FALSE);

			// SetWindowPos to the upper-left corner of the screen
			SetWindowPos(hParent, NULL, 0, 0, w_fs, h_fs, SWP_NOREPOSITION | SWP_NOZORDER);

			// Eventually show the window!
			EmuWindow::Show();
		}
	}
}

void Show()
{
	ShowWindow(m_hWnd, SW_SHOW);
	BringWindowToTop(m_hWnd);
	UpdateWindow(m_hWnd);

	// gShowDebugger from main.cpp is forgotten between the Dolphin-Debugger is opened and a game is
	// started so we have to use an ini file setting here
	/*
	bool bVideoWindow = false;
	IniFile ini;
	ini.Load(DEBUGGER_CONFIG_FILE);
	ini.Get("ShowOnStart", "VideoWindow", &bVideoWindow, false);
	if(bVideoWindow) DoDllDebugger();
	*/
}

HWND Create(HWND hParent, HINSTANCE hInstance, const TCHAR *title)
{
	return OpenWindow(hParent, hInstance, 640, 480, title);
}

void Close()
{
	DestroyWindow(m_hWnd);
	UnregisterClass(m_szClassName, m_hInstance);
}

// ------------------------------------------
// Set the size of the child or main window
// ------------------------------------------
void SetSize(int width, int height)
{
	RECT rc = {0, 0, width, height};
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, false);

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
