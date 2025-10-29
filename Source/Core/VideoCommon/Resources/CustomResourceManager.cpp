// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Resources/CustomResourceManager.h"

#include "VideoCommon/VideoEvents.h"

namespace VideoCommon
{
void CustomResourceManager::Initialize()
{
  m_asset_cache.Initialize();

  m_xfb_event =
      AfterFrameEvent::Register([this](Core::System&) { XFBTriggered(); }, "CustomResourceManager");
}

void CustomResourceManager::Shutdown()
{
  m_asset_cache.Shutdown();
  Reset();
}

void CustomResourceManager::Reset()
{
  m_texture_data_resources.clear();

  m_asset_cache.Reset();
}

void CustomResourceManager::MarkAssetDirty(const CustomAssetLibrary::AssetID& asset_id)
{
  m_asset_cache.MarkAssetDirty(asset_id);
}

void CustomResourceManager::XFBTriggered()
{
  m_asset_cache.Update();
}

TextureDataResource* CustomResourceManager::GetTextureDataFromAsset(
    const CustomAssetLibrary::AssetID& asset_id,
    std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  const auto [it, added] = m_texture_data_resources.try_emplace(asset_id, nullptr);
  if (added)
  {
    it->second = std::make_unique<TextureDataResource>(CreateResourceContext(asset_id, library));
  }
  ProcessResource(it->second.get());
  return it->second.get();
}

Resource::ResourceContext CustomResourceManager::CreateResourceContext(
    const CustomAssetLibrary::AssetID& asset_id,
    const std::shared_ptr<VideoCommon::CustomAssetLibrary>& library)
{
  return Resource::ResourceContext{asset_id, library, &m_asset_cache, this};
}

void CustomResourceManager::ProcessResource(Resource* resource)
{
  resource->MarkAsActive();

  const auto data_processed = resource->IsDataProcessed();
  if (data_processed == Resource::TaskComplete::Yes ||
      data_processed == Resource::TaskComplete::Error)
  {
    resource->MarkAsActive();
    if (data_processed == Resource::TaskComplete::Error)
      return;
  }

  // Early out if we're already at our end state
  if (resource->GetState() == Resource::State::DataAvailable)
    return;

  ProcessResourceState(resource);
}

void CustomResourceManager::ProcessResourceState(Resource* resource)
{
  Resource::State next_state = resource->GetState();
  Resource::TaskComplete task_complete = Resource::TaskComplete::No;
  switch (resource->GetState())
  {
  case Resource::State::ReloadData:
    resource->ResetData();
    task_complete = Resource::TaskComplete::Yes;
    next_state = Resource::State::CollectingPrimaryData;
    break;
  case Resource::State::CollectingPrimaryData:
    task_complete = resource->CollectPrimaryData();
    next_state = Resource::State::CollectingDependencyData;
    if (task_complete == Resource::TaskComplete::No)
      resource->MarkAsPending();
    break;
  case Resource::State::CollectingDependencyData:
    task_complete = resource->CollectDependencyData();
    next_state = Resource::State::ProcessingData;
    break;
  case Resource::State::ProcessingData:
    task_complete = resource->ProcessData();
    next_state = Resource::State::DataAvailable;
  };

  if (task_complete == Resource::TaskComplete::Yes)
  {
    resource->m_state = next_state;
    if (next_state == Resource::State::DataAvailable)
    {
      resource->m_data_processed = task_complete;
    }
    else
    {
      ProcessResourceState(resource);
    }
  }
  else if (task_complete == Resource::TaskComplete::Error)
  {
    resource->m_data_processed = task_complete;
  }
}
}  // namespace VideoCommon
