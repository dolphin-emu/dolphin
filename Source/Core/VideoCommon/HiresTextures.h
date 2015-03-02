// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <unordered_map>
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoCommon.h"

class HiresTexture
{
public:
	static void Init(const std::string& gameCode);

	static HiresTexture* Search(
		const u8* texture, size_t texture_size,
		const u8* tlut, size_t tlut_size,
		u32 width, u32 height,
		int format, bool has_mipmaps
	);

	static std::string GenBaseName(
		const u8* texture, size_t texture_size,
		const u8* tlut, size_t tlut_size,
		u32 width, u32 height,
		int format, bool has_mipmaps,
		bool dump = false
	);

	~HiresTexture();

	struct Level
	{
		std::unique_ptr<u8[]> data;
		size_t data_size;
		u32 width, height;

		// Work around VS 2013 bugs.
		// TODO: Get rid of this when we bump up the minimum requirement
		// to VS 2015.
		Level() = default;
		Level(Level&& other)
		{
			data = std::move(other.data);
			data_size = other.data_size;
			width = other.width;
			height = other.height;
		}
	};
	std::vector<Level> m_levels;

private:
	HiresTexture() {}

};
