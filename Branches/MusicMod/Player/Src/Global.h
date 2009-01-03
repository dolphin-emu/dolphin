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



//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯¯
#include "../../Common/Src/Console.h" // Local common
/////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Declarations and definitions
// ¯¯¯¯¯¯¯¯¯¯

// This will build Plainamp without the GUI
#define NOGUI


// =======================================================================================
// Because there are undefined in 64 bit it's easy to redefine them in case we use 64 bit
// ---------------------
#ifdef _M_X64
	#define GWL_WNDPROC        (-4)
	#define GetWindowLong  GetWindowLongPtrA // or GetWindowLongPtr
#endif
// =======================================================================================

////////////////////////////////////


// =======================================================================================
// Back to Plainamp code
// ---------------------
#ifndef PA_GLOBAL_H
#define PA_GLOBAL_H



// #include "ide_devcpp/Plainamp_Private.h"



#ifdef UNICODE
# define PA_UNICODE
#else
# ifdef _UNICODE
#  define PA_UNICODE
# endif
#endif


// For GetLongPathName
#if _WIN32_WINDOWS < 0x0410
# undef _WIN32_WINDOWS
# define _WIN32_WINDOWS 0x0410
#endif


#define WIN32_LEAN_AND_MEAN

/*
#ifndef WINVER
# define WINVER 0x0500
#else
# if (WINVER < 0x0500)
#  undef WINVER
#  define WINVER 0x0500
# endif
#endif
*/

#include <windows.h>
#include <tchar.h>
#include <stdio.h>



#ifndef _WIN32_IE
# define _WIN32_IE 0x0400
#else
# if (_WIN32_IE < 0x0400)
#  undef _WIN32_IE
#  define _WIN32_IE 0x0400
# endif
#endif

#include <commctrl.h>





extern HINSTANCE g_hInstance;

extern TCHAR *  szHomeDir;
extern int      iHomeDirLen;

extern TCHAR *  szPluginDir;
extern int      iPluginDirLen;



/*
inline int abs( int x )
{
	return ( x < 0 ) ? -x : x;
}
*/

inline int MIN( int a, int b )
{
	return ( a < b ) ? a : b;
}

inline int MAX( int a, int b )
{
	return ( a > b ) ? a : b;
}




// Typo help
#define UNIT      UINT
#define UINT_PRT  UINT_PTR



struct TextCompare
{
	bool operator()( const TCHAR * a, const TCHAR * b ) const
	{
		return _tcscmp( a, b ) < 0;
	}
};


#endif // PA_GLOBAL_H

