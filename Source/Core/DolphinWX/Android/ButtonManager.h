// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>

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
		std::map<ButtonType, bool> _buttons;
		std::map<ButtonType, float> _axises;

		// Key is padID and ButtonType
		std::map<std::pair<int, ButtonType>, sBind*> _inputbinds;
	public:
		InputDevice(std::string dev)
			: _dev(dev) {}
		~InputDevice()
		{
			for (const auto& bind : _inputbinds)
				delete bind.second;
			_inputbinds.clear();
		}
		void AddBind(sBind* bind) { _inputbinds[std::make_pair(bind->_padID, bind->_buttontype)] = bind; }
		void PressEvent(int button, int action);
		void AxisEvent(int axis, float value);
		bool ButtonValue(int padID, ButtonType button);
		float AxisValue(int padID, ButtonType axis);
	};

	void Init();
	bool GetButtonPressed(int padID, ButtonType button);
	float GetAxisValue(int padID, ButtonType axis);
	void GamepadEvent(std::string dev, int button, int action);
	void GamepadAxisEvent(std::string dev, int axis, float value);
	void Shutdown();
}
