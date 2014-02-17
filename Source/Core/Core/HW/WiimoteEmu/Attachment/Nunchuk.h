// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/WiimoteEmu/Attachment/Attachment.h"

class UDPWrapper;

namespace WiimoteEmu
{

class Nunchuk : public Attachment
{
public:
	Nunchuk(UDPWrapper * wrp, WiimoteEmu::ExtensionReg& _reg);

	virtual void GetState(u8* const data, const bool focus) override;

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

	UDPWrapper* const m_udpWrap;
};

}
