
#include <windows.h>

#include "../../Core/Src/Core.h"
#include "Config.h"
#include "main.h"
#include "EmuWindow.h"

namespace EmuWindow
{
	HWND m_hWnd = NULL;
	HWND m_hParent = NULL;
	HINSTANCE m_hInstance = NULL;
	WNDCLASSEX wndClass;
	const TCHAR m_szClassName[] = "DolphinEmuWnd";
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
		HDC         hdc;
		PAINTSTRUCT ps;
		switch( iMsg )
		{
		case WM_PAINT:
			hdc = BeginPaint( hWnd, &ps );
			EndPaint( hWnd, &ps );
			return 0;

		case WM_KEYDOWN:
			switch( LOWORD( wParam ))
			{
			case VK_ESCAPE:     /*  Pressing esc quits */
				//DestroyWindow(hWnd);
				//PostQuitMessage(0);
				break;
				/*
				case MY_KEYS:
				hypotheticalScene->sendMessage(KEYDOWN...);
				*/
			}
			g_VideoInitialize.pKeyPress(LOWORD(wParam), GetAsyncKeyState(VK_SHIFT) != 0, GetAsyncKeyState(VK_CONTROL) != 0);
			break;

		case WM_CLOSE:
			//Core::SetState(Core::CORE_UNINITIALIZED);
			return 0;

		case WM_DESTROY:
			//Shutdown();
			//PostQuitMessage( 0 );
			break;
		
		case WM_SIZE:
			// Reset the D3D Device here
			// Also make damn sure that this is not called from inside rendering a frame :P
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
		wndClass.style  = CS_HREDRAW | CS_VREDRAW;
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
