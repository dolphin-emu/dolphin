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

#include "VideoCommon/HiresTextures.h"

namespace HiresTextures
{

static std::map<std::string, std::string> textureMap;

void Init(const std::string& gameCode)
{
	textureMap.clear();

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

	if (rFilenames.size() > 0)
	{
		for (auto& rFilename : rFilenames)
		{
			std::string FileName;
			SplitPath(rFilename, nullptr, &FileName, nullptr);

			if (FileName.substr(0, code.length()).compare(code) == 0 && textureMap.find(FileName) == textureMap.end())
				textureMap.insert(std::map<std::string, std::string>::value_type(FileName, rFilename));
		}
	}
}

bool HiresTexExists(const std::string& filename)
{
	return textureMap.find(filename) != textureMap.end();
}

PC_TexFormat GetHiresTex(const std::string& filename, unsigned int* pWidth, unsigned int* pHeight, unsigned int* required_size, int texformat, unsigned int data_size, u8* data)
{
	if (textureMap.find(filename) == textureMap.end())
		return PC_TEX_FMT_NONE;

	int width;
	int height;
	int channels;

	File::IOFile file;
	file.Open(textureMap[filename], "rb");
	std::vector<u8> buffer(file.GetSize());
	file.ReadBytes(buffer.data(), file.GetSize());

	u8* temp = SOIL_load_image_from_memory(buffer.data(), (int)buffer.size(), &width, &height, &channels, SOIL_LOAD_RGBA);

	if (temp == nullptr)
	{
		ERROR_LOG(VIDEO, "Custom texture %s failed to load", textureMap[filename].c_str());
		return PC_TEX_FMT_NONE;
	}

	*pWidth = width;
	*pHeight = height;

	//int offset = 0;
	PC_TexFormat returnTex = PC_TEX_FMT_NONE;

	// TODO(neobrain): This function currently has no way to enforce RGBA32
	// output, which however is required on some configurations to function
	// properly. As a lazy workaround, we hence disable the optimized code
	// path for now.
#if 0
	switch (texformat)
	{
	case GX_TF_I4:
	case GX_TF_I8:
	case GX_TF_IA4:
	case GX_TF_IA8:
		*required_size = width * height * 8;
		if (data_size < *required_size)
			goto cleanup;

		for (int i = 0; i < width * height * 4; i += 4)
		{
			// Rather than use a luminosity function, just use the most intense color for luminance
			// TODO(neobrain): Isn't this kind of.. stupid?
			data[offset++] = *std::max_element(temp+i, temp+i+3);
			data[offset++] = temp[i+3];
		}
		returnTex = PC_TEX_FMT_IA8;
		break;
	default:
		*required_size = width * height * 4;
		if (data_size < *required_size)
			goto cleanup;

		memcpy(data, temp, width * height * 4);
		returnTex = PC_TEX_FMT_RGBA32;
		break;
	}
#else
	*required_size = width * height * 4;
	if (data_size < *required_size)
		goto cleanup;

	memcpy(data, temp, width * height * 4);
	returnTex = PC_TEX_FMT_RGBA32;
#endif

	INFO_LOG(VIDEO, "Loading custom texture from %s", textureMap[filename].c_str());
cleanup:
	SOIL_free_image_data(temp);
	return returnTex;
}

}
