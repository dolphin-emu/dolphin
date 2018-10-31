// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/Network/NCD/Manage.h"

#include <string>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Network.h"

#include "Core/HW/Memmap.h"
#include "Core/IOS/Network/MACUtils.h"

namespace IOS::HLE::Device
{
NetNCDManage::NetNCDManage(Kernel& ios, const std::string& device_name) : Device(ios, device_name)
{
  config.ReadConfig(ios.GetFS().get());
}

IPCCommandResult NetNCDManage::IOCtlV(const IOCtlVRequest& request)
{
  s32 return_value = IPC_SUCCESS;
  u32 common_result = 0;
  u32 common_vector = 0;

  switch (request.request)
  {
  case IOCTLV_NCD_LOCKWIRELESSDRIVER:
    break;

  case IOCTLV_NCD_UNLOCKWIRELESSDRIVER:
    // Memory::Read_U32(request.in_vectors.at(0).address);
    break;

  case IOCTLV_NCD_GETCONFIG:
    INFO_LOG(IOS_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETCONFIG");
    config.WriteToMem(request.io_vectors.at(0).address);
    common_vector = 1;
    break;

  case IOCTLV_NCD_SETCONFIG:
    INFO_LOG(IOS_NET, "NET_NCD_MANAGE: IOCTLV_NCD_SETCONFIG");
    config.ReadFromMem(request.in_vectors.at(0).address);
    break;

  case IOCTLV_NCD_READCONFIG:
    INFO_LOG(IOS_NET, "NET_NCD_MANAGE: IOCTLV_NCD_READCONFIG");
    config.ReadConfig(m_ios.GetFS().get());
    config.WriteToMem(request.io_vectors.at(0).address);
    break;

  case IOCTLV_NCD_WRITECONFIG:
    INFO_LOG(IOS_NET, "NET_NCD_MANAGE: IOCTLV_NCD_WRITECONFIG");
    config.ReadFromMem(request.in_vectors.at(0).address);
    config.WriteConfig(m_ios.GetFS().get());
    break;

  case IOCTLV_NCD_GETLINKSTATUS:
    INFO_LOG(IOS_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETLINKSTATUS");
    // Always connected
    Memory::Write_U32(Net::ConnectionSettings::LINK_WIRED, request.io_vectors.at(0).address + 4);
    break;

  case IOCTLV_NCD_GETWIRELESSMACADDRESS:
  {
    INFO_LOG(IOS_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETWIRELESSMACADDRESS");

    const Common::MACAddress address = IOS::Net::GetMACAddress();
    Memory::CopyToEmu(request.io_vectors.at(1).address, address.data(), address.size());
    break;
  }

  default:
    INFO_LOG(IOS_NET, "NET_NCD_MANAGE IOCtlV: %#x", request.request);
    break;
  }

  Memory::Write_U32(common_result, request.io_vectors.at(common_vector).address);
  if (common_vector == 1)
  {
    Memory::Write_U32(common_result, request.io_vectors.at(common_vector).address + 4);
  }
  return GetDefaultReply(return_value);
}
}  // namespace IOS::HLE::Device
