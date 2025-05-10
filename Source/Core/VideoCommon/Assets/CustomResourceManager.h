// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/HookableEvent.h"

#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/Assets/CustomAssetLoader.h"
#include "VideoCommon/Assets/CustomTextureData.h"

namespace VideoCommon
{
class GameTextureAsset;

// The resource manager manages custom resources (textures, shaders, meshes)
// called assets.  These assets are loaded using a priority system,
// where assets requested more often gets loaded first.  This system
// also tracks memory usage and if memory usage goes over a calculated limit,
// then assets will be purged with older assets being targeted first.
class CustomResourceManager
{
public:
  void Initialize();
  void Shutdown();

  void Reset();

  // Request that an asset be reloaded
  void ReloadAsset(const CustomAssetLibrary::AssetID& asset_id);

  void XFBTriggered();

  CustomTextureData*
  GetTextureDataFromAsset(const CustomAssetLibrary::AssetID& asset_id,
                          std::shared_ptr<VideoCommon::CustomAssetLibrary> library);

private:
  // A generic interface to describe an assets' type
  // and load state
  struct AssetData
  {
    std::unique_ptr<CustomAsset> asset;
    CustomAssetLibrary::TimeType load_request_time = {};
    std::set<std::size_t> asset_owners;

    enum class AssetType
    {
      TextureData
    };
    AssetType type;

    enum class LoadStatus
    {
      PendingReload,
      LoadFinished,
      LoadFinalized,
      DependenciesChanged,
      Unloaded
    };
    LoadStatus load_status = LoadStatus::PendingReload;
    bool has_errors = false;
  };

  // A structure to represent some raw texture data
  // (this data hasn't hit the GPU yet, used for custom textures)
  struct InternalTextureDataResource
  {
    AssetData* asset_data = nullptr;
    VideoCommon::GameTextureAsset* asset = nullptr;
    std::shared_ptr<TextureData> texture_data;
  };

  void LoadTextureDataAsset(const CustomAssetLibrary::AssetID& asset_id,
                            std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
                            InternalTextureDataResource* internal_texture_data);

  void ProcessAssetsToReload();
  void ProcessLoadedAssets();
  void RemoveAssetsUntilBelowMemoryLimit();

  template <typename T>
  T* CreateAsset(const CustomAssetLibrary::AssetID& asset_id, AssetData::AssetType asset_type,
                 std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
  {
    const auto [it, added] =
        m_asset_id_to_handle.try_emplace(asset_id, m_asset_handle_to_data.size());

    if (added)
    {
      AssetData asset_data;
      asset_data.asset = std::make_unique<T>(library, asset_id, it->second);
      asset_data.type = asset_type;
      asset_data.has_errors = false;
      asset_data.load_request_time = {};

      m_asset_handle_to_data.insert_or_assign(it->second, std::move(asset_data));
    }
    auto& asset_data_from_handle = m_asset_handle_to_data[it->second];
    asset_data_from_handle.load_status = AssetData::LoadStatus::PendingReload;

    return static_cast<T*>(asset_data_from_handle.asset.get());
  }

  // This class defines a cache of least or most recently used assets
  // It is used to keep track of the least used asset,
  // to know what assets we can remove when memory is low.
  // It is also used to find the most recent asset,
  // to know which assets take priority in terms of loading
  class LeastRecentlyUsedCache
  {
  public:
    const std::list<CustomAsset*>& Elements() const { return m_asset_cache; }

    void MarkAssetAsRecent(u64 asset_handle, CustomAsset* asset)
    {
      RemoveAsset(asset_handle);
      m_asset_cache.push_front(asset);

      // See CreateAsset for how a handle gets defined
      if (asset_handle >= m_iterator_lookup.size())
        m_iterator_lookup.resize(asset_handle + 1);
      m_iterator_lookup[asset_handle] = m_asset_cache.begin();
    }

    // Do not change the order of the asset in the cache
    // but add it to the back if it isn't available
    void MarkAssetAsAvailable(u64 asset_handle, CustomAsset* asset)
    {
      if (asset_handle >= m_iterator_lookup.size())
      {
        m_iterator_lookup.resize(asset_handle + 1);
      }

      if (!m_iterator_lookup[asset_handle].has_value())
      {
        m_asset_cache.push_back(asset);
        m_iterator_lookup[asset_handle] = std::prev(m_asset_cache.end());
      }
    }

    CustomAsset* RemoveLeastRecentAsset()
    {
      if (m_asset_cache.empty()) [[unlikely]]
        return nullptr;
      const auto ret = m_asset_cache.back();
      if (ret != nullptr)
      {
        m_iterator_lookup[ret->GetHandle()].reset();
      }
      m_asset_cache.pop_back();
      return ret;
    }

    void RemoveAsset(u64 asset_handle)
    {
      if (asset_handle >= m_iterator_lookup.size())
        return;

      if (const auto iter = m_iterator_lookup[asset_handle])
      {
        m_asset_cache.erase(*iter);
        m_iterator_lookup[asset_handle].reset();
      }
    }

    bool IsEmpty() const { return m_asset_cache.empty(); }

    std::size_t Size() const { return m_asset_cache.size(); }

  private:
    std::list<CustomAsset*> m_asset_cache;

    // Note: this vector is expected to be kept in sync with
    // the total amount of (unique) assets ever seen
    std::vector<std::optional<decltype(m_asset_cache)::iterator>> m_iterator_lookup;
  };

  LeastRecentlyUsedCache m_loaded_assets;
  LeastRecentlyUsedCache m_pending_assets;

  std::map<std::size_t, AssetData> m_asset_handle_to_data;
  std::map<CustomAssetLibrary::AssetID, std::size_t> m_asset_id_to_handle;

  u64 m_ram_used = 0;
  u64 m_max_ram_available = 0;

  std::map<CustomAssetLibrary::AssetID, InternalTextureDataResource> m_texture_data_asset_cache;

  std::mutex m_reload_mutex;
  std::set<CustomAssetLibrary::AssetID> m_assets_to_reload;

  CustomAssetLoader m_asset_loader;

  Common::EventHook m_xfb_event;
};

}  // namespace VideoCommon
