// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/WiimoteEmu/Attachment/Attachment.h"

namespace WiimoteEmu
{

class Nunchuk : public Attachment
{
public:
	Nunchuk(WiimoteEmu::ExtensionReg& _reg);

	virtual void GetState(u8* const data) override;

	enum
	{
		BUTTON_C = 0x02,
		BUTTON_Z = 0x01,
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
