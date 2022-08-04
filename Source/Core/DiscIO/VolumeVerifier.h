// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <future>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <mbedtls/md5.h>

#include "Common/CommonTypes.h"
#include "Common/Crypto/SHA1.h"
#include "Core/IOS/ES/Formats.h"
#include "DiscIO/DiscScrubber.h"
#include "DiscIO/Volume.h"

// To be used as follows:
//
// VolumeVerifier verifier(volume, redump_verification, hashes_to_calculate);
// verifier.Start();
// while (verifier.GetBytesProcessed() != verifier.GetTotalBytes())
//   verifier.Process();
// verifier.Finish();
// auto result = verifier.GetResult();
//
// Start, Process and Finish may take some time to run.
//
// GetResult() can be called before the processing is finished, but the result will be incomplete.

namespace DiscIO
{
template <typename T>
struct Hashes
{
  T crc32;
  T md5;
  T sha1;
};

class RedumpVerifier final
{
public:
  enum class Status
  {
    Unknown,
    GoodDump,
    BadDump,
    Error,
  };

  struct Result
  {
    Status status = Status::Unknown;
    std::string message;
  };

  void Start(const Volume& volume);
  Result Finish(const Hashes<std::vector<u8>>& hashes);

private:
  enum class DownloadStatus
  {
    NotAttempted,
    Success,
    Fail,
    FailButOldCacheAvailable,
    SystemNotAvailable,
  };

  struct DownloadState
  {
    std::mutex mutex;
    DownloadStatus status = DownloadStatus::NotAttempted;
  };

  struct PotentialMatch
  {
    u64 size = 0;
    Hashes<std::vector<u8>> hashes;
  };

  static DownloadStatus DownloadDatfile(const std::string& system, DownloadStatus old_status);
  static std::vector<u8> ReadDatfile(const std::string& system);
  std::vector<PotentialMatch> ScanDatfile(const std::vector<u8>& data, const std::string& system);

  std::string m_game_id;
  u16 m_revision = 0;
  u8 m_disc_number = 0;
  u64 m_size = 0;

  std::future<std::vector<PotentialMatch>> m_future;
  Result m_result;

  static DownloadState m_gc_download_state;
  static DownloadState m_wii_download_state;
};

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

  struct Result
  {
    Hashes<std::vector<u8>> hashes;
    std::string summary_text;
    std::vector<Problem> problems;
    RedumpVerifier::Result redump;
  };

  VolumeVerifier(const Volume& volume, bool redump_verification, Hashes<bool> hashes_to_calculate);
  ~VolumeVerifier();

  static Hashes<bool> GetDefaultHashesToCalculate();
  void Start();
  void Process();
  u64 GetBytesProcessed() const;
  u64 GetTotalBytes() const;
  void Finish();
  const Result& GetResult() const;

private:
  struct GroupToVerify
  {
    Partition partition;
    u64 offset;
    size_t block_index_start;
    size_t block_index_end;
  };

  std::vector<Partition> CheckPartitions();
  bool CheckPartition(const Partition& partition);  // Returns false if partition should be ignored
  std::string GetPartitionName(std::optional<u32> type) const;
  bool IsDebugSigned() const;
  bool ShouldHaveChannelPartition() const;
  bool ShouldHaveInstallPartition() const;
  bool ShouldHaveMasterpiecePartitions() const;
  bool ShouldBeDualLayer() const;
  void CheckVolumeSize();
  void CheckMisc();
  void CheckSuperPaperMario();
  void SetUpHashing();
  void WaitForAsyncOperations() const;
  bool ReadChunkAndWaitForAsyncOperations(u64 bytes_to_read);

  void AddProblem(Severity severity, std::string text);

  const Volume& m_volume;
  Result m_result;
  bool m_is_tgc = false;
  bool m_is_datel = false;
  bool m_is_not_retail = false;

  bool m_redump_verification;
  RedumpVerifier m_redump_verifier;

  bool m_read_errors_occurred = false;

  Hashes<bool> m_hashes_to_calculate{};
  bool m_calculating_any_hash = false;
  u32 m_crc32_context = 0;
  mbedtls_md5_context m_md5_context{};
  std::unique_ptr<Common::SHA1::Context> m_sha1_context;

  u64 m_excess_bytes = 0;
  std::vector<u8> m_data;
  std::future<void> m_crc32_future;
  std::future<void> m_md5_future;
  std::future<void> m_sha1_future;
  std::future<void> m_content_future;
  std::future<void> m_group_future;

  DiscScrubber m_scrubber;
  IOS::ES::TicketReader m_ticket;
  std::vector<u64> m_content_offsets;
  u16 m_content_index = 0;
  std::vector<GroupToVerify> m_groups;
  size_t m_group_index = 0;  // Index in m_groups, not index in a specific partition
  std::map<Partition, size_t> m_block_errors;
  std::map<Partition, size_t> m_unused_block_errors;

  u64 m_biggest_referenced_offset = 0;
  u64 m_biggest_verified_offset = 0;

  bool m_started = false;
  bool m_done = false;
  u64 m_progress = 0;
  u64 m_max_progress = 0;
  DataSizeType m_data_size_type;
};

}  // namespace DiscIO
