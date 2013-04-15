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

#include <string>
#include "CommonPaths.h"
#include "Android/TextureLoader.h"

namespace ButtonManager
{
	enum ButtonType
	{
		BUTTON_A = 0,
		BUTTON_B,
		BUTTON_START,
	};
	enum ButtonState
	{
		BUTTON_RELEASED = 0,
		BUTTON_PRESSED = 1
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
		void SetState(ButtonState state) { m_state = state; }
		bool Pressed() { return m_state == BUTTON_PRESSED; }
		ButtonType GetButtonType() { return m_button; }
		GLuint GetTexture() { return m_tex; }
		float *GetCoords() { return m_coords; }
			
		~Button() { glDeleteTextures(1, &m_tex); }
	};
	void Init();
	void DrawButtons();
	bool GetButtonPressed(ButtonType button);
	void TouchEvent(int action, float x, float y);
	void Shutdown();
}
