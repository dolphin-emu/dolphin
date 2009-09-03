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

#ifndef _D3DBASE_H
#define _D3DBASE_H

#include <d3d9.h>

#include <vector>
#include <set>

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#endif

#include "Common.h"

namespace D3D
{
	enum ShaderVersion
	{
		PSNONE = 0,
		PS20 = 2,
		PS30 = 3,
		PS40 = 4,
	};

	HRESULT Init();
	HRESULT Create(int adapter, HWND wnd, bool fullscreen, int resolution, int aa_mode);
	void Close();
	void Shutdown();

	void Reset();
	bool BeginFrame(bool clear=true, u32 color=0, float z=1.0f);
	void EndFrame();
	void SwitchFullscreen(bool fullscreen);
	bool IsFullscreen();
	int GetDisplayWidth();
	int GetDisplayHeight();
	ShaderVersion GetShaderVersion();
	LPDIRECT3DSURFACE9 GetBackBufferSurface();
	const D3DCAPS9 &GetCaps();
	void ShowD3DError(HRESULT err);
	void EnableAlphaToCoverage();

	extern IDirect3DDevice9 *dev;

	// The following are "filtered" versions of the corresponding D3Ddev-> functions.
	void SetTexture(DWORD Stage, IDirect3DBaseTexture9 *pTexture);
	void SetRenderState(D3DRENDERSTATETYPE State, DWORD Value);
	void SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value);
	void SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value);

	struct Resolution
	{
		char name[32];
		int xres;
		int yres;
		std::set<D3DFORMAT> bitdepths;
		std::set<int> refreshes;
	};

	struct AALevel
	{
		AALevel(const char *n, D3DMULTISAMPLE_TYPE m, int q) {strcpy(name, n); ms_setting=m; qual_setting=q;}
	    char name[32];
		D3DMULTISAMPLE_TYPE ms_setting;
		int qual_setting;
	};

	struct Adapter
	{
		D3DADAPTER_IDENTIFIER9 ident;
		std::vector<Resolution> resolutions;
		std::vector<AALevel> aa_levels;
		bool supports_alpha_to_coverage;
	};

	struct Shader
	{
		int Minor;
		int Major;
	};
	
	const Adapter &GetAdapter(int i);
	const Adapter &GetCurAdapter();
	int GetNumAdapters();
}

#endif
