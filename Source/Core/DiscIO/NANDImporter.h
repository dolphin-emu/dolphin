// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace DiscIO
{
class NANDImporter final
{
public:
  NANDImporter();
  ~NANDImporter();

  // Extract a NAND image to the configured NAND root.
  // If the associated OTP/SEEPROM dump (keys.bin) is not included in the image,
  // get_otp_dump_path will be called to get a path to it.
  void ImportNANDBin(const std::string& path_to_bin, std::function<void()> update_callback,
                     std::function<std::string()> get_otp_dump_path);
  bool ExtractCertificates(const std::string& nand_root);

private:
#pragma pack(push, 1)
  struct NANDFSTEntry
  {
    char name[12];
    u8 mode;   // 0x0C
    u8 attr;   // 0x0D
    u16 sub;   // 0x0E
    u16 sib;   // 0x10
    u32 size;  // 0x12
    u16 x1;    // 0x16
    u16 uid;   // 0x18
    u16 gid;   // 0x1A
    u32 x3;    // 0x1C
  };
#pragma pack(pop)

  bool ReadNANDBin(const std::string& path_to_bin, std::function<std::string()> get_otp_dump_path);
  void FindSuperblock();
  std::string GetPath(const NANDFSTEntry& entry, const std::string& parent_path);
  std::string FormatDebugString(const NANDFSTEntry& entry);
  void ProcessEntry(u16 entry_number, const std::string& parent_path);
  void ProcessFile(const NANDFSTEntry& entry, const std::string& parent_path);
  void ProcessDirectory(const NANDFSTEntry& entry, const std::string& parent_path);
  void ExportKeys(const std::string& nand_root);

  std::vector<u8> m_nand;
  std::vector<u8> m_nand_keys;
  size_t m_nand_fat_offset = 0;
  size_t m_nand_fst_offset = 0;
  std::function<void()> m_update_callback;
  size_t m_nand_root_length = 0;
};
}  // namespace DiscIO
