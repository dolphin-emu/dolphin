// Copyright 2016 Dolphin Emulator Project
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
	STICK_MAIN = 10,
	STICK_MAIN_UP = 11,
	STICK_MAIN_DOWN = 12,
	STICK_MAIN_LEFT = 13,
	STICK_MAIN_RIGHT = 14,
	STICK_C = 15,
	STICK_C_UP = 16,
	STICK_C_DOWN = 17,
	STICK_C_LEFT = 18,
	STICK_C_RIGHT = 19,
	TRIGGER_L = 20,
	TRIGGER_R = 21,
	// Wiimote
	WIIMOTE_BUTTON_A = 100,
	WIIMOTE_BUTTON_B = 101,
	WIIMOTE_BUTTON_MINUS = 102,
	WIIMOTE_BUTTON_PLUS = 103,
	WIIMOTE_BUTTON_HOME = 104,
	WIIMOTE_BUTTON_1 = 105,
	WIIMOTE_BUTTON_2 = 106,
	WIIMOTE_UP = 107,
	WIIMOTE_DOWN = 108,
	WIIMOTE_LEFT = 109,
	WIIMOTE_RIGHT = 110,
	WIIMOTE_IR = 111,
	WIIMOTE_IR_UP = 112,
	WIIMOTE_IR_DOWN = 113,
	WIIMOTE_IR_LEFT = 114,
	WIIMOTE_IR_RIGHT = 115,
	WIIMOTE_IR_FORWARD = 116,
	WIIMOTE_IR_BACKWARD = 117,
	WIIMOTE_IR_HIDE = 118,
	WIIMOTE_SWING = 119,
	WIIMOTE_SWING_UP = 120,
	WIIMOTE_SWING_DOWN = 121,
	WIIMOTE_SWING_LEFT = 122,
	WIIMOTE_SWING_RIGHT = 123,
	WIIMOTE_SWING_FORWARD = 124,
	WIIMOTE_SWING_BACKWARD = 125,
	WIIMOTE_TILT = 126,
	WIIMOTE_TILT_FORWARD = 127,
	WIIMOTE_TILT_BACKWARD = 128,
	WIIMOTE_TILT_LEFT = 129,
	WIIMOTE_TILT_RIGHT = 130,
	WIIMOTE_TILT_MODIFIER = 131,
	WIIMOTE_SHAKE_X = 132,
	WIIMOTE_SHAKE_Y = 133,
	WIIMOTE_SHAKE_Z = 134,
	//Nunchuk
	NUNCHUK_BUTTON_C = 200,
	NUNCHUK_BUTTON_Z = 201,
	NUNCHUK_STICK = 202,
	NUNCHUK_STICK_UP = 203,
	NUNCHUK_STICK_DOWN = 204,
	NUNCHUK_STICK_LEFT = 205,
	NUNCHUK_STICK_RIGHT = 206,
	NUNCHUK_SWING = 207,
	NUNCHUK_SWING_UP = 208,
	NUNCHUK_SWING_DOWN = 209,
	NUNCHUK_SWING_LEFT = 210,
	NUNCHUK_SWING_RIGHT = 211,
	NUNCHUK_SWING_FORWARD = 212,
	NUNCHUK_SWING_BACKWARD = 213,
	NUNCHUK_TILT = 214,
	NUNCHUK_TILT_FORWARD = 215,
	NUNCHUK_TILT_BACKWARD = 216,
	NUNCHUK_TILT_LEFT = 217,
	NUNCHUK_TILT_RIGHT = 218,
	NUNCHUK_TILT_MODIFIER = 219,
	NUNCHUK_SHAKE_X = 220,
	NUNCHUK_SHAKE_Y = 221,
	NUNCHUK_SHAKE_Z = 222,
	//Classic
	CLASSIC_BUTTON_A = 300,
	CLASSIC_BUTTON_B = 301,
	CLASSIC_BUTTON_X = 302,
	CLASSIC_BUTTON_Y = 303,
	CLASSIC_BUTTON_MINUS = 304,
	CLASSIC_BUTTON_PLUS = 305,
	CLASSIC_BUTTON_HOME = 306,
	CLASSIC_BUTTON_ZL = 307,
	CLASSIC_BUTTON_ZR = 308,
	CLASSIC_DPAD_UP = 309,
	CLASSIC_DPAD_DOWN = 310,
	CLASSIC_DPAD_LEFT = 311,
	CLASSIC_DPAD_RIGHT = 312,
	CLASSIC_STICK_LEFT = 313,
	CLASSIC_STICK_LEFT_UP = 314,
	CLASSIC_STICK_LEFT_DOWN = 315,
	CLASSIC_STICK_LEFT_LEFT = 316,
	CLASSIC_STICK_LEFT_RIGHT = 317,
	CLASSIC_STICK_RIGHT = 318,
	CLASSIC_STICK_RIGHT_UP = 319,
	CLASSIC_STICK_RIGHT_DOWN = 320,
	CLASSIC_STICK_RIGHT_LEFT = 321,
	CLASSIC_STICK_RIGHT_RIGHT = 322,
	CLASSIC_TRIGGER_L = 323,
	CLASSIC_TRIGGER_R = 324,
	//Guitar
	GUITAR_BUTTON_MINUS = 400,
	GUITAR_BUTTON_PLUS = 401,
	GUITAR_FRET_GREEN = 402,
	GUITAR_FRET_RED = 403,
	GUITAR_FRET_YELLOW = 404,
	GUITAR_FRET_BLUE = 405,
	GUITAR_FRET_ORANGE = 406,
	GUITAR_STRUM_UP = 407,
	GUITAR_STRUM_DOWN = 408,
	GUITAR_STICK = 409,
	GUITAR_STICK_UP = 410,
	GUITAR_STICK_DOWN = 411,
	GUITAR_STICK_LEFT = 412,
	GUITAR_STICK_RIGHT = 413,
	GUITAR_WHAMMY_BAR = 414,
	//Drums
	DRUMS_BUTTON_MINUS = 500,
	DRUMS_BUTTON_PLUS = 501,
	DRUMS_PAD_RED = 502,
	DRUMS_PAD_YELLOW = 503,
	DRUMS_PAD_BLUE = 504,
	DRUMS_PAD_GREEN	 = 505,
	DRUMS_PAD_ORANGE = 506,
	DRUMS_PAD_BASS = 507,
	DRUMS_STICK = 508,
	DRUMS_STICK_UP = 509,
	DRUMS_STICK_DOWN = 510,
	DRUMS_STICK_LEFT = 511,
	DRUMS_STICK_RIGHT = 512,
	//Turntable
	TURNTABLE_BUTTON_GREEN_LEFT = 600,
	TURNTABLE_BUTTON_RED_LEFT = 601,
	TURNTABLE_BUTTON_BLUE_LEFT = 602,
	TURNTABLE_BUTTON_GREEN_RIGHT = 603,
	TURNTABLE_BUTTON_RED_RIGHT = 604,
	TURNTABLE_BUTTON_BLUE_RIGHT = 605,
	TURNTABLE_BUTTON_MINUS = 606,
	TURNTABLE_BUTTON_PLUS = 607,
	TURNTABLE_BUTTON_HOME = 608,
	TURNTABLE_BUTTON_EUPHORIA = 609,
	TURNTABLE_TABLE_LEFT = 610,
	TURNTABLE_TABLE_LEFT_LEFT = 611,
	TURNTABLE_TABLE_LEFT_RIGHT = 612,
	TURNTABLE_TABLE_RIGHT = 613,
	TURNTABLE_TABLE_RIGHT_LEFT = 614,
	TURNTABLE_TABLE_RIGHT_RIGHT = 615,
	TURNTABLE_STICK = 616,
	TURNTABLE_STICK_UP = 617,
	TURNTABLE_STICK_DOWN = 618,
	TURNTABLE_STICK_LEFT = 619,
	TURNTABLE_STICK_RIGHT = 620,
	TURNTABLE_EFFECT_DIAL = 621,
	TURNTABLE_CROSSFADE = 622,
	TURNTABLE_CROSSFADE_LEFT = 623,
	TURNTABLE_CROSSFADE_RIGHT = 624,
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
}// namespace ButtonManager
