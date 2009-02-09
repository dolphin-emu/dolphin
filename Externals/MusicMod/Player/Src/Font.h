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


#ifndef PA_FONT_H
#define PA_FONT_H



#include "Global.h"



namespace Font
{
	bool Create();
	bool Destroy();

	bool Apply( HWND hwnd );
	HFONT Get();
};



#endif // PA_FONT_H

