// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteEmu/Attachment/Turntable.h"

namespace WiimoteEmu
{

static const u8 turntable_id[] = {0x03, 0x00, 0xa4, 0x20, 0x01, 0x03};

static const u16 turntable_button_bitmasks[] =
{
	Turntable::BUTTON_L_GREEN,
	Turntable::BUTTON_L_RED,
	Turntable::BUTTON_L_BLUE,
	Turntable::BUTTON_R_GREEN,
	Turntable::BUTTON_R_RED,
	Turntable::BUTTON_R_BLUE,
	Turntable::BUTTON_MINUS,
	Turntable::BUTTON_PLUS,
	Turntable::BUTTON_EUPHORIA,
};

static const char* const turntable_button_names[] =
{
	_trans("Green Left"), _trans("Red Left"), _trans("Blue Left"),
	_trans("Green Right"), _trans("Red Right"), _trans("Blue Right"),
	"-", "+", _trans("Euphoria"),
};

Turntable::Turntable(WiimoteEmu::ExtensionReg& _reg) : Attachment(_trans("Turntable"), _reg)
{
	// buttons
	groups.emplace_back(m_buttons = new Buttons("Buttons"));
	for (auto& turntable_button_name : turntable_button_names)
		m_buttons->controls.emplace_back(new ControlGroup::Input(turntable_button_name));

	// turntables
	groups.emplace_back(m_left_table = new Slider(_trans("Table Left")));
	groups.emplace_back(m_right_table = new Slider(_trans("Table Right")));

	// stick
	groups.emplace_back(m_stick = new AnalogStick("Stick", DEFAULT_ATTACHMENT_STICK_RADIUS));

	// effect dial
	groups.emplace_back(m_effect_dial = new Triggers(_trans("Effect")));
	m_effect_dial->controls.emplace_back(new ControlGroup::Input(_trans("Dial")));

	// crossfade
	groups.emplace_back(m_crossfade = new Slider(_trans("Crossfade")));

	// set up register
	// id
	memcpy(&id, turntable_id, sizeof(turntable_id));
}

void Turntable::GetState(u8* const data)
{
	wm_turntable_extension* const ttdata = (wm_turntable_extension*)data;
	ttdata->bt = 0;

	// stick
	{
	ControlState x, y;
	m_stick->GetState(&x, &y);

	ttdata->sx = static_cast<u8>((x * 0x1F) + 0x20);
	ttdata->sy = static_cast<u8>((y * 0x1F) + 0x20);
	}

	// left table
	{
	ControlState tt;
	s8 tt_;
	m_left_table->GetState(&tt);

	tt_ = static_cast<s8>(tt * 0x1F);

	ttdata->ltable1 = tt_;
	ttdata->ltable2 = tt_ >> 5;
	}

	// right table
	{
	ControlState tt;
	s8 tt_;
	m_right_table->GetState(&tt);

	tt_ = static_cast<s8>(tt * 0x1F);

	ttdata->rtable1 = tt_;
	ttdata->rtable2 = tt_ >> 1;
	ttdata->rtable3 = tt_ >> 3;
	ttdata->rtable4 = tt_ >> 5;
	}

	// effect dial
	{
	ControlState dial;
	u8 dial_;
	m_effect_dial->GetState(&dial);

	dial_ = static_cast<u8>(dial * 0x0F);

	ttdata->dial1 = dial_;
	ttdata->dial2 = dial_ >> 3;
	}

	// crossfade slider
	{
	ControlState cfs;
	m_crossfade->GetState(&cfs);

	ttdata->slider = static_cast<u8>((cfs * 0x07) + 0x08);
	}

	// buttons
	m_buttons->GetState(&ttdata->bt, turntable_button_bitmasks);

	// flip button bits :/
	ttdata->bt ^= (
		BUTTON_L_GREEN | BUTTON_L_RED | BUTTON_L_BLUE |
		BUTTON_R_GREEN | BUTTON_R_RED | BUTTON_R_BLUE |
		BUTTON_MINUS | BUTTON_PLUS | BUTTON_EUPHORIA
		);
}


}
