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

#pragma once

#include <string>
#include <map>
#include "CommonPaths.h"
#include "Android/TextureLoader.h"
#include "VideoBackendBase.h"

namespace ButtonManager
{
	enum ButtonType
	{
		BUTTON_A = 0,
		BUTTON_B = 1,
		BUTTON_START = 2,
		BUTTON_X = 3,
		BUTTON_Y = 4,
		BUTTON_Z = 5,
		BUTTON_UP = 6,
		BUTTON_DOWN = 7,
		BUTTON_LEFT = 8,
		BUTTON_RIGHT = 9,
		STICK_MAIN = 10, /* Used on Java Side */
		STICK_MAIN_UP = 11,
		STICK_MAIN_DOWN = 12,
		STICK_MAIN_LEFT = 13,
		STICK_MAIN_RIGHT = 14,
		STICK_C = 15, /* Used on Java Side */
		STICK_C_UP = 16,
		STICK_C_DOWN = 17,
		STICK_C_LEFT = 18,
		STICK_C_RIGHT = 19,
		TRIGGER_L = 20,
		TRIGGER_R = 21,
	};
	enum ButtonState
	{
		BUTTON_RELEASED = 0,
		BUTTON_PRESSED = 1
	};
	enum BindType
	{
		BIND_BUTTON = 0,
		BIND_AXIS
	};
	class Button
	{
	private:
		ButtonState m_state;
	public:
		Button() : m_state(BUTTON_RELEASED) {}
		void SetState(ButtonState state) { m_state = state; }
		bool Pressed() { return m_state == BUTTON_PRESSED; }
			
		~Button() {}
	};
	class Axis
	{
	private:
		float m_value;
	public:
		Axis() : m_value(0.0f) {}
		void SetValue(float value) { m_value = value; }
		float AxisValue() { return m_value; }

		~Axis() {}
	};

	struct sBind
	{
		const int _padID;
		const ButtonType _buttontype;
		const BindType _bindtype;
		const int _bind;
		const float _neg;
		sBind(int padID, ButtonType buttontype, BindType bindtype, int bind, float neg)
			: _padID(padID), _buttontype(buttontype), _bindtype(bindtype), _bind(bind), _neg(neg)
		{}
	};


	class InputDevice
	{
	private:
		const std::string _dev;
		std::map<int, bool> _buttons;
		std::map<int, float> _axises;
		std::map<ButtonType, sBind*> _binds;
	public:
		InputDevice(std::string dev)
			: _dev(dev) {}
		~InputDevice()
		{
			for (auto it = _binds.begin(); it != _binds.end(); ++it)
				delete it->second;
		}
		void AddBind(sBind *bind) { _binds[bind->_buttontype] = bind; }
		void PressEvent(ButtonType button, int action);
		void AxisEvent(ButtonType axis, float value);
		bool ButtonValue(int padID, ButtonType button);
		float AxisValue(int padID, ButtonType axis);
	};

	void Init();
	bool GetButtonPressed(int padID, ButtonType button);
	float GetAxisValue(int padID, ButtonType axis);
	void TouchEvent(int padID, ButtonType button, int action);
	void TouchAxisEvent(int padID, ButtonType axis, float value);
	void GamepadEvent(std::string dev, ButtonType button, int action);
	void GamepadAxisEvent(std::string dev, ButtonType axis, float value);
	void Shutdown();
}
