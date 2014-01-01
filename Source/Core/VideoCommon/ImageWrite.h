// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _IMAGEWRITE_H
#define _IMAGEWRITE_H

#include "Common.h"

bool SaveData(const char* filename, const char* pdata);
bool TextureToPng(u8* data, int row_stride, const std::string filename, int width, int height, bool saveAlpha = true);

#endif  // _IMAGEWRITE_H

