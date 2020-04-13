// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/DI/DI.h"

#include <cinttypes>
#include <memory>
#include <vector>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Core/Analytics.h"
#include "Core/CoreTiming.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/DVD/DVDThread.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "DiscIO/Volume.h"

template <u32 addr>
class RegisterWrapper
{
public:
  operator u32() const { return Memory::mmio_mapping->Read<u32>(addr); }
  void operator=(u32 rhs) { Memory::mmio_mapping->Write(addr, rhs); }
};
static RegisterWrapper<0x0D806000> DISR;
static RegisterWrapper<0x0D806004> DICVR;
static RegisterWrapper<0x0D806008> DICMDBUF0;
static RegisterWrapper<0x0D80600C> DICMDBUF1;
static RegisterWrapper<0x0D806010> DICMDBUF2;
static RegisterWrapper<0x0D806014> DIMAR;
static RegisterWrapper<0x0D806018> DILENGTH;
static RegisterWrapper<0x0D80601C> DICR;
static RegisterWrapper<0x0D806020> DIIMMBUF;

namespace IOS::HLE::Device
{
CoreTiming::EventType* DI::s_finish_executing_di_command;

DI::DI(Kernel& ios, const std::string& device_name) : Device(ios, device_name)
{
}

void DI::DoState(PointerWrap& p)
{
  DoStateShared(p);
  p.Do(m_commands_to_execute);
  p.Do(m_executing_command);
  p.Do(m_current_partition);
  p.Do(m_has_initialized);
  p.Do(m_last_length);
}

IPCCommandResult DI::Open(const OpenRequest& request)
{
  InitializeIfFirstTime();
  return Device::Open(request);
}

IPCCommandResult DI::IOCtl(const IOCtlRequest& request)
{
  InitializeIfFirstTime();

  // DI IOCtls are handled in a special way by Dolphin compared to other IOS functions.
  // Many of them use DVDInterface's ExecuteCommand, which will execute commands more or less
  // asynchronously. The rest are also treated as async to match this.  Only one command can be
  // executed at a time, so commands are queued until DVDInterface is ready to handle them.

  const u8 command = Memory::Read_U8(request.buffer_in);
  if (request.request != command)
  {
    WARN_LOG(
        IOS_DI,
        "IOCtl: Received conflicting commands: ioctl 0x%02x, buffer 0x%02x.  Using ioctl command.",
        request.request, command);
  }

  bool ready_to_execute = !m_executing_command.has_value();
  m_commands_to_execute.push_back(request.address);

  if (ready_to_execute)
  {
    ProcessQueuedIOCtl();
  }

  // FinishIOCtl will be called after the command has been executed
  // to reply to the request, so we shouldn't reply here.
  return GetNoReply();
}

void DI::ProcessQueuedIOCtl()
{
  if (m_commands_to_execute.empty())
  {
    PanicAlert("IOS::HLE::Device::DI: There is no command to execute!");
    return;
  }

  m_executing_command = {m_commands_to_execute.front()};
  m_commands_to_execute.pop_front();

  IOCtlRequest request{m_executing_command->m_request_address};
  auto finished = StartIOCtl(request);
  if (finished)
  {
    CoreTiming::ScheduleEvent(2700 * SystemTimers::TIMER_RATIO, s_finish_executing_di_command,
                              static_cast<u64>(finished.value()));
    return;
  }
}

std::optional<DI::DIResult> DI::WriteIfFits(const IOCtlRequest& request, u32 value)
{
  if (request.buffer_out_size < 4)
  {
    WARN_LOG(IOS_DI, "Output buffer is too small to contain result; returning security error");
    return DIResult::SecurityError;
  }
  else
  {
    Memory::Write_U32(value, request.buffer_out);
    return DIResult::Success;
  }
}

std::optional<DI::DIResult> DI::StartIOCtl(const IOCtlRequest& request)
{
  if (request.buffer_in_size != 0x20)
  {
    ERROR_LOG(IOS_DI, "IOCtl: Received bad input buffer size 0x%02x, should be 0x20",
              request.buffer_in_size);
    return DIResult::SecurityError;
  }

  // DVDInterface's ExecuteCommand handles most of the work for most of these.
  // The IOCtl callback is used to generate a reply afterwards.
  switch (static_cast<DIIoctl>(request.request))
  {
  case DIIoctl::DVDLowInquiry:
    INFO_LOG(IOS_DI, "DVDLowInquiry");
    DICMDBUF0 = 0x12000000;
    DICMDBUF1 = 0;
    return StartDMATransfer(0x20, request);
  case DIIoctl::DVDLowReadDiskID:
    INFO_LOG(IOS_DI, "DVDLowReadDiskID");
    DICMDBUF0 = 0xA8000040;
    DICMDBUF1 = 0;
    DICMDBUF2 = 0x20;
    return StartDMATransfer(0x20, request);
    // TODO: Include an additional read that happens on Wii discs, or at least
    // emulate its side effect of disabling DTK configuration
    // if (Memory::Read_U32(request.buffer_out + 24) == 0x5d1c9ea3) { // Wii Magic
    //   if (!m_has_read_encryption_info) {
    //     // Read 0x44 (=> 0x60) bytes starting from offset 8 or byte 0x20;
    //     // byte 0x60 is disable hashing and byte 0x61 is disable encryption
    //   }
    // }
  case DIIoctl::DVDLowRead:
  {
    const u32 length = Memory::Read_U32(request.buffer_in + 4);
    const u32 position = Memory::Read_U32(request.buffer_in + 8);
    INFO_LOG(IOS_DI, "DVDLowRead: offset 0x%08x (byte 0x%09" PRIx64 "), length 0x%x", position,
             static_cast<u64>(position) << 2, length);
    if (m_current_partition == DiscIO::PARTITION_NONE)
    {
      ERROR_LOG(IOS_DI, "Attempted to perform a decrypting read when no partition is open!");
      return DIResult::SecurityError;
    }
    if (request.buffer_out_size < length)
    {
      WARN_LOG(IOS_DI,
               "Output buffer is too small for the result of the read (%d bytes given, needed at "
               "least %d); returning security error",
               request.buffer_out_size, length);
      return DIResult::SecurityError;
    }
    m_last_length = position;  // An actual mistake in IOS
    DVDInterface::PerformDecryptingRead(position, length, request.buffer_out, m_current_partition,
                                        DVDInterface::ReplyType::IOS);
    return {};
  }
  case DIIoctl::DVDLowWaitForCoverClose:
    // This is a bit awkward to implement, as it doesn't return for a long time, so just act as if
    // the cover was immediately closed
    INFO_LOG(IOS_DI, "DVDLowWaitForCoverClose - skipping");
    return DIResult::CoverClosed;
  case DIIoctl::DVDLowGetCoverRegister:
  {
    u32 dicvr = DICVR;
    DEBUG_LOG(IOS_DI, "DVDLowGetCoverRegister 0x%08x", dicvr);
    return WriteIfFits(request, dicvr);
  }
  case DIIoctl::DVDLowNotifyReset:
    INFO_LOG(IOS_DI, "DVDLowNotifyReset");
    ResetDIRegisters();
    return DIResult::Success;
  case DIIoctl::DVDLowSetSpinupFlag:
    ERROR_LOG(IOS_DI, "DVDLowSetSpinupFlag - not a valid command, rejecting");
    return DIResult::BadArgument;
  case DIIoctl::DVDLowReadDvdPhysical:
  {
    const u8 position = Memory::Read_U8(request.buffer_in + 7);
    INFO_LOG(IOS_DI, "DVDLowReadDvdPhysical: position 0x%02x", position);
    DICMDBUF0 = 0xAD000000 | (position << 8);
    DICMDBUF1 = 0;
    DICMDBUF2 = 0;
    return StartDMATransfer(0x800, request);
  }
  case DIIoctl::DVDLowReadDvdCopyright:
  {
    const u8 position = Memory::Read_U8(request.buffer_in + 7);
    INFO_LOG(IOS_DI, "DVDLowReadDvdCopyright: position 0x%02x", position);
    DICMDBUF0 = 0xAD010000 | (position << 8);
    DICMDBUF1 = 0;
    DICMDBUF2 = 0;
    return StartImmediateTransfer(request);
  }
  case DIIoctl::DVDLowReadDvdDiscKey:
  {
    const u8 position = Memory::Read_U8(request.buffer_in + 7);
    INFO_LOG(IOS_DI, "DVDLowReadDvdDiscKey: position 0x%02x", position);
    DICMDBUF0 = 0xAD020000 | (position << 8);
    DICMDBUF1 = 0;
    DICMDBUF2 = 0;
    return StartDMATransfer(0x800, request);
  }
  case DIIoctl::DVDLowGetLength:
    INFO_LOG(IOS_DI, "DVDLowGetLength 0x%08x", m_last_length);
    return WriteIfFits(request, m_last_length);
  case DIIoctl::DVDLowGetImmBuf:
  {
    u32 diimmbuf = DIIMMBUF;
    INFO_LOG(IOS_DI, "DVDLowGetImmBuf 0x%08x", diimmbuf);
    return WriteIfFits(request, diimmbuf);
  }
  case DIIoctl::DVDLowMaskCoverInterrupt:
    INFO_LOG(IOS_DI, "DVDLowMaskCoverInterrupt");
    DVDInterface::SetInterruptEnabled(DVDInterface::DIInterruptType::CVRINT, false);
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DI_INTERRUPT_MASK_COMMAND);
    return DIResult::Success;
  case DIIoctl::DVDLowClearCoverInterrupt:
    DEBUG_LOG(IOS_DI, "DVDLowClearCoverInterrupt");
    DVDInterface::ClearInterrupt(DVDInterface::DIInterruptType::CVRINT);
    return DIResult::Success;
  case DIIoctl::DVDLowUnmaskStatusInterrupts:
    INFO_LOG(IOS_DI, "DVDLowUnmaskStatusInterrupts");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DI_INTERRUPT_MASK_COMMAND);
    // Dummied out
    return DIResult::Success;
  case DIIoctl::DVDLowGetCoverStatus:
    // TODO: handle resetting case
    INFO_LOG(IOS_DI, "DVDLowGetCoverStatus: Disc %sInserted",
             DVDInterface::IsDiscInside() ? "" : "Not ");
    return WriteIfFits(request, DVDInterface::IsDiscInside() ? 2 : 1);
  case DIIoctl::DVDLowUnmaskCoverInterrupt:
    INFO_LOG(IOS_DI, "DVDLowUnmaskCoverInterrupt");
    DVDInterface::SetInterruptEnabled(DVDInterface::DIInterruptType::CVRINT, true);
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DI_INTERRUPT_MASK_COMMAND);
    return DIResult::Success;
  case DIIoctl::DVDLowReset:
  {
    const bool spinup = Memory::Read_U32(request.buffer_in + 4);
    INFO_LOG(IOS_DI, "DVDLowReset %s spinup", spinup ? "with" : "without");
    DVDInterface::ResetDrive(spinup);
    ResetDIRegisters();
    return DIResult::Success;
  }
  case DIIoctl::DVDLowOpenPartition:
    ERROR_LOG(IOS_DI, "DVDLowOpenPartition as an ioctl - rejecting");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    return DIResult::SecurityError;
  case DIIoctl::DVDLowClosePartition:
    INFO_LOG(IOS_DI, "DVDLowClosePartition");
    ChangePartition(DiscIO::PARTITION_NONE);
    return DIResult::Success;
  case DIIoctl::DVDLowUnencryptedRead:
  {
    const u32 length = Memory::Read_U32(request.buffer_in + 4);
    const u32 position = Memory::Read_U32(request.buffer_in + 8);
    const u32 end = position + (length >> 2);  // a 32-bit offset
    INFO_LOG(IOS_DI, "DVDLowUnencryptedRead: offset 0x%08x (byte 0x%09" PRIx64 "), length 0x%x",
             position, static_cast<u64>(position) << 2, length);
    // (start, end) as 32-bit offsets
    // Older IOS versions only accept the first range.  Later versions added the extra ranges to
    // check how the drive responds to out of bounds reads (an error 001 check).
    constexpr std::array<std::pair<u32, u32>, 3> valid_ranges = {
        std::make_pair(0, 0x14000),  // "System area"
        std::make_pair(0x460A0000, 0x460A0008),
        std::make_pair(0x7ED40000, 0x7ED40008),
    };
    for (auto range : valid_ranges)
    {
      if (range.first <= position && position <= range.second && range.first <= end &&
          end <= range.second)
      {
        DICMDBUF0 = 0xA8000000;
        DICMDBUF1 = position;
        DICMDBUF2 = length;
        return StartDMATransfer(length, request);
      }
    }
    WARN_LOG(IOS_DI, "DVDLowUnencryptedRead: trying to read from an illegal region!");
    return DIResult::SecurityError;
  }
  case DIIoctl::DVDLowEnableDvdVideo:
    ERROR_LOG(IOS_DI, "DVDLowEnableDvdVideo - rejecting");
    return DIResult::SecurityError;
  // There are a bunch of ioctlvs that are also (unintentionally) usable as ioctls; reject these in
  // Dolphin as games are unlikely to use them.
  case DIIoctl::DVDLowGetNoDiscOpenPartitionParams:
    ERROR_LOG(IOS_DI, "DVDLowGetNoDiscOpenPartitionParams as an ioctl - rejecting");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    return DIResult::SecurityError;
  case DIIoctl::DVDLowNoDiscOpenPartition:
    ERROR_LOG(IOS_DI, "DVDLowNoDiscOpenPartition as an ioctl - rejecting");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    return DIResult::SecurityError;
  case DIIoctl::DVDLowGetNoDiscBufferSizes:
    ERROR_LOG(IOS_DI, "DVDLowGetNoDiscBufferSizes as an ioctl - rejecting");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    return DIResult::SecurityError;
  case DIIoctl::DVDLowOpenPartitionWithTmdAndTicket:
    ERROR_LOG(IOS_DI, "DVDLowOpenPartitionWithTmdAndTicket as an ioctl - rejecting");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    return DIResult::SecurityError;
  case DIIoctl::DVDLowOpenPartitionWithTmdAndTicketView:
    ERROR_LOG(IOS_DI, "DVDLowOpenPartitionWithTmdAndTicketView as an ioctl - rejecting");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    return DIResult::SecurityError;
  case DIIoctl::DVDLowGetStatusRegister:
  {
    u32 disr = DISR;
    INFO_LOG(IOS_DI, "DVDLowGetStatusRegister: 0x%08x", disr);
    return WriteIfFits(request, disr);
  }
  case DIIoctl::DVDLowGetControlRegister:
  {
    u32 dicr = DICR;
    INFO_LOG(IOS_DI, "DVDLowGetControlRegister: 0x%08x", dicr);
    return WriteIfFits(request, dicr);
  }
  case DIIoctl::DVDLowReportKey:
  {
    const u8 param1 = Memory::Read_U8(request.buffer_in + 7);
    const u32 param2 = Memory::Read_U32(request.buffer_in + 8);
    INFO_LOG(IOS_DI, "DVDLowReportKey: param1 0x%02x, param2 0x%06x", param1, param2);
    DICMDBUF0 = 0xA4000000 | (param1 << 16);
    DICMDBUF1 = param2 & 0xFFFFFF;
    DICMDBUF2 = 0;
    return StartDMATransfer(0x20, request);
  }
  case DIIoctl::DVDLowSeek:
  {
    const u32 position = Memory::Read_U32(request.buffer_in + 4);  // 32-bit offset
    INFO_LOG(IOS_DI, "DVDLowSeek: position 0x%08x, translated to 0x%08x", position,
             position);  // TODO: do partition translation!
    DICMDBUF0 = 0xAB000000;
    DICMDBUF1 = position;
    return StartImmediateTransfer(request, false);
  }
  case DIIoctl::DVDLowReadDvd:
  {
    const u8 flag1 = Memory::Read_U8(request.buffer_in + 7);
    const u8 flag2 = Memory::Read_U8(request.buffer_in + 11);
    const u32 length = Memory::Read_U32(request.buffer_in + 12);
    const u32 position = Memory::Read_U32(request.buffer_in + 16);
    INFO_LOG(IOS_DI, "DVDLowReadDvd(%d, %d): position 0x%06x, length 0x%06x", flag1, flag2,
             position, length);
    DICMDBUF0 = 0xD0000000 | ((flag1 & 1) << 7) | ((flag2 & 1) << 6);
    DICMDBUF1 = position & 0xFFFFFF;
    DICMDBUF2 = length & 0xFFFFFF;
    return StartDMATransfer(0x800 * length, request);
  }
  case DIIoctl::DVDLowReadDvdConfig:
  {
    const u8 flag1 = Memory::Read_U8(request.buffer_in + 7);
    const u8 param2 = Memory::Read_U8(request.buffer_in + 11);
    const u32 position = Memory::Read_U32(request.buffer_in + 12);
    INFO_LOG(IOS_DI, "DVDLowReadDvdConfig(%d, %d): position 0x%06x", flag1, param2, position);
    DICMDBUF0 = 0xD1000000 | ((flag1 & 1) << 16) | param2;
    DICMDBUF1 = position & 0xFFFFFF;
    DICMDBUF2 = 0;
    return StartImmediateTransfer(request);
  }
  case DIIoctl::DVDLowStopLaser:
    INFO_LOG(IOS_DI, "DVDLowStopLaser");
    DICMDBUF0 = 0xD2000000;
    return StartImmediateTransfer(request);
  case DIIoctl::DVDLowOffset:
  {
    const u8 flag = Memory::Read_U8(request.buffer_in + 7);
    const u32 offset = Memory::Read_U32(request.buffer_in + 8);
    INFO_LOG(IOS_DI, "DVDLowOffset(%d): offset 0x%08x", flag, offset);
    DICMDBUF0 = 0xD9000000 | ((flag & 1) << 16);
    DICMDBUF1 = offset;
    return StartImmediateTransfer(request);
  }
  case DIIoctl::DVDLowReadDiskBca:
    INFO_LOG(IOS_DI, "DVDLowReadDiskBca");
    DICMDBUF0 = 0xDA000000;
    return StartDMATransfer(0x40, request);
  case DIIoctl::DVDLowRequestDiscStatus:
    INFO_LOG(IOS_DI, "DVDLowRequestDiscStatus");
    DICMDBUF0 = 0xDB000000;
    return StartImmediateTransfer(request);
  case DIIoctl::DVDLowRequestRetryNumber:
    INFO_LOG(IOS_DI, "DVDLowRequestRetryNumber");
    DICMDBUF0 = 0xDC000000;
    return StartImmediateTransfer(request);
  case DIIoctl::DVDLowSetMaximumRotation:
  {
    const u8 speed = Memory::Read_U8(request.buffer_in + 7);
    INFO_LOG(IOS_DI, "DVDLowSetMaximumRotation: speed %d", speed);
    DICMDBUF0 = 0xDD000000 | ((speed & 3) << 16);
    return StartImmediateTransfer(request, false);
  }
  case DIIoctl::DVDLowSerMeasControl:
  {
    const u8 flag1 = Memory::Read_U8(request.buffer_in + 7);
    const u8 flag2 = Memory::Read_U8(request.buffer_in + 11);
    INFO_LOG(IOS_DI, "DVDLowSerMeasControl(%d, %d)", flag1, flag2);
    DICMDBUF0 = 0xDF000000 | ((flag1 & 1) << 17) | ((flag2 & 1) << 16);
    return StartDMATransfer(0x20, request);
  }
  case DIIoctl::DVDLowRequestError:
    INFO_LOG(IOS_DI, "DVDLowRequestError");
    DICMDBUF0 = 0xE0000000;
    return StartImmediateTransfer(request);
  case DIIoctl::DVDLowAudioStream:
  {
    const u8 mode = Memory::Read_U8(request.buffer_in + 7);
    const u32 length = Memory::Read_U32(request.buffer_in + 8);
    const u32 position = Memory::Read_U32(request.buffer_in + 12);
    INFO_LOG(IOS_DI, "DVDLowAudioStream(%d): offset 0x%08x (byte 0x%09" PRIx64 "), length 0x%x",
             mode, position, static_cast<u64>(position) << 2, length);
    DICMDBUF0 = 0xE1000000 | ((mode & 3) << 16);
    DICMDBUF1 = position;
    DICMDBUF2 = length;
    return StartImmediateTransfer(request, false);
  }
  case DIIoctl::DVDLowRequestAudioStatus:
  {
    const u8 mode = Memory::Read_U8(request.buffer_in + 7);
    INFO_LOG(IOS_DI, "DVDLowRequestAudioStatus(%d)", mode);
    DICMDBUF0 = 0xE2000000 | ((mode & 3) << 16);
    DICMDBUF1 = 0;
    // Note that this command does not copy the value written to DIIMMBUF, which makes it rather
    // useless (to actually use it, DVDLowGetImmBuf would need to be used afterwards)
    return StartImmediateTransfer(request, false);
  }
  case DIIoctl::DVDLowStopMotor:
  {
    const u8 eject = Memory::Read_U8(request.buffer_in + 7);
    const u8 kill = Memory::Read_U8(request.buffer_in + 11);
    INFO_LOG(IOS_DI, "DVDLowStopMotor(%d, %d)", eject, kill);
    DICMDBUF0 = 0xE3000000 | ((eject & 1) << 17) | ((kill & 1) << 20);
    DICMDBUF1 = 0;
    return StartImmediateTransfer(request);
  }
  case DIIoctl::DVDLowAudioBufferConfig:
  {
    const u8 enable = Memory::Read_U8(request.buffer_in + 7);
    const u8 buffer_size = Memory::Read_U8(request.buffer_in + 11);
    INFO_LOG(IOS_DI, "DVDLowAudioBufferConfig: %s, buffer size %d", enable ? "enabled" : "disabled",
             buffer_size);
    DICMDBUF0 = 0xE4000000 | ((enable & 1) << 16) | (buffer_size & 0xf);
    DICMDBUF1 = 0;
    // On the other hand, this command *does* copy DIIMMBUF, but the actual code in the drive never
    // writes anything to it, so it just copies over a stale value (e.g. from DVDLowRequestError).
    return StartImmediateTransfer(request);
  }
  default:
    ERROR_LOG(IOS_DI, "Unknown ioctl 0x%02x", request.request);
    return DIResult::SecurityError;
  }
}

