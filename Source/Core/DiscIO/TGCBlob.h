// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "DiscIO/Blob.h"

namespace DiscIO
{
static constexpr u32 TGC_MAGIC = 0xA2380FAE;

struct TGCHeader
{
  u32 magic;
  u32 unknown_1;
  u32 tgc_header_size;
  u32 disc_header_area_size;

  u32 fst_real_offset;
  u32 fst_size;
  u32 fst_max_size;
  u32 dol_real_offset;

  u32 dol_size;
  u32 file_area_real_offset;
  u32 unknown_2;
  u32 unknown_3;

  u32 unknown_4;
  u32 file_area_virtual_offset;
};

class TGCFileReader final : public BlobReader
{
public:
  static std::unique_ptr<TGCFileReader> Create(File::IOFile file);

  BlobType GetBlobType() const override { return BlobType::TGC; }

  u64 GetRawSize() const override { return m_size; }
  u64 GetDataSize() const override;
  DataSizeType GetDataSizeType() const override { return DataSizeType::Accurate; }

  u64 GetBlockSize() const override { return 0; }
  bool HasFastRandomAccessInBlock() const override { return true; }
  std::string GetCompressionMethod() const override { return {}; }
  std::optional<int> GetCompressionLevel() const override { return std::nullopt; }

  bool Read(u64 offset, u64 nbytes, u8* out_ptr) override;

private:
  TGCFileReader(File::IOFile file);

  File::IOFile m_file;
  u64 m_size;

  std::vector<u8> m_fst;

  // Stored as big endian in memory, regardless of the host endianness
  TGCHeader m_header = {};
};

}  // namespace DiscIO
