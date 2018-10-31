// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "Common/CommonTypes.h"

namespace Common
{
bool LoadPNG(const std::vector<u8>& input, std::vector<u8>* data_out, u32* width_out,
             u32* height_out);
}
