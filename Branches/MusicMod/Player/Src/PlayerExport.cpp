
//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯¯
#include <iostream>  // System

#include "../../Common/Src/Console.h" // Local common

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
int GlobalVolume;
int GlobalCurrentVolume;
bool GlobalPause;
bool TimerCreated = false;
bool Initialized = false;
/////////////////////////////////


// -------------------------
/* We keep the file in the playlist, even though we currently only every have one file here
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
	wprintf("Play file <%s>\n", FileName);

	// Check if the file exists
	if(GetFileAttributes(FileName) == INVALID_FILE_ATTRIBUTES)
	{
		wprintf("Warning: The file <%s> does not exist. Something is wrong.\n", FileName);
		return;
	}

	Playback::Stop();
	//wprintf("Stop\n");
	playlist->RemoveAll();
	//wprintf("RemoveAll\n");
	AddFileToPlaylist(FileName);
	//wprintf("addfiletoplaylist\n");

	// Play the file
	Playback::Play();

	CurrentlyPlayingFile = FileName;

	// ---------------------------------------------------------------------------------------
	// Set volume. This must probably be done after the dll is loaded.
	//Output_SetVolume( Playback::Volume::Get() );
	//wprintf("Volume(%i)\n", Playback::Volume::Get());
	// ---------------------------------------------------------------------------------------

	GlobalPause = false;
}

void Player_Pause()
{
	if (!GlobalPause)
	{
		wprintf("DLL > Pause\n");
		Playback::Pause();
		GlobalPause = true;
	}
	else
	{
		wprintf("DLL > UnPause from Pause\n");
		Player_Unpause();
		GlobalPause = false;
	}
}

void Player_Unpause()
{
	wprintf("DLL > UnPause\n");
	Playback::Play();
	GlobalPause = false;
}

void Player_Mute()
{
	wprintf("DLL > Mute <%i> <%i>\n", GlobalVolume, GlobalCurrentVolume);

	// ---------------------------------------------------------------------------------------
	// Set volume. For some reason Playback::Volume::Get() is not updated when we run Output_SetVolume()
	// I probably disabled it by mistake, so we only keep track of the volume with GlobalVolume from now on
	if(GlobalCurrentVolume > 0)
	{
		Output_SetVolume( 0 );
		GlobalCurrentVolume = 0;
		wprintf("DLL > Volume <%i>\n", GlobalCurrentVolume);
	}
	else
	{
		//Output_SetVolume( GlobalVolume );
		Output_SetVolume( GlobalVolume );
		GlobalCurrentVolume = GlobalVolume;
		wprintf("DLL > Volume <%i>\n", GlobalCurrentVolume);
	}

	//wprintf("Volume(%i)\n", Playback::Volume::Get());
	// ---------------------------------------------------------------------------------------
}



void Player_Volume(int a)
{
	GlobalVolume = a;
	Output_SetVolume( GlobalVolume );
	wprintf("DLL > Volume <%i> <%i>\n", GlobalVolume, GlobalCurrentVolume);
}

void ShowConsole()
{
	//startConsoleWin(130, 10000, "Console Window"); // give room for 2000 rows
	StartConsoleWin(100, 2000, "ConsoleDLL"); // give room for 2000 rows
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