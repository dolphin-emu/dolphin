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

  // False if the OS could not deliver the command; the fields above are not set then
  bool delivered = false;

  // Set with delivered = false when the OS refused the command for lack of permission
  bool refused = false;
};

struct OpticalDriveInfo
{
  std::string vendor;
  std::string product;
  std::string revision;

  // Empty string if the drive has none, std::nullopt until it is known
  std::optional<std::string> serial_number;

  bool operator==(const OpticalDriveInfo&) const = default;
};

// OS abstraction over SCSI passthrough
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

  // Sends SCSI TEST UNIT READY command through the open handle, with STATUS_PROBE_TIMEOUT_SECONDS
  // as its timeout, and logs nothing when the drive is not ready
  std::optional<OpticalDriveStatus> GetStatus() const;

  // False on platforms that are still stubbed
  static bool IsSupported();

  static std::vector<std::string> ListOpticalDrives();

  // Returns what the operating system knows about the drive
  static std::optional<OpticalDriveInfo> GetDriveInfo(const std::string& path);

  // Reads the drive's serial number, GetDriveInfo reports it from then on
  static std::optional<std::string> ReadSerialNumber(const std::string& path);

  static bool IsOpticalDrive(const std::string& path);

  // One name per drive, so differently written paths to the same drive compare equal
  // ("d:" and "D:", or "/dev/cdrom" and "/dev/sr0")
  static std::string GetCanonicalPath(const std::string& path);

  // Long enough to cover a disc spin-up.
  static constexpr u32 DEFAULT_TIMEOUT_SECONDS = 60;

  // Timeout for the TEST UNIT READY that reads drive status. Short, so a spinning-up drive is
  // reported as such at once instead of blocking until it is ready
  static constexpr u32 STATUS_PROBE_TIMEOUT_SECONDS = 1;

private:
  // OpticalDrive keeps one handle per drive open and sends every command through it
  friend class OpticalDrive;

  // Opens a handle of its own for a single SCSI TEST UNIT READY command
  static OpticalDriveStatus GetDriveStatus(const std::string& path);

  // Decodes drive status from the sense data of a TEST UNIT READY that returned a non-GOOD status
  static OpticalDriveStatus GetStatusFromSense(const SCSISense& sense);

#if defined(_WIN32)
  // nullptr means closed; Open() maps INVALID_HANDLE_VALUE to nullptr
  void* m_handle = nullptr;
  std::string m_path;

#elif defined(__linux__) && !defined(__ANDROID__)
  // File descriptor of the opened device; -1 means closed
  int m_fd = -1;
  std::string m_path;
#endif
};
}  // namespace Common
