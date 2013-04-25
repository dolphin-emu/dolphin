// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Android.h"

namespace ciface
{

namespace Android
{

void Init( std::vector<ControllerInterface::Device*>& devices )
{
	devices.push_back(new Touchscreen());
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
Touchscreen::Touchscreen()
{
	AddInput(new Button(ButtonManager::BUTTON_A));
	AddInput(new Button(ButtonManager::BUTTON_B));
	AddInput(new Button(ButtonManager::BUTTON_START));
}
// Buttons and stuff

std::string Touchscreen::Button::GetName() const
{
	std::ostringstream ss;
	ss << "Button " << (int)m_index;
	return ss.str();
}

ControlState Touchscreen::Button::GetState() const
{
	return ButtonManager::GetButtonPressed(m_index);
}

}
}
