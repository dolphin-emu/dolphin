// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>

#include "Common/HookableEvent.h"

#include "VideoCommon/Assets/CustomAssetCache.h"
#include "VideoCommon/Resources/TextureDataResource.h"

namespace VideoCommon
{
class CustomResourceManager
{
public:
  void Initialize();
  void Shutdown();

  void Reset();

  // Request that an asset be reloaded
  void MarkAssetDirty(const CustomAssetLibrary::AssetID& asset_id);

  void XFBTriggered();

  TextureDataResource*
  GetTextureDataFromAsset(const CustomAssetLibrary::AssetID& asset_id,
                          std::shared_ptr<VideoCommon::CustomAssetLibrary> library);

private:
  Resource::ResourceContext
  CreateResourceContext(const CustomAssetLibrary::AssetID& asset_id,
                        const std::shared_ptr<VideoCommon::CustomAssetLibrary>& library);
  void ProcessResource(Resource* resource);
  void ProcessResourceState(Resource* resource);
  CustomAssetCache m_asset_cache;

  std::map<CustomAssetLibrary::AssetID, std::unique_ptr<TextureDataResource>>
      m_texture_data_resources;

  Common::EventHook m_xfb_event;
};
}  // namespace VideoCommon
