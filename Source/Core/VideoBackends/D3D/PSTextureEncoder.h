// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/D3D/TextureEncoder.h"

#include "VideoCommon/TextureCacheBase.h"

struct ID3D11Texture2D;
struct ID3D11RenderTargetView;
struct ID3D11Buffer;
struct ID3D11InputLayout;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11ClassLinkage;
struct ID3D11ClassInstance;
struct ID3D11BlendState;
struct ID3D11DepthStencilState;
struct ID3D11RasterizerState;
struct ID3D11SamplerState;

namespace DX11
{

class PSTextureEncoder : public TextureEncoder
{
public:
	PSTextureEncoder();

	void Init();
	void Shutdown();
	void Encode(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
	            PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
	            bool isIntensity, bool scaleByHalf);

private:
	bool m_ready;

	ID3D11Texture2D* m_out;
	ID3D11RenderTargetView* m_outRTV;
	ID3D11Texture2D* m_outStage;
	ID3D11Buffer* m_encodeParams;

	ID3D11PixelShader* SetStaticShader(unsigned int dstFormat,
		PEControl::PixelFormat srcFormat, bool isIntensity, bool scaleByHalf);

	typedef unsigned int ComboKey; // Key for a shader combination

	ComboKey MakeComboKey(unsigned int dstFormat,
		PEControl::PixelFormat srcFormat, bool isIntensity, bool scaleByHalf)
	{
		return (dstFormat << 4) | (static_cast<int>(srcFormat) << 2) | (isIntensity ? (1<<1) : 0)
			| (scaleByHalf ? (1<<0) : 0);
	}

	typedef std::map<ComboKey, ID3D11PixelShader*> ComboMap;

	ComboMap m_staticShaders;
};

}
