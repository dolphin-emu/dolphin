// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Attachment.h"

namespace WiimoteEmu
{

class Classic : public Attachment
{
public:
	Classic(WiimoteEmu::ExtensionReg& _reg);
	void GetState( u8* const data, const bool focus );

	enum
	{
		PAD_RIGHT = 0x80,
		PAD_DOWN = 0x40,
		TRIGGER_L = 0x20,
		BUTTON_MINUS = 0x10,
		BUTTON_HOME = 0x08,
		BUTTON_PLUS = 0x04,
		TRIGGER_R = 0x02,
		NOTHING = 0x01,
		BUTTON_ZL = 0x8000,
		BUTTON_B = 0x4000,
		BUTTON_Y = 0x2000,
		BUTTON_A = 0x1000,
		BUTTON_X = 0x0800,
		BUTTON_ZR = 0x0400,
		PAD_LEFT = 0x0200,
		PAD_UP = 0x0100,
	};

private:
	Buttons*		m_buttons;
	MixedTriggers*	m_triggers;
	Buttons*		m_dpad;
	AnalogStick*	m_left_stick;
	AnalogStick*	m_right_stick;
};


}
