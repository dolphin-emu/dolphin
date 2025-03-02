// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <list>
#include <map>
#include <memory>
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
class TextureAsset;

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
  void MarkAssetDirty(const CustomAssetLibrary::AssetID& asset_id);

  void XFBTriggered();

  using TextureTimePair = std::pair<std::shared_ptr<CustomTextureData>, CustomAsset::TimeType>;

  // Returns a pair with the custom texture data and the time it was last loaded
  // Callees are not expected to hold onto the shared_ptr as that will prevent
  // the resource manager from being able to properly release data
  TextureTimePair GetTextureDataFromAsset(const CustomAssetLibrary::AssetID& asset_id,
                                          std::shared_ptr<VideoCommon::CustomAssetLibrary> library);

private:
  // A generic interface to describe an assets' type
  // and load state
  struct AssetData
  {
    std::unique_ptr<CustomAsset> asset;
    CustomAsset::TimeType load_request_time = {};
    bool has_load_error = false;

    enum class AssetType
    {
      TextureData
    };
    AssetType type;

    enum class LoadStatus
    {
      PendingReload,
      LoadFinished,
      ResourceDataAvailable,
      Unloaded,
    };
    LoadStatus load_status = LoadStatus::PendingReload;
  };

  // A structure to represent some raw texture data
  // (this data hasn't hit the GPU yet, used for custom textures)
  struct InternalTextureDataResource
  {
    AssetData* asset_data = nullptr;
    VideoCommon::TextureAsset* asset = nullptr;
    std::shared_ptr<CustomTextureData> texture_data;
  };

  void LoadTextureDataAsset(const CustomAssetLibrary::AssetID& asset_id,
                            std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
                            InternalTextureDataResource* resource);

  void ProcessDirtyAssets();
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
      asset_data.load_request_time = {};
      asset_data.has_load_error = false;

      m_asset_handle_to_data.insert_or_assign(it->second, std::move(asset_data));
    }
    auto& asset_data_from_handle = m_asset_handle_to_data[it->second];
    asset_data_from_handle.load_status = AssetData::LoadStatus::PendingReload;

    return static_cast<T*>(asset_data_from_handle.asset.get());
  }

  // Maintains a priority-sorted list of assets.
  // Used to figure out which assets to load or unload first.
  // Most recently used assets get marked with highest priority.
  class AssetPriorityQueue
  {
  public:
    const auto& Elements() const { return m_assets; }

    // Inserts or moves the asset to the top of the queue.
    void MakeAssetHighestPriority(u64 asset_handle, CustomAsset* asset)
    {
      RemoveAsset(asset_handle);
      m_assets.push_front(asset);

      // See CreateAsset for how a handle gets defined
      if (asset_handle >= m_iterator_lookup.size())
        m_iterator_lookup.resize(asset_handle + 1, m_assets.end());

      m_iterator_lookup[asset_handle] = m_assets.begin();
    }

    // Inserts an asset at lowest priority or
    //  does nothing if asset is already in the queue.
    void InsertAsset(u64 asset_handle, CustomAsset* asset)
    {
      if (asset_handle >= m_iterator_lookup.size())
        m_iterator_lookup.resize(asset_handle + 1, m_assets.end());

      if (m_iterator_lookup[asset_handle] == m_assets.end())
      {
        m_assets.push_back(asset);
        m_iterator_lookup[asset_handle] = std::prev(m_assets.end());
      }
    }

    CustomAsset* RemoveLowestPriorityAsset()
    {
      if (m_assets.empty()) [[unlikely]]
        return nullptr;
      auto* const ret = m_assets.back();
      if (ret != nullptr)
      {
        m_iterator_lookup[ret->GetHandle()] = m_assets.end();
      }
      m_assets.pop_back();
      return ret;
    }

    void RemoveAsset(u64 asset_handle)
    {
      if (asset_handle >= m_iterator_lookup.size())
        return;

      const auto iter = m_iterator_lookup[asset_handle];
      if (iter != m_assets.end())
      {
        m_assets.erase(iter);
        m_iterator_lookup[asset_handle] = m_assets.end();
      }
    }

    bool IsEmpty() const { return m_assets.empty(); }

    std::size_t Size() const { return m_assets.size(); }

  private:
    std::list<CustomAsset*> m_assets;

    // Handle-to-iterator lookup for fast access.
    // Grows as needed on insert.
    std::vector<decltype(m_assets)::iterator> m_iterator_lookup;
  };

  // Assets that are currently active in memory, in order of most recently used by the game.
  AssetPriorityQueue m_active_assets;

  // Assets that need to be loaded.
  // e.g. Because the game tried to use them or because they changed on disk.
  // Ordered by most recently used.
  AssetPriorityQueue m_pending_assets;

  std::map<std::size_t, AssetData> m_asset_handle_to_data;
  std::map<CustomAssetLibrary::AssetID, std::size_t> m_asset_id_to_handle;

  // Memory used by currently "loaded" assets.
  u64 m_ram_used = 0;

  // A calculated amount of memory to avoid exceeding.
  u64 m_max_ram_available = 0;

  std::map<CustomAssetLibrary::AssetID, InternalTextureDataResource> m_texture_data_asset_cache;

  std::mutex m_dirty_mutex;
  std::set<CustomAssetLibrary::AssetID> m_dirty_assets;

  CustomAssetLoader m_asset_loader;

  Common::EventHook m_xfb_event;
};

}  // namespace VideoCommon
