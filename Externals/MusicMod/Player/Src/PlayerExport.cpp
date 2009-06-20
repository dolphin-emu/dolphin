
//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯¯
#include <iostream>  // System

#include "Common.h" // Global common
#include "Log.h"

//#include "../../Common/Src/Console.h" // Local common

#include "OutputPlugin.h" // Local
#include "Playback.h"
#include "Playlist.h"

#define _DLL_PLAYER_H_
#include "PlayerExport.h"  // DLL Player
//////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Declarations and definitions
// ¯¯¯¯¯¯¯¯¯¯
std::string CurrentlyPlayingFile;
int GlobalVolume = -1;
bool GlobalMute = false;
bool GlobalPause;
bool TimerCreated = false;
bool Initialized = false;
/////////////////////////////////


// -------------------------
/* We keep the file in the playlist, even though we currently only ever have one file here
   at a time */
// ---------
void AddFileToPlaylist(char * a)
{
	//playlist->RemoveAll();

	#include "unicode.h"
	const int iLen = strlen(a); // I can't do this because I don't

	printf( "iLen <%i>\n", iLen );

	// ---------------------------------------------------------------------------------------
	// Do some string conversion
	TCHAR * szKeep;
	szKeep = new TCHAR[ iLen + 1 ];
	ToTchar( szKeep, a, iLen );
	szKeep[ iLen ] = TEXT( '\0' );
	playlist->PushBack( szKeep );
	// ---------------------------------------------------------------------------------------

	// If we added a second file the current index = -1 so we have to change that back
	playlist->SetCurIndex( 0 );
}



void Player_Play(char * FileName)
{
	INFO_LOG(AUDIO,"Play file <%s>\n", FileName);

	// Check if the file exists
	if(GetFileAttributes(FileName) == INVALID_FILE_ATTRIBUTES)
	{
		INFO_LOG(AUDIO,"Warning: The file <%s> does not exist. Something is wrong.\n", FileName);
		return;
	}

	Playback::Stop();
	//INFO_LOG(AUDIO,"Stop\n");
	playlist->RemoveAll();
	//INFO_LOG(AUDIO,"RemoveAll\n");
	AddFileToPlaylist(FileName);
	//INFO_LOG(AUDIO,"addfiletoplaylist\n");

	// Play the file
	Playback::Play();

	CurrentlyPlayingFile = FileName;

	// ---------------------------------------------------------------------------------------
	// Set volume. This must probably be done after the dll is loaded.
	//Output_SetVolume( Playback::Volume::Get() );
	//INFO_LOG(AUDIO,"Volume(%i)\n", Playback::Volume::Get());
	// ---------------------------------------------------------------------------------------

	GlobalPause = false;
}

void Player_Stop()
{
	Playback::Stop();
	//INFO_LOG(AUDIO,"Stop\n");
	playlist->RemoveAll();

	CurrentlyPlayingFile = "";

	GlobalPause = false;
}


void Player_Pause()
{
	if (!GlobalPause)
	{
		INFO_LOG(AUDIO,"DLL > Pause\n");
		Playback::Pause();
		GlobalPause = true;
	}
	else
	{
		INFO_LOG(AUDIO,"DLL > UnPause from Pause\n");
		Player_Unpause();
		GlobalPause = false;
	}
}

void Player_Unpause()
{
	INFO_LOG(AUDIO,"DLL > UnPause\n");
	Playback::Play();
	GlobalPause = false;
}


//////////////////////////////////////////////////////////////////////////////////////////
// Declarations and definitions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
/* About the volume: The colume is normally update for the output plugin in Output.cpp and for the
   inout plugin (why?) in Playback.cpp with Playback::Volume::Set(). But I don't know how that works
   so I only use GlobalVolume to keep track of the volume */
// ------------------------------
void Player_Mute(int Vol)
{
	if(GlobalVolume == -1) GlobalVolume = Vol;
	INFO_LOG(AUDIO,"DLL > Mute <%i> <%i>\n", GlobalVolume, GlobalMute);

	GlobalMute = !GlobalMute;

	// Set volume
	if(GlobalMute)
	{
		Output_SetVolume( 0 );
		INFO_LOG(AUDIO,"DLL > Volume <%i>\n", GlobalMute);
	}
	else
	{
		Output_SetVolume( GlobalVolume );
		INFO_LOG(AUDIO,"DLL > Volume <%i>\n", GlobalMute);
	}

	//INFO_LOG(AUDIO,"Volume(%i)\n", Playback::Volume::Get());
}
///////////////////////////////////////


void Player_Volume(int Vol)
{
	GlobalVolume = Vol;
	Output_SetVolume( GlobalVolume );
	//INFO_LOG(AUDIO,"DLL > Volume <%i> <%i>\n", GlobalVolume, GlobalCurrentVolume);
}

void ShowConsole()
{
//	Console::Open(100, 2000, "MusicMod", true); // give room for 2000 rows
}


void Player_Console(bool Console)
{
	if(Console)
		ShowConsole();
	else
		#if defined (_WIN32)
			FreeConsole();
		#endif
}
