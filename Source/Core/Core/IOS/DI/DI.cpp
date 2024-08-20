// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/DI/DI.h"

#include <memory>
#include <vector>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Core/Config/SessionSettings.h"
#include "Core/CoreTiming.h"
#include "Core/DolphinAnalytics.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/DVD/DVDThread.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/WII_IPC.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/System.h"
#include "DiscIO/Volume.h"

constexpr u32 ADDRESS_DISR = 0x0D806000;
constexpr u32 ADDRESS_DICVR = 0x0D806004;
constexpr u32 ADDRESS_DICMDBUF0 = 0x0D806008;
constexpr u32 ADDRESS_DICMDBUF1 = 0x0D80600C;
constexpr u32 ADDRESS_DICMDBUF2 = 0x0D806010;
constexpr u32 ADDRESS_DIMAR = 0x0D806014;
constexpr u32 ADDRESS_DILENGTH = 0x0D806018;
constexpr u32 ADDRESS_DICR = 0x0D80601C;
constexpr u32 ADDRESS_DIIMMBUF = 0x0D806020;

constexpr u32 ADDRESS_HW_GPIO_OUT = 0x0D8000E0;
constexpr u32 ADDRESS_HW_RESETS = 0x0D800194;

namespace IOS::HLE
{
CoreTiming::EventType* DIDevice::s_finish_executing_di_command;

DIDevice::DIDevice(EmulationKernel& ios, const std::string& device_name)
    : EmulationDevice(ios, device_name)
{
}

void DIDevice::DoState(PointerWrap& p)
{
  Device::DoState(p);
  p.Do(m_commands_to_execute);
  p.Do(m_executing_command);
  p.Do(m_current_partition);
  p.Do(m_has_initialized);
  p.Do(m_last_length);
}

std::optional<IPCReply> DIDevice::Open(const OpenRequest& request)
{
  InitializeIfFirstTime();
  return Device::Open(request);
}

std::optional<IPCReply> DIDevice::IOCtl(const IOCtlRequest& request)
{
  InitializeIfFirstTime();

  // DI IOCtls are handled in a special way by Dolphin compared to other IOS functions.
  // Many of them use DVDInterface's ExecuteCommand, which will execute commands more or less
  // asynchronously. The rest are also treated as async to match this.  Only one command can be
  // executed at a time, so commands are queued until DVDInterface is ready to handle them.

  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  const u8 command = memory.Read_U8(request.buffer_in);
  if (request.request != command)
  {
    WARN_LOG_FMT(IOS_DI,
                 "IOCtl: Received conflicting commands: ioctl {:#04x}, buffer {:#04x}.  Using "
                 "ioctl command.",
                 request.request, command);
  }

  const bool ready_to_execute = !m_executing_command.has_value();
  m_commands_to_execute.push_back(request.address);

  if (ready_to_execute)
  {
    ProcessQueuedIOCtl();
  }

  // FinishIOCtl will be called after the command has been executed
  // to reply to the request, so we shouldn't reply here.
  return std::nullopt;
}

void DIDevice::ProcessQueuedIOCtl()
{
  if (m_commands_to_execute.empty())
  {
    PanicAlertFmt("IOS::HLE::DIDevice: There is no command to execute!");
    return;
  }

  m_executing_command = {m_commands_to_execute.front()};
  m_commands_to_execute.pop_front();

  auto& system = GetSystem();
  IOCtlRequest request{system, m_executing_command->m_request_address};
  if (auto finished = StartIOCtl(request))
  {
    system.GetCoreTiming().ScheduleEvent(IPC_OVERHEAD_TICKS, s_finish_executing_di_command,
                                         static_cast<u64>(finished.value()));
    return;
  }
}

std::optional<DIDevice::DIResult> DIDevice::WriteIfFits(const IOCtlRequest& request, u32 value)
{
  if (request.buffer_out_size < 4)
  {
    WARN_LOG_FMT(IOS_DI, "Output buffer is too small to contain result; returning security error");
    return DIResult::SecurityError;
  }
  else
  {
    auto& system = GetSystem();
    auto& memory = system.GetMemory();
    memory.Write_U32(value, request.buffer_out);
    return DIResult::Success;
  }
}

namespace
{
struct DiscRange
{
  u32 start;
  u32 end;
  bool is_error_001_range;
};
}  // namespace

std::optional<DIDevice::DIResult> DIDevice::StartIOCtl(const IOCtlRequest& request)
{
  if (request.buffer_in_size != 0x20)
  {
    ERROR_LOG_FMT(IOS_DI, "IOCtl: Received bad input buffer size {:#04x}, should be 0x20",
                  request.buffer_in_size);
    return DIResult::SecurityError;
  }

  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  auto* mmio = memory.GetMMIOMapping();

  // DVDInterface's ExecuteCommand handles most of the work for most of these.
  // The IOCtl callback is used to generate a reply afterwards.
  switch (static_cast<DIIoctl>(request.request))
  {
  case DIIoctl::DVDLowInquiry:
    INFO_LOG_FMT(IOS_DI, "DVDLowInquiry");
    mmio->Write<u32>(system, ADDRESS_DICMDBUF0, 0x12000000);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF1, 0);
    return StartDMATransfer(0x20, request);
  case DIIoctl::DVDLowReadDiskID:
    INFO_LOG_FMT(IOS_DI, "DVDLowReadDiskID");
    mmio->Write<u32>(system, ADDRESS_DICMDBUF0, 0xA8000040);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF1, 0);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF2, 0x20);
    return StartDMATransfer(0x20, request);
    // TODO: Include an additional read that happens on Wii discs, or at least
    // emulate its side effect of disabling DTK configuration
    // if (memory.Read_U32(request.buffer_out + 24) == 0x5d1c9ea3) { // Wii Magic
    //   if (!m_has_read_encryption_info) {
    //     // Read 0x44 (=> 0x60) bytes starting from offset 8 or byte 0x20;
    //     // byte 0x60 is disable hashing and byte 0x61 is disable encryption
    //   }
    // }
  case DIIoctl::DVDLowRead:
  {
    const u32 length = memory.Read_U32(request.buffer_in + 4);
    const u32 position = memory.Read_U32(request.buffer_in + 8);
    INFO_LOG_FMT(IOS_DI, "DVDLowRead: offset {:#010x} (byte {:#011x}), length {:#x}", position,
                 static_cast<u64>(position) << 2, length);
    if (m_current_partition == DiscIO::PARTITION_NONE)
    {
      ERROR_LOG_FMT(IOS_DI, "Attempted to perform a decrypting read when no partition is open!");
      return DIResult::SecurityError;
    }
    if (request.buffer_out_size < length)
    {
      WARN_LOG_FMT(
          IOS_DI,
          "Output buffer is too small for the result of the read ({} bytes given, needed at "
          "least {}); returning security error",
          request.buffer_out_size, length);
      return DIResult::SecurityError;
    }
    m_last_length = position;  // An actual mistake in IOS
    system.GetDVDInterface().PerformDecryptingRead(position, length, request.buffer_out,
                                                   m_current_partition, DVD::ReplyType::IOS);
    return {};
  }
  case DIIoctl::DVDLowWaitForCoverClose:
    // This is a bit awkward to implement, as it doesn't return for a long time, so just act as if
    // the cover was immediately closed
    INFO_LOG_FMT(IOS_DI, "DVDLowWaitForCoverClose - skipping");
    return DIResult::CoverClosed;
  case DIIoctl::DVDLowGetCoverRegister:
  {
    const u32 dicvr = mmio->Read<u32>(system, ADDRESS_DICVR);
    DEBUG_LOG_FMT(IOS_DI, "DVDLowGetCoverRegister {:#010x}", dicvr);
    return WriteIfFits(request, dicvr);
  }
  case DIIoctl::DVDLowNotifyReset:
    INFO_LOG_FMT(IOS_DI, "DVDLowNotifyReset");
    ResetDIRegisters();
    return DIResult::Success;
  case DIIoctl::DVDLowSetSpinupFlag:
    ERROR_LOG_FMT(IOS_DI, "DVDLowSetSpinupFlag - not a valid command, rejecting");
    return DIResult::BadArgument;
  case DIIoctl::DVDLowReadDvdPhysical:
  {
    const u8 position = memory.Read_U8(request.buffer_in + 7);
    INFO_LOG_FMT(IOS_DI, "DVDLowReadDvdPhysical: position {:#04x}", position);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF0, 0xAD000000 | (position << 8));
    mmio->Write<u32>(system, ADDRESS_DICMDBUF1, 0);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF2, 0);
    return StartDMATransfer(0x800, request);
  }
  case DIIoctl::DVDLowReadDvdCopyright:
  {
    const u8 position = memory.Read_U8(request.buffer_in + 7);
    INFO_LOG_FMT(IOS_DI, "DVDLowReadDvdCopyright: position {:#04x}", position);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF0, 0xAD010000 | (position << 8));
    mmio->Write<u32>(system, ADDRESS_DICMDBUF1, 0);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF2, 0);
    return StartImmediateTransfer(request);
  }
  case DIIoctl::DVDLowReadDvdDiscKey:
  {
    const u8 position = memory.Read_U8(request.buffer_in + 7);
    INFO_LOG_FMT(IOS_DI, "DVDLowReadDvdDiscKey: position {:#04x}", position);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF0, 0xAD020000 | (position << 8));
    mmio->Write<u32>(system, ADDRESS_DICMDBUF1, 0);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF2, 0);
    return StartDMATransfer(0x800, request);
  }
  case DIIoctl::DVDLowGetLength:
    INFO_LOG_FMT(IOS_DI, "DVDLowGetLength {:#010x}", m_last_length);
    return WriteIfFits(request, m_last_length);
  case DIIoctl::DVDLowGetImmBuf:
  {
    const u32 diimmbuf = mmio->Read<u32>(system, ADDRESS_DIIMMBUF);
    INFO_LOG_FMT(IOS_DI, "DVDLowGetImmBuf {:#010x}", diimmbuf);
    return WriteIfFits(request, diimmbuf);
  }
  case DIIoctl::DVDLowMaskCoverInterrupt:
    INFO_LOG_FMT(IOS_DI, "DVDLowMaskCoverInterrupt");
    system.GetDVDInterface().SetInterruptEnabled(DVD::DIInterruptType::CVRINT, false);
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DI_INTERRUPT_MASK_COMMAND);
    return DIResult::Success;
  case DIIoctl::DVDLowClearCoverInterrupt:
    DEBUG_LOG_FMT(IOS_DI, "DVDLowClearCoverInterrupt");
    system.GetDVDInterface().ClearInterrupt(DVD::DIInterruptType::CVRINT);
    return DIResult::Success;
  case DIIoctl::DVDLowUnmaskStatusInterrupts:
    INFO_LOG_FMT(IOS_DI, "DVDLowUnmaskStatusInterrupts");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DI_INTERRUPT_MASK_COMMAND);
    // Dummied out
    return DIResult::Success;
  case DIIoctl::DVDLowGetCoverStatus:
  {
    // TODO: handle resetting case
    const bool is_disc_inside = system.GetDVDInterface().IsDiscInside();
    INFO_LOG_FMT(IOS_DI, "DVDLowGetCoverStatus: Disc {}Inserted", is_disc_inside ? "" : "Not ");
    return WriteIfFits(request, is_disc_inside ? 2 : 1);
  }
  case DIIoctl::DVDLowUnmaskCoverInterrupt:
    INFO_LOG_FMT(IOS_DI, "DVDLowUnmaskCoverInterrupt");
    system.GetDVDInterface().SetInterruptEnabled(DVD::DIInterruptType::CVRINT, true);
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DI_INTERRUPT_MASK_COMMAND);
    return DIResult::Success;
  case DIIoctl::DVDLowReset:
  {
    const bool spinup = memory.Read_U32(request.buffer_in + 4);

    // The GPIO *disables* spinning up the drive.  Normally handled via syscall 0x4e.
    const u32 old_gpio = mmio->Read<u32>(system, ADDRESS_HW_GPIO_OUT);
    if (spinup)
      mmio->Write<u32>(system, ADDRESS_HW_GPIO_OUT, old_gpio & ~static_cast<u32>(GPIO::DI_SPIN));
    else
      mmio->Write<u32>(system, ADDRESS_HW_GPIO_OUT, old_gpio | static_cast<u32>(GPIO::DI_SPIN));

    // Syscall 0x46 check_di_reset
    const bool was_resetting = (mmio->Read<u32>(system, ADDRESS_HW_RESETS) & (1 << 10)) == 0;
    if (was_resetting)
    {
      // This route will not generally be taken in Dolphin but is included for completeness
      // Syscall 0x45 deassert_di_reset
      mmio->Write<u32>(system, ADDRESS_HW_RESETS,
                       mmio->Read<u32>(system, ADDRESS_HW_RESETS) | (1 << 10));
    }
    else
    {
      // Syscall 0x44 assert_di_reset
      mmio->Write<u32>(system, ADDRESS_HW_RESETS,
                       mmio->Read<u32>(system, ADDRESS_HW_RESETS) & ~(1 << 10));
      // Normally IOS sleeps for 12 microseconds here, but we can't easily emulate that
      // Syscall 0x45 deassert_di_reset
      mmio->Write<u32>(system, ADDRESS_HW_RESETS,
                       mmio->Read<u32>(system, ADDRESS_HW_RESETS) | (1 << 10));
    }
    ResetDIRegisters();
    return DIResult::Success;
  }
  case DIIoctl::DVDLowOpenPartition:
    ERROR_LOG_FMT(IOS_DI, "DVDLowOpenPartition as an ioctl - rejecting");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    return DIResult::SecurityError;
  case DIIoctl::DVDLowClosePartition:
    INFO_LOG_FMT(IOS_DI, "DVDLowClosePartition");
    ChangePartition(DiscIO::PARTITION_NONE);
    return DIResult::Success;
  case DIIoctl::DVDLowUnencryptedRead:
  {
    const u32 length = memory.Read_U32(request.buffer_in + 4);
    const u32 position = memory.Read_U32(request.buffer_in + 8);
    const u32 end = position + (length >> 2);  // a 32-bit offset
    INFO_LOG_FMT(IOS_DI, "DVDLowUnencryptedRead: offset {:#010x} (byte {:#011x}), length {:#x}",
                 position, static_cast<u64>(position) << 2, length);
    // (start, end) as 32-bit offsets
    // Older IOS versions only accept the first range.  Later versions added the extra ranges to
    // check how the drive responds to out of bounds reads (an error 001 check).
    constexpr std::array<DiscRange, 3> valid_ranges = {
        DiscRange{0, 0x14000, false},  // "System area"
        DiscRange{0x460A0000, 0x460A0008, true},
        DiscRange{0x7ED40000, 0x7ED40008, true},
    };
    for (auto range : valid_ranges)
    {
      if (range.start <= position && position <= range.end && range.start <= end &&
          end <= range.end)
      {
        mmio->Write<u32>(system, ADDRESS_DICMDBUF0, 0xA8000000);
        mmio->Write<u32>(system, ADDRESS_DICMDBUF1, position);
        mmio->Write<u32>(system, ADDRESS_DICMDBUF2, length);
        if (range.is_error_001_range && Config::Get(Config::SESSION_SHOULD_FAKE_ERROR_001))
        {
          mmio->Write<u32>(system, ADDRESS_DIMAR, request.buffer_out);
          m_last_length = length;
          mmio->Write<u32>(system, ADDRESS_DILENGTH, length);
          system.GetDVDInterface().ForceOutOfBoundsRead(DVD::ReplyType::IOS);
          return {};
        }
        else
        {
          return StartDMATransfer(length, request);
        }
      }
    }
    WARN_LOG_FMT(IOS_DI, "DVDLowUnencryptedRead: trying to read from an illegal region!");
    return DIResult::SecurityError;
  }
  case DIIoctl::DVDLowEnableDvdVideo:
    ERROR_LOG_FMT(IOS_DI, "DVDLowEnableDvdVideo - rejecting");
    return DIResult::SecurityError;
  // There are a bunch of ioctlvs that are also (unintentionally) usable as ioctls; reject these in
  // Dolphin as games are unlikely to use them.
  case DIIoctl::DVDLowGetNoDiscOpenPartitionParams:
    ERROR_LOG_FMT(IOS_DI, "DVDLowGetNoDiscOpenPartitionParams as an ioctl - rejecting");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    return DIResult::SecurityError;
  case DIIoctl::DVDLowNoDiscOpenPartition:
    ERROR_LOG_FMT(IOS_DI, "DVDLowNoDiscOpenPartition as an ioctl - rejecting");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    return DIResult::SecurityError;
  case DIIoctl::DVDLowGetNoDiscBufferSizes:
    ERROR_LOG_FMT(IOS_DI, "DVDLowGetNoDiscBufferSizes as an ioctl - rejecting");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    return DIResult::SecurityError;
  case DIIoctl::DVDLowOpenPartitionWithTmdAndTicket:
    ERROR_LOG_FMT(IOS_DI, "DVDLowOpenPartitionWithTmdAndTicket as an ioctl - rejecting");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    return DIResult::SecurityError;
  case DIIoctl::DVDLowOpenPartitionWithTmdAndTicketView:
    ERROR_LOG_FMT(IOS_DI, "DVDLowOpenPartitionWithTmdAndTicketView as an ioctl - rejecting");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    return DIResult::SecurityError;
  case DIIoctl::DVDLowGetStatusRegister:
  {
    const u32 disr = mmio->Read<u32>(system, ADDRESS_DISR);
    INFO_LOG_FMT(IOS_DI, "DVDLowGetStatusRegister: {:#010x}", disr);
    return WriteIfFits(request, disr);
  }
  case DIIoctl::DVDLowGetControlRegister:
  {
    const u32 dicr = mmio->Read<u32>(system, ADDRESS_DICR);
    INFO_LOG_FMT(IOS_DI, "DVDLowGetControlRegister: {:#010x}", dicr);
    return WriteIfFits(request, dicr);
  }
  case DIIoctl::DVDLowReportKey:
  {
    const u8 param1 = memory.Read_U8(request.buffer_in + 7);
    const u32 param2 = memory.Read_U32(request.buffer_in + 8);
    INFO_LOG_FMT(IOS_DI, "DVDLowReportKey: param1 {:#04x}, param2 {:#08x}", param1, param2);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF0, 0xA4000000 | (param1 << 16));
    mmio->Write<u32>(system, ADDRESS_DICMDBUF1, param2 & 0xFFFFFF);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF2, 0);
    return StartDMATransfer(0x20, request);
  }
  case DIIoctl::DVDLowSeek:
  {
    const u32 position = memory.Read_U32(request.buffer_in + 4);  // 32-bit offset
    INFO_LOG_FMT(IOS_DI, "DVDLowSeek: position {:#010x}, translated to {:#010x}", position,
                 position);  // TODO: do partition translation!
    mmio->Write<u32>(system, ADDRESS_DICMDBUF0, 0xAB000000);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF1, position);
    return StartImmediateTransfer(request, false);
  }
  case DIIoctl::DVDLowReadDvd:
  {
    const u8 flag1 = memory.Read_U8(request.buffer_in + 7);
    const u8 flag2 = memory.Read_U8(request.buffer_in + 11);
    const u32 length = memory.Read_U32(request.buffer_in + 12);
    const u32 position = memory.Read_U32(request.buffer_in + 16);
    INFO_LOG_FMT(IOS_DI, "DVDLowReadDvd({}, {}): position {:#08x}, length {:#08x}", flag1, flag2,
                 position, length);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF0,
                     0xD0000000 | ((flag1 & 1) << 7) | ((flag2 & 1) << 6));
    mmio->Write<u32>(system, ADDRESS_DICMDBUF1, position & 0xFFFFFF);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF2, length & 0xFFFFFF);
    return StartDMATransfer(0x800 * length, request);
  }
  case DIIoctl::DVDLowReadDvdConfig:
  {
    const u8 flag1 = memory.Read_U8(request.buffer_in + 7);
    const u8 param2 = memory.Read_U8(request.buffer_in + 11);
    const u32 position = memory.Read_U32(request.buffer_in + 12);
    INFO_LOG_FMT(IOS_DI, "DVDLowReadDvdConfig({}, {}): position {:#08x}", flag1, param2, position);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF0, 0xD1000000 | ((flag1 & 1) << 16) | param2);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF1, position & 0xFFFFFF);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF2, 0);
    return StartImmediateTransfer(request);
  }
  case DIIoctl::DVDLowStopLaser:
    INFO_LOG_FMT(IOS_DI, "DVDLowStopLaser");
    mmio->Write<u32>(system, ADDRESS_DICMDBUF0, 0xD2000000);
    return StartImmediateTransfer(request);
  case DIIoctl::DVDLowOffset:
  {
    const u8 flag = memory.Read_U8(request.buffer_in + 7);
    const u32 offset = memory.Read_U32(request.buffer_in + 8);
    INFO_LOG_FMT(IOS_DI, "DVDLowOffset({}): offset {:#010x}", flag, offset);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF0, 0xD9000000 | ((flag & 1) << 16));
    mmio->Write<u32>(system, ADDRESS_DICMDBUF1, offset);
    return StartImmediateTransfer(request);
  }
  case DIIoctl::DVDLowReadDiskBca:
    INFO_LOG_FMT(IOS_DI, "DVDLowReadDiskBca");
    mmio->Write<u32>(system, ADDRESS_DICMDBUF0, 0xDA000000);
    return StartDMATransfer(0x40, request);
  case DIIoctl::DVDLowRequestDiscStatus:
    INFO_LOG_FMT(IOS_DI, "DVDLowRequestDiscStatus");
    mmio->Write<u32>(system, ADDRESS_DICMDBUF0, 0xDB000000);
    return StartImmediateTransfer(request);
  case DIIoctl::DVDLowRequestRetryNumber:
    INFO_LOG_FMT(IOS_DI, "DVDLowRequestRetryNumber");
    mmio->Write<u32>(system, ADDRESS_DICMDBUF0, 0xDC000000);
    return StartImmediateTransfer(request);
  case DIIoctl::DVDLowSetMaximumRotation:
  {
    const u8 speed = memory.Read_U8(request.buffer_in + 7);
    INFO_LOG_FMT(IOS_DI, "DVDLowSetMaximumRotation: speed {}", speed);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF0, 0xDD000000 | ((speed & 3) << 16));
    return StartImmediateTransfer(request, false);
  }
  case DIIoctl::DVDLowSerMeasControl:
  {
    const u8 flag1 = memory.Read_U8(request.buffer_in + 7);
    const u8 flag2 = memory.Read_U8(request.buffer_in + 11);
    INFO_LOG_FMT(IOS_DI, "DVDLowSerMeasControl({}, {})", flag1, flag2);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF0,
                     0xDF000000 | ((flag1 & 1) << 17) | ((flag2 & 1) << 16));
    return StartDMATransfer(0x20, request);
  }
  case DIIoctl::DVDLowRequestError:
    INFO_LOG_FMT(IOS_DI, "DVDLowRequestError");
    mmio->Write<u32>(system, ADDRESS_DICMDBUF0, 0xE0000000);
    return StartImmediateTransfer(request);
  case DIIoctl::DVDLowAudioStream:
  {
    const u8 mode = memory.Read_U8(request.buffer_in + 7);
    const u32 length = memory.Read_U32(request.buffer_in + 8);
    const u32 position = memory.Read_U32(request.buffer_in + 12);
    INFO_LOG_FMT(IOS_DI, "DVDLowAudioStream({}): offset {:#010x} (byte {:#011x}), length {:#x}",
                 mode, position, static_cast<u64>(position) << 2, length);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF0, 0xE1000000 | ((mode & 3) << 16));
    mmio->Write<u32>(system, ADDRESS_DICMDBUF1, position);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF2, length);
    return StartImmediateTransfer(request, false);
  }
  case DIIoctl::DVDLowRequestAudioStatus:
  {
    const u8 mode = memory.Read_U8(request.buffer_in + 7);
    INFO_LOG_FMT(IOS_DI, "DVDLowRequestAudioStatus({})", mode);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF0, 0xE2000000 | ((mode & 3) << 16));
    mmio->Write<u32>(system, ADDRESS_DICMDBUF1, 0);
    // Note that this command does not copy the value written to DIIMMBUF, which makes it rather
    // useless (to actually use it, DVDLowGetImmBuf would need to be used afterwards)
    return StartImmediateTransfer(request, false);
  }
  case DIIoctl::DVDLowStopMotor:
  {
    const u8 eject = memory.Read_U8(request.buffer_in + 7);
    const u8 kill = memory.Read_U8(request.buffer_in + 11);
    INFO_LOG_FMT(IOS_DI, "DVDLowStopMotor({}, {})", eject, kill);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF0,
                     0xE3000000 | ((eject & 1) << 17) | ((kill & 1) << 20));
    mmio->Write<u32>(system, ADDRESS_DICMDBUF1, 0);
    return StartImmediateTransfer(request);
  }
  case DIIoctl::DVDLowAudioBufferConfig:
  {
    const u8 enable = memory.Read_U8(request.buffer_in + 7);
    const u8 buffer_size = memory.Read_U8(request.buffer_in + 11);
    INFO_LOG_FMT(IOS_DI, "DVDLowAudioBufferConfig: {}, buffer size {}",
                 enable ? "enabled" : "disabled", buffer_size);
    mmio->Write<u32>(system, ADDRESS_DICMDBUF0,
                     0xE4000000 | ((enable & 1) << 16) | (buffer_size & 0xf));
    mmio->Write<u32>(system, ADDRESS_DICMDBUF1, 0);
    // On the other hand, this command *does* copy DIIMMBUF, but the actual code in the drive never
    // writes anything to it, so it just copies over a stale value (e.g. from DVDLowRequestError).
    return StartImmediateTransfer(request);
  }
  default:
    ERROR_LOG_FMT(IOS_DI, "Unknown ioctl {:#04x}", request.request);
    return DIResult::SecurityError;
  }
}

