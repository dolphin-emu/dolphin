// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _EMU_DECLARATIONS_
#define _EMU_DECLARATIONS_

#include <vector>
#include <string>

#include "../../../Core/InputCommon/Src/SDL.h" // Core
#include "../../../Core/InputCommon/Src/XInput.h"

#include "Common.h"
#include "pluginspecs_wiimote.h"

#include "wiimote_hid.h" // Local
#include "Encryption.h"

extern SWiimoteInitialize g_WiimoteInitialize;

namespace WiiMoteEmu
{
	
//******************************************************************************
// Definitions and variable declarations
//******************************************************************************

/* The Libogc bounding box in smoothed IR coordinates is 232,284 792,704. However, there is no
   universal standard that works with all games. They all use their own calibration. Also,
   there is no widescreen mode for the calibration, at least not in the games I tried, the
   game decides for example that a horizontal value of 500 is 50% from the left of the screen,
   and then that's the same regardless if we use the widescreen mode or not.*/
#define LEFT 266
#define TOP 215
#define RIGHT 752
#define BOTTOM 705
/* Since the width of the entire virtual screen is 1024 a reasonable sensor bar width is perhaps 200,
   given how small most sensor bars are compared to the total TV width. When I tried the distance with
   my Wiimote from around three meters distance from the sensor bar (that has around 15 cm beteen the
   IR lights) I got a dot distance of around 110 (and a dot size of between 1 and 2). */
#define SENSOR_BAR_RADIUS 100

// Movement recording
extern int g_RecordingPlaying[3]; 
extern int g_RecordingCounter[3];
extern int g_RecordingPoint[3];
extern double g_RecordingStart[3];
extern double g_RecordingCurrentTime[3];

// Registry sizes 
#define WIIMOTE_EEPROM_SIZE (16*1024)
#define WIIMOTE_EEPROM_FREE_SIZE 0x16ff
#define WIIMOTE_REG_SPEAKER_SIZE 10
#define WIIMOTE_REG_EXT_SIZE 0x100
#define WIIMOTE_REG_IR_SIZE 0x34

extern u8 g_Leds;
extern u8 g_Speaker;
extern u8 g_SpeakerVoice;
extern u8 g_IR;

extern u8 g_Eeprom[WIIMOTE_EEPROM_SIZE];
extern u8 g_RegSpeaker[WIIMOTE_REG_SPEAKER_SIZE];
extern u8 g_RegExt[WIIMOTE_REG_EXT_SIZE];
extern u8 g_RegExtTmp[WIIMOTE_REG_EXT_SIZE];
extern u8 g_RegIr[WIIMOTE_REG_IR_SIZE];

extern u8 g_ReportingMode;
extern u16 g_ReportingChannel;

// Ack delay
struct wm_ackdelay
{
	u8 Delay;
	u8 ReportID;
	u16 ChannelID;
	bool Sent;
};
extern std::vector<wm_ackdelay> AckDelay;

extern wiimote_key g_ExtKey; // extension encryption key
extern bool g_Encryption;

/* An example of a factory default first bytes of the Eeprom memory. There are differences between
   different Wiimotes, my Wiimote had different neutral values for the accelerometer. */
static const u8 EepromData_0[] = {
	0xA1, 0xAA, 0x8B, 0x99, 0xAE, 0x9E, 0x78, 0x30, 0xA7, 0x74, 0xD3,
	0xA1, 0xAA, 0x8B, 0x99, 0xAE, 0x9E, 0x78, 0x30, 0xA7, 0x74, 0xD3,
	// Accelerometer neutral values
	0x82, 0x82, 0x82, 0x15, 0x9C, 0x9C, 0x9E, 0x38, 0x40, 0x3E,
	0x82, 0x82, 0x82, 0x15, 0x9C, 0x9C, 0x9E, 0x38, 0x40, 0x3E
};

static const u8 EepromData_16D0[] = {
	0x00, 0x00, 0x00, 0xFF, 0x11, 0xEE, 0x00, 0x00,
	0x33, 0xCC, 0x44, 0xBB, 0x00, 0x00, 0x66, 0x99,
	0x77, 0x88, 0x00, 0x00, 0x2B, 0x01, 0xE8, 0x13
};


/* Default calibration for the nunchuck. It should be written to 0x20 - 0x3f of the
   extension register. 0x80 is the neutral x and y accelerators and 0xb3 is the
   neutral z accelerometer that is adjusted for gravity. */
static const u8 nunchuck_calibration[] =
{
	0x80,0x80,0x80,0x00, // accelerometer x, y, z neutral
	0xb3,0xb3,0xb3,0x00, //  x, y, z g-force values 

	0xff, 0x00, 0x80, 0xff, // 0x80 = analog stick x and y axis center
	0x00, 0x80, 0xee, 0x43 // checksum on the last two bytes
};
static const u8 wireless_nunchuck_calibration[] =
{
	128, 128, 128, 0x00,
	181, 181, 181, 0x00,
	255, 0, 125, 255,
	0, 126, 0xed, 0x43
};

/* Classic Controller calibration */
static const u8 classic_calibration[] =
{
	0xff,0x00,0x80, 0xff,0x00,0x80, 0xff,0x00,0x80, 0xff,0x00,0x80,
	0x00,0x00, 0x51,0xa6
};



/* The Nunchuck id. It should be written to the last bytes of the
   extension register */
static const u8 nunchuck_id[] =
{
	0x00, 0x00, 0xa4, 0x20, 0x00, 0x00
};

/* The Classic Controller id. It should be written to the last bytes of the
   extension register */
static const u8 classic_id[] =
{
	0x00, 0x00, 0xa4, 0x20, 0x01, 0x01
};

/* The GH3 guitar id. It should be written to the last bytes of the
   extension register */
static const u8 gh3glp_id[] =
{
	0x00, 0x00, 0xa4, 0x20, 0x01, 0x03
};

/* The GHWT drums id. It should be written to the last bytes of the
   extension register */
static const u8 ghwtdrums_id[] =
{
	0x01, 0x00, 0xa4, 0x20, 0x01, 0x03
};

/* The id for nothing inserted */
static const u8 nothing_id[] =
{
	0x00, 0x00, 0x00, 0x00, 0x2e, 0x2e
};

/* The id for a partially inserted extension */
static const u8 partially_id[] =
{
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff
};

// Gamepad input
extern int NumPads, NumGoodPads; // Number of goods pads
extern std::vector<InputCommon::CONTROLLER_INFO> joyinfo;
extern InputCommon::CONTROLLER_STATE_NEW PadState[4];
extern InputCommon::CONTROLLER_MAPPING_NEW PadMapping[4];

// Wiimote status
struct SDot
{
	int Rx, Ry, X, Y;
	bool Visible;
	int Size; 				// Size of the IR dot (0-15)
	int Order;				// Increasing order from low to higher x-axis values
};

struct SIR
{
	SDot Dot[4];
	int Distance;
};

// Keyboard input
struct KeyboardWiimote
{
	enum EKeyboardWiimote
	{
		A = 0, // Keyboard A and Mouse A
		B,
		ONE, TWO,
		P, M, H,
		L, R, U, D,
		SHAKE,
		PITCH_L, PITCH_R,
		MA, MB,
		LAST_CONSTANT
	};

