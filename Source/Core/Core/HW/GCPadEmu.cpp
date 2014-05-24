// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/Host.h"
#include "Core/HW/GCPadEmu.h"

#ifdef _WIN32
#include "InputCommon/ControllerInterface/Sixense/SixenseHack.h"
#endif

const u16 button_bitmasks[] =
{
	PAD_BUTTON_A,
	PAD_BUTTON_B,
	PAD_BUTTON_X,
	PAD_BUTTON_Y,
	PAD_TRIGGER_Z,
	PAD_BUTTON_START,
	0 // MIC HAX
};

const u16 trigger_bitmasks[] =
{
	PAD_TRIGGER_L,
	PAD_TRIGGER_R,
};

const u16 dpad_bitmasks[] =
{
	PAD_BUTTON_UP, PAD_BUTTON_DOWN, PAD_BUTTON_LEFT, PAD_BUTTON_RIGHT
};

const char* const named_buttons[] =
{
	"A",
	"B",
	"X",
	"Y",
	"Z",
	_trans("Start"),
	"Mic"
};

const char* const named_triggers[] =
{
	// i18n:  Left
	_trans("L"),
	// i18n:  Right
	_trans("R"),
	// i18n:  Left-Analog
	_trans("L-Analog"),
	// i18n:  Right-Analog
	_trans("R-Analog")
};

GCPad::GCPad(const unsigned int index) : m_index(index)
{
	int const mic_hax = index > 1;

	// buttons
	groups.emplace_back(m_buttons = new Buttons(_trans("Buttons")));
	for (unsigned int i=0; i < sizeof(named_buttons)/sizeof(*named_buttons) - mic_hax; ++i)
		m_buttons->controls.emplace_back(new ControlGroup::Input(named_buttons[i]));

	// sticks
	groups.emplace_back(m_main_stick = new AnalogStick(_trans("Main Stick")));
	groups.emplace_back(m_c_stick = new AnalogStick(_trans("C-Stick")));

	// triggers
	groups.emplace_back(m_triggers = new MixedTriggers(_trans("Triggers")));
	for (auto& named_trigger : named_triggers)
		m_triggers->controls.emplace_back(new ControlGroup::Input(named_trigger));

	// rumble
	groups.emplace_back(m_rumble = new ControlGroup(_trans("Rumble")));
	m_rumble->controls.emplace_back(new ControlGroup::Output(_trans("Motor")));

	// dpad
	groups.emplace_back(m_dpad = new Buttons(_trans("D-Pad")));
	for (auto& named_direction : named_directions)
		m_dpad->controls.emplace_back(new ControlGroup::Input(named_direction));

	// options
	groups.emplace_back(m_options = new ControlGroup(_trans("Options")));
	m_options->settings.emplace_back(new ControlGroup::Setting(_trans("Background Input"), false));
}

std::string GCPad::GetName() const
{
	return std::string("GCPad") + char('1'+m_index);
}

void GCPad::GetInput(SPADStatus* const pad)
{
	// if window has focus or background input enabled
	if (Host_RendererHasFocus() || m_options[0].settings[0]->value)
	{
		// buttons
		m_buttons->GetState(&pad->button, button_bitmasks);

		// set analog A/B analog to full or w/e, prolly not needed
		if (pad->button & PAD_BUTTON_A) pad->analogA = 0xFF;
		if (pad->button & PAD_BUTTON_B) pad->analogB = 0xFF;

		// dpad
		m_dpad->GetState(&pad->button, dpad_bitmasks);

		// sticks
		m_main_stick->GetState(&pad->stickX, &pad->stickY, 0x80, 127);
		m_c_stick->GetState(&pad->substickX, &pad->substickY, 0x80, 127);

		// triggers
		m_triggers->GetState(&pad->button, trigger_bitmasks, &pad->triggerLeft, 0xFF);

#ifdef _WIN32
		// VR Sixense Razer hydra support
		if (HydraUpdate() && g_hydra.c[0].enabled && !g_hydra.c[0].docked)
		{
			const int left = 0, right = 1;
			if (g_hydra.c[left].buttons & HYDRA_BUTTON_BUMPER || g_hydra.c[right].buttons & HYDRA_BUTTON_BUMPER)
			{
				pad->button |= PAD_TRIGGER_Z;
			}
			// The hydra doesn't have a DPad, so use the buttons on the left controller.
			// Imagine the controllers are tilted inwards.
			// Therefore 3 is at the top.
			if (g_hydra.c[left].buttons & HYDRA_BUTTON_3)
			{
				pad->button |= PAD_BUTTON_UP;
			}
			if (g_hydra.c[left].buttons & HYDRA_BUTTON_2)
			{
				pad->button |= PAD_BUTTON_DOWN;
			}
			if (g_hydra.c[left].buttons & HYDRA_BUTTON_1)
			{
				pad->button |= PAD_BUTTON_LEFT;
			}
			if (g_hydra.c[left].buttons & HYDRA_BUTTON_4)
			{
				pad->button |= PAD_BUTTON_RIGHT;
			}
			{
				// Left analog stick
				pad->stickX = (u8)(0x80 + g_hydra_state[left].jx * 127);
				pad->stickY = (u8)(0x80 + g_hydra_state[left].jy * 127);
				// Left analog trigger = L
				pad->triggerLeft = (u8)(g_hydra.c[left].trigger * 255);
				if (g_hydra.c[left].trigger > 0.9)
				{
					pad->button |= PAD_TRIGGER_L;
				}
			}
			// Right controller
			if (g_hydra.c[right].enabled && !g_hydra.c[right].docked)
			{
				// Right analog stick
				pad->substickX = (u8)(0x80 + g_hydra_state[right].jx * 127);
				pad->substickY = (u8)(0x80 + g_hydra_state[right].jy * 127);
				// Right analog trigger = R
				pad->triggerRight = (u8)(g_hydra.c[right].trigger * 255);
				if (g_hydra.c[right].trigger > 0.9)
				{
					pad->button |= PAD_TRIGGER_R;
				}
			}
			// Right Start = START/Pause button
			if (g_hydra.c[right].buttons & HYDRA_BUTTON_START)
			{
				pad->button |= PAD_BUTTON_START;
			}
			// 1 = b, 2 = a, 3 = y, 4 = x
			if (g_hydra.c[right].buttons & HYDRA_BUTTON_1)
			{
				pad->button |= PAD_BUTTON_B;
			}
			if (g_hydra.c[right].buttons & HYDRA_BUTTON_2)
			{
				pad->button |= PAD_BUTTON_A;
			}
			if (g_hydra.c[right].buttons & HYDRA_BUTTON_3)
			{
				pad->button |= PAD_BUTTON_Y;
			}
			if (g_hydra.c[right].buttons & HYDRA_BUTTON_4)
			{
				pad->button |= PAD_BUTTON_X;
			}
			// set analog A/B analog to full or w/e, prolly not needed
			if (pad->button & PAD_BUTTON_A) pad->analogA = 0xFF;
			if (pad->button & PAD_BUTTON_B) pad->analogB = 0xFF;
		}
#endif

	}
	else
	{
		// center sticks
		memset(&pad->stickX, 0x80, 4);
	}
}