std::optional<DIDevice::DIResult> DIDevice::StartDMATransfer(u32 command_length,
                                                             const IOCtlRequest& request)
{
  if (request.buffer_out_size < command_length)
  {
    // Actual /dev/di will still send a command, but won't write the length or output address,
    // causing it to eventually time out after 15 seconds.  Just immediately time out here,
    // instead.
    WARN_LOG_FMT(
        IOS_DI,
        "Output buffer is too small for the result of the command ({} bytes given, needed at "
        "least {}); returning read timed out (immediately, instead of waiting)",
        request.buffer_out_size, command_length);
    return DIResult::ReadTimedOut;
  }

  if ((command_length & 0x1f) != 0 || (request.buffer_out & 0x1f) != 0)
  {
    // In most cases, the actual DI driver just hangs for unaligned data, but let's be a bit more
    // gentle.
    WARN_LOG_FMT(
        IOS_DI,
        "Output buffer or length is incorrectly aligned (buffer {:#010x}, buffer length {:x}, "
        "command length {:x})",
        request.buffer_out, request.buffer_out_size, command_length);
    return DIResult::BadArgument;
  }

  auto& system = GetSystem();
  auto* mmio = system.GetMemory().GetMMIOMapping();
  mmio->Write<u32>(system, ADDRESS_DIMAR, request.buffer_out);
  m_last_length = command_length;
  mmio->Write<u32>(system, ADDRESS_DILENGTH, command_length);

  system.GetDVDInterface().ExecuteCommand(DVD::ReplyType::IOS);
  // Reply will be posted when done by FinishIOCtl.
  return {};
}
std::optional<DIDevice::DIResult> DIDevice::StartImmediateTransfer(const IOCtlRequest& request,
                                                                   bool write_to_buf)
{
  if (write_to_buf && request.buffer_out_size < 4)
  {
    WARN_LOG_FMT(
        IOS_DI,
        "Output buffer size is too small for an immediate transfer ({} bytes, should be at "
        "least 4).  Performing transfer anyways.",
        request.buffer_out_size);
  }

  m_executing_command->m_copy_diimmbuf = write_to_buf;

  auto& system = GetSystem();
  system.GetDVDInterface().ExecuteCommand(DVD::ReplyType::IOS);
  // Reply will be posted when done by FinishIOCtl.
  return {};
}

