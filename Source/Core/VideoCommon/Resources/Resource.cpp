// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Resources/Resource.h"

namespace VideoCommon
{
Resource::Resource(ResourceContext resource_context)
    : m_resource_context(std::move(resource_context))
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

void Resource::NotifyAssetLoadSuccess()
{
  NotifyAssetChanged(false);
}

void Resource::NotifyAssetLoadFailed()
{
  NotifyAssetChanged(true);
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
