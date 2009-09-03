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

#ifndef __H_RENDER__
#define __H_RENDER__

#include <vector>
#include <string>

#include "pluginspecs_video.h"
#include "D3DBase.h"

class Renderer
{
public:
	static void Init(SVideoInitialize &_VideoInitialize);
	static void Shutdown();

	static void Initialize();

	// must be called if the window size has changed
	static void ReinitView();

	static void SwapBuffers();

    static void SetColorMask();
	static void SetBlendMode(bool forceUpdate);
	static bool SetScissorRect();

	static float GetTargetScaleX() { return xScale; }
	static float GetTargetScaleY() { return yScale; }
	static float GetTargetWidth() { return m_width; }
	static float GetTargetHeight() { return m_height; }

//	static void SetProjection(float* _pProjection, int constantIndex = -1);
	static u32 AccessEFB(EFBAccessType type, int x, int y);

	// The little status display.
	static void AddMessage(const std::string &message, unsigned int ms);
	static void ProcessMessages();

	static void RenderText(const char *pstr, int left, int top, unsigned int color);

private:
		// screen offset
	static float m_x;
	static float m_y;
	static float m_width;
	static float m_height;
	static float xScale;
	static float yScale;
	static bool m_LastFrameDumped;
	static bool m_AVIDumping;
	static int m_recordWidth;
	static int m_recordHeight;
	static std::vector<LPDIRECT3DBASETEXTURE9> m_Textures;
};

#endif	// __H_RENDER__
