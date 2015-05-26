// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <xxhash.h>
#include <SOIL/SOIL.h>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/Flag.h"
#include "Common/MemoryUtil.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/Timer.h"

#include "Core/ConfigManager.h"

#include "VideoCommon/HiresTextures.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoConfig.h"

static std::unordered_map<std::string, std::string> s_textureMap;
static std::unordered_map<std::string, std::shared_ptr<HiresTexture>> s_textureCache;
static std::mutex s_textureCacheMutex;
static std::mutex s_textureCacheAquireMutex; // for high priority access
static Common::Flag s_textureCacheAbortLoading;
static bool s_check_native_format;
static bool s_check_new_format;

static std::thread s_prefetcher;

static const std::string s_format_prefix = "tex1_";

void HiresTexture::Init()
{
	s_check_native_format = false;
	s_check_new_format = false;

	Update();
}

void HiresTexture::Shutdown()
{
	if (s_prefetcher.joinable())
	{
		s_textureCacheAbortLoading.Set();
		s_prefetcher.join();
	}

	s_textureMap.clear();
	s_textureCache.clear();
}

void HiresTexture::Update()
{
	if (s_prefetcher.joinable())
	{
		s_textureCacheAbortLoading.Set();
		s_prefetcher.join();
	}

	if (!g_ActiveConfig.bHiresTextures)
	{
		s_textureMap.clear();
		s_textureCache.clear();
		return;
	}

	if (!g_ActiveConfig.bCacheHiresTextures)
	{
		s_textureCache.clear();
	}

	CFileSearch::XStringVector Directories;
	const std::string& gameCode = SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID;

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

	if (g_ActiveConfig.bCacheHiresTextures)
	{
		// remove cached but deleted textures
		auto iter = s_textureCache.begin();
		while (iter != s_textureCache.end())
		{
			if (s_textureMap.find(iter->first) == s_textureMap.end())
			{
				iter = s_textureCache.erase(iter);
			}
			else
			{
				iter++;
			}
		}

		s_textureCacheAbortLoading.Clear();
		s_prefetcher = std::thread(Prefetch);
	}
}

void HiresTexture::Prefetch()
{
	Common::SetCurrentThreadName("Prefetcher");

	size_t size_sum = 0;
	size_t max_mem = MemPhysical() / 2;
	u32 starttime = Common::Timer::GetTimeMs();
	for (const auto& entry : s_textureMap)
	{
		const std::string& base_filename = entry.first;

		if (base_filename.find("_mip") == std::string::npos)
		{
			{
				// try to get this mutex first, so the video thread is allow to get the real mutex faster
				std::unique_lock<std::mutex> lk(s_textureCacheAquireMutex);
			}
			std::unique_lock<std::mutex> lk(s_textureCacheMutex);

			auto iter = s_textureCache.find(base_filename);
			if (iter == s_textureCache.end())
			{
				// unlock while loading a texture. This may result in a race condition where we'll load a texture twice,
				// but it reduces the stuttering a lot. Notice: The loading library _must_ be thread safe now.
				// But bad luck, SOIL isn't, so TODO: remove SOIL usage here and use libpng directly
				// Also TODO: remove s_textureCacheAquireMutex afterwards. It won't be needed as the main mutex will be locked rarely
				//lk.unlock();
				std::shared_ptr<HiresTexture> ptr(Load(base_filename, 0, 0));
				//lk.lock();

				iter = s_textureCache.insert(iter, std::make_pair(base_filename, ptr));
			}
			for (const Level& l : iter->second->m_levels)
			{
				size_sum += l.data_size;
			}
		}

		if (s_textureCacheAbortLoading.IsSet())
		{
			return;
		}

		if (size_sum > max_mem)
		{
			g_Config.bCacheHiresTextures = false;

			OSD::AddMessage(StringFromFormat("Custom Textures prefetching after %.1f MB aborted, not enough RAM available", size_sum / (1024.0 * 1024.0)), 10000);
			return;
		}
	}
	u32 stoptime = Common::Timer::GetTimeMs();
	OSD::AddMessage(StringFromFormat("Custom Textures loaded, %.1f MB in %.1f s", size_sum / (1024.0 * 1024.0), (stoptime - starttime) / 1000.0), 10000);
}

