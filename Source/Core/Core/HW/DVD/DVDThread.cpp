// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/DVD/DVDThread.h"

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
#include "Core/System.h"

#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"

namespace DVDThread
{
struct ReadRequest
{
  bool copy_to_ram = false;
  u32 output_address = 0;
  u64 dvd_offset = 0;
  u32 length = 0;
  DiscIO::Partition partition{};

  // This determines which code DVDInterface will run to reply
  // to the emulated software. We can't use callbacks,
  // because function pointers can't be stored in savestates.
  DVDInterface::ReplyType reply_type = DVDInterface::ReplyType::NoReply;

  // IDs are used to uniquely identify a request. They must not be
  // identical to IDs of any other requests that currently exist, but
  // it's fine to re-use IDs of requests that have existed in the past.
  u64 id = 0;

  // Only used for logging
  u64 time_started_ticks = 0;
  u64 realtime_started_us = 0;
  u64 realtime_done_us = 0;
};

using ReadResult = std::pair<ReadRequest, std::vector<u8>>;

static void StartDVDThread(DVDThreadState::Data& state);
static void StopDVDThread(DVDThreadState::Data& state);

static void DVDThread();
static void WaitUntilIdle();

static void StartReadInternal(bool copy_to_ram, u32 output_address, u64 dvd_offset, u32 length,
                              const DiscIO::Partition& partition,
                              DVDInterface::ReplyType reply_type, s64 ticks_until_completion);

static void FinishRead(Core::System& system, u64 id, s64 cycles_late);

struct DVDThreadState::Data
{
  CoreTiming::EventType* finish_read;

  u64 next_id = 0;

  std::thread dvd_thread;
  Common::Event request_queue_expanded;                   // Is set by CPU thread
  Common::Event result_queue_expanded;                    // Is set by DVD thread
  Common::Flag dvd_thread_exiting = Common::Flag(false);  // Is set by CPU thread

  Common::SPSCQueue<ReadRequest, false> request_queue;
  Common::SPSCQueue<ReadResult, false> result_queue;
  std::map<u64, ReadResult> result_map;

  std::unique_ptr<DiscIO::Volume> disc;

