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
#define SENSOR_BAR_WIDTH 200

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

extern u8 g_Eeprom[MAX_WIIMOTES][WIIMOTE_EEPROM_SIZE];
extern u8 g_RegExt[MAX_WIIMOTES][WIIMOTE_REG_EXT_SIZE];
extern u8 g_RegMotionPlus[MAX_WIIMOTES][WIIMOTE_REG_EXT_SIZE];
extern u8 g_RegSpeaker[MAX_WIIMOTES][WIIMOTE_REG_SPEAKER_SIZE];
extern u8 g_RegIr[MAX_WIIMOTES][WIIMOTE_REG_IR_SIZE];
extern u8 g_IRClock[MAX_WIIMOTES];
extern u8 g_IR[MAX_WIIMOTES];
extern u8 g_Leds[MAX_WIIMOTES];
extern u8 g_Speaker[MAX_WIIMOTES];
extern u8 g_SpeakerMute[MAX_WIIMOTES];

extern u8 g_RegExtTmp[WIIMOTE_REG_EXT_SIZE];

extern int g_ID;
extern bool g_ReportingAuto[MAX_WIIMOTES];
extern u8 g_ReportingMode[MAX_WIIMOTES];
extern u16 g_ReportingChannel[MAX_WIIMOTES];

extern wiimote_key g_ExtKey[MAX_WIIMOTES]; // extension encryption key
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

// Extension device IDs to be written to the last bytes of the extension reg
static const u8 nunchuck_id[]	= { 0x00, 0x00, 0xa4, 0x20, 0x00, 0x00 };
static const u8 classic_id[]	= { 0x00, 0x00, 0xa4, 0x20, 0x01, 0x01 };
static const u8 gh3glp_id[]		= { 0x00, 0x00, 0xa4, 0x20, 0x01, 0x03 };
static const u8 ghwtdrums_id[]	= { 0x01, 0x00, 0xa4, 0x20, 0x01, 0x03 };
// The id for nothing inserted
static const u8 nothing_id[]	= { 0x00, 0x00, 0x00, 0x00, 0x2e, 0x2e };
// The id for a partially inserted extension
static const u8 partially_id[]	= { 0x00, 0x00, 0x00, 0x00, 0xff, 0xff };


enum EExtensionType
{
	EXT_NONE = 0,
	EXT_NUNCHUCK,
	EXT_CLASSIC_CONTROLLER,
	EXT_GUITARHERO,
};

enum EInputType
{
	FROM_KEYBOARD,
	FROM_ANALOG1,
	FROM_ANALOG2,
	FROM_TRIGGER,
};

enum EButtonCode
{
	EWM_A = 0, // Keyboard A and Mouse A
	EWM_B,
	EWM_ONE, EWM_TWO,
	EWM_P, EWM_M, EWM_H,
	EWM_L, EWM_R, EWM_U, EWM_D,
	EWM_ROLL_L, EWM_ROLL_R,
	EWM_PITCH_U, EWM_PITCH_D,
	EWM_SHAKE,

	ENC_Z, ENC_C,
	ENC_L, ENC_R, ENC_U, ENC_D,
	ENC_ROLL_L, ENC_ROLL_R,
	ENC_PITCH_U, ENC_PITCH_D,
	ENC_SHAKE,

	ECC_A, ECC_B, ECC_X, ECC_Y,
	ECC_P, ECC_M, ECC_H,
	ECC_Tl, ECC_Tr, ECC_Zl, ECC_Zr,
	ECC_Dl, ECC_Dr, ECC_Du, ECC_Dd,
	ECC_Ll, ECC_Lr, ECC_Lu, ECC_Ld,
	ECC_Rl, ECC_Rr, ECC_Ru, ECC_Rd,

	EGH_Green, EGH_Red, EGH_Yellow, EGH_Blue, EGH_Orange,
	EGH_Plus, EGH_Minus, EGH_Whammy,
	EGH_Al, EGH_Ar, EGH_Au, EGH_Ad,
	EGH_StrumUp, EGH_StrumDown,

	LAST_CONSTANT,
};


union UAxis
{
	int Code[6];
	struct
	{
		int Lx;
		int Ly;
		int Rx;
		int Ry;
		int Tl; // Trigger
		int Tr; // Trigger
	};
};

struct SStickMapping
{
	int NC;
	int CCL;
	int CCR;
	int CCT; //Trigger
	int GH; // Analog Stick
	int GHB; // Whammy bar
};

struct STiltMapping
{
	int InputWM;
	int InputNC;
	bool RollInvert;
	bool PitchInvert;
	int RollDegree;
	bool RollSwing;
	int RollRange;
	int PitchDegree;
	bool PitchSwing;
	int PitchRange;
};


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

struct STiltData
{
	// FakeNoise is used to prevent disconnection
	// when there is no input for a long time
	int FakeNoise;
	int Shake;
	int Roll, Pitch;
	STiltData()
	{
		FakeNoise = 1;
		Shake = 0;
		Roll = 0;
		Pitch = 0;
	}
};

struct SMotion
{
	// Raw X and Y coordinate and processed X and Y coordinates
	SIR IR;
	STiltData TiltWM;
	STiltData TiltNC;
};


struct CONTROLLER_MAPPING_WII	// WII PAD MAPPING
{
	SDL_Joystick *joy;		// SDL joystick device
	UAxis AxisState;
	UAxis AxisMapping;		// 6 Axes (Main, Sub, Triggers)
	int TriggerType;		// SDL or XInput trigger
	int ID;					// SDL joystick device ID
	bool Rumble;
	int RumbleStrength;
	int DeadZoneL;			// Analog 1 Deadzone
	int DeadZoneR;			// Analog 2 Deadzone
	bool bCircle2Square;
	std::string Diagonal;

	int Source; // 0: inactive, 1: emu, 2: real
	bool bSideways;
	bool bUpright;
	bool bMotionPlusConnected;
	int iExtensionConnected;

	STiltMapping Tilt;
	SStickMapping Stick;
	int Button[LAST_CONSTANT];
	SMotion Motion;
};

// Gamepad input
extern int NumPads, NumGoodPads; // Number of goods pads
extern std::vector<InputCommon::CONTROLLER_INFO> joyinfo;
extern CONTROLLER_MAPPING_WII WiiMapping[4];
extern bool KeyStatus[64];

} // namespace

#endif	//_EMU_DEFINITIONS_
