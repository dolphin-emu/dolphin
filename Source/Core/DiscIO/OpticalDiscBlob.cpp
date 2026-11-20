// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/OpticalDiscBlob.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <utility>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/SCSIDevice.h"
#include "Common/Swap.h"
#include "Common/Thread.h"

#include "DiscIO/DVDScrambler.h"
#include "DiscIO/DiscUtils.h"
#include "DiscIO/Enums.h"
#include "DiscIO/OmniDrive.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeDisc.h"

namespace DiscIO
{
// Longest wait for a drive that reports it is becoming ready (spinning up)
constexpr std::chrono::seconds DRIVE_READY_TIMEOUT{60};
constexpr std::chrono::milliseconds DRIVE_READY_POLL_INTERVAL{250};

// How many times a sector is re-read with FUA after an EDC mismatch, both before and after the
// disc key is re-derived. A sector that still mismatches then is a read error
constexpr int EDC_RETRY_COUNT = 5;

// Drives stop spinning after a while without reads, and spinning up again delays the next read
// enough to upset time-sensitive games. After this long without a read command, the drive is kept
// spinning with SPIN_KEEP_ALIVE_METHOD
constexpr std::chrono::seconds SPIN_KEEP_ALIVE_INTERVAL{15};

enum class SpinKeepAliveMethod
{
  // Issue a SCSI START STOP UNIT command with the spin bit set. Some drives also move the lens
  // back to its default position, which delays the next read (but less than a full disc spin-up)
  StartStopUnit,

