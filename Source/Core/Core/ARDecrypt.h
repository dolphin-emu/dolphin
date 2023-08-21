// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/ActionReplay.h"

namespace ActionReplay
{
void DecryptARCode(std::vector<std::string> vCodes, std::vector<AREntry>* ops);

}  // namespace ActionReplay
