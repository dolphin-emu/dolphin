// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <optional>
#include <span>

#include "Common/CommonTypes.h"

namespace Common
{
class OpticalDrive;
}

namespace DiscIO::OmniDrive
{
constexpr u32 RAW_SECTOR_SIZE = 2064;
constexpr u32 USER_SECTOR_SIZE = 2048;

// Keeps each transfer under the 64 KiB limit of common Windows storage stacks
constexpr u32 MAX_TRANSFER_SECTORS_RAW = 31;
constexpr u32 MAX_TRANSFER_SECTORS_STD = 32;

struct FirmwareVersion
{
  u8 major = 0;
  u8 minor = 0;
  u8 patch = 0;
};

struct PhysicalFormat
{
  u8 book_type = 0;
  u8 num_layers = 0;
  bool opposite_track_path = false;
  u32 start_psn = 0;
  u32 end_psn = 0;
  u32 end_layer0_psn = 0;
};

struct DriveSpeeds
{
  u16 max_kbs = 0;
  u16 current_kbs = 0;
};

// Returns false if INQUIRY fails. Otherwise, version receives the OmniDrive firmware version, or
// std::nullopt if the drive does not run OmniDrive firmware
bool QueryFirmwareVersion(Common::OpticalDrive& drive, std::optional<FirmwareVersion>* version);

std::optional<PhysicalFormat> ReadPhysicalFormat(Common::OpticalDrive& drive);
std::optional<std::array<u8, 192>> ReadBCA(Common::OpticalDrive& drive);
std::optional<DriveSpeeds> ReadDriveSpeeds(Common::OpticalDrive& drive);

// SCSI SET CD SPEED with the read speed in KB/s and the write speed at maximum
bool SetCDSpeed(Common::OpticalDrive& drive, u16 speed_kbs);

// Sends speed_kbs to the drive as is, then returns the read speed the drive reports afterwards,
// which can differ from the request.
std::optional<u16> SetReadSpeed(Common::OpticalDrive& drive, u16 speed_kbs);

// SCSI START STOP UNIT. With wait set to false, the command returns before the transition completes
bool StartStopUnit(Common::OpticalDrive& drive, bool wait, bool spin, bool eject);

// SCSI PREVENT ALLOW MEDIUM REMOVAL. With prevent set to false, the tray can be opened again
bool PreventAllowMediumRemoval(Common::OpticalDrive& drive, bool prevent);

// out must hold num_sectors * RAW_SECTOR_SIZE bytes; num_sectors <= MAX_TRANSFER_SECTORS_RAW
bool ReadSectorsRaw(Common::OpticalDrive& drive, u32 lba, u32 num_sectors, std::span<u8> out,
                    bool force_unit_access = false);

// out must hold num_sectors * USER_SECTOR_SIZE bytes; num_sectors <= MAX_TRANSFER_SECTORS_STD
bool ReadSectorsStandard(Common::OpticalDrive& drive, u32 lba, u32 num_sectors, std::span<u8> out,
                         bool force_unit_access = false);
}  // namespace DiscIO::OmniDrive
