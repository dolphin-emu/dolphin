// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/Attachment/Classic.h"

#ifdef _WIN32
#include "InputCommon/ControllerInterface/Sixense/SixenseHack.h"
#endif

namespace WiimoteEmu
{

static const u8 classic_id[] = { 0x00, 0x00, 0xa4, 0x20, 0x01, 0x01 };
/* Classic Controller calibration */
static const u8 classic_calibration[] =
{
	0xff, 0x00, 0x80, 0xff, 0x00, 0x80,
	0xff, 0x00, 0x80, 0xff, 0x00, 0x80,
	0x00, 0x00, 0x51, 0xa6
};

static const u16 classic_button_bitmasks[] =
{
	Classic::BUTTON_A,
	Classic::BUTTON_B,
	Classic::BUTTON_X,
	Classic::BUTTON_Y,

	Classic::BUTTON_ZL,
	Classic::BUTTON_ZR,

	Classic::BUTTON_MINUS,
	Classic::BUTTON_PLUS,

	Classic::BUTTON_HOME,
};

static const char* const classic_button_names[] =
{
	"A","B","X","Y","ZL","ZR","-","+","Home",
};

static const u16 classic_trigger_bitmasks[] =
{
	Classic::TRIGGER_L, Classic::TRIGGER_R,
};

static const char* const classic_trigger_names[] =
{
	"L", "R", "L-Analog", "R-Analog"
};

static const u16 classic_dpad_bitmasks[] =
{
	Classic::PAD_UP, Classic::PAD_DOWN, Classic::PAD_LEFT, Classic::PAD_RIGHT
};

Classic::Classic(WiimoteEmu::ExtensionReg& _reg) : Attachment(_trans("Classic"), _reg)
{
	// buttons
	groups.emplace_back(m_buttons = new Buttons("Buttons"));
	for (auto& classic_button_name : classic_button_names)
		m_buttons->controls.emplace_back(new ControlGroup::Input(classic_button_name));

	// sticks
	groups.emplace_back(m_left_stick = new AnalogStick(_trans("Left Stick")));
	groups.emplace_back(m_right_stick = new AnalogStick(_trans("Right Stick")));

	// triggers
	groups.emplace_back(m_triggers = new MixedTriggers("Triggers"));
	for (auto& classic_trigger_name : classic_trigger_names)
		m_triggers->controls.emplace_back(new ControlGroup::Input(classic_trigger_name));

	// dpad
	groups.emplace_back(m_dpad = new Buttons("D-Pad"));
	for (auto& named_direction : named_directions)
		m_dpad->controls.emplace_back(new ControlGroup::Input(named_direction));

	// set up register
	// calibration
	memcpy(&calibration, classic_calibration, sizeof(classic_calibration));
	// id
	memcpy(&id, classic_id, sizeof(classic_id));
}

void Classic::GetState(u8* const data, const bool focus)
{
	wm_classic_extension* const ccdata = (wm_classic_extension*)data;
	ccdata->bt = 0;

	// not using calibration data, o well

	// left stick
	{
	u8 x, y;
	m_left_stick->GetState(&x, &y, 0x20, focus ? 0x1F /*0x15*/ : 0);

	ccdata->lx = x;
	ccdata->ly = y;
	}

	// right stick
	{
	u8 x, y;
	m_right_stick->GetState(&x, &y, 0x10, focus ? 0x0F /*0x0C*/ : 0);

	ccdata->rx1 = x;
	ccdata->rx2 = x >> 1;
	ccdata->rx3 = x >> 3;
	ccdata->ry = y;
	}

	//triggers
	{
	u8 trigs[2];
	m_triggers->GetState(&ccdata->bt, classic_trigger_bitmasks, trigs, focus ? 0x1F : 0);

	ccdata->lt1 = trigs[0];
	ccdata->lt2 = trigs[0] >> 3;
	ccdata->rt = trigs[1];
	}

	if (focus)
	{
		// buttons
		m_buttons->GetState(&ccdata->bt, classic_button_bitmasks);
		// dpad
		m_dpad->GetState(&ccdata->bt, classic_dpad_bitmasks);
	}

#ifdef _WIN32
	// VR Sixense Razer hydra support
	if (HydraUpdate() && g_hydra.c[0].enabled)
	{
		const int left = 0, right = 1;
		// L is analog so must be Trigger, therefore ZL is left bumper.
		// Unfortunately this is the opposite layout from the Classic Controller Pro.
		if (g_hydra.c[left].buttons & HYDRA_BUTTON_BUMPER)
		{
			ccdata->bt |= Classic::BUTTON_ZL;
		}
		// The hydra doesn't have a DPad, so use the buttons on the left controller.
		// Imagine the controllers are tilted inwards like a Classic Controller Pro.
		// Therefore 3 is at the top.
		if (g_hydra.c[left].buttons & HYDRA_BUTTON_3)
		{
			ccdata->bt |= Classic::PAD_UP;
		}
		if (g_hydra.c[left].buttons & HYDRA_BUTTON_2)
		{
			ccdata->bt |= Classic::PAD_DOWN;
		}
		if (g_hydra.c[left].buttons & HYDRA_BUTTON_1)
		{
			ccdata->bt |= Classic::PAD_LEFT;
		}
		if (g_hydra.c[left].buttons & HYDRA_BUTTON_4)
		{
			ccdata->bt |= Classic::PAD_RIGHT;
		}
		// Left Start = - button
		if (g_hydra.c[left].buttons & HYDRA_BUTTON_START)
		{
			ccdata->bt |= Classic::BUTTON_MINUS;
		}
		if (!g_hydra.c[left].docked)
		{
			// Left analog stick
			ccdata->lx = (u8)(31.5f + g_hydra.c[left].stick_x * 31.5f);
			ccdata->ly = (u8)(31.5f + g_hydra.c[left].stick_y * 31.5f);
			// Left analog trigger = L
			u8 trigger = (u8)(g_hydra.c[left].trigger * 31);
			ccdata->lt1 = trigger;
			ccdata->lt2 = trigger >> 3;
			if (g_hydra.c[left].trigger > 0.9)
			{
				ccdata->bt |= Classic::TRIGGER_L;
			}
		}
		// Right controller
		if (g_hydra.c[right].enabled && !g_hydra.c[right].docked)
		{
			// Right analog stick
			u8 x = u8(15.5f + g_hydra.c[right].stick_x * 15.5f);
			u8 y = u8(15.5f + g_hydra.c[right].stick_y * 15.5f);
			ccdata->rx1 = x;
			ccdata->rx2 = x >> 1;
			ccdata->rx3 = x >> 3;
			ccdata->ry = y;
			// Right analog trigger = R
			u8 trigger = (u8)(g_hydra.c[right].trigger * 31);
			ccdata->rt = trigger;
			if (g_hydra.c[right].trigger > 0.9)
			{
				ccdata->bt |= Classic::TRIGGER_R;
			}
		}
		// Right stick in = Home button
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_STICK)
		{
			ccdata->bt |= Classic::BUTTON_HOME;
		}
		// Right Start = + button
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_START)
		{
			ccdata->bt |= Classic::BUTTON_PLUS;
		}
		// Imagine controllers are tilted inwards like holding a Classic Controller Pro.
		// Therefore 1 = b, 2 = a, 3 = y, 4 = x
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_1)
		{
			ccdata->bt |= Classic::BUTTON_B;
		}
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_2)
		{
			ccdata->bt |= Classic::BUTTON_A;
		}
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_3)
		{
			ccdata->bt |= Classic::BUTTON_Y;
		}
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_4)
		{
			ccdata->bt |= Classic::BUTTON_X;
		}
		// R is analog so must be Trigger, therefore ZR is right bumper.
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_BUMPER)
		{
			ccdata->bt |= Classic::BUTTON_ZL;
		}
	}
#endif

	// flip button bits
	ccdata->bt ^= 0xFFFF;
}


}
