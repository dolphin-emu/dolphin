// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/Network/NCD/WiiNetConfig.h"

namespace IOS::HLE::Device
{
// Interface for reading and changing network configuration (probably some other stuff as well)
class NetNCDManage : public Device
{
public:
  NetNCDManage(Kernel& ios, const std::string& device_name);

  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;

private:
  enum
  {
    IOCTLV_NCD_LOCKWIRELESSDRIVER = 0x1,    // NCDLockWirelessDriver
    IOCTLV_NCD_UNLOCKWIRELESSDRIVER = 0x2,  // NCDUnlockWirelessDriver
    IOCTLV_NCD_GETCONFIG = 0x3,             // NCDiGetConfig
    IOCTLV_NCD_SETCONFIG = 0x4,             // NCDiSetConfig
    IOCTLV_NCD_READCONFIG = 0x5,
    IOCTLV_NCD_WRITECONFIG = 0x6,
    IOCTLV_NCD_GETLINKSTATUS = 0x7,          // NCDGetLinkStatus
    IOCTLV_NCD_GETWIRELESSMACADDRESS = 0x8,  // NCDGetWirelessMacAddress
  };

  Net::WiiNetConfig config;
};
}  // namespace IOS::HLE::Device