static std::shared_ptr<DIDevice> GetDevice()
{
  auto ios = Core::System::GetInstance().GetIOS();
  if (!ios)
    return nullptr;
  auto di = ios->GetDeviceByName("/dev/di");
  // di may be empty, but static_pointer_cast returns empty in that case
  return std::static_pointer_cast<DIDevice>(di);
}

void DIDevice::InterruptFromDVDInterface(DVD::DIInterruptType interrupt_type)
{
  DIResult result;
  switch (interrupt_type)
  {
  case DVD::DIInterruptType::TCINT:
    result = DIResult::Success;
    break;
  case DVD::DIInterruptType::DEINT:
    result = DIResult::DriveError;
    break;
  default:
    PanicAlertFmt("IOS::HLE::DIDevice: Unexpected DVDInterface interrupt {0}!",
                  static_cast<int>(interrupt_type));
    result = DIResult::DriveError;
    break;
  }

  if (auto di = GetDevice())
  {
    di->FinishDICommand(result);
  }
  else
  {
    PanicAlertFmt("IOS::HLE::DIDevice: Received interrupt from DVDInterface when device wasn't "
                  "registered!");
  }
}

void DIDevice::FinishDICommandCallback(Core::System& system, u64 userdata, s64 ticksbehind)
{
  const DIResult result = static_cast<DIResult>(userdata);

  if (auto di = GetDevice())
    di->FinishDICommand(result);
  else
    PanicAlertFmt("IOS::HLE::DIDevice: Received interrupt from DI when device wasn't registered!");
}

