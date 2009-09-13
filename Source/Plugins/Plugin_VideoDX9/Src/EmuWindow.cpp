
#include <windows.h>

#include "../../Core/Src/Core.h"
#include "Config.h"
#include "main.h"
#include "EmuWindow.h"
#include "Fifo.h"

namespace EmuWindow
{
HWND m_hWnd = NULL;
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
		return 0;

	case WM_KEYDOWN:
		switch( LOWORD( wParam ))
		{
			case VK_ESCAPE:   // Pressing Esc switch FullScreen/Windowed
				if (g_ActiveConfig.bFullscreen)
				{
					DestroyWindow(hWnd);
					PostQuitMessage(0);
					ExitProcess(0);
					/* Get out of fullscreen
					g_Config.bFullscreen = false;

					// FullScreen - > Desktop
					ChangeDisplaySettings(NULL, 0);

					// Re-Enable the cursor
					ShowCursor(TRUE);
				
					RECT rcdesktop;
					RECT rc = {0, 0, 640, 480};
					GetWindowRect(GetDesktopWindow(), &rcdesktop);

					int X = (rcdesktop.right-rcdesktop.left)/2 - (rc.right-rc.left)/2;
					int Y = (rcdesktop.bottom-rcdesktop.top)/2 - (rc.bottom-rc.top)/2;
					// SetWindowPos to the center of the screen
					SetWindowPos(hWnd, NULL, X, Y, 640, 480, SWP_NOREPOSITION | SWP_NOZORDER);

					// Set new window style FS -> Windowed
					SetWindowLong(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

					// Eventually show the window!
					EmuWindow::Show();*/
				}
				/*
				else
				{
					// Get into fullscreen
					g_Config.bFullscreen = true;
					DEVMODE dmScreenSettings;
					memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
					RECT rcdesktop;
					GetWindowRect(GetDesktopWindow(), &rcdesktop);

					// Desktop -> FullScreen
					dmScreenSettings.dmSize			= sizeof(dmScreenSettings);
					dmScreenSettings.dmPelsWidth	= rcdesktop.right;
					dmScreenSettings.dmPelsHeight	= rcdesktop.bottom;
					dmScreenSettings.dmBitsPerPel	= 32;
					dmScreenSettings.dmFields = DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT;
					if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
						return 0;
					// Disable the cursor
					ShowCursor(FALSE);

					// SetWindowPos to the upper-left corner of the screen
					SetWindowPos(hWnd, NULL, 0, 0, rcdesktop.right, rcdesktop.bottom, SWP_NOREPOSITION | SWP_NOZORDER);
					
					// Set new window style -> PopUp
					SetWindowLong(hWnd, GWL_STYLE, WS_POPUP);
					
					// Eventually show the window!
					EmuWindow::Show();
				}*/
				break;
		}
		g_VideoInitialize.pKeyPress(LOWORD(wParam), GetAsyncKeyState(VK_SHIFT) != 0, GetAsyncKeyState(VK_CONTROL) != 0);
		break;

	/* Post thes mouse events to the main window, it's nessesary because in difference to the
	   keyboard inputs these events only appear here, not in the parent window or any other WndProc()*/
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_LBUTTONDBLCLK:
		PostMessage(GetParentWnd(), iMsg, wParam, lParam);
	break;

	case WM_CLOSE:
		Fifo_ExitLoopNonBlocking();
		Shutdown();
		// Simple hack to easily exit without stopping. Hope to fix the stopping errors soon.
		ExitProcess(0);
		return 0;

	case WM_DESTROY:
		//Shutdown();
		//PostQuitMessage( 0 );
		break;
	
	case WM_SIZE:
		// Reset the D3D Device here
		// Also make damn sure that this is not called from inside rendering a frame :P
		// Renderer::ReinitView();
		break;

	case WM_SYSCOMMAND:
		switch (wParam) 
		{
		case SC_SCREENSAVE:
		case SC_MONITORPOWER:
			return 0;
		}
		break;
	}

	return DefWindowProc(hWnd, iMsg, wParam, lParam);
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
	wndClass.hCursor = LoadCursor( NULL, IDC_ARROW );
	wndClass.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = m_szClassName;
	wndClass.hIconSm = LoadIcon( NULL, IDI_APPLICATION );

	m_hInstance = hInstance;
	RegisterClassEx( &wndClass );

	if (parent)
	{
		m_hWnd = CreateWindow(m_szClassName, title,
			WS_CHILD,
			CW_USEDEFAULT, CW_USEDEFAULT,CW_USEDEFAULT, CW_USEDEFAULT,
			parent, NULL, hInstance, NULL );

		m_hParent = parent;

		ShowWindow(m_hWnd, SW_SHOWMAXIMIZED);            
	}
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

		m_hWnd = CreateWindow(m_szClassName, title,
			style,
			rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top,
			parent, NULL, hInstance, NULL );

		g_winstyle = GetWindowLong( m_hWnd, GWL_STYLE );
		g_winstyle &= ~WS_MAXIMIZE & ~WS_MINIMIZE; // remove minimize/maximize style

	}

	return m_hWnd;
}

void Show()
{
	ShowWindow(m_hWnd, SW_SHOW);
	BringWindowToTop(m_hWnd);
	UpdateWindow(m_hWnd);
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

void SetSize(int width, int height)
{
	RECT rc = {0, 0, width, height};
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, false);

	int w = rc.right - rc.left;
	int h = rc.bottom - rc.top;

	rc.left = (1280 - w)/2;
	rc.right = rc.left + w;
	rc.top = (1024 - h)/2;
	rc.bottom = rc.top + h;
	::MoveWindow(m_hWnd, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, TRUE);
}

}
