// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"

#include <atomic>
#include <memory>
#include <mutex>

namespace VideoCommon
{
// An abstract class that provides operations for loading
// data from a 'CustomAssetLibrary'
class CustomAsset
{
public:
  using ClockType = std::chrono::steady_clock;
  using TimeType = ClockType::time_point;

  CustomAsset(std::shared_ptr<CustomAssetLibrary> library,
              const CustomAssetLibrary::AssetID& asset_id, u64 session_id);
  virtual ~CustomAsset() = default;
  CustomAsset(const CustomAsset&) = delete;
  CustomAsset(CustomAsset&&) = delete;
  CustomAsset& operator=(const CustomAsset&) = delete;
  CustomAsset& operator=(CustomAsset&&) = delete;

  // Loads the asset from the library returning the number of bytes loaded
  std::size_t Load();

  // Unloads the asset data, resets the bytes loaded and
  // returns the number of bytes unloaded
  std::size_t Unload();

  // Returns the time that the data was last loaded
  TimeType GetLastLoadedTime() const;

  // Returns an id that uniquely identifies this asset
  const CustomAssetLibrary::AssetID& GetAssetId() const;

  // Returns an id that is unique to this game session
  // This is a faster form to hash and can be used
  // as an index
  std::size_t GetHandle() const;

protected:
  const std::shared_ptr<CustomAssetLibrary> m_owning_library;

private:
  virtual CustomAssetLibrary::LoadInfo LoadImpl(const CustomAssetLibrary::AssetID& asset_id) = 0;
  virtual void UnloadImpl() = 0;
  CustomAssetLibrary::AssetID m_asset_id;
  std::size_t m_handle;

  mutable std::mutex m_info_lock;
  std::size_t m_bytes_loaded = 0;
  std::atomic<TimeType> m_last_loaded_time = {};
};

// An abstract class that is expected to
// be the base class for all assets
// It provides a simple interface for
// verifying that an asset data of type
// 'UnderlyingType' is loaded by
// checking against 'GetData()'
template <typename UnderlyingType>
class CustomLoadableAsset : public CustomAsset
{
public:
  using CustomAsset::CustomAsset;

  // Callees should understand that the type returned is
  // a local copy and 'GetData()' needs to be called
  // to ensure the latest copy is available if
  // they want to handle reloads
  [[nodiscard]] std::shared_ptr<UnderlyingType> GetData() const
  {
    std::lock_guard lk(m_data_lock);
    if (m_loaded)
      return m_data;
    return nullptr;
  }

protected:
  bool m_loaded = false;
  mutable std::mutex m_data_lock;
  std::shared_ptr<UnderlyingType> m_data;

private:
  void UnloadImpl() override
  {
    std::lock_guard lk(m_data_lock);
    m_loaded = false;
    m_data.reset();
  }
};

// A helper struct that contains
// an asset with a last cached write time
// This can be used to determine if the asset
// has diverged from the last known state
// To know if it is time to update other dependencies
// based on the asset data
// TODO C++20: use a 'derived_from' concept against 'CustomAsset' when available
template <typename AssetType>
struct CachedAsset
{
  std::shared_ptr<AssetType> m_asset;
  CustomAsset::TimeType m_cached_write_time;
};

}  // namespace VideoCommon
