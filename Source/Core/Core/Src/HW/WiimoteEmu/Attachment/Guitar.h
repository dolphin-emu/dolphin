// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Attachment.h"

namespace WiimoteEmu
{

class Guitar : public Attachment
{
public:
	Guitar(WiimoteEmu::ExtensionReg& _reg);
	void GetState( u8* const data, const bool focus );

	enum
	{
		BUTTON_PLUS = 0x04,
		BUTTON_MINUS = 0x10,
		BAR_DOWN = 0x40,

		BAR_UP = 0x0100,
		FRET_YELLOW = 0x0800,
		FRET_GREEN = 0x1000,
		FRET_BLUE = 0x2000,
		FRET_RED = 0x4000,
		FRET_ORANGE = 0x8000,
	};

private:
	Buttons*		m_buttons;
	Buttons*		m_frets;
	Buttons*		m_strum;
	Triggers*		m_whammy;
	AnalogStick*	m_stick;
};


}
