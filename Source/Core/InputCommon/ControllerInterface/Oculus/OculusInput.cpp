// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.


#include "InputCommon/ControllerInterface/Oculus/OculusInput.h"
#include "VideoCommon/VR.h"

namespace ciface
{
namespace OculusInput
{

	static const struct
	{
		const char* const name;
		const u32 bitmask;
	} named_buttons[] =
	{
		{ "Select", OCULUS_BUTTON_ENTER },
		{ "Back", OCULUS_BUTTON_BACK },
		{ "Up", OCULUS_BUTTON_UP },
		{ "Down", OCULUS_BUTTON_DOWN },
		{ "Left", OCULUS_BUTTON_LEFT },
		{ "Right", OCULUS_BUTTON_RIGHT },
		{ "Minus", OCULUS_BUTTON_MINUS },
		{ "Plus", OCULUS_BUTTON_PLUS },
		{ "Oculus", OCULUS_BUTTON_HOME }
	};
	
void Init(std::vector<Core::Device*>& devices)
{
	devices.push_back(new OculusDevice());
}

void DeInit()
{
}

OculusDevice::OculusDevice()
{
	// get supported buttons
	for (int i = 0; i != sizeof(named_buttons) / sizeof(*named_buttons); ++i)
	{
		AddInput(new Button(i, m_state));
	}

	ZeroMemory(&m_state, sizeof(m_state));
}

OculusDevice::~OculusDevice()
{
}

std::string OculusDevice::GetName() const
{
	return "Remote";
}

int OculusDevice::GetId() const
{
	return 0;
}

std::string OculusDevice::GetSource() const
{
	return "VR";
}

// Update I/O

void OculusDevice::UpdateInput()
{
	VR_GetRemoteButtons(&m_state);
}

// GET name/source/id

std::string OculusDevice::Button::GetName() const
{
	return named_buttons[m_index].name;
}

// GET / SET STATES

ControlState OculusDevice::Button::GetState() const
{
	return (m_buttons & named_buttons[m_index].bitmask) > 0;
}

u32 OculusDevice::Button::GetStates() const
{
	return (u32) m_buttons;
}

}
}
