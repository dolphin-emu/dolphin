// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "Common/CommonTypes.h"

bool SaveData(const std::string& filename, const char* pdata);
bool TextureToPng(u8* data, int row_stride, const std::string& filename, int width, int height, bool saveAlpha = true);
