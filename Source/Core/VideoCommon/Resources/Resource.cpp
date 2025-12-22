// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Resources/Resource.h"

#include "Common/Logging/Log.h"

namespace VideoCommon
{
Resource::Resource(ResourceContext resource_context)
    : m_resource_context(std::move(resource_context))
{
}

void Resource::Process()
{
  MarkAsActive();

  if (m_data_processed == TaskComplete::Error)
    return;

  ProcessCurrentTask();
}

void Resource::ProcessCurrentTask()
{
  Task next_task = m_current_task;
  TaskComplete task_complete = TaskComplete::No;
  switch (m_current_task)
  {
  case Task::ReloadData:
    ResetData();
    task_complete = TaskComplete::Yes;
    next_task = Task::CollectPrimaryData;
    break;
  case Task::CollectPrimaryData:
    task_complete = CollectPrimaryData();
    next_task = Task::CollectDependencyData;
    if (task_complete == TaskComplete::No)
      MarkAsPending();
    break;
  case Task::CollectDependencyData:
    task_complete = CollectDependencyData();
    next_task = Task::ProcessData;
    break;
  case Task::ProcessData:
    task_complete = ProcessData();
    next_task = Task::DataAvailable;
    break;
  case Task::DataAvailable:
    // Early out, we're already at our end state
    return;
  default:
    ERROR_LOG_FMT(VIDEO, "Unknown task '{}' for resource '{}'", static_cast<int>(m_current_task),
                  m_resource_context.primary_asset_id);
    return;
  };

  if (task_complete == Resource::TaskComplete::Yes)
  {
    m_current_task = next_task;
    if (next_task == Resource::Task::DataAvailable)
    {
      m_data_processed = task_complete;
    }
    else
    {
      // No need to wait for the next resource request
      // process the new task immediately
      ProcessCurrentTask();
    }
  }
  else if (task_complete == Resource::TaskComplete::Error)
  {
    // If we failed our task due to an error,
    // we can't service this resource request,
    // wait for a reload and mark the whole
    // data processing as an error
    m_data_processed = task_complete;
  }
}

void Resource::NotifyAssetChanged(bool has_error)
{
  m_data_processed = has_error ? TaskComplete::Error : TaskComplete::No;
  m_current_task = Task::ReloadData;

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
