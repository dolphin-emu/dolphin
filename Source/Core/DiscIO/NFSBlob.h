// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Crypto/AES.h"
#include "Common/IOFile.h"
#include "DiscIO/Blob.h"

// This is the file format used for Wii games released on the Wii U eShop.

namespace DiscIO
{
static constexpr u32 NFS_MAGIC = 0x53474745;  // "EGGS" (byteswapped to little endian)

struct NFSLBARange
{
  u32 start_block;
  u32 num_blocks;
};

struct NFSHeader
{
  u32 magic;  // EGGS
  u32 version;
  u32 unknown_1;
  u32 unknown_2;
  u32 lba_range_count;
  std::array<NFSLBARange, 61> lba_ranges;
  u32 end_magic;  // SGGE
};
static_assert(sizeof(NFSHeader) == 0x200);

class NFSFileReader : public BlobReader
{
public:
  static std::unique_ptr<NFSFileReader> Create(File::IOFile first_file,
                                               const std::string& directory_path);

  BlobType GetBlobType() const override { return BlobType::NFS; }

  u64 GetRawSize() const override;
  u64 GetDataSize() const override;
  DataSizeType GetDataSizeType() const override { return DataSizeType::LowerBound; }

  u64 GetBlockSize() const override { return BLOCK_SIZE; }
  bool HasFastRandomAccessInBlock() const override { return false; }
  std::string GetCompressionMethod() const override { return {}; }
  std::optional<int> GetCompressionLevel() const override { return std::nullopt; }

  bool Read(u64 offset, u64 nbytes, u8* out_ptr) override;

private:
  using Key = std::array<u8, Common::AES::Context::KEY_SIZE>;
  static constexpr u32 BLOCK_SIZE = 0x8000;
  static constexpr u32 MAX_FILE_SIZE = 0xFA00000;

  static bool ReadKey(const std::string& path, const std::string& directory, Key* key_out);
  static std::vector<NFSLBARange> GetLBARanges(const NFSHeader& header);
  static std::vector<File::IOFile> OpenFiles(const std::string& directory, File::IOFile first_file,
                                             u64 expected_raw_size, u64* raw_size_out);
  static u64 CalculateExpectedRawSize(const std::vector<NFSLBARange>& lba_ranges);
  static u64 CalculateExpectedDataSize(const std::vector<NFSLBARange>& lba_ranges);

  NFSFileReader(std::vector<NFSLBARange> lba_ranges, std::vector<File::IOFile> files, Key key,
                u64 raw_size);

  u64 ToPhysicalBlockIndex(u64 logical_block_index);
  bool ReadEncryptedBlock(u64 physical_block_index);
  void DecryptBlock(u64 logical_block_index);
  bool ReadAndDecryptBlock(u64 logical_block_index);

  std::array<u8, BLOCK_SIZE> m_current_block_encrypted;
  std::array<u8, BLOCK_SIZE> m_current_block_decrypted;
  u64 m_current_logical_block_index = std::numeric_limits<u64>::max();

  std::vector<NFSLBARange> m_lba_ranges;
  std::vector<File::IOFile> m_files;
  std::unique_ptr<Common::AES::Context> m_aes_context;
  u64 m_raw_size;
  u64 m_data_size;
};

}  // namespace DiscIO
