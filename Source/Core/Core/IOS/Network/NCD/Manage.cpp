// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/NCD/Manage.h"

#include <string>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Network.h"

#include "Core/HW/Memmap.h"
#include "Core/IOS/Network/MACUtils.h"
#include "Core/System.h"

namespace IOS::HLE
{
NetNCDManageDevice::NetNCDManageDevice(EmulationKernel& ios, const std::string& device_name)
    : EmulationDevice(ios, device_name)
{
  config.ReadConfig(ios.GetFS().get());
}

void NetNCDManageDevice::DoState(PointerWrap& p)
{
  Device::DoState(p);
  p.Do(m_ipc_fd);
}

std::optional<IPCReply> NetNCDManageDevice::IOCtlV(const IOCtlVRequest& request)
{
  s32 return_value = IPC_SUCCESS;
  u32 common_result = 0;
  u32 common_vector = 0;

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  switch (request.request)
  {
  case IOCTLV_NCD_LOCKWIRELESSDRIVER:
    if (!request.HasNumberOfValidVectors(0, 1))
      return IPCReply(IPC_EINVAL);

    if (request.io_vectors[0].size < 2 * sizeof(u32))
      return IPCReply(IPC_EINVAL);

    if (m_ipc_fd != 0)
    {
      // It is an error to lock the driver again when it is already locked.
      common_result = IPC_EINVAL;
    }
    else
    {
      // NCD writes the internal address of the request's file descriptor.
      // We will just write the value of the file descriptor.
      // The value will be positive so this will work fine.
      m_ipc_fd = request.fd;
      memory.Write_U32(request.fd, request.io_vectors[0].address + 4);
    }
    break;

  case IOCTLV_NCD_UNLOCKWIRELESSDRIVER:
  {
    if (!request.HasNumberOfValidVectors(1, 1))
      return IPCReply(IPC_EINVAL);

    if (request.in_vectors[0].size < sizeof(u32))
      return IPCReply(IPC_EINVAL);

    if (request.io_vectors[0].size < sizeof(u32))
      return IPCReply(IPC_EINVAL);

    const u32 request_handle = memory.Read_U32(request.in_vectors[0].address);
    if (m_ipc_fd == request_handle)
    {
      m_ipc_fd = 0;
      common_result = 0;
    }
    else
    {
      common_result = -3;
    }

    break;
  }

  case IOCTLV_NCD_GETCONFIG:
    INFO_LOG_FMT(IOS_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETCONFIG");
    config.WriteToMem(request.io_vectors.at(0).address);
    common_vector = 1;
    break;

  case IOCTLV_NCD_SETCONFIG:
    INFO_LOG_FMT(IOS_NET, "NET_NCD_MANAGE: IOCTLV_NCD_SETCONFIG");
    config.ReadFromMem(request.in_vectors.at(0).address);
    break;

  case IOCTLV_NCD_READCONFIG:
    INFO_LOG_FMT(IOS_NET, "NET_NCD_MANAGE: IOCTLV_NCD_READCONFIG");
    config.ReadConfig(m_ios.GetFS().get());
    config.WriteToMem(request.io_vectors.at(0).address);
    break;

  case IOCTLV_NCD_WRITECONFIG:
    INFO_LOG_FMT(IOS_NET, "NET_NCD_MANAGE: IOCTLV_NCD_WRITECONFIG");
    config.ReadFromMem(request.in_vectors.at(0).address);
    config.WriteConfig(m_ios.GetFS().get());
    break;

  case IOCTLV_NCD_GETLINKSTATUS:
    INFO_LOG_FMT(IOS_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETLINKSTATUS");
    // Always connected
    memory.Write_U32(Net::ConnectionSettings::LINK_WIRED, request.io_vectors.at(0).address + 4);
    break;

  case IOCTLV_NCD_GETWIRELESSMACADDRESS:
  {
    INFO_LOG_FMT(IOS_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETWIRELESSMACADDRESS");

    const Common::MACAddress address = IOS::Net::GetMACAddress();
    memory.CopyToEmu(request.io_vectors.at(1).address, address.data(), address.size());
    break;
  }

  default:
    INFO_LOG_FMT(IOS_NET, "NET_NCD_MANAGE IOCtlV: {:#x}", request.request);
    break;
  }

  memory.Write_U32(common_result, request.io_vectors.at(common_vector).address);
  if (common_vector == 1)
  {
    memory.Write_U32(common_result, request.io_vectors.at(common_vector).address + 4);
  }
  return IPCReply(return_value);
}
}  // namespace IOS::HLE
