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


#ifndef PA_PLUGIN_MANAGER_H
#define PA_PLUGIN_MANAGER_H



#include "Global.h"
#include "Plugin.h"



extern HWND WindowManager;



void UpdatePluginStatus( Plugin * plugin, bool bLoaded, bool bActive );



namespace PluginManager
{
	bool Build();
	bool Fill();
	bool Destroy();
	
	bool Popup();
};



#endif // PA_PLUGIN_MANAGER_H
