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


#ifndef PA_STATUS_H
#define PA_STATUS_H



#include "Global.h"



extern int iStatusHeight;
extern HWND WindowStatus;



bool BuildMainStatus();
bool StatusUpdate( TCHAR * szText );
void StatusReset();



#endif // PA_STATUS_H
