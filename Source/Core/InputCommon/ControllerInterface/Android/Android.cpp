// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
Touchscreen::Touchscreen(int _padID)
	: padID(_padID)
{
	AddInput(new Button(padID, ButtonManager::BUTTON_A));
	AddInput(new Button(padID, ButtonManager::BUTTON_B));
	AddInput(new Button(padID, ButtonManager::BUTTON_START));
	AddInput(new Button(padID, ButtonManager::BUTTON_X));
	AddInput(new Button(padID, ButtonManager::BUTTON_Y));
	AddInput(new Button(padID, ButtonManager::BUTTON_Z));
	AddInput(new Button(padID, ButtonManager::BUTTON_UP));
	AddInput(new Button(padID, ButtonManager::BUTTON_DOWN));
	AddInput(new Button(padID, ButtonManager::BUTTON_LEFT));
	AddInput(new Button(padID, ButtonManager::BUTTON_RIGHT));
	AddAnalogInputs(new Axis(padID, ButtonManager::STICK_MAIN_LEFT, -1.0f), new Axis(padID, ButtonManager::STICK_MAIN_RIGHT));
	AddAnalogInputs(new Axis(padID, ButtonManager::STICK_MAIN_UP, -1.0f), new Axis(padID, ButtonManager::STICK_MAIN_DOWN));
	AddAnalogInputs(new Axis(padID, ButtonManager::STICK_C_UP, -1.0f), new Axis(padID, ButtonManager::STICK_C_DOWN));
	AddAnalogInputs(new Axis(padID, ButtonManager::STICK_C_LEFT, -1.0f), new Axis(padID, ButtonManager::STICK_C_RIGHT));
	AddAnalogInputs(new Axis(padID, ButtonManager::TRIGGER_L), new Axis(padID, ButtonManager::TRIGGER_L));
	AddAnalogInputs(new Axis(padID, ButtonManager::TRIGGER_R), new Axis(padID, ButtonManager::TRIGGER_R));
}
// Buttons and stuff

std::string Touchscreen::Button::GetName() const
{
	std::ostringstream ss;
	ss << "Button " << (int)index;
	return ss.str();
}

ControlState Touchscreen::Button::GetState() const
{
	return ButtonManager::GetButtonPressed(padID, index);
}
std::string Touchscreen::Axis::GetName() const
{
	std::ostringstream ss;
	ss << "Axis " << (int)index;
	return ss.str();
}

ControlState Touchscreen::Axis::GetState() const
{
	return ButtonManager::GetAxisValue(padID, index) * neg;
}

}
}
