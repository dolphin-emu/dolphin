// Copyright (C) 2003-2008 Dolphin Project.

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

#include "../../Core/Src/Core.h"
#include "Win32.h"

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
			break;

		case WM_CLOSE:
			//Core::SetState(Core::CORE_UNINITIALIZED);
			exit(0);
			return 0;

		case WM_DESTROY:
			//Shutdown();
			//PostQuitMessage( 0 );
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

        if (parent && windowed)
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
