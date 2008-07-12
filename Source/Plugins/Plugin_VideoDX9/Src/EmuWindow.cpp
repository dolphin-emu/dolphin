
#include <windows.h>

#include "../../Core/Src/Core.h"
#include "EmuWindow.h"

namespace EmuWindow
{
	HWND m_hWnd;
	HINSTANCE m_hInstance;
	WNDCLASSEX wndClass;
	const TCHAR m_szClassName[] = "DolphinEmuWnd";

	HWND GetWnd()
	{

		return m_hWnd;
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
			break;

		case WM_CLOSE:
			//Core::SetState(Core::CORE_UNINITIALIZED);
			exit(0);
			return 0;

		case WM_DESTROY:
			//Shutdown();
			//PostQuitMessage( 0 );
			break;
		
		case WM_SIZE:
			// Reset the D3D Device here
			// Also make damn sure that this is not called from inside rendering a frame :P
			break;
		}

		return DefWindowProc(hWnd, iMsg, wParam, lParam);
	}


	HWND OpenWindow(HWND parent, HINSTANCE hInstance, bool windowed, int width, int height, const TCHAR *title)
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

		DWORD style = windowed ? WS_OVERLAPPEDWINDOW : WS_POPUP;

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
		return OpenWindow(hParent, hInstance, true, 640, 480, title);
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
		::MoveWindow(m_hWnd, rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top, TRUE);
	}
}