  FileMonitor::FileLogger file_logger;
};

DVDThreadState::DVDThreadState() : m_data(std::make_unique<Data>())
{
}

DVDThreadState::~DVDThreadState() = default;

void Start()
{
  auto& system = Core::System::GetInstance();
  auto& state = system.GetDVDThreadState().GetData();

  state.finish_read = system.GetCoreTiming().RegisterEvent("FinishReadDVDThread", FinishRead);

  state.request_queue_expanded.Reset();
  state.result_queue_expanded.Reset();
  state.request_queue.Clear();
  state.result_queue.Clear();

  // This is reset on every launch for determinism, but it doesn't matter
  // much, because this will never get exposed to the emulated game.
  state.next_id = 0;

  StartDVDThread(state);
}

static void StartDVDThread(DVDThreadState::Data& state)
{
  ASSERT(!state.dvd_thread.joinable());
  state.dvd_thread_exiting.Clear();
  state.dvd_thread = std::thread(DVDThread);
}

void Stop()
{
  auto& state = Core::System::GetInstance().GetDVDThreadState().GetData();
  StopDVDThread(state);
  state.disc.reset();
}

static void StopDVDThread(DVDThreadState::Data& state)
{
  ASSERT(state.dvd_thread.joinable());

  // By setting dvd_thread_exiting, we ask the DVD thread to cleanly exit.
  // In case the request queue is empty, we need to set request_queue_expanded
  // so that the DVD thread will wake up and check dvd_thread_exiting.
  state.dvd_thread_exiting.Set();
  state.request_queue_expanded.Set();

  state.dvd_thread.join();
}

void DoState(PointerWrap& p)
{
  auto& state = Core::System::GetInstance().GetDVDThreadState().GetData();

  // By waiting for the DVD thread to be done working, we ensure
  // that request_queue will be empty and that the DVD thread
  // won't be touching anything while this function runs.
  WaitUntilIdle();

  // Move all results from result_queue to result_map because
  // PointerWrap::Do supports std::map but not Common::SPSCQueue.
  // This won't affect the behavior of FinishRead.
  ReadResult result;
  while (state.result_queue.Pop(result))
    state.result_map.emplace(result.first.id, std::move(result));

  // Both queues are now empty, so we don't need to savestate them.
  p.Do(state.result_map);
  p.Do(state.next_id);

  // state.disc isn't savestated (because it points to files on the
  // local system). Instead, we check that the status of the disc
  // is the same as when the savestate was made. This won't catch
  // cases of having the wrong disc inserted, though.
  // TODO: Check the game ID, disc number, revision?
  bool had_disc = HasDisc();
  p.Do(had_disc);
  if (had_disc != HasDisc())
  {
    if (had_disc)
      PanicAlertFmtT("An inserted disc was expected but not found.");
    else
      state.disc.reset();
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
  auto& state = Core::System::GetInstance().GetDVDThreadState().GetData();

  WaitUntilIdle();
  state.disc = std::move(disc);
}

bool HasDisc()
{
  auto& state = Core::System::GetInstance().GetDVDThreadState().GetData();
  return state.disc != nullptr;
}

bool HasWiiHashes()
{
  // HasWiiHashes is thread-safe, so calling WaitUntilIdle isn't necessary.
  auto& state = Core::System::GetInstance().GetDVDThreadState().GetData();
  return state.disc->HasWiiHashes();
}

DiscIO::Platform GetDiscType()
{
  // GetVolumeType is thread-safe, so calling WaitUntilIdle isn't necessary.
  auto& state = Core::System::GetInstance().GetDVDThreadState().GetData();
  return state.disc->GetVolumeType();
}

u64 PartitionOffsetToRawOffset(u64 offset, const DiscIO::Partition& partition)
{
  // PartitionOffsetToRawOffset is thread-safe, so calling WaitUntilIdle isn't necessary.
  auto& state = Core::System::GetInstance().GetDVDThreadState().GetData();
  return state.disc->PartitionOffsetToRawOffset(offset, partition);
}

IOS::ES::TMDReader GetTMD(const DiscIO::Partition& partition)
{
  auto& state = Core::System::GetInstance().GetDVDThreadState().GetData();
  WaitUntilIdle();
  return state.disc->GetTMD(partition);
}

IOS::ES::TicketReader GetTicket(const DiscIO::Partition& partition)
{
  auto& state = Core::System::GetInstance().GetDVDThreadState().GetData();
  WaitUntilIdle();
  return state.disc->GetTicket(partition);
}

bool IsInsertedDiscRunning()
{
  auto& state = Core::System::GetInstance().GetDVDThreadState().GetData();

  if (!state.disc)
    return false;

  WaitUntilIdle();

  return SConfig::GetInstance().GetGameID() == state.disc->GetGameID();
}

bool UpdateRunningGameMetadata(const DiscIO::Partition& partition, std::optional<u64> title_id)
{
  auto& state = Core::System::GetInstance().GetDVDThreadState().GetData();

  if (!state.disc)
    return false;

  WaitUntilIdle();

  if (title_id)
  {
    const std::optional<u64> volume_title_id = state.disc->GetTitleID(partition);
    if (!volume_title_id || *volume_title_id != *title_id)
      return false;
  }

  SConfig::GetInstance().SetRunningGameMetadata(*state.disc, partition);
  return true;
}

void WaitUntilIdle()
{
  ASSERT(Core::IsCPUThread());

  auto& state = Core::System::GetInstance().GetDVDThreadState().GetData();

  while (!state.request_queue.Empty())
    state.result_queue_expanded.Wait();

  StopDVDThread(state);
  StartDVDThread(state);
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

  auto& system = Core::System::GetInstance();
  auto& core_timing = system.GetCoreTiming();
  auto& state = system.GetDVDThreadState().GetData();

  ReadRequest request;

  request.copy_to_ram = copy_to_ram;
  request.output_address = output_address;
  request.dvd_offset = dvd_offset;
  request.length = length;
  request.partition = partition;
  request.reply_type = reply_type;

  u64 id = state.next_id++;
  request.id = id;

  request.time_started_ticks = core_timing.GetTicks();
  request.realtime_started_us = Common::Timer::NowUs();

  state.request_queue.Push(std::move(request));
  state.request_queue_expanded.Set();

  core_timing.ScheduleEvent(ticks_until_completion, state.finish_read, id);
}

static void FinishRead(Core::System& system, u64 id, s64 cycles_late)
{
  auto& state = system.GetDVDThreadState().GetData();

  // We can't simply pop result_queue and always get the ReadResult
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
  auto it = state.result_map.find(id);
  if (it != state.result_map.end())
  {
    result = std::move(it->second);
    state.result_map.erase(it);
  }
  else
  {
    while (true)
    {
      while (!state.result_queue.Pop(result))
        state.result_queue_expanded.Wait();

      if (result.first.id == id)
        break;
      else
        state.result_map.emplace(result.first.id, std::move(result));
    }
  }
  // We have now obtained the right ReadResult.

  const ReadRequest& request = result.first;
  const std::vector<u8>& buffer = result.second;

  DEBUG_LOG_FMT(DVDINTERFACE,
                "Disc has been read. Real time: {} us. "
                "Real time including delay: {} us. "
                "Emulated time including delay: {} us.",
                request.realtime_done_us - request.realtime_started_us,
                Common::Timer::NowUs() - request.realtime_started_us,
                (system.GetCoreTiming().GetTicks() - request.time_started_ticks) /
                    (SystemTimers::GetTicksPerSecond() / 1000000));

  DVDInterface::DIInterruptType interrupt;
  if (buffer.size() != request.length)
  {
    PanicAlertFmtT("The disc could not be read (at {0:#x} - {1:#x}).", request.dvd_offset,
                   request.dvd_offset + request.length);

    DVDInterface::SetDriveError(DVDInterface::DriveError::ReadError);
    interrupt = DVDInterface::DIInterruptType::DEINT;
  }
  else
  {
    if (request.copy_to_ram)
    {
      auto& memory = system.GetMemory();
      memory.CopyToEmu(request.output_address, buffer.data(), request.length);
    }

    interrupt = DVDInterface::DIInterruptType::TCINT;
  }

  // Notify the emulated software that the command has been executed
  DVDInterface::FinishExecutingCommand(request.reply_type, interrupt, cycles_late, buffer);
}

static void DVDThread()
{
  auto& state = Core::System::GetInstance().GetDVDThreadState().GetData();

  Common::SetCurrentThreadName("DVD thread");

  while (true)
  {
    state.request_queue_expanded.Wait();

    if (state.dvd_thread_exiting.IsSet())
      return;

    ReadRequest request;
    while (state.request_queue.Pop(request))
    {
      state.file_logger.Log(*state.disc, request.partition, request.dvd_offset);

      std::vector<u8> buffer(request.length);
      if (!state.disc->Read(request.dvd_offset, request.length, buffer.data(), request.partition))
        buffer.resize(0);

      request.realtime_done_us = Common::Timer::NowUs();

      state.result_queue.Push(ReadResult(std::move(request), std::move(buffer)));
      state.result_queue_expanded.Set();

      if (state.dvd_thread_exiting.IsSet())
        return;
    }
  }
}
}  // namespace DVDThread
