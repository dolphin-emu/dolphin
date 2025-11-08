// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/AsyncRequests.h"

#include "Core/System.h"

#include "VideoCommon/Fifo.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoEvents.h"

AsyncRequests AsyncRequests::s_singleton;

AsyncRequests::AsyncRequests() = default;

void AsyncRequests::PullEvents()
{
  if (m_queue.Empty())
    return;

  // This is only called if the queue isn't empty.
  // So just flush the pipeline to get accurate results.
  g_vertex_manager->Flush();

  while (!m_queue.Empty())
  {
    std::invoke(std::move(m_queue.Front()));
    m_queue.Pop();
  }
}

void AsyncRequests::QueueEvent(Event&& event)
{
  m_queue.Push(std::move(event));

  auto& system = Core::System::GetInstance();
  system.GetFifo().RunGpu();
}

void AsyncRequests::WaitForEmptyQueue()
{
  m_queue.WaitForEmpty();
}

void AsyncRequests::SetPassthrough(bool enable)
{
  m_passthrough = enable;
}
