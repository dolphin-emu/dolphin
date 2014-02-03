// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/TextureCacheBase.h"

namespace Null
{

class TextureCache : public TextureCacheBase
{
public:
	TextureCache() {}
	~TextureCache() {}

	void CompileShaders() override {};
	void DeleteShaders() override {};
	void ConvertTexture(TCacheEntryBase* entry, TCacheEntryBase* unconverted, void* palette, TlutFormat format) override {};

	void CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
		PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
		bool isIntensity, bool scaleByHalf) override {};

private:
	struct TCacheEntry : TCacheEntryBase
	{
		TCacheEntry(const TCacheEntryConfig& _config) : TCacheEntryBase(_config) {}
		~TCacheEntry() {}

		void Load(unsigned int width, unsigned int height,
			unsigned int expanded_width, unsigned int level) override {}

		void FromRenderTarget(u8* dst, PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
			bool scaleByHalf, unsigned int cbufid, const float *colmat) override {}

		void CopyRectangleFromTexture(
			const TCacheEntryBase* source,
			const MathUtil::Rectangle<int> &srcrect,
			const MathUtil::Rectangle<int> &dstrect) override {}

		void Bind(unsigned int stage) override {}

		bool Save(const std::string& filename, unsigned int level) override { return false; }
	};

	TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config) override
	{
		return new TCacheEntry(config);
	}
};

} // Null name space
