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
	"Green Left", "Red Left", "Blue Left",
	"Green Right", "Red Right", "Blue Right",
	"-", "+", "Euphoria",
};

Turntable::Turntable() : Attachment("Turntable")
{
	// buttons
	// TODO: separate buttons into Left and Right
	groups.push_back(m_buttons = new Buttons("Buttons"));
	for (unsigned int i = 0; i < sizeof(turntable_button_names)/sizeof(*turntable_button_names); ++i)
		m_buttons->controls.push_back(new ControlGroup::Input( turntable_button_names[i]));

	// stick
	groups.push_back(m_stick = new AnalogStick("Stick"));

	// TODO: 
	groups.push_back(m_effect_dial = new Triggers("Effect"));
	m_effect_dial->controls.push_back(new ControlGroup::Input("Dial"));

	//m_left_turntable
	//m_right_turntable
	//m_crossfade_slider

	// set up register
	// id
	memcpy(&reg[0xfa], turntable_id, sizeof(turntable_id));
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
	// TODO:
	{
	s8 tt = 0;
	//m_left_turntable->GetState(&tt .....);

	ttdata->ltable1 = tt;
	ttdata->ltable2 = tt << 5;
	}

	// right table
	// TODO:
	{
	s8 tt = 0;
	//m_right_turntable->GetState(&tt .....);

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
	// TODO:
	{
	u8 cfs = 0;
	//m_crossfade_slider->GetState(&cfs .....);

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
