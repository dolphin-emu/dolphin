// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoCommon.h"

namespace HiresTextures
{
void Init(const std::string& gameCode);
bool HiresTexExists(const std::string& filename);
PC_TexFormat GetHiresTex(const std::string& fileName, unsigned int* pWidth, unsigned int* pHeight, unsigned int* required_size, int texformat, unsigned int data_size, u8* data);

};
