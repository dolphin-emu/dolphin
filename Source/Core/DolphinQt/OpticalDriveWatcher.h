// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QObject>
#include <QString>

#include "Common/Flag.h"
#include "Common/SCSIDevice.h"
#include "Common/WorkQueueThread.h"
#include "DiscIO/OmniDrive.h"

// Lists the host's optical drives at once and fills in what is known about each from a drive cache
// that lasts until Dolphin exits. Only a drive new to the cache is queried for its firmware, once,
// on a worker thread, so the UI never waits for a drive and known drives are left alone.
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

    // Unknown until WatchStatus has checked the drive
    std::optional<Common::OpticalDriveStatus> status;
  };

  explicit OpticalDriveWatcher(QObject* parent = nullptr);
  ~OpticalDriveWatcher() override;

  void Refresh();
  void Stop();

  // Polls the status of the drive at index until StopWatchingStatus, Stop or the next WatchStatus
  // for another drive. Its previous status is forgotten until the first poll.
  void WatchStatus(int index);

  void StopWatchingStatus();

  const std::vector<Drive>& GetDrives() const;

  // "<identifier> (<vendor> <product>) <tag>", with "..." in place of the tag until the drive's
  // firmware is known, then "[OD]" if the drive runs OmniDrive firmware or empty otherwise
  static QString GetLabel(const Drive& drive);

  static std::optional<Identity> GetCachedIdentity(const std::string& drive);

  // Like GetCachedIdentity, but queries the firmware of a drive that is not cached yet, which may
  // wait for it to spin up (is_cancelled is polled meanwhile)
  static std::optional<Identity> Identify(const std::string& drive,
                                          const std::function<bool()>& is_cancelled = {});

signals:
  // The firmware or the status of the drive at index became known or changed
  void DriveUpdated(int index);

private:
  void QueryFirmware(int index, const std::shared_ptr<Common::Flag>& stopped);

  std::vector<Drive> m_drives;

  // Set to stop the workers started by the latest Refresh
  std::shared_ptr<Common::Flag> m_stopped = std::make_shared<Common::Flag>();

  // One worker per drive, so a drive that is spinning up does not hold up the others
  std::map<std::string, std::unique_ptr<Common::AsyncWorkThreadSP>> m_workers;

  // The drive whose status is polled, if any, and the flag that stops its polling
  std::optional<int> m_status_index;

  std::shared_ptr<Common::Flag> m_status_stopped = std::make_shared<Common::Flag>();
  std::unique_ptr<Common::AsyncWorkThreadSP> m_status_worker;
};
