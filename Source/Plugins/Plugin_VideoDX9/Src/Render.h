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

#include <Cg/cg.h>

extern CGcontext g_cgcontext;
extern CGprofile g_cgvProf, g_cgfProf;

class Renderer
{
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
	const static int MaxTextureStages = 9;
	const static int MaxRenderStates = 210;
	const static DWORD MaxTextureTypes = 33;
	const static DWORD MaxSamplerSize = 13;
	const static DWORD MaxSamplerTypes = 15;
	static std::vector<LPDIRECT3DBASETEXTURE9> m_Textures;
	static DWORD m_RenderStates[MaxRenderStates+46];
	static DWORD m_TextureStageStates[MaxTextureStages][MaxTextureTypes];
	static DWORD m_SamplerStates[MaxSamplerSize][MaxSamplerTypes];
	static DWORD m_FVF;

public:
	static void Init(SVideoInitialize &_VideoInitialize);
	static void Shutdown();

	static void Initialize();

	// must be called if the window size has changed
	static void ReinitView();

	static void SwapBuffers();

	static float GetXScale() { return xScale; }
	static float GetYScale() { return yScale; }

	static void SetScissorRect();
//	static void SetViewport(float* _Viewport);
//	static void SetProjection(float* _pProjection, int constantIndex = -1);

	// The little status display.
	static void AddMessage(const std::string &message, unsigned int ms);
	static void ProcessMessages();
	static void RenderText(const std::string &text, int left, int top, unsigned int color);

	// The following are "filtered" versions of the corresponding D3Ddev-> functions.
	static void SetTexture(DWORD Stage, IDirect3DBaseTexture9 *pTexture);
	static void SetFVF(DWORD FVF);
	static void SetRenderState(D3DRENDERSTATETYPE State, DWORD Value);
	static void SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value);
	static void SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value);
};

#endif	// __H_RENDER__
