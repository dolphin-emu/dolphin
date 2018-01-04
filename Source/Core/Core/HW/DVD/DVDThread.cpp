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
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/DVD/DVDThread.h"
#include "Core/HW/DVD/FileMonitor.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"

#include "DiscIO/Volume.h"

namespace DVDThread
{
struct ReadRequest
{
  bool copy_to_ram;
  u32 output_address;
  u64 dvd_offset;
  u32 length;
  bool decrypt;

  // This determines which code DVDInterface will run to reply
  // to the emulated software. We can't use callbacks,
  // because function pointers can't be stored in savestates.
  DVDInterface::ReplyType reply_type;

  // IDs are used to uniquely identify a request. They must not be
  // identical to IDs of any other requests that currently exist, but
  // it's fine to re-use IDs of requests that have existed in the past.
  u64 id;

  // Only used for logging
  u64 time_started_ticks;
  u64 realtime_started_us;
  u64 realtime_done_us;
};

using ReadResult = std::pair<ReadRequest, std::vector<u8>>;

static void StartDVDThread();
static void StopDVDThread();

static void DVDThread();

static void StartReadInternal(bool copy_to_ram, u32 output_address, u64 dvd_offset, u32 length,
                              bool decrypt, DVDInterface::ReplyType reply_type,
                              s64 ticks_until_completion);

static void FinishRead(u64 id, s64 cycles_late);
static CoreTiming::EventType* s_finish_read;

static u64 s_next_id = 0;

static std::thread s_dvd_thread;
static Common::Event s_request_queue_expanded;    // Is set by CPU thread
static Common::Event s_result_queue_expanded;     // Is set by DVD thread
static Common::Flag s_dvd_thread_exiting(false);  // Is set by CPU thread

static Common::FifoQueue<ReadRequest, false> s_request_queue;
static Common::FifoQueue<ReadResult, false> s_result_queue;
static std::map<u64, ReadResult> s_result_map;

void Start()
{
  s_finish_read = CoreTiming::RegisterEvent("FinishReadDVDThread", FinishRead);

  s_request_queue_expanded.Reset();
  s_result_queue_expanded.Reset();
  s_request_queue.Clear();
  s_result_queue.Clear();

  // This is reset on every launch for determinism, but it doesn't matter
  // much, because this will never get exposed to the emulated game.
  s_next_id = 0;

  StartDVDThread();
}

static void StartDVDThread()
{
  _assert_(!s_dvd_thread.joinable());
  s_dvd_thread_exiting.Clear();
  s_dvd_thread = std::thread(DVDThread);
}

void Stop()
{
  StopDVDThread();
}

static void StopDVDThread()
{
  _assert_(s_dvd_thread.joinable());

  // By setting s_DVD_thread_exiting, we ask the DVD thread to cleanly exit.
  // In case the request queue is empty, we need to set s_request_queue_expanded
  // so that the DVD thread will wake up and check s_DVD_thread_exiting.
  s_dvd_thread_exiting.Set();
  s_request_queue_expanded.Set();

  s_dvd_thread.join();
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
    s_result_map.emplace(result.first.id, std::move(result));

  // Everything is now in s_result_map, so we simply savestate that.
  // We also savestate s_next_id to avoid ID collisions.
  p.Do(s_result_map);
  p.Do(s_next_id);

  // TODO: Savestates can be smaller if the buffers of results aren't saved,
  // but instead get re-read from the disc when loading the savestate.

  // TODO: It would be possible to create a savestate faster by stopping
  // the DVD thread regardless of whether there are pending requests.

  // After loading a savestate, the debug log in FinishRead will report
  // screwed up times for requests that were submitted before the savestate
  // was made. Handling that properly may be more effort than it's worth.
}

void WaitUntilIdle()
{
  _assert_(Core::IsCPUThread());

  while (!s_request_queue.Empty())
    s_result_queue_expanded.Wait();

  StopDVDThread();
  StartDVDThread();
}

void StartRead(u64 dvd_offset, u32 length, bool decrypt, DVDInterface::ReplyType reply_type,
               s64 ticks_until_completion)
{
  StartReadInternal(false, 0, dvd_offset, length, decrypt, reply_type, ticks_until_completion);
}

void StartReadToEmulatedRAM(u32 output_address, u64 dvd_offset, u32 length, bool decrypt,
                            DVDInterface::ReplyType reply_type, s64 ticks_until_completion)
{
  StartReadInternal(true, output_address, dvd_offset, length, decrypt, reply_type,
                    ticks_until_completion);
}

static void StartReadInternal(bool copy_to_ram, u32 output_address, u64 dvd_offset, u32 length,
                              bool decrypt, DVDInterface::ReplyType reply_type,
                              s64 ticks_until_completion)
{
  _assert_(Core::IsCPUThread());

  ReadRequest request;

  request.copy_to_ram = copy_to_ram;
  request.output_address = output_address;
  request.dvd_offset = dvd_offset;
  request.length = length;
  request.decrypt = decrypt;
  request.reply_type = reply_type;

  u64 id = s_next_id++;
  request.id = id;

  request.time_started_ticks = CoreTiming::GetTicks();
  request.realtime_started_us = Common::Timer::GetTimeUs();

  s_request_queue.Push(std::move(request));
  s_request_queue_expanded.Set();

  CoreTiming::ScheduleEvent(ticks_until_completion, s_finish_read, id);
}

static void FinishRead(u64 id, s64 cycles_late)
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
  auto it = s_result_map.find(id);
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

      if (result.first.id == id)
        break;
      else
        s_result_map.emplace(result.first.id, std::move(result));
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

  if (buffer.empty())
  {
    PanicAlertT("The disc could not be read (at 0x%" PRIx64 " - 0x%" PRIx64 ").",
                request.dvd_offset, request.dvd_offset + request.length);
  }
  else
  {
    if (request.copy_to_ram)
      Memory::CopyToEmu(request.output_address, buffer.data(), request.length);
  }

  // Notify the emulated software that the command has been executed
  DVDInterface::FinishExecutingCommand(request.reply_type, DVDInterface::INT_TCINT, cycles_late,
                                       buffer);
}

static void DVDThread()
{
  Common::SetCurrentThreadName("DVD thread");

  while (true)
  {
    s_request_queue_expanded.Wait();

    if (s_dvd_thread_exiting.IsSet())
      return;

    ReadRequest request;
    while (s_request_queue.Pop(request))
    {
      FileMonitor::Log(request.dvd_offset, request.decrypt);

      std::vector<u8> buffer(request.length);
      const DiscIO::IVolume& volume = DVDInterface::GetVolume();
      if (!volume.Read(request.dvd_offset, request.length, buffer.data(), request.decrypt))
        buffer.resize(0);

      request.realtime_done_us = Common::Timer::GetTimeUs();

      s_result_queue.Push(ReadResult(std::move(request), std::move(buffer)));
      s_result_queue_expanded.Set();

      if (s_dvd_thread_exiting.IsSet())
        return;
    }
  }
}
}
