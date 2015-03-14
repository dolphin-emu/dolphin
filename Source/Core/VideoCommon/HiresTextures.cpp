// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <png.h>
#include <string>
#include <utility>
#include <xxhash.h>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"

#include "VideoCommon/HiresTextures.h"
#include "VideoCommon/OnScreenDisplay.h"
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

static void PNGErrorFn(png_structp png_ptr, png_const_charp error_message)
{
	ERROR_LOG(VIDEO, "libpng error %s", error_message);
	longjmp(png_ptr->jmpbuf, 1);
}

static void PNGWarnFn(png_structp png_ptr, png_const_charp error_message)
{
	WARN_LOG(VIDEO, "libpng warning %s", error_message);
}

static bool ReadPNG(File::IOFile *file, std::unique_ptr<u8[]> *output, size_t *out_data_size, u32 *out_width, u32 *out_height)
{
	// Set up libpng reading.
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, PNGErrorFn, PNGWarnFn);
	if (!png_ptr)
		return false;
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		return false;
	}
	png_infop end_info = png_create_info_struct(png_ptr);
	if (!end_info)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		return false;
	}
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return false;
	}

	// We read using a FILE*; this could be changed.
	png_init_io(png_ptr, file->GetHandle());
	png_set_sig_bytes(png_ptr, 4);

	// Process PNG header, etc.
	png_read_info(png_ptr, info_ptr);
	png_uint_32 width, height;
	int bit_depth, color_type, interlace_type, compression_type, filter_method;
	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, &compression_type, &filter_method);

	// Force RGB (8 or 16-bit).
	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png_ptr);
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png_ptr);
	if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

	// Force 8-bit RGB.
	if (bit_depth == 16)
		png_set_strip_16(png_ptr);

	// Force alpha channel (combined with the above, 8-bit RGBA).
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);
	else if ((color_type & PNG_COLOR_MASK_ALPHA) == 0)
		png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);

	png_read_update_info(png_ptr, info_ptr);

	*output = std::unique_ptr<u8[]>(new u8[width * height * 4]);
	*out_width = width;
	*out_height = height;
	*out_data_size = width * height * 4;

	std::vector<u8*> row_pointers(height);
	u8* row_pointer = output->get();
	for (unsigned i = 0; i < height; ++i)
	{
		row_pointers[i] = row_pointer;
		row_pointer += width * 4;
	}

	png_read_image(png_ptr, row_pointers.data());
	png_read_end(png_ptr, end_info);
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	return true;
}

static bool ReadDDS(File::IOFile *file, std::unique_ptr<u8[]> *output, size_t *out_data_size, u32 *out_width, u32 *out_height)
{
	ERROR_LOG(VIDEO, "DDS decoding not yet implemented");
	return false;
}

static bool ReadJPEG(File::IOFile *file, std::unique_ptr<u8[]> *output, size_t *out_data_size, u32 *out_width, u32 *out_height)
{
	ERROR_LOG(VIDEO, "JPEG decoding not yet implemented");
	return false;
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
			if (!file.Open(s_textureMap[filename], "rb"))
			{
				ERROR_LOG(VIDEO, "Custom texture %s failed to load", filename.c_str());
				break;
			}
			u8 signature_bytes[4];
			if (!file.ReadBytes(signature_bytes, 4))
			{
				ERROR_LOG(VIDEO, "Custom texture %s failed to load", filename.c_str());
				break;
			}

			if (png_sig_cmp(signature_bytes, 0, 4) == 0)
			{
				// Found a PNG file.
				if (!ReadPNG(&file, &l.data, &l.data_size, &l.width, &l.height))
					break;
			}
			else if (memcmp(signature_bytes, "DDS ", 4) == 0)
			{
				// Found a DDS file.
				if (!ReadDDS(&file, &l.data, &l.data_size, &l.width, &l.height))
					break;
			}
			else if (memcmp(signature_bytes, "\xFF\xD8xFF\xE0", 4) == 0)
			{
				// Found a JPEG file.
				if (!ReadJPEG(&file, &l.data, &l.data_size, &l.width, &l.height))
					break;
			}
			else
			{
				ERROR_LOG(VIDEO, "Unknown texture format for custom texture %s", filename.c_str());
				break;
			}


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
				break;
			}

			// calculate the size of the next mipmap
			width >>= 1;
			height >>= 1;

			if (!ret)
				ret = new HiresTexture();
			ret->m_levels.emplace_back(std::move(l));
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
}

