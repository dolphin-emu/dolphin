// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/D3D/D3DTexture.h"
#include "VideoCommon/TextureCacheBase.h"

namespace DX11
{

class TextureCache : public ::TextureCache
{
public:
	TextureCache();
	~TextureCache();

private:
	struct TCacheEntry : TCacheEntryBase
	{
		D3DTexture2D *const texture;

		D3D11_USAGE usage;

		TCacheEntry(D3DTexture2D *_tex) : texture(_tex) {}
		~TCacheEntry();

		void Load(unsigned int width, unsigned int height,
			unsigned int expanded_width, unsigned int levels) override;

		void FromRenderTarget(u32 dstAddr, unsigned int dstFormat,
			PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
			bool isIntensity, bool scaleByHalf, unsigned int cbufid,
			const float *colmat) override;

		void Bind(unsigned int stage) override;
		bool Save(const std::string& filename, unsigned int level) override;
	};

	TCacheEntryBase* CreateTexture(unsigned int width, unsigned int height,
		unsigned int expanded_width, unsigned int tex_levels, PC_TexFormat pcfmt) override;

	TCacheEntryBase* CreateRenderTargetTexture(unsigned int scaled_tex_w, unsigned int scaled_tex_h) override;
	u64 EncodeToRamFromTexture(u32 address, void* source_texture, u32 SourceW, u32 SourceH, bool bFromZBuffer, bool bIsIntensityFmt, u32 copyfmt, int bScaleByHalf, const EFBRectangle& source) {return 0;};

	void CompileShaders() override { }
	void DeleteShaders() override { }
};

}
