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



//////////////////////////////////////////////////////////////////////////////////////////
// Usage instructions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯

// =======================================================================================
// Plugins
// ---------------------------------------------------------------------------------------
// Plainamp setup > The output plugin must be manually loaded and activated the first time it's used.
// After that the ini saves OutputPluginActive___out_wave_gpl.dll=1. Input plugins are automatically
// found, loaded and activated.
// =======================================================================================


// =======================================================================================
// The ini path szIniPath
// ---------------------------------------------------------------------------------------
/* We will get problems if the program can't find the ini settings. Plugins will not be loaded,
   or loadedand then unloaded before activated, or not working. */
// =======================================================================================

///////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯¯
#include <iostream>
//#include "Unicode.h"
//#include "Output.h"
#include <windows.h>
#include <string>

#include "Global.h" // Local
#include "Font.h"
#include "InputPlugin.h"
#include "OutputPlugin.h"
#include "VisPlugin.h"
#include "DspPlugin.h"
#include "GenPlugin.h"
#include "Main.h"
#include "Rebar.h"
#include "Playlist.h"
#include "Status.h"
#include "PluginManager.h"
#include "Prefs.h"
#include "Config.h"
#include "Emabox/Emabox.h"
#include "Console.h" 

#include "PlayerExport.h"  // DLL Player
/////////////////////////



//////////////////////////////////////////////////////////////////////////////////////////
// Declarations and definitions
// ¯¯¯¯¯¯¯¯¯¯


// -------------------------
// Keys
// ---------
#define PLUS_ALT            ( FVIRTKEY | FALT )
#define PLUS_CONTROL        ( FVIRTKEY | FCONTROL )
#define PLUS_CONTROL_ALT    ( FVIRTKEY | FCONTROL | FALT )
#define PLUS_CONTROL_SHIFT  ( FVIRTKEY | FCONTROL | FSHIFT )
#define PLUS_SHIFT          ( FVIRTKEY | FSHIFT )
// -------------


HINSTANCE g_hInstance = NULL; // extern
HINSTANCE hInstance = NULL; // extern

TCHAR * szHomeDir = NULL; // extern
int     iHomeDirLen = 0; // extern

TCHAR * szPluginDir = NULL; // extern
int     iPluginDirLen = 0; // extern


// -------------------------
/* Read global settings from the ini file. They are read from the ini file through Config.h.
		Usage: ( &where to place it, the option name, public or private, default value) */
// ---------
TCHAR szCurDir[ MAX_PATH + 1 ] = TEXT( "" );
ConfCurDir ccdCurDir( szCurDir, TEXT( "CurDir" ) );
// -------------------------

bool bWarnPluginsMissing;
ConfBool cbWarnPluginsMissing( &bWarnPluginsMissing, TEXT( "WarnPluginsMissing" ), CONF_MODE_PUBLIC, false );

bool bLoop;
ConfBool cbLoop( &bLoop, TEXT( "Loop" ), CONF_MODE_PUBLIC, false );
/////////////////////////



