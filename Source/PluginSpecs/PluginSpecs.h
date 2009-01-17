//////////////////////////////////////////////////////////////////////////////////////////
//________________________________________________________________________________________
// File description: Common plugin spec, version #1.0 maintained by F|RES
///////////////////////////

#ifndef _PLUGINS_H_INCLUDED__
#define _PLUGINS_H_INCLUDED__


//////////////////////////////////////////////////////////////////////////////////////////
// Includes
// ------------
#ifdef _WIN32
	#include <windows.h>
#endif
#include "CommonTypes.h"
///////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Declarations and definitions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
#ifdef _WIN32
	#define EXPORT	__declspec(dllexport)
	#define CALL	__cdecl
#else
	#define EXPORT	__attribute__ ((visibility("default")))
	#define CALL
	#define __cdecl

	#ifndef TRUE
		#define TRUE 1
		#define FALSE 0 
	#endif

	// simulate something that looks like win32
	// long term, kill these
	#define HWND  void*
	#define HINSTANCE void*
#endif

#if defined(__cplusplus)
	extern "C" {
#endif

// Plugin types
enum PLUGIN_TYPE {
    PLUGIN_TYPE_VIDEO = 1,
    PLUGIN_TYPE_DVD,
    PLUGIN_TYPE_PAD,
    PLUGIN_TYPE_AUDIO,
    PLUGIN_TYPE_COMPILER,
    PLUGIN_TYPE_DSP,
    PLUGIN_TYPE_WIIMOTE,
};

#define STATE_MODE_READ    1
#define STATE_MODE_WRITE   2
#define STATE_MODE_MEASURE 3

typedef struct 
{
	u16 Version;		// Set to 0x0100
	PLUGIN_TYPE Type;	// Set to PLUGIN_TYPE_DVD
	char Name[100];		// Name of the DLL
} PLUGIN_INFO;

typedef struct 
{
    void *eventHandler;
    void *config;
    void *messageLogger;
} PLUGIN_GLOBALS;
///////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// GLOBAL I N T E R F A C E ///////////////////////////////////////////////////
// ____________________________________________________________________________
// Function: GetDllInfo
// Purpose:  This function allows the emulator to gather information
//           about the DLL by filling in the PluginInfo structure.
// input:    A pointer to a PLUGIN_INFO structure that needs to be
//           filled by the function. (see def above)
// output:   none
//
EXPORT void CALL GetDllInfo(PLUGIN_INFO* _pPluginInfo);

// ___________________________________________________________________________
// Function: DllConfig
// Purpose:  This function is optional function that is provided
//           to allow the user to configure the DLL
// input:    A handle to the window that calls this function
// output:   none
//
EXPORT void CALL DllConfig(HWND _hParent);

// ___________________________________________________________________________
// Function: DllDebugger
// Purpose:  Open the debugger
// input:    a handle to the window that calls this function
// output:   none
//
EXPORT void CALL DllDebugger(HWND _hParent, bool Show);

// ___________________________________________________________________________
// Function: DllSetGlobals
// Purpose:  Set the pointer for globals variables
// input:    a pointer to the global struct
// output:   none
//
EXPORT void CALL SetDllGlobals(PLUGIN_GLOBALS* _pPluginGlobals);
// __________________________________________________________________________________________________

// Function: Initialize
// Purpose: Initialize the plugin
// input:    Init
// output:   none
//
EXPORT void CALL Initialize(void *init);
// __________________________________________________________________________________________________
// Function: Shutdown
// Purpose:  This function is called when the emulator is shutting down
//           a game allowing the dll to de-initialise.
// input:    none
// output:   none
//
EXPORT void CALL Shutdown(void);

// __________________________________________________________________________________________________
// Function: DoState
// Purpose:  Saves/load state
// input/output: ptr
// input: mode
//
EXPORT void CALL DoState(unsigned char **ptr, int mode);
///////////////////////////////


#if defined(__cplusplus)
	}
#endif

#endif // _PLUGINS_H_INCLUDED__
