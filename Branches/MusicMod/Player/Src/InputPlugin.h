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


#ifndef PA_INPUT_PLUGIN_H
#define PA_INPUT_PLUGIN_H



#include "Global.h"
#include "Plugin.h"
#include "Playback.h"
#include "Playlist.h"
#include "Winamp/In2.h"
#include <map>
#include <vector>

using namespace std;



typedef In_Module * ( * WINAMP_INPUT_GETTER )( void );



class InputPlugin;

extern map <TCHAR *, InputPlugin *, TextCompare> ext_map;
extern vector <InputPlugin *> input_plugins;
extern InputPlugin * active_input_plugin;



////////////////////////////////////////////////////////////////////////////////
/// Winamp input plugin wrapper
////////////////////////////////////////////////////////////////////////////////
class InputPlugin : public Plugin
{
public:
	InputPlugin( TCHAR * szDllpath, bool bKeepLoaded );
	~InputPlugin();
	
	bool Load();
	bool Unload();
	
	TCHAR *     GetTypeString()     { return TEXT( "Input" );   }
	int         GetTypeStringLen()  { return 5;                 }
	PluginType  GetType()           { return PLUGIN_TYPE_INPUT; }

	inline bool IsActive() { return false; }
		
	bool About( HWND hParent );
	bool Config( HWND hParent );

	In_Module *  plugin;  // I moved this from private to public

private:
	TCHAR *      szFilters;
	int          iFiltersLen;
	

	bool Integrate();
	bool DisIntegrate();


	// TODO
	friend bool OpenPlay( TCHAR * szFilename, int iNumber );
	friend bool Playback_PrevOrNext( bool bPrevOrNext );
	friend bool Playback::Play();
	friend bool Playback::Pause();
	friend bool Playback::Stop();
	friend bool Playback::UpdateSeek(); // this one calls some plugin-> members
	friend int  Playback::PercentToMs( float fPercent );
	friend bool Playback::SeekPercent( float fPercent );
	friend bool SeekRelative( int ms );
	friend void Playback_Volume_Set( int iVol );
	friend bool Playback::Pan::Set( int iPan );
	friend void Playback_Eq_Set( int iPresetIndex );
	
	friend void AddFiles();
	friend void VSAAdd( void * data, int timestamp );
	friend void VSAAddPCMData( void * PCMData, int nch, int bps, int timestamp );
	friend int  Playlist::GetTitle( int iIndex, char * szAnsiTitle, int iChars );
};



#endif // PA_INPUT_PLUGIN_H
