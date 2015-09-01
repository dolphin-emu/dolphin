// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/WiimoteEmu/Attachment/Attachment.h"

namespace WiimoteEmu
{

struct ExtensionReg;

class Nunchuk : public Attachment
{
public:
	Nunchuk(WiimoteEmu::ExtensionReg& _reg);

	void GetState(u8* const data) override;
	bool IsButtonPressed() const override;

	enum
	{
		BUTTON_C = 0x02,
		BUTTON_Z = 0x01,
	};

	enum
	{
		ACCEL_ZERO_G = 0x80,
		ACCEL_ONE_G = 0xB3,
		ACCEL_RANGE = (ACCEL_ONE_G - ACCEL_ZERO_G),
	};

	enum
	{
		STICK_CENTER = 0x80,
		STICK_RADIUS = 0x7F,
	};

	void LoadDefaults(const ControllerInterface& ciface) override;

private:
	Tilt*        m_tilt;
	Force*       m_swing;

	Buttons*     m_shake;

	Buttons*     m_buttons;
	AnalogStick* m_stick;

	u8 m_shake_step[3];
};

}
