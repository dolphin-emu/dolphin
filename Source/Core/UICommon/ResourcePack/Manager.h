// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "UICommon/ResourcePack/ResourcePack.h"

namespace ResourcePack
{
bool Init();

ResourcePack* Add(const std::string& path, int offset = -1);
bool Remove(ResourcePack& pack);
void SetInstalled(const ResourcePack& pack, bool installed);
bool IsInstalled(const ResourcePack& pack);

std::vector<ResourcePack>& GetPacks();

std::vector<ResourcePack*> GetHigherPriorityPacks(const ResourcePack& pack);
std::vector<ResourcePack*> GetLowerPriorityPacks(const ResourcePack& pack);
}  // namespace ResourcePack
