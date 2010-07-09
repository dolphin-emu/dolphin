//__________________________________________________________________________________________________
// Common wiimote plugin spec, unversioned
//

#ifndef _WIIMOTE_H_INCLUDED__
#define _WIIMOTE_H_INCLUDED__

#include "PluginSpecs.h"
#include "ExportProlog.h"

#if !defined _WIN32 && !defined __APPLE__
#include "Config.h"
#endif


typedef void (*TLogv)(const char* _pMessage, int _v);

// This is called when the Wiimote sends input reports to the Core.
// Payload: an L2CAP packet.
typedef void (*TWiimoteInterruptChannel)(int _number, u16 _channelID, const void* _pData, u32 _Size);
typedef bool (*TRendererHasFocus)(void);

// This data is passed from the core on initialization.
typedef struct
{
	HWND hWnd;
#if defined HAVE_X11 && HAVE_X11
	void *pXWindow;
#endif
	u32 ISOId;
	TLogv pLog;
	TWiimoteInterruptChannel pWiimoteInterruptChannel;
	TRendererHasFocus pRendererHasFocus;
} SWiimoteInitialize;


// I N T E R F A C E 


// __________________________________________________________________________________________________
// Function: Wiimote_Output
// Purpose:  An L2CAP packet is passed from the Core to the Wiimote,
//           on the HID CONTROL channel.
// input:    Da pakket.
// output:   none
//
EXPORT void CALL Wiimote_ControlChannel(int _number, u16 _channelID, const void* _pData, u32 _Size);

// __________________________________________________________________________________________________
// Function: Send keyboard input to the plugin
// Purpose:
// input:   The key and if it's pressed or released
// output:  None
//
EXPORT void CALL Wiimote_Input(u16 _Key, u8 _UpDown);

// __________________________________________________________________________________________________
// Function: Wiimote_InterruptChannel
// Purpose:  An L2CAP packet is passed from the Core to the Wiimote,
//           on the HID INTERRUPT channel.
// input:    Da pakket.
// output:   none
//
EXPORT void CALL Wiimote_InterruptChannel(int _number, u16 _channelID, const void* _pData, u32 _Size);

// __________________________________________________________________________________________________
// Function: Wiimote_Update
// Purpose:  This function is called periodically by the Core.
// input:    none
// output:   none
//
EXPORT void CALL Wiimote_Update(int _number);

// __________________________________________________________________________________________________
// Function: Wiimote_UnPairWiimotes
// Purpose:  Unpair real wiimotes to safe battery
// input:	 none
// output:   number of unpaired wiimotes
//
EXPORT unsigned int CALL Wiimote_UnPairWiimotes();

// __________________________________________________________________________________________________
// Function: PAD_GetAttachedPads
// Purpose:  Get mask of attached pads (eg: controller 1 & 4 -> 0x9)
// input:	 none
// output:   number of pads
//
EXPORT unsigned int CALL Wiimote_GetAttachedControllers();

#include "ExportEpilog.h"

#endif	//_WIIMOTE_H_INCLUDED__
