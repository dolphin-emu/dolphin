// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.


#include "InputCommon/ControllerInterface/Vive/ViveInput.h"
#include "VideoCommon/VR.h"

namespace ciface
{
namespace ViveInput
{

	static const struct
	{
		const char* const name;
		const u32 bitmask;
	} vive_buttons[] =
	{
		// These exist on a normal Vive controller
		{ "LTouchpad", VIVE_BUTTON_LEFT_TOUCHPAD },
		{ "LMenu", VIVE_BUTTON_LEFT_MENU },
		{ "LGrip", VIVE_BUTTON_LEFT_GRIP },
		{ "RTouchpad", VIVE_BUTTON_RIGHT_TOUCHPAD },
		{ "RMenu", VIVE_BUTTON_RIGHT_MENU },
		{ "RGrip", VIVE_BUTTON_RIGHT_GRIP },
		// These exist but aren't normally readable
		{ "LSystem", VIVE_BUTTON_LEFT_SYSTEM },
		{ "RSystem", VIVE_BUTTON_RIGHT_SYSTEM },
		// These are defined in the SDK, but don't exist on a normal Vive controller
		// Other controllers are usable with OpenVR though, so we expose these
		{ "LA", VIVE_BUTTON_LEFT_A },
		{ "LDPadUp", VIVE_BUTTON_LEFT_UP },
		{ "LDPadDown", VIVE_BUTTON_LEFT_DOWN },
		{ "LDPadLeft", VIVE_BUTTON_LEFT_LEFT },
		{ "LDPadRight", VIVE_BUTTON_LEFT_RIGHT },
		{ "RA", VIVE_BUTTON_RIGHT_A },
		{ "RDPadUp", VIVE_BUTTON_RIGHT_UP },
		{ "RDPadDown", VIVE_BUTTON_RIGHT_DOWN },
		{ "RDPadRight", VIVE_BUTTON_RIGHT_LEFT },
		{ "RDPadRight", VIVE_BUTTON_RIGHT_RIGHT },
	}, vive_specials[] =
	{
		{ "LClickUp", VIVE_SPECIAL_DPAD_UP },
		{ "LClickDown", VIVE_SPECIAL_DPAD_DOWN },
		{ "LClickLeft", VIVE_SPECIAL_DPAD_LEFT },
		{ "LClickRight", VIVE_SPECIAL_DPAD_RIGHT },
		{ "LClickMiddle", VIVE_SPECIAL_DPAD_MIDDLE },
		{ "RClickUp", VIVE_SPECIAL_DPAD_UP << 16},
		{ "RClickDown", VIVE_SPECIAL_DPAD_DOWN << 16},
		{ "RClickLeft", VIVE_SPECIAL_DPAD_LEFT << 16},
		{ "RClickRight", VIVE_SPECIAL_DPAD_RIGHT << 16},
		{ "RClickMiddle", VIVE_SPECIAL_DPAD_MIDDLE << 16},
		{ "RGameCubeA", VIVE_SPECIAL_GC_A << 16},
		{ "RGameCubeB", VIVE_SPECIAL_GC_B << 16 },
		{ "RGameCubeX", VIVE_SPECIAL_GC_X << 16 },
		{ "RGameCubeY", VIVE_SPECIAL_GC_Y << 16 },
		{ "RGameCubeEmpty", VIVE_SPECIAL_GC_EMPTY << 16 },
	}, vive_touches[] =
	{
		// These exist on a normal Vive controller
		{ "TouchLTouchpad", VIVE_BUTTON_LEFT_TOUCHPAD },
		{ "TouchRTouchpad", VIVE_BUTTON_RIGHT_TOUCHPAD },
		// These are defined in the SDK, but don't exist on a normal Vive controller
		// Other controllers are usable with OpenVR though, so we expose these
		{ "TouchLTrigger", VIVE_BUTTON_LEFT_TRIGGER },
		{ "TouchLMenu", VIVE_BUTTON_LEFT_MENU },
		{ "TouchLGrip", VIVE_BUTTON_LEFT_GRIP },
		{ "TouchRTrigger", VIVE_BUTTON_RIGHT_TRIGGER },
		{ "TouchRMenu", VIVE_BUTTON_RIGHT_MENU },
		{ "TouchRGrip", VIVE_BUTTON_RIGHT_GRIP },
		{ "TouchLSystem", VIVE_BUTTON_LEFT_SYSTEM },
		{ "TouchLA", VIVE_BUTTON_LEFT_A },
		{ "TouchLDPadUp", VIVE_BUTTON_LEFT_UP },
		{ "TouchLDPadDown", VIVE_BUTTON_LEFT_DOWN },
		{ "TouchLDPadLeft", VIVE_BUTTON_LEFT_LEFT },
		{ "TouchLDPadRight", VIVE_BUTTON_LEFT_RIGHT },
		{ "TouchRA", VIVE_BUTTON_RIGHT_A },
		{ "TouchRSystem", VIVE_BUTTON_RIGHT_SYSTEM },
		{ "TouchRDPadUp", VIVE_BUTTON_RIGHT_UP },
		{ "TouchRDPadDown", VIVE_BUTTON_RIGHT_DOWN },
		{ "TouchRDPadLeft", VIVE_BUTTON_RIGHT_LEFT },
		{ "TouchRDPadRight", VIVE_BUTTON_RIGHT_RIGHT },
	};

