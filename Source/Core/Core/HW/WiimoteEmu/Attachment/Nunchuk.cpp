// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/Attachment/Nunchuk.h"

namespace WiimoteEmu
{

static const u8 nunchuk_id[] = { 0x00, 0x00, 0xa4, 0x20, 0x00, 0x00 };
/* Default calibration for the nunchuk. It should be written to 0x20 - 0x3f of the
   extension register. 0x80 is the neutral x and y accelerators and 0xb3 is the
   neutral z accelerometer that is adjusted for gravity. */
static const u8 nunchuk_calibration[] =
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

Nunchuk::Nunchuk(WiimoteEmu::ExtensionReg& _reg) : Attachment(_trans("Nunchuk"), _reg)
{
	// buttons
	groups.emplace_back(m_buttons = new Buttons("Buttons"));
	m_buttons->controls.emplace_back(new ControlGroup::Input("C"));
	m_buttons->controls.emplace_back(new ControlGroup::Input("Z"));

	// stick
	groups.emplace_back(m_stick = new AnalogStick("Stick", DEFAULT_ATTACHMENT_STICK_RADIUS));

	// swing
	groups.emplace_back(m_swing = new Force("Swing"));

	// tilt
	groups.emplace_back(m_tilt = new Tilt("Tilt"));

	// shake
	groups.emplace_back(m_shake = new Buttons("Shake"));
	m_shake->controls.emplace_back(new ControlGroup::Input("X"));
	m_shake->controls.emplace_back(new ControlGroup::Input("Y"));
	m_shake->controls.emplace_back(new ControlGroup::Input("Z"));

	// set up register
	// calibration
	memcpy(&calibration, nunchuk_calibration, sizeof(nunchuk_calibration));
	// id
	memcpy(&id, nunchuk_id, sizeof(nunchuk_id));

	// this should get set to 0 on disconnect, but it isn't, o well
	memset(m_shake_step, 0, sizeof(m_shake_step));
}

void Nunchuk::GetState(u8* const data)
{
	wm_nc* const ncdata = (wm_nc*)data;
	ncdata->bt.hex = 0;

	// stick
	double state[2];
	m_stick->GetState(&state[0], &state[1]);

	nu_cal &cal = *(nu_cal*)&reg.calibration;
	nu_js cal_js[2];
	cal_js[0] = cal.jx;
	cal_js[1] = cal.jy;

	for (int i = 0; i < 2; i++)
	{
		double &s = state[i];
		nu_js c = cal_js[i];
		if (s < 0)
			s = s * abs(c.min - c.center) + c.center;
		else if (s > 0)
			s = s * abs(c.max - c.center) + c.center;
		else
			s = c.center;
	}

	ncdata->jx = u8(trim(state[0]));
	ncdata->jy = u8(trim(state[1]));

	if (ncdata->jx != cal.jx.center || ncdata->jy != cal.jy.center)
	{
		if (ncdata->jy == cal.jy.center)
			ncdata->jy = cal.jy.center + 1;
		if (ncdata->jx == cal.jx.center)
			ncdata->jx = cal.jx.center + 1;
	}

	AccelData accel;

	// tilt
	EmulateTilt(&accel, m_tilt);

	// swing
	EmulateSwing(&accel, m_swing);
	// shake
	EmulateShake(&accel, m_shake, m_shake_step);
	// buttons
	m_buttons->GetState(&ncdata->bt.hex, nunchuk_button_bitmasks);

	// flip the button bits :/
	*(u8*)&ncdata->bt ^= 0x03;

	accel_cal& calib = *(accel_cal*)&reg.calibration;

	u16 x = (u16)(accel.x * (calib.one_g.x - calib.zero_g.x) + calib.zero_g.x);
	u16 y = (u16)(accel.y * (calib.one_g.y - calib.zero_g.y) + calib.zero_g.y);
	u16 z = (u16)(accel.z * (calib.one_g.z - calib.zero_g.z) + calib.zero_g.z);

	if (x > 1024)
		x = 1024;
	if (y > 1024)
		y = 1024;
	if (z > 1024)
		z = 1024;

	ncdata->ax = x & 0xFF;
	ncdata->ay = y & 0xFF;
	ncdata->az = z & 0xFF;
	ncdata->passthrough_data.acc_x_lsb = x >> 8 & 0x3;
	ncdata->passthrough_data.acc_y_lsb = y >> 8 & 0x3;
	ncdata->passthrough_data.acc_z_lsb = z >> 8 & 0x3;
}

void Nunchuk::LoadDefaults(const ControllerInterface& ciface)
{
	// ugly macroooo
	#define set_control(group, num, str)  (group)->controls[num]->control_ref->expression = (str)

	// Stick
	set_control(m_stick, 0, "W"); // up
	set_control(m_stick, 1, "S"); // down
	set_control(m_stick, 2, "A"); // left
	set_control(m_stick, 3, "D"); // right

	// Buttons
#ifdef _WIN32
	set_control(m_buttons, 0, "LCONTROL");  // C
	set_control(m_buttons, 1, "LSHIFT");    // Z
#elif __APPLE__
	set_control(m_buttons, 0, "Left Control"); // C
	set_control(m_buttons, 1, "Left Shift");   // Z
#else
	set_control(m_buttons, 0, "Control_L"); // C
	set_control(m_buttons, 1, "Shift_L");   // Z
#endif
}

}
