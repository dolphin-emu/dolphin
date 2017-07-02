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
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/DVD/DVDThread.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "DiscIO/Volume.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
DI::DI(Kernel& ios, const std::string& device_name) : Device(ios, device_name)
{
}

void DI::DoState(PointerWrap& p)
{
  DoStateShared(p);
  p.Do(m_commands_to_execute);
}

IPCCommandResult DI::IOCtl(const IOCtlRequest& request)
{
  // DI IOCtls are handled in a special way by Dolphin
  // compared to other IOS functions.
  // This is a wrapper around DVDInterface's ExecuteCommand,
  // which will execute commands more or less asynchronously.
  // Only one command can be executed at a time, so commands
  // are queued until DVDInterface is ready to handle them.

  bool ready_to_execute = m_commands_to_execute.empty();
  m_commands_to_execute.push_back(request.address);
  if (ready_to_execute)
    StartIOCtl(request);

  // DVDInterface handles the timing and will call FinishIOCtl after the command
  // has been executed to reply to the request, so we shouldn't reply here.
  return GetNoReply();
}

void DI::StartIOCtl(const IOCtlRequest& request)
{
  const u32 command_0 = Memory::Read_U32(request.buffer_in);
  const u32 command_1 = Memory::Read_U32(request.buffer_in + 4);
  const u32 command_2 = Memory::Read_U32(request.buffer_in + 8);

  // DVDInterface's ExecuteCommand handles most of the work.
  // The IOCtl callback is used to generate a reply afterwards.
  DVDInterface::ExecuteCommand(command_0, command_1, command_2, request.buffer_out,
                               request.buffer_out_size, true);
}

void DI::FinishIOCtl(DVDInterface::DIInterruptType interrupt_type)
{
  if (m_commands_to_execute.empty())
  {
    PanicAlert("IOS::HLE::Device::DI: There is no command to execute!");
    return;
  }

  // This command has been executed, so it's removed from the queue
  u32 command_address = m_commands_to_execute.front();
  m_commands_to_execute.pop_front();
  m_ios.EnqueueIPCReply(IOCtlRequest{command_address}, interrupt_type);

  // DVDInterface is now ready to execute another command,
  // so we start executing a command from the queue if there is one
  if (!m_commands_to_execute.empty())
  {
    IOCtlRequest next_request{m_commands_to_execute.front()};
    StartIOCtl(next_request);
  }
}

IPCCommandResult DI::IOCtlV(const IOCtlVRequest& request)
{
  for (const auto& vector : request.io_vectors)
    Memory::Memset(vector.address, 0, vector.size);
  s32 return_value = IPC_SUCCESS;
  switch (request.request)
  {
  case DVDInterface::DVDLowOpenPartition:
  {
    _dbg_assert_msg_(IOS_DI, request.in_vectors[1].address == 0, "DVDLowOpenPartition with ticket");
    _dbg_assert_msg_(IOS_DI, request.in_vectors[2].address == 0,
                     "DVDLowOpenPartition with cert chain");

    const u64 partition_offset =
        static_cast<u64>(Memory::Read_U32(request.in_vectors[0].address + 4)) << 2;
    const DiscIO::Partition partition(partition_offset);
    DVDInterface::ChangePartition(partition);

    INFO_LOG(IOS_DI, "DVDLowOpenPartition: partition_offset 0x%016" PRIx64, partition_offset);

    // Read TMD to the buffer
    const IOS::ES::TMDReader tmd = DVDThread::GetTMD(partition);
    const std::vector<u8>& raw_tmd = tmd.GetBytes();
    Memory::CopyToEmu(request.io_vectors[0].address, raw_tmd.data(), raw_tmd.size());
    ES::DIVerify(tmd, DVDThread::GetTicket(partition));

    return_value = 1;
    break;
  }
  default:
    request.DumpUnknown(GetDeviceName(), LogTypes::IOS_DI);
  }
  return GetDefaultReply(return_value);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
