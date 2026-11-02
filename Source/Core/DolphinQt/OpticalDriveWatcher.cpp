// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/OpticalDriveWatcher.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "Common/SCSIDevice.h"
#include "DiscIO/OmniDrive.h"
#include "DiscIO/OpticalDiscBlob.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"

// How often a watched drive's status is checked again
constexpr std::chrono::milliseconds DRIVE_POLL_INTERVAL{1000};

// How often a waiting worker checks whether it should stop
constexpr std::chrono::milliseconds STOP_CHECK_INTERVAL{250};

namespace
{
struct CachedDrive
{
  Common::OpticalDriveInfo info;
  std::optional<DiscIO::OmniDrive::FirmwareVersion> omnidrive;
  bool firmware_queried = false;
};
}  // namespace

static std::mutex s_drive_cache_mutex;
static std::map<std::string, CachedDrive> s_drive_cache;

static CachedDrive& GetCacheEntry(const std::string& drive, const Common::OpticalDriveInfo& info)
{
  const auto [it, inserted] =
      s_drive_cache.try_emplace(drive, CachedDrive{info, std::nullopt, false});
  if (!inserted && it->second.info != info)
    it->second = CachedDrive{info, std::nullopt, false};
  return it->second;
}

static Common::OpticalDriveInfo ReadDriveInfo(const std::string& drive)
{
  return Common::SCSIDevice::GetDriveInfo(drive).value_or(Common::OpticalDriveInfo{});
}

OpticalDriveWatcher::OpticalDriveWatcher(QObject* parent) : QObject(parent)
{
}

OpticalDriveWatcher::~OpticalDriveWatcher()
{
  Stop();
  m_workers.clear();
  m_status_worker.reset();
}

void OpticalDriveWatcher::Refresh()
{
  Stop();
  m_stopped = std::make_shared<Common::Flag>();

  m_drives.clear();
  const std::vector<std::string> paths = Common::SCSIDevice::ListOpticalDrives();

  {
    // Forget the drives this scan no longer finds, so they are queried again if they come back
    std::lock_guard lock(s_drive_cache_mutex);
    std::erase_if(s_drive_cache, [&paths](const auto& entry) {
      return std::ranges::find(paths, entry.first) == paths.end();
    });
  }

  for (const std::string& path : paths)
  {
    const Common::OpticalDriveInfo info = ReadDriveInfo(path);

    Drive drive;
    drive.path = path;
    drive.name = QStringLiteral("%1 %2")
                     .arg(QString::fromStdString(info.vendor), QString::fromStdString(info.product))
                     .simplified();
    {
      std::lock_guard lock(s_drive_cache_mutex);
      const CachedDrive& entry = GetCacheEntry(path, info);
      if (entry.firmware_queried)
        drive.omnidrive = entry.omnidrive.has_value();
    }
    m_drives.push_back(std::move(drive));
  }

  for (int i = 0; i < static_cast<int>(m_drives.size()); ++i)
  {
    if (!m_drives[i].omnidrive)
      QueryFirmware(i, m_stopped);
  }
}

void OpticalDriveWatcher::Stop()
{
  m_stopped->Set();
  StopWatchingStatus();
}

void OpticalDriveWatcher::WatchStatus(int index)
{
  if (m_status_index == index)
    return;

  StopWatchingStatus();
  m_status_index = index;
  const auto stopped = std::make_shared<Common::Flag>();
  m_status_stopped = stopped;

  m_drives[index].status.reset();
  emit DriveUpdated(index);

  if (!m_status_worker)
    m_status_worker = std::make_unique<Common::AsyncWorkThreadSP>("Optical Drive Status");

  m_status_worker->Push([this, index, path = m_drives[index].path, stopped] {
    std::optional<Common::OpticalDriveStatus> last_status;
    while (!stopped->IsSet())
    {
      const Common::OpticalDriveStatus status = Common::SCSIDevice::GetDriveStatus(path);
      if (status != last_status)
      {
        last_status = status;
        QueueOnObject(this, [this, index, stopped, status] {
          if (stopped->IsSet())
            return;
          m_drives[index].status = status;
          emit DriveUpdated(index);
        });
      }

      for (auto waited = std::chrono::milliseconds::zero();
           waited < DRIVE_POLL_INTERVAL && !stopped->IsSet(); waited += STOP_CHECK_INTERVAL)
      {
        std::this_thread::sleep_for(STOP_CHECK_INTERVAL);
      }
    }
  });
}

void OpticalDriveWatcher::StopWatchingStatus()
{
  m_status_stopped->Set();
  m_status_index.reset();
}

const std::vector<OpticalDriveWatcher::Drive>& OpticalDriveWatcher::GetDrives() const
{
  return m_drives;
}

QString OpticalDriveWatcher::GetLabel(const Drive& drive)
{
  QString label = QString::fromStdString(drive.path);
  if (!drive.name.isEmpty())
    label += QStringLiteral(" (%1)").arg(drive.name);

  if (!drive.omnidrive)
    label += QStringLiteral(" ... ");
  else if (*drive.omnidrive)
    label += QStringLiteral(" [OD]");
  else
    label += QStringLiteral("     ");  // A bit hacky, but keeps the menu width consistent

  return label;
}

std::optional<OpticalDriveWatcher::Identity>
OpticalDriveWatcher::GetCachedIdentity(const std::string& drive)
{
  const Common::OpticalDriveInfo info = ReadDriveInfo(drive);

  std::lock_guard lock(s_drive_cache_mutex);
  const auto it = s_drive_cache.find(drive);
  if (it == s_drive_cache.end() || !it->second.firmware_queried || it->second.info != info)
    return std::nullopt;
  return Identity{it->second.info, it->second.omnidrive};
}

std::optional<OpticalDriveWatcher::Identity>
OpticalDriveWatcher::Identify(const std::string& drive, const std::function<bool()>& is_cancelled)
{
  const Common::OpticalDriveInfo info = ReadDriveInfo(drive);
  {
    std::lock_guard lock(s_drive_cache_mutex);
    const CachedDrive& entry = GetCacheEntry(drive, info);
    if (entry.firmware_queried)
      return Identity{entry.info, entry.omnidrive};
  }

  std::optional<DiscIO::OmniDrive::FirmwareVersion> firmware;
  if (!DiscIO::QueryOpticalDriveFirmware(drive, &firmware, is_cancelled))
    return std::nullopt;

  std::lock_guard lock(s_drive_cache_mutex);
  CachedDrive& entry = GetCacheEntry(drive, info);
  entry.omnidrive = firmware;
  entry.firmware_queried = true;
  return Identity{info, firmware};
}

void OpticalDriveWatcher::QueryFirmware(int index, const std::shared_ptr<Common::Flag>& stopped)
{
  const std::string& path = m_drives[index].path;
  std::unique_ptr<Common::AsyncWorkThreadSP>& worker = m_workers[path];
  if (!worker)
    worker = std::make_unique<Common::AsyncWorkThreadSP>("Optical Drive Watcher");

  worker->Push([this, index, path, stopped] {
    // A drive whose firmware cannot be queried gets no tag
    const std::optional<Identity> identity =
        Identify(path, [&stopped] { return stopped->IsSet(); });

    const bool omnidrive = identity && identity->omnidrive.has_value();
    QueueOnObject(this, [this, index, stopped, omnidrive] {
      if (stopped->IsSet())
        return;
      m_drives[index].omnidrive = omnidrive;
      emit DriveUpdated(index);
    });
  });
}
