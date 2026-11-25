// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/DVD/DVDThread.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/SPSCQueue.h"
#include "Common/Timer.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/DVD/FileMonitor.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/System.h"

#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"

namespace DVD
{
DVDThread::DVDThread(Core::System& system) : m_system(system)
{
}

DVDThread::~DVDThread() = default;

void DVDThread::Start()
{
  m_finish_read = m_system.GetCoreTiming().RegisterEvent("FinishReadDVDThread", GlobalFinishRead);

  // This is reset on every launch for determinism, but it doesn't matter
  // much, because this will never get exposed to the emulated game.
  m_next_id = 0;

  m_dvd_thread.Reset("DVD thread", std::bind_front(&DVDThread::ProcessReadRequest, this));
}

void DVDThread::Stop()
{
  m_dvd_thread.Cancel();
  m_dvd_thread.Shutdown();

  m_result_queue.Clear();
  m_result_map.clear();
  m_pending_reads.clear();
  m_hybrid_lateness_ticks = 0;

  m_disc.reset();
  m_from_optical_drive = false;
}

void DVDThread::DoState(PointerWrap& p)
{
  // Ensure all requests are completed with results Push'd to m_result_queue.
  WaitUntilIdle();

  // Move all results from result_queue to result_map because
  // PointerWrap::Do supports std::map but not Common::WaitableSPSCQueue.
  // This won't affect the behavior of FinishRead.
  ReadResult result;
  while (m_result_queue.Pop(result))
    m_result_map.emplace(result.first.id, std::move(result));

  p.Do(m_result_map);
  p.Do(m_next_id);
  p.Do(m_pending_reads);
  p.Do(m_hybrid_lateness_ticks);

  // m_disc isn't savestated (because it points to files on the
  // local system). Instead, we check that the status of the disc
  // is the same as when the savestate was made. This won't catch
  // cases of having the wrong disc inserted, though.
  // TODO: Check the game ID, disc number, revision?
  bool had_disc = HasDisc();
  p.Do(had_disc);
  if (had_disc != HasDisc())
  {
    if (had_disc)
    {
      PanicAlertFmtT("An inserted disc was expected but not found.");
    }
    else
    {
      m_disc.reset();
      m_from_optical_drive = false;
    }
  }

  // TODO: Savestates can be smaller if the buffers of results aren't saved,
  // but instead get re-read from the disc when loading the savestate.

  // TODO: It would be possible to create a savestate faster by stopping
  // the DVD thread regardless of whether there are pending requests.

  // After loading a savestate, the debug log in FinishRead will report
  // screwed up times for requests that were submitted before the savestate
  // was made. Handling that properly may be more effort than it's worth.
}

void DVDThread::SetDisc(std::unique_ptr<DiscIO::Volume> disc)
{
  WaitUntilIdle();
  m_disc = std::move(disc);

  // Ask the reader, because the blob type can mislead: a Riivolution-patched disc is a directory
  // blob that wraps the drive's reader
  m_from_optical_drive = m_disc && m_disc->GetBlobReader().ReadsFromOpticalDrive();
}

bool DVDThread::HasDisc() const
{
  return m_disc != nullptr;
}

DVDThread::DiscTiming DVDThread::GetDiscTiming() const
{
  if (!m_from_optical_drive)
    return DiscTiming::Deterministic;

  return Config::Get(Config::MAIN_FAST_DISC_SPEED) ? DiscTiming::HostPaced : DiscTiming::Hybrid;
}

bool DVDThread::HasWiiHashes() const
{
  // HasWiiHashes is thread-safe, so calling WaitUntilIdle isn't necessary.
  return m_disc->HasWiiHashes();
}

DiscIO::Platform DVDThread::GetDiscType() const
{
  // GetVolumeType is thread-safe, so calling WaitUntilIdle isn't necessary.
  return m_disc->GetVolumeType();
}

u64 DVDThread::PartitionOffsetToRawOffset(u64 offset, const DiscIO::Partition& partition)
{
  // PartitionOffsetToRawOffset is thread-safe, so calling WaitUntilIdle isn't necessary.
  return m_disc->PartitionOffsetToRawOffset(offset, partition);
}

IOS::ES::TMDReader DVDThread::GetTMD(const DiscIO::Partition& partition)
{
  WaitUntilIdle();
  return m_disc->GetTMD(partition);
}

IOS::ES::TicketReader DVDThread::GetTicket(const DiscIO::Partition& partition)
{
  WaitUntilIdle();
  return m_disc->GetTicket(partition);
}

bool DVDThread::IsInsertedDiscRunning()
{
  if (!m_disc)
    return false;

  WaitUntilIdle();

  return SConfig::GetInstance().GetGameID() == m_disc->GetGameID();
}

bool DVDThread::UpdateRunningGameMetadata(const DiscIO::Partition& partition,
                                          std::optional<u64> title_id)
{
  if (!m_disc)
    return false;

  WaitUntilIdle();

  if (title_id)
  {
    const std::optional<u64> volume_title_id = m_disc->GetTitleID(partition);
    if (!volume_title_id || *volume_title_id != *title_id)
      return false;
  }

  SConfig::GetInstance().SetRunningGameMetadata(*m_disc, partition);
  return true;
}

void DVDThread::WaitUntilIdle()
{
  ASSERT(Core::IsCPUThread());

  m_dvd_thread.WaitForCompletion();
}

void DVDThread::StartRead(u64 dvd_offset, u32 length, const DiscIO::Partition& partition,
                          DVD::ReplyType reply_type, s64 ticks_until_completion)
{
  StartReadInternal(false, 0, dvd_offset, length, partition, reply_type, ticks_until_completion);
}

void DVDThread::StartReadToEmulatedRAM(u32 output_address, u64 dvd_offset, u32 length,
                                       const DiscIO::Partition& partition,
                                       DVD::ReplyType reply_type, s64 ticks_until_completion)
{
  StartReadInternal(true, output_address, dvd_offset, length, partition, reply_type,
                    ticks_until_completion);
}

void DVDThread::StartReadInternal(bool copy_to_ram, u32 output_address, u64 dvd_offset, u32 length,
                                  const DiscIO::Partition& partition, DVD::ReplyType reply_type,
                                  s64 ticks_until_completion)
{
  ASSERT(Core::IsCPUThread());

  auto& core_timing = m_system.GetCoreTiming();

  ReadRequest request;

  request.copy_to_ram = copy_to_ram;
  request.output_address = output_address;
  request.dvd_offset = dvd_offset;
  request.length = length;
  request.partition = partition;
  request.reply_type = reply_type;

  u64 id = m_next_id++;
  request.id = id;

  request.time_started_ticks = core_timing.GetTicks();
  request.realtime_started_us = Common::Timer::NowUs();
  request.from_optical_drive = m_from_optical_drive;
  request.timing = GetDiscTiming();
  request.ticks_until_completion = ticks_until_completion;

  m_dvd_thread.Push(std::move(request));

  // Every read completes through an event scheduled on the CPU thread, which savestates keep. For a
  // read from an optical drive, it first checks whether the data is there (see FinishRead)
  core_timing.ScheduleEvent(ticks_until_completion, m_finish_read, id);
}

void DVDThread::GlobalFinishRead(Core::System& system, u64 id, s64 cycles_late)
{
  system.GetDVDThread().FinishRead(id, cycles_late);
}

bool DVDThread::IsResultReady(u64 id)
{
  ReadResult result;
  while (m_result_queue.Pop(result))
    m_result_map.emplace(result.first.id, std::move(result));
  return m_result_map.contains(id);
}

void DVDThread::FinishRead(u64 id, s64 cycles_late)
{
  // Optical drive reads don't block the CPU thread. A read whose data hasn't arrived is checked
  // again shortly, so it completes soon after the drive returns the data. Reads complete in the
  // order they were issued, even when a later read comes due while an earlier one is still pending.
  // A DI read is split into chunks and only the last one raises the interrupt, so its chunks must
  // not complete out of order. Ejecting or changing the disc keeps the order intact: SetDisc waits
  // for every read in flight, so the previous disc's pending reads already have their data
  const bool held_back = !m_pending_reads.empty() && *m_pending_reads.begin() < id;
  if (held_back || (m_from_optical_drive && !IsResultReady(id)))
  {
    m_pending_reads.insert(id);
    const s64 ticks_per_us = m_system.GetSystemTimers().GetTicksPerSecond() / 1000000;
    m_system.GetCoreTiming().ScheduleEvent(OPTICAL_READ_POLL_INTERVAL_US * ticks_per_us,
                                           m_finish_read, id);
    return;
  }

  m_pending_reads.erase(id);

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
  auto it = m_result_map.find(id);
  if (it != m_result_map.end())
  {
    result = std::move(it->second);
    m_result_map.erase(it);
  }
  else
  {
    while (true)
    {
      m_result_queue.WaitForData();
      m_result_queue.Pop(result);
      if (result.first.id == id)
        break;

      m_result_map.emplace(result.first.id, std::move(result));
    }
  }
  // We have now obtained the right ReadResult.

  // An optical drive read can't complete before its requested time. The disc speed emulation
  // computes it for hybrid reads, and DTK reads request it for host-paced ones. A hybrid DI chunk
  // is pushed back by however late the previous chunk of the same read finished. A host-paced read
  // also lasts at least as long in emulated time as the drive took in real time, which keeps the
  // drive's real latency visible to emulated software when emulation runs slower than real time
  const bool hybrid_di_chunk =
      result.first.timing == DiscTiming::Hybrid && result.first.reply_type != DVD::ReplyType::DTK;
  if (result.first.timing != DiscTiming::Deterministic)
  {
    const ReadRequest& pending = result.first;
    s64 completion_ticks = pending.ticks_until_completion;
    if (pending.timing == DiscTiming::HostPaced)
    {
      const s64 ticks_per_us = m_system.GetSystemTimers().GetTicksPerSecond() / 1000000;
      const s64 drive_ticks =
          static_cast<s64>(pending.realtime_done_us - pending.realtime_started_us) * ticks_per_us;
      completion_ticks = std::max(completion_ticks, drive_ticks);
    }
    else if (hybrid_di_chunk)
    {
      completion_ticks += m_hybrid_lateness_ticks;
    }
    const s64 elapsed_ticks =
        static_cast<s64>(m_system.GetCoreTiming().GetTicks() - pending.time_started_ticks);
    const s64 remaining_ticks = completion_ticks - elapsed_ticks;
    if (remaining_ticks > 0)
    {
      // A hybrid chunk moved later keeps holding back the chunks after it
      if (hybrid_di_chunk)
        m_pending_reads.insert(id);
      m_result_map.emplace(id, std::move(result));
      m_system.GetCoreTiming().ScheduleEvent(remaining_ticks, m_finish_read, id);
      return;
    }
  }

  const ReadRequest& request = result.first;
  const std::vector<u8>& buffer = result.second;

  auto& dvd_interface = m_system.GetDVDInterface();

  // Carry this chunk's lateness over to the rest of the DI read. The last chunk (the one with the
  // reply) resets it and shifts the read-ahead buffer model by the whole read's lateness, because
  // the model assumed the drive started prefetching when the read was due
  if (request.timing != DiscTiming::Deterministic && request.reply_type != DVD::ReplyType::DTK)
  {
    const s64 lateness_ticks =
        hybrid_di_chunk ? std::max<s64>(static_cast<s64>(m_system.GetCoreTiming().GetTicks() -
                                                         request.time_started_ticks) -
                                            request.ticks_until_completion,
                                        0) :
                          0;
    if (request.reply_type == DVD::ReplyType::NoReply)
    {
      m_hybrid_lateness_ticks = lateness_ticks;
    }
    else
    {
      m_hybrid_lateness_ticks = 0;
      if (lateness_ticks > 0)
      {
        dvd_interface.DelayReadBuffer(request.time_started_ticks +
                                          static_cast<u64>(request.ticks_until_completion),
                                      lateness_ticks);
      }
    }
  }

  DEBUG_LOG_FMT(DVDINTERFACE,
                "Disc has been read. Real time: {} us. "
                "Real time including delay: {} us. "
                "Emulated time including delay: {} us.",
                request.realtime_done_us - request.realtime_started_us,
                Common::Timer::NowUs() - request.realtime_started_us,
                (m_system.GetCoreTiming().GetTicks() - request.time_started_ticks) /
                    (m_system.GetSystemTimers().GetTicksPerSecond() / 1000000));

  DVD::DIInterruptType interrupt;
  // Use the read's own flag. m_from_optical_drive may no longer apply, since the emulated disc is
  // ejected when its drive's tray opens, and that can happen while this read is in flight
  if (buffer.size() != request.length && request.from_optical_drive &&
      dvd_interface.IsOpticalDiscRemoved())
  {
    // The disc was taken out of the optical drive itself. Instead of failing the read, report a
    // medium not present error and eject the emulated disc
    dvd_interface.SetDriveError(DVD::DriveError::MediumNotPresent);
    interrupt = DVD::DIInterruptType::DEINT;
    if (dvd_interface.IsDiscInside())
    {
      NOTICE_LOG_FMT(DVDINTERFACE, "The optical disc was removed from its drive, ejecting it");
      dvd_interface.EjectDisc(Core::CPUThreadGuard{m_system}, DVD::EjectCause::User);
    }
  }
  else if (buffer.size() != request.length)
  {
    if (request.from_optical_drive)
    {
      // Skip panic alerts from unreadable areas when using a physical drive
      ERROR_LOG_FMT(DVDINTERFACE, "The optical disc could not be read (at {:#x} - {:#x})",
                    request.dvd_offset, request.dvd_offset + request.length);
    }
    else
    {
      PanicAlertFmtT("The disc could not be read (at {0:#x} - {1:#x}).", request.dvd_offset,
                     request.dvd_offset + request.length);
    }

    dvd_interface.SetDriveError(DVD::DriveError::ReadError);
    interrupt = DVD::DIInterruptType::DEINT;
  }
  else
  {
    if (request.copy_to_ram)
    {
      auto& memory = m_system.GetMemory();
      memory.CopyToEmu(request.output_address, buffer.data(), request.length);
    }

    interrupt = DVD::DIInterruptType::TCINT;
  }

  // Notify the emulated software that the command has been executed
  dvd_interface.FinishExecutingCommand(request.reply_type, interrupt, cycles_late, buffer);
}

void DVDThread::ProcessReadRequest(ReadRequest&& request)
{
  m_file_logger.Log(*m_disc, request.partition, request.dvd_offset);

  std::vector<u8> buffer(request.length);
  if (!m_disc->Read(request.dvd_offset, request.length, buffer.data(), request.partition))
    buffer.resize(0);

  request.realtime_done_us = Common::Timer::NowUs();

  m_result_queue.Push(ReadResult(std::move(request), std::move(buffer)));
}
}  // namespace DVD
