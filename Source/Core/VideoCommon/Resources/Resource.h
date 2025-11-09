// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/WorkQueueThread.h"

#include "VideoCommon/Assets/AssetListener.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"

#include <memory>
#include <unordered_set>

class AbstractTexture;
namespace VideoCommon
{
class AsyncShaderCompiler;
class CustomAssetCache;
class CustomResourceManager;
class TexturePool;

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
    TexturePool* texture_pool;
    Common::AsyncWorkThreadSP* worker_queue;
    AsyncShaderCompiler* shader_compiler;
    AbstractTexture* invalid_array_texture;
    AbstractTexture* invalid_color_texture;
    AbstractTexture* invalid_cubemap_texture;
    AbstractTexture* invalid_transparent_texture;
  };
  explicit Resource(ResourceContext resource_context);

  enum class TaskComplete
  {
    Yes,
    No,
    Error,
  };

  void Process();
  TaskComplete IsDataProcessed() const { return m_data_processed; }

  void AddReference(Resource* reference);
  void RemoveReference(Resource* reference);

  virtual void MarkAsActive() = 0;
  virtual void MarkAsPending() = 0;

protected:
  ResourceContext m_resource_context;

private:
  void ProcessCurrentTask();
  void NotifyAssetChanged(bool has_error);
  void NotifyAssetUnloaded();

  void NotifyAssetLoadSuccess() final;
  void NotifyAssetLoadFailed() final;
  void AssetUnloaded() final;
  virtual void OnUnloadRequested();

  friend class CustomResourceManager;
  virtual void ResetData();
  virtual TaskComplete CollectPrimaryData();
  virtual TaskComplete CollectDependencyData();
  virtual TaskComplete ProcessData();

  TaskComplete m_data_processed = TaskComplete::No;

  enum class Task
  {
    ReloadData,
    CollectPrimaryData,
    CollectDependencyData,
    ProcessData,
    DataAvailable,
  };
  Task m_current_task = Task::ReloadData;

  std::unordered_set<Resource*> m_references;
};
}  // namespace VideoCommon
