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

#ifndef _EMU_DEFINITIONS_
#define _EMU_DEFINITIONS_

#include <vector>
#include <string>

#include "pluginspecs_wiimote.h"
#include "Common.h"
#include "wiimote_hid.h"
#include "EmuDefinitions.h"
#include "Encryption.h"

extern SWiimoteInitialize g_WiimoteInitialize;

namespace WiiMoteEmu
{
	
//******************************************************************************
// Definitions and variable declarations
//******************************************************************************
u8 g_Eeprom[MAX_WIIMOTES][WIIMOTE_EEPROM_SIZE];
u8 g_RegExt[MAX_WIIMOTES][WIIMOTE_REG_EXT_SIZE];
u8 g_RegMotionPlus[MAX_WIIMOTES][WIIMOTE_REG_EXT_SIZE];
u8 g_RegSpeaker[MAX_WIIMOTES][WIIMOTE_REG_SPEAKER_SIZE];
u8 g_RegIr[MAX_WIIMOTES][WIIMOTE_REG_IR_SIZE];
u8 g_IRClock[MAX_WIIMOTES];
u8 g_IR[MAX_WIIMOTES];
u8 g_Leds[MAX_WIIMOTES];
u8 g_Speaker[MAX_WIIMOTES];
u8 g_SpeakerMute[MAX_WIIMOTES];

u8 g_RegExtTmp[WIIMOTE_REG_EXT_SIZE];

int g_ID; // Current refreshing Wiimote
bool g_ReportingAuto[MAX_WIIMOTES]; // Auto report or passive report
u8 g_ReportingMode[MAX_WIIMOTES]; // The reporting mode and channel id
u16 g_ReportingChannel[MAX_WIIMOTES];

wiimote_key g_ExtKey[MAX_WIIMOTES]; // The extension encryption key
bool g_Encryption; // Encryption on or off

// Gamepad input
int NumPads = 0, NumGoodPads = 0; // Number of goods pads
std::vector<InputCommon::CONTROLLER_INFO> joyinfo;
CONTROLLER_MAPPING_WII WiiMapping[MAX_WIIMOTES];

// Keyboard input
bool KeyStatus[LAST_CONSTANT];

} // namespace

#endif	//_EMU_DECLARATIONS_

