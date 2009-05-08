// ================================================================================================
// File description
// ----------------
/* In the GUI build there is a timer that is initiated with SetTimer() that will setup a timer that
   calls the lpTimerFunc function pointer as long as your program is running a message loop. If it
   doesn't run one, like in a console application (the NOGUI build), you'll have to use a different
   kind of timer API, like timeSetEvent() or CreateThreadPoolTimer().  These timers use a different
   thread to make the callback so be careful to properly lock and synchronize. */
// ================================================================================================

// ================================================================================================
// Library requirements
// ----------------
// This program needs winmm.lib. There's no simpler or better way to make a timer withouth it.
// ================================================================================================

// ================================================================================================
// Includes
// ----------------
#include <iostream>
//using namespace std;
#include <windows.h>
#include <mmsystem.h>
#include "Global.h"
#include "PlayerExport.h"

#include "InputPlugin.h"
// ================================================================================================


////////////////////////////////////////////////////////////////////////////////////////////
// Declarations and definitions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

void MakeTime();

int g_Stop = 0;
extern std::string CurrentlyPlayingFile;
extern bool GlobalPause;
///////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////
// Manage restart when playback for a file has reached the end of the file
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#ifdef _M_X64
	void CALLBACK Update()
#else
	void CALLBACK Update(unsigned int,unsigned int,unsigned long,unsigned long,unsigned long)
#endif
{
	//INFO_LOG(AUDIO,"DLL > Update() > Begin (%i)\n", active_input_plugin);

	// --------------------------------
	// Manage restart when playback for a file has reached the end of the file
	// --------------
	// Check if the input plugin is activated
	if(!active_input_plugin || !active_input_plugin->plugin)
	{
		//INFO_LOG(AUDIO,"The input plugin is not activated yet\n");
	}
	else
	{
		const int ms_len = active_input_plugin->plugin->GetLength();
		const int ms_cur = active_input_plugin->plugin->GetOutputTime();

		// Get the current playback progress
		float progress;
		if(ms_len > 0)
			progress = (float)ms_cur / ms_len;
		else
			progress = 0;

		if (  progress > 0.7 ) // Only show this if we are getting close to the end, for bugtesting
									// basically
		{
			//INFO_LOG(AUDIO,"Playback progress <%i of %i>\n", ms_cur, ms_len);
		}

		// Because cur never go all the way to len we can't use a == comparison. Insted of this
		// we could also check if the location is the same as right before.
		if(ms_cur > ms_len - 1000 && !GlobalPause) // avoid restarting in cases where we just pressed pause
		{
			INFO_LOG(AUDIO,"Restart <%s>\n", CurrentlyPlayingFile.c_str());
			Player_Play((char *)CurrentlyPlayingFile.c_str());
		}
	}
	// --------------
	
	//INFO_LOG(AUDIO,"Make new time\n");
	MakeTime(); // Make a new one
}

void MakeTime()
{
	timeSetEvent(
	2000, // Interval in ms
	0, 
	#ifdef _M_X64
		(LPTIMECALLBACK) Update, // The function
	#else
		Update,
	#endif
	0, 
	0); 
}
///////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////
// Start the timer
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
int MainTimer()
{
	MakeTime();

	//while( g_Stop == 0 )
	//{
	//	cout << ".";
	//}

	//INFO_LOG(AUDIO,"MakeTime\n");

	return 0;
}
///////////////////////////
