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


#ifndef PA_PREFS_H
#define PA_PREFS_H



#include "Global.h"
#include "Winamp/wa_ipc.h"



namespace Prefs
{
	bool Create();
	bool Destroy();
	
	bool Show();
	bool Show( prefsDlgRec * PageData );

	bool AddPage( prefsDlgRec * PageData );
};



#endif // PA_PREFS_H
