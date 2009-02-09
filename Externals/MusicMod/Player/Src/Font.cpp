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


#include "Font.h"



HFONT hFont = NULL;



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Font::Create()
{
	hFont = CreateFont(
		-11,                          // int nHeight
		0,                            // int nWidth
		0,                            // int nEscapement
		0,                            // int nOrientation
		FW_REGULAR,                   // int fnWeight
		FALSE,                        // DWORD fdwItalic
		FALSE,                        // DWORD fdwUnderline
		FALSE,                        // DWORD fdwStrikeOut
		ANSI_CHARSET,                 // DWORD fdwCharSet
		OUT_TT_PRECIS,                // DWORD fdwOutputPrecision
		CLIP_DEFAULT_PRECIS,          // DWORD fdwClipPrecision
		ANTIALIASED_QUALITY,          // DWORD fdwQuality
		FF_DONTCARE | DEFAULT_PITCH,  // DWORD fdwPitchAndFamily
		TEXT( "Verdana" )             // LPCTSTR lpszFace
	);
	
	return ( hFont != NULL );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Font::Destroy()
{
	if( !hFont ) return false;
	DeleteObject( hFont );
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Font::Apply( HWND hwnd )
{
	if( !hFont ) return false;
	SendMessage(
		hwnd,
		WM_SETFONT,
		( WPARAM )hFont,
		FALSE
	);
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
HFONT Font::Get()
{
	return hFont;
}
