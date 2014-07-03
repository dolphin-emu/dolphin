// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

//UDP Wiimote Translation Layer

#pragma once

#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "InputCommon/UDPWiimote.h"

namespace UDPTLayer
{
	void GetButtons(UDPWrapper * m , wm_core * butt)
	{
		if (!(m->inst)) return;
		if (!(m->updButt)) return;
		u32 mask = m->inst->getButtons();
		*butt |= (mask & UDPWM_BA) ? WiimoteEmu::Wiimote::BUTTON_A : 0;
		*butt |= (mask & UDPWM_BB) ? WiimoteEmu::Wiimote::BUTTON_B : 0;
		*butt |= (mask & UDPWM_B1) ? WiimoteEmu::Wiimote::BUTTON_ONE : 0;
		*butt |= (mask & UDPWM_B2) ? WiimoteEmu::Wiimote::BUTTON_TWO : 0;
		*butt |= (mask & UDPWM_BP) ? WiimoteEmu::Wiimote::BUTTON_PLUS : 0;
		*butt |= (mask & UDPWM_BM) ? WiimoteEmu::Wiimote::BUTTON_MINUS : 0;
		*butt |= (mask & UDPWM_BH) ? WiimoteEmu::Wiimote::BUTTON_HOME : 0;
		*butt |= (mask & UDPWM_BU) ? WiimoteEmu::Wiimote::PAD_UP : 0;
		*butt |= (mask & UDPWM_BD) ? WiimoteEmu::Wiimote::PAD_DOWN : 0;
		*butt |= (mask & UDPWM_BL) ? WiimoteEmu::Wiimote::PAD_LEFT : 0;
		*butt |= (mask & UDPWM_BR) ? WiimoteEmu::Wiimote::PAD_RIGHT : 0;
	}

	void GetAcceleration(UDPWrapper * m , WiimoteEmu::AccelData * const data)
	{
		if (!(m->inst)) return;
		if (!(m->updAccel)) return;
		float x, y, z;
		m->inst->getAccel(&x, &y, &z);
		data->x = x;
		data->y = y;
		data->z = z;
	}

	void GetIR( UDPWrapper * m, float * x,  float * y,  float * z)
	{
		if (!(m->inst)) return;
		if (!(m->updIR)) return;
		if ((*x >= -0.999) && (*x <= 0.999) && (*y >= -0.999) && (*y <= 0.999)) return; //the received values are used ONLY when the normal pointer is offscreen
		float _x, _y;
		m->inst->getIR(&_x, &_y);
		*x = _x * 2 - 1;
		*y = -(_y * 2 - 1);
		*z = 0;
	}
}
