// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/SCSIDevice.h"

#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace Common
{
SCSIDevice::~SCSIDevice()
{
  Close();
}

OpticalDriveStatus SCSIDevice::GetStatusFromSense(const SCSISense& sense)
{
  // Sense key 0x06 (UNIT ATTENTION): reports an event instead of the drive's state.
  // Assume it's becoming ready so the next check gets the actual drive state
  if (sense.sense_key == 0x06)
    return OpticalDriveStatus::BecomingReady;

  // Sense key 0x02 (NOT READY): drive can't execute media access commands right now
  if (sense.sense_key == 0x02)
  {
    // ASC 0x3A: no medium present (ASCQ 0x00: no disc; ASCQ 0x02: tray open)
    if (sense.asc == 0x3A)
      return sense.ascq == 0x02 ? OpticalDriveStatus::TrayOpen : OpticalDriveStatus::NoDisc;

    // ASC 0x04: logical unit not ready (ASCQ 0x00: cause unknown; ASCQ 0x01: becoming ready).
    // Drives briefly report ASCQ 0x00 between spinning up and being ready, so it counts as
    // becoming ready too
    if (sense.asc == 0x04 && (sense.ascq == 0x00 || sense.ascq == 0x01))
      return OpticalDriveStatus::BecomingReady;
  }
  return OpticalDriveStatus::NotReadyOther;
}

#if !defined(_WIN32) && (!defined(__linux__) || defined(__ANDROID__))
// Feature not implemented on this operating system, fall back to the stubs
bool SCSIDevice::IsSupported()
{
  return false;
}

bool SCSIDevice::Open(const std::string&, bool)
{
  return false;
}

void SCSIDevice::Close()
{
}

bool SCSIDevice::IsOpen() const
{
  return false;
}

bool SCSIDevice::SendCommand(std::span<const u8>, std::span<u8>, SCSIDataDirection, u32,
                             SCSISense*) const
{
  return false;
}

std::optional<OpticalDriveStatus> SCSIDevice::GetStatus() const
{
  return std::nullopt;
}

OpticalDriveStatus SCSIDevice::GetDriveStatus(const std::string&)
{
  return OpticalDriveStatus::NotReadyOther;
}

std::vector<std::string> SCSIDevice::ListOpticalDrives()
{
  return {};
}

std::optional<OpticalDriveInfo> SCSIDevice::GetDriveInfo(const std::string&)
{
  return std::nullopt;
}

std::optional<std::string> SCSIDevice::ReadSerialNumber(const std::string&)
{
  return std::nullopt;
}

bool SCSIDevice::IsOpticalDrive(const std::string&)
{
  return false;
}

std::string SCSIDevice::GetCanonicalPath(const std::string& path)
{
  return path;
}
#endif
}  // namespace Common
