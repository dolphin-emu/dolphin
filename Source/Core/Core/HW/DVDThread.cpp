// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>
#include <map>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/FifoQueue.h"
#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Thread.h"
#include "Common/Timer.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/DVDThread.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"

#include "DiscIO/Volume.h"

namespace DVDThread
{
struct ReadRequest
{
  u64 dvd_offset;
  u32 output_address;
  u32 length;
  bool decrypt;

  // This determines which function will be used as a callback.
  // We can't have a function pointer here, because they can't be in savestates.
  bool reply_to_ios;

  // IDs are used to uniquely identify a request. They must not be identical
  // to the IDs of any other requests that currently exist, but it's fine
  // fine to for instance re-use IDs from a previous emulation session.
  u64 id;

  // Only used for logging
  u64 time_started_ticks;
  u64 realtime_started_us;
  u64 realtime_done_us;
};

using ReadResult = std::pair<ReadRequest, std::vector<u8>>;

static void DVDThread();

static void FinishRead(u64 userdata, s64 cycles_late);
static int s_finish_read;

static u32 s_next_id = 0;

static std::thread s_dvd_thread;
static Common::Event s_request_queue_expanded;    // Is set by CPU thread
static Common::Event s_result_queue_expanded;     // Is set by DVD thread
static Common::Event s_dvd_thread_done_working;   // Is set by DVD thread
static Common::Flag s_dvd_thread_exiting(false);  // Is set by CPU thread

static Common::FifoQueue<ReadRequest, false> s_request_queue;
static Common::FifoQueue<ReadResult, false> s_result_queue;
static std::map<u64, ReadResult> s_result_map;

void Start()
{
  s_finish_read = CoreTiming::RegisterEvent("FinishReadDVDThread", FinishRead);
  _assert_(!s_dvd_thread.joinable());
  s_dvd_thread = std::thread(DVDThread);
}

void Stop()
{
  _assert_(s_dvd_thread.joinable());

  // By setting s_DVD_thread_exiting, we ask the DVD thread to cleanly exit.
  // In case the request queue is empty, we need to set s_request_queue_expanded
  // so that the DVD thread will wake up and check s_DVD_thread_exiting.
  s_dvd_thread_exiting.Set();
  s_request_queue_expanded.Set();

  s_dvd_thread.join();

  s_dvd_thread_exiting.Clear();
  s_request_queue_expanded.Reset();
  s_result_queue_expanded.Reset();
  s_request_queue.Clear();
  s_result_queue.Clear();

  // This is reset for determinism, but it doesn't matter much,
  // because this will never get exposed to the emulated game.
  s_next_id = 0;
}

void DoState(PointerWrap& p)
{
  // By waiting for the DVD thread to be done working, we ensure that
  // there are no pending requests. The DVD thread won't be touching
  // s_result_queue, and everything we need to save will be in either
  // s_result_queue or s_result_map (other than s_next_id).
  WaitUntilIdle();

  // Move everything from s_result_queue to s_result_map because
  // PointerWrap::Do supports std::map but not Common::FifoQueue.
  // This won't affect the behavior of FinishRead.
  ReadResult result;
  while (s_result_queue.Pop(result))
  {
    u64 id = result.first.id;
    s_result_map.emplace(id, std::move(result));
  }

  // Everything is now in s_result_map, so we simply savestate that.
  // We also savestate s_next_id to avoid ID collisions.
  p.Do(s_result_map);
  p.Do(s_next_id);

  // TODO: Savestates can be smaller if the buffers of results aren't saved,
  // but instead get re-read from the disc when loading the savestate.

  // After loading a savestate, the debug log in FinishRead will report
  // screwed up times for requests that were submitted before the savestate
  // was made. Handling that properly may be more effort than it's worth.
}

void WaitUntilIdle()
{
  _assert_(Core::IsCPUThread());

  // Wait until DVD thread isn't working
  s_dvd_thread_done_working.Wait();

  // Set the event again so that we still know that the DVD thread
  // isn't working. This is needed in case this function gets called
  // again before the DVD thread starts working.
  s_dvd_thread_done_working.Set();
}

void StartRead(u64 dvd_offset, u32 output_address, u32 length, bool decrypt, bool reply_to_ios,
               int ticks_until_completion)
{
  _assert_(Core::IsCPUThread());

  ReadRequest request;

  request.dvd_offset = dvd_offset;
  request.output_address = output_address;
  request.length = length;
  request.decrypt = decrypt;
  request.reply_to_ios = reply_to_ios;

  u64 id = s_next_id++;
  request.id = id;

  request.time_started_ticks = CoreTiming::GetTicks();
  request.realtime_started_us = Common::Timer::GetTimeUs();

  s_request_queue.Push(std::move(request));
  s_request_queue_expanded.Set();

  CoreTiming::ScheduleEvent(ticks_until_completion, s_finish_read, id);
}

static void FinishRead(u64 userdata, s64 cycles_late)
{
  // We can't simply pop s_result_queue and always get the ReadResult
  // we want, because the DVD thread may add ReadResults to the queue
  // in a different order than we want to get them. What we do instead
  // is to pop the queue until we find the ReadResult we want (the one
  // whose ID matches userdata), which means we may end up popping
  // ReadResults that we don't want. We can't add those unwanted results
  // back to the queue, because the queue can only have one writer.
  // Instead, we add them to a map that only is used by the CPU thread.
  // When this function is called again later, it will check the map for
  // the wanted ReadResult before it starts searching through the queue.
  ReadResult result;
  auto it = s_result_map.find(userdata);
  if (it != s_result_map.end())
  {
    result = std::move(it->second);
    s_result_map.erase(it);
  }
  else
  {
    while (true)
    {
      while (!s_result_queue.Pop(result))
        s_result_queue_expanded.Wait();

      u64 id = result.first.id;
      if (id == userdata)
        break;
      else
        s_result_map.emplace(id, std::move(result));
    }
  }
  // We have now obtained the right ReadResult.

  const ReadRequest& request = result.first;
  const std::vector<u8>& buffer = result.second;

  DEBUG_LOG(DVDINTERFACE, "Disc has been read. Real time: %" PRIu64 " us. "
                          "Real time including delay: %" PRIu64 " us. "
                          "Emulated time including delay: %" PRIu64 " us.",
            request.realtime_done_us - request.realtime_started_us,
            Common::Timer::GetTimeUs() - request.realtime_started_us,
            (CoreTiming::GetTicks() - request.time_started_ticks) /
                (SystemTimers::GetTicksPerSecond() / 1000000));

  if (!buffer.empty())
    Memory::CopyToEmu(request.output_address, buffer.data(), request.length);
  else
    PanicAlertT("The disc could not be read (at 0x%" PRIx64 " - 0x%" PRIx64 ").",
                request.dvd_offset, request.dvd_offset + request.length);

  // Notify the emulated software that the command has been executed
  DVDInterface::FinishExecutingCommand(request.reply_to_ios, DVDInterface::INT_TCINT);
}

static void DVDThread()
{
  Common::SetCurrentThreadName("DVD thread");

  while (true)
  {
    s_dvd_thread_done_working.Set();

    s_request_queue_expanded.Wait();

    if (s_dvd_thread_exiting.IsSet())
      return;

    s_dvd_thread_done_working.Reset();

    ReadRequest request;
    while (s_request_queue.Pop(request))
    {
      if (s_dvd_thread_exiting.IsSet())
        return;

      std::vector<u8> buffer(request.length);
      const DiscIO::IVolume& volume = DVDInterface::GetVolume();
      if (!volume.Read(request.dvd_offset, request.length, buffer.data(), request.decrypt))
        buffer.resize(0);

      request.realtime_done_us = Common::Timer::GetTimeUs();

      s_result_queue.Push(ReadResult(std::move(request), std::move(buffer)));
      s_result_queue_expanded.Set();
    }
  }
}
}