  // Issue a read command to the last sector that was read from the disc again, using the same type
  // of command the readers used. The data is discarded afterwards
  RereadLastSector,
};
constexpr SpinKeepAliveMethod SPIN_KEEP_ALIVE_METHOD = SpinKeepAliveMethod::StartStopUnit;

// Book type reported in the Physical Format Information of GameCube and Wii discs
constexpr u8 GC_WII_BOOK_TYPE = 0xF;

// The DVD physical data area begins at PSN 0x030000 (LBA 0)
constexpr u32 FIRST_DATA_PSN = 0x30000;

// Raw 2064-byte frame: unscrambled ID (PSN in bytes 1-3), IED and CPR_MAI headers in the first
// 12 bytes, 2048 bytes of scrambled main data, then the unscrambled 4-byte EDC
constexpr u32 FRAME_PSN_OFFSET = 1;
constexpr u32 FRAME_CPR_MAI_OFFSET = 6;
constexpr u32 CPR_MAI_SIZE = 6;
constexpr u32 FRAME_MAIN_DATA_OFFSET = 12;
constexpr u32 FRAME_EDC_OFFSET = 2060;

// Sectors below this LBA are descrambled with key 0
constexpr u32 KEY_ZERO_LBA_LIMIT = 16;

// Size assumed while the disc header is read to find the real size (comes from DiscUtils.h)
constexpr u64 PROBE_SIZE = DL_DVD_R_SIZE;

// Polls the drive while it reports that it is becoming ready (spinning up), until the timeout
// passes or the caller cancels. Returns the last status, or std::nullopt if cancelled
static std::optional<Common::OpticalDriveStatus>
WaitWhileBecomingReady(const std::string& drive, const std::function<bool()>& is_cancelled)
{
  const std::chrono::steady_clock::time_point deadline =
      std::chrono::steady_clock::now() + DRIVE_READY_TIMEOUT;
  while (true)
  {
    const Common::OpticalDriveStatus status = Common::SCSIDevice::GetDriveStatus(drive);
    if (status != Common::OpticalDriveStatus::BecomingReady ||
        std::chrono::steady_clock::now() >= deadline)
    {
      return status;
    }

    if (is_cancelled && is_cancelled())
      return std::nullopt;

    std::this_thread::sleep_for(DRIVE_READY_POLL_INTERVAL);
  }
}

// Reads the raw frame at LBA 0 and derives the disc key
static std::optional<u8> DeriveDiscKey(const Common::SCSIDevice& device)
{
  std::array<u8, OmniDrive::RAW_SECTOR_SIZE> frame;
  if (!OmniDrive::ReadSectorsRaw(device, 0, 1, frame))
    return std::nullopt;

  // Extract the scrambled user data payload starting at byte offset 12 of the frame
  std::array<u8, OmniDrive::USER_SECTOR_SIZE> main_data;
  std::copy_n(frame.begin() + FRAME_MAIN_DATA_OFFSET, main_data.size(), main_data.begin());

  // Block 0 acts as the master key block and must be descrambled with key 0
  DescrambleDVDSector(main_data, FIRST_DATA_PSN, u8{0});

  // Add the 6 bytes of the CPR_MAI header to the first 2 bytes of the descrambled main data,
  // then map the result down to a 4-bit value with a shift-add
  u32 sum = 0;
  for (u32 i = 0; i < CPR_MAI_SIZE; ++i)
    sum += frame[FRAME_CPR_MAI_OFFSET + i];
  sum += main_data[0];
  sum += main_data[1];

  return static_cast<u8>(((sum >> 4) + sum) & 0xF);
}

// Descrambles one raw frame into a 2048-byte user sector. Returns false on an EDC mismatch, but
// the user sector is written either way
static bool DecodeGCWiiFrame(std::span<const u8> frame, u32 lba, u8 disc_key, u8* user_sector)
{
  // Extract the scrambled user payload and the PSN from the frame header
  std::array<u8, OmniDrive::USER_SECTOR_SIZE> main_data;
  std::copy_n(frame.begin() + FRAME_MAIN_DATA_OFFSET, main_data.size(), main_data.begin());
  const u32 psn = Common::swap24(&frame[FRAME_PSN_OFFSET]);

  const u8 key = lba < KEY_ZERO_LBA_LIMIT ? u8{0} : disc_key;
  DescrambleDVDSector(main_data, psn, key);

  // Rebuild a standard 2048-byte sector: the first 6 bytes of unscrambled data live in the
  // CPR_MAI header, followed by the first 2042 bytes of descrambled main data
  std::copy_n(frame.begin() + FRAME_CPR_MAI_OFFSET, CPR_MAI_SIZE, user_sector);
  std::copy_n(main_data.begin(), OmniDrive::USER_SECTOR_SIZE - CPR_MAI_SIZE,
              user_sector + CPR_MAI_SIZE);

  // The EDC covers the 12 unscrambled header bytes followed by the descrambled main data. The EDC
  // stored in the frame is not scrambled
  std::array<u8, FRAME_EDC_OFFSET> edc_payload;
  std::copy_n(frame.begin(), FRAME_MAIN_DATA_OFFSET, edc_payload.begin());
  std::ranges::copy(main_data, edc_payload.begin() + FRAME_MAIN_DATA_OFFSET);
  return CalculateDVDEDC(edc_payload) == Common::swap32(&frame[FRAME_EDC_OFFSET]);
}

// Finds the disc image size by reading the disc header through Dolphin's volume code
static std::optional<u64> DetermineDiscSize(const BlobReader& reader)
{
  const std::unique_ptr<VolumeDisc> volume = CreateDisc(reader.CopyReader());
  if (!volume)
    return std::nullopt;

  switch (volume->GetVolumeType())
  {
  case Platform::GameCubeDisc:
    return MINI_DVD_SIZE;
  case Platform::WiiDisc:
    return GetBiggestReferencedOffset(*volume) > SL_DVD_SIZE ? DL_DVD_SIZE : SL_DVD_SIZE;
  default:
    return std::nullopt;
  }
}

// Keeps the disc in one drive spinning while the drive is idle. Every reader of a drive shares the
// drive's instance, so each drive has a single countdown and a single thread issuing spin
// commands, both gone once the last reader of the drive is destroyed
class OpticalDriveSpinKeeper final
{
public:
  explicit OpticalDriveSpinKeeper(std::string drive);
  ~OpticalDriveSpinKeeper();

  OpticalDriveSpinKeeper(const OpticalDriveSpinKeeper&) = delete;
  OpticalDriveSpinKeeper& operator=(const OpticalDriveSpinKeeper&) = delete;
  OpticalDriveSpinKeeper(OpticalDriveSpinKeeper&&) = delete;
  OpticalDriveSpinKeeper& operator=(OpticalDriveSpinKeeper&&) = delete;

