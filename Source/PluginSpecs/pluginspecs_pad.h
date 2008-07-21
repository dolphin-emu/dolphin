//__________________________________________________________________________________________________
// Common pad plugin spec, version #1.0 maintained by F|RES
//

#ifndef _PAD_H_INCLUDED__
#define _PAD_H_INCLUDED__

#include "PluginSpecs.h"

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef _WIN32
#define EXPORT				__declspec(dllexport)
#define CALL				__cdecl
#else
#define CALL
#define EXPORT
#endif

#define PAD_ERR_NONE            0
#define PAD_ERR_NO_CONTROLLER   -1
#define PAD_ERR_NOT_READY       -2
#define PAD_ERR_TRANSFER        -3

#define PAD_USE_ORIGIN          0x0080
#define PAD_BUTTON_LEFT         0x0001
#define PAD_BUTTON_RIGHT        0x0002
#define PAD_BUTTON_DOWN         0x0004
#define PAD_BUTTON_UP           0x0008
#define PAD_TRIGGER_Z           0x0010
#define PAD_TRIGGER_R           0x0020
#define PAD_TRIGGER_L           0x0040
#define PAD_BUTTON_A            0x0100
#define PAD_BUTTON_B            0x0200
#define PAD_BUTTON_X            0x0400
#define PAD_BUTTON_Y            0x0800
#define PAD_BUTTON_START        0x1000

typedef void (*TLog)(const char* _pMessage);

typedef struct
{
	HWND hWnd;
	TLog				pLog;	
} SPADInitialize;

typedef struct
{
	unsigned short	button;                 // Or-ed PAD_BUTTON_* bits
	unsigned char	stickX;                 // 0 <= stickX       <= 255
	unsigned char	stickY;                 // 0 <= stickY       <= 255
	unsigned char	substickX;              // 0 <= substickX    <= 255
	unsigned char	substickY;              // 0 <= substickY    <= 255
	unsigned char	triggerLeft;            // 0 <= triggerLeft  <= 255
	unsigned char	triggerRight;           // 0 <= triggerRight <= 255
	unsigned char	analogA;                // 0 <= analogA      <= 255
	unsigned char	analogB;                // 0 <= analogB      <= 255
	signed char		err;                    // one of PAD_ERR_* number
} SPADStatus;

/////////////////////////////////////////////////////////////////////////////////////////////////////
// I N T E R F A C E ////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

// __________________________________________________________________________________________________
// Function: GetDllInfo
// Purpose:  This function allows the emulator to gather information
//           about the DLL by filling in the PluginInfo structure.
// input:    a pointer to a PLUGIN_INFO structure that needs to be
//           filled by the function. (see def above)
// output:   none
//
EXPORT void CALL GetDllInfo(PLUGIN_INFO* _pPluginInfo);

// __________________________________________________________________________________________________
// Function: DllAbout
// Purpose:  This function is optional function that is provided
//           to give further information about the DLL.
// input:    a handle to the window that calls this function
// output:   none
//
EXPORT void CALL DllAbout(HWND _hParent);

// __________________________________________________________________________________________________
// Function: DllConfig
// Purpose:  This function is optional function that is provided
//           to allow the user to configure the DLL
// input:    a handle to the window that calls this function
// output:   none
//
EXPORT void CALL DllConfig(HWND _hParent);

// __________________________________________________________________________________________________
// Function: 
// Purpose:  
// input:    SPADInitialize
// output:   none
//
EXPORT void CALL PAD_Initialize(SPADInitialize _PADInitialize);

// __________________________________________________________________________________________________
// Function: PAD_Shutdown
// Purpose:  This function is called when the emulator is closing
//           down allowing the DLL to de-initialise.
// input:    none
// output:   none
//
EXPORT void CALL PAD_Shutdown();

// __________________________________________________________________________________________________
// Function: 
// Purpose:  
// input:   
// output:   
//
EXPORT void CALL PAD_GetStatus(BYTE _numPAD, SPADStatus* _pPADStatus);

// __________________________________________________________________________________________________
// Function: PAD_Rumble
// Purpose:  Pad rumble!
// input:	 PAD number, Command type (Stop=0, Rumble=1, Stop Hard=2) and strength of Rumble 
// output:   none
//
EXPORT void CALL PAD_Rumble(BYTE _numPAD, unsigned int _uType, unsigned int _uStrength);

// __________________________________________________________________________________________________
// Function: PAD_GetNumberOfPads
// Purpose:  Get number of pads (it is flag eg: controller 1 & 4 -> 5)
// input:	 none
// output:   number of pads
//
EXPORT unsigned int CALL PAD_GetNumberOfPads();

// __________________________________________________________________________________________________
// Function: SaveLoadState
// Purpose:  Saves/load state
// input:    pointer to a 32k scratchpad/saved state
// output:   writes or reads to the scratchpad, returning size of data
// TODO:     save format should be standardized so that save 
//		     states don't become plugin dependent which would suck
//
EXPORT unsigned int CALL SaveLoadState(char *ptr, BOOL save);
#undef CALL
#if defined(__cplusplus)
}
#endif
#endif
