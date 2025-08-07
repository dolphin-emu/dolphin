// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Resources/Resource.h"

namespace VideoCommon
{
Resource::Resource(CustomAssetLibrary::AssetID primary_asset_id,
                   std::shared_ptr<CustomAssetLibrary> asset_library, CustomAssetCache* asset_cache,
                   CustomResourceManager* resource_manager, TexturePool* texture_pool,
                   Common::AsyncWorkThreadSP* worker_queue)
    : m_primary_asset_id(std::move(primary_asset_id)), m_asset_library(std::move(asset_library)),
      m_asset_cache(asset_cache), m_resource_manager(resource_manager),
      m_texture_pool(texture_pool), m_worker_queue(worker_queue)
{
}

void Resource::NotifyAssetChanged(bool has_error)
{
  m_data_processed = has_error ? TaskComplete::Error : TaskComplete::No;
  m_state = State::ReloadData;

  for (Resource* reference : m_references)
  {
    reference->NotifyAssetChanged(has_error);
  }
}

void Resource::NotifyAssetUnloaded()
{
  OnUnloadRequested();

  for (Resource* reference : m_references)
  {
    reference->NotifyAssetUnloaded();
  }
}

void Resource::AddReference(Resource* reference)
{
  m_references.insert(reference);
}

void Resource::RemoveReference(Resource* reference)
{
  m_references.erase(reference);
}

void Resource::AssetLoaded(bool has_error, bool triggered_by_reload)
{
  if (triggered_by_reload)
    NotifyAssetChanged(has_error);
}

void Resource::AssetUnloaded()
{
  NotifyAssetUnloaded();
}

void Resource::OnUnloadRequested()
{
}

void Resource::ResetData()
{
}

Resource::TaskComplete Resource::CollectPrimaryData()
{
  return TaskComplete::Yes;
}

Resource::TaskComplete Resource::CollectDependencyData()
{
  return TaskComplete::Yes;
}

Resource::TaskComplete Resource::ProcessData()
{
  return TaskComplete::Yes;
}
}  // namespace VideoCommon
