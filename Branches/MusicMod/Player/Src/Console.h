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


#ifndef PA_CONSOLE_H
#define PA_CONSOLE_H



#include "Global.h"



extern HWND WindowConsole;


////////////////////////////////////////////////////////////////////////////////
/// Logging console window
////////////////////////////////////////////////////////////////////////////////
namespace Console
{
	bool Create();
	bool Destroy();
	
	bool Popup();

	bool Append( TCHAR * szText );
}



#endif // PA_CONSOLE_H
