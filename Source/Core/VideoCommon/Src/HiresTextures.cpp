// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "HiresTextures.h"

#include <cstring>
#include <utility>
#include <algorithm>
#include <SOIL/SOIL.h>
#include "CommonPaths.h"
#include "FileUtil.h"
#include "FileSearch.h"
#include "StringUtil.h"

namespace HiresTextures
{

std::map<std::string, std::string> textureMap;

void Init(const char *gameCode)
{
	textureMap.clear();

	CFileSearch::XStringVector Directories;
	//Directories.push_back(File::GetUserPath(D_HIRESTEXTURES_IDX));
	char szDir[MAX_PATH];
	sprintf(szDir, "%s%s", File::GetUserPath(D_HIRESTEXTURES_IDX).c_str(), gameCode);
	Directories.push_back(std::string(szDir));
	

	for (u32 i = 0; i < Directories.size(); i++)
	{
		File::FSTEntry FST_Temp;
		File::ScanDirectoryTree(Directories[i], FST_Temp);
		for (u32 j = 0; j < FST_Temp.children.size(); j++)
		{
			if (FST_Temp.children.at(j).isDirectory)
			{
				bool duplicate = false;
				for (u32 k = 0; k < Directories.size(); k++)
				{
					if (strcmp(Directories[k].c_str(), FST_Temp.children.at(j).physicalName.c_str()) == 0)
					{
						duplicate = true;
						break;
					}
				}
				if (!duplicate)
					Directories.push_back(FST_Temp.children.at(j).physicalName.c_str());
			}
		}
	}

	CFileSearch::XStringVector Extensions;
	Extensions.push_back("*.png");
	Extensions.push_back("*.bmp");
	Extensions.push_back("*.tga");
	Extensions.push_back("*.dds");
	Extensions.push_back("*.jpg"); // Why not? Could be useful for large photo-like textures

	CFileSearch FileSearch(Extensions, Directories);
	const CFileSearch::XStringVector& rFilenames = FileSearch.GetFileNames();
	char code[MAX_PATH];
	sprintf(code, "%s_", gameCode);

	if (rFilenames.size() > 0)
	{
		for (u32 i = 0; i < rFilenames.size(); i++)
		{
			std::string FileName;
			SplitPath(rFilenames[i], NULL, &FileName, NULL);

			if (FileName.substr(0, strlen(code)).compare(code) == 0 && textureMap.find(FileName) == textureMap.end())
				textureMap.insert(std::map<std::string, std::string>::value_type(FileName, rFilenames[i]));
		}
	}
}

bool HiresTexExists(const char* filename)
{
	std::string key(filename);
	return textureMap.find(key) != textureMap.end();
}

PC_TexFormat GetHiresTex(const char *fileName, unsigned int *pWidth, unsigned int *pHeight, unsigned int *required_size, int texformat, unsigned int data_size, u8 *data)
{
	std::string key(fileName);
	if (textureMap.find(key) == textureMap.end())
		return PC_TEX_FMT_NONE;

	int width;
	int height;
	int channels;

	u8 *temp = SOIL_load_image(textureMap[key].c_str(), &width, &height, &channels, SOIL_LOAD_RGBA);
	if (temp == NULL)
	{
		ERROR_LOG(VIDEO, "Custom texture %s failed to load", textureMap[key].c_str());
		return PC_TEX_FMT_NONE;
	}

	*pWidth = width;
	*pHeight = height;

	int offset = 0;
	PC_TexFormat returnTex = PC_TEX_FMT_NONE;

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

	INFO_LOG(VIDEO, "Loading custom texture from %s", textureMap[key].c_str());
cleanup:
	SOIL_free_image_data(temp);
	return returnTex;
}

}
