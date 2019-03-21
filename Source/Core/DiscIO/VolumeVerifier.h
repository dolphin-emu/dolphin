// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
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
class SignedBlobReader;
}

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

  struct Result
  {
    std::string summary_text;
    std::vector<Problem> problems;
  };

  VolumeVerifier(const Volume& volume);
  void Start();
  void Process();
  u64 GetBytesProcessed() const;
  u64 GetTotalBytes() const;
  void Finish();
  const Result& GetResult() const;

private:
  void CheckPartitions();
  bool CheckPartition(const Partition& partition);  // Returns false if partition should be ignored
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

  void AddProblem(Severity severity, const std::string& text);

  const Volume& m_volume;
  Result m_result;
  bool m_is_tgc;
  bool m_is_datel;
  bool m_is_not_retail;

  bool m_started;
  bool m_done;
  u64 m_progress;
  u64 m_max_progress;
};

}  // namespace DiscIO
