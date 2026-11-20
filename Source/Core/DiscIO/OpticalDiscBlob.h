// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/SCSIDevice.h"

#include "DiscIO/Blob.h"
#include "DiscIO/OmniDrive.h"

namespace DiscIO
{
class OpticalDriveSpinKeeper;
class VolumeDisc;

enum class OpticalDiscError
{
  None,
  Cancelled,
  NoDisc,
  NotReady,
  ReadyTimeout,
  OpenFailed,
  DriveQueryFailed,
  DiscStructureFailed,
  DiscKeyFailed,
  BackupReadFailed,
  OmniDriveReadFailed,
  NotGameCubeOrWiiDisc,
};

class OpticalDiscReader final : public BlobReader
{
public:
  ~OpticalDiscReader() override;

  // Waits while the drive spins up (is_cancelled is polled meanwhile), queries the firmware,
  // picks the read path and finds the image size from the disc header. On failure, error
  // receives the reason
  static std::unique_ptr<OpticalDiscReader> Create(const std::string& drive,
                                                   OpticalDiscError* error = nullptr,
                                                   const std::function<bool()>& is_cancelled = {});

  BlobType GetBlobType() const override { return BlobType::DRIVE; }
  std::unique_ptr<BlobReader> CopyReader() const override;

  u64 GetRawSize() const override { return m_size; }
  u64 GetDataSize() const override { return m_size; }
  DataSizeType GetDataSizeType() const override { return DataSizeType::Accurate; }

  u64 GetBlockSize() const override { return ECC_BLOCK_SIZE; }
  bool HasFastRandomAccessInBlock() const override { return false; }
  std::string GetCompressionMethod() const override { return {}; }
  std::optional<int> GetCompressionLevel() const override { return std::nullopt; }

  bool Read(u64 offset, u64 nbytes, u8* out_ptr) override;

private:
  static constexpr u64 ECC_BLOCK_SIZE = 0x8000;

  OpticalDiscReader(std::string drive, u64 size, std::optional<u8> disc_key);

  bool ReadStandardSectors(u32 lba, u32 count, u8* out_ptr);
  bool ReadRawSectors(u32 lba, u32 count, u8* out_ptr);
  bool RetryGCWiiSector(u32 lba, u8* out_ptr);
  bool RereadGCWiiSector(u32 lba, u8* out_ptr);

  // Records that a read command ending at last_lba is about to be sent, which restarts the drive's
  // spin keep-alive countdown
  void NoteReadCommand(u32 last_lba);

  std::string m_drive_path;
  Common::SCSIDevice m_device;
  u64 m_size = 0;

  // Set for GameCube/Wii book type discs on OmniDrive drives (raw path); std::nullopt for standard
  // read commands. Physical disc changes cannot be easily detected, so it is re-derived from LBA 0
  // after an EDC mismatch error can't be corrected
  std::optional<u8> m_disc_key;

  std::vector<u8> m_raw_buffer;
  std::vector<u8> m_sector_buffer;

  // Shared by every reader of this drive
  std::shared_ptr<OpticalDriveSpinKeeper> m_spin_keeper;
};

// Opens the disc in drive as a volume for the emulated DVD drive. Load Whole Game Into Memory is
// always ignored, as filling the cache from the optical drive while the game is accessing the disc
// causes constant seeking, significantly slowing down the loading speeds to the point it's
// unusable. On failure, error receives the reason
std::unique_ptr<VolumeDisc>
CreateOpticalDiscForCore(const std::string& drive, OpticalDiscError* error,
                         const std::function<bool()>& is_cancelled = {});

// Translated text for a panic alert. Empty for None and Cancelled
std::string GetOpticalDiscErrorMessage(OpticalDiscError error, const std::string& drive);

// Waits while the drive spins up (is_cancelled is polled meanwhile), then queries its firmware:
// firmware receives the OmniDrive version, or std::nullopt for stock firmware. Returns false if
// cancelled, if the drive stays busy, or if it does not answer
bool QueryOpticalDriveFirmware(const std::string& drive,
                               std::optional<OmniDrive::FirmwareVersion>* firmware,
                               const std::function<bool()>& is_cancelled = {});

// Stops the disc and ejects the tray
bool EjectOpticalDisc(const std::string& drive);
}  // namespace DiscIO
