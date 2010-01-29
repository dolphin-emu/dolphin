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

#ifndef WIIMOTE_EMU_H
#define WIIMOTE_EMU_H


#include <string>

#include "wiimote_hid.h"
#include "EmuDefinitions.h"
#include "ChunkFile.h"

namespace WiiMoteEmu
{

u32 convert24bit(const u8* src);
u16 convert16bit(const u8* src);

// General functions
void ResetVariables();
void Initialize();
void Shutdown();
void InitCalibration();
void UpdateExtRegisterBlocks(int Slot);

void InterruptChannel(int _number, u16 _channelID, const void* _pData, u32 _Size);
void ControlChannel(int _number, u16 _channelID, const void* _pData, u32 _Size) ;
void Update(int _number);
void DoState(PointerWrap &p);
void ReadLinuxKeyboard();
bool IsKey(int Key);

// Recordings
void LoadRecordedMovements();
void GetMousePos(float& x, float& y);

// Gamepad
void Close_Devices();
bool Search_Devices(std::vector<InputCommon::CONTROLLER_INFO> &_joyinfo, int &_NumPads, int &_NumGoodPads);
void GetAxisState(CONTROLLER_MAPPING_WII &_WiiMapping);
void UpdatePadState(CONTROLLER_MAPPING_WII &_WiiMapping);

void TiltWiimote(int &_x, int &_y, int &_z);
void TiltNunchuck(int &_x, int &_y, int &_z);
void ShakeToAccelerometer(int  &_x, int &_y, int &_z, STiltData &_TiltData);
void TiltToAccelerometer(int &_x, int &_y, int &_z, STiltData &_TiltData);
void AdjustAngles(int &Roll, int &Pitch);
void RotateIRDot(int &_x, int &_y, STiltData &_TiltData);

#if defined(HAVE_X11) && HAVE_X11
struct MousePosition
{
	int x, y;
	int WinWidth, WinHeight;
};
extern MousePosition MousePos;
#endif

// Accelerometer
//void PitchAccelerometerToDegree(u8 _x, u8 _y, u8 _z, int &_Roll, int &_Pitch, int&, int&);
//float AccelerometerToG(float Current, float Neutral, float G);

// IR data
//void IRData2Dots(u8 *Data);
//void IRData2DotsBasic(u8 *Data);
//void ReorderIRDots();
//void IRData2Distance();

// Classic Controller data
//std::string CCData2Values(u8 *Data);

}; // WiiMoteEmu


#endif