////////////////////////////////////////////////////////////////////////////////
// The first function that is called by the exe
////////////////////////////////////////////////////////////////////////////////
void Player_Main(bool Console)
{
	// =======================================================================================
	// Set variables
		//g_hInstance = hInstance;
	// =======================================================================================
	
	//printf( "DLL > main_dll() opened\n" );

	if (Console) Player_Console(true);

	//MessageBox(0, "main() opened", "", 0);
	//printf( "main() opened\n" );
	INFO_LOG(AUDIO,"\n=========================================================\n\n\n" );
	//INFO_LOG(AUDIO, "DLL > Player_Main() > Begin\n" );
	INFO_LOG(AUDIO, "DLL > Settings:\n", bLoop);

		
	// =======================================================================================
	// Load full config from ini file
	//Conf::Init( hInstance );
	Conf::Init( );

	// ---------------------------------------------------------------------------------------
	
	INFO_LOG(AUDIO, "DLL > Loop: %i\n", bLoop);
	INFO_LOG(AUDIO, "DLL > WarnPluginsMissing: %i\n", bWarnPluginsMissing);	
	// ---------------------------------------------------------------------------------------

	// =======================================================================================


	// =======================================================================================
	/* Get home dir. We use the TCHAR type for the dirname to insure us against foreign letters
	   in the dirnames */
	szHomeDir = new TCHAR[ MAX_PATH + 1 ];
	iHomeDirLen = GetModuleFileName( NULL, szHomeDir, MAX_PATH );
	//if( !iHomeDirLen ) return 1;	

	// ---------------------------------------------------------------------------------------
	// Walk through the pathname and look for backslashes
	TCHAR * walk = szHomeDir + iHomeDirLen - 1;
	while( ( walk > szHomeDir ) && ( *walk != TEXT( '\\' ) ) ) walk--;
	// ---------------------------------------------------------------------------------------

	walk++;
	*walk = TEXT( '\0' );
	iHomeDirLen = walk - szHomeDir;
	// =======================================================================================


	// =======================================================================================
	/* Get plugins dir. Notice to change the number 8 in two places below if the dir name
	   is changed */
	szPluginDir = new TCHAR[ MAX_PATH + 1 ];
	memcpy( szPluginDir, szHomeDir, iHomeDirLen * sizeof( TCHAR ) );
	memcpy( szPluginDir + iHomeDirLen, TEXT( "PluginsMusic" ), 12 * sizeof( TCHAR ) );
	szPluginDir[ iHomeDirLen + 12 ] = TEXT( '\0' );
	INFO_LOG(AUDIO,"DLL > Plugindir: %s\n", szPluginDir);
	// =======================================================================================
	#ifndef NOGUI
		Font::Create();
		//Console::Append( TEXT( "Winmain.cpp called Font::Create()" ) );
	#endif


	// ---------------------------------------------------------------------------------------
	// Set volume. This must probably be done after the dll is loaded.
	//GlobalVolume = Playback::Volume::Get(); // Don't bother with this for now
	//GlobalCurrentVolume = GlobalVolume;
	//Output_SetVolume( GlobalVolume );
	INFO_LOG(AUDIO,"DLL > Volume: %i\n\n", GlobalVolume);
	// ---------------------------------------------------------------------------------------


	// =======================================================================================
	// The only thing this function currently does is creating the Playlist.
	// =======================================================================================
	BuildMainWindow();

	//addfiletoplaylist("c:\\zelda\\demo37_01.ast");
	//addfiletoplaylist("c:\\zelda\\demo36_02.ast");
	//Console::Append( TEXT( "Winmain.cpp called BuildMainWindow()" ) );

	//Prefs::Create(); // This creates windows preferences
	//Console::Append( TEXT( "Winmain.cpp called Prefs::Create()" ) );

	// Find plugins
	Plugin::FindAll<InputPlugin> ( szPluginDir, TEXT( "in_*.dll"  ), true  );
	Plugin::FindAll<OutputPlugin>( szPluginDir, TEXT( "out_*.dll" ), false );	
	Plugin::FindAll<VisPlugin>   ( szPluginDir, TEXT( "vis_*.dll" ), false );	
	Plugin::FindAll<DspPlugin>   ( szPluginDir, TEXT( "dsp_*.dll" ), false );	
	Plugin::FindAll<GenPlugin>   ( szPluginDir, TEXT( "gen_*.dll" ), true  );	

	//INFO_LOG(AUDIO, "Winmain.cpp > PluginManager::Fill()\n" );
	PluginManager::Fill();

	//INFO_LOG(AUDIO, "Winmain.cpp > PluginManager::Fill()\n" );



	// =======================================================================================
		
		// Check plugin presence
	// =======================================================================================


	// =======================================================================================

		// Todo: all the rest...
		// ACCEL accels[] = {

	// =======================================================================================



	// =======================================================================================
	
	//Playback::Play();
	//play_file("C:\\opening.hps");
	//play_file("C:\\Files\\Spel och spelfusk\\Console\\Gamecube\\Code\\Dolphin\\Binary\\win32\\evt_x_event_00.dsp");
	//printf("Winmain.cpp called Playback.cpp:Playback::Play()\n");

	// =======================================================================================
	// ---- Set volume and get current location ----
	// Somehow we don't have access to active_input_plugin->plugin->GetLength() from here so
	// we have to call it in Playback::UpdateSeek() instead
	//Sleep(1000);

	//Playback::UpdateSeek();
	//Output_SetVolume( 100 ); // volume is better set from the ini file
	// ---------------------------------------------------------------------------------------
	// =======================================================================================


	// =======================================================================================
	// Check the plugins
	if( input_plugins.empty() )
	{
		INFO_LOG(AUDIO,"\n *** Warning: No valid input plugins found\n\n");
	}
	else
	{
		INFO_LOG(AUDIO," >>> These valid input plugins were found:\n");
		for(int i = 0; i < input_plugins.size(); i++)
			INFO_LOG(AUDIO,"       %i: %s\n", (i + 1), input_plugins.at(i)->GetFilename());
		INFO_LOG(AUDIO,"\n");
	}

	// The input plugins are never activated here, they are activate for each file
	if( !active_input_plugin || !active_input_plugin->plugin )
	{
		// INFO_LOG(AUDIO,"The input plugin is not activated yet\n");
	}
	else
	{
		//const int ms_len = active_input_plugin->plugin->GetLength();
		//const int ms_cur = active_input_plugin->plugin->GetOutputTime();
		//INFO_LOG(AUDIO,"We are at <%i of %i>\n", ms_cur, ms_len);
	}
	// ---------------------------------------------------------------------------------------
	if( active_output_count > 0 )
	{
		// Show current playback progress
		/*int res_temp;
		for( int i = 0; i < active_output_count; i++ )
		{
			res_temp = active_output_plugins[ i ]->plugin->GetOutputTime();
		}
		INFO_LOG(AUDIO,"Playback progress <%i>\n", res_temp);*/
	}
	else
	{
		INFO_LOG(AUDIO,"\n *** Warning: The output plugin is not working\n\n");
	}
	// =======================================================================================

	// =======================================================================================
	// Start the timer
	if(!TimerCreated && bLoop) // Only create this the first time
	{
		//INFO_LOG(AUDIO,"Created the timer\n");
		MakeTime();
		TimerCreated = true;
	}
	// =======================================================================================

	INFO_LOG(AUDIO, "\n=========================================================\n\n" );
	//INFO_LOG(AUDIO, "DLL > main_dll() > End\n\n\n" );

	//std::cin.get();
}






