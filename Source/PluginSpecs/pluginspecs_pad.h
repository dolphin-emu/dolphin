//__________________________________________________________________________________________________
// Common pad plugin spec, version #1.0 maintained by F|RES
//

#ifndef _PAD_H_INCLUDED__
#define _PAD_H_INCLUDED__

#include "PluginSpecs.h"

#include "ExportProlog.h"

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
	HWND			hWnd;
	TLog			pLog;
} SPADInitialize;

typedef struct
{
	unsigned short	button;                 // Or-ed PAD_BUTTON_* and PAD_TRIGGER_* bits
	unsigned char	stickX;                 // 0 <= stickX       <= 255
	unsigned char	stickY;                 // 0 <= stickY       <= 255
	unsigned char	substickX;              // 0 <= substickX    <= 255
	unsigned char	substickY;              // 0 <= substickY    <= 255
	unsigned char	triggerLeft;            // 0 <= triggerLeft  <= 255
	unsigned char	triggerRight;           // 0 <= triggerRight <= 255
	unsigned char	analogA;                // 0 <= analogA      <= 255
	unsigned char	analogB;                // 0 <= analogB      <= 255
	bool			MicButton;				// HAX
	signed char		err;                    // one of PAD_ERR_* number
} SPADStatus;

/////////////////////////////////////////////////////////////////////////////////////////////////////
// I N T E R F A C E ///////////////////////////////////////////////////////////

// __________________________________________________________________________________________________
// Function:
// Purpose:  
// input:   
// output:   
//
EXPORT void CALL PAD_GetStatus(u8 _numPAD, SPADStatus* _pPADStatus);

// __________________________________________________________________________________________________
// Function: Send keyboard input to the plugin
// Purpose:  
// input:   The key and if it's pressed or released
// output:  None
//
EXPORT void CALL PAD_Input(u8 _Key, u8 _UpDown);

// __________________________________________________________________________________________________
// Function: PAD_Rumble
// Purpose:  Pad rumble!
// input:	 PAD number, Command type (Stop=0, Rumble=1, Stop Hard=2) and strength of Rumble
// output:   none
//
EXPORT void CALL PAD_Rumble(u8 _numPAD, unsigned int _uType, unsigned int _uStrength);

// __________________________________________________________________________________________________
// Function: PAD_GetAttachedPads
// Purpose:  Get mask of attached pads (eg: controller 1 & 4 -> 0x9)
// input:	 none
// output:   number of pads
//
EXPORT unsigned int CALL PAD_GetAttachedPads();

#include "ExportEpilog.h"
#endif
