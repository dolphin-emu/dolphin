// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteEmu/Attachment/Drums.h"

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
	groups.emplace_back(m_pads = new Buttons(_trans("Pads")));
	for (auto& drum_pad_name : drum_pad_names)
		m_pads->controls.emplace_back(new ControlGroup::Input(drum_pad_name));

	// stick
	groups.emplace_back(m_stick = new AnalogStick("Stick", DEFAULT_ATTACHMENT_STICK_RADIUS));

	// buttons
	groups.emplace_back(m_buttons = new Buttons("Buttons"));
	m_buttons->controls.emplace_back(new ControlGroup::Input("-"));
	m_buttons->controls.emplace_back(new ControlGroup::Input("+"));

	// set up register
	// id
	memcpy(&id, drums_id, sizeof(drums_id));
}

void Drums::GetState(u8* const data)
{
	wm_drums_extension* const ddata = (wm_drums_extension*)data;
	ddata->bt = 0;

	// calibration data not figured out yet?

	// stick
	{
	ControlState x, y;
	m_stick->GetState(&x, &y);

	ddata->sx = static_cast<u8>((x * 0x1F) + 0x20);
	ddata->sy = static_cast<u8>((y * 0x1F) + 0x20);
	}

	// TODO: softness maybe
	data[2] = 0xFF;
	data[3] = 0xFF;

	// buttons
	m_buttons->GetState(&ddata->bt, drum_button_bitmasks);
	// pads
	m_pads->GetState(&ddata->bt, drum_pad_bitmasks);

	// flip button bits
	ddata->bt ^= 0xFFFF;
}

bool Drums::IsButtonPressed() const
{
	u16 buttons = 0;
	m_buttons->GetState(&buttons, drum_button_bitmasks);
	m_pads->GetState(&buttons, drum_pad_bitmasks);
	return buttons != 0;
}

}