	static const char* const named_triggers[] =
	{
		"LTrigger",
		"RTrigger",
	};

	static const char* const named_axes[] =
	{
		"LAnalogX",
		"LAnalogY",
		"RAnalogX",
		"RAnalogY",
		"LTouchX",
		"LTouchY",
		"RTouchX",
		"RTouchY"
	};

	static const char* const named_motors[] =
	{
		"LHaptic",
		"RHaptic",
	};

void Init(std::vector<Core::Device*>& devices)
{
	devices.push_back(new ViveController());
}

void DeInit()
{
}

ViveController::ViveController()
{
	for (int i = 0; i != sizeof(vive_buttons) / sizeof(*vive_buttons); ++i)
	{
		AddInput(new Button(i, m_buttons));
	}
	for (int i = 0; i < 2; ++i)
	{
		AddInput(new Trigger(i, m_triggers));
	}
	for (int i = 0; i != sizeof(vive_specials) / sizeof(*vive_specials); ++i)
	{
		AddInput(new Special(i, m_specials));
	}
	for (int i = 0; i < 8; ++i)
	{
		AddInput(new Axis(i, -1, m_axes));
		AddInput(new Axis(i, 1, m_axes));
	}
	for (int i = 0; i != sizeof(vive_touches) / sizeof(*vive_touches); ++i)
	{
		AddInput(new Touch(i, m_touches));
	}

	for (int i = 0; i != sizeof(named_motors) / sizeof(*named_motors); ++i)
	{
		AddOutput(new Motor(i, this, m_motors));
	}

	ZeroMemory(&m_buttons, sizeof(m_buttons));
	ZeroMemory(&m_touches, sizeof(m_touches));
	ZeroMemory(&m_motors, sizeof(m_motors));
	ZeroMemory(m_triggers, sizeof(m_triggers));
	ZeroMemory(m_axes, sizeof(m_axes));
}

std::string ViveController::GetName() const
{
	return "Vive";
}

int ViveController::GetId() const
{
	return 0;
}

std::string ViveController::GetSource() const
{
	return "VR";
}

// Update I/O

void ViveController::UpdateInput()
{
	VR_GetViveButtons(&m_buttons, &m_touches, &m_specials, m_triggers, m_axes);
	UpdateMotors();
}

void ViveController::UpdateMotors()
{
	if (m_motors)
		VR_ViveHapticPulse(m_motors, 3999);
}

// GET name/source/id

std::string ViveController::Button::GetName() const
{
	return vive_buttons[m_index].name;
}

// GET / SET STATES

ControlState ViveController::Button::GetState() const
{
	return (m_buttons & vive_buttons[m_index].bitmask) > 0;
}

u32 ViveController::Button::GetStates() const
{
	return (u32)m_buttons;
}

// GET name/source/id

std::string ViveController::Special::GetName() const
{
	return vive_specials[m_index].name;
}

// GET / SET STATES

ControlState ViveController::Special::GetState() const
{
	return (m_buttons & vive_specials[m_index].bitmask) > 0;
}

u32 ViveController::Special::GetStates() const
{
	return (u32)m_buttons;
}

// GET name/source/id

std::string ViveController::Touch::GetName() const
{
	return vive_touches[m_index].name;
}

std::string ViveController::Axis::GetName() const
{
	return std::string(named_axes[m_index]) + (m_range<0 ? '-' : '+');
}

std::string ViveController::Trigger::GetName() const
{
	return named_triggers[m_index];
}

std::string ViveController::Motor::GetName() const
{
	return named_motors[m_index];
}

// GET / SET STATES

ControlState ViveController::Touch::GetState() const
{
	return (m_touches & vive_touches[m_index].bitmask) > 0;
}

ControlState ViveController::Trigger::GetState() const
{
	return ControlState(m_triggers[m_index]);
}

ControlState ViveController::Axis::GetState() const
{
	return std::max(0.0, ControlState(m_axes[m_index]*m_range));
}

void ViveController::Motor::SetState(ControlState state)
{
	if (state > 0)
		m_motors |= (1 << m_index);
	else
		m_motors &= ~(1 << m_index);
	m_parent->UpdateMotors();
}

u32 ViveController::Touch::GetStates() const
{
	return (u32)m_touches;
}


}
}
