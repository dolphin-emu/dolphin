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


#ifndef PA_OUTPUT_PLUGIN_H
#define PA_OUTPUT_PLUGIN_H



#include "Global.h"
#include "Plugin.h"
#include "Winamp/Out.h"
#include <vector>

using namespace std;



typedef Out_Module * ( * WINAMP_OUTPUT_GETTER )( void );


class OutputPlugin;

extern vector <OutputPlugin *> output_plugins;
extern OutputPlugin ** active_output_plugins;
extern int active_output_count;



////////////////////////////////////////////////////////////////////////////////
/// Winamp output plugin wrapper
////////////////////////////////////////////////////////////////////////////////
class OutputPlugin : public Plugin
{
public:
	OutputPlugin( TCHAR * szDllpath, bool bKeepLoaded );
	
	bool Load();
	bool Unload();

	TCHAR *     GetTypeString()     { return TEXT( "Output" );   }
	int         GetTypeStringLen()  { return 6;                  }
	PluginType  GetType()           { return PLUGIN_TYPE_OUTPUT; }
	
	inline bool IsActive() { return bActive; }
	
	bool About( HWND hParent );
	bool Config( HWND hParent );
	
	bool Start();
	bool Stop();

private:
	bool          bActive;
	int           iArrayIndex;
	Out_Module *  plugin;

	
	// TODO
	friend bool OpenPlay( TCHAR * szFilename, int iNumber );
	
	friend int  Output_Open( int samplerate, int numchannels, int bitspersamp, int bufferlenms, int prebufferms );
	friend void Output_Close();
	friend int  Output_Write( char * buf, int len );
	friend int  Output_CanWrite();
	friend int  Output_IsPlaying();
	friend int  Output_Pause( int pause );
	friend void Output_SetVolume( int volume );
	friend void Output_SetPan( int pan );
	friend void Output_Flush( int t );
	friend int  Output_GetOutputTime();
	friend int  Output_GetWrittenTime();
};



#endif // PA_OUTPUT_PLUGIN_H