std::optional<DI::DIResult> DI::StartDMATransfer(u32 command_length, const IOCtlRequest& request)
{
  if (request.buffer_out_size < command_length)
  {
    // Actual /dev/di will still send a command, but won't write the length or output address,
    // causing it to eventually time out after 15 seconds.  Just immediately time out here,
    // instead.
    WARN_LOG(IOS_DI,
             "Output buffer is too small for the result of the command (%d bytes given, needed at "
             "least %d); returning read timed out (immediately, instead of waiting)",
             request.buffer_out_size, command_length);
    return DIResult::ReadTimedOut;
  }

  if ((command_length & 0x1f) != 0 || (request.buffer_out & 0x1f) != 0)
  {
    // In most cases, the actual DI driver just hangs for unaligned data, but let's be a bit more
    // gentle.
    WARN_LOG(IOS_DI,
             "Output buffer or length is incorrectly aligned (buffer 0x%08x, buffer length %x, "
             "command length %x)",
             request.buffer_out, request.buffer_out_size, command_length);
    return DIResult::BadArgument;
  }

  DIMAR = request.buffer_out;
  m_last_length = command_length;
  DILENGTH = command_length;

  DVDInterface::ExecuteCommand(DVDInterface::ReplyType::IOS);
  // Reply will be posted when done by FinishIOCtl.
  return {};
}
std::optional<DI::DIResult> DI::StartImmediateTransfer(const IOCtlRequest& request,
                                                       bool write_to_buf)
{
  if (write_to_buf && request.buffer_out_size < 4)
  {
    WARN_LOG(IOS_DI,
             "Output buffer size is too small for an immediate transfer (%d bytes, should be at "
             "least 4).  Performing transfer anyways.",
             request.buffer_out_size);
  }

  m_executing_command->m_copy_diimmbuf = write_to_buf;

  DVDInterface::ExecuteCommand(DVDInterface::ReplyType::IOS);
  // Reply will be posted when done by FinishIOCtl.
  return {};
}