	// Raw X and Y coordinate and processed X and Y coordinates
	SIR IR;
};
	extern KeyboardWiimote g_Wiimote_kbd;
struct KeyboardNunchuck
{
	enum EKeyboardNunchuck
	{
		// This is not allowed in Linux so we have to set the starting value manually
		#ifdef _WIN32
			Z = g_Wiimote_kbd.LAST_CONSTANT,
		#else
			Z = 16,
		#endif
		C,
		L, R, U, D,
		SHAKE,
		LAST_CONSTANT
	};
};
extern KeyboardNunchuck g_NunchuckExt;
struct KeyboardClassicController
{
	enum EKeyboardClassicController
	{
		// This is not allowed in Linux so we have to set the starting value manually
		#ifdef _WIN32
			A = g_NunchuckExt.LAST_CONSTANT,
		#else
			A = 23,
		#endif
		B, X, Y,
		P, M, H,
		Dl, Dr, Du, Dd,
		Tl, Tr, Zl, Zr,
		Ll, Lr, Lu, Ld,
		Rl, Rr, Ru, Rd,
		SHAKE,
		LAST_CONSTANT
	};
};
extern KeyboardClassicController g_ClassicContExt;

struct KeyboardGH3GLP
{
	enum EKeyboardGH3GLP
	{
		// This is not allowed in Linux so we have to set the starting value manually
		#ifdef _WIN32
			Green = g_ClassicContExt.LAST_CONSTANT,
		#else
			Green = 47,
		#endif
		Red, Yellow, Blue,
		Orange,Plus, Minus,
		Whammy,
		Al, Ar, Au, Ad,
		StrumUp, StrumDown,
		SHAKE,
		LAST_CONSTANT
	};
};
extern KeyboardGH3GLP g_GH3Ext;

extern bool KeyStatus[64];
} // namespace

#endif	//_EMU_DEFINITIONS_
