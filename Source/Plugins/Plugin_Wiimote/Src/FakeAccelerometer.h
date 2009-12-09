// Copyright (C) 2009 Dolphin Project.

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

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!                                                                        !!
// !!                          THIS CODE IS UNUSED                           !!
// !!                                                                        !!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

#ifndef FAKEACCELEROMETER_H
#define FAKEACCELEROMETER_H

#include <vector>
#include <string>

#include "Common.h" // Common
#include "Timer.h"
#include "pluginspecs_wiimote.h"
#include "StringUtil.h" // For ArrayToString

#include "wiimote_hid.h"
#include "main.h"
#include "EmuMain.h"
#include "EmuSubroutines.h"
#include "EmuDefinitions.h"
#include "Config.h" // For g_Config

namespace WiiMoteEmu {

extern int IsKey(int Key);
extern bool RecordingCheckKeys(int WmNuIr);
extern bool RecordingPlay(u8 &_x, u8 &_y, u8 &_z, int Wm);

class FakeAccelerometer : ShakeData {
	private:
	int x, y, z;
	public:
	void StartShake();
	void StartShake(ShakeData &shakeData);
	void SingleShake();
	void SingleShake(int &_x, int &_y, int &_z, ShakeData &shakeData);
	void TiltWiimoteGamepad();
	void TiltWiimoteGamepad(int &Roll, int &Pitch);
	void TiltWiimoteKeyboard();
	void TiltWiimoteKeyboard(int &Roll, int &Pitch);
	void Tilt();
	void Tilt(int &_x, int &_y, int &_z);
	void FillReportAcc(wm_accel& _acc);
};

} // namespace
#endif
