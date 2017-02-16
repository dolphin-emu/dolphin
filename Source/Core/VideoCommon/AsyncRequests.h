// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Flag.h"

struct EfbPokeData;

class AsyncRequests
{
public:
  struct Event
  {
    enum Type
    {
      EFB_POKE_COLOR,
      EFB_POKE_Z,
      EFB_PEEK_COLOR,
      EFB_PEEK_Z,
      SWAP_EVENT,
      BBOX_READ,
      PERF_QUERY,
    } type;
    u64 time;

    union
    {
      struct
      {
        u16 x;
        u16 y;
        u32 data;
      } efb_poke;

      struct
      {
        u16 x;
        u16 y;
        u32* data;
      } efb_peek;

      struct
      {
        u32 xfbAddr;
        u32 fbWidth;
        u32 fbStride;
        u32 fbHeight;
      } swap_event;

      struct
      {
        int index;
        u16* data;
      } bbox;

      struct
      {
      } perf_query;
    };
  };

  AsyncRequests();

  void PullEvents()
  {
    if (!m_empty.IsSet())
      PullEventsInternal();
  }
  void PushEvent(const Event& event, bool blocking = false);
  void SetEnable(bool enable);
  void SetPassthrough(bool enable);

  static AsyncRequests* GetInstance() { return &s_singleton; }
private:
  void PullEventsInternal();
  void HandleEvent(const Event& e);

  static AsyncRequests s_singleton;

  Common::Flag m_empty;
  std::queue<Event> m_queue;
  std::mutex m_mutex;
  std::condition_variable m_cond;

  bool m_wake_me_up_again;
  bool m_enable;
  bool m_passthrough;

  std::vector<EfbPokeData> m_merged_efb_pokes;
};
