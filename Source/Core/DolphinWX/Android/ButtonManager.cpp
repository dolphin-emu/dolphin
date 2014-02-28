// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <unordered_map>

#include "DolphinWX/Android/ButtonManager.h"
#include "DolphinWX/GLInterface/GLInterface.h"

namespace ButtonManager
{
	// Pair key is padID, BUTTONTYPE
	std::map<std::pair<int, int>, Button*> m_buttons;
	std::map<std::pair<int, int>, Axis*> m_axises;
	std::unordered_map<std::string, InputDevice*> m_controllers;
	const char* configStrings[] = {
		"InputA",
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
		"InputR"
	};
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
		for (int a = 0; a < 4; ++a)
		{
			m_buttons[std::make_pair(a, BUTTON_A)] = new Button();
			m_buttons[std::make_pair(a, BUTTON_B)] = new Button();
			m_buttons[std::make_pair(a, BUTTON_START)] = new Button();
			m_buttons[std::make_pair(a, BUTTON_X)] = new Button();
			m_buttons[std::make_pair(a, BUTTON_Y)] = new Button();
			m_buttons[std::make_pair(a, BUTTON_Z)] = new Button();
			m_buttons[std::make_pair(a, BUTTON_UP)] = new Button();
			m_buttons[std::make_pair(a, BUTTON_DOWN)] = new Button();
			m_buttons[std::make_pair(a, BUTTON_LEFT)] = new Button();
			m_buttons[std::make_pair(a, BUTTON_RIGHT)] = new Button();

			m_axises[std::make_pair(a, STICK_MAIN_UP)] = new Axis();
			m_axises[std::make_pair(a, STICK_MAIN_DOWN)] = new Axis();
			m_axises[std::make_pair(a, STICK_MAIN_LEFT)] = new Axis();
			m_axises[std::make_pair(a, STICK_MAIN_RIGHT)] = new Axis();
			m_axises[std::make_pair(a, STICK_C_UP)] = new Axis();
			m_axises[std::make_pair(a, STICK_C_DOWN)] = new Axis();
			m_axises[std::make_pair(a, STICK_C_LEFT)] = new Axis();
			m_axises[std::make_pair(a, STICK_C_RIGHT)] = new Axis();
			m_buttons[std::make_pair(a, TRIGGER_L)] = new Button();
			m_buttons[std::make_pair(a, TRIGGER_R)] = new Button();
		}
		// Init our controller bindings
		IniFile ini;
		ini.Load(File::GetUserPath(D_CONFIG_IDX) + std::string("Dolphin.ini"));
		for (int a = 0; a < configStringNum; ++a)
		{
			for (int padID = 0; padID < 4; ++padID)
			{
				std::ostringstream config;
				config << configStrings[a] << "_" << padID;
				BindType type;
				int bindnum;
				char dev[128];
				bool hasbind = false;
				char modifier = 0;
				std::string value;
				ini.Get("Android", config.str(), &value, "None");
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
					AddBind(std::string(dev), new sBind(padID, (ButtonType)a, type, bindnum, modifier == '-' ? -1.0f : 1.0f));
			}
		}

	}
	bool GetButtonPressed(int padID, ButtonType button)
	{
		bool pressed = m_buttons[std::make_pair(padID, button)]->Pressed();

		for (const auto& ctrl : m_controllers)
			pressed |= ctrl.second->ButtonValue(padID, button);

		return pressed;
	}
	float GetAxisValue(int padID, ButtonType axis)
	{
		float value = m_axises[std::make_pair(padID, axis)]->AxisValue();
		auto it = m_controllers.begin();
		if (it == m_controllers.end())
			return value;
		return value != 0.0f ? value : it->second->AxisValue(padID, axis);
	}
	void TouchEvent(int padID, ButtonType button, int action)
	{
		m_buttons[std::make_pair(padID, button)]->SetState(action ? BUTTON_PRESSED : BUTTON_RELEASED);
	}
	void TouchAxisEvent(int padID, ButtonType axis, float value)
	{
		m_axises[std::make_pair(padID, axis)]->SetValue(value);
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
		for (const auto& button : m_buttons)
			delete button.second;
		for (const auto& controller : m_controllers)
			delete controller.second;
		m_controllers.clear();
		m_buttons.clear();
	}

	// InputDevice
	void InputDevice::PressEvent(int button, int action)
	{
		if (_inputbinds.find(button) == _inputbinds.end())
			return;
		_buttons[_inputbinds[button]->_buttontype] = action == 0 ? true : false;
	}
	void InputDevice::AxisEvent(int axis, float value)
	{
		if (_inputbinds.find(axis) == _inputbinds.end())
			return;
		_axises[_inputbinds[axis]->_buttontype] = value;
	}
	bool InputDevice::ButtonValue(int padID, ButtonType button)
	{
		auto it = _binds.find(button);
		if (it == _binds.end())
			return false;
		if (it->second->_padID != padID)
			return false;
		if (it->second->_bindtype == BIND_BUTTON)
			return _buttons[it->second->_buttontype];
		else
			return AxisValue(padID, button);
	}
	float InputDevice::AxisValue(int padID, ButtonType axis)
	{
		auto it = _binds.find(axis);
		if (it == _binds.end())
			return 0.0f;
		if (it->second->_padID != padID)
			return 0.0f;
		if (it->second->_bindtype == BIND_BUTTON)
			return ButtonValue(padID, axis);
		else
			return _axises[it->second->_buttontype] * it->second->_neg;
	}

}
