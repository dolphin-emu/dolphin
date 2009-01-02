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


#ifndef PA_UNICODE_H
#define PA_UNICODE_H



#include "Global.h"



void ToAnsi( char * szDest, wchar_t * szSource, int iLen );
// void ToUnicode( wchar_t * szDest, char * szSource, int iLen )
void ToTchar( TCHAR * szDest, wchar_t * szSource, int iLen );
void ToTchar( TCHAR * szDest, char * szSource, int iLen );



#endif // PA_UNICODE_H
