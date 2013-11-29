// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <windows.h>

#include "VideoConfig.h"
#include "EmuWindow.h"
#include "Fifo.h"
#include "VideoBackendBase.h"
#include "Core.h"
#include "Host.h"
#include "ConfigManager.h"

namespace EmuWindow
{
HWND m_hWnd = NULL;
HWND m_hParent = NULL;
HINSTANCE m_hInstance = NULL;
WNDCLASSEX wndClass;
const TCHAR m_szClassName[] = _T("DolphinEmuWnd");
int g_winstyle;
static volatile bool s_sizing;
static const int TITLE_TEXT_BUF_SIZE = 1024;
TCHAR m_titleTextBuffer[TITLE_TEXT_BUF_SIZE];
static const int WM_SETTEXT_CUSTOM = WM_USER + WM_SETTEXT;

bool IsSizing()
{
	return s_sizing;
}

HWND GetWnd()
{
	return m_hWnd;
}

HWND GetParentWnd()
{
	return m_hParent;
}

LRESULT CALLBACK WndProc( HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam )
{
	switch( iMsg )
	{
	case WM_PAINT:
		{
			HDC hdc;
			PAINTSTRUCT ps;
			hdc = BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
		}
		break;

	case WM_ENTERSIZEMOVE:
		s_sizing = true;
		break;

	case WM_EXITSIZEMOVE:
		s_sizing = false;
		break;

	/* Post the mouse events to the main window, it's necessary, because the difference between the
	   keyboard inputs is that these events only appear here, not in the parent window or any other WndProc()*/
	case WM_LBUTTONDOWN:
		if(g_ActiveConfig.backend_info.bSupports3DVision && g_ActiveConfig.b3DVision)
		{
			// This basically throws away the left button down input when b3DVision is activated so WX
			// can't get access to it, stopping focus pulling on mouse click.
			// (Input plugins use a different system so it doesn't cause too much weirdness)
			break;
		}
	case WM_LBUTTONUP:
	case WM_LBUTTONDBLCLK:
		PostMessage(GetParentWnd(), iMsg, wParam, lParam);
		break;

	// This is called when we close the window when we render to a separate window
	case WM_CLOSE:
		// When the user closes the window, we post an event to the main window to call Stop()
		// Which then handles all the necessary steps to Shutdown the core
		if (m_hParent == NULL)
		{
			// Stop the game
			//PostMessage(m_hParent, WM_USER, WM_USER_STOP, 0);
		}
		break;

	case WM_USER:
		break;

	// Called when a screensaver wants to show up while this window is active
	case WM_SYSCOMMAND:

		switch (wParam)
		{
		case SC_SCREENSAVE:
		case SC_MONITORPOWER:
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bDisableScreenSaver)
			break;
		default:
			return DefWindowProc(hWnd, iMsg, wParam, lParam);
		}
		break;
	case WM_SETCURSOR:
		PostMessage(m_hParent, WM_USER, WM_USER_SETCURSOR, 0);
		return true;

	case WM_SETTEXT_CUSTOM:
		SendMessage(hWnd, WM_SETTEXT, wParam, lParam);
		break;

	default:
		return DefWindowProc(hWnd, iMsg, wParam, lParam);
	}
	return 0;
}

HWND OpenWindow(HWND parent, HINSTANCE hInstance, int width, int height, const TCHAR *title)
{
	wndClass.cbSize = sizeof( wndClass );
	wndClass.style  = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
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

	m_hParent = parent;

	m_hWnd = CreateWindow(m_szClassName, title, (g_ActiveConfig.backend_info.bSupports3DVision && g_ActiveConfig.b3DVision) ? WS_EX_TOPMOST | WS_POPUP : WS_CHILD,
		0, 0, width, height, m_hParent, NULL, hInstance, NULL);

	return m_hWnd;
}

void Show()
{
	ShowWindow(m_hWnd, SW_SHOW);
	BringWindowToTop(m_hWnd);
	UpdateWindow(m_hWnd);
	SetFocus(m_hParent);
}

HWND Create(HWND hParent, HINSTANCE hInstance, const TCHAR *title)
{
	// TODO:
	// 1. Remove redundant window manipulation,
	// 2. Make DX9 in fullscreen can be overlapped by other dialogs
	// 3. Request window sizes which actually make the client area map to a common resolution
	HWND Ret;
	int x=0, y=0, width=640, height=480;
	Host_GetRenderWindowSize(x, y, width, height);

	 // TODO: Don't show if fullscreen
	Ret = OpenWindow(hParent, hInstance, width, height, title);

	if (Ret)
	{
		Show();
	}
	return Ret;
}

void Close()
{
	DestroyWindow(m_hWnd);
	UnregisterClass(m_szClassName, m_hInstance);
}

void SetSize(int width, int height)
{
	RECT rc = {0, 0, width, height};
	DWORD style = GetWindowLong(m_hWnd, GWL_STYLE);
	AdjustWindowRect(&rc, style, false);

	int w = rc.right - rc.left;
	int h = rc.bottom - rc.top;

	rc.left = (1280 - w)/2;
	rc.right = rc.left + w;
	rc.top = (1024 - h)/2;
	rc.bottom = rc.top + h;
	MoveWindow(m_hWnd, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, TRUE);
}

void SetWindowText(const TCHAR* text)
{
	// the simple way.
	// we don't do this because it's a blocking call and the GUI thread might be waiting for us.
	//::SetWindowText(m_hWnd, text);

	// copy to m_titleTextBuffer in such a way that
	// it remains null-terminated and without garbage data at every point in time,
	// in case another thread reads it while we're doing this.
	for (int i = 0; i < TITLE_TEXT_BUF_SIZE-1; ++i)
	{
		m_titleTextBuffer[i+1] = 0;
		TCHAR c = text[i];
		m_titleTextBuffer[i] = c;
		if (!c)
			break;
	}

	// the OS doesn't allow posting WM_SETTEXT,
	// so we post our own message and convert it to that in WndProc
	PostMessage(m_hWnd, WM_SETTEXT_CUSTOM, 0, (LPARAM)m_titleTextBuffer);
}

}
