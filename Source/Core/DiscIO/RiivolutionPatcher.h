// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include "DiscIO/RiivolutionParser.h"

namespace DiscIO
{
struct FSTBuilderNode;
}

namespace DiscIO::Riivolution
{
void ApplyPatchesToFiles(const std::vector<Patch>& patches,
                         std::vector<DiscIO::FSTBuilderNode>* fst,
                         DiscIO::FSTBuilderNode* dol_node);
void ApplyPatchesToMemory(const std::vector<Patch>& patches);
}  // namespace DiscIO::Riivolution
