// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "Common/Common.h"
#include "Core/ActionReplay.h"

namespace ActionReplay
{

void DecryptARCode(std::vector<std::string> vCodes, std::vector<AREntry> &ops);

} //namespace
