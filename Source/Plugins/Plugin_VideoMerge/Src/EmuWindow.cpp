
#include <windows.h>

// VideoCommon
#include "VideoConfig.h"
#include "Fifo.h"

#include "EmuWindow.h"

int OSDChoice = 0 , OSDTime = 0, OSDInternalW = 0, OSDInternalH = 0;

namespace EmuWindow
{

HWND m_hWnd = NULL;
HWND m_hParent = NULL;
HINSTANCE m_hInstance = NULL;
WNDCLASSEX wndClass;
const TCHAR m_szClassName[] = _T("DolphinEmuWnd");
int g_winstyle;
static volatile bool s_sizing;

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

	/* Post thes mouse events to the main window, it's nessesary because in difference to the
	   keyboard inputs these events only appear here, not in the parent window or any other WndProc()*/
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_LBUTTONDBLCLK:
		PostMessage(GetParentWnd(), iMsg, wParam, lParam);
		break;

	case WM_CLOSE:
		// When the user closes the window, we post an event to the main window to call Stop()
		// Which then handles all the necessary steps to Shutdown the core + the plugins
		if (m_hParent == NULL)
			PostMessage(m_hParent, WM_USER, WM_USER_STOP, 0);
		break;

	case WM_USER:
		if (wParam == WM_USER_KEYDOWN)
			OnKeyDown(lParam);
		else if (wParam == WIIMOTE_DISCONNECT)
			PostMessage(m_hParent, WM_USER, wParam, lParam);
		break;

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

// ---------------------------------------------------------------------
// OSD Menu
// -------------
// Let's begin with 3 since 1 and 2 are default Wii keys
// -------------
void OSDMenu(WPARAM wParam)
{
	switch( LOWORD( wParam ))
	{
	//case '3':
	//	OSDChoice = 1;
	//	// Toggle native resolution
	//	OSDInternalW = D3D::GetBackBufferWidth();
	//	OSDInternalH = D3D::GetBackBufferHeight();
	//	break;
	case '4':
		OSDChoice = 2;
		// Toggle aspect ratio
		g_Config.iAspectRatio = (g_Config.iAspectRatio + 1) & 3;
		break;
	case '5':
		OSDChoice = 3;
		PanicAlert("Toggling EFB copy not implemented!\n");
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

	m_hWnd = CreateWindow(m_szClassName, title, WS_CHILD,
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
	// 2. Make DX11 in fullscreen can be overlapped by other dialogs
	// 3. Request window sizes which actually make the client area map to a common resolution
	HWND Ret;
	int x=0, y=0, width=640, height=480;
	g_VideoInitialize.pRequestWindowSize(x, y, width, height);

	Ret = OpenWindow(hParent, hInstance, width, height, title);

	if (Ret)
	{
		Show();
	}
	return Ret;
}

void Close()
{
	if (m_hParent == NULL)
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

}
