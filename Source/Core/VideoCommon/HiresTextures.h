// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <map>
#include "VideoCommon.h"
#include "TextureDecoder.h"

namespace HiresTextures
{
void Init(const char *gameCode);
bool HiresTexExists(const char *filename);
PC_TexFormat GetHiresTex(const char *fileName, unsigned int *pWidth, unsigned int *pHeight, unsigned int *required_size, int texformat, unsigned int data_size, u8 *data);

};
