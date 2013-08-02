// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Attachment.h"

namespace WiimoteEmu
{

class Turntable : public Attachment
{
public:
	Turntable(WiimoteEmu::ExtensionReg& _reg);
	void GetState(u8* const data, const bool focus);

	enum
	{
		BUTTON_EUPHORIA = 0x1000,

		BUTTON_L_GREEN = 0x0800,
		BUTTON_L_RED = 0x20,
		BUTTON_L_BLUE = 0x8000,

		BUTTON_R_GREEN = 0x2000,
		BUTTON_R_RED = 0x02,
		BUTTON_R_BLUE = 0x0400,

		BUTTON_MINUS = 0x10,
		BUTTON_PLUS = 0x04,
	};

private:
	Buttons*		m_buttons;
	MixedTriggers*	m_triggers;
	AnalogStick*	m_stick;
	Triggers	*m_effect_dial;
	Slider		*m_left_table, *m_right_table, *m_crossfade;
};


}
