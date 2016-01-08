// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/VideoCommon.h"

namespace DX12
{

class Television final
{

public:

	Television();

	void Init();
	void Shutdown();

	// Submit video data to be drawn. This will change the current state of the
	// TV. xfbAddr points to YUYV data stored in GameCube/Wii RAM, but the XFB
	// may be virtualized when rendering so the RAM may not actually be read.
	void Submit(u32 xfb_address, u32 stride, u32 width, u32 height);

	// Render the current state of the TV.
	void Render();

private:

	// Properties of last Submit call
	u32 m_current_address;
	u32 m_current_width;
	u32 m_current_height;

	// Used for real XFB mode

	//ID3D11Texture2D* m_yuyvTexture;
	//ID3D11ShaderResourceView* m_yuyvTextureSRV;
	//ID3D11PixelShader* m_pShader;
	//ID3D11SamplerState* m_sampler_state;

};

}
