// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/Network/Net.h"

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Network.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/Network/MACUtils.h"
#include "Core/IOS/Network/Socket.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
// **********************************************************************************
// Handle /dev/net/ncd/manage requests
NetNCDManage::NetNCDManage(u32 device_id, const std::string& device_name)
    : Device(device_id, device_name)
{
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
    config.ReadConfig();
    config.WriteToMem(request.io_vectors.at(0).address);
    break;

  case IOCTLV_NCD_WRITECONFIG:
    INFO_LOG(IOS_NET, "NET_NCD_MANAGE: IOCTLV_NCD_WRITECONFIG");
    config.ReadFromMem(request.in_vectors.at(0).address);
    config.WriteConfig();
    break;

  case IOCTLV_NCD_GETLINKSTATUS:
    INFO_LOG(IOS_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETLINKSTATUS");
    // Always connected
    Memory::Write_U32(Net::ConnectionSettings::LINK_WIRED, request.io_vectors.at(0).address + 4);
    break;

  case IOCTLV_NCD_GETWIRELESSMACADDRESS:
    INFO_LOG(IOS_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETWIRELESSMACADDRESS");

    u8 address[Common::MAC_ADDRESS_SIZE];
    IOS::Net::GetMACAddress(address);
    Memory::CopyToEmu(request.io_vectors.at(1).address, address, sizeof(address));
    break;

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

// **********************************************************************************
// Handle /dev/net/wd/command requests
NetWDCommand::NetWDCommand(u32 device_id, const std::string& device_name)
    : Device(device_id, device_name)
{
}

// This is just for debugging / playing around.
// There really is no reason to implement wd unless we can bend it such that
// we can talk to the DS.
IPCCommandResult NetWDCommand::IOCtlV(const IOCtlVRequest& request)
{
  s32 return_value = IPC_SUCCESS;

  switch (request.request)
  {
  case IOCTLV_WD_SCAN:
  {
    // Gives parameters detailing type of scan and what to match
    // XXX - unused
    // ScanInfo *scan = (ScanInfo *)Memory::GetPointer(request.in_vectors.at(0).m_Address);

    u16* results = (u16*)Memory::GetPointer(request.io_vectors.at(0).address);
    // first u16 indicates number of BSSInfo following
    results[0] = Common::swap16(1);

    BSSInfo* bss = (BSSInfo*)&results[1];
    memset(bss, 0, sizeof(BSSInfo));

    bss->length = Common::swap16(sizeof(BSSInfo));
    bss->rssi = Common::swap16(0xffff);

    for (int i = 0; i < BSSID_SIZE; ++i)
      bss->bssid[i] = i;

    const char* ssid = "dolphin-emu";
    strcpy((char*)bss->ssid, ssid);
    bss->ssid_length = Common::swap16((u16)strlen(ssid));

    bss->channel = Common::swap16(2);
  }
  break;

  case IOCTLV_WD_GET_INFO:
  {
    Info* info = (Info*)Memory::GetPointer(request.io_vectors.at(0).address);
    memset(info, 0, sizeof(Info));
    // Probably used to disallow certain channels?
    memcpy(info->country, "US", 2);
    info->ntr_allowed_channels = Common::swap16(0xfffe);

    u8 address[Common::MAC_ADDRESS_SIZE];
    IOS::Net::GetMACAddress(address);
    memcpy(info->mac, address, sizeof(info->mac));
  }
  break;

  case IOCTLV_WD_GET_MODE:
  case IOCTLV_WD_SET_LINKSTATE:
  case IOCTLV_WD_GET_LINKSTATE:
  case IOCTLV_WD_SET_CONFIG:
  case IOCTLV_WD_GET_CONFIG:
  case IOCTLV_WD_CHANGE_BEACON:
  case IOCTLV_WD_DISASSOC:
  case IOCTLV_WD_MP_SEND_FRAME:
  case IOCTLV_WD_SEND_FRAME:
  case IOCTLV_WD_CALL_WL:
  case IOCTLV_WD_MEASURE_CHANNEL:
  case IOCTLV_WD_GET_LASTERROR:
  case IOCTLV_WD_CHANGE_GAMEINFO:
  case IOCTLV_WD_CHANGE_VTSF:
  case IOCTLV_WD_RECV_FRAME:
  case IOCTLV_WD_RECV_NOTIFICATION:
  default:
    request.Dump(GetDeviceName(), LogTypes::IOS_NET, LogTypes::LINFO);
  }

  return GetDefaultReply(return_value);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
