// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Drums.h"


namespace WiimoteEmu
{

static const u8 drums_id[] = { 0x01, 0x00, 0xa4, 0x20, 0x01, 0x03 };

static const u16 drum_pad_bitmasks[] =
{
	Drums::PAD_RED,
	Drums::PAD_YELLOW,
	Drums::PAD_BLUE,
	Drums::PAD_GREEN,
	Drums::PAD_ORANGE,
	Drums::PAD_BASS,
};

static const char* const drum_pad_names[] =
{
	_trans("Red"), _trans("Yellow"), _trans("Blue"),
	_trans("Green"), _trans("Orange"), _trans("Bass")
};

static const u16 drum_button_bitmasks[] =
{
	Drums::BUTTON_MINUS,
	Drums::BUTTON_PLUS,
};

Drums::Drums(WiimoteEmu::ExtensionReg& _reg) : Attachment(_trans("Drums"), _reg)
{
	// pads
	groups.push_back(m_pads = new Buttons(_trans("Pads")));
	for (unsigned int i = 0; i < sizeof(drum_pad_names)/sizeof(*drum_pad_names); ++i)
		m_pads->controls.push_back(new ControlGroup::Input(drum_pad_names[i]));

	// stick
	groups.push_back(m_stick = new AnalogStick("Stick"));

	// buttons
	groups.push_back(m_buttons = new Buttons("Buttons"));
	m_buttons->controls.push_back(new ControlGroup::Input("-"));
	m_buttons->controls.push_back(new ControlGroup::Input("+"));

	// set up register
	// id
	memcpy(&id, drums_id, sizeof(drums_id));
}

void Drums::GetState(u8* const data, const bool focus)
{
	wm_drums_extension* const ddata = (wm_drums_extension*)data;
	ddata->bt = 0;

	// calibration data not figured out yet?

	// stick
	{
	u8 x, y;
	m_stick->GetState(&x, &y, 0x20, focus ? 0x1F /*0x15*/ : 0);

	ddata->sx = x;
	ddata->sy = y;
	}

	// TODO: softness maybe
	data[2] = 0xFF;
	data[3] = 0xFF;

	if (focus)
	{
		// buttons
		m_buttons->GetState(&ddata->bt, drum_button_bitmasks);
		// pads
		m_pads->GetState(&ddata->bt, drum_pad_bitmasks);
	}

	// flip button bits
	ddata->bt ^= 0xFFFF;
}

}
