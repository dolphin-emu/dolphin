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

	// XXX: This needs to not be here so we can load the locations from file
	// This will allow customizable button locations in the future
	// These are the OpenGL on screen coordinates
	float m_coords[][8] =	{ // X, Y, X, EY, EX, EY, EX, Y
				{0.75f, -1.0f, 0.75f, -0.75f, 1.0f, -0.75f, 1.0f, -1.0f}, // A
				{0.50f, -1.0f, 0.50f, -0.75f, 0.75f, -0.75f, 0.75f, -1.0f}, // B
				{-0.10f, -1.0f, -0.10f, -0.80f, 0.10f, -0.80f, 0.10f, -1.0f}, // Start
				};
	
	void Init()
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		
		// Initialize our buttons
		m_buttons.push_back(new Button("ButtonA.png", BUTTON_A, m_coords[0]));
		m_buttons.push_back(new Button("ButtonB.png", BUTTON_B, m_coords[1]));
		m_buttons.push_back(new Button("ButtonStart.png", BUTTON_START, m_coords[2]));
	}
	bool GetButtonPressed(ButtonType button)
	{
		for (auto it = m_buttons.begin(); it != m_buttons.end(); ++it)
			if ((*it)->GetButtonType() == button)
				return (*it)->Pressed();
		return false;
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
	void Shutdown()
	{
		for(auto it = m_buttons.begin(); it != m_buttons.end(); ++it)
			delete *it;
	}

	void DrawButtons()
	{
		for(auto it = m_buttons.begin(); it != m_buttons.end(); ++it)
			DrawButton((*it)->GetTexture(), (*it)->GetCoords());	
	}

}
