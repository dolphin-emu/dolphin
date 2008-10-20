//__________________________________________________________________________________________________
// Common wiimote plugin spec, unversioned
//

#ifndef _WIIMOTE_H_INCLUDED__
#define _WIIMOTE_H_INCLUDED__

#include "PluginSpecs.h"

#include "ExportProlog.h"


typedef void (*TLog)(const char* _pMessage);

// Called when the Wiimote sends input reports to the Core.
// Payload: an L2CAP packet.
typedef void (*TWiimoteInput)(u16 _channelID, const void* _pData, u32 _Size);

// This data is passed from the core on initialization.
typedef struct
{
	HWND hWnd;
	TLog pLog;
	TWiimoteInput pWiimoteInput;
} SWiimoteInitialize;

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
// input:    WiimoteInitialize
// output:   none
//
EXPORT void CALL Wiimote_Initialize(SWiimoteInitialize _WiimoteInitialize);

// __________________________________________________________________________________________________
// Function: Wiimote_Shutdown
// Purpose:  This function is called when the emulator is closing
//           down allowing the DLL to de-initialise.
// input:    none
// output:   none
//
EXPORT void CALL Wiimote_Shutdown();

// __________________________________________________________________________________________________
// Function: Wiimote_Output
// Purpose:  An L2CAP packet is passed from the Core to the Wiimote,
//           on the HID OUTPUT channel.
// input:    Da pakket.
// output:   none
//
EXPORT void CALL Wiimote_Output(u16 _channelID, const void* _pData, u32 _Size);

// __________________________________________________________________________________________________
// Function: Wiimote_Input
// Purpose:  An L2CAP packet is passed from the Core to the Wiimote,
//           on the HID INPUT channel.
// input:    Da pakket.
// output:   none
//
EXPORT void CALL Wiimote_Input(u16 _channelID, const void* _pData, u32 _Size);

// __________________________________________________________________________________________________
// Function: Wiimote_Update
// Purpose:  This function is called periodically by the Core.
// input:    none
// output:   none
//
EXPORT void CALL Wiimote_Update();

// __________________________________________________________________________________________________
// Function: PAD_GetAttachedPads
// Purpose:  Get mask of attached pads (eg: controller 1 & 4 -> 0x9)
// input:	 none
// output:   number of pads
//
EXPORT unsigned int CALL Wiimote_GetAttachedControllers();

// __________________________________________________________________________________________________
// Function: Wiimote_DoState
// Purpose:  Saves/load state
// input/output: ptr
// input: mode
//
EXPORT void CALL Wiimote_DoState(void *ptr, int mode);

#include "ExportEpilog.h"

#endif	//_WIIMOTE_H_INCLUDED__
