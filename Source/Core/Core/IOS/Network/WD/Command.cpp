// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/WD/Command.h"

#include <cstring>
#include <string>

#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Network.h"
#include "Common/Swap.h"
#include "Core/DolphinAnalytics.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/Network/MACUtils.h"
#include "Core/System.h"

namespace IOS::HLE
{
namespace
{
// clang-format off
//                           Channel:   FEDC BA98 7654 3210
constexpr u16 LegalChannelMask =      0b0111'1111'1111'1110u;
constexpr u16 LegalNitroChannelMask = 0b0011'1111'1111'1110u;
// clang-format on

u16 SelectWifiChannel(u16 enabled_channels_mask, u16 current_channel)
{
  const Common::BitSet<u16> enabled_channels{enabled_channels_mask & LegalChannelMask};
  u16 next_channel = current_channel;
  for (int i = 0; i < 16; ++i)
  {
    next_channel = (next_channel + 3) % 16;
    if (enabled_channels[next_channel])
      return next_channel;
  }
  // This does not make a lot of sense, but it is what WD does.
  return u16(enabled_channels[next_channel]);
}

u16 MakeNitroAllowedChannelMask(u16 enabled_channels_mask, u16 nitro_mask)
{
  nitro_mask &= LegalNitroChannelMask;
  // TODO: WD's version of this function has some complicated logic to determine the actual mask.
  return enabled_channels_mask & nitro_mask;
}
}  // namespace

NetWDCommandDevice::Status NetWDCommandDevice::GetTargetStatusForMode(WD::Mode mode)
{
  switch (mode)
  {
  case WD::Mode::DSCommunications:
    return Status::ScanningForDS;
  case WD::Mode::AOSSAccessPointScan:
    return Status::ScanningForAOSSAccessPoint;
  default:
    return Status::Idle;
  }
}

NetWDCommandDevice::NetWDCommandDevice(EmulationKernel& ios, const std::string& device_name)
    : EmulationDevice(ios, device_name)
{
  // TODO: use the MPCH setting in setting.txt to determine this value.
  m_nitro_enabled_channels = LegalNitroChannelMask;

  // TODO: Set the version string here. This is exposed to the PPC.
  m_info.mac = Net::GetMACAddress();
  m_info.enabled_channels = 0xfffe;
  m_info.channel = SelectWifiChannel(m_info.enabled_channels, 0);
  // The country code is supposed to be null terminated as it is logged with printf in WD.
  std::strncpy(m_info.country_code.data(), "US", m_info.country_code.size());
  m_info.nitro_allowed_channels =
      MakeNitroAllowedChannelMask(m_info.enabled_channels, m_nitro_enabled_channels);
  m_info.initialised = true;
}

void NetWDCommandDevice::Update()
{
  Device::Update();
  ProcessRecvRequests();
  HandleStateChange();
}

void NetWDCommandDevice::ProcessRecvRequests()
{
  auto& system = GetSystem();

  // Because we currently do not actually emulate the wireless driver, we have no frames
  // and no notification data that could be used to reply to requests.
  // Therefore, requests are left pending to simulate the situation where there is nothing to send.

  // All pending requests must still be processed when the handle to the resource manager is closed.
  const bool force_process = m_clear_all_requests.TestAndClear();

  const auto process_queue = [&](std::deque<u32>& queue) {
    if (!force_process)
      return;

    while (!queue.empty())
    {
      const auto request = queue.front();
      s32 result;

      // If the resource manager handle is closed while processing a request,
      // InvalidFd is returned.
      if (m_ipc_owner_fd < 0)
      {
        result = s32(ResultCode::InvalidFd);
      }
      else
      {
        // TODO: Frame/notification data would be copied here.
        // And result would be set to the data length or to an error code.
        result = 0;
      }

      INFO_LOG_FMT(IOS_NET, "Processed request {:08x} (result {:08x})", request, result);
      GetEmulationKernel().EnqueueIPCReply(Request{system, request}, result);
      queue.pop_front();
    }
  };

  process_queue(m_recv_notification_requests);
  process_queue(m_recv_frame_requests);
}

void NetWDCommandDevice::HandleStateChange()
{
  const auto status = m_status;
  const auto target_status = m_target_status;

  if (status == target_status)
    return;

  INFO_LOG_FMT(IOS_NET, "{}: Handling status change ({} -> {})", __func__, status, target_status);

  switch (status)
  {
  case Status::Idle:
    switch (target_status)
    {
    case Status::ScanningForAOSSAccessPoint:
      // This is supposed to reset the driver first by going into another state.
      // However, we can worry about that once we actually emulate WL.
      m_status = Status::ScanningForAOSSAccessPoint;
      break;
    case Status::ScanningForDS:
      // This is supposed to set a bunch of Wi-Fi driver parameters and initiate a scan.
      m_status = Status::ScanningForDS;
      break;
    case Status::Idle:
      break;
    }
    break;

  case Status::ScanningForDS:
    m_status = Status::Idle;
    break;

  case Status::ScanningForAOSSAccessPoint:
    // We are supposed to reset the driver by going into a reset state.
    // However, we can worry about that once we actually emulate WL.
    break;
  }

  INFO_LOG_FMT(IOS_NET, "{}: done (status: {} -> {}, target was {})", __func__, status, m_status,
               target_status);
}

void NetWDCommandDevice::DoState(PointerWrap& p)
{
  Device::DoState(p);
  p.Do(m_ipc_owner_fd);
  p.Do(m_mode);
  p.Do(m_buffer_flags);
  p.Do(m_status);
  p.Do(m_target_status);
  p.Do(m_nitro_enabled_channels);
  p.Do(m_info);
  p.Do(m_recv_frame_requests);
  p.Do(m_recv_notification_requests);
}

std::optional<IPCReply> NetWDCommandDevice::Open(const OpenRequest& request)
{
  if (m_ipc_owner_fd < 0)
  {
    const auto flags = u32(request.flags);
    const auto mode = WD::Mode(flags & 0xFFFF);
    const auto buffer_flags = flags & 0x7FFF0000;
    INFO_LOG_FMT(IOS_NET, "Opening with mode={} buffer_flags={:08x}", mode, buffer_flags);

    // We don't support anything other than mode 1 and mode 3 at the moment.
    if (mode != WD::Mode::DSCommunications && mode != WD::Mode::AOSSAccessPointScan)
    {
      ERROR_LOG_FMT(IOS_NET, "Unsupported WD operating mode: {}", mode);
      DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_UNCOMMON_WD_MODE);
      return IPCReply(s32(ResultCode::UnavailableCommand));
    }

    if (m_target_status == Status::Idle && mode <= WD::Mode::Unknown6)
    {
      m_mode = mode;
      m_ipc_owner_fd = request.fd;
      m_buffer_flags = buffer_flags;
    }
  }

  INFO_LOG_FMT(IOS_NET, "Opened");
  return Device::Open(request);
}

std::optional<IPCReply> NetWDCommandDevice::Close(u32 fd)
{
  if (m_ipc_owner_fd < 0 || fd != u32(m_ipc_owner_fd))
  {
    ERROR_LOG_FMT(IOS_NET, "Invalid close attempt.");
    return IPCReply(u32(ResultCode::InvalidFd));
  }

  INFO_LOG_FMT(IOS_NET, "Closing and resetting status to Idle");
  m_target_status = m_status = Status::Idle;

  m_ipc_owner_fd = -1;
  m_clear_all_requests.Set();
  return Device::Close(fd);
}

IPCReply NetWDCommandDevice::SetLinkState(const IOCtlVRequest& request)
{
  const auto* vector = request.GetVector(0);
  if (!vector || vector->address == 0)
    return IPCReply(u32(ResultCode::IllegalParameter));

  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  const u32 state = memory.Read_U32(vector->address);
  INFO_LOG_FMT(IOS_NET, "WD_SetLinkState called (state={}, mode={})", state, m_mode);

  if (state == 0)
  {
    if (!WD::IsValidMode(m_mode))
      return IPCReply(u32(ResultCode::UnavailableCommand));

    m_target_status = Status::Idle;
    INFO_LOG_FMT(IOS_NET, "WD_SetLinkState: setting target status to {}", m_target_status);
  }
  else
  {
    if (state != 1)
      return IPCReply(u32(ResultCode::IllegalParameter));

    if (!WD::IsValidMode(m_mode))
      return IPCReply(u32(ResultCode::UnavailableCommand));

    const auto target_status = GetTargetStatusForMode(m_mode);
    if (m_status != target_status && m_info.enabled_channels == 0)
      return IPCReply(u32(ResultCode::UnavailableCommand));

    INFO_LOG_FMT(IOS_NET, "WD_SetLinkState: setting target status to {}", target_status);
    m_target_status = target_status;
  }

  return IPCReply(IPC_SUCCESS);
}

IPCReply NetWDCommandDevice::GetLinkState(const IOCtlVRequest& request) const
{
  INFO_LOG_FMT(IOS_NET, "WD_GetLinkState called (status={}, mode={})", m_status, m_mode);
  if (!WD::IsValidMode(m_mode))
    return IPCReply(u32(ResultCode::UnavailableCommand));

  // Contrary to what the name of the ioctl suggests, this returns a boolean, not the current state.
  return IPCReply(u32(m_status == GetTargetStatusForMode(m_mode)));
}

