#pragma once

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

struct SPADStatus
{
	u16	button;			// Or-ed PAD_BUTTON_* and PAD_TRIGGER_* bits
	u8	stickX;			// 0 <= stickX       <= 255
	u8	stickY;			// 0 <= stickY       <= 255
	u8	substickX;		// 0 <= substickX    <= 255
	u8	substickY;		// 0 <= substickY    <= 255
	u8	triggerLeft;	// 0 <= triggerLeft  <= 255
	u8	triggerRight;	// 0 <= triggerRight <= 255
	u8	analogA;		// 0 <= analogA      <= 255
	u8	analogB;		// 0 <= analogB      <= 255
	u8	err;			// one of PAD_ERR_* number
	bool	MicButton;	// This is hax for the mic device input...
};

// if plugin isn't initialized, init and load config
void PAD_Init();

void PAD_Shutdown();

void PAD_GetStatus(u8 _numPAD, SPADStatus* _pPADStatus);

// Function: Send keyboard input to the plugin
// Purpose:  
// input:   The key and if it's pressed or released
// output:  None
void PAD_Input(u16 _Key, u8 _UpDown);

// Function: PAD_Rumble
// Purpose:  Pad rumble!
// input:	 PAD number, Command type (Stop=0, Rumble=1, Stop Hard=2) and strength of Rumble
// output:   none
void PAD_Rumble(u8 _numPAD, u8 _uType, u8 _uStrength);
