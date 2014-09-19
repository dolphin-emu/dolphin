// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <sstream>
#include "InputCommon/ControllerInterface/Android/Android.h"

namespace ciface
{

namespace Android
{

void Init( std::vector<Core::Device*>& devices )
{
	devices.push_back(new Touchscreen(0));
	devices.push_back(new Touchscreen(1));
	devices.push_back(new Touchscreen(2));
	devices.push_back(new Touchscreen(3));
}

// Touchscreens and stuff
std::string Touchscreen::GetName() const
{
	return "Touchscreen";
}

std::string Touchscreen::GetSource() const
{
	return "Android";
}

int Touchscreen::GetId() const
{
	return 0;
}
Touchscreen::Touchscreen(int padID)
	: _padID(padID)
{
	AddInput(new Button(_padID, ButtonManager::BUTTON_A));
	AddInput(new Button(_padID, ButtonManager::BUTTON_B));
	AddInput(new Button(_padID, ButtonManager::BUTTON_START));
	AddInput(new Button(_padID, ButtonManager::BUTTON_X));
	AddInput(new Button(_padID, ButtonManager::BUTTON_Y));
	AddInput(new Button(_padID, ButtonManager::BUTTON_Z));
	AddInput(new Button(_padID, ButtonManager::BUTTON_UP));
	AddInput(new Button(_padID, ButtonManager::BUTTON_DOWN));
	AddInput(new Button(_padID, ButtonManager::BUTTON_LEFT));
	AddInput(new Button(_padID, ButtonManager::BUTTON_RIGHT));
	AddAnalogInputs(new Axis(_padID, ButtonManager::STICK_MAIN_LEFT), new Axis(_padID, ButtonManager::STICK_MAIN_RIGHT));
	AddAnalogInputs(new Axis(_padID, ButtonManager::STICK_MAIN_UP), new Axis(_padID, ButtonManager::STICK_MAIN_DOWN));
	AddAnalogInputs(new Axis(_padID, ButtonManager::STICK_C_LEFT), new Axis(_padID, ButtonManager::STICK_C_RIGHT));
	AddAnalogInputs(new Axis(_padID, ButtonManager::STICK_C_UP), new Axis(_padID, ButtonManager::STICK_C_DOWN));
	AddAnalogInputs(new Axis(_padID, ButtonManager::TRIGGER_L), new Axis(_padID, ButtonManager::TRIGGER_L));
	AddAnalogInputs(new Axis(_padID, ButtonManager::TRIGGER_R), new Axis(_padID, ButtonManager::TRIGGER_R));
}
// Buttons and stuff

std::string Touchscreen::Button::GetName() const
{
	std::ostringstream ss;
	ss << "Button " << (int)_index;
	return ss.str();
}

ControlState Touchscreen::Button::GetState() const
{
	return ButtonManager::GetButtonPressed(_padID, _index);
}
std::string Touchscreen::Axis::GetName() const
{
	std::ostringstream ss;
	ss << "Axis " << (int)_index;
	return ss.str();
}

ControlState Touchscreen::Axis::GetState() const
{
	return ButtonManager::GetAxisValue(_padID, _index) * _neg;
}

}
}
