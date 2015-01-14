// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <SOIL/SOIL.h>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"

#include "VideoCommon/HiresTextures.h"
#include "VideoCommon/VideoConfig.h"

static std::unordered_map<std::string, std::string> s_textureMap;
static bool s_check_native_format;
static bool s_check_new_format;

static const std::string s_format_prefix = "tex1_";


void HiresTexture::Init(const std::string& gameCode)
{
	s_textureMap.clear();
	s_check_native_format = false;
	s_check_new_format = false;

	CFileSearch::XStringVector Directories;

	std::string szDir = StringFromFormat("%s%s", File::GetUserPath(D_HIRESTEXTURES_IDX).c_str(), gameCode.c_str());
	Directories.push_back(szDir);

	for (u32 i = 0; i < Directories.size(); i++)
	{
		File::FSTEntry FST_Temp;
		File::ScanDirectoryTree(Directories[i], FST_Temp);
		for (auto& entry : FST_Temp.children)
		{
			if (entry.isDirectory)
			{
				bool duplicate = false;

				for (auto& Directory : Directories)
				{
					if (Directory == entry.physicalName)
					{
						duplicate = true;
						break;
					}
				}

				if (!duplicate)
					Directories.push_back(entry.physicalName);
			}
		}
	}

	CFileSearch::XStringVector Extensions = {
		"*.png",
		"*.bmp",
		"*.tga",
		"*.dds",
		"*.jpg" // Why not? Could be useful for large photo-like textures
	};

	CFileSearch FileSearch(Extensions, Directories);
	const CFileSearch::XStringVector& rFilenames = FileSearch.GetFileNames();

	const std::string code = StringFromFormat("%s_", gameCode.c_str());
	const std::string code2 = "";

	for (auto& rFilename : rFilenames)
	{
		std::string FileName;
		SplitPath(rFilename, nullptr, &FileName, nullptr);

		if (FileName.substr(0, code.length()) == code)
		{
			s_textureMap[FileName] = rFilename;
			s_check_native_format = true;
		}

		if (FileName.substr(0, s_format_prefix.length()) == s_format_prefix)
		{
			s_textureMap[FileName] = rFilename;
			s_check_new_format = true;
		}
	}
}

std::string HiresTexture::GenBaseName(const u8* texture, size_t texture_size, const u8* tlut, size_t tlut_size, u32 width, u32 height, int format, bool has_mipmaps, bool dump)
{
	if (!dump && s_check_native_format)
	{
		// try to load the old format first
		u64 tex_hash = GetHashHiresTexture(texture, (int)texture_size, g_ActiveConfig.iSafeTextureCache_ColorSamples);
		u64 tlut_hash = tlut_size ? GetHashHiresTexture(tlut, (int)tlut_size, g_ActiveConfig.iSafeTextureCache_ColorSamples) : 0;
		std::string name = StringFromFormat("%s_%08x_%i", SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID.c_str(), (u32)(tex_hash ^ tlut_hash), (u16)format);
		if (s_textureMap.find(name) != s_textureMap.end())
			return name;
	}

	if (dump || s_check_new_format)
	{
		// checking for min/max on paletted textures
		u32 min = 0xffff;
		u32 max = 0;
		switch(tlut_size)
		{
			case 0: break;
			case 16 * 2:
				for (size_t i = 0; i < texture_size; i++)
				{
					min = std::min<u32>(min, texture[i] & 0xf);
					min = std::min<u32>(min, texture[i] >> 4);
					max = std::max<u32>(max, texture[i] & 0xf);
					max = std::max<u32>(max, texture[i] >> 4);
				}
				break;
			case 256 * 2:
				for (size_t i = 0; i < texture_size; i++)
				{
					min = std::min<u32>(min, texture[i]);
					max = std::max<u32>(max, texture[i]);
				}
				break;
			case 16384 * 2:
				for (size_t i = 0; i < texture_size/2; i++)
				{
					min = std::min<u32>(min, Common::swap16(((u16*)texture)[i]) & 0x3fff);
					max = std::max<u32>(max, Common::swap16(((u16*)texture)[i]) & 0x3fff);
				}
				break;
		}
		if (tlut_size > 0)
		{
			tlut_size = 2 * (max + 1 - min);
			tlut += 2 * min;
		}

		u64 tex_hash = GetHashHiresTexture(texture, (int)texture_size);
		u64 tlut_hash = tlut_size ? GetHashHiresTexture(tlut, (int)tlut_size) : 0;

		std::string basename = s_format_prefix + StringFromFormat("%s%dx%d_%016lx", has_mipmaps ? "m_" : "", width, height, tex_hash);
		std::string tlutname = tlut_size ? StringFromFormat("_%016lx", tlut_hash) : "";
		std::string formatname = StringFromFormat("_%d", format);

		// try to match a wildcard template
		if (!dump && s_textureMap.find(basename + "_*" + formatname) != s_textureMap.end())
			return basename + "_*" + formatname;

		// else generate the complete texture
		if (dump || s_textureMap.find(basename + tlutname + formatname) != s_textureMap.end())
			return basename + tlutname + formatname;
	}

	return "";
}

HiresTexture* HiresTexture::Search(const u8* texture, size_t texture_size, const u8* tlut, size_t tlut_size, u32 width, u32 height, int format, bool has_mipmaps)
{
	std::string base_filename = GenBaseName(texture, texture_size, tlut, tlut_size, width, height, format, has_mipmaps);

	HiresTexture* ret = nullptr;
	for (int level = 0;; level++)
	{
		std::string filename = base_filename;
		if (level)
		{
			filename += StringFromFormat("_mip%u", level);
		}

		if (s_textureMap.find(filename) != s_textureMap.end())
		{
			Level l;

			File::IOFile file;
			file.Open(s_textureMap[filename], "rb");
			std::vector<u8> buffer(file.GetSize());
			file.ReadBytes(buffer.data(), file.GetSize());

			int channels;
			l.data = SOIL_load_image_from_memory(buffer.data(), (int)buffer.size(), (int*)&l.width, (int*)&l.height, &channels, SOIL_LOAD_RGBA);
			l.data_size = (size_t)l.width * l.height * 4;

			if (l.data == nullptr)
			{
				ERROR_LOG(VIDEO, "Custom texture %s failed to load", filename.c_str());
				break;
			}

			if (!level)
			{
				if (l.width * height != l.height * width)
					ERROR_LOG(VIDEO, "Invalid custom texture size %dx%d for texture %s. The aspect differs from the native size %dx%d.",
					          l.width, l.height, filename.c_str(), width, height);
				if (l.width % width || l.height % height)
					WARN_LOG(VIDEO, "Invalid custom texture size %dx%d for texture %s. Please use an integer upscaling factor based on the native size %dx%d.",
					         l.width, l.height, filename.c_str(), width, height);
				width = l.width;
				height = l.height;
			}
			else if (width != l.width || height != l.height)
			{
				ERROR_LOG(VIDEO, "Invalid custom texture size %dx%d for texture %s. This mipmap layer _must_ be %dx%d.",
				          l.width, l.height, filename.c_str(), width, height);
				SOIL_free_image_data(l.data);
				break;
			}

			// calculate the size of the next mipmap
			width >>= 1;
			height >>= 1;

			if (!ret)
				ret = new HiresTexture();
			ret->m_levels.push_back(l);
		}
		else
		{
			break;
		}
	}

	return ret;
}

HiresTexture::~HiresTexture()
{
	for (auto& l : m_levels)
	{
		SOIL_free_image_data(l.data);
	}
}