// =======================================================================================
// Should I use this?
void close()
{

	printf( "The Winmain.cpp message loop was reached\n" );
	
	// Message loop
	//std::cin.get();

	Playback::Stop(); // If we don't call this before we unload the dll we get a crash
	
	printf("We are now past the message loop\n" );

	//DestroyAcceleratorTable( hAccel );


	// =======================================================================================
	// Input
	vector <InputPlugin *>::iterator iter_input = input_plugins.begin();
	while( iter_input != input_plugins.end() )
	{
		( *iter_input )->Unload();
		iter_input++;
	}
	
	// Output
	vector <OutputPlugin *>::iterator iter_output = output_plugins.begin();
	while( iter_output != output_plugins.end() )
	{
		( *iter_output )->Unload();
		iter_output++;
	}

	// General
	vector <GenPlugin *>::iterator iter_gen = gen_plugins.begin();
	while( iter_gen != gen_plugins.end() )
	{
		( *iter_gen )->Unload();
		iter_gen++;
	}
	// =======================================================================================
	
	
	// TODO: create main::destroy
	// UnregisterClass( PA_CLASSNAME, g_hInstance );

	//Prefs::Destroy();

	//Font::Destroy();

/*	
	delete [] szPluginDir;
	delete [] szHomeDir;
*/	

	// ---------------------------------------------------------------------------------------
	// We don't save any changes
	//Conf::Write();

	//printf("Winmain.cpp called Conf::Write(), the last function\n");
	// ---------------------------------------------------------------------------------------

	//std::cin.get(); // Let use see all messages

	//return 0;
}
// =======================================================================================
