// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

class HiresTexture
{
public:
	using SOILPointer = std::unique_ptr<u8, void(*)(unsigned char*)>;

	static void Init();
	static void Update();
	static void Shutdown();

	static std::shared_ptr<HiresTexture> Search(
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
		Level();

		SOILPointer data;
		size_t data_size = 0;
		u32 width = 0;
		u32 height = 0;
	};
	std::vector<Level> m_levels;

private:
	static std::unique_ptr<HiresTexture> Load(const std::string& base_filename, u32 width, u32 height);
	static void Prefetch();

	static std::string GetTextureDirectory(const std::string& game_id);

	HiresTexture() {}

};
