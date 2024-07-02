// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <set>

#include "Common/HookableEvent.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/GraphicsModEditor/EditorTypes.h"

namespace GraphicsModEditor::EditorEvents
{
// Event called when a target or action is selected
using ItemsSelectedEvent = Common::HookableEvent<"ItemsSelected", std::set<SelectableType>>;

// Event called when a change that can be saved occurs
using ChangeOccurredEvent = Common::HookableEvent<"ChangeOccurred">;

// Event called when requesting an asset in the asset browser
using JumpToAssetInBrowserEvent =
    Common::HookableEvent<"JumpToAssetInBrowser", VideoCommon::CustomAssetLibrary::AssetID>;

}  // namespace GraphicsModEditor::EditorEvents
