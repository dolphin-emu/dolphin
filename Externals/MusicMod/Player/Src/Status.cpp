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


#include "Status.h"
#include "Main.h"
#include "Util.h"
#include "GlobalVersion.h"



int iStatusHeight = 40; // extern
HWND WindowStatus = NULL; // extern

const TCHAR * const szStatusDefault = TEXT( " " ) PLAINAMP_LONG_TITLE;



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool BuildMainStatus()
{
	LoadCommonControls();
	
	WindowStatus = CreateWindowEx(
		0,
		STATUSCLASSNAME,
		szStatusDefault,
		WS_CHILD |
			WS_VISIBLE,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		WindowMain,
		NULL,
		g_hInstance,
		NULL
	);

	if( !WindowStatus ) return false;
	
	RECT r = { 0, 0, 0, 0 };
	GetWindowRect( WindowStatus, &r );
	iStatusHeight = r.bottom - r.top;

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool StatusUpdate( TCHAR * szText )
{
	if( !WindowStatus ) return false;
	SetWindowText( WindowStatus, szText );
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void StatusReset()
{
	if( !WindowStatus ) return;
	SetWindowText( WindowStatus, szStatusDefault );
}