static std::shared_ptr<DI> GetDevice()
{
  auto ios = IOS::HLE::GetIOS();
  if (!ios)
    return nullptr;
  auto di = ios->GetDeviceByName("/dev/di");
  // di may be empty, but static_pointer_cast returns empty in that case
  return std::static_pointer_cast<DI>(di);
}

void DI::InterruptFromDVDInterface(DVDInterface::DIInterruptType interrupt_type)
{
  DIResult result;
  switch (interrupt_type)
  {
  case DVDInterface::DIInterruptType::TCINT:
    result = DIResult::Success;
    break;
  case DVDInterface::DIInterruptType::DEINT:
    result = DIResult::DriveError;
    break;
  default:
    PanicAlert("IOS::HLE::Device::DI: Unexpected DVDInterface interrupt %d!",
               static_cast<int>(interrupt_type));
    result = DIResult::DriveError;
    break;
  }

  auto di = GetDevice();
  if (di)
  {
    di->FinishDICommand(result);
  }
  else
  {
    PanicAlert("IOS::HLE::Device::DI: Received interrupt from DVDInterface when device wasn't "
               "registered!");
  }
}

void DI::FinishDICommandCallback(u64 userdata, s64 ticksbehind)
{
  const DIResult result = static_cast<DIResult>(userdata);

  auto di = GetDevice();
  if (di)
    di->FinishDICommand(result);
  else
    PanicAlert("IOS::HLE::Device::DI: Received interrupt from DI when device wasn't registered!");
}

