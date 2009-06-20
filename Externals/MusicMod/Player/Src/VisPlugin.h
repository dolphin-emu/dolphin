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


#ifndef PA_VIS_PLUGIN_H
#define PA_VIS_PLUGIN_H



#include "Global.h"
#include "Plugin.h"
#include "Winamp/Vis.h"
#include "VisModule.h"
#include "Lock.h"
#include <vector>

using namespace std;



typedef winampVisHeader * ( * WINAMP_VIS_GETTER )( void );



class VisModule;
class VisPlugin;

extern vector <VisPlugin *> vis_plugins;
extern Lock VisLock;



////////////////////////////////////////////////////////////////////////////////
/// Winamp visualization plugin wrapper
////////////////////////////////////////////////////////////////////////////////
class VisPlugin : public Plugin
{
public:
	VisPlugin( TCHAR * szDllpath, bool bKeepLoaded );
	
	bool Load();
	bool Unload();

	TCHAR *     GetTypeString()     { return TEXT( "Visual" ); }
	int         GetTypeStringLen()  { return 6;                }
	PluginType  GetType()           { return PLUGIN_TYPE_VIS;  }
	
	bool IsActive();
	
private:
	winampVisHeader *    header;
	vector<VisModule *>  modules;


	friend void ContextMenuVis( VisPlugin * vis, POINT * p );
};



#endif // PA_VIS_PLUGIN_H
