// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
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

  // Only used for logging
  u64 time_started_ticks;
  u64 realtime_started_us;
  u64 realtime_done_us;
};

using ReadResult = std::pair<ReadRequest, std::vector<u8>>;

static void DVDThread();

static void FinishRead(u64 userdata, s64 cycles_late);
static int s_finish_read;

static std::thread s_dvd_thread;
static Common::Event s_dvd_thread_start_working;
static Common::Event s_dvd_thread_done_working;
static Common::Flag s_dvd_thread_exiting(false);

static ReadRequest s_read_request;
static ReadResult s_read_result;

void Start()
{
  s_finish_read = CoreTiming::RegisterEvent("FinishReadDVDThread", FinishRead);
  _assert_(!s_dvd_thread.joinable());
  s_dvd_thread = std::thread(DVDThread);
}

void Stop()
{
  _assert_(s_dvd_thread.joinable());

  // The DVD thread will return if s_DVD_thread_exiting
  // is set when it starts working
  s_dvd_thread_exiting.Set();
  s_dvd_thread_start_working.Set();

  s_dvd_thread.join();

  s_dvd_thread_exiting.Clear();
}

void DoState(PointerWrap& p)
{
  // By waiting for the DVD thread to be done working, we ensure
  // that the value of s_read_request won't be used again.
  // We thus don't need to save or load s_read_request here.
  WaitUntilIdle();

  // TODO: Savestates can be smaller if s_read_result.buffer isn't saved,
  // but instead gets re-read from the disc when loading the savestate.
  p.Do(s_read_result);

  // After loading a savestate, the debug log in FinishRead will report
  // screwed up times for requests that were submitted before the savestate
  // was made. Handling that properly may be more effort than it's worth.
}

void WaitUntilIdle()
{
  _assert_(Core::IsCPUThread());

  // Wait until DVD thread isn't working
  s_dvd_thread_done_working.Wait();

  // Set the event again so that we still know that the DVD thread isn't working
  s_dvd_thread_done_working.Set();
}

void StartRead(u64 dvd_offset, u32 output_address, u32 length, bool decrypt, bool reply_to_ios,
               int ticks_until_completion)
{
  _assert_(Core::IsCPUThread());

  s_dvd_thread_done_working.Wait();

  ReadRequest request;

  request.dvd_offset = dvd_offset;
  request.output_address = output_address;
  request.length = length;
  request.decrypt = decrypt;
  request.reply_to_ios = reply_to_ios;

  request.time_started_ticks = CoreTiming::GetTicks();
  request.realtime_started_us = Common::Timer::GetTimeUs();

  s_read_request = std::move(request);

  s_dvd_thread_start_working.Set();

  CoreTiming::ScheduleEvent(ticks_until_completion, s_finish_read);
}

static void FinishRead(u64 userdata, s64 cycles_late)
{
  WaitUntilIdle();

  const ReadResult result = std::move(s_read_result);
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

    s_dvd_thread_start_working.Wait();

    if (s_dvd_thread_exiting.IsSet())
      return;

    ReadRequest request = std::move(s_read_request);

    std::vector<u8> buffer(request.length);
    const DiscIO::IVolume& volume = DVDInterface::GetVolume();
    if (!volume.Read(request.dvd_offset, request.length, buffer.data(), request.decrypt))
      buffer.resize(0);

    request.realtime_done_us = Common::Timer::GetTimeUs();

    s_read_result = ReadResult(std::move(request), std::move(buffer));
  }
}
}
