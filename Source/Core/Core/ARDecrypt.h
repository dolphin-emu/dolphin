// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "Common/CommonTypes.h"
#include "Core/ActionReplay.h"

namespace ActionReplay
{

void DecryptARCode(std::vector<std::string> vCodes, std::vector<AREntry> &ops);

} //namespace
