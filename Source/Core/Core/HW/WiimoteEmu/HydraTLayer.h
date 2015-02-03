// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// Razer Hydra Wiimote Translation Layer

#pragma once

#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteEmu/Attachment/Classic.h"

namespace HydraTLayer
{

void GetButtons(int index, bool sideways, bool has_extension, wm_buttons * butt, bool * cycle_extension);
void GetAcceleration(int index, bool sideways, bool has_extension, WiimoteEmu::AccelData * const data);
void GetIR(int index, double * x, double * y, double * z);
void GetNunchukAcceleration(int index, WiimoteEmu::AccelData * const data);
void GetNunchuk(int index, u8 *jx, u8 *jy, wm_nc_core *butt);
void GetClassic(int index, wm_classic_extension *ccdata);
void GetGameCube(int index, u16 *butt, u8 *stick_x, u8 *stick_y, u8 *substick_x, u8 *substick_y, u8 *ltrigger, u8 *rtrigger);

extern float vr_gc_dpad_speed, vr_gc_leftstick_speed, vr_gc_rightstick_speed;
extern float vr_cc_dpad_speed, vr_wm_dpad_speed, vr_wm_leftstick_speed, vr_wm_rightstick_speed;

}
