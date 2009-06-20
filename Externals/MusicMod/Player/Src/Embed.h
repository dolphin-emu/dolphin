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


#ifndef PA_EMBED_H
#define PA_EMBED_H



#include "Global.h"
#include "Winamp/wa_ipc.h"



////////////////////////////////////////////////////////////////////////////////
/// Embed window service.
/// Winamp provides embed windows so plugins don't have to take care
/// of window skinning. A plugin let's Winamp create an embed window
/// and uses this new window as parent for its own window.
////////////////////////////////////////////////////////////////////////////////
namespace Embed
{
	HWND Embed( embedWindowState * ews );
};



#endif // PA_EMBED_H
