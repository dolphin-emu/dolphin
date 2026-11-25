// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include <QObject>
#include <QString>

#include "Common/OpticalDrive.h"
#include "Common/SCSIDevice.h"
#include "Common/WorkQueueThread.h"
#include "DiscIO/OmniDrive.h"

class QTimer;

// Lists the host's optical drives at once and fills in what is known about each from a drive cache
// that lasts until Dolphin exits. Only a drive new to the cache is queried for its firmware, once,
// on a worker thread, so the UI never waits for a drive and known drives receive no identification
// command. The watcher also holds every listed drive open and polls the status of the drive named
// by WatchStatus, or of every drive after WatchAllStatuses.
class OpticalDriveWatcher final : public QObject
{
  Q_OBJECT

public:
  struct Identity
  {
    Common::OpticalDriveInfo info;

    // Set if the drive runs OmniDrive firmware
    std::optional<DiscIO::OmniDrive::FirmwareVersion> omnidrive;
  };

  struct Drive
  {
    std::string path;
    QString name;

    // Unknown until the drive's firmware has been queried, then whether it runs OmniDrive firmware
    std::optional<bool> omnidrive;

    // Unknown until the drive's status has been polled since watching it started
    std::optional<Common::OpticalDriveStatus> status;
  };

  explicit OpticalDriveWatcher(QObject* parent = nullptr);
  ~OpticalDriveWatcher() override;

  void Refresh();
  void Stop();

  // Polls the status of the drive at index until StopWatchingStatus, Stop or the next WatchStatus
  // for another drive. Its previous status is forgotten until the first poll
  void WatchStatus(int index);

  // Like WatchStatus, but for every drive the latest Refresh found
  void WatchAllStatuses();

  void StopWatchingStatus();

  const std::vector<Drive>& GetDrives() const;

  // "<identifier> (<vendor> <product>) <tag>", with "..." in place of the tag until the drive's
  // firmware is known, then "[OmniDrive]" if the drive runs OmniDrive firmware or empty otherwise
  static QString GetLabel(const Drive& drive);

  static std::optional<Identity> GetCachedIdentity(const std::string& drive);

  // Like GetCachedIdentity, but queries the firmware and serial number of a drive that is not
  // cached yet, which may wait for it to spin up (a stop requested on stoken ends the wait)
  static std::optional<Identity> Identify(const std::string& drive, std::stop_token stoken = {});

signals:
  // The firmware or the status of the drive at index became known or changed
  void DriveUpdated(int index);

private:
  void QueryFirmware(int index, std::stop_token stoken);
  void WatchStatuses(std::vector<int> indices);
  void UpdateStatuses();

  std::vector<Drive> m_drives;

  // The drives in m_drives, in the same order, held so they stay open
  std::vector<std::shared_ptr<Common::OpticalDrive>> m_optical_drives;

  // Requesting a stop on it ends the waits of the jobs the latest Refresh queued,
  // and drops their results
  std::stop_source m_stop_source;

  // One worker per drive, so a drive that is spinning up does not hold up the others
  std::map<std::string, std::unique_ptr<Common::AsyncWorkThreadSP>> m_workers;

  // The drives whose status is polled, and their polling
  std::vector<int> m_status_indices;
  std::vector<Common::OpticalDrive::StatusPolling> m_status_pollings;

  // Copies the polled statuses into m_drives while drives are watched
  QTimer* m_status_timer;
};
