// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "HiresTextures.h"

#include <cstring>
#include <utility>
#include "../../../Externals/SOIL/SOIL.h"
#include "CommonPaths.h"
#include "FileUtil.h"
#include "FileSearch.h"

namespace HiresTextures
{

std::map<std::string, std::string> textureMap;

void Init(const char *gameCode)
{
	CFileSearch::XStringVector Directories;
	Directories.push_back(std::string(FULL_HIRES_TEXTURES_DIR));

	for(u32 i = 0; i < Directories.size(); i++)
	{
		File::FSTEntry FST_Temp;
		File::ScanDirectoryTree(Directories.at(i).c_str(), FST_Temp);
		for(u32 j = 0; j < FST_Temp.children.size(); j++)
		{
			if(FST_Temp.children.at(j).isDirectory)
			{
				bool duplicate = false;
				NormalizeDirSep(&(FST_Temp.children.at(j).physicalName));
				for(u32 k = 0; k < Directories.size(); k++)
				{
					NormalizeDirSep(&Directories.at(k));
					if(strcmp(Directories.at(k).c_str(), FST_Temp.children.at(j).physicalName.c_str()) == 0)
					{
						duplicate = true;
						break;
					}
				}
				if(!duplicate)
					Directories.push_back(FST_Temp.children.at(j).physicalName.c_str());
			}
		}
	}

	CFileSearch::XStringVector Extensions;
	Extensions.push_back("*.png");
	Extensions.push_back("*.bmp");
	Extensions.push_back("*.tga");
	Extensions.push_back("*.dds");

	CFileSearch FileSearch(Extensions, Directories);
	const CFileSearch::XStringVector& rFilenames = FileSearch.GetFileNames();

	if(rFilenames.size() > 0)
	{
		for(u32 i = 0; i < rFilenames.size(); i++)
		{
			std::string FileName;
			SplitPath(rFilenames[i], NULL, &FileName, NULL);

			if(FileName.substr(0, strlen(gameCode)).compare(gameCode) == 0 && textureMap.find(FileName) == textureMap.end())
				textureMap.insert(std::map<std::string, std::string>::value_type(FileName, rFilenames[i]));
		}
	}
}

void Shutdown()
{
	textureMap.clear();
}

PC_TexFormat GetHiresTex(const char *fileName, int *pWidth, int *pHeight, u8 *data)
{
	std::string key(fileName);

	if(textureMap.find(key) == textureMap.end())
		return PC_TEX_FMT_NONE;

	int width;
	int height;
	int channels;
	u8 *temp = SOIL_load_image(textureMap[key].c_str(), &width, &height, &channels, SOIL_LOAD_RGBA);

	if (temp == NULL) {
		ERROR_LOG(VIDEO, "Custom texture %s failed to load", textureMap[key].c_str(), width, height);
		SOIL_free_image_data(temp);
		return PC_TEX_FMT_NONE;
	}

	if (width > 1024 || height > 1024) {
		ERROR_LOG(VIDEO, "Custom texture %s is too large (%ix%i); textures can only be 1024 pixels tall and wide", textureMap[key].c_str(), width, height);
		SOIL_free_image_data(temp);
		return PC_TEX_FMT_NONE;
	}

	memcpy(data, temp, width*height*4);
	*pWidth = width;
	*pHeight = height;
	SOIL_free_image_data(temp);
	return PC_TEX_FMT_RGBA32;
}

}
