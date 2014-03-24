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
		const int padID;
		const ButtonType buttontype;
		const BindType bindtype;
		const int bind;
		const float neg;
		sBind(int _padID, ButtonType _buttontype, BindType _bindtype, int _bind, float _neg)
			: padID(_padID), buttontype(_buttontype), bindtype(_bindtype), bind(_bind), neg(_neg)
		{}
	};


	class InputDevice
	{
	private:
		const std::string dev;
		std::map<ButtonType, bool> buttons;
		std::map<ButtonType, float> axises;
		std::map<ButtonType, sBind*> binds;
		std::map<int, sBind*> inputbinds;
	public:
		InputDevice(std::string _dev)
			: dev(_dev) {}
		~InputDevice()
		{
			for (const auto& bind : binds)
				delete bind.second;
		}
		void AddBind(sBind *bind) { binds[bind->buttontype] = bind; inputbinds[bind->bind] = bind; }
		void PressEvent(int button, int action);
		void AxisEvent(int axis, float value);
		bool ButtonValue(int padID, ButtonType button);
		float AxisValue(int padID, ButtonType axis);
	};

	void Init();
	bool GetButtonPressed(int padID, ButtonType button);
	float GetAxisValue(int padID, ButtonType axis);
	void TouchEvent(int padID, ButtonType button, int action);
	void TouchAxisEvent(int padID, ButtonType axis, float value);
	void GamepadEvent(std::string dev, int button, int action);
	void GamepadAxisEvent(std::string dev, int axis, float value);
	void Shutdown();
}
