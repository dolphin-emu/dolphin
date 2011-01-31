
//________________________________________________________________________________________
// File description: Common plugin spec, version #1.0 maintained by F|RES

#ifndef _PLUGINS_H_INCLUDED__
#define _PLUGINS_H_INCLUDED__

// Includes
// ------------
// TODO: See if we can get rid of the windows.h include.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "Common.h"
#include "CommonTypes.h"

// Plugin communication. I place this here rather in Common.h to rebuild less if any of this is changed
// -----------------
enum PLUGIN_COMM
{
	// Begin at 10 in case there is already messages with wParam = 0, 1, 2 and so on
	WM_USER_PAUSE = 10,
	WM_USER_STOP,
	WM_USER_CREATE,
	WM_USER_SETCURSOR,
	WM_USER_KEYDOWN,
	WIIMOTE_DISCONNECT, // Disconnect Wiimote
	INPUT_FRAME_COUNTER // Wind back the frame counter for rerecording
};

// System specific declarations and definitions
// ------------

// TODO: get rid of this i think
#if !defined(_WIN32) && !defined(TRUE)
#define TRUE 1
#define FALSE 0
#endif

// Global values
// ------------

//enum STATE_MODE
//{
//	STATE_MODE_READ = 1,
//	STATE_MODE_WRITE,
//	STATE_MODE_MEASURE,
//};

// used for notification on emulation state
enum PLUGIN_EMUSTATE
{
	PLUGIN_EMUSTATE_PLAY = 1,
	PLUGIN_EMUSTATE_PAUSE,
	PLUGIN_EMUSTATE_STOP,
};

#endif // _PLUGINS_H_INCLUDED__