void DIDevice::FinishDICommand(DIResult result)
{
  if (!m_executing_command.has_value())
  {
    PanicAlertFmt("IOS::HLE::DIDevice: There is no command to finish!");
    return;
  }

  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  auto* mmio = memory.GetMMIOMapping();

  IOCtlRequest request{system, m_executing_command->m_request_address};
  if (m_executing_command->m_copy_diimmbuf)
    memory.Write_U32(mmio->Read<u32>(system, ADDRESS_DIIMMBUF), request.buffer_out);

  GetEmulationKernel().EnqueueIPCReply(request, static_cast<s32>(result));

  m_executing_command.reset();

  // DVDInterface is now ready to execute another command,
  // so we start executing a command from the queue if there is one
  if (!m_commands_to_execute.empty())
  {
    ProcessQueuedIOCtl();
  }
}

std::optional<IPCReply> DIDevice::IOCtlV(const IOCtlVRequest& request)
{
  // IOCtlVs are not queued since they don't (currently) go into DVDInterface and act
  // asynchronously. This does mean that an IOCtlV can be executed while an IOCtl is in progress,
  // which isn't ideal.
  InitializeIfFirstTime();

  if (request.in_vectors[0].size != 0x20)
  {
    ERROR_LOG_FMT(IOS_DI, "IOCtlV: Received bad input buffer size {:#04x}, should be 0x20",
                  request.in_vectors[0].size);
    return IPCReply{static_cast<s32>(DIResult::BadArgument)};
  }

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const u8 command = memory.Read_U8(request.in_vectors[0].address);
  if (request.request != command)
  {
    WARN_LOG_FMT(
        IOS_DI,
        "IOCtlV: Received conflicting commands: ioctl {:#04x}, buffer {:#04x}.  Using ioctlv "
        "command.",
        request.request, command);
  }

  DIResult return_value = DIResult::BadArgument;
  switch (static_cast<DIIoctl>(request.request))
  {
  case DIIoctl::DVDLowOpenPartition:
  {
    if (request.in_vectors.size() != 3 || request.io_vectors.size() != 2)
    {
      ERROR_LOG_FMT(IOS_DI, "DVDLowOpenPartition: bad vector count {} in/{} out",
                    request.in_vectors.size(), request.io_vectors.size());
      break;
    }
    if (request.in_vectors[1].address != 0)
    {
      ERROR_LOG_FMT(IOS_DI,
                    "DVDLowOpenPartition with ticket - not implemented, ignoring ticket parameter");
      DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    }
    if (request.in_vectors[2].address != 0)
    {
      ERROR_LOG_FMT(
          IOS_DI,
          "DVDLowOpenPartition with cert chain - not implemented, ignoring certs parameter");
      DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    }

    const u64 partition_offset =
        static_cast<u64>(memory.Read_U32(request.in_vectors[0].address + 4)) << 2;
    ChangePartition(DiscIO::Partition(partition_offset));

    INFO_LOG_FMT(IOS_DI, "DVDLowOpenPartition: partition_offset {:#011x}", partition_offset);

    // Read TMD to the buffer
    auto& dvd_thread = system.GetDVDThread();
    const ES::TMDReader tmd = dvd_thread.GetTMD(m_current_partition);
    const std::vector<u8>& raw_tmd = tmd.GetBytes();
    memory.CopyToEmu(request.io_vectors[0].address, raw_tmd.data(), raw_tmd.size());

    ReturnCode es_result = GetEmulationKernel().GetESDevice()->DIVerify(
        tmd, dvd_thread.GetTicket(m_current_partition));
    memory.Write_U32(es_result, request.io_vectors[1].address);

    return_value = DIResult::Success;
    break;
  }
  case DIIoctl::DVDLowGetNoDiscOpenPartitionParams:
    ERROR_LOG_FMT(IOS_DI, "DVDLowGetNoDiscOpenPartitionParams - dummied out");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    request.DumpUnknown(system, GetDeviceName(), Common::Log::LogType::IOS_DI);
    break;
  case DIIoctl::DVDLowNoDiscOpenPartition:
    ERROR_LOG_FMT(IOS_DI, "DVDLowNoDiscOpenPartition - dummied out");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    request.DumpUnknown(system, GetDeviceName(), Common::Log::LogType::IOS_DI);
    break;
  case DIIoctl::DVDLowGetNoDiscBufferSizes:
    ERROR_LOG_FMT(IOS_DI, "DVDLowGetNoDiscBufferSizes - dummied out");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    request.DumpUnknown(system, GetDeviceName(), Common::Log::LogType::IOS_DI);
    break;
  case DIIoctl::DVDLowOpenPartitionWithTmdAndTicket:
    ERROR_LOG_FMT(IOS_DI, "DVDLowOpenPartitionWithTmdAndTicket - not implemented");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    break;
  case DIIoctl::DVDLowOpenPartitionWithTmdAndTicketView:
    ERROR_LOG_FMT(IOS_DI, "DVDLowOpenPartitionWithTmdAndTicketView - not implemented");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    break;
  default:
    ERROR_LOG_FMT(IOS_DI, "Unknown ioctlv {:#04x}", request.request);
    request.DumpUnknown(system, GetDeviceName(), Common::Log::LogType::IOS_DI);
  }
  return IPCReply{static_cast<s32>(return_value)};
}

