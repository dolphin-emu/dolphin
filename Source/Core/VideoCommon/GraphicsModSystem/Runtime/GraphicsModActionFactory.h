// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string_view>

#include <nlohmann/json_fwd.hpp>

#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"

namespace GraphicsModActionFactory
{
std::unique_ptr<GraphicsModAction> Create(std::string_view name, const nlohmann::json& json_data,
                                          std::shared_ptr<VideoCommon::CustomAssetLibrary> library);
}