void GCPad::SetMotor(const u8 on)
{
	float state = (float)on / 255;
	float force = abs(state - 0.5f) * 2;
	if (state < 0.5)
		force = -force;

	// only rumble if window has focus or background input is enabled
	if (Host_RendererHasFocus() || m_options[0].settings[0]->value)
		m_rumble->controls[0]->control_ref->State(force);
	else
		m_rumble->controls[0]->control_ref->State(0);
}

void GCPad::SetOutput(const u8 on)
{
	// only rumble if window has focus or background input is enabled
	m_rumble->controls[0]->control_ref->State(on && (Host_RendererHasFocus() || m_options[0].settings[0]->value));
}

void GCPad::LoadDefaults(const ControllerInterface& ciface)
{
	#define set_control(group, num, str)  (group)->controls[num]->control_ref->expression = (str)

	ControllerEmu::LoadDefaults(ciface);

	// Buttons
	set_control(m_buttons, 0, "X"); // A
	set_control(m_buttons, 1, "Z"); // B
	set_control(m_buttons, 2, "C"); // X
	set_control(m_buttons, 3, "S"); // Y
	set_control(m_buttons, 4, "D"); // Z
#ifdef _WIN32
	set_control(m_buttons, 5, "RETURN"); // Start
#else
	// osx/linux
	set_control(m_buttons, 5, "Return"); // Start
#endif

	// stick modifiers to 50 %
	m_main_stick->controls[4]->control_ref->range = 0.5f;
	m_c_stick->controls[4]->control_ref->range = 0.5f;

	// D-Pad
	set_control(m_dpad, 0, "T"); // Up
	set_control(m_dpad, 1, "G"); // Down
	set_control(m_dpad, 2, "F"); // Left
	set_control(m_dpad, 3, "H"); // Right

	// C-Stick
	set_control(m_c_stick, 0, "I"); // Up
	set_control(m_c_stick, 1, "K"); // Down
	set_control(m_c_stick, 2, "J"); // Left
	set_control(m_c_stick, 3, "L"); // Right
#ifdef _WIN32
	set_control(m_c_stick, 4, "LCONTROL"); // Modifier

	// Main Stick
	set_control(m_main_stick, 0, "UP");     // Up
	set_control(m_main_stick, 1, "DOWN");   // Down
	set_control(m_main_stick, 2, "LEFT");   // Left
	set_control(m_main_stick, 3, "RIGHT");  // Right
	set_control(m_main_stick, 4, "LSHIFT"); // Modifier

#elif __APPLE__
	set_control(m_c_stick, 4, "Left Control"); // Modifier

	// Main Stick
	set_control(m_main_stick, 0, "Up Arrow");    // Up
	set_control(m_main_stick, 1, "Down Arrow");  // Down
	set_control(m_main_stick, 2, "Left Arrow");  // Left
	set_control(m_main_stick, 3, "Right Arrow"); // Right
	set_control(m_main_stick, 4, "Left Shift");  // Modifier
#else
	// not sure if these are right

	set_control(m_c_stick, 4, "Control_L"); // Modifier

	// Main Stick
	set_control(m_main_stick, 0, "Up");      // Up
	set_control(m_main_stick, 1, "Down");    // Down
	set_control(m_main_stick, 2, "Left");    // Left
	set_control(m_main_stick, 3, "Right");   // Right
	set_control(m_main_stick, 4, "Shift_L"); // Modifier
#endif

	// Triggers
	set_control(m_triggers, 0, "Q"); // L
	set_control(m_triggers, 1, "W"); // R
}

bool GCPad::GetMicButton() const
{
	return (0.0f != m_buttons->controls.back()->control_ref->State());
}
