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
		const ButtonType m_buttontype;
		const BindType m_bindtype;
		const int m_bind;
		const float m_neg;
		sBind(ButtonType buttontype, BindType bindtype, int bind, float neg)
			: m_buttontype(buttontype), m_bindtype(bindtype), m_bind(bind), m_neg(neg)
		{}
	};


	class InputDevice
	{
	private:
		std::string m_dev;
		std::map<int, bool> m_buttons;
		std::map<int, float> m_axises;
		std::map<ButtonType, sBind*> m_binds;
	public:
		InputDevice(std::string dev)
		{
			m_dev = dev;
		}
		~InputDevice()
		{
			for (auto it = m_binds.begin(); it != m_binds.end(); ++it)
				delete it->second;
		}
		void AddBind(sBind *bind) { m_binds[bind->m_buttontype] = bind; }
		void PressEvent(int button, int action);
		void AxisEvent(int axis, float value);
		bool ButtonValue(ButtonType button);
		float AxisValue(ButtonType axis);
	};

	void Init();
	bool GetButtonPressed(int padID, ButtonType button);
	float GetAxisValue(int padID, ButtonType axis);
	void TouchEvent(int padID, int button, int action);
	void TouchAxisEvent(int padID, int axis, float value);
	void GamepadEvent(std::string dev, int button, int action);
	void GamepadAxisEvent(std::string dev, int axis, float value);
	void Shutdown();
}
