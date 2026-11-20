// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace Common
{
enum class SCSIDataDirection
{
  // Commands without a data phase use Out with an empty buffer
  Out = 0,
  In = 1,
};

enum class OpticalDriveStatus
{
  Ready,
  NoDisc,
  TrayOpen,
  BecomingReady,
  NotReadyOther,
};

struct SCSISense
{
  u8 status = 0;
  u8 sense_key = 0;
  u8 asc = 0;
  u8 ascq = 0;
};

struct OpticalDriveInfo
{
  std::string vendor;
  std::string product;
  std::string revision;

  // Empty if the operating system does not know it
  std::string serial_number;

  bool operator==(const OpticalDriveInfo&) const = default;
};

// OS abstraction over SCSI pass-through
class SCSIDevice final
{
public:
  SCSIDevice() = default;
  ~SCSIDevice();

  SCSIDevice(const SCSIDevice&) = delete;
  SCSIDevice& operator=(const SCSIDevice&) = delete;
  SCSIDevice(SCSIDevice&&) = delete;
  SCSIDevice& operator=(SCSIDevice&&) = delete;

  bool Open(const std::string& path, bool read_write);
  void Close();
  bool IsOpen() const;

  // Returns false if the OS rejects the request or the device returns a non-GOOD status. If sense
  // is not null, it receives the SCSI status and the decoded sense data
  bool SendCommand(std::span<const u8> cdb, std::span<u8> buffer, SCSIDataDirection direction,
                   u32 timeout_seconds = DEFAULT_TIMEOUT_SECONDS, SCSISense* sense = nullptr) const;

  // False on platforms that are still stubbed
  static bool IsSupported();

  static OpticalDriveStatus GetDriveStatus(const std::string& path);
  static std::vector<std::string> ListOpticalDrives();
  static std::optional<OpticalDriveInfo> GetDriveInfo(const std::string& path);
  static bool IsOpticalDrive(const std::string& path);

  // One name per drive, so differently written paths to the same drive compare equal
  // ("d:" and "D:", or "/dev/cdrom" and "/dev/sr0")
  static std::string GetCanonicalPath(const std::string& path);

  // Long enough to cover a disc spin-up.
  static constexpr u32 DEFAULT_TIMEOUT_SECONDS = 60;

private:
#if defined(_WIN32)
  // nullptr means closed; Open() maps INVALID_HANDLE_VALUE to nullptr
  void* m_handle = nullptr;
  std::string m_path;

#elif defined(__linux__) && !defined(__ANDROID__)
  // File descriptor of the opened device; -1 means closed
  int m_fd = -1;
  std::string m_path;

#else
  int m_fd = -1;
#endif
};
}  // namespace Common
