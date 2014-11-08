// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// Razer Hydra Wiimote Translation Layer

#pragma once

#include "Core/HW/WiimoteEmu/Attachment/Nunchuk.h"
#include "Core/HW/GCPadEmu.h"
#include "Core/HW/WiimoteEmu/HydraTLayer.h"

#include "InputCommon/ControllerInterface/Sixense/RazerHydra.h"

namespace HydraTLayer
{

void GetButtons(int index, bool sideways, bool has_extension, wm_buttons * butt, bool * cycle_extension)
{
	u32 mask;
	if (RazerHydra::getButtons(index, sideways, has_extension, &mask, cycle_extension))
	{
		butt->hex |= (mask & UDPWM_BA) ? WiimoteEmu::Wiimote::BUTTON_A : 0;
		butt->hex |= (mask & UDPWM_BB) ? WiimoteEmu::Wiimote::BUTTON_B : 0;
		butt->hex |= (mask & UDPWM_B1) ? WiimoteEmu::Wiimote::BUTTON_ONE : 0;
		butt->hex |= (mask & UDPWM_B2) ? WiimoteEmu::Wiimote::BUTTON_TWO : 0;
		butt->hex |= (mask & UDPWM_BP) ? WiimoteEmu::Wiimote::BUTTON_PLUS : 0;
		butt->hex |= (mask & UDPWM_BM) ? WiimoteEmu::Wiimote::BUTTON_MINUS : 0;
		butt->hex |= (mask & UDPWM_BH) ? WiimoteEmu::Wiimote::BUTTON_HOME : 0;
		butt->hex |= (mask & UDPWM_BU) ? WiimoteEmu::Wiimote::PAD_UP : 0;
		butt->hex |= (mask & UDPWM_BD) ? WiimoteEmu::Wiimote::PAD_DOWN : 0;
		butt->hex |= (mask & UDPWM_BL) ? WiimoteEmu::Wiimote::PAD_LEFT : 0;
		butt->hex |= (mask & UDPWM_BR) ? WiimoteEmu::Wiimote::PAD_RIGHT : 0;
	}
}

void GetAcceleration(int index, bool sideways, bool has_extension, WiimoteEmu::AccelData * const data)
{
	float x, y, z;
	if (RazerHydra::getAccel(index, sideways, has_extension, &x, &y, &z))
	{
		data->x = x;
		data->y = y;
		data->z = z;
	}
}

void GetIR(int index, double * x,  double * y,  double * z)
{
	RazerHydra::getIR(index, x, y, z);
}

void GetNunchukAcceleration(int index, WiimoteEmu::AccelData * const data)
{
	float gx, gy, gz;
	if (RazerHydra::getNunchuckAccel(index, &gx, &gy, &gz))
	{
		data->x = gx;
		data->y = gy;
		data->z = gz;
	}
}

void GetNunchuk(int index, u8 *jx, u8 *jy, u8 *butt)
{
	float x, y;
	u8 mask;
	if (RazerHydra::getNunchuk(index, &x, &y, &mask))
	{
		*butt |= (mask & UDPWM_NC) ? WiimoteEmu::Nunchuk::BUTTON_C : 0;
		*butt |= (mask & UDPWM_NZ) ? WiimoteEmu::Nunchuk::BUTTON_Z : 0;
		*jx = u8(0x80 + x * 127);
		*jy = u8(0x80 + y * 127);
	}
}

void GetClassic(int index, wm_classic_extension *ccdata)
{
	float lx, ly, rx, ry, l, r;
	u32 mask;
	if (RazerHydra::getClassic(index, &lx, &ly, &rx, &ry, &l, &r, &mask))
	 {
		ccdata->bt.hex |= (mask & CC_B_A)     ? WiimoteEmu::Classic::BUTTON_A : 0;
		ccdata->bt.hex |= (mask & CC_B_B) ? WiimoteEmu::Classic::BUTTON_B : 0;
		ccdata->bt.hex |= (mask & CC_B_X) ? WiimoteEmu::Classic::BUTTON_X : 0;
		ccdata->bt.hex |= (mask & CC_B_Y) ? WiimoteEmu::Classic::BUTTON_Y : 0;
		ccdata->bt.hex |= (mask & CC_B_PLUS) ? WiimoteEmu::Classic::BUTTON_PLUS : 0;
		ccdata->bt.hex |= (mask & CC_B_MINUS) ? WiimoteEmu::Classic::BUTTON_MINUS : 0;
		ccdata->bt.hex |= (mask & CC_B_HOME) ? WiimoteEmu::Classic::BUTTON_HOME : 0;
		ccdata->bt.hex |= (mask & CC_B_L) ? WiimoteEmu::Classic::TRIGGER_L : 0;
		ccdata->bt.hex |= (mask & CC_B_R) ? WiimoteEmu::Classic::TRIGGER_R : 0;
		ccdata->bt.hex |= (mask & CC_B_ZL) ? WiimoteEmu::Classic::BUTTON_ZL : 0;
		ccdata->bt.hex |= (mask & CC_B_ZR) ? WiimoteEmu::Classic::BUTTON_ZR : 0;
		ccdata->bt.hex |= (mask & CC_B_UP) ? WiimoteEmu::Classic::PAD_UP : 0;
		ccdata->bt.hex |= (mask & CC_B_DOWN) ? WiimoteEmu::Classic::PAD_DOWN : 0;
		ccdata->bt.hex |= (mask & CC_B_LEFT) ? WiimoteEmu::Classic::PAD_LEFT : 0;
		ccdata->bt.hex |= (mask & CC_B_RIGHT) ? WiimoteEmu::Classic::PAD_RIGHT : 0;
		ccdata->regular_data.lx = (u8)(31.5f + lx * 31.5f);
		ccdata->regular_data.ly = (u8)(31.5f + ly * 31.5f);
		u8 trigger = (u8)(l * 31);
		ccdata->lt1 = trigger;
		ccdata->lt2 = trigger >> 3;
		u8 x = u8(15.5f + rx * 15.5f);
		u8 y = u8(15.5f + ry * 15.5f);
		ccdata->rx1 = x;
		ccdata->rx2 = x >> 1;
		ccdata->rx3 = x >> 3;
		ccdata->ry = y;
		trigger = (u8)(r * 31);
		ccdata->rt = trigger;
	}
}

void GetGameCube(int index, u16 *butt, u8 *stick_x, u8 *stick_y, u8 *substick_x, u8 *substick_y, u8 *ltrigger, u8 *rtrigger)
{
	float lx, ly, rx, ry, l, r;
	u32 mask;
	if (RazerHydra::getGameCube(index, &lx, &ly, &rx, &ry, &l, &r, &mask))
	{
		*butt |= (mask & GC_B_A)     ? PAD_BUTTON_A : 0;
		*butt |= (mask & GC_B_B)     ? PAD_BUTTON_B : 0;
		*butt |= (mask & GC_B_X)     ? PAD_BUTTON_X : 0;
		*butt |= (mask & GC_B_Y)     ? PAD_BUTTON_Y : 0;
		*butt |= (mask & GC_B_START) ? PAD_BUTTON_START : 0;
		*butt |= (mask & GC_B_L)     ? PAD_TRIGGER_L : 0;
		*butt |= (mask & GC_B_R)     ? PAD_TRIGGER_R : 0;
		*butt |= (mask & GC_B_Z)     ? PAD_TRIGGER_Z : 0;
		*butt |= (mask & GC_B_UP)    ? PAD_BUTTON_UP : 0;
		*butt |= (mask & GC_B_DOWN)  ? PAD_BUTTON_DOWN : 0;
		*butt |= (mask & GC_B_LEFT)  ? PAD_BUTTON_LEFT : 0;
		*butt |= (mask & GC_B_RIGHT) ? PAD_BUTTON_RIGHT : 0;
		*stick_x = (u8)(0x80 + lx * 127);
		*stick_y = (u8)(0x80 + ly * 127);
		*substick_x = (u8)(0x80 + rx * 127);
		*substick_y = (u8)(0x80 + ry * 127);
		*ltrigger = (u8)(l * 255);
		*rtrigger = (u8)(r * 255);
	}
}

}