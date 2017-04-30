// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/Network/WD/Command.h"

#include <cstring>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Network.h"
#include "Common/Swap.h"

#include "Core/HW/Memmap.h"
#include "Core/IOS/Network/MACUtils.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
NetWDCommand::NetWDCommand(Kernel& ios, const std::string& device_name) : Device(ios, device_name)
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