  // Returns the instance for drive, creating it if no reader of the drive holds one
  static std::shared_ptr<OpticalDriveSpinKeeper> Acquire(const std::string& drive);

  // Records that a read command is about to be sent to the drive, which restarts the countdown.
  // last_lba is the last sector the command reads, and raw tells whether it is an OmniDrive raw
  // read or standard SCSI read; RereadLastSector repeats that read
  void NoteReadCommand(u32 last_lba, bool raw);

private:
  void Run();

  std::string m_drive;
  std::mutex m_mutex;
  std::condition_variable m_condition;
  std::chrono::steady_clock::time_point m_last_read_command;
  u32 m_last_lba = 0;
  bool m_last_read_raw = false;
  bool m_stop = false;
  std::thread m_thread;
};

// Instances by canonical drive path. A drive's entry expires with the last reader of the drive
static std::mutex s_spin_keepers_mutex;
static std::map<std::string, std::weak_ptr<OpticalDriveSpinKeeper>> s_spin_keepers;

OpticalDriveSpinKeeper::OpticalDriveSpinKeeper(std::string drive)
    : m_drive(std::move(drive)), m_last_read_command(std::chrono::steady_clock::now())
{
  m_thread = std::thread(&OpticalDriveSpinKeeper::Run, this);
}

OpticalDriveSpinKeeper::~OpticalDriveSpinKeeper()
{
  {
    std::lock_guard lock(m_mutex);
    m_stop = true;
  }
  m_condition.notify_one();
  m_thread.join();

  // Forget the drive, unless a new reader has already registered a new instance for it
  std::lock_guard lock(s_spin_keepers_mutex);
  const auto it = s_spin_keepers.find(m_drive);
  if (it != s_spin_keepers.end() && it->second.expired())
    s_spin_keepers.erase(it);
}

std::shared_ptr<OpticalDriveSpinKeeper> OpticalDriveSpinKeeper::Acquire(const std::string& drive)
{
  const std::string canonical_drive = Common::SCSIDevice::GetCanonicalPath(drive);

  std::lock_guard lock(s_spin_keepers_mutex);
  std::weak_ptr<OpticalDriveSpinKeeper>& entry = s_spin_keepers[canonical_drive];
  std::shared_ptr<OpticalDriveSpinKeeper> keeper = entry.lock();
  if (!keeper)
  {
    keeper = std::make_shared<OpticalDriveSpinKeeper>(canonical_drive);
    entry = keeper;
  }
  return keeper;
}

void OpticalDriveSpinKeeper::NoteReadCommand(u32 last_lba, bool raw)
{
  std::lock_guard lock(m_mutex);
  m_last_read_command = std::chrono::steady_clock::now();
  m_last_lba = last_lba;
  m_last_read_raw = raw;
}

void OpticalDriveSpinKeeper::Run()
{
  Common::SetCurrentThreadName("Optical Disc Keep-Alive");

  // A handle of its own, so spin commands never touch the handles the readers use
  Common::SCSIDevice device;
  if (!device.Open(m_drive, true))
    return;

  std::unique_lock lock(m_mutex);
  while (!m_stop)
  {
    const std::chrono::steady_clock::time_point deadline =
        m_last_read_command + SPIN_KEEP_ALIVE_INTERVAL;
    if (std::chrono::steady_clock::now() < deadline)
    {
      // Woken by the deadline, a spurious wakeup or the destructor; the loop checks which
      m_condition.wait_until(lock, deadline);
      continue;
    }

    // Counting the keep-alive command like a read schedules the next one SPIN_KEEP_ALIVE_INTERVAL
    // seconds later
    m_last_read_command = std::chrono::steady_clock::now();
    const u32 lba = m_last_lba;
    const bool raw = m_last_read_raw;
    lock.unlock();

    if constexpr (SPIN_KEEP_ALIVE_METHOD == SpinKeepAliveMethod::RereadLastSector)
    {
      std::array<u8, OmniDrive::RAW_SECTOR_SIZE> sector;
      if (raw)
        OmniDrive::ReadSectorsRaw(device, lba, 1, sector);
      else
        OmniDrive::ReadSectorsStandard(device, lba, 1, sector);
    }
    else
    {
      OmniDrive::StartStopUnit(device, false, true, false);
    }

    lock.lock();
  }
}

OpticalDiscReader::OpticalDiscReader(std::string drive, u64 size, std::optional<u8> disc_key)
    : m_drive_path(std::move(drive)), m_size(size), m_disc_key(disc_key),
      m_sector_buffer(OmniDrive::MAX_TRANSFER_SECTORS_STD * OmniDrive::USER_SECTOR_SIZE)
{
  if (m_disc_key)
    m_raw_buffer.resize(OmniDrive::MAX_TRANSFER_SECTORS_RAW * OmniDrive::RAW_SECTOR_SIZE);

  if (m_device.Open(m_drive_path, true))
    m_spin_keeper = OpticalDriveSpinKeeper::Acquire(m_drive_path);
}

OpticalDiscReader::~OpticalDiscReader() = default;

std::unique_ptr<OpticalDiscReader>
OpticalDiscReader::Create(const std::string& drive, OpticalDiscError* error,
                          const std::function<bool()>& is_cancelled)
{
  const auto fail = [error](OpticalDiscError reason) -> std::unique_ptr<OpticalDiscReader> {
    if (error != nullptr)
      *error = reason;
    return nullptr;
  };

  const std::optional<Common::OpticalDriveStatus> status =
      WaitWhileBecomingReady(drive, is_cancelled);
  if (!status)
    return fail(OpticalDiscError::Cancelled);

  switch (*status)
  {
  case Common::OpticalDriveStatus::Ready:
    break;
  case Common::OpticalDriveStatus::NoDisc:
  case Common::OpticalDriveStatus::TrayOpen:
    ERROR_LOG_FMT(DISCIO, "No disc in optical drive {}", drive);
    return fail(OpticalDiscError::NoDisc);
  case Common::OpticalDriveStatus::BecomingReady:
    ERROR_LOG_FMT(DISCIO, "Optical drive {} did not become ready within {} seconds", drive,
                  DRIVE_READY_TIMEOUT.count());
    return fail(OpticalDiscError::ReadyTimeout);
  case Common::OpticalDriveStatus::NotReadyOther:
    ERROR_LOG_FMT(DISCIO, "Optical drive {} is not ready", drive);
    return fail(OpticalDiscError::NotReady);
  }

  Common::SCSIDevice device;
  if (!device.Open(drive, true))
    return fail(OpticalDiscError::OpenFailed);

  // Query the drive's firmware before any read. Without OmniDrive firmware, only the standard SCSI
  // READ (12) command is used, which cannot read original discs, only backups
  std::optional<OmniDrive::FirmwareVersion> firmware;
  if (!OmniDrive::QueryFirmwareVersion(device, &firmware))
  {
    ERROR_LOG_FMT(DISCIO, "INQUIRY failed on optical drive {}", drive);
    return fail(OpticalDiscError::DriveQueryFailed);
  }

  std::optional<u8> disc_key;
  if (firmware)
  {
    NOTICE_LOG_FMT(DISCIO, "Optical drive {} runs OmniDrive firmware v{}.{}.{}", drive,
                   firmware->major, firmware->minor, firmware->patch);

    // The book type decides between the OmniDrive raw path and the standard path
    const std::optional<OmniDrive::PhysicalFormat> format = OmniDrive::ReadPhysicalFormat(device);
    if (!format)
    {
      ERROR_LOG_FMT(DISCIO, "READ DISC STRUCTURE failed on optical drive {}", drive);
      return fail(OpticalDiscError::DiscStructureFailed);
    }

    if (format->book_type == GC_WII_BOOK_TYPE)
    {
      disc_key = DeriveDiscKey(device);
      if (!disc_key)
      {
        ERROR_LOG_FMT(DISCIO, "Failed to read LBA 0 to derive the disc key from {}", drive);
        return fail(OpticalDiscError::DiscKeyFailed);
      }
      NOTICE_LOG_FMT(DISCIO, "GameCube/Wii book type in {}, using OmniDrive raw reads", drive);
    }
    else
    {
      NOTICE_LOG_FMT(DISCIO, "Book type 0x{:X} in {}, using standard reads", format->book_type,
                     drive);
    }
  }
  else
  {
    NOTICE_LOG_FMT(DISCIO, "Optical drive {} does not run OmniDrive firmware, using standard reads",
                   drive);
  }

  device.Close();

  auto reader =
      std::unique_ptr<OpticalDiscReader>(new OpticalDiscReader(drive, PROBE_SIZE, disc_key));
  if (!reader->m_device.IsOpen())
    return fail(OpticalDiscError::OpenFailed);

  // A test read of the first sector separates unreadable discs from discs that are readable but
  // are not GameCube or Wii discs
  std::array<u8, OmniDrive::USER_SECTOR_SIZE> first_sector;
  if (!reader->Read(0, first_sector.size(), first_sector.data()))
  {
    ERROR_LOG_FMT(DISCIO, "Failed to read the disc in {}", drive);
    return fail(firmware ? OpticalDiscError::OmniDriveReadFailed :
                           OpticalDiscError::BackupReadFailed);
  }

  const std::optional<u64> size = DetermineDiscSize(*reader);
  if (!size)
  {
    ERROR_LOG_FMT(DISCIO, "The disc in {} is not a GameCube or Wii disc", drive);
    return fail(OpticalDiscError::NotGameCubeOrWiiDisc);
  }

  reader->m_size = *size;
  if (error != nullptr)
    *error = OpticalDiscError::None;
  return reader;
}

std::unique_ptr<BlobReader> OpticalDiscReader::CopyReader() const
{
  auto reader =
      std::unique_ptr<OpticalDiscReader>(new OpticalDiscReader(m_drive_path, m_size, m_disc_key));
  if (!reader->m_device.IsOpen())
    reader.reset();
  return reader;
}

bool OpticalDiscReader::Read(u64 offset, u64 nbytes, u8* out_ptr)
{
  if (nbytes == 0)
    return true;
  if (offset >= m_size || nbytes > m_size - offset)
    return false;

  // Cover the requested range with as few commands as possible: one command per
  // MAX_TRANSFER_SECTORS_RAW (raw path) or MAX_TRANSFER_SECTORS_STD (standard path) sectors
  const u32 max_per_command =
      m_disc_key ? OmniDrive::MAX_TRANSFER_SECTORS_RAW : OmniDrive::MAX_TRANSFER_SECTORS_STD;
  const u64 last_lba = (offset + nbytes - 1) / OmniDrive::USER_SECTOR_SIZE;
  u64 lba = offset / OmniDrive::USER_SECTOR_SIZE;
  u64 offset_in_sector = offset % OmniDrive::USER_SECTOR_SIZE;

  while (lba <= last_lba)
  {
    const u32 count = static_cast<u32>(std::min<u64>(last_lba - lba + 1, max_per_command));
    const bool success = m_disc_key ?
                        ReadRawSectors(static_cast<u32>(lba), count, m_sector_buffer.data()) :
                        ReadStandardSectors(static_cast<u32>(lba), count, m_sector_buffer.data());
    if (!success)
      return false;

    const u64 available = u64{count} * OmniDrive::USER_SECTOR_SIZE - offset_in_sector;
    const u64 to_copy = std::min(available, nbytes);
    std::copy_n(m_sector_buffer.data() + offset_in_sector, to_copy, out_ptr);

    out_ptr += to_copy;
    nbytes -= to_copy;
    offset_in_sector = 0;
    lba += count;
  }
  return true;
}

bool OpticalDiscReader::ReadStandardSectors(u32 lba, u32 count, u8* out_ptr)
{
  NoteReadCommand(lba + count - 1);

  const std::span<u8> out(out_ptr, count * OmniDrive::USER_SECTOR_SIZE);
  if (!OmniDrive::ReadSectorsStandard(m_device, lba, count, out))
  {
    // The batch read failed. Fall back to sector-by-sector to find the exact failing LBA
    for (u32 i = 0; i < count; ++i)
    {
      const std::span<u8> sector =
          out.subspan(i * OmniDrive::USER_SECTOR_SIZE, OmniDrive::USER_SECTOR_SIZE);
      if (!OmniDrive::ReadSectorsStandard(m_device, lba + i, 1, sector))
      {
        ERROR_LOG_FMT(DISCIO, "Read error at LBA {} on {}", lba + i, m_drive_path);
        return false;
      }
    }
  }

  DEBUG_LOG_FMT(DISCIO, "Standard read of {} sectors at LBA {} on {} succeeded", count, lba,
                m_drive_path);
  return true;
}

bool OpticalDiscReader::ReadRawSectors(u32 lba, u32 count, u8* out_ptr)
{
  NoteReadCommand(lba + count - 1);

  const std::span<u8> raw(m_raw_buffer.data(), count * OmniDrive::RAW_SECTOR_SIZE);
  const bool batch_ok = OmniDrive::ReadSectorsRaw(m_device, lba, count, raw);

  for (u32 i = 0; i < count; ++i)
  {
    const u32 current_lba = lba + i;
    const std::span<u8> frame =
        raw.subspan(i * OmniDrive::RAW_SECTOR_SIZE, OmniDrive::RAW_SECTOR_SIZE);
    u8* user_sector = out_ptr + i * OmniDrive::USER_SECTOR_SIZE;

    // If the batch read failed, fall back to sector-by-sector, bypassing the drive cache, to find
    // the exact failing LBA.
    if (!batch_ok && !OmniDrive::ReadSectorsRaw(m_device, current_lba, 1, frame, true))
    {
      ERROR_LOG_FMT(DISCIO, "Read error at LBA {} on {}", current_lba, m_drive_path);
      return false;
    }

    if (!DecodeGCWiiFrame(frame, current_lba, *m_disc_key, user_sector) &&
        !RetryGCWiiSector(current_lba, user_sector))
    {
      return false;
    }
  }

  DEBUG_LOG_FMT(DISCIO, "OmniDrive read of {} sectors at LBA {} on {} succeeded", count, lba,
                m_drive_path);
  return true;
}

bool OpticalDiscReader::RetryGCWiiSector(u32 lba, u8* out_ptr)
{
  WARN_LOG_FMT(DISCIO, "EDC mismatch at LBA {} on {}, retrying", lba, m_drive_path);
  if (RereadGCWiiSector(lba, out_ptr))
    return true;

  // Physical disc changes cannot be easily detected, so the cached disc key may be stale.
  // Re-derive it from LBA 0 and update the cache, then retry the sector again
  const std::optional<u8> current_key = DeriveDiscKey(m_device);
  if (!current_key)
  {
    WARN_LOG_FMT(DISCIO, "Failed to re-read the disc key from {}, keeping the cached key",
                 m_drive_path);
  }
  else if (*current_key != *m_disc_key)
  {
    NOTICE_LOG_FMT(DISCIO, "Disc key on {} changed from {} to {}", m_drive_path, *m_disc_key,
                   *current_key);
    m_disc_key = current_key;
  }

  if (RereadGCWiiSector(lba, out_ptr))
    return true;

  ERROR_LOG_FMT(DISCIO, "Read error at LBA {} on {}: EDC mismatch after {} retries", lba,
                m_drive_path, 2 * EDC_RETRY_COUNT);
  return false;
}

// Re-reads the sector with FUA up to EDC_RETRY_COUNT times. Returns true once its EDC matches
bool OpticalDiscReader::RereadGCWiiSector(u32 lba, u8* out_ptr)
{
  std::array<u8, OmniDrive::RAW_SECTOR_SIZE> frame;
  for (int attempt = 1; attempt <= EDC_RETRY_COUNT; ++attempt)
  {
    // Bypass the drive cache, so the sector is read from the disc again
    if (OmniDrive::ReadSectorsRaw(m_device, lba, 1, frame, true) &&
        DecodeGCWiiFrame(frame, lba, *m_disc_key, out_ptr))
    {
      return true;
    }
    WARN_LOG_FMT(DISCIO, "Retry {} of {} failed at LBA {} on {}", attempt, EDC_RETRY_COUNT, lba,
                 m_drive_path);
  }
  return false;
}

void OpticalDiscReader::NoteReadCommand(u32 last_lba)
{
  if (m_spin_keeper)
    m_spin_keeper->NoteReadCommand(last_lba, m_disc_key.has_value());
}

std::unique_ptr<VolumeDisc> CreateOpticalDiscForCore(const std::string& drive,
                                                     OpticalDiscError* error,
                                                     const std::function<bool()>& is_cancelled)
{
  std::unique_ptr<OpticalDiscReader> reader = OpticalDiscReader::Create(drive, error, is_cancelled);
  if (!reader)
    return nullptr;

  // CreateDisc never caches, so Load Whole Game Into Memory currently is ignored regardless of the
  // user preference
  std::unique_ptr<VolumeDisc> disc = CreateDisc(std::move(reader));
  if (!disc && error != nullptr)
    *error = OpticalDiscError::NotGameCubeOrWiiDisc;
  return disc;
}

std::string GetOpticalDiscErrorMessage(OpticalDiscError error, const std::string& drive)
{
  switch (error)
  {
  case OpticalDiscError::None:
  case OpticalDiscError::Cancelled:
    return {};
  case OpticalDiscError::NoDisc:
    return Common::FmtFormatT("There is no disc in the optical drive \"{0}\".", drive);
  case OpticalDiscError::NotReady:
    return Common::FmtFormatT("The optical drive \"{0}\" is not ready.", drive);
  case OpticalDiscError::ReadyTimeout:
    return Common::FmtFormatT("The optical drive \"{0}\" did not become ready within {1} seconds.",
                              drive, DRIVE_READY_TIMEOUT.count());
  case OpticalDiscError::OpenFailed:
    return Common::FmtFormatT("Could not open the optical drive \"{0}\".", drive);
  case OpticalDiscError::DriveQueryFailed:
    return Common::FmtFormatT("Could not identify the optical drive \"{0}\".", drive);
  case OpticalDiscError::DiscStructureFailed:
    return Common::FmtFormatT("Could not read the disc structure of the disc in \"{0}\".", drive);
  case OpticalDiscError::DiscKeyFailed:
    return Common::FmtFormatT(
        "Could not read the first sector of the GameCube/Wii disc in \"{0}\".", drive);
  case OpticalDiscError::BackupReadFailed:
    return Common::FmtFormatT(
        "Could not read the disc in \"{0}\". This drive does not run OmniDrive firmware, so it can "
        "only read GameCube/Wii disc backups, not original discs.",
        drive);
  case OpticalDiscError::OmniDriveReadFailed:
    return Common::FmtFormatT("Could not read the disc in \"{0}\".", drive);
  case OpticalDiscError::NotGameCubeOrWiiDisc:
    return Common::FmtFormatT("The disc in \"{0}\" is not a GameCube or Wii disc.", drive);
  }
  return {};
}

bool QueryOpticalDriveFirmware(const std::string& drive,
                               std::optional<OmniDrive::FirmwareVersion>* firmware,
                               const std::function<bool()>& is_cancelled)
{
  // Drives cannot answer the firmware query command while they are becoming ready
  const std::optional<Common::OpticalDriveStatus> status =
      WaitWhileBecomingReady(drive, is_cancelled);
  if (!status || *status == Common::OpticalDriveStatus::BecomingReady)
    return false;

  Common::SCSIDevice device;
  return device.Open(drive, true) && OmniDrive::QueryFirmwareVersion(device, firmware);
}

bool EjectOpticalDisc(const std::string& drive)
{
  Common::SCSIDevice device;
  if (!device.Open(drive, true))
    return false;

  // SCSI START STOP UNIT with LOEJ set and START clear stops the disc and opens the tray in one
  // command, without waiting for the mechanism. A separate stop first would be refused by an empty
  // drive (no medium present), which would keep its tray shut
  return OmniDrive::StartStopUnit(device, false, false, true);
}
}  // namespace DiscIO
