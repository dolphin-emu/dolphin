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
		{ "LA", VIVE_BUTTON_LEFT_A },
		{ "LMenu", VIVE_BUTTON_LEFT_MENU },
		{ "LGrip", VIVE_BUTTON_LEFT_GRIP },
		{ "LSystem", VIVE_BUTTON_LEFT_SYSTEM },
		{ "LDPadUp", VIVE_BUTTON_LEFT_UP },
		{ "LDPadDown", VIVE_BUTTON_LEFT_DOWN },
		{ "LDPadLeft", VIVE_BUTTON_LEFT_LEFT },
		{ "LDPadRight", VIVE_BUTTON_LEFT_RIGHT },
		{ "RA", VIVE_BUTTON_RIGHT_A },
		{ "RMenu", VIVE_BUTTON_RIGHT_MENU },
		{ "RGrip", VIVE_BUTTON_RIGHT_GRIP },
		{ "RSystem", VIVE_BUTTON_RIGHT_SYSTEM },
		{ "RDPadUp", VIVE_BUTTON_RIGHT_UP },
		{ "RDPadDown", VIVE_BUTTON_RIGHT_DOWN },
		{ "RDPadRight", VIVE_BUTTON_RIGHT_LEFT },
		{ "RDPadRight", VIVE_BUTTON_RIGHT_RIGHT },
	}, vive_touches[] =
	{
		{ "TouchLA", VIVE_BUTTON_LEFT_A },
		{ "TouchLMenu", VIVE_BUTTON_LEFT_MENU },
		{ "TouchLGrip", VIVE_BUTTON_LEFT_GRIP },
		{ "TouchLSystem", VIVE_BUTTON_LEFT_SYSTEM },
		{ "TouchLDPadUp", VIVE_BUTTON_LEFT_UP },
		{ "TouchLDPadDown", VIVE_BUTTON_LEFT_DOWN },
		{ "TouchLDPadLeft", VIVE_BUTTON_LEFT_LEFT },
		{ "TouchLDPadRight", VIVE_BUTTON_LEFT_RIGHT },
		{ "TouchRA", VIVE_BUTTON_RIGHT_A },
		{ "TouchRMenu", VIVE_BUTTON_RIGHT_MENU },
		{ "TouchRGrip", VIVE_BUTTON_RIGHT_GRIP },
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
		"LTouchX",
		"LTouchY",
		"RTouchX",
		"RTouchY"
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
	for (int i = 0; i < 4; ++i)
	{
		AddInput(new Axis(i, -1, m_axes));
		AddInput(new Axis(i, 1, m_axes));
	}
	for (int i = 0; i != sizeof(vive_touches) / sizeof(*vive_touches); ++i)
	{
		AddInput(new Touch(i, m_touches));
	}

	ZeroMemory(&m_buttons, sizeof(m_buttons));
	ZeroMemory(&m_touches, sizeof(m_touches));
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
	VR_GetViveButtons(&m_buttons, &m_touches, m_triggers, m_axes);
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

u32 ViveController::Touch::GetStates() const
{
	return (u32)m_touches;
}


}
}
