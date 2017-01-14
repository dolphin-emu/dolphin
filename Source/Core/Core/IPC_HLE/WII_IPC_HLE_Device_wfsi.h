// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include <mbedtls/aes.h>

#include "Common/CommonTypes.h"
#include "Core/IPC_HLE/ESFormats.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

class ARCUnpacker
{
public:
  ARCUnpacker() { Reset(); }
  void Reset();

  void AddBytes(const std::vector<u8>& bytes);

  using WriteCallback = std::function<void(const std::string&, const std::vector<u8>&)>;
  void Extract(const WriteCallback& callback);

private:
  std::vector<u8> m_whole_file;
};

class CWII_IPC_HLE_Device_wfsi : public IWII_IPC_HLE_Device
{
public:
  CWII_IPC_HLE_Device_wfsi(u32 device_id, const std::string& device_name);

  IPCCommandResult Open(u32 command_address, u32 mode) override;
  IPCCommandResult IOCtl(u32 command_address) override;
  IPCCommandResult IOCtlV(u32 command_address) override;

private:
  std::string m_device_name;

  mbedtls_aes_context m_aes_ctx;
  u8 m_aes_key[0x10] = {};
  u8 m_aes_iv[0x10] = {};

  TMDReader m_tmd;
  std::string m_base_extract_path;

  ARCUnpacker m_arc_unpacker;

  enum
  {
    IOCTL_WFSI_PREPARE_DEVICE = 0x02,

    IOCTL_WFSI_PREPARE_CONTENT = 0x03,
    IOCTL_WFSI_IMPORT_CONTENT = 0x04,
    IOCTL_WFSI_FINALIZE_CONTENT = 0x05,

    IOCTL_WFSI_DELETE_TITLE = 0x17,
    IOCTL_WFSI_IMPORT_TITLE = 0x2f,

    IOCTL_WFSI_INIT = 0x81,
    IOCTL_WFSI_SET_DEVICE_NAME = 0x82,

    IOCTL_WFSI_PREPARE_PROFILE = 0x86,
    IOCTL_WFSI_IMPORT_PROFILE = 0x87,
    IOCTL_WFSI_FINALIZE_PROFILE = 0x88,

    IOCTL_WFSI_APPLY_TITLE_PROFILE = 0x89,
  };
};
