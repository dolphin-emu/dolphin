// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/SPSCQueue.h"

#include "Common/WorkQueueThread.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/DVD/FileMonitor.h"

#include "DiscIO/Volume.h"

class PointerWrap;
namespace Core
{
class System;
}
namespace CoreTiming
{
struct EventType;
}
namespace DiscIO
{
struct Partition;
}

namespace DiscIO
{
enum class Platform;
class Volume;
}  // namespace DiscIO

namespace IOS::ES
{
class TMDReader;
class TicketReader;
}  // namespace IOS::ES

namespace DVD
{
enum class ReplyType : u32;

class DVDThread
{
public:
  explicit DVDThread(Core::System& system);
  DVDThread(const DVDThread&) = delete;
  DVDThread(DVDThread&&) = delete;
  DVDThread& operator=(const DVDThread&) = delete;
  DVDThread& operator=(DVDThread&&) = delete;
  ~DVDThread();

  void Start();
  void Stop();
  void DoState(PointerWrap& p);

  void SetDisc(std::unique_ptr<DiscIO::Volume> disc);
  bool HasDisc() const;

  enum class DiscTiming : u8
  {
    // Used by disc images: completes at the time computed by the disc speed emulation, and waits
    // for the host read if it takes longer, blocking the CPU thread until the data is there
    Deterministic,

    // Used by optical discs with "Emulate Disc Speed" enabled: completes at the emulated time or
    // when the drive returns the data, whichever is later. Doesn't block the CPU thread
    Hybrid,

    // Used by optical discs with "Emulate Disc Speed" disabled: no disc speed emulation. The drive
    // paces the read, which completes once the drive returns the data and at least as much emulated
    // time has passed as the drive took in real time. Doesn't block the CPU thread
    HostPaced,
  };
  DiscTiming GetDiscTiming() const;

  bool HasWiiHashes() const;
  DiscIO::Platform GetDiscType() const;
  u64 PartitionOffsetToRawOffset(u64 offset, const DiscIO::Partition& partition);
  IOS::ES::TMDReader GetTMD(const DiscIO::Partition& partition);
  IOS::ES::TicketReader GetTicket(const DiscIO::Partition& partition);
  bool IsInsertedDiscRunning();

  // This function returns true and calls SConfig::SetRunningGameMetadata(Volume&, Partition&)
  // if both of the following conditions are true:
  // - A disc is inserted
  // - The title_id argument doesn't contain a value, or its value matches the disc's title ID
  bool UpdateRunningGameMetadata(const DiscIO::Partition& partition,
                                 std::optional<u64> title_id = {});

  void StartRead(u64 dvd_offset, u32 length, const DiscIO::Partition& partition,
                 DVD::ReplyType reply_type, s64 ticks_until_completion);
  void StartReadToEmulatedRAM(u32 output_address, u64 dvd_offset, u32 length,
                              const DiscIO::Partition& partition, DVD::ReplyType reply_type,
                              s64 ticks_until_completion);

private:
  void WaitUntilIdle();

  void StartReadInternal(bool copy_to_ram, u32 output_address, u64 dvd_offset, u32 length,
                         const DiscIO::Partition& partition, DVD::ReplyType reply_type,
                         s64 ticks_until_completion);

  static void GlobalFinishRead(Core::System& system, u64 id, s64 cycles_late);
  void FinishRead(u64 id, s64 cycles_late);

  // Moves all results the DVD thread has finished into m_result_map without waiting. Returns
  // whether the result for id is in the map
  bool IsResultReady(u64 id);

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
    DVD::ReplyType reply_type = DVD::ReplyType::NoReply;

    // IDs are used to uniquely identify a request. They must not be
    // identical to IDs of any other requests that currently exist, but
    // it's fine to re-use IDs of requests that have existed in the past.
    u64 id = 0;

    // Used for logging. Host-paced reads also use it to make the read's emulated duration match
    // the real time the drive took
    u64 time_started_ticks = 0;
    u64 realtime_started_us = 0;
    u64 realtime_done_us = 0;

    // Set for a read originating from an optical drive
    bool from_optical_drive = false;

    // With any timing other than Deterministic, an optical drive read can't complete before
    // time_started_ticks + ticks_until_completion
    DiscTiming timing = DiscTiming::Deterministic;
    s64 ticks_until_completion = 0;
  };

  // How often, in emulated time, to check on an optical drive read whose data hasn't arrived yet
  static constexpr s64 OPTICAL_READ_POLL_INTERVAL_US = 100;

  void ProcessReadRequest(ReadRequest&& read_request);

  using ReadResult = std::pair<ReadRequest, std::vector<u8>>;

  CoreTiming::EventType* m_finish_read = nullptr;

  u64 m_next_id = 0;

  Common::WorkQueueThreadSP<ReadRequest> m_dvd_thread;

  Common::WaitableSPSCQueue<ReadResult> m_result_queue;
  std::map<u64, ReadResult> m_result_map;

  // Optical drive reads held back so that drive reads complete in the order they were issued.
  // This covers reads that came due before their data arrived, the later reads held behind them,
  // and hybrid reads delayed by m_hybrid_lateness_ticks
  std::set<u64> m_pending_reads;

  // How far behind the timing model the last completed chunk of the in-progress hybrid DI read
  // completed. The read's remaining chunks are delayed by the same amount, so one late chunk
  // can't make the chunks after it arrive closer together than the model allows. The read's
  // last chunk resets it
  s64 m_hybrid_lateness_ticks = 0;

  std::unique_ptr<DiscIO::Volume> m_disc;

  // True when m_disc is read from an optical drive
  bool m_from_optical_drive = false;

  FileMonitor::FileLogger m_file_logger;

  Core::System& m_system;
};
}  // namespace DVD
