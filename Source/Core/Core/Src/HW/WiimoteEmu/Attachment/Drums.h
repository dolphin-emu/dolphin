// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Attachment.h"

namespace WiimoteEmu
{

class Drums : public Attachment
{
public:
	Drums(WiimoteEmu::ExtensionReg& _reg);
	void GetState( u8* const data, const bool focus );

	enum
	{
		BUTTON_PLUS = 0x04,
		BUTTON_MINUS = 0x10,

		PAD_BASS = 0x0400,
		PAD_BLUE = 0x0800,
		PAD_GREEN = 0x1000,
		PAD_YELLOW = 0x2000,
		PAD_RED = 0x4000,
		PAD_ORANGE = 0x8000,
	};

private:
	Buttons*		m_buttons;
	Buttons*		m_pads;
	AnalogStick*	m_stick;
};


}