IPCReply NetWDCommandDevice::Disassociate(const IOCtlVRequest& request)
{
  const auto* vector = request.GetVector(0);
  if (!vector || vector->address == 0)
    return IPCReply(u32(ResultCode::IllegalParameter));

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  Common::MACAddress mac;
  memory.CopyFromEmu(mac.data(), vector->address, mac.size());

  INFO_LOG_FMT(IOS_NET, "WD_Disassociate: MAC {}", Common::MacAddressToString(mac));

  if (m_mode != WD::Mode::DSCommunications && m_mode != WD::Mode::Unknown5 &&
      m_mode != WD::Mode::Unknown6)
  {
    ERROR_LOG_FMT(IOS_NET, "WD_Disassociate: cannot disassociate in mode {}", m_mode);
    return IPCReply(u32(ResultCode::UnavailableCommand));
  }

  const auto target_status = GetTargetStatusForMode(m_mode);
  if (m_status != target_status)
  {
    ERROR_LOG_FMT(IOS_NET, "WD_Disassociate: cannot disassociate in status {} (target {})",
                  m_status, target_status);
    return IPCReply(u32(ResultCode::UnavailableCommand));
  }

  // TODO: Check the input MAC address and only return 0x80008001 if it is unknown.
  return IPCReply(u32(ResultCode::IllegalParameter));
}

IPCReply NetWDCommandDevice::GetInfo(const IOCtlVRequest& request) const
{
  const auto* vector = request.GetVector(0);
  if (!vector || vector->address == 0)
    return IPCReply(u32(ResultCode::IllegalParameter));

  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  memory.CopyToEmu(vector->address, &m_info, sizeof(m_info));
  return IPCReply(IPC_SUCCESS);
}

std::optional<IPCReply> NetWDCommandDevice::IOCtlV(const IOCtlVRequest& request)
{
  switch (request.request)
  {
  case IOCTLV_WD_INVALID:
    return IPCReply(u32(ResultCode::UnavailableCommand));
  case IOCTLV_WD_GET_MODE:
    return IPCReply(s32(m_mode));
  case IOCTLV_WD_SET_LINKSTATE:
    return SetLinkState(request);
  case IOCTLV_WD_GET_LINKSTATE:
    return GetLinkState(request);
  case IOCTLV_WD_DISASSOC:
    return Disassociate(request);

  case IOCTLV_WD_SCAN:
  {
    // Gives parameters detailing type of scan and what to match
    // XXX - unused
    // ScanInfo *scan = (ScanInfo *)memory.GetPointer(request.in_vectors.at(0).m_Address);

    auto& system = GetSystem();
    auto& memory = system.GetMemory();
    u16* results = (u16*)memory.GetPointerForRange(request.io_vectors.at(0).address,
                                                   sizeof(u16) + sizeof(BSSInfo));
    // first u16 indicates number of BSSInfo following
    results[0] = Common::swap16(2);

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

    // Add Nintendo Wi-Fi USB Connector access point
    // The next BSSInfo loaded is at the current address plus bss->length * 2
    bss += 2;
    memset(bss, 0, sizeof(BSSInfo));

    bss->length = Common::swap16(sizeof(BSSInfo));
    bss->rssi = Common::swap16(0xffff);

    for (int i = 0; i < BSSID_SIZE; ++i)
      bss->bssid[i] = i;
    memcpy(bss->ssid, "NWCUSBAP\0\1", 10);
    bss->ssid_length = Common::swap16(0x20);
  }
  break;

  case IOCTLV_WD_GET_INFO:
    return GetInfo(request);

  case IOCTLV_WD_RECV_FRAME:
    m_recv_frame_requests.emplace_back(request.address);
    return std::nullopt;

  case IOCTLV_WD_RECV_NOTIFICATION:
    m_recv_notification_requests.emplace_back(request.address);
    return std::nullopt;

  case IOCTLV_WD_SET_CONFIG:
  case IOCTLV_WD_GET_CONFIG:
  case IOCTLV_WD_CHANGE_BEACON:
  case IOCTLV_WD_MP_SEND_FRAME:
  case IOCTLV_WD_SEND_FRAME:
  case IOCTLV_WD_CALL_WL:
  case IOCTLV_WD_MEASURE_CHANNEL:
  case IOCTLV_WD_GET_LASTERROR:
  case IOCTLV_WD_CHANGE_GAMEINFO:
  case IOCTLV_WD_CHANGE_VTSF:
  default:
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_WD_UNIMPLEMENTED_IOCTL);
    request.Dump(GetSystem(), GetDeviceName(), Common::Log::LogType::IOS_NET,
                 Common::Log::LogLevel::LWARNING);
  }

  return IPCReply(IPC_SUCCESS);
}
}  // namespace IOS::HLE