void DI::FinishDICommand(DIResult result)
{
  if (!m_executing_command.has_value())
  {
    PanicAlert("IOS::HLE::Device::DI: There is no command to finish!");
    return;
  }

  IOCtlRequest request{m_executing_command->m_request_address};
  if (m_executing_command->m_copy_diimmbuf)
    Memory::Write_U32(DIIMMBUF, request.buffer_out);

  m_ios.EnqueueIPCReply(request, static_cast<s32>(result));

  m_executing_command.reset();

  // DVDInterface is now ready to execute another command,
  // so we start executing a command from the queue if there is one
  if (!m_commands_to_execute.empty())
  {
    ProcessQueuedIOCtl();
  }
}

IPCCommandResult DI::IOCtlV(const IOCtlVRequest& request)
{
  // IOCtlVs are not queued since they don't (currently) go into DVDInterface and act
  // asynchronously. This does mean that an IOCtlV can be executed while an IOCtl is in progress,
  // which isn't ideal.
  InitializeIfFirstTime();

  if (request.in_vectors[0].size != 0x20)
  {
    ERROR_LOG(IOS_DI, "IOCtlV: Received bad input buffer size 0x%02x, should be 0x20",
              request.in_vectors[0].size);
    return GetDefaultReply(static_cast<s32>(DIResult::BadArgument));
  }
  const u8 command = Memory::Read_U8(request.in_vectors[0].address);
  if (request.request != command)
  {
    WARN_LOG(IOS_DI,
             "IOCtlV: Received conflicting commands: ioctl 0x%02x, buffer 0x%02x.  Using ioctlv "
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
      ERROR_LOG(IOS_DI, "DVDLowOpenPartition: bad vector count %zu in/%zu out",
                request.in_vectors.size(), request.io_vectors.size());
      break;
    }
    if (request.in_vectors[1].address != 0)
    {
      ERROR_LOG(IOS_DI,
                "DVDLowOpenPartition with ticket - not implemented, ignoring ticket parameter");
      DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    }
    if (request.in_vectors[2].address != 0)
    {
      ERROR_LOG(IOS_DI,
                "DVDLowOpenPartition with cert chain - not implemented, ignoring certs parameter");
      DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    }

    const u64 partition_offset =
        static_cast<u64>(Memory::Read_U32(request.in_vectors[0].address + 4)) << 2;
    ChangePartition(DiscIO::Partition(partition_offset));

    INFO_LOG(IOS_DI, "DVDLowOpenPartition: partition_offset 0x%09" PRIx64, partition_offset);

    // Read TMD to the buffer
    const IOS::ES::TMDReader tmd = DVDThread::GetTMD(m_current_partition);
    const std::vector<u8>& raw_tmd = tmd.GetBytes();
    Memory::CopyToEmu(request.io_vectors[0].address, raw_tmd.data(), raw_tmd.size());

    ReturnCode es_result = m_ios.GetES()->DIVerify(tmd, DVDThread::GetTicket(m_current_partition));
    Memory::Write_U32(es_result, request.io_vectors[1].address);

    return_value = DIResult::Success;
    break;
  }
  case DIIoctl::DVDLowGetNoDiscOpenPartitionParams:
    ERROR_LOG(IOS_DI, "DVDLowGetNoDiscOpenPartitionParams - dummied out");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    request.DumpUnknown(GetDeviceName(), Common::Log::IOS_DI);
    break;
  case DIIoctl::DVDLowNoDiscOpenPartition:
    ERROR_LOG(IOS_DI, "DVDLowNoDiscOpenPartition - dummied out");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    request.DumpUnknown(GetDeviceName(), Common::Log::IOS_DI);
    break;
  case DIIoctl::DVDLowGetNoDiscBufferSizes:
    ERROR_LOG(IOS_DI, "DVDLowGetNoDiscBufferSizes - dummied out");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    request.DumpUnknown(GetDeviceName(), Common::Log::IOS_DI);
    break;
  case DIIoctl::DVDLowOpenPartitionWithTmdAndTicket:
    ERROR_LOG(IOS_DI, "DVDLowOpenPartitionWithTmdAndTicket - not implemented");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    break;
  case DIIoctl::DVDLowOpenPartitionWithTmdAndTicketView:
    ERROR_LOG(IOS_DI, "DVDLowOpenPartitionWithTmdAndTicketView - not implemented");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DIFFERENT_PARTITION_COMMAND);
    break;
  default:
    ERROR_LOG(IOS_DI, "Unknown ioctlv 0x%02x", request.request);
    request.DumpUnknown(GetDeviceName(), Common::Log::IOS_DI);
  }
  return GetDefaultReply(static_cast<s32>(return_value));
}

