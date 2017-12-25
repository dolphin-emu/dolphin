// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/Network/KD/NWC24Config.h"

namespace IOS::HLE::Device
{
// KD is the IOS module responsible for implementing WiiConnect24 functionality.
// It can perform HTTPS downloads, send and receive mail via SMTP, and execute a
// JavaScript-like language while the Wii is in standby mode.
class NetKDRequest : public Device
{
public:
  NetKDRequest(Kernel& ios, const std::string& device_name);
  ~NetKDRequest() override;

  IPCCommandResult IOCtl(const IOCtlRequest& request) override;

private:
  enum
  {
    IOCTL_NWC24_SUSPEND_SCHEDULAR = 0x01,
    IOCTL_NWC24_EXEC_TRY_SUSPEND_SCHEDULAR = 0x02,
    IOCTL_NWC24_EXEC_RESUME_SCHEDULAR = 0x03,
    IOCTL_NWC24_KD_GET_TIME_TRIGGERS = 0x04,
    IOCTL_NWC24_SET_SCHEDULE_SPAN = 0x05,
    IOCTL_NWC24_STARTUP_SOCKET = 0x06,
    IOCTL_NWC24_CLEANUP_SOCKET = 0x07,
    IOCTL_NWC24_LOCK_SOCKET = 0x08,
    IOCTL_NWC24_UNLOCK_SOCKET = 0x09,
    IOCTL_NWC24_CHECK_MAIL_NOW = 0x0A,
    IOCTL_NWC24_SEND_MAIL_NOW = 0x0B,
    IOCTL_NWC24_RECEIVE_MAIL_NOW = 0x0C,
    IOCTL_NWC24_SAVE_MAIL_NOW = 0x0D,
    IOCTL_NWC24_DOWNLOAD_NOW_EX = 0x0E,
    IOCTL_NWC24_REQUEST_GENERATED_USER_ID = 0x0F,
    IOCTL_NWC24_REQUEST_REGISTER_USER_ID = 0x10,
    IOCTL_NWC24_GET_SCHEDULAR_STAT = 0x1E,
    IOCTL_NWC24_SET_FILTER_MODE = 0x1F,
    IOCTL_NWC24_SET_DEBUG_MODE = 0x20,
    IOCTL_NWC24_KD_SET_NEXT_WAKEUP = 0x21,
    IOCTL_NWC24_SET_SCRIPT_MODE = 0x22,
    IOCTL_NWC24_REQUEST_SHUTDOWN = 0x28,
  };

  enum
  {
    MODEL_RVT = 0,
    MODEL_RVV = 0,
    MODEL_RVL = 1,
    MODEL_RVD = 2,
    MODEL_ELSE = 7
  };

  u8 GetAreaCode(const std::string& area) const;
  u8 GetHardwareModel(const std::string& model) const;

  s32 NWC24MakeUserID(u64* nwc24_id, u32 hollywood_id, u16 id_ctr, u8 hardware_model, u8 area_code);

  NWC24::NWC24Config config;
};
}  // namespace IOS::HLE::Device
