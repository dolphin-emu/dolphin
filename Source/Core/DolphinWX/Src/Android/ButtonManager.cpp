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

#include <vector>
#include "GLInterface.h"
#include "Android/TextureLoader.h"
#include "Android/ButtonManager.h"

extern void DrawButton(GLuint tex, float *coords);

namespace ButtonManager
{
	std::vector<Button*> m_buttons;
	std::map<std::string, InputDevice*> m_controllers;
	// XXX: This needs to not be here so we can load the locations from file
	// This will allow customizable button locations in the future
	// These are the OpenGL on screen coordinates
	float m_coords[][8] =	{ // X, Y, X, EY, EX, EY, EX, Y
				{0.75f, -1.0f, 0.75f, -0.75f, 1.0f, -0.75f, 1.0f, -1.0f}, // A
				{0.50f, -1.0f, 0.50f, -0.75f, 0.75f, -0.75f, 0.75f, -1.0f}, // B
				{-0.10f, -1.0f, -0.10f, -0.80f, 0.10f, -0.80f, 0.10f, -1.0f}, // Start
				};
	const char *configStrings[] = {	"InputA",
					"InputB",
					"InputStart",
					"InputX",
					"InputY",
					"InputZ",
					"DPadUp",
					"DPadDown",
					"DPadLeft",
					"DPadRight",
					"MainUp",
					"MainDown",
					"MainLeft",
					"MainRight",
					"CStickUp",
					"CStickDown",
					"CStickLeft",
					"CStickRight",
					"InputL",
					"InputR" };
	const int configStringNum = 20;
	
	void AddBind(std::string dev, sBind *bind)
	{
		auto it = m_controllers.find(dev);
		if (it != m_controllers.end())
		{
			it->second->AddBind(bind);
			return;
		}
		m_controllers[dev] = new InputDevice(dev);
		m_controllers[dev]->AddBind(bind);
	}

	void Init()
	{
		// Initialize our touchscreen buttons
		m_buttons.push_back(new Button("ButtonA.png", BUTTON_A, m_coords[0]));
		m_buttons.push_back(new Button("ButtonB.png", BUTTON_B, m_coords[1]));
		m_buttons.push_back(new Button("ButtonStart.png", BUTTON_START, m_coords[2]));

		// Init our controller bindings
		IniFile ini;
		ini.Load(File::GetUserPath(D_CONFIG_IDX) + std::string("Dolphin.ini"));
		for (int a = 0; a < configStringNum; ++a)
		{
			BindType type;
			int bindnum;
			char dev[128];
			bool hasbind = false;
			char modifier = 0;
			std::string value;
			ini.Get("Android", configStrings[a], &value, "None");
			if (value == "None")
				continue;
			if (std::string::npos != value.find("Axis"))
			{
				hasbind = true;
				type = BIND_AXIS;
				sscanf(value.c_str(), "Device '%[^\']'-Axis %d%c", dev, &bindnum, &modifier);	
			}
			else if (std::string::npos != value.find("Button"))  
			{
				hasbind = true;
				type = BIND_BUTTON;
				sscanf(value.c_str(), "Device '%[^\']'-Button %d", dev, &bindnum);
			}
			if (hasbind)
				AddBind(std::string(dev), new sBind((ButtonType)a, type, bindnum, modifier == '-' ? -1.0f : 1.0f));
		}

	}
	bool GetButtonPressed(ButtonType button)
	{
		bool pressed = false;
		for (auto it = m_buttons.begin(); it != m_buttons.end(); ++it)
			if ((*it)->GetButtonType() == button)
				pressed = (*it)->Pressed();

		for (auto it = m_controllers.begin(); it != m_controllers.end(); ++it)
			pressed |= it->second->ButtonValue(button);

		return pressed;
	}
	float GetAxisValue(ButtonType axis)
	{
		auto it = m_controllers.begin();
		if (it == m_controllers.end())
			return 0.0f;
		return it->second->AxisValue(axis); 
	}
	void TouchEvent(int action, float x, float y)
	{
		// Actions
		// 0 is press
		// 1 is let go
		// 2 is move
		for (auto it = m_buttons.begin(); it != m_buttons.end(); ++it)
		{
			float *coords = (*it)->GetCoords();
			if (	x >= coords[0] &&
				x <= coords[4] &&
				y >= coords[1] &&
				y <= coords[3])
			{
				if (action == 0)
					(*it)->SetState(BUTTON_PRESSED);
				if (action == 1)
					(*it)->SetState(BUTTON_RELEASED);
				if (action == 2)
					; // XXX: Be used later for analog stick
			}
		}
	}

	void GamepadEvent(std::string dev, int button, int action)
	{
		auto it = m_controllers.find(dev);
		if (it != m_controllers.end())
		{
			it->second->PressEvent(button, action);
			return;
		}
		m_controllers[dev] = new InputDevice(dev);
		m_controllers[dev]->PressEvent(button, action);
	}
	void GamepadAxisEvent(std::string dev, int axis, float value)
	{
		auto it = m_controllers.find(dev);
		if (it != m_controllers.end())
		{
			it->second->AxisEvent(axis, value);
			return;
		}
		m_controllers[dev] = new InputDevice(dev);
		m_controllers[dev]->AxisEvent(axis, value);
	}
	void Shutdown()
	{
		for(auto it = m_buttons.begin(); it != m_buttons.end(); ++it)
			delete *it;
		for (auto it = m_controllers.begin(); it != m_controllers.end(); ++it)
			delete it->second;
	}

	void DrawButtons()
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		for(auto it = m_buttons.begin(); it != m_buttons.end(); ++it)
			DrawButton((*it)->GetTexture(), (*it)->GetCoords());	

		glDisable(GL_BLEND);
	}
	
	// InputDevice
	void InputDevice::PressEvent(int button, int action)
	{
		m_buttons[button] = action == 0 ? true : false;
	}
	void InputDevice::AxisEvent(int axis, float value)
	{
		m_axises[axis] = value;
	}
	bool InputDevice::ButtonValue(ButtonType button)
	{
		auto it = m_binds.find(button);
		if (it == m_binds.end())
			return false;
		if (it->second->m_bindtype == BIND_BUTTON)
			return m_buttons[it->second->m_bind];
		else
			return AxisValue(button);
	}
	float InputDevice::AxisValue(ButtonType axis)
	{
		auto it = m_binds.find(axis);
		if (it == m_binds.end())
			return 0.0f;
		if (it->second->m_bindtype == BIND_BUTTON)
			return ButtonValue(axis);
		else		
			return m_axises[it->second->m_bind] * it->second->m_neg;
	}
	
}