void DIDevice::ChangePartition(const DiscIO::Partition partition)
{
  m_current_partition = partition;
}

DiscIO::Partition DIDevice::GetCurrentPartition()
{
  auto di = GetDevice();
  // Note that this function is called in Gamecube mode for UpdateRunningGameMetadata,
  // so both cases are hit in normal circumstances.
  if (!di)
    return DiscIO::PARTITION_NONE;
  return di->m_current_partition;
}

void DIDevice::InitializeIfFirstTime()
{
  // Match the behavior of Nintendo's initDvdDriverStage2, which is called the first time
  // an open/ioctl/ioctlv occurs.  This behavior is observable by directly reading the DI registers,
  // so it shouldn't be handled in the constructor.
  // Note that ResetDIRegisters also clears the current partition, which actually normally happens
  // earlier; however, that is not observable.
  // Also, registers are not cleared if syscall_check_di_reset (0x46) returns true (bit 10 of
  // HW_RESETS is set), which is not currently emulated.
  if (!m_has_initialized)
  {
    ResetDIRegisters();
    m_has_initialized = true;
  }
}

void DIDevice::ResetDIRegisters()
{
  // Clear transfer complete and error interrupts (normally r/z, but here we just directly write
  // zero)
  auto& di = GetSystem().GetDVDInterface();
  di.ClearInterrupt(DVD::DIInterruptType::TCINT);
  di.ClearInterrupt(DVD::DIInterruptType::DEINT);
  // Enable transfer complete and error interrupts, and disable cover interrupt
  di.SetInterruptEnabled(DVD::DIInterruptType::TCINT, true);
  di.SetInterruptEnabled(DVD::DIInterruptType::DEINT, true);
  di.SetInterruptEnabled(DVD::DIInterruptType::CVRINT, false);
  // Close the current partition, if there is one
  ChangePartition(DiscIO::PARTITION_NONE);
}
}  // namespace IOS::HLE
