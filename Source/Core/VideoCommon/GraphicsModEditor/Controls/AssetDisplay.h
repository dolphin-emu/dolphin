// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <string_view>

#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/GraphicsModEditor/EditorState.h"
#include "VideoCommon/GraphicsModEditor/EditorTypes.h"

namespace GraphicsModEditor::Controls
{
// Displays an asset that can be overwritten by dragging/dropping or selecting from an asset browser
// Returns true if asset id changed
bool AssetDisplay(std::string_view popup_name, EditorState* state,
                  VideoCommon::CustomAssetLibrary::AssetID* asset_id_chosen,
                  AssetDataType* asset_type_chosen,
                  std::span<const AssetDataType> asset_types_allowed);
bool AssetDisplay(std::string_view popup_name, EditorState* state,
                  VideoCommon::CustomAssetLibrary::AssetID* asset_id_chosen,
                  AssetDataType asset_type_allowed);
}  // namespace GraphicsModEditor::Controls
