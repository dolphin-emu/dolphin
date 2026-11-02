// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Heavily based on redumper by superg
// https://github.com/superg/redumper

#include "DiscIO/OmniDrive.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/SCSIDevice.h"
#include "Common/Swap.h"

namespace DiscIO::OmniDrive
{
// Stores value at out in big-endian byte order
static void PutBE32(u8* out, u32 value)
{
  const u32 big_endian = Common::swap32(value);
  std::memcpy(out, &big_endian, sizeof(big_endian));
}

// Builds a 12-byte read CDB. Bytes 2-5 hold the LBA and bytes 6-9 the sector count (big-endian)
static std::array<u8, 12> MakeReadCDB(u8 opcode, u8 flags, u32 lba, u32 num_sectors)
{
  std::array<u8, 12> cdb{opcode, flags};
  PutBE32(&cdb[2], lba);
  PutBE32(&cdb[6], num_sectors);
  return cdb;
}

// SCSI READ DISC STRUCTURE (0xAD). Bytes 8 and 9 define the allocation length (big-endian)
static bool ReadDiscStructure(const Common::SCSIDevice& device, u8 format, std::span<u8> buffer)
{
  const std::array<u8, 12> cdb{0xAD,
                               0x00,
                               0x00,
                               0x00,
                               0x00,
                               0x00,
                               0x00,
                               format,
                               static_cast<u8>(buffer.size() >> 8),
                               static_cast<u8>(buffer.size())};
  return device.SendCommand(cdb, buffer, Common::SCSIDataDirection::In);
}

// SCSIINQUIRY (0x12)
static bool Inquiry(const Common::SCSIDevice& device, std::span<u8> buffer)
{
  const std::array<u8, 6> cdb{0x12, 0x00, 0x00, 0x00, static_cast<u8>(buffer.size()), 0x00};
  return device.SendCommand(cdb, buffer, Common::SCSIDataDirection::In);
}

// SCSI MODE SENSE (10) (0x5A). Bytes 7 and 8 define the allocation length (big-endian)
static bool ModeSense10(const Common::SCSIDevice& device, u8 page_code, std::span<u8> buffer)
{
  const std::array<u8, 10> cdb{0x5A,
                               0x00,
                               page_code,
                               0x00,
                               0x00,
                               0x00,
                               0x00,
                               static_cast<u8>(buffer.size() >> 8),
                               static_cast<u8>(buffer.size()),
                               0x00};
  return device.SendCommand(cdb, buffer, Common::SCSIDataDirection::In);
}

bool QueryFirmwareVersion(const Common::SCSIDevice& device, std::optional<FirmwareVersion>* version)
{
  std::array<u8, 96> buffer{};
  if (!Inquiry(device, buffer))
    return false;

  // The OmniDrive firmware signature starts at offset 74 (vendor specific reserved space) and is
  // followed by three semantic version bytes
  constexpr std::string_view SIGNATURE = "OmniDrive";
  constexpr std::size_t SIGNATURE_OFFSET = 74;
  if (std::memcmp(&buffer[SIGNATURE_OFFSET], SIGNATURE.data(), SIGNATURE.size()) != 0)
  {
    *version = std::nullopt;
    return true;
  }

  const std::size_t version_offset = SIGNATURE_OFFSET + SIGNATURE.size();
  *version = FirmwareVersion{buffer[version_offset], buffer[version_offset + 1],
                             buffer[version_offset + 2]};
  return true;
}

std::optional<PhysicalFormat> ReadPhysicalFormat(const Common::SCSIDevice& device)
{
  std::array<u8, 256> buffer{};

  // Format 0x00 requests the Physical Format Information block from the disc lead-in
  if (!ReadDiscStructure(device, 0x00, buffer))
    return std::nullopt;

  PhysicalFormat format;
  format.book_type = static_cast<u8>((buffer[4] >> 4) & 0x0F);

  // Byte 6: bits 5 and 6 hold the layer count minus one; bit 4 is 0 for PTP and 1 for OTP
  format.num_layers = static_cast<u8>(((buffer[6] >> 5) & 0x03) + 1);
  format.opposite_track_path = ((buffer[6] >> 4) & 0x01) != 0;
  format.start_psn = Common::swap24(&buffer[9]);
  format.end_psn = Common::swap24(&buffer[13]);
  format.end_layer0_psn = Common::swap24(&buffer[17]);
  return format;
}

std::optional<std::array<u8, 192>> ReadBCA(const Common::SCSIDevice& device)
{
  // The Burst Cutting Area is 192 bytes. Format 0x03 requests it from the disc lead-in
  std::array<u8, 192> bca{};
  if (!ReadDiscStructure(device, 0x03, bca))
    return std::nullopt;
  return bca;
}

std::optional<DriveSpeeds> ReadDriveSpeeds(const Common::SCSIDevice& device)
{
  std::array<u8, 256> buffer{};
  // Mode page 0x2A contains the mechanical status and drive performance ratings
  if (!ModeSense10(device, 0x2A, buffer))
    return std::nullopt;

  // The page starts after the 8-byte mode parameter header and the block descriptors, whose
  // length is stored in bytes 6 and 7
  const std::size_t page_offset = 8 + Common::swap16(&buffer[6]);
  if (page_offset + 16 > buffer.size() || (buffer[page_offset] & 0x3F) != 0x2A)
    return std::nullopt;

  // Bytes 8-9 of the page hold the maximum read speed and bytes 14-15 the current one
  return DriveSpeeds{Common::swap16(&buffer[page_offset + 8]),
                     Common::swap16(&buffer[page_offset + 14])};
}

// SCSI SET CD SPEED (0xBB). Bytes 2-3 hold the read speed (big-endian) and bytes 4-5 the write
// speed, where 0xFFFF selects the maximum. There is no data phase for this command
bool SetCDSpeed(const Common::SCSIDevice& device, u16 speed_kbs)
{
  const std::array<u8, 12> cdb{
      0xBB, 0x00, static_cast<u8>(speed_kbs >> 8), static_cast<u8>(speed_kbs), 0xFF, 0xFF};
  return device.SendCommand(cdb, {}, Common::SCSIDataDirection::Out);
}

std::optional<u16> SetReadSpeed(const Common::SCSIDevice& device, u16 speed_kbs)
{
  if (!SetCDSpeed(device, speed_kbs))
  {
    ERROR_LOG_FMT(DISCIO, "The drive rejected SET CD SPEED with {} KB/s", speed_kbs);
    return std::nullopt;
  }

  // Re-read page 0x2A to verify which speed the drive applied
  const std::optional<DriveSpeeds> applied = ReadDriveSpeeds(device);
  if (!applied)
    return std::nullopt;

  if (applied->current_kbs != speed_kbs)
  {
    NOTICE_LOG_FMT(DISCIO, "The drive accepted {} KB/s but mapped it to {} KB/s internally",
                   speed_kbs, applied->current_kbs);
  }
  return applied->current_kbs;
}

// SCSI START STOP UNIT (0x1B). Byte 1 bit 0 (IMMED): 0 blocks until the transition completes, 1
// returns status immediately. Byte 4: bit 0 (START) spins the disc, bit 1 (LOEJ) ejects.
bool StartStopUnit(const Common::SCSIDevice& device, bool wait, bool spin, bool eject)
{
  const std::array<u8, 6> cdb{0x1B,
                              static_cast<u8>(wait ? 0x00 : 0x01),
                              0x00,
                              0x00,
                              static_cast<u8>((eject ? 0x02 : 0x00) | (spin ? 0x01 : 0x00)),
                              0x00};
  return device.SendCommand(cdb, {}, Common::SCSIDataDirection::Out);
}

bool ReadSectorsRaw(const Common::SCSIDevice& device, u32 lba, u32 num_sectors, std::span<u8> out,
                    bool force_unit_access)
{
  if (num_sectors == 0 || num_sectors > MAX_TRANSFER_SECTORS_RAW ||
      out.size() < num_sectors * RAW_SECTOR_SIZE)
  {
    return false;
  }

  // OmniDrive raw read (0xC0). Byte 1 flags:
  //   Bits 0-1: media type (00 for CD, 01 for DVD, 10 for BD)
  //   Bit 2: raw addressing
  //   Bit 3: force unit access (bypasses the drive cache)
  //   Bit 4: descramble (0 bypasses hardware descrambling)
  //   Bits 5-7: reserved
  const u8 flags = static_cast<u8>(0x01 | (force_unit_access ? 0x08 : 0x00));
  const std::array<u8, 12> cdb = MakeReadCDB(0xC0, flags, lba, num_sectors);
  return device.SendCommand(cdb, out.first(num_sectors * RAW_SECTOR_SIZE),
                            Common::SCSIDataDirection::In);
}

bool ReadSectorsStandard(const Common::SCSIDevice& device, u32 lba, u32 num_sectors,
                         std::span<u8> out, bool force_unit_access)
{
  if (num_sectors == 0 || num_sectors > MAX_TRANSFER_SECTORS_STD ||
      out.size() < num_sectors * USER_SECTOR_SIZE)
  {
    return false;
  }

  // READ (12) (0xA8). Byte 1 bit 3 is FUA, which bypasses the drive cache
  const u8 flags = static_cast<u8>(force_unit_access ? 0x08 : 0x00);
  const std::array<u8, 12> cdb = MakeReadCDB(0xA8, flags, lba, num_sectors);
  return device.SendCommand(cdb, out.first(num_sectors * USER_SECTOR_SIZE),
                            Common::SCSIDataDirection::In);
}
}  // namespace DiscIO::OmniDrive
