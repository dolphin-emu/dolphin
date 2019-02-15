// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "UICommon/ResourcePack/ResourcePack.h"

namespace ResourcePack
{
bool Init();

bool Add(const std::string& path, int offset = -1);
bool Remove(ResourcePack& pack);
void SetInstalled(const ResourcePack& pack, bool installed);
bool IsInstalled(const ResourcePack& pack);

std::vector<ResourcePack>& GetPacks();

std::vector<ResourcePack*> GetHigherPriorityPacks(ResourcePack& pack);
std::vector<ResourcePack*> GetLowerPriorityPacks(ResourcePack& pack);
}  // namespace ResourcePack
