// Copyright (C) 2003-2008 Dolphin Project.

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

#include <list>

#include "Common.h"
#include "GLUtil.h"

#include "OnScreenDisplay.h"
#include "Render.h"

namespace OSD
{

struct MESSAGE
{
	MESSAGE() {}
	MESSAGE(const char* p, u32 dw) {
		strncpy(str, p, 255);
		str[255] = '\0';
		dwTimeStamp = dw;
	}
	char str[255];
	u32 dwTimeStamp;
};

static std::list<MESSAGE> s_listMsgs;

void AddMessage(const char* pstr, u32 ms)
{
	s_listMsgs.push_back(MESSAGE(pstr, timeGetTime() + ms));
}

void DrawMessages()
{
	GLboolean wasEnabled = glIsEnabled(GL_BLEND);
	if (!wasEnabled)
		glEnable(GL_BLEND);
	
	if (s_listMsgs.size() > 0)
	{
		int left = 25, top = 15;
		std::list<MESSAGE>::iterator it = s_listMsgs.begin();
		while (it != s_listMsgs.end()) 
		{
			int time_left = (int)(it->dwTimeStamp - timeGetTime());
			int alpha = 255;

			if (time_left < 1024)
			{
				alpha = time_left >> 2;
				if (time_left < 0) alpha = 0;
			}

			alpha <<= 24;

			Renderer::RenderText(it->str, left+1, top+1, 0x000000|alpha);
			Renderer::RenderText(it->str, left, top, 0xffff30|alpha);
			top += 15;

			if (time_left <= 0)
				it = s_listMsgs.erase(it);
			else
				++it;
		}
	}

	if (!wasEnabled)
		glDisable(GL_BLEND);
}

}  // namespace