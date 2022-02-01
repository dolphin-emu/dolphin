// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Based off of tachtig/twintig http://git.infradead.org/?p=users/segher/wii.git
// Copyright 2007,2008  Segher Boessenkool  <segher@kernel.crashing.org>
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Lazy.h"
#include "Common/Swap.h"

namespace WiiSave
{
enum
{
  BLOCK_SZ = 0x40,
  ICON_SZ = 0x1200,
  BNR_SZ = 0x60a0,
  FULL_BNR_MIN = 0x72a0,  // BNR_SZ + 1*ICON_SZ
  FULL_BNR_MAX = 0xF0A0,  // BNR_SZ + 8*ICON_SZ
  BK_LISTED_SZ = 0x70,    // Size before rounding to nearest block
  SIG_SZ = 0x40,
  FULL_CERT_SZ = 0x3C0,  // SIG_SZ + NG_CERT_SZ + AP_CERT_SZ + 0x80?

  BK_HDR_MAGIC = 0x426B0001,
  FILE_HDR_MAGIC = 0x03adf17e
};

#pragma pack(push, 1)
struct Header
{
  Common::BigEndianValue<u64> tid;
  Common::BigEndianValue<u32> banner_size;  // (0x72A0 or 0xF0A0, also seen 0xBAA0)
  u8 permissions;
  u8 unk1;                   // maybe permissions is a be16
  std::array<u8, 0x10> md5;  // md5 of plaintext header with md5 blanker applied
  Common::BigEndianValue<u16> unk2;
  u8 banner[FULL_BNR_MAX];
};
static_assert(sizeof(Header) == 0xf0c0, "Header has an incorrect size");

struct BkHeader
{
  Common::BigEndianValue<u32> size;  // 0x00000070
  // u16 magic;  // 'Bk'
  // u16 magic2; // or version (0x0001)
  Common::BigEndianValue<u32> magic;  // 0x426B0001
  Common::BigEndianValue<u32> ngid;
  Common::BigEndianValue<u32> number_of_files;
  Common::BigEndianValue<u32> size_of_files;
  Common::BigEndianValue<u32> unk1;
  Common::BigEndianValue<u32> unk2;
  Common::BigEndianValue<u32> total_size;
  std::array<u8, 64> unk3;
  Common::BigEndianValue<u64> tid;
  std::array<u8, 6> mac_address;
  std::array<u8, 0x12> padding;
};
static_assert(sizeof(BkHeader) == 0x80, "BkHeader has an incorrect size");

struct FileHDR
{
  Common::BigEndianValue<u32> magic;  // 0x03adf17e
  Common::BigEndianValue<u32> size;
  u8 permissions;
  u8 attrib;
  u8 type;  // (1=file, 2=directory)
  std::array<char, 0x40> name;
  std::array<u8, 5> padding;
  std::array<u8, 0x10> iv;
  std::array<u8, 0x20> unk;
};
static_assert(sizeof(FileHDR) == 0x80, "FileHDR has an incorrect size");
#pragma pack(pop)

class Storage
{
public:
  struct SaveFile
  {
    enum class Type : u8
    {
      File = 1,
      Directory = 2,
    };
    u8 mode = 0;
    u8 attributes = 0;
    Type type{};
    /// File name relative to the title data directory.
    std::string path;
    // Only valid for regular (i.e. non-directory) files.
    Common::Lazy<std::optional<std::vector<u8>>> data;
  };

  virtual ~Storage() = default;
  virtual bool SaveExists() const = 0;
  virtual bool EraseSave() = 0;
  virtual std::optional<Header> ReadHeader() = 0;
  virtual std::optional<BkHeader> ReadBkHeader() = 0;
  virtual std::optional<std::vector<SaveFile>> ReadFiles() = 0;
  virtual bool WriteHeader(const Header& header) = 0;
  virtual bool WriteBkHeader(const BkHeader& bk_header) = 0;
  virtual bool WriteFiles(const std::vector<SaveFile>& files) = 0;
};
}  // namespace WiiSave
