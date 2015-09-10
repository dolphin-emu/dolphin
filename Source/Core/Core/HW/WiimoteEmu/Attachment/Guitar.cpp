// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteEmu/Attachment/Guitar.h"

namespace WiimoteEmu
{

static const u8 guitar_id[] = { 0x00, 0x00, 0xa4, 0x20, 0x01, 0x03 };

static const u16 guitar_fret_bitmasks[] =
{
	Guitar::FRET_GREEN,
	Guitar::FRET_RED,
	Guitar::FRET_YELLOW,
	Guitar::FRET_BLUE,
	Guitar::FRET_ORANGE,
};

static const char* const guitar_fret_names[] =
{
	"Green","Red","Yellow","Blue","Orange",
};

static const u16 guitar_button_bitmasks[] =
{
	Guitar::BUTTON_MINUS,
	Guitar::BUTTON_PLUS,
};

static const u16 guitar_strum_bitmasks[] =
{
	Guitar::BAR_UP,
	Guitar::BAR_DOWN,
};

Guitar::Guitar(WiimoteEmu::ExtensionReg& _reg) : Attachment(_trans("Guitar"), _reg)
{
	// frets
	groups.emplace_back(m_frets = new Buttons(_trans("Frets")));
	for (auto& guitar_fret_name : guitar_fret_names)
		m_frets->controls.emplace_back(new ControlGroup::Input(guitar_fret_name));

	// strum
	groups.emplace_back(m_strum = new Buttons(_trans("Strum")));
	m_strum->controls.emplace_back(new ControlGroup::Input("Up"));
	m_strum->controls.emplace_back(new ControlGroup::Input("Down"));

	// buttons
	groups.emplace_back(m_buttons = new Buttons("Buttons"));
	m_buttons->controls.emplace_back(new ControlGroup::Input("-"));
	m_buttons->controls.emplace_back(new ControlGroup::Input("+"));

	// stick
	groups.emplace_back(m_stick = new AnalogStick(_trans("Stick"), DEFAULT_ATTACHMENT_STICK_RADIUS));

	// whammy
	groups.emplace_back(m_whammy = new Triggers(_trans("Whammy")));
	m_whammy->controls.emplace_back(new ControlGroup::Input(_trans("Bar")));

	// set up register
	// id
	memcpy(&id, guitar_id, sizeof(guitar_id));
}

void Guitar::GetState(u8* const data)
{
	wm_guitar_extension* const gdata = (wm_guitar_extension*)data;
	gdata->bt = 0;

	// calibration data not figured out yet?

	// stick
	{
	ControlState x, y;
	m_stick->GetState(&x, &y);

	gdata->sx = static_cast<u8>((x * 0x1F) + 0x20);
	gdata->sy = static_cast<u8>((y * 0x1F) + 0x20);
	}

	// TODO: touch bar, probably not
	gdata->tb = 0x0F; // not touched

	// whammy bar
	ControlState whammy;
	m_whammy->GetState(&whammy);
	gdata->whammy = static_cast<u8>(whammy * 0x1F);

	// buttons
	m_buttons->GetState(&gdata->bt, guitar_button_bitmasks);
	// frets
	m_frets->GetState(&gdata->bt, guitar_fret_bitmasks);
	// strum
	m_strum->GetState(&gdata->bt, guitar_strum_bitmasks);

	// flip button bits
	gdata->bt ^= 0xFFFF;
}

bool Guitar::IsButtonPressed() const
{
	u16 buttons = 0;
	m_buttons->GetState(&buttons, guitar_button_bitmasks);
	m_frets->GetState(&buttons, guitar_fret_bitmasks);
	m_strum->GetState(&buttons, guitar_strum_bitmasks);
	return buttons != 0;
}

}
