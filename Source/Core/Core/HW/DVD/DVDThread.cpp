// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/DVD/DVDThread.h"

#include <cinttypes>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/SPSCQueue.h"
#include "Common/Thread.h"
#include "Common/Timer.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/DVD/FileMonitor.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/IOS/ES/Formats.h"

#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"

namespace DVDThread
{
struct ReadRequest
{
  bool copy_to_ram;
  u32 output_address;
  u64 dvd_offset;
  u32 length;
  DiscIO::Partition partition;

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
static void WaitUntilIdle();

static void StartReadInternal(bool copy_to_ram, u32 output_address, u64 dvd_offset, u32 length,
                              const DiscIO::Partition& partition,
                              DVDInterface::ReplyType reply_type, s64 ticks_until_completion);

static void FinishRead(u64 id, s64 cycles_late);
static CoreTiming::EventType* s_finish_read;

static u64 s_next_id = 0;

static std::thread s_dvd_thread;
static Common::Event s_request_queue_expanded;    // Is set by CPU thread
static Common::Event s_result_queue_expanded;     // Is set by DVD thread
static Common::Flag s_dvd_thread_exiting(false);  // Is set by CPU thread

static Common::SPSCQueue<ReadRequest, false> s_request_queue;
static Common::SPSCQueue<ReadResult, false> s_result_queue;
static std::map<u64, ReadResult> s_result_map;

static std::unique_ptr<DiscIO::Volume> s_disc;

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
  ASSERT(!s_dvd_thread.joinable());
  s_dvd_thread_exiting.Clear();
  s_dvd_thread = std::thread(DVDThread);
}

void Stop()
{
  StopDVDThread();
  s_disc.reset();
}

static void StopDVDThread()
{
  ASSERT(s_dvd_thread.joinable());

  // By setting s_DVD_thread_exiting, we ask the DVD thread to cleanly exit.
  // In case the request queue is empty, we need to set s_request_queue_expanded
  // so that the DVD thread will wake up and check s_DVD_thread_exiting.
  s_dvd_thread_exiting.Set();
  s_request_queue_expanded.Set();

  s_dvd_thread.join();
}

void DoState(PointerWrap& p)
{
  // By waiting for the DVD thread to be done working, we ensure
  // that s_request_queue will be empty and that the DVD thread
  // won't be touching anything while this function runs.
  WaitUntilIdle();

  // Move all results from s_result_queue to s_result_map because
  // PointerWrap::Do supports std::map but not Common::SPSCQueue.
  // This won't affect the behavior of FinishRead.
  ReadResult result;
  while (s_result_queue.Pop(result))
    s_result_map.emplace(result.first.id, std::move(result));

  // Both queues are now empty, so we don't need to savestate them.
  p.Do(s_result_map);
  p.Do(s_next_id);

  // s_disc isn't savestated (because it points to files on the
  // local system). Instead, we check that the status of the disc
  // is the same as when the savestate was made. This won't catch
  // cases of having the wrong disc inserted, though.
  // TODO: Check the game ID, disc number, revision?
  bool had_disc = HasDisc();
  p.Do(had_disc);
  if (had_disc != HasDisc())
  {
    if (had_disc)
      PanicAlertT("An inserted disc was expected but not found.");
    else
      s_disc.reset();
  }

  // TODO: Savestates can be smaller if the buffers of results aren't saved,
  // but instead get re-read from the disc when loading the savestate.

  // TODO: It would be possible to create a savestate faster by stopping
  // the DVD thread regardless of whether there are pending requests.

  // After loading a savestate, the debug log in FinishRead will report
  // screwed up times for requests that were submitted before the savestate
  // was made. Handling that properly may be more effort than it's worth.
}

void SetDisc(std::unique_ptr<DiscIO::Volume> disc)
{
  WaitUntilIdle();
  s_disc = std::move(disc);
}

bool HasDisc()
{
  return s_disc != nullptr;
}

bool IsEncryptedAndHashed()
{
  // IsEncryptedAndHashed is thread-safe, so calling WaitUntilIdle isn't necessary.
  return s_disc->IsEncryptedAndHashed();
}

DiscIO::Platform GetDiscType()
{
  // GetVolumeType is thread-safe, so calling WaitUntilIdle isn't necessary.
  return s_disc->GetVolumeType();
}

u64 PartitionOffsetToRawOffset(u64 offset, const DiscIO::Partition& partition)
{
  // PartitionOffsetToRawOffset is thread-safe, so calling WaitUntilIdle isn't necessary.
  return s_disc->PartitionOffsetToRawOffset(offset, partition);
}

IOS::ES::TMDReader GetTMD(const DiscIO::Partition& partition)
{
  WaitUntilIdle();
  return s_disc->GetTMD(partition);
}

IOS::ES::TicketReader GetTicket(const DiscIO::Partition& partition)
{
  WaitUntilIdle();
  return s_disc->GetTicket(partition);
}

bool IsInsertedDiscRunning()
{
  if (!s_disc)
    return false;

  WaitUntilIdle();

  return SConfig::GetInstance().GetGameID() == s_disc->GetGameID();
}

bool UpdateRunningGameMetadata(const DiscIO::Partition& partition, std::optional<u64> title_id)
{
  if (!s_disc)
    return false;

  WaitUntilIdle();

  if (title_id)
  {
    const std::optional<u64> volume_title_id = s_disc->GetTitleID(partition);
    if (!volume_title_id || *volume_title_id != *title_id)
      return false;
  }

  SConfig::GetInstance().SetRunningGameMetadata(*s_disc, partition);
  return true;
}

void WaitUntilIdle()
{
  ASSERT(Core::IsCPUThread());

  while (!s_request_queue.Empty())
    s_result_queue_expanded.Wait();

  StopDVDThread();
  StartDVDThread();
}

void StartRead(u64 dvd_offset, u32 length, const DiscIO::Partition& partition,
               DVDInterface::ReplyType reply_type, s64 ticks_until_completion)
{
  StartReadInternal(false, 0, dvd_offset, length, partition, reply_type, ticks_until_completion);
}

void StartReadToEmulatedRAM(u32 output_address, u64 dvd_offset, u32 length,
                            const DiscIO::Partition& partition, DVDInterface::ReplyType reply_type,
                            s64 ticks_until_completion)
{
  StartReadInternal(true, output_address, dvd_offset, length, partition, reply_type,
                    ticks_until_completion);
}

static void StartReadInternal(bool copy_to_ram, u32 output_address, u64 dvd_offset, u32 length,
                              const DiscIO::Partition& partition,
                              DVDInterface::ReplyType reply_type, s64 ticks_until_completion)
{
  ASSERT(Core::IsCPUThread());

  ReadRequest request;

  request.copy_to_ram = copy_to_ram;
  request.output_address = output_address;
  request.dvd_offset = dvd_offset;
  request.length = length;
  request.partition = partition;
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

  DEBUG_LOG(DVDINTERFACE,
            "Disc has been read. Real time: %" PRIu64 " us. "
            "Real time including delay: %" PRIu64 " us. "
            "Emulated time including delay: %" PRIu64 " us.",
            request.realtime_done_us - request.realtime_started_us,
            Common::Timer::GetTimeUs() - request.realtime_started_us,
            (CoreTiming::GetTicks() - request.time_started_ticks) /
                (SystemTimers::GetTicksPerSecond() / 1000000));

  if (buffer.size() != request.length)
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
      FileMonitor::Log(*s_disc, request.partition, request.dvd_offset);

      std::vector<u8> buffer(request.length);
      if (!s_disc->Read(request.dvd_offset, request.length, buffer.data(), request.partition))
        buffer.resize(0);

      request.realtime_done_us = Common::Timer::GetTimeUs();

      s_result_queue.Push(ReadResult(std::move(request), std::move(buffer)));
      s_result_queue_expanded.Set();

      if (s_dvd_thread_exiting.IsSet())
        return;
    }
  }
}
}  // namespace DVDThread
