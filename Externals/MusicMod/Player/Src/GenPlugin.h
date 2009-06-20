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


#ifndef PA_GEN_PLUGIN_H
#define PA_GEN_PLUGIN_H



#include "Global.h"
#include "Plugin.h"
#include "Winamp/Gen.h"
#include <vector>

using namespace std;



typedef winampGeneralPurposePlugin * ( * WINAMP_GEN_GETTER )( void );



class GenPlugin;

extern vector <GenPlugin *> gen_plugins;



////////////////////////////////////////////////////////////////////////////////
/// Winamp general purpose plugin wrapper
////////////////////////////////////////////////////////////////////////////////
class GenPlugin : public Plugin
{
public:
	GenPlugin( TCHAR * szDllpath, bool bKeepLoaded );
	
	bool Load();
	bool Unload();

	TCHAR *     GetTypeString()     { return TEXT( "General" ); }
	int         GetTypeStringLen()  { return 7;                 }
	PluginType  GetType()           { return PLUGIN_TYPE_GEN;   }
	
	inline bool IsActive() { return IsLoaded(); }
	
	bool Config();
	
	inline bool AllowRuntimeUnload() { return ( iHookerIndex == -1 ) || ( iHookerIndex == iWndprocHookCounter - 1 ); }

private:
	winampGeneralPurposePlugin * plugin;
};



#endif // PA_GEN_PLUGIN_H
