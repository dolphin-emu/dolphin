// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/OpticalDriveWatcher.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>
#include <numeric>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include <QTimer>

#include "Common/OpticalDrive.h"
#include "Common/SCSIDevice.h"
#include "DiscIO/OmniDrive.h"
#include "DiscIO/OpticalDiscBlob.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"

// How often the polled status of the watched drives is copied into the list
constexpr std::chrono::milliseconds STATUS_UPDATE_INTERVAL{100};

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

// info must hold the serial number, so two drives of the same model never share an entry
static CachedDrive& GetCacheEntry(const std::string& drive, const Common::OpticalDriveInfo& info)
{
  const auto [it, inserted] =
      s_drive_cache.try_emplace(drive, CachedDrive{info, std::nullopt, false});
  if (!inserted && it->second.info != info)
    it->second = CachedDrive{info, std::nullopt, false};
  return it->second;
}

// The serial number is std::nullopt until it is known
static Common::OpticalDriveInfo ReadDriveInfo(const std::string& drive)
{
  return Common::SCSIDevice::GetDriveInfo(drive).value_or(Common::OpticalDriveInfo{});
}

// Fills in a serial number the operating system does not know yet by asking the drive, once it is
// no longer becoming ready. If it can't be read, it's left empty and asked for again on the next
// scan. Returns false if a stop is requested on stoken during that wait
static bool CompleteDriveInfo(const std::string& drive, Common::OpticalDriveInfo* info,
                              std::stop_token stoken)
{
  if (info->serial_number)
    return true;

  if (!DiscIO::WaitWhileBecomingReady(drive, stoken))
    return false;

  info->serial_number = Common::SCSIDevice::ReadSerialNumber(drive).value_or(std::string());
  return true;
}

OpticalDriveWatcher::OpticalDriveWatcher(QObject* parent) : QObject(parent)
{
  m_status_timer = new QTimer(this);
  connect(m_status_timer, &QTimer::timeout, this, &OpticalDriveWatcher::UpdateStatuses);
}

OpticalDriveWatcher::~OpticalDriveWatcher()
{
  Stop();
  m_workers.clear();
}

void OpticalDriveWatcher::Refresh()
{
  Stop();
  m_stop_source = std::stop_source();

  m_drives.clear();
  const std::vector<std::string> paths = Common::SCSIDevice::ListOpticalDrives();

  // Drives that are no longer listed are let go, which closes them once nothing else holds them
  std::vector<std::shared_ptr<Common::OpticalDrive>> optical_drives;
  for (const std::string& path : paths)
    optical_drives.push_back(Common::OpticalDrive::Acquire(path));
  m_optical_drives = std::move(optical_drives);

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

    // Without the serial number, the cache entry is looked up by the worker once it has read it
    if (info.serial_number)
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
      QueryFirmware(i, m_stop_source.get_token());
  }
}

void OpticalDriveWatcher::Stop()
{
  m_stop_source.request_stop();
  StopWatchingStatus();
}

void OpticalDriveWatcher::WatchStatus(int index)
{
  WatchStatuses({index});
}

void OpticalDriveWatcher::WatchAllStatuses()
{
  std::vector<int> indices(m_drives.size());
  std::iota(indices.begin(), indices.end(), 0);
  WatchStatuses(std::move(indices));
}

void OpticalDriveWatcher::StopWatchingStatus()
{
  m_status_timer->stop();
  m_status_pollings.clear();
  m_status_indices.clear();
}

void OpticalDriveWatcher::WatchStatuses(std::vector<int> indices)
{
  if (indices == m_status_indices)
    return;

  StopWatchingStatus();
  m_status_indices = std::move(indices);
  if (m_status_indices.empty())
    return;

  for (const int index : m_status_indices)
  {
    m_status_pollings.push_back(m_optical_drives[index]->PollStatus());
    m_drives[index].status.reset();
    emit DriveUpdated(index);
  }
  m_status_timer->start(STATUS_UPDATE_INTERVAL);
}

void OpticalDriveWatcher::UpdateStatuses()
{
  for (const int index : m_status_indices)
  {
    const std::optional<Common::OpticalDriveStatus> status =
        m_optical_drives[index]->GetLastStatus();
    if (status && status != m_drives[index].status)
    {
      m_drives[index].status = status;
      emit DriveUpdated(index);
    }
  }
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
    label += QStringLiteral(" ...");
  else if (*drive.omnidrive)
    label += QStringLiteral(" [OmniDrive]");

  return label;
}

std::optional<OpticalDriveWatcher::Identity>
OpticalDriveWatcher::GetCachedIdentity(const std::string& drive)
{
  const Common::OpticalDriveInfo info = ReadDriveInfo(drive);
  if (!info.serial_number)
    return std::nullopt;

  std::lock_guard lock(s_drive_cache_mutex);
  const auto it = s_drive_cache.find(drive);
  if (it == s_drive_cache.end() || !it->second.firmware_queried || it->second.info != info)
    return std::nullopt;
  return Identity{it->second.info, it->second.omnidrive};
}

std::optional<OpticalDriveWatcher::Identity> OpticalDriveWatcher::Identify(const std::string& drive,
                                                                           std::stop_token stoken)
{
  Common::OpticalDriveInfo info = ReadDriveInfo(drive);
  if (!CompleteDriveInfo(drive, &info, stoken))
    return std::nullopt;

  {
    std::lock_guard lock(s_drive_cache_mutex);
    const CachedDrive& entry = GetCacheEntry(drive, info);
    if (entry.firmware_queried)
      return Identity{entry.info, entry.omnidrive};
  }

  std::optional<DiscIO::OmniDrive::FirmwareVersion> firmware;
  if (!DiscIO::QueryOpticalDriveFirmware(drive, &firmware, stoken))
    return std::nullopt;

  std::lock_guard lock(s_drive_cache_mutex);
  CachedDrive& entry = GetCacheEntry(drive, info);
  entry.omnidrive = firmware;
  entry.firmware_queried = true;
  return Identity{info, firmware};
}

void OpticalDriveWatcher::QueryFirmware(int index, std::stop_token stoken)
{
  const std::string& path = m_drives[index].path;
  std::unique_ptr<Common::AsyncWorkThreadSP>& worker = m_workers[path];
  if (!worker)
    worker = std::make_unique<Common::AsyncWorkThreadSP>("Optical Drive Watcher");

  worker->Push([this, index, path, stoken] {
    // A drive whose firmware cannot be queried gets no tag
    const std::optional<Identity> identity = Identify(path, stoken);

    const bool omnidrive = identity && identity->omnidrive.has_value();
    QueueOnObject(this, [this, index, stoken, omnidrive] {
      if (stoken.stop_requested())
        return;
      m_drives[index].omnidrive = omnidrive;
      emit DriveUpdated(index);
    });
  });
}
