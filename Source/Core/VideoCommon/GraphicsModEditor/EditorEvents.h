// Copyright 2028 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <set>

#include "Common/HookableEvent.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/GraphicsModEditor/EditorTypes.h"

namespace GraphicsModEditor
{
struct EditorEvents
{
  // Event called when a target or action is selected
  Common::HookableEvent<std::set<SelectableType>> items_selected_event;

  // Event called when a change that can be saved occurs
  Common::HookableEvent<> change_occurred_event;

  // Event called when an asset should reload, the event provides the asset id
  // for use in the loader
  // Will also trigger the above ChangeOccurred event
  Common::HookableEvent<VideoCommon::CustomAssetLibrary::AssetID> asset_reload_event;

  // Event called when requesting an asset in the asset browser
  Common::HookableEvent<VideoCommon::CustomAssetLibrary::AssetID> jump_to_asset_in_browser_event;
};

EditorEvents& GetEditorEvents();

}  // namespace GraphicsModEditor
