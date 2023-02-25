// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/AsyncRequests.h"

#include <mutex>

#include "Core/System.h"

#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoEvents.h"
#include "VideoCommon/VideoState.h"

AsyncRequests AsyncRequests::s_singleton;

AsyncRequests::AsyncRequests() = default;

void AsyncRequests::PullEventsInternal()
{
  // This is only called if the queue isn't empty.
  // So just flush the pipeline to get accurate results.
  g_vertex_manager->Flush();

  std::unique_lock<std::mutex> lock(m_mutex);
  m_empty.Set();

  while (!m_queue.empty())
  {
    Event e = m_queue.front();

    // try to merge as many efb pokes as possible
    // it's a bit hacky, but some games render a complete frame in this way
    if ((e.type == Event::EFB_POKE_COLOR || e.type == Event::EFB_POKE_Z))
    {
      m_merged_efb_pokes.clear();
      Event first_event = m_queue.front();
      const auto t = first_event.type == Event::EFB_POKE_COLOR ? EFBAccessType::PokeColor :
                                                                 EFBAccessType::PokeZ;

      do
      {
        e = m_queue.front();

        EfbPokeData d;
        d.data = e.efb_poke.data;
        d.x = e.efb_poke.x;
        d.y = e.efb_poke.y;
        m_merged_efb_pokes.push_back(d);

        m_queue.pop();
      } while (!m_queue.empty() && m_queue.front().type == first_event.type);

      lock.unlock();
      g_renderer->PokeEFB(t, m_merged_efb_pokes.data(), m_merged_efb_pokes.size());
      lock.lock();
      continue;
    }

    lock.unlock();
    HandleEvent(e);
    lock.lock();

    m_queue.pop();
  }

  if (m_wake_me_up_again)
  {
    m_wake_me_up_again = false;
    m_cond.notify_all();
  }
}

void AsyncRequests::PushEvent(const AsyncRequests::Event& event, bool blocking)
{
  std::unique_lock<std::mutex> lock(m_mutex);

  if (m_passthrough)
  {
    HandleEvent(event);
    return;
  }

  m_empty.Clear();
  m_wake_me_up_again |= blocking;

  if (!m_enable)
    return;

  m_queue.push(event);

  auto& system = Core::System::GetInstance();
  system.GetFifo().RunGpu(system);
  if (blocking)
  {
    m_cond.wait(lock, [this] { return m_queue.empty(); });
  }
}

void AsyncRequests::WaitForEmptyQueue()
{
  std::unique_lock<std::mutex> lock(m_mutex);
  m_cond.wait(lock, [this] { return m_queue.empty(); });
}

void AsyncRequests::SetEnable(bool enable)
{
  std::unique_lock<std::mutex> lock(m_mutex);
  m_enable = enable;

  if (!enable)
  {
    // flush the queue on disabling
    while (!m_queue.empty())
      m_queue.pop();
    if (m_wake_me_up_again)
      m_cond.notify_all();
  }
}

void AsyncRequests::HandleEvent(const AsyncRequests::Event& e)
{
  switch (e.type)
  {
  case Event::EFB_POKE_COLOR:
  {
    INCSTAT(g_stats.this_frame.num_efb_pokes);
    EfbPokeData poke = {e.efb_poke.x, e.efb_poke.y, e.efb_poke.data};
    g_renderer->PokeEFB(EFBAccessType::PokeColor, &poke, 1);
  }
  break;

  case Event::EFB_POKE_Z:
  {
    INCSTAT(g_stats.this_frame.num_efb_pokes);
    EfbPokeData poke = {e.efb_poke.x, e.efb_poke.y, e.efb_poke.data};
    g_renderer->PokeEFB(EFBAccessType::PokeZ, &poke, 1);
  }
  break;

  case Event::EFB_PEEK_COLOR:
    INCSTAT(g_stats.this_frame.num_efb_peeks);
    *e.efb_peek.data =
        g_renderer->AccessEFB(EFBAccessType::PeekColor, e.efb_peek.x, e.efb_peek.y, 0);
    break;

  case Event::EFB_PEEK_Z:
    INCSTAT(g_stats.this_frame.num_efb_peeks);
    *e.efb_peek.data = g_renderer->AccessEFB(EFBAccessType::PeekZ, e.efb_peek.x, e.efb_peek.y, 0);
    break;

  case Event::SWAP_EVENT:
    g_presenter->ViSwap(e.swap_event.xfbAddr, e.swap_event.fbWidth, e.swap_event.fbStride,
                        e.swap_event.fbHeight, e.time);
    break;

  case Event::BBOX_READ:
    *e.bbox.data = g_bounding_box->Get(e.bbox.index);
    break;

  case Event::FIFO_RESET:
    Core::System::GetInstance().GetFifo().ResetVideoBuffer();
    break;

  case Event::PERF_QUERY:
    g_perf_query->FlushResults();
    break;

  case Event::DO_SAVE_STATE:
    VideoCommon_DoState(*e.do_save_state.p);
    break;
  }
}

void AsyncRequests::SetPassthrough(bool enable)
{
  std::unique_lock<std::mutex> lock(m_mutex);
  m_passthrough = enable;
}
