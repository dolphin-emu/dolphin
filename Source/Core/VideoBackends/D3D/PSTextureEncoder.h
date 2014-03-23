// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/D3D/TextureEncoder.h"

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
	size_t Encode(u8* dst, unsigned int dstFormat,
	              PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
	              bool isIntensity, bool scaleByHalf);

private:

	bool m_ready;

	ID3D11Texture2D* m_out;
	ID3D11RenderTargetView* m_outRTV;
	ID3D11Texture2D* m_outStage;
	ID3D11Buffer* m_encodeParams;
	ID3D11Buffer* m_quad;
	ID3D11VertexShader* m_vShader;
	ID3D11InputLayout* m_quadLayout;
	ID3D11BlendState* m_efbEncodeBlendState;
	ID3D11DepthStencilState* m_efbEncodeDepthState;
	ID3D11RasterizerState* m_efbEncodeRastState;
	ID3D11SamplerState* m_efbSampler;

	// Stuff only used in static-linking mode (SM4.0-compatible)

	bool InitStaticMode();
	bool SetStaticShader(unsigned int dstFormat,
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

	// Stuff only used for dynamic-linking mode (SM5.0+, available as soon as
	// Microsoft fixes their bloody HLSL compiler)

	bool InitDynamicMode();
	bool SetDynamicShader(unsigned int dstFormat,
		PEControl::PixelFormat srcFormat, bool isIntensity, bool scaleByHalf);

	ID3D11PixelShader* m_dynamicShader;
	ID3D11ClassLinkage* m_classLinkage;

	// Interface slots
	UINT m_fetchSlot;
	UINT m_scaledFetchSlot;
	UINT m_intensitySlot;
	UINT m_generatorSlot;

	// Class instances
	// Fetch: 0 is RGB, 1 is RGBA, 2 is RGB565, 3 is Z
	ID3D11ClassInstance* m_fetchClass[4];
	// ScaledFetch: 0 is off, 1 is on
	ID3D11ClassInstance* m_scaledFetchClass[2];
	// Intensity: 0 is off, 1 is on
	ID3D11ClassInstance* m_intensityClass[2];
	// Generator: one for each dst format, 16 total
	ID3D11ClassInstance* m_generatorClass[16];

	std::vector<ID3D11ClassInstance*> m_linkageArray;

};

}
