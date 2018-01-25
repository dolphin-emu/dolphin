// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string>
#include <vector>

#include <mbedtls/aes.h>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/IOS.h"

namespace IOS
{
namespace HLE
{
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

namespace Device
{
class WFSI : public Device
{
public:
  WFSI(Kernel& ios, const std::string& device_name);

  IPCCommandResult IOCtl(const IOCtlRequest& request) override;

private:
  u32 GetTmd(u16 group_id, u32 title_id, u64 subtitle_id, u32 address, u32* size) const;

  void SetCurrentTitleIdAndGroupId(u64 tid, u16 gid);
  void SetImportTitleIdAndGroupId(u64 tid, u16 gid);

  s32 CancelTitleImport(bool continue_install);
  s32 CancelPatchImport(bool continue_install);

  std::string m_device_name;

  mbedtls_aes_context m_aes_ctx;
  u8 m_aes_key[0x10] = {};
  u8 m_aes_iv[0x10] = {};

  IOS::ES::TMDReader m_tmd;
  std::string m_base_extract_path;

  u64 m_current_title_id;
  std::string m_current_title_id_str;
  u16 m_current_group_id;
  std::string m_current_group_id_str;
  u64 m_import_title_id;
  std::string m_import_title_id_str;
  u16 m_import_group_id;
  std::string m_import_group_id_str;

  // Set on IMPORT_TITLE_INIT when the next profile application should not delete
  // temporary install files.
  bool m_continue_install = false;

  // Set on IMPORT_TITLE_INIT to indicate that the install is a patch and not a
  // standalone title.
  enum PatchType
  {
    NOT_A_PATCH,
    PATCH_TYPE_1,
    PATCH_TYPE_2,
  };
  PatchType m_patch_type = NOT_A_PATCH;

  ARCUnpacker m_arc_unpacker;

  enum
  {
    IOCTL_WFSI_IMPORT_TITLE_INIT = 0x02,

    IOCTL_WFSI_PREPARE_CONTENT = 0x03,
    IOCTL_WFSI_IMPORT_CONTENT = 0x04,
    IOCTL_WFSI_IMPORT_CONTENT_END = 0x05,

    IOCTL_WFSI_FINALIZE_TITLE_INSTALL = 0x06,

    IOCTL_WFSI_DELETE_TITLE = 0x17,

    IOCTL_WFSI_GET_VERSION = 0x1b,

    IOCTL_WFSI_IMPORT_TITLE_CANCEL = 0x2f,

    IOCTL_WFSI_INIT = 0x81,
    IOCTL_WFSI_SET_DEVICE_NAME = 0x82,

    IOCTL_WFSI_PREPARE_PROFILE = 0x86,
    IOCTL_WFSI_IMPORT_PROFILE = 0x87,
    IOCTL_WFSI_IMPORT_PROFILE_END = 0x88,

    IOCTL_WFSI_APPLY_TITLE_PROFILE = 0x89,

    IOCTL_WFSI_GET_TMD = 0x8a,
    IOCTL_WFSI_GET_TMD_ABSOLUTE = 0x8b,

    IOCTL_WFSI_SET_FST_BUFFER = 0x8e,

    IOCTL_WFSI_NOOP = 0x8f,

    IOCTL_WFSI_LOAD_DOL = 0x90,

    IOCTL_WFSI_FINALIZE_PATCH_INSTALL = 0x91,

    IOCTL_WFSI_CHECK_HAS_SPACE = 0x95,
  };
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
