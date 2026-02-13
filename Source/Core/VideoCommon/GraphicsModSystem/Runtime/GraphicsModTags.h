// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <string>
#include <vector>

#include "VideoCommon/GraphicsModSystem/Config/GraphicsModTag.h"
#include "VideoCommon/GraphicsModSystem/Types.h"

namespace GraphicsModSystem::Runtime
{
std::span<const GraphicsModSystem::Config::GraphicsModTag> GetDrawCallTagConfig();
std::span<const DrawCallTag> GetDrawCallTags();

std::span<const GraphicsModSystem::Config::GraphicsModTag> GetTextureTagConfig();
std::span<const TextureTag> GetTextureTags();

std::vector<std::string> GetNamesFromDrawCallTags(GraphicsModSystem::DrawCallTags drawcall_tags);
GraphicsModSystem::DrawCallTags GetDrawCallTagsFromNames(std::span<const std::string> names);

std::vector<std::string> GetNamesFromTextureTags(GraphicsModSystem::TextureTags texture_tags);
GraphicsModSystem::TextureTags GetTextureTagsFromNames(std::span<const std::string> names);
}  // namespace GraphicsModSystem::Runtime
