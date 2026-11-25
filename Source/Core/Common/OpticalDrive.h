// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/SCSIDevice.h"

namespace Common
{
// An optical drive, shared by everything in Dolphin that uses it. The drive is opened once, and
// the handle is kept for as long as anything holds the drive. Every command to the drive goes
// through this object, one at a time
class OpticalDrive final : public std::enable_shared_from_this<OpticalDrive>
{
public:
  // Keeps the drive's status polled while it exists
  class StatusPolling final
  {
  public:
    StatusPolling() = default;
    ~StatusPolling();

    StatusPolling(const StatusPolling&) = delete;
    StatusPolling& operator=(const StatusPolling&) = delete;
    StatusPolling(StatusPolling&& other) noexcept = default;
    StatusPolling& operator=(StatusPolling&& other) noexcept;

    void Reset();

  private:
    friend class OpticalDrive;
    explicit StatusPolling(std::shared_ptr<OpticalDrive> drive);

    std::shared_ptr<OpticalDrive> m_drive;
  };

  ~OpticalDrive();

  OpticalDrive(const OpticalDrive&) = delete;
  OpticalDrive& operator=(const OpticalDrive&) = delete;
  OpticalDrive(OpticalDrive&&) = delete;
  OpticalDrive& operator=(OpticalDrive&&) = delete;

  // The instance for the drive, shared with everything else holding it, or a new one
  static std::shared_ptr<OpticalDrive> Acquire(const std::string& path);

  // The canonical path of the drive (see SCSIDevice::GetCanonicalPath)
  const std::string& GetPath() const;

  // Waits until the drive is open, opening it on a thread of its own if it is not. Returns false at
  // once if the last open failed, or the handle was closed, less than OPEN_RETRY_INTERVAL ago. A
  // stop requested on stoken ends the wait, but the opening goes on
  bool WaitForOpen(std::stop_token stoken = {});

  // Calls SCSIDevice::SendCommand once the drive is open and the command in flight, if any, is
  // done. If the OS cannot deliver the command for any reason other than missing permission, this
  // closes the handle, and a later command opens the drive again
  bool SendCommand(std::span<const u8> cdb, std::span<u8> buffer, SCSIDataDirection direction,
                   u32 timeout_seconds = SCSIDevice::DEFAULT_TIMEOUT_SECONDS,
                   SCSISense* sense = nullptr);

  // Sends SCSI TEST UNIT READY once the drive is open and the command in flight, if any, is done.
  // If the drive cannot be opened, or the command cannot be delivered, SCSIDevice::GetDriveStatus
  // reads the status through a handle of its own
  OpticalDriveStatus QueryStatus();

  // Same as QueryStatus(), but sends only if the drive is open and no command is in flight.
  // Otherwise, or if the command cannot be delivered, it returns GetLastStatus() without waiting
  std::optional<OpticalDriveStatus> TryQueryStatus();

  // The status the last SCSI TEST UNIT READY reported since the drive's status started to be polled
  std::optional<OpticalDriveStatus> GetLastStatus() const;

  // Polls the drive's status every STATUS_POLL_INTERVAL until the returned object is destroyed
  [[nodiscard]] StatusPolling PollStatus();

  static constexpr std::chrono::milliseconds STATUS_POLL_INTERVAL{1000};
  static constexpr std::chrono::seconds OPEN_RETRY_INTERVAL{5};

private:
  explicit OpticalDrive(std::string path);

  // Must be called with m_open_mutex held
  void StartOpening();

  // Must be called with m_command_mutex held
  OpticalDriveStatus QueryStatusLocked();
  void CloseLocked();

  void RecordStatus(OpticalDriveStatus status);
  void StopPolling();
  void RunStatusPolling();

  const std::string m_path;

  // Serializes every command and guards m_device
  std::mutex m_command_mutex;
  SCSIDevice m_device;

  std::mutex m_open_mutex;
  std::condition_variable_any m_open_finished;
  bool m_opening = false;
  bool m_open = false;
  std::optional<std::chrono::steady_clock::time_point> m_last_failure;

  mutable std::mutex m_status_mutex;
  std::condition_variable m_polling_changed;
  int m_polling_count = 0;
  bool m_polling_thread_running = false;
  std::optional<OpticalDriveStatus> m_last_status;
};
}  // namespace Common
