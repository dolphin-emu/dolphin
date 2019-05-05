// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <mbedtls/md5.h>
#include <mbedtls/sha1.h>

#include "Common/CommonTypes.h"
#include "DiscIO/DiscScrubber.h"
#include "DiscIO/Volume.h"

// To be used as follows:
//
// VolumeVerifier verifier(volume);
// verifier.Start();
// while (verifier.GetBytesProcessed() != verifier.GetTotalBytes())
//   verifier.Process();
// verifier.Finish();
// auto result = verifier.GetResult();
//
// Start, Process and Finish may take some time to run.
//
// GetResult() can be called before the processing is finished, but the result will be incomplete.

namespace IOS::ES
{
struct Content;
class SignedBlobReader;
}  // namespace IOS::ES

namespace DiscIO
{
class FileInfo;

class VolumeVerifier final
{
public:
  enum class Severity
  {
    None,  // Only used internally
    Low,
    Medium,
    High,
  };

  struct Problem
  {
    Severity severity;
    std::string text;
  };

  template <typename T>
  struct Hashes
  {
    T crc32;
    T md5;
    T sha1;
  };

  struct Result
  {
    Hashes<std::vector<u8>> hashes;
    std::string summary_text;
    std::vector<Problem> problems;
  };

  VolumeVerifier(const Volume& volume, Hashes<bool> hashes_to_calculate);
  void Start();
  void Process();
  u64 GetBytesProcessed() const;
  u64 GetTotalBytes() const;
  void Finish();
  const Result& GetResult() const;

private:
  struct BlockToVerify
  {
    Partition partition;
    u64 offset;
    u64 block_index;
  };

  void CheckPartitions();
  bool CheckPartition(const Partition& partition);  // Returns false if partition should be ignored
  std::string GetPartitionName(std::optional<u32> type) const;
  void CheckCorrectlySigned(const Partition& partition, const std::string& error_text);
  bool IsDebugSigned() const;
  bool ShouldHaveChannelPartition() const;
  bool ShouldHaveInstallPartition() const;
  bool ShouldHaveMasterpiecePartitions() const;
  bool ShouldBeDualLayer() const;
  void CheckDiscSize();
  u64 GetBiggestUsedOffset();
  u64 GetBiggestUsedOffset(const FileInfo& file_info) const;
  void CheckMisc();
  void SetUpHashing();
  bool CheckContentIntegrity(const IOS::ES::Content& content);

  void AddProblem(Severity severity, const std::string& text);

  const Volume& m_volume;
  Result m_result;
  bool m_is_tgc;
  bool m_is_datel;
  bool m_is_not_retail;

  Hashes<bool> m_hashes_to_calculate;
  bool m_calculating_any_hash;
  unsigned long m_crc32_context;
  mbedtls_md5_context m_md5_context;
  mbedtls_sha1_context m_sha1_context;

  DiscScrubber m_scrubber;
  std::vector<u64> m_content_offsets;
  u16 m_content_index = 0;
  std::vector<BlockToVerify> m_blocks;
  size_t m_block_index = 0;  // Index in m_blocks, not index in a specific partition
  std::map<Partition, size_t> m_block_errors;
  std::map<Partition, size_t> m_unused_block_errors;

  bool m_started;
  bool m_done;
  u64 m_progress;
  u64 m_max_progress;
};

}  // namespace DiscIO
