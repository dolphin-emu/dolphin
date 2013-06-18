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

namespace ButtonManager
{
	enum ButtonType
	{
		BUTTON_A = 0,
		BUTTON_B,
		BUTTON_START,
		BUTTON_X,
		BUTTON_Y,
		BUTTON_Z,
		BUTTON_UP,
		BUTTON_DOWN,
		BUTTON_LEFT,
		BUTTON_RIGHT,
		STICK_MAIN_UP,
		STICK_MAIN_DOWN,
		STICK_MAIN_LEFT,
		STICK_MAIN_RIGHT,
		STICK_C_UP,
		STICK_C_DOWN,
		STICK_C_LEFT,
		STICK_C_RIGHT,
		TRIGGER_L,
		TRIGGER_R
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
		GLuint m_tex;
		ButtonType m_button;
		ButtonState m_state;
		float m_coords[8];
	public:
		Button(std::string filename, ButtonType button, float *coords)
		{
			m_tex = LoadPNG((std::string(DOLPHIN_DATA_DIR "/") + filename).c_str());
			m_button = button;
			memcpy(m_coords, coords, sizeof(float) * 8);
			m_state = BUTTON_RELEASED;
		}
		Button(ButtonType button)
		{
			m_button = button;
			m_state = BUTTON_RELEASED;
		}
		void SetState(ButtonState state) { m_state = state; }
		bool Pressed() { return m_state == BUTTON_PRESSED; }
		ButtonType GetButtonType() { return m_button; }
		GLuint GetTexture() { return m_tex; }
		float *GetCoords() { return m_coords; }
			
		~Button() { if(m_tex) glDeleteTextures(1, &m_tex); }
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
	void DrawButtons();
	bool GetButtonPressed(ButtonType button);
	float GetAxisValue(ButtonType axis);
	void TouchEvent(int action, float x, float y);
	void GamepadEvent(std::string dev, int button, int action);
	void GamepadAxisEvent(std::string dev, int axis, float value);
	void Shutdown();
}