void DI::ChangePartition(const DiscIO::Partition partition)
{
  m_current_partition = partition;
}

DiscIO::Partition DI::GetCurrentPartition()
{
  auto di = GetDevice();
  // Note that this function is called in Gamecube mode for UpdateRunningGameMetadata,
  // so both cases are hit in normal circumstances.
  if (!di)
    return DiscIO::PARTITION_NONE;
  return di->m_current_partition;
}

void DI::InitializeIfFirstTime()
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

void DI::ResetDIRegisters()
{
  // Clear transfer complete and error interrupts (normally r/z, but here we just directly write
  // zero)
  DVDInterface::ClearInterrupt(DVDInterface::DIInterruptType::TCINT);
  DVDInterface::ClearInterrupt(DVDInterface::DIInterruptType::DEINT);
  // Enable transfer complete and error interrupts, and disable cover interrupt
  DVDInterface::SetInterruptEnabled(DVDInterface::DIInterruptType::TCINT, true);
  DVDInterface::SetInterruptEnabled(DVDInterface::DIInterruptType::DEINT, true);
  DVDInterface::SetInterruptEnabled(DVDInterface::DIInterruptType::CVRINT, false);
  // Close the current partition, if there is one
  ChangePartition(DiscIO::PARTITION_NONE);
}
}  // namespace IOS::HLE::Device
