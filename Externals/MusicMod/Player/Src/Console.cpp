////////////////////////////////////////////////////////////////////////////////
// Plainamp, Open source Winamp core
// 
// Copyright © 2005  Sebastian Pipping <webmaster@hartwork.org>
// 
// -->  http://www.hartwork.org
// 
// This source code is released under the GNU General Public License (GPL).
// See GPL.txt for details. Any non-GPL usage is strictly forbidden.
////////////////////////////////////////////////////////////////////////////////


#include "Console.h"
#include "Font.h"
#include "Main.h"
#include "Config.h"
#include <time.h>



HWND WindowConsole = NULL; // extern
int iNext = 0;

const int iMaxEntries = 10000;



WNDPROC WndprocConsoleBackup = NULL;
LRESULT CALLBACK WndprocConsole( HWND hwnd, UINT message, WPARAM wp, LPARAM lp );



bool bConsoleVisible;
WINDOWPLACEMENT WinPlaceConsole;

void WinPlaceConsoleCallback( ConfVar * var )
{
	if( !IsWindow( WindowConsole ) ) return;
	
	GetWindowPlacement( WindowConsole, &WinPlaceConsole );

	// MSDN:  If the window identified by the hWnd parameter
	//        is maximized, the showCmd member is SW_SHOWMAXIMIZED.
	//        If the window is minimized, showCmd is SW_SHOWMINIMIZED.
	//        Otherwise, it is SW_SHOWNORMAL.
	if( !bConsoleVisible )
	{
		WinPlaceConsole.showCmd = SW_HIDE;
	}
}

RECT rConsoleDefault = { 50, 400, 450, 700 };

ConfWinPlaceCallback cwpcWinPlaceConsole(
	&WinPlaceConsole,
	TEXT( "WinPlaceConsole" ),
	&rConsoleDefault,
	WinPlaceConsoleCallback
);



////////////////////////////////////////////////////////////////////////////////
/// Creates the console window.
/// Size and visibility is used from config.
///
/// @return Success flag
////////////////////////////////////////////////////////////////////////////////
bool Console::Create()
{
	WindowConsole = CreateWindowEx(
		WS_EX_TOOLWINDOW |                             // DWORD dwExStyle
			WS_EX_CLIENTEDGE,                          //
		TEXT( "LISTBOX" ),                             // LPCTSTR lpClassName
		TEXT( "Console" ),                             // LPCTSTR lpWindowName
		WS_VSCROLL |                                   // DWORD dwStyle
			LBS_DISABLENOSCROLL |                      //
			LBS_EXTENDEDSEL |                          //
			LBS_HASSTRINGS |                           //
			LBS_NOTIFY |                               //
			LBS_NOINTEGRALHEIGHT |                     //
			WS_POPUP |                                 //
			WS_OVERLAPPEDWINDOW,                       //
		rConsoleDefault.left,                          // int x
		rConsoleDefault.top,                           // int y
		rConsoleDefault.right - rConsoleDefault.left,  // int nWidth
		rConsoleDefault.bottom - rConsoleDefault.top,  // int nHeight
		WindowMain,                                    // HWND hWndParent
		NULL,                                          // HMENU hMenu
		g_hInstance,                                   // HINSTANCE hInstance
		NULL                                           // LPVOID lpParam
	);
	
	if( !WindowConsole ) return false;

	// A blank line at the bottom will give us more space
	SendMessage( WindowConsole, LB_INSERTSTRING, 0, ( LPARAM )TEXT( "" ) );
	
	Font::Apply( WindowConsole );

	bConsoleVisible = ( WinPlaceConsole.showCmd != SW_HIDE );
	SetWindowPlacement( WindowConsole, &WinPlaceConsole );
	
	// Exchange window procedure
	WndprocConsoleBackup = ( WNDPROC )GetWindowLong( WindowConsole, GWL_WNDPROC );
	if( WndprocConsoleBackup != NULL )
	{
		SetWindowLong( WindowConsole, GWL_WNDPROC, ( LONG )WndprocConsole );
	}
	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
/// Destroys the console window.
///
/// @return Success flag
////////////////////////////////////////////////////////////////////////////////
bool Console::Destroy()
{
	if( !WindowConsole ) return false;
	DestroyWindow( WindowConsole );
	return true;
}



////////////////////////////////////////////////////////////////////////////////
/// Pops up the console window.
///
/// @return Success flag
////////////////////////////////////////////////////////////////////////////////
bool Console::Popup()
{
	if( !WindowConsole ) return false;	
	if( !IsWindowVisible( WindowConsole ) )
	{
		ShowWindow( WindowConsole, SW_SHOW );
	}
	
	SetActiveWindow( WindowConsole );
	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
/// Adds a new entry at the end/bottom
///
/// @param szText Log entry
/// @return Success flag
////////////////////////////////////////////////////////////////////////////////
bool Console::Append( TCHAR * szText )
{
	if( !WindowConsole ) return false;
	if( iNext > iMaxEntries - 1 )
	{
		SendMessage(
			WindowConsole,
			LB_DELETESTRING,
			0,
			0
		);
		iNext--;
	}
	
	const int uTextLen = ( int )_tcslen( szText );
	TCHAR * szBuffer = new TCHAR[ 11 + uTextLen + 1 ];
	time_t now_time_t = time( NULL );
	struct tm * now_tm = localtime( &now_time_t );
	_tcsftime( szBuffer, 12, TEXT( "%H:%M:%S   " ), now_tm );
	memcpy( szBuffer + 11, szText, uTextLen * sizeof( TCHAR ) );
	szBuffer[ 11 + uTextLen ] = TEXT( '\0' );
	
	SendMessage( WindowConsole, LB_INSERTSTRING, iNext, ( LPARAM )szBuffer );
	SendMessage( WindowConsole, LB_SETSEL, FALSE, -1 );
	SendMessage( WindowConsole, LB_SETSEL, TRUE, iNext );
	SendMessage( WindowConsole, LB_SETTOPINDEX, iNext, 0 );
	iNext++;
	
	delete [] szBuffer;
	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
// 
////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndprocConsole( HWND hwnd, UINT message, WPARAM wp, LPARAM lp )
{
	switch( message )
	{
/*		
	case WM_CTLCOLORLISTBOX:
		if( ( HWND )lp == WindowConsole )
		{
			SetBkColor (( HDC )wp, GetSysColor(COLOR_3DFACE));
			return ( LRESULT )GetSysColorBrush(COLOR_3DFACE);
		}
		break;
*/

	case WM_SYSCOMMAND:
		// Hide instead of closing
		if( ( wp & 0xFFF0 ) == SC_CLOSE )
		{
			ShowWindow( hwnd, SW_HIDE );
			return 0;
		}
		break;
		
	case WM_DESTROY:
		cwpcWinPlaceConsole.TriggerCallback();
		cwpcWinPlaceConsole.RemoveCallback();
		break;

	case WM_SHOWWINDOW:
		bConsoleVisible = ( wp == TRUE );
		break;

	}
	return CallWindowProc( WndprocConsoleBackup, hwnd, message, wp, lp );
}
