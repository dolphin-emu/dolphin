// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <deque>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/EnumFormatter.h"
#include "Common/Flag.h"
#include "Common/Network.h"
#include "Common/Swap.h"
#include "Core/IOS/Device.h"

namespace IOS::HLE::WD
{
// Values 2, 4, 5, 6 exist as well but are not known to be used by games, the Mii Channel
// or the system menu.
enum class Mode
{
  NotInitialized = 0,
  // Used by games to broadcast DS programs or to communicate with a DS more generally.
  DSCommunications = 1,
  Unknown2 = 2,
  // AOSS (https://en.wikipedia.org/wiki/AOSS) is a WPS-like feature.
  // This is only known to be used by the system menu.
  AOSSAccessPointScan = 3,
  Unknown4 = 4,
  Unknown5 = 5,
  Unknown6 = 6,
};

constexpr bool IsValidMode(Mode mode)
{
  return mode >= Mode::DSCommunications && mode <= Mode::Unknown6;
}
}  // namespace IOS::HLE::WD

namespace IOS::HLE
{
class NetWDCommandDevice : public EmulationDevice
{
public:
  enum class ResultCode : u32
  {
    InvalidFd = 0x80008000,
    IllegalParameter = 0x80008001,
    UnavailableCommand = 0x80008002,
    DriverError = 0x80008003,
  };

  NetWDCommandDevice(EmulationKernel& ios, const std::string& device_name);

  std::optional<IPCReply> Open(const OpenRequest& request) override;
  std::optional<IPCReply> Close(u32 fd) override;
  std::optional<IPCReply> IOCtlV(const IOCtlVRequest& request) override;
  void Update() override;
  bool IsOpened() const override { return true; }
  void DoState(PointerWrap& p) override;

private:
  enum
  {
    IOCTLV_WD_INVALID = 0x1000,
    IOCTLV_WD_GET_MODE = 0x1001,          // WD_GetMode
    IOCTLV_WD_SET_LINKSTATE = 0x1002,     // WD_SetLinkState
    IOCTLV_WD_GET_LINKSTATE = 0x1003,     // WD_GetLinkState
    IOCTLV_WD_SET_CONFIG = 0x1004,        // WD_SetConfig
    IOCTLV_WD_GET_CONFIG = 0x1005,        // WD_GetConfig
    IOCTLV_WD_CHANGE_BEACON = 0x1006,     // WD_ChangeBeacon
    IOCTLV_WD_DISASSOC = 0x1007,          // WD_DisAssoc
    IOCTLV_WD_MP_SEND_FRAME = 0x1008,     // WD_MpSendFrame
    IOCTLV_WD_SEND_FRAME = 0x1009,        // WD_SendFrame
    IOCTLV_WD_SCAN = 0x100a,              // WD_Scan
    IOCTLV_WD_CALL_WL = 0x100c,           // WD_CallWL
    IOCTLV_WD_MEASURE_CHANNEL = 0x100b,   // WD_MeasureChannel
    IOCTLV_WD_GET_LASTERROR = 0x100d,     // WD_GetLastError
    IOCTLV_WD_GET_INFO = 0x100e,          // WD_GetInfo
    IOCTLV_WD_CHANGE_GAMEINFO = 0x100f,   // WD_ChangeGameInfo
    IOCTLV_WD_CHANGE_VTSF = 0x1010,       // WD_ChangeVTSF
    IOCTLV_WD_RECV_FRAME = 0x8000,        // WD_ReceiveFrame
    IOCTLV_WD_RECV_NOTIFICATION = 0x8001  // WD_ReceiveNotification
  };

  enum
  {
    BSSID_SIZE = 6,
    SSID_SIZE = 32
  };

  enum
  {
    SCAN_ACTIVE,
    SCAN_PASSIVE
  };

#pragma pack(push, 1)
  struct ScanInfo
  {
    u16 channel_bitmap;
    u16 max_channel_time;
    u8 bssid[BSSID_SIZE];
    u16 scan_type;
    u16 ssid_length;
    u8 ssid[SSID_SIZE];
    u8 ssid_match_mask[SSID_SIZE];
  };

  struct BSSInfo
  {
    u16 length;
    u16 rssi;
    u8 bssid[BSSID_SIZE];
    u16 ssid_length;
    u8 ssid[SSID_SIZE];
    u16 capabilities;
    struct rate
    {
      u16 basic;
      u16 support;
    };
    u16 beacon_period;
    u16 DTIM_period;
    u16 channel;
    u16 CF_period;
    u16 CF_max_duration;
    u16 element_info_length;
    u16 element_info[1];
  };

  struct Info
  {
    Common::MACAddress mac{};
    Common::BigEndianValue<u16> enabled_channels{};
    Common::BigEndianValue<u16> nitro_allowed_channels{};
    std::array<char, 4> country_code{};
    u8 channel{};
    bool initialised{};
    std::array<char, 0x80> wl_version{};
  };
  static_assert(sizeof(Info) == 0x90);
#pragma pack(pop)

  enum class Status
  {
    Idle,
    ScanningForAOSSAccessPoint,
    ScanningForDS,
  };
  friend struct fmt::formatter<IOS::HLE::NetWDCommandDevice::Status>;

  void ProcessRecvRequests();
  void HandleStateChange();
  static Status GetTargetStatusForMode(WD::Mode mode);

  IPCReply SetLinkState(const IOCtlVRequest& request);
  IPCReply GetLinkState(const IOCtlVRequest& request) const;
  IPCReply Disassociate(const IOCtlVRequest& request);
  IPCReply GetInfo(const IOCtlVRequest& request) const;

  s32 m_ipc_owner_fd = -1;
  WD::Mode m_mode = WD::Mode::NotInitialized;
  u32 m_buffer_flags{};

  Status m_status = Status::Idle;
  Status m_target_status = Status::Idle;

  u16 m_nitro_enabled_channels{};
  Info m_info;

  Common::Flag m_clear_all_requests;
  std::deque<u32> m_recv_frame_requests;
  std::deque<u32> m_recv_notification_requests;
};
}  // namespace IOS::HLE

template <>
struct fmt::formatter<IOS::HLE::WD::Mode> : EnumFormatter<IOS::HLE::WD::Mode::Unknown6>
{
  static constexpr array_type names{
      "Not initialized", "DS Communications", "Unknown 2", "AOSS Access Point Scan",
      "Unknown 4",       "Unknown 5",         "Unknown 6",
  };
  constexpr formatter() : EnumFormatter(names) {}
};
template <>
struct fmt::formatter<IOS::HLE::NetWDCommandDevice::Status>
    : EnumFormatter<IOS::HLE::NetWDCommandDevice::Status::ScanningForDS>
{
  static constexpr array_type names{"Idle", "Scanning for AOSS Access Point", "Scanning for DS"};
  constexpr formatter() : EnumFormatter(names) {}
};
