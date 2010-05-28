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

#include "../../../Core/InputCommon/Src/InputCommon.h" // Core
#include "../../../Core/InputCommon/Src/SDL_Util.h"
#ifdef _WIN32
#include "../../../Core/InputCommon/Src/XInput_Util.h"
#endif

#include "Common.h"
#include "pluginspecs_wiimote.h"

#include "wiimote_hid.h" // Local
#include "Encryption.h"

#include "UDPWiimote.h"

#if defined(HAVE_X11) && HAVE_X11
#include <X11/X.h>
#endif

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
extern u8 g_MotionPlus[MAX_WIIMOTES];
extern u8 g_MotionPlusLastWriteReg[MAX_WIIMOTES];
extern u8 g_SpeakerMute[MAX_WIIMOTES];


extern int g_MotionPlusReadError[MAX_WIIMOTES];
extern u8 g_RegExtTmp[WIIMOTE_REG_EXT_SIZE];

extern int g_ID;
extern bool g_ReportingAuto[MAX_WIIMOTES];
extern u8 g_ReportingMode[MAX_WIIMOTES];
extern u16 g_ReportingChannel[MAX_WIIMOTES];
extern bool g_InterleavedData[MAX_WIIMOTES];

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
//I'll clean this up, we don't need the whole register
static const u8 motionplus_register[] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0x79, 0x83, 0x73, 0x54, 0x72, 0xE8, 0x30, 0xC3, 0xCC, 0x4A, 0x34, 0xFC, 0xC8, 0x4F, 0xCC, 0x5B, //MP calibration data
0x77, 0x49, 0x75, 0xA4, 0x73, 0x9A, 0x35, 0x52, 0xCA, 0x22, 0x37, 0x26, 0x2D, 0xE5, 0xB5, 0xA2, //MP calibration data
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //Extensions(NC) calibration data
0x1e, 0x54, 0x74, 0xa2, 0x96, 0xec, 0x2b, 0xd6, 0xe1, 0xef, 0xc3, 0xf7, 0x84, 0x9e, 0x06, 0xbb,
0x39, 0x33, 0x3d, 0x20, 0x97, 0xed, 0x75, 0x52, 0xfd, 0x98, 0xaf, 0xd8, 0xc9, 0x5a, 0x17, 0x23,
0x74, 0x3a, 0x49, 0xd3, 0xb9, 0xf6, 0xff, 0x4f, 0x34, 0xa8, 0x6d, 0xc4, 0x96, 0x5c, 0xcd, 0xb2,
0x33, 0x78, 0x98, 0xe9, 0xa9, 0x7f, 0xf7, 0x5e, 0x07, 0x87, 0xbb, 0x29, 0x01, 0x2b, 0x70, 0x3f,
0xC1, 0x6D, 0x84, 0x2A, 0xD8, 0x6F, 0x8A, 0xE5, 0x2D, 0x3B, 0x7B, 0xCC, 0xD2, 0x59, 0xD5, 0xD1,
0x9F, 0x5B, 0x6F, 0xAE, 0x82, 0xDE, 0xEA, 0xC3, 0x73, 0x42, 0x06, 0xA9, 0x77, 0xFF, 0x61, 0xA8,
0x1A, 0x70, 0xE4, 0x16, 0x90, 0x7A, 0x80, 0xF7, 0x79, 0x4B, 0x41, 0x18, 0x82, 0x6C, 0x62, 0x1A,
0x3B, 0xBF, 0xFC, 0xFF, 0x2C, 0xF2, 0x32, 0x97, 0xB8, 0x2F, 0x17, 0xE7, 0xBD, 0x35, 0x1D, 0x0A,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0x55, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x10, 0xff, 0xff, 0x00, 0x00, 0xa6, 0x20, 0x00, 0x05
};

/* Default calibration for the nunchuck. It should be written to 0x20 - 0x3f of the
   extension register. 0x80 is the neutral x and y accelerators and 0xb3 is the
   neutral z accelerometer that is adjusted for gravity. */
static const u8 nunchuck_calibration[] =
{
	0x80,0x80,0x80,0x00, // accelerometer x, y, z neutral
	0xb3,0xb3,0xb3,0x00, //  x, y, z g-force values 

	0xff, 0x00, 0x80, 0xff, // 0xff max, 0x00 min, 0x80 = analog stick x and y axis center
	0x00, 0x80, 0xec, 0x41	// checksum on the last two bytes 
};


static const u8 wireless_nunchuck_calibration[] =
{
	128, 128, 128, 0x00,
	181, 181, 181, 0x00,
	255, 0, 125, 255,
	0, 126, 0xed, 0x43
};
/* Default calibration for the motion plus*/
static const u8 motion_plus_calibration[] =
{
	0x79, 0xbe, 0x77, 0x5a, 0x77, 0x38, 0x2f, 0x90, 0xcd, 0x3b, 0x2f, 0xfd, 0xc8, 0x29, 0x9c, 0x75,
	0x7d, 0xd4, 0x78, 0xef, 0x78, 0x8a, 0x35, 0xa6, 0xc9, 0x9b, 0x33, 0x50, 0x2d, 0x00, 0xbd, 0x23
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
static const u8 wbb_id[]	= { 0x00, 0x00, 0xa4, 0x20, 0x4, 0x02 };
static const u8 motionplus_id[]	= { 0x00, 0x00, 0xa4, 0x20, 0x04, 0x05 };
static const u8 motionplusnunchuk_id[]	= { 0x00, 0x00, 0xa6, 0x20, 0x05, 0x05 };
//initial control packet for datatransfers over 0x37 reports
static const u8 motionpluscheck_id[]	= { 0xa3, 0x62, 0x45, 0xaa, 0x04, 0x02};
// The id for nothing inserted
static const u8 nothing_id[]	= { 0x00, 0x00, 0x00, 0x00, 0x2e, 0x2e };
// The id for a partially inserted extension
static const u8 partially_id[]	= { 0x00, 0x00, 0x00, 0x00, 0xff, 0xff };


enum EExtensionType
{
	EXT_NONE = 0,
	EXT_NUNCHUK,
	EXT_CLASSIC_CONTROLLER,
	EXT_GUITARHERO,
	EXT_WBB,
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

struct UDPWiiSettings
{
	bool enable;
	bool enableAccel;
	bool enableButtons;
	bool enableIR;
	bool enableNunchuck;
	char port[10];
	UDPWiimote *instance;
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
	int Diagonal;

	int Source; // 0: inactive, 1: emu, 2: real
	bool bSideways;
	bool bUpright;
	bool bMotionPlusConnected;
	bool bWiiAutoReconnect;
	int iExtensionConnected;

	STiltMapping Tilt;
	SStickMapping Stick;
	int Button[LAST_CONSTANT];
	SMotion Motion;

	UDPWiiSettings UDPWM;
};

// Gamepad input
extern int NumPads, NumGoodPads; // Number of goods pads
extern std::vector<InputCommon::CONTROLLER_INFO> joyinfo;
extern CONTROLLER_MAPPING_WII WiiMapping[4];

} // namespace

#endif	//_EMU_DEFINITIONS_
