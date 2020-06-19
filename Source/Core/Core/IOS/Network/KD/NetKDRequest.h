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
    ///Information from http://forum.wiibrew.org/read.php?27,2146,3605
    #pragma pack(push, 1)
    struct WC24Record final
    {
        short record_index;
        short unknown_value_1;
        int unknown_value_2;
        char game_title[4];
        int unknown_value_3;
        int unknown_value_4;
        int unknown_value_5;
        int unknown_value_6;
        int unknown_value_7;
        int unknown_value_8;
        int unknown_value_9;
        int unknown_value_10;
        int unknown_value_11;
        int unknown_value_12;
        char unknown_value_13[128];
        char wc24_main_url[236];
        char wc24_sub_url_files[96];
    };
    #pragma pack(pop)
    #pragma pack(push, 1)
    struct WC24File final
    {
        char magic_word[4]; // 'WcDl'
        int unknown_value_1;
        int unknown_value_2;
        int unknown_value_3;
        short unknown_value_4;
        short wc24_header_file_offset;
        short wc24_record_count;
        short unknown_value_5;
    };
    #pragma pack(pop)
    #pragma pack(push, 1)
    struct WC24Header final
    {
        char game_title[4];
        u32 memory_address_1;
        u32 memory_address_2;
        u8 record_pointer;
        u8 padding[3];
    };
    #pragma pack(pop)
    WC24File WC24FileHeader;
    WC24Header WC24RecordHeader;
    WC24Record WC24Log;

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
