// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/OpticalDrive.h"

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/SCSIDevice.h"
#include "Common/Thread.h"

namespace Common
{
// Instances by canonical drive path. An entry expires with the last holder of its drive
static std::mutex s_drives_mutex;
static std::map<std::string, std::weak_ptr<OpticalDrive>> s_drives;

OpticalDrive::StatusPolling::StatusPolling(std::shared_ptr<OpticalDrive> drive)
    : m_drive(std::move(drive))
{
}

OpticalDrive::StatusPolling::~StatusPolling()
{
  Reset();
}

OpticalDrive::StatusPolling& OpticalDrive::StatusPolling::operator=(StatusPolling&& other) noexcept
{
  if (this != &other)
  {
    Reset();
    m_drive = std::move(other.m_drive);
  }
  return *this;
}

void OpticalDrive::StatusPolling::Reset()
{
  if (m_drive)
  {
    m_drive->StopPolling();
    m_drive.reset();
  }
}

OpticalDrive::OpticalDrive(std::string path) : m_path(std::move(path))
{
}

OpticalDrive::~OpticalDrive() = default;

std::shared_ptr<OpticalDrive> OpticalDrive::Acquire(const std::string& path)
{
  const std::string canonical_path = SCSIDevice::GetCanonicalPath(path);

  std::lock_guard lock(s_drives_mutex);
  std::erase_if(s_drives, [](const auto& entry) { return entry.second.expired(); });

  std::weak_ptr<OpticalDrive>& entry = s_drives[canonical_path];
  std::shared_ptr<OpticalDrive> drive = entry.lock();
  if (!drive)
  {
    drive = std::shared_ptr<OpticalDrive>(new OpticalDrive(canonical_path));
    entry = drive;
  }
  return drive;
}

const std::string& OpticalDrive::GetPath() const
{
  return m_path;
}

bool OpticalDrive::WaitForOpen(std::stop_token stoken)
{
  std::unique_lock lock(m_open_mutex);
  if (m_open)
    return true;

  if (!m_opening)
  {
    // After a failed open, or after a failed command closed the handle, don't open the drive again
    // until OPEN_RETRY_INTERVAL has passed, so a failing drive isn't reopened on every poll
    if (m_last_failure && std::chrono::steady_clock::now() < *m_last_failure + OPEN_RETRY_INTERVAL)
    {
      return false;
    }
    StartOpening();
  }

  // Returns false at once if a stop is requested before the drive has finished opening
  if (!m_open_finished.wait(lock, std::move(stoken), [this] { return !m_opening; }))
    return false;
  return m_open;
}

void OpticalDrive::StartOpening()
{
  m_opening = true;

  // The open can block for as long as the disc takes to spin up, so it runs on a detached thread
  // that keeps the drive alive, and nothing ever has to wait for it to finish
  std::thread([drive = shared_from_this()] {
    Common::SetCurrentThreadName("Optical Drive Opener");

    bool opened;
    {
      std::lock_guard command_lock(drive->m_command_mutex);
      opened = drive->m_device.IsOpen() || drive->m_device.Open(drive->m_path, true);
    }

    {
      std::lock_guard open_lock(drive->m_open_mutex);
      drive->m_opening = false;
      drive->m_open = opened;
      if (opened)
        drive->m_last_failure.reset();
      else
        drive->m_last_failure = std::chrono::steady_clock::now();
    }
    drive->m_open_finished.notify_all();
  }).detach();
}

bool OpticalDrive::SendCommand(std::span<const u8> cdb, std::span<u8> buffer,
                               SCSIDataDirection direction, u32 timeout_seconds, SCSISense* sense)
{
  if (!WaitForOpen())
    return false;

  std::lock_guard lock(m_command_mutex);
  if (!m_device.IsOpen())
    return false;

  SCSISense decoded;
  const bool success = m_device.SendCommand(cdb, buffer, direction, timeout_seconds, &decoded);
  if (sense != nullptr)
    *sense = decoded;

  // The handle may no longer be usable, e.g. after the drive was unplugged. A refusal for lack of
  // permission stays the same with a new handle
  if (!decoded.delivered && !decoded.refused)
    CloseLocked();
  return success;
}

OpticalDriveStatus OpticalDrive::QueryStatus()
{
  if (!WaitForOpen())
  {
    // A handle of its own tells why the drive cannot be opened
    const OpticalDriveStatus status = SCSIDevice::GetDriveStatus(m_path);
    RecordStatus(status);
    return status;
  }

  std::lock_guard lock(m_command_mutex);
  return QueryStatusLocked();
}

std::optional<OpticalDriveStatus> OpticalDrive::TryQueryStatus()
{
  std::unique_lock lock(m_command_mutex, std::try_to_lock);
  if (lock.owns_lock() && m_device.IsOpen())
  {
    if (const std::optional<OpticalDriveStatus> status = m_device.GetStatus())
    {
      RecordStatus(*status);
      return status;
    }
  }
  return GetLastStatus();
}

OpticalDriveStatus OpticalDrive::QueryStatusLocked()
{
  std::optional<OpticalDriveStatus> status = m_device.GetStatus();
  if (!status)
  {
    WARN_LOG_FMT(COMMON, "Could not send TEST UNIT READY to {}, opening it again", m_path);
    CloseLocked();
    status = SCSIDevice::GetDriveStatus(m_path);
  }
  RecordStatus(*status);
  return *status;
}

void OpticalDrive::CloseLocked()
{
  m_device.Close();

  std::lock_guard lock(m_open_mutex);
  m_open = false;
  m_last_failure = std::chrono::steady_clock::now();
}

std::optional<OpticalDriveStatus> OpticalDrive::GetLastStatus() const
{
  std::lock_guard lock(m_status_mutex);
  return m_last_status;
}

void OpticalDrive::RecordStatus(OpticalDriveStatus status)
{
  std::lock_guard lock(m_status_mutex);
  if (m_polling_count > 0)
    m_last_status = status;
}

OpticalDrive::StatusPolling OpticalDrive::PollStatus()
{
  {
    std::lock_guard lock(m_status_mutex);

    ++m_polling_count;

    // The thread keeps the drive alive until the last StatusPolling is gone, so nothing has to wait
    // for a status query in flight to finish
    if (!m_polling_thread_running)
    {
      m_polling_thread_running = true;
      std::thread([drive = shared_from_this()] { drive->RunStatusPolling(); }).detach();
    }
  }
  return StatusPolling(shared_from_this());
}

void OpticalDrive::StopPolling()
{
  {
    std::lock_guard lock(m_status_mutex);
    if (--m_polling_count == 0)
      m_last_status.reset();
  }
  m_polling_changed.notify_all();
}

void OpticalDrive::RunStatusPolling()
{
  Common::SetCurrentThreadName("Optical Drive Status Poller");

  std::unique_lock lock(m_status_mutex);
  while (m_polling_count > 0)
  {
    const std::chrono::steady_clock::time_point next_poll =
        std::chrono::steady_clock::now() + STATUS_POLL_INTERVAL;

    lock.unlock();
    QueryStatus();
    lock.lock();

    m_polling_changed.wait_until(lock, next_poll, [this] { return m_polling_count == 0; });
  }
  m_polling_thread_running = false;
}
}  // namespace Common
