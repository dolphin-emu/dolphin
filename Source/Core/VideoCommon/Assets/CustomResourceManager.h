// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/HookableEvent.h"

#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/Assets/CustomAssetLoader2.h"
#include "VideoCommon/Assets/CustomTextureData.h"

namespace VideoCommon
{
class GameTextureAsset;

class CustomResourceManager
{
public:
  void Initialize();
  void Shutdown();

  void Reset();

  // Requests that an asset that exists be reloaded
  void ReloadAsset(const CustomAssetLibrary::AssetID& asset_id);

  void XFBTriggered(std::string_view texture_hash);

  CustomTextureData*
  GetTextureDataFromAsset(const CustomAssetLibrary::AssetID& asset_id,
                          std::shared_ptr<VideoCommon::CustomAssetLibrary> library);

private:
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

    enum class LoadType
    {
      PendingReload,
      LoadFinished,
      LoadFinalyzed,
      DependenciesChanged,
      Unloaded
    };
    LoadType load_type = LoadType::PendingReload;
    bool has_errors = false;
  };

  struct InternalTextureDataResource
  {
    AssetData* asset_data = nullptr;
    VideoCommon::GameTextureAsset* asset = nullptr;
    std::shared_ptr<TextureData> texture_data;
  };

  void LoadTextureDataAsset(const CustomAssetLibrary::AssetID& asset_id,
                            std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
                            InternalTextureDataResource* internal_texture_data);

  template <typename T>
  T* CreateAsset(const CustomAssetLibrary::AssetID& asset_id, AssetData::AssetType asset_type,
                 std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
  {
    const auto [it, added] =
        m_asset_id_to_session_id.try_emplace(asset_id, m_session_id_to_asset_data.size());

    if (added)
    {
      AssetData asset_data;
      asset_data.asset = std::make_unique<T>(library, asset_id, it->second);
      asset_data.type = asset_type;
      asset_data.has_errors = false;
      asset_data.load_type = AssetData::LoadType::PendingReload;
      asset_data.load_request_time = {};

      m_session_id_to_asset_data.insert_or_assign(it->second, std::move(asset_data));

      // Synchronize the priority cache session id
      m_pending_assets.prepare();
      m_loaded_assets.prepare();
    }
    auto& asset_data_from_session = m_session_id_to_asset_data[it->second];

    // Asset got unloaded, rebuild it with the same metadata
    if (!asset_data_from_session.asset)
    {
      asset_data_from_session.asset = std::make_unique<T>(library, asset_id, it->second);
      asset_data_from_session.has_errors = false;
      asset_data_from_session.load_type = AssetData::LoadType::PendingReload;
    }

    return static_cast<T*>(asset_data_from_session.asset.get());
  }

  class LeastRecentlyUsedCache
  {
  public:
    const std::list<CustomAsset*>& elements() const { return m_asset_cache; }

    void put(u64 asset_session_id, CustomAsset* asset)
    {
      erase(asset_session_id);
      m_asset_cache.push_front(asset);
      m_iterator_lookup[m_asset_cache.front()->GetSessionId()] = m_asset_cache.begin();
    }

    CustomAsset* pop()
    {
      if (m_asset_cache.empty()) [[unlikely]]
        return nullptr;
      const auto ret = m_asset_cache.back();
      if (ret != nullptr)
      {
        m_iterator_lookup[ret->GetSessionId()].reset();
      }
      m_asset_cache.pop_back();
      return ret;
    }

    void prepare() { m_iterator_lookup.push_back(std::nullopt); }

    void erase(u64 asset_session_id)
    {
      if (const auto iter = m_iterator_lookup[asset_session_id])
      {
        m_asset_cache.erase(*iter);
        m_iterator_lookup[asset_session_id].reset();
      }
    }

    bool empty() const { return m_asset_cache.empty(); }

    std::size_t size() const { return m_asset_cache.size(); }

  private:
    std::list<CustomAsset*> m_asset_cache;

    // Note: this vector is expected to be kept in sync with
    // the total amount of (unique) assets ever seen
    std::vector<std::optional<decltype(m_asset_cache)::iterator>> m_iterator_lookup;
  };

  LeastRecentlyUsedCache m_loaded_assets;
  LeastRecentlyUsedCache m_pending_assets;

  std::map<std::size_t, AssetData> m_session_id_to_asset_data;
  std::map<CustomAssetLibrary::AssetID, std::size_t> m_asset_id_to_session_id;

  u64 m_ram_used = 0;
  u64 m_max_ram_available = 0;

  std::map<CustomAssetLibrary::AssetID, InternalTextureDataResource> m_texture_data_asset_cache;

  std::mutex m_reload_mutex;
  std::set<CustomAssetLibrary::AssetID> m_assets_to_reload;

  CustomAssetLoader2 m_asset_loader;

  Common::EventHook m_xfb_event;
};

}  // namespace VideoCommon
