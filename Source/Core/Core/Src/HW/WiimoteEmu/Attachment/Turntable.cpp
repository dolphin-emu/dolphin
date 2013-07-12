// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Turntable.h"


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
	groups.push_back(m_buttons = new Buttons("Buttons"));
	for (unsigned int i = 0; i < sizeof(turntable_button_names)/sizeof(*turntable_button_names); ++i)
		m_buttons->controls.push_back(new ControlGroup::Input(turntable_button_names[i]));

	// turntables
	groups.push_back(m_left_table = new Slider(_trans("Table Left")));
	groups.push_back(m_right_table = new Slider(_trans("Table Right")));

	// stick
	groups.push_back(m_stick = new AnalogStick("Stick"));

	// effect dial
	groups.push_back(m_effect_dial = new Triggers(_trans("Effect")));
	m_effect_dial->controls.push_back(new ControlGroup::Input(_trans("Dial")));

	// crossfade
	groups.push_back(m_crossfade = new Slider(_trans("Crossfade")));

	// set up register
	// id
	memcpy(&id, turntable_id, sizeof(turntable_id));
}

void Turntable::GetState(u8* const data, const bool focus)
{
	wm_turntable_extension* const ttdata = (wm_turntable_extension*)data;
	ttdata->bt = 0;

	// stick
	{
	u8 x, y;
	m_stick->GetState(&x, &y, 0x20, focus ? 0x1F /*0x15*/ : 0);

	ttdata->sx = x;
	ttdata->sy = y;
	}

	// left table
	{
	s8 tt = 0;
	m_left_table->GetState(&tt, focus ? 0x1F : 0);

	ttdata->ltable1 = tt;
	ttdata->ltable2 = tt << 5;
	}

	// right table
	{
	s8 tt = 0;
	m_right_table->GetState(&tt, focus ? 0x1F : 0);

	ttdata->rtable1 = tt;
	ttdata->rtable2 = tt << 1;
	ttdata->rtable3 = tt << 3;
	ttdata->rtable4 = tt << 5;
	}

	// effect dial
	{
	u8 dial = 0;
	m_effect_dial->GetState(&dial, focus ? 0xF : 0);

	ttdata->dial1 = dial;
	ttdata->dial2 = dial << 3;
	}

	// crossfade slider
	{
	s8 cfs = 0;
	m_crossfade->GetState(&cfs, focus ? 7 : 0);
	cfs += 8;

	ttdata->slider = cfs;
	}

	if (focus)
	{
		// buttons
		m_buttons->GetState(&ttdata->bt, turntable_button_bitmasks);
	}

	// flip button bits :/
	ttdata->bt ^= (
		BUTTON_L_GREEN | BUTTON_L_RED | BUTTON_L_BLUE |
		BUTTON_R_GREEN | BUTTON_R_RED | BUTTON_R_BLUE |
		BUTTON_MINUS | BUTTON_PLUS | BUTTON_EUPHORIA
		);
}


}
