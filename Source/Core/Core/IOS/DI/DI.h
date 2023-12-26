// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <deque>
#include <optional>
#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"
#include "DiscIO/Volume.h"

class CBoot;
class PointerWrap;

namespace DVD
{
enum class DIInterruptType : int;
}
namespace Core
{
class System;
}
namespace CoreTiming
{
struct EventType;
}

namespace IOS::HLE
{
void Init();
}

namespace IOS::HLE
{
class DIDevice : public EmulationDevice
{
public:
  DIDevice(EmulationKernel& ios, const std::string& device_name);

  static void InterruptFromDVDInterface(DVD::DIInterruptType interrupt_type);
  static DiscIO::Partition GetCurrentPartition();

  void DoState(PointerWrap& p) override;

  std::optional<IPCReply> Open(const OpenRequest& request) override;
  std::optional<IPCReply> IOCtl(const IOCtlRequest& request) override;
  std::optional<IPCReply> IOCtlV(const IOCtlVRequest& request) override;

  enum class DIIoctl : u32
  {
    DVDLowInquiry = 0x12,
    DVDLowReadDiskID = 0x70,
    DVDLowRead = 0x71,
    DVDLowWaitForCoverClose = 0x79,
    DVDLowGetCoverRegister = 0x7a,  // DVDLowPrepareCoverRegister
    DVDLowNotifyReset = 0x7e,
    DVDLowSetSpinupFlag = 0x7f,
    DVDLowReadDvdPhysical = 0x80,
    DVDLowReadDvdCopyright = 0x81,
    DVDLowReadDvdDiscKey = 0x82,
    DVDLowGetLength = 0x83,
    DVDLowGetImmBuf = 0x84,  // Unconfirmed name
    DVDLowMaskCoverInterrupt = 0x85,
    DVDLowClearCoverInterrupt = 0x86,
    DVDLowUnmaskStatusInterrupts = 0x87,  // Dummied out, ID is educated guess
    DVDLowGetCoverStatus = 0x88,
    DVDLowUnmaskCoverInterrupt = 0x89,
    DVDLowReset = 0x8a,
    DVDLowOpenPartition = 0x8b,  // ioctlv only
    DVDLowClosePartition = 0x8c,
    DVDLowUnencryptedRead = 0x8d,
    DVDLowEnableDvdVideo = 0x8e,
    DVDLowGetNoDiscOpenPartitionParams = 0x90,       // ioctlv, dummied out
    DVDLowNoDiscOpenPartition = 0x91,                // ioctlv, dummied out
    DVDLowGetNoDiscBufferSizes = 0x92,               // ioctlv, dummied out
    DVDLowOpenPartitionWithTmdAndTicket = 0x93,      // ioctlv
    DVDLowOpenPartitionWithTmdAndTicketView = 0x94,  // ioctlv
    DVDLowGetStatusRegister = 0x95,                  // DVDLowPrepareStatusRegsiter
    DVDLowGetControlRegister = 0x96,                 // DVDLowPrepareControlRegister
    DVDLowReportKey = 0xa4,
    // 0xa8 is unusable externally
    DVDLowSeek = 0xab,
    DVDLowReadDvd = 0xd0,
    DVDLowReadDvdConfig = 0xd1,
    DVDLowStopLaser = 0xd2,
    DVDLowOffset = 0xd9,
    DVDLowReadDiskBca = 0xda,
    DVDLowRequestDiscStatus = 0xdb,
    DVDLowRequestRetryNumber = 0xdc,
    DVDLowSetMaximumRotation = 0xdd,
    DVDLowSerMeasControl = 0xdf,
    DVDLowRequestError = 0xe0,
    DVDLowAudioStream = 0xe1,
    DVDLowRequestAudioStatus = 0xe2,
    DVDLowStopMotor = 0xe3,
    DVDLowAudioBufferConfig = 0xe4,
  };

  enum class DIResult : s32
  {
    Success = 1,
    DriveError = 2,
    CoverClosed = 4,
    ReadTimedOut = 16,
    SecurityError = 32,
    VerifyError = 64,
    BadArgument = 128,
  };

private:
  struct ExecutingCommandInfo
  {
    ExecutingCommandInfo() {}
    ExecutingCommandInfo(u32 request_address) : m_request_address(request_address) {}
    u32 m_request_address = 0;
    bool m_copy_diimmbuf = false;
  };

  friend class ::CBoot;
  friend void ::IOS::HLE::Init(Core::System&);

  void ProcessQueuedIOCtl();
  std::optional<DIResult> StartIOCtl(const IOCtlRequest& request);
  std::optional<DIResult> WriteIfFits(const IOCtlRequest& request, u32 value);
  std::optional<DIResult> StartDMATransfer(u32 command_length, const IOCtlRequest& request);
  std::optional<DIResult> StartImmediateTransfer(const IOCtlRequest& request,
                                                 bool write_to_buf = true);

  void ChangePartition(const DiscIO::Partition partition);
  void InitializeIfFirstTime();
  void ResetDIRegisters();
  static void FinishDICommandCallback(Core::System& system, u64 userdata, s64 ticksbehind);
  void FinishDICommand(DIResult result);

  static CoreTiming::EventType* s_finish_executing_di_command;

  std::optional<ExecutingCommandInfo> m_executing_command;
  std::deque<u32> m_commands_to_execute;

  DiscIO::Partition m_current_partition = DiscIO::PARTITION_NONE;

  bool m_has_initialized = false;
  u32 m_last_length = 0;
};
}  // namespace IOS::HLE
