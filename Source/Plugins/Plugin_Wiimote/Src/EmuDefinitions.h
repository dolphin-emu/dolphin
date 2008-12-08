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

#ifndef _EMU_DEFINITIONS_
#define _EMU_DEFINITIONS_

#include "pluginspecs_wiimote.h"

#include <vector>
#include <string>
#include "Common.h"
#include "wiimote_hid.h"
#include "Console.h" // for startConsoleWin, wprintf, GetConsoleHwnd

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

// vars 
#define WIIMOTE_EEPROM_SIZE (16*1024)
#define WIIMOTE_REG_SPEAKER_SIZE 10
#define WIIMOTE_REG_EXT_SIZE 0x100
#define WIIMOTE_REG_IR_SIZE 0x34


} // namespace

#endif	//_EMU_DEFINITIONS_