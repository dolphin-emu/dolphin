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

#ifndef WIIMOTE_EMU_H
#define WIIMOTE_EMU_H


#include <string>

#include "../../../Core/InputCommon/Src/SDL.h" // Core

#include "wiimote_hid.h"
#include "EmuDefinitions.h"

namespace WiiMoteEmu
{

u32 convert24bit(const u8* src);
u16 convert16bit(const u8* src);
void GetMousePos(float& x, float& y);

// General functions
void Initialize();
void DoState(void* ptr, int mode);
void Shutdown(void);
void InterruptChannel(u16 _channelID, const void* _pData, u32 _Size);
void ControlChannel(u16 _channelID, const void* _pData, u32 _Size) ;
void Update();

// Recordings
void LoadRecordedMovements();

// Registers and calibration values
void UpdateEeprom();
void SetDefaultExtensionRegistry();

// Gamepad
bool Search_Devices(std::vector<InputCommon::CONTROLLER_INFO> &_joyinfo, int &_NumPads, int &_NumGoodPads);
void GetJoyState(InputCommon::CONTROLLER_STATE_NEW &_PadState, InputCommon::CONTROLLER_MAPPING_NEW _PadMapping, int controller, int NumButtons);
void PitchDegreeToAccelerometer(float _Roll, float _Pitch, u8 &_x, u8 &_y, u8 &_z, bool RollOn, bool PitchOn);
void PitchAccelerometerToDegree(u8 _x, u8 _y, u8 _z, int &_Roll, int &_Pitch);


}; // WiiMoteEmu


#endif
