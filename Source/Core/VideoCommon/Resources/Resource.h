// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/Assets/AssetListener.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"

#include <memory>
#include <unordered_set>

class AbstractTexture;
namespace VideoCommon
{
class CustomAssetCache;
class CustomResourceManager;

// A resource is an abstract object that maintains
// relationships between assets (ex: a material that references a texture),
// as well as a standard way of calculating the final data (ex: a material's AbstractPipeline)
class Resource : public AssetListener
{
public:
  // Everything the resource needs to manage itself
  struct ResourceContext
  {
    CustomAssetLibrary::AssetID primary_asset_id;
    std::shared_ptr<CustomAssetLibrary> asset_library;
    CustomAssetCache* asset_cache;
    CustomResourceManager* resource_manager;
  };
  Resource(ResourceContext resource_context);

  enum class TaskComplete
  {
    Yes,
    No,
    Error
  };

  enum class State
  {
    ReloadData,
    CollectingPrimaryData,
    CollectingDependencyData,
    ProcessingData,
    DataAvailable
  };

  TaskComplete IsDataProcessed() const { return m_data_processed; }
  State GetState() const { return m_state; }

  void AddReference(Resource* resource);
  void RemoveReference(Resource* resource);

  virtual void MarkAsActive() = 0;
  virtual void MarkAsPending() = 0;

protected:
  ResourceContext m_resource_context;

private:
  void NotifyAssetChanged(bool has_error);
  void NotifyAssetUnloaded();

  void AssetLoaded(bool has_error, bool triggered_by_reload) final;
  void AssetUnloaded() final;
  virtual void OnUnloadRequested();

  friend class CustomResourceManager;
  virtual void ResetData();
  virtual TaskComplete CollectPrimaryData();
  virtual TaskComplete CollectDependencyData();
  virtual TaskComplete ProcessData();

  TaskComplete m_data_processed = TaskComplete::No;
  State m_state = State::ReloadData;

  std::unordered_set<Resource*> m_references;
};
}  // namespace VideoCommon
