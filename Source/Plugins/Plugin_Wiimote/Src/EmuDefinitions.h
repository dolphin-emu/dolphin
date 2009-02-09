// Copyright (C) 2003-2008 Dolphin Project.

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
#include "Logging.h" // for startConsoleWin, Console::Print, GetConsoleHwnd

extern SWiimoteInitialize g_WiimoteInitialize;
//extern void __Log(int log, const char *format, ...);
//extern void __Log(int log, int v, const char *format, ...);

namespace WiiMoteEmu
{
	
//******************************************************************************
// Definitions and variable declarations
//******************************************************************************

/* Libogc bounding box, in smoothed IR coordinates: 232,284 792,704, however, it was
   possible for me to get a better calibration with these values, if they are not
   universal for all PCs we have to make a setting for it. */
#define LEFT 266
#define TOP 211
#define RIGHT 752
#define BOTTOM 728
#define SENSOR_BAR_RADIUS 200

#define wLEFT 332
#define wTOP 348
#define wRIGHT 693
#define wBOTTOM 625
#define wSENSOR_BAR_RADIUS 200

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

/* Classic Controller calibration. 0x80 is the neutral for the analog triggers and
   sticks. The left analog range is 0x1c - 0xe4 and the right is 0x28 - 0xd8.
   We use this range because it's closest to the GC controller range. */
static const u8 classic_calibration[] =
{
	0xe4,0x1c,0x80,0xe4, 0x1c,0x80,0xd8,0x28,
	0x80,0xd8,0x28,0x80, 0x20,0x20,0x95,0xea
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

} // namespace

#endif	//_EMU_DEFINITIONS_