std::string HiresTexture::GenBaseName(const u8* texture, size_t texture_size, const u8* tlut, size_t tlut_size, u32 width, u32 height, int format, bool has_mipmaps, bool dump)
{
	std::string name = "";
	bool convert = false;
	if (!dump && s_check_native_format)
	{
		// try to load the old format first
		u64 tex_hash = GetHashHiresTexture(texture, (int)texture_size, g_ActiveConfig.iSafeTextureCache_ColorSamples);
		u64 tlut_hash = tlut_size ? GetHashHiresTexture(tlut, (int)tlut_size, g_ActiveConfig.iSafeTextureCache_ColorSamples) : 0;
		name = StringFromFormat("%s_%08x_%i", SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID.c_str(), (u32)(tex_hash ^ tlut_hash), (u16)format);
		if (s_textureMap.find(name) != s_textureMap.end())
		{
			if (g_ActiveConfig.bConvertHiresTextures)
				convert = true;
			else
				return name;
		}
	}

	if (dump || s_check_new_format || convert)
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

		u64 tex_hash = XXH64(texture, texture_size, 0);
		u64 tlut_hash = tlut_size ? XXH64(tlut, tlut_size, 0) : 0;

		std::string basename = s_format_prefix + StringFromFormat("%dx%d%s_%016" PRIx64, width, height, has_mipmaps ? "_m" : "", tex_hash);
		std::string tlutname = tlut_size ? StringFromFormat("_%016" PRIx64, tlut_hash) : "";
		std::string formatname = StringFromFormat("_%d", format);
		std::string fullname = basename + tlutname + formatname;

		for (int level = 0; level < 10 && convert; level++)
		{
			std::string oldname = name;
			if (level)
				oldname += StringFromFormat("_mip%d", level);

			// skip not existing levels
			if (s_textureMap.find(oldname) == s_textureMap.end())
				continue;

			for (int i = 0;; i++)
			{
				// for hash collisions, padd with an integer
				std::string newname = fullname;
				if (level)
					newname += StringFromFormat("_mip%d", level);
				if (i)
					newname += StringFromFormat(".%d", i);

				// new texture
				if (s_textureMap.find(newname) == s_textureMap.end())
				{
					std::string src = s_textureMap[oldname];
					size_t postfix = src.find_last_of('.');
					std::string dst = src.substr(0, postfix - oldname.length()) + newname + src.substr(postfix, src.length() - postfix);
					if (File::Rename(src, dst))
					{
						s_textureMap.erase(oldname);
						s_textureMap[newname] = dst;
						s_check_new_format = true;
						OSD::AddMessage(StringFromFormat("Rename custom texture %s to %s", oldname.c_str(), newname.c_str()), 5000);
					}
					else
					{
						ERROR_LOG(VIDEO, "rename failed");
					}
					break;
				}
				else
				{
					// dst fail already exist, compare content
					std::string a, b;
					File::ReadFileToString(s_textureMap[oldname], a);
					File::ReadFileToString(s_textureMap[newname], b);

					if (a == b && a != "")
					{
						// equal, so remove
						if (File::Delete(s_textureMap[oldname]))
						{
							s_textureMap.erase(oldname);
							OSD::AddMessage(StringFromFormat("Delete double old custom texture %s", oldname.c_str()), 5000);
						}
						else
						{
							ERROR_LOG(VIDEO, "delete failed");
						}
						break;
					}

					// else continue in this loop with the next higher padding variable
				}
			}
		}

		// try to match a wildcard template
		if (!dump && s_textureMap.find(basename + "_*" + formatname) != s_textureMap.end())
			return basename + "_*" + formatname;

		// else generate the complete texture
		if (dump || s_textureMap.find(fullname) != s_textureMap.end())
			return fullname;
	}

	return name;
}

std::shared_ptr<HiresTexture> HiresTexture::Search(const u8* texture, size_t texture_size, const u8* tlut, size_t tlut_size, u32 width, u32 height, int format, bool has_mipmaps)
{
	std::string base_filename = GenBaseName(texture, texture_size, tlut, tlut_size, width, height, format, has_mipmaps);

	std::lock_guard<std::mutex> lk2(s_textureCacheAquireMutex);
	std::lock_guard<std::mutex> lk(s_textureCacheMutex);

	auto iter = s_textureCache.find(base_filename);
	if (iter != s_textureCache.end())
	{
		return iter->second;
	}

	std::shared_ptr<HiresTexture> ptr(Load(base_filename, width, height));

	if (ptr && g_ActiveConfig.bCacheHiresTextures)
	{
		s_textureCache[base_filename] = ptr;
	}

	return ptr;
}

HiresTexture* HiresTexture::Load(const std::string& base_filename, u32 width, u32 height)
{
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
				if (width && height && (l.width % width || l.height % height))
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

