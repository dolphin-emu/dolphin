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

  void ImportNANDBin(const std::string& path_to_bin,
                     std::function<void(size_t, size_t)> update_callback);
  void ExtractCertificates(const std::string& nand_root);

private:
#pragma pack(push, 1)
  struct NANDFSTEntry
  {
    u8 name[12];
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

  bool ReadNANDBin(const std::string& path_to_bin);
  void FindSuperblock();
  std::string GetPath(const NANDFSTEntry& entry, const std::string& parent_path);
  void ProcessEntry(u16 entry_number, const std::string& parent_path);
  void ProcessFile(const NANDFSTEntry& entry, const std::string& parent_path);
  void ProcessDirectory(const NANDFSTEntry& entry, const std::string& parent_path);
  void ExportKeys(const std::string& nand_root);
  void CountEntries(u16 entry_number);
  void UpdateStatus();

  std::vector<u8> m_nand;
  std::vector<u8> m_nand_keys;
  size_t m_nand_fat_offset = 0;
  size_t m_nand_fst_offset = 0;
  std::function<void(size_t, size_t)> m_update_callback;
  size_t m_total_entries = 0;
  size_t m_current_entry = 0;
};
}
