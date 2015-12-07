// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>

namespace ButtonManager
{
	enum ButtonType
	{
		// GC
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
		// Wiimote
		WIIMOTE_BUTTON_A = 22,
		WIIMOTE_BUTTON_B = 23,
		WIIMOTE_BUTTON_MINUS = 24,
		WIIMOTE_BUTTON_PLUS = 25,
		WIIMOTE_BUTTON_HOME = 26,
		WIIMOTE_BUTTON_1 = 27,
		WIIMOTE_BUTTON_2 = 28,
		WIIMOTE_UP = 29,
		WIIMOTE_DOWN = 30,
		WIIMOTE_LEFT = 31,
		WIIMOTE_RIGHT = 32,
		WIIMOTE_IR_UP = 33,
		WIIMOTE_IR_DOWN = 34,
		WIIMOTE_IR_LEFT = 35,
		WIIMOTE_IR_RIGHT = 36,
		WIIMOTE_IR_FORWARD = 37,
		WIIMOTE_IR_BACKWARD = 38,
		WIIMOTE_IR_HIDE = 39,
		WIIMOTE_SWING_UP = 40,
		WIIMOTE_SWING_DOWN = 41,
		WIIMOTE_SWING_LEFT = 42,
		WIIMOTE_SWING_RIGHT = 43,
		WIIMOTE_SWING_FORWARD = 44,
		WIIMOTE_SWING_BACKWARD = 45,
		WIIMOTE_TILT_FORWARD = 46,
		WIIMOTE_TILT_BACKWARD = 47,
		WIIMOTE_TILT_LEFT = 48,
		WIIMOTE_TILT_RIGHT = 49,
		WIIMOTE_SHAKE_X = 51,
		WIIMOTE_SHAKE_Y = 52,
		WIIMOTE_SHAKE_Z = 53,
		//Nunchuk
		NUNCHUK_BUTTON_C = 54,
		NUNCHUK_BUTTON_Z = 55,
		NUNCHUK_STICK = 56,
		NUNCHUK_STICK_UP = 57,
		NUNCHUK_STICK_DOWN = 58,
		NUNCHUK_STICK_LEFT = 59,
		NUNCHUK_STICK_RIGHT = 60,
		NUNCHUK_SWING_UP = 61,
		NUNCHUK_SWING_DOWN = 62,
		NUNCHUK_SWING_LEFT = 63,
		NUNCHUK_SWING_RIGHT = 64,
		NUNCHUK_SWING_FORWARD = 65,
		NUNCHUK_SWING_BACKWARD = 66,
		NUNCHUK_TILT_FORWARD = 67,
		NUNCHUK_TILT_BACKWARD = 68,
		NUNCHUK_TILT_LEFT = 69,
		NUNCHUK_TILT_RIGHT = 70,
		NUNCHUK_SHAKE_X = 72,
		NUNCHUK_SHAKE_Y = 73,
		NUNCHUK_SHAKE_Z = 74,
		//Classic
		CLASSIC_BUTTON_A = 75,
		CLASSIC_BUTTON_B = 76,
		CLASSIC_BUTTON_X = 77,
		CLASSIC_BUTTON_Y = 78,
		CLASSIC_BUTTON_MINUS = 79,
		CLASSIC_BUTTON_PLUS = 80,
		CLASSIC_BUTTON_HOME = 81,
		CLASSIC_BUTTON_ZL = 82,
		CLASSIC_BUTTON_ZR = 83,
		CLASSIC_DPAD_UP = 84,
		CLASSIC_DPAD_DOWN = 85,
		CLASSIC_DPAD_LEFT = 86,
		CLASSIC_DPAD_RIGHT = 87,
		CLASSIC_STICK_LEFT = 88,
		CLASSIC_STICK_LEFT_UP = 89,
		CLASSIC_STICK_LEFT_DOWN = 90,
		CLASSIC_STICK_LEFT_LEFT = 91,
		CLASSIC_STICK_LEFT_RIGHT = 92,
		CLASSIC_STICK_RIGHT = 93,
		CLASSIC_STICK_RIGHT_UP = 94,
		CLASSIC_STICK_RIGHT_DOWN = 95,
		CLASSIC_STICK_RIGHT_LEFT = 96,
		CLASSIC_STICK_RIGHT_RIGHT = 97,
		CLASSIC_TRIGGER_L = 98,
		CLASSIC_TRIGGER_R = 99,
		//Guitar
		GUITAR_BUTTON_MINUS = 100,
		GUITAR_BUTTON_PLUS = 101,
		GUITAR_FRET_GREEN = 102,
		GUITAR_FRET_RED = 103,
		GUITAR_FRET_YELLOW = 104,
		GUITAR_FRET_BLUE = 105,
		GUITAR_FRET_ORANGE = 106,
		GUITAR_STRUM_UP = 107,
		GUITAR_STRUM_DOWN = 108,
		GUITAR_STICK = 109,
		GUITAR_STICK_UP = 110,
		GUITAR_STICK_DOWN = 111,
		GUITAR_STICK_LEFT = 112,
		GUITAR_STICK_RIGHT = 113,
		GUITAR_WHAMMY_BAR = 114,
		//Drums
		DRUMS_BUTTON_MINUS = 115,
		DRUMS_BUTTON_PLUS = 116,
		DRUMS_PAD_RED = 117,
		DRUMS_PAD_YELLOW = 118,
		DRUMS_PAD_BLUE = 119,
		DRUMS_PAD_GREEN	 = 120,
		DRUMS_PAD_ORANGE = 121,
		DRUMS_PAD_BASS = 122,
		DRUMS_STICK = 123,
		DRUMS_STICK_UP = 124,
		DRUMS_STICK_DOWN = 125,
		DRUMS_STICK_LEFT = 126,
		DRUMS_STICK_RIGHT = 127,
		//Turntable
		TURNTABLE_BUTTON_GREEN_LEFT = 128,
		TURNTABLE_BUTTON_RED_LEFT = 129,
		TURNTABLE_BUTTON_BLUE_LEFT = 130,
		TURNTABLE_BUTTON_GREEN_RIGHT = 131,
		TURNTABLE_BUTTON_RED_RIGHT = 132,
		TURNTABLE_BUTTON_BLUE_RIGHT = 133,
		TURNTABLE_BUTTON_MINUS = 134,
		TURNTABLE_BUTTON_PLUS = 135,
		TURNTABLE_BUTTON_HOME = 136,
		TURNTABLE_BUTTON_EUPHORIA = 137,
		TURNTABLE_TABLE_LEFT_LEFT = 138,
		TURNTABLE_TABLE_LEFT_RIGHT = 139,
		TURNTABLE_TABLE_RIGHT_LEFT = 140,
		TURNTABLE_TABLE_RIGHT_RIGHT = 141,
		TURNTABLE_STICK = 142,
		TURNTABLE_STICK_UP = 143,
		TURNTABLE_STICK_DOWN = 144,
		TURNTABLE_STICK_LEFT = 145,
		TURNTABLE_STICK_RIGHT = 146,
		TURNTABLE_EFFECT_DIAL = 147,
		TURNTABLE_CROSSFADE_LEFT = 148,
		TURNTABLE_CROSSFADE_RIGHT = 149,
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
		bool PressEvent(int button, int action);
		void AxisEvent(int axis, float value);
		bool ButtonValue(int padID, ButtonType button);
		float AxisValue(int padID, ButtonType axis);
	};

	void Init();
	bool GetButtonPressed(int padID, ButtonType button);
	float GetAxisValue(int padID, ButtonType axis);
	bool GamepadEvent(const std::string& dev, int button, int action);
	void GamepadAxisEvent(const std::string& dev, int axis, float value);
	void Shutdown();
}
