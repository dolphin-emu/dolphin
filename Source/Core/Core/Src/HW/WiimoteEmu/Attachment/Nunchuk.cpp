// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Nunchuk.h"

#include "UDPWrapper.h"
#include "UDPWiimote.h"

namespace WiimoteEmu
{

static const u8 nunchuck_id[] = { 0x00, 0x00, 0xa4, 0x20, 0x00, 0x00 };
/* Default calibration for the nunchuck. It should be written to 0x20 - 0x3f of the
   extension register. 0x80 is the neutral x and y accelerators and 0xb3 is the
   neutral z accelerometer that is adjusted for gravity. */
static const u8 nunchuck_calibration[] =
{
	0x80, 0x80, 0x80, 0x00, // accelerometer x, y, z neutral
	0xb3, 0xb3, 0xb3, 0x00, //  x, y, z g-force values 

	// 0x80 = analog stick x and y axis center
	0xff, 0x00, 0x80,
	0xff, 0x00, 0x80,
	0xec, 0x41 // checksum on the last two bytes
};

static const u8 nunchuk_button_bitmasks[] =
{
	Nunchuk::BUTTON_C,
	Nunchuk::BUTTON_Z,
};

Nunchuk::Nunchuk(UDPWrapper *wrp, WiimoteEmu::ExtensionReg& _reg)
	: Attachment(_trans("Nunchuk"), _reg) , m_udpWrap(wrp)
{
	// buttons
	groups.push_back(m_buttons = new Buttons("Buttons"));
	m_buttons->controls.push_back(new ControlGroup::Input("C"));
	m_buttons->controls.push_back(new ControlGroup::Input("Z"));

	// stick
	groups.push_back(m_stick = new AnalogStick("Stick"));

	// swing
	groups.push_back(m_swing = new Force("Swing"));

	// tilt
	groups.push_back(m_tilt = new Tilt("Tilt"));

	// shake
	groups.push_back(m_shake = new Buttons("Shake"));
	m_shake->controls.push_back(new ControlGroup::Input("X"));
	m_shake->controls.push_back(new ControlGroup::Input("Y"));
	m_shake->controls.push_back(new ControlGroup::Input("Z"));

	// set up register
	// calibration
	memcpy(&calibration, nunchuck_calibration, sizeof(nunchuck_calibration));
	// id
	memcpy(&id, nunchuck_id, sizeof(nunchuck_id));

	// this should get set to 0 on disconnect, but it isn't, o well
	memset(m_shake_step, 0, sizeof(m_shake_step));
}

void Nunchuk::GetState(u8* const data, const bool focus)
{
	wm_extension* const ncdata = (wm_extension*)data;
	ncdata->bt = 0;

	// stick
	ControlState state[2];
	m_stick->GetState(&state[0], &state[1], 0, 1);

	nu_cal &cal = *(nu_cal*)&reg.calibration;
	nu_js cal_js[2];
	cal_js[0] = *&cal.jx;
	cal_js[1] = *&cal.jy;

	for (int i = 0; i < 2; i++) {
		ControlState &s = *&state[i];
		nu_js c = *&cal_js[i];
		if (s < 0)
			s = s * abs(c.min - c.center) + c.center;
		else if (s > 0)
			s = s * abs(c.max - c.center) + c.center;
		else
			s = c.center;
	}

	ncdata->jx = u8(trim(state[0]));
	ncdata->jy = u8(trim(state[1]));

	if (!focus)
	{
		ncdata->jx = cal.jx.center;
		ncdata->jy = cal.jy.center;
	}

	AccelData accel;
	accel_cal* calib = (accel_cal*)&reg.calibration[0];

	// tilt
	EmulateTilt(&accel, m_tilt, focus);

	if (focus)
	{
		// swing
		EmulateSwing(&accel, m_swing);
		// shake
		EmulateShake(&accel, calib, m_shake, m_shake_step);
		// buttons
		m_buttons->GetState(&ncdata->bt, nunchuk_button_bitmasks);
	}
	
	// flip the button bits :/
	ncdata->bt ^= 0x03;
	
	if (m_udpWrap->inst)
	{
		if (m_udpWrap->updNun)
		{
			u8 mask;
			float x, y;
			m_udpWrap->inst->getNunchuck(x, y, mask);
			// buttons
			if (mask & UDPWM_NC)
				ncdata->bt &= ~WiimoteEmu::Nunchuk::BUTTON_C;
			if (mask & UDPWM_NZ)
				ncdata->bt &= ~WiimoteEmu::Nunchuk::BUTTON_Z;
			// stick
			if (ncdata->jx == 0x80 && ncdata->jy == 0x80)
			{
				ncdata->jx = u8(0x80 + x*127);
				ncdata->jy = u8(0x80 + y*127);
			}
		}
		if (m_udpWrap->updNunAccel)
		{
			float x, y, z;
			m_udpWrap->inst->getNunchuckAccel(x, y, z);
			accel.x = x;
			accel.y = y;
			accel.z = z;
		}	
	}

	wm_accel* dt = (wm_accel*)&ncdata->ax;
	dt->x = u8(trim(accel.x * (calib->one_g.x - calib->zero_g.x) + calib->zero_g.x));
	dt->y = u8(trim(accel.y * (calib->one_g.y - calib->zero_g.y) + calib->zero_g.y));
	dt->z = u8(trim(accel.z * (calib->one_g.z - calib->zero_g.z) + calib->zero_g.z));
}

void Nunchuk::LoadDefaults(const ControllerInterface& ciface)
{
	// ugly macroooo
	#define set_control(group, num, str)	(group)->controls[num]->control_ref->expression = (str)

	// Stick
	set_control(m_stick, 0, "W");	// up
	set_control(m_stick, 1, "S");	// down
	set_control(m_stick, 2, "A");	// left
	set_control(m_stick, 3, "D");	// right

	// Buttons
#ifdef _WIN32
	set_control(m_buttons, 0, "LCONTROL");	// C
	set_control(m_buttons, 1, "LSHIFT");	// Z
#elif __APPLE__
	set_control(m_buttons, 0, "Left Control");	// C
	set_control(m_buttons, 1, "Left Shift");	// Z
#else
	set_control(m_buttons, 0, "Control_L");	// C
	set_control(m_buttons, 1, "Shift_L");	// Z
#endif
}

}
