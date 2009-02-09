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


#ifndef PA_DSP_PLUGIN_H
#define PA_DSP_PLUGIN_H



#include "Global.h"
#include "Plugin.h"
#include "Winamp/Dsp.h"
#include "Lock.h"
#include "DspModule.h"
#include <vector>

using namespace std;



typedef winampDSPHeader * ( * WINAMP_DSP_GETTER )( void );



class DspModule;
class DspPlugin;

extern vector <DspPlugin *> dsp_plugins;
extern Lock DspLock;



////////////////////////////////////////////////////////////////////////////////
/// Winamp DSP plugin wrapper
////////////////////////////////////////////////////////////////////////////////
class DspPlugin : public Plugin
{
public:
	DspPlugin( TCHAR * szDllpath, bool bKeepLoaded );
	
	bool Load();
	bool Unload();

	TCHAR *     GetTypeString()     { return TEXT( "DSP" );   }
	int         GetTypeStringLen()  { return 3;               }
	PluginType  GetType()           { return PLUGIN_TYPE_DSP; }

	bool IsActive();

private:
	winampDSPHeader *    header;
	vector<DspModule *>  modules;


	friend class DspModule;
	friend void ContextMenuDsp( DspPlugin * dsp, POINT * p );
};



#endif // PA_DSP_PLUGIN_H
