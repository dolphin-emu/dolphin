// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Host.h"
#include "Core/HW/GCPadEmu.h"

static const u16 button_bitmasks[] =
{
	PAD_BUTTON_A,
	PAD_BUTTON_B,
	PAD_BUTTON_X,
	PAD_BUTTON_Y,
	PAD_TRIGGER_Z,
	PAD_BUTTON_START,
	0 // MIC HAX
};

static const u16 trigger_bitmasks[] =
{
	PAD_TRIGGER_L,
	PAD_TRIGGER_R,
};

static const u16 dpad_bitmasks[] =
{
	PAD_BUTTON_UP, PAD_BUTTON_DOWN, PAD_BUTTON_LEFT, PAD_BUTTON_RIGHT
};

static const char* const named_buttons[] =
{
	"A",
	"B",
	"X",
	"Y",
	"Z",
	_trans("Start"),
	"Mic"
};

static const char* const named_triggers[] =
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
	groups.emplace_back(m_main_stick = new AnalogStick("Main Stick", _trans("Control Stick"), DEFAULT_PAD_STICK_RADIUS));
	groups.emplace_back(m_c_stick = new AnalogStick("C-Stick", _trans("C Stick"), DEFAULT_PAD_STICK_RADIUS));

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
	m_options->settings.emplace_back(new ControlGroup::BackgroundInputSetting(_trans("Background Input")));
	m_options->settings.emplace_back(new ControlGroup::IterateUI(_trans("Iterative Input")));
}

std::string GCPad::GetName() const
{
	return std::string("GCPad") + char('1'+m_index);
}

void GCPad::GetInput(GCPadStatus* const pad)
{
	ControlState x, y, triggers[2];

	// buttons
	m_buttons->GetState(&pad->button, button_bitmasks);

	// set analog A/B analog to full or w/e, prolly not needed
	if (pad->button & PAD_BUTTON_A) pad->analogA = 0xFF;
	if (pad->button & PAD_BUTTON_B) pad->analogB = 0xFF;

	// dpad
	m_dpad->GetState(&pad->button, dpad_bitmasks);

	// sticks
	m_main_stick->GetState(&x, &y);
	pad->stickX = static_cast<u8>(GCPadStatus::MAIN_STICK_CENTER_X + (x * GCPadStatus::MAIN_STICK_RADIUS));
	pad->stickY = static_cast<u8>(GCPadStatus::MAIN_STICK_CENTER_Y + (y * GCPadStatus::MAIN_STICK_RADIUS));

	m_c_stick->GetState(&x, &y);
	pad->substickX = static_cast<u8>(GCPadStatus::C_STICK_CENTER_X + (x * GCPadStatus::C_STICK_RADIUS));
	pad->substickY = static_cast<u8>(GCPadStatus::C_STICK_CENTER_Y + (y * GCPadStatus::C_STICK_RADIUS));

	// triggers
	m_triggers->GetState(&pad->button, trigger_bitmasks, triggers);
	pad->triggerLeft = static_cast<u8>(triggers[0] * 0xFF);
	pad->triggerRight = static_cast<u8>(triggers[1] * 0xFF);
}

void GCPad::SetOutput(const ControlState strength)
{
	m_rumble->controls[0]->control_ref->State(strength);
}

void GCPad::LoadDefaults(const ControllerInterface& ciface)
{
	ControllerEmu::LoadDefaults(ciface);

	// Buttons
	m_buttons->SetControlExpression(0, "X"); // A
	m_buttons->SetControlExpression(1, "Z"); // B
	m_buttons->SetControlExpression(2, "C"); // X
	m_buttons->SetControlExpression(3, "S"); // Y
	m_buttons->SetControlExpression(4, "D"); // Z
#ifdef _WIN32
	m_buttons->SetControlExpression(5, "!LMENU & RETURN"); // Start
#else
	// OS X/Linux
	m_buttons->SetControlExpression(5, "!`Alt_L` & Return"); // Start
#endif

	// stick modifiers to 50 %
	m_main_stick->controls[4]->control_ref->range = 0.5f;
	m_c_stick->controls[4]->control_ref->range = 0.5f;

	// D-Pad
	m_dpad->SetControlExpression(0, "T"); // Up
	m_dpad->SetControlExpression(1, "G"); // Down
	m_dpad->SetControlExpression(2, "F"); // Left
	m_dpad->SetControlExpression(3, "H"); // Right

	// C Stick
	m_c_stick->SetControlExpression(0, "I"); // Up
	m_c_stick->SetControlExpression(1, "K"); // Down
	m_c_stick->SetControlExpression(2, "J"); // Left
	m_c_stick->SetControlExpression(3, "L"); // Right
#ifdef _WIN32
	m_c_stick->SetControlExpression(4, "LCONTROL"); // Modifier

	// Control Stick
	m_main_stick->SetControlExpression(0, "UP");     // Up
	m_main_stick->SetControlExpression(1, "DOWN");   // Down
	m_main_stick->SetControlExpression(2, "LEFT");   // Left
	m_main_stick->SetControlExpression(3, "RIGHT");  // Right
	m_main_stick->SetControlExpression(4, "LSHIFT"); // Modifier

#elif __APPLE__
	m_c_stick->SetControlExpression(4, "Left Control"); // Modifier

	// Control Stick
	m_main_stick->SetControlExpression(0, "Up Arrow");    // Up
	m_main_stick->SetControlExpression(1, "Down Arrow");  // Down
	m_main_stick->SetControlExpression(2, "Left Arrow");  // Left
	m_main_stick->SetControlExpression(3, "Right Arrow"); // Right
	m_main_stick->SetControlExpression(4, "Left Shift");  // Modifier
#else
	// not sure if these are right

	m_c_stick->SetControlExpression(4, "Control_L"); // Modifier

	// Control Stick
	m_main_stick->SetControlExpression(0, "Up");      // Up
	m_main_stick->SetControlExpression(1, "Down");    // Down
	m_main_stick->SetControlExpression(2, "Left");    // Left
	m_main_stick->SetControlExpression(3, "Right");   // Right
	m_main_stick->SetControlExpression(4, "Shift_L"); // Modifier
#endif

	// Triggers
	m_triggers->SetControlExpression(0, "Q"); // L
	m_triggers->SetControlExpression(1, "W"); // R
}

bool GCPad::GetMicButton() const
{
	return (0.0f != m_buttons->controls.back()->control_ref->State());
}
