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


#ifndef PA_TOOLBAR_H
#define PA_TOOLBAR_H



#include "Global.h"



extern HWND WindowRebar;
extern HWND WindowOrder;
extern HWND WindowEq;
extern HWND WindowSeek;

extern HWND WindowVis;

extern int iRebarHeight;



namespace Toolbar
{
	bool Create();
};



#endif // PA_TOOLBAR_H
