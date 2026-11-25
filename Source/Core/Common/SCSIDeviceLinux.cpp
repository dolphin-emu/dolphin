// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/SCSIDevice.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include <fcntl.h>
#include <scsi/sg.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/OpticalDrive.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

namespace Common
{
namespace
{
// Optical drives are SCSI CD-ROM block devices named sr<N>
constexpr std::string_view OPTICAL_DEVICE_PREFIX = "sr";

// Large enough for fixed and descriptor format sense data
constexpr std::size_t SENSE_BUFFER_SIZE = 32;

bool IsOpticalDeviceName(std::string_view name)
{
  if (!name.starts_with(OPTICAL_DEVICE_PREFIX) || name.size() == OPTICAL_DEVICE_PREFIX.size())
    return false;

  const std::string_view number = name.substr(OPTICAL_DEVICE_PREFIX.size());
  return std::ranges::all_of(
      number, [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
}

// Block device name ("sr0") behind a path such as "/dev/sr0" or a symlink to it like
// "/dev/cdrom". Empty if the path is not an optical drive
std::string GetBlockDeviceName(const std::string& path)
{
  // Only paths under /dev qualify, which keeps this cheap for game list scans
  if (!path.starts_with("/dev/"))
    return {};

  std::error_code error;
  const std::filesystem::path resolved = std::filesystem::canonical(path, error);
  if (error)
    return {};

  const std::string name = resolved.filename().string();
  if (!IsOpticalDeviceName(name) || !File::Exists("/sys/block/" + name))
    return {};
  return name;
}

// The SCSI generic node bound to the drive ("/dev/sg1" for "sr0"), or an empty string when the
// sg kernel module is not loaded
std::string GetSCSIGenericNode(const std::string& name)
{
  std::error_code error;
  const std::filesystem::directory_iterator it(
      fmt::format("/sys/block/{}/device/scsi_generic", name), error);
  if (error || it == std::filesystem::directory_iterator())
    return {};
  return "/dev/" + it->path().filename().string();
}

// Contents of a sysfs file, or an empty string if it cannot be read. File::ReadFileToString cannot
// read these, because sysfs reports a size of one page for every attribute, so its read of that
// many bytes always comes up short and fails
std::string ReadSysfsFile(const std::string& path)
{
  const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd == -1)
    return {};

  std::string contents;
  std::array<char, 256> chunk;
  while (true)
  {
    const ssize_t count = read(fd, chunk.data(), chunk.size());
    if (count <= 0)
      break;
    contents.append(chunk.data(), static_cast<std::size_t>(count));
  }
  close(fd);

  return contents;
}

std::string ReadDeviceAttribute(const std::string& name, std::string_view attribute)
{
  return std::string(
      StripWhitespace(ReadSysfsFile(fmt::format("/sys/block/{}/device/{}", name, attribute))));
}

// Issues the command with the SG_IO ioctl
bool SendSGIO(int fd, std::span<const u8> cdb, std::span<u8> buffer, SCSIDataDirection direction,
              u32 timeout_seconds, sg_io_hdr_t* header,
              std::array<u8, SENSE_BUFFER_SIZE>* sense_buffer)
{
  // sg_io_hdr_t takes a non-const CDB pointer, so the CDB is copied
  std::array<u8, 16> cdb_copy{};
  if (cdb.empty() || cdb.size() > cdb_copy.size())
  {
    errno = EINVAL;
    return false;
  }
  std::ranges::copy(cdb, cdb_copy.begin());

  *header = {};
  header->interface_id = 'S';
  header->cmd_len = static_cast<unsigned char>(cdb.size());
  header->cmdp = cdb_copy.data();
  header->mx_sb_len = static_cast<unsigned char>(sense_buffer->size());
  header->sbp = sense_buffer->data();
  header->dxfer_len = static_cast<unsigned int>(buffer.size());
  header->dxferp = buffer.empty() ? nullptr : buffer.data();

  if (buffer.empty())
    header->dxfer_direction = SG_DXFER_NONE;
  else if (direction == SCSIDataDirection::In)
    header->dxfer_direction = SG_DXFER_FROM_DEV;
  else
    header->dxfer_direction = SG_DXFER_TO_DEV;

  header->timeout = timeout_seconds * 1000;

  return ioctl(fd, SG_IO, header) == 0;
}

// Decodes the SCSI status and the sense data the kernel wrote, in fixed or descriptor format
SCSISense DecodeSense(const sg_io_hdr_t& header, std::span<const u8> sense_buffer)
{
  SCSISense sense;
  sense.status = header.status;

  const std::span<const u8> data =
      sense_buffer.first(std::min<std::size_t>(header.sb_len_wr, sense_buffer.size()));
  if (data.empty())
    return sense;

  const u8 response_code = data[0] & 0x7F;
  if ((response_code == 0x72 || response_code == 0x73) && data.size() >= 4)
  {
    // Descriptor format
    sense.sense_key = data[1] & 0x0F;
    sense.asc = data[2];
    sense.ascq = data[3];
  }
  else if ((response_code == 0x70 || response_code == 0x71) && data.size() >= 14)
  {
    // Fixed format
    sense.sense_key = data[2] & 0x0F;
    sense.asc = data[12];
    sense.ascq = data[13];
  }
  return sense;
}

bool CommandSucceeded(const sg_io_hdr_t& header)
{
  return (header.info & SG_INFO_OK_MASK) == SG_INFO_OK;
}

// SCSI TEST UNIT READY (0x00). A spinning-up drive answers it at once with a CHECK CONDITION
// (sense 02/04/01), so a short timeout returns the real state instead of waiting for the drive
std::optional<std::pair<bool, SCSISense>> SendTestUnitReady(int fd, const std::string& path)
{
  constexpr std::array<u8, 6> cdb{};
  DEBUG_LOG_FMT(COMMON, "Sending SCSI opcode {:02X} to {} (0 data bytes)", cdb[0], path);
  sg_io_hdr_t header{};
  std::array<u8, SENSE_BUFFER_SIZE> sense_buffer{};
  if (!SendSGIO(fd, cdb, {}, SCSIDataDirection::Out, SCSIDevice::STATUS_PROBE_TIMEOUT_SECONDS,
                &header, &sense_buffer))
  {
    return std::nullopt;
  }
  return std::pair(CommandSucceeded(header), DecodeSense(header, sense_buffer));
}

std::mutex s_feature_serial_numbers_mutex;
std::map<std::string, std::string> s_feature_serial_numbers;

std::optional<std::string> QueryFeatureSerialNumber(OpticalDrive& drive)
{
  // SCSI GET CONFIGURATION (0x46), starting at feature 0x0108, RT 0
  // This command is on the kernel's list of safe commands, so the block device works too
  std::array<u8, 64> buffer{};
  const std::array<u8, 10> cdb{
      0x46, 0x00, 0x01, 0x08, 0x00, 0x00, 0x00, 0x00, static_cast<u8>(buffer.size()), 0x00};
  if (!drive.SendCommand(cdb, buffer, SCSIDataDirection::In,
                         SCSIDevice::STATUS_PROBE_TIMEOUT_SECONDS))
  {
    return std::nullopt;
  }

  // A drive without the feature returns the next feature it has, or none
  const u32 data_length = Common::swap32(buffer.data()) + 4;
  if (data_length < 12 || Common::swap16(&buffer[8]) != 0x0108)
    return std::string();

  const std::size_t length =
      std::min<std::size_t>({buffer[11], buffer.size() - 12, std::size_t{data_length} - 12});
  return std::string(
      StripWhitespace(std::string_view(reinterpret_cast<const char*>(&buffer[12]), length)));
}

// Returns the sysfs device path of the attached drive, which changes when it's attached again
std::string GetAttachedDevicePath(const std::string& name)
{
  std::error_code error;
  const std::filesystem::path device =
      std::filesystem::canonical(fmt::format("/sys/block/{}/device", name), error);
  return error ? std::string() : device.string();
}

// Returns the serial number GetSerialNumber has read from the attached drive
std::optional<std::string> GetRememberedSerialNumber(const std::string& name)
{
  // Without a device path there is nothing to remember it by, and nothing to read
  const std::string device = GetAttachedDevicePath(name);
  if (device.empty())
    return std::string();

  std::lock_guard lock(s_feature_serial_numbers_mutex);
  const auto it = s_feature_serial_numbers.find(device);
  if (it == s_feature_serial_numbers.end())
    return std::nullopt;
  return it->second;
}

// Returns the drive's serial number, read via SCSI GET CONFIGURATION, sent after TEST UNIT READY
// reports the drive is not busy. Later calls ask again until the attached drive answers, and then
// send it nothing
std::optional<std::string> GetSerialNumber(const std::string& name)
{
  if (const std::optional<std::string> remembered = GetRememberedSerialNumber(name))
    return remembered;

  // A busy drive may hold the command until it settles. Try again on a later scan instead
  const std::shared_ptr<OpticalDrive> drive = OpticalDrive::Acquire("/dev/" + name);
  const OpticalDriveStatus status = drive->QueryStatus();
  if (status == OpticalDriveStatus::BecomingReady || status == OpticalDriveStatus::NotReadyOther)
    return std::nullopt;

  const std::optional<std::string> serial = QueryFeatureSerialNumber(*drive);
  if (!serial)
    return std::nullopt;

  std::lock_guard lock(s_feature_serial_numbers_mutex);
  s_feature_serial_numbers[GetAttachedDevicePath(name)] = *serial;
  return serial;
}
}  // namespace

bool SCSIDevice::IsSupported()
{
  return true;
}

bool SCSIDevice::Open(const std::string& path, bool read_write)
{
  Close();

  const std::string name = GetBlockDeviceName(path);
  if (name.empty())
  {
    ERROR_LOG_FMT(COMMON, "{} is not an optical drive", path);
    return false;
  }

  // O_NONBLOCK lets the device open with an empty tray
  constexpr int flags = O_NONBLOCK | O_CLOEXEC;
  int fd = -1;

  // The kernel filters SG_IO on block devices for processes without CAP_SYS_RAWIO, which rejects
  // vendor commands such as OmniDrive's raw read. The sg node has no such filter once it is
  // opened for reading and writing, so it's tried first
  if (read_write)
  {
    const std::string sg_node = GetSCSIGenericNode(name);
    if (!sg_node.empty())
      fd = open(sg_node.c_str(), O_RDWR | flags);

    if (fd == -1)
    {
      WARN_LOG_FMT(COMMON,
                   "No writable SCSI generic node for {}; vendor commands will be refused. Load "
                   "the sg kernel module and give this user write access to its /dev/sg node.",
                   path);
    }
  }

  const std::string block_node = "/dev/" + name;
  if (fd == -1 && read_write)
    fd = open(block_node.c_str(), O_RDWR | flags);

  // Without write access, open the block device read-only. The kernel then allows standard commands
  // such as READ (12), but refuses vendor commands such as OmniDrive's raw read
  if (fd == -1)
    fd = open(block_node.c_str(), O_RDONLY | flags);

  if (fd == -1)
  {
    ERROR_LOG_FMT(COMMON, "Failed to open optical drive {}: {}", path, LastStrerrorString());
    return false;
  }

  m_fd = fd;
  m_path = path;
  return true;
}

void SCSIDevice::Close()
{
  if (m_fd != -1)
  {
    close(m_fd);
    m_fd = -1;
  }
}

bool SCSIDevice::IsOpen() const
{
  return m_fd != -1;
}

bool SCSIDevice::SendCommand(std::span<const u8> cdb, std::span<u8> buffer,
                             SCSIDataDirection direction, u32 timeout_seconds,
                             SCSISense* sense) const
{
  if (!IsOpen() || cdb.empty())
    return false;

  DEBUG_LOG_FMT(COMMON, "Sending SCSI opcode {:02X} to {} ({} data bytes)", cdb[0], m_path,
                buffer.size());

  sg_io_hdr_t header{};
  std::array<u8, SENSE_BUFFER_SIZE> sense_buffer{};
  if (!SendSGIO(m_fd, cdb, buffer, direction, timeout_seconds, &header, &sense_buffer))
  {
    const int error = errno;
    if (sense != nullptr)
      *sense = SCSISense{.refused = error == EPERM || error == EACCES};

    if (error == EPERM)
    {
      ERROR_LOG_FMT(COMMON,
                    "The kernel refused SCSI opcode {:02X}. Load the sg kernel module and give "
                    "this user write access to the drive's /dev/sg node.",
                    cdb[0]);
    }
    else
    {
      ERROR_LOG_FMT(COMMON, "SG_IO for SCSI opcode {:02X} failed: {}", cdb[0],
                    StrerrorString(error));
    }
    return false;
  }

  SCSISense decoded = DecodeSense(header, sense_buffer);
  decoded.delivered = true;
  if (sense != nullptr)
    *sense = decoded;

  if (!CommandSucceeded(header))
  {
    ERROR_LOG_FMT(COMMON,
                  "SCSI opcode {:02X} failed. Status: {:02X}, Host: {:04X}, Driver: {:04X}, "
                  "Sense Key: {:02X}, ASC: {:02X}, ASCQ: {:02X}",
                  cdb[0], header.status, header.host_status, header.driver_status,
                  decoded.sense_key, decoded.asc, decoded.ascq);
    return false;
  }
  return true;
}

OpticalDriveStatus SCSIDevice::GetDriveStatus(const std::string& path)
{
  // Prefer the drive's SCSI generic node. On the block device, the sr driver swallows the CHECK
  // CONDITION of a spinning-up drive and retries internally, so the ioctl blocks until the drive
  // is ready, while the sg node hands the sense data straight back, which is what the caller wants
  const std::string name = GetBlockDeviceName(path);
  const std::string sg_node = name.empty() ? std::string() : GetSCSIGenericNode(name);

  // O_NONBLOCK opens the device even with an empty tray
  constexpr int flags = O_RDONLY | O_NONBLOCK | O_CLOEXEC;
  int fd = sg_node.empty() ? -1 : open(sg_node.c_str(), flags);
  if (fd == -1)
    fd = open(path.c_str(), flags);
  if (fd == -1)
    return OpticalDriveStatus::NotReadyOther;

  const std::optional<std::pair<bool, SCSISense>> result = SendTestUnitReady(fd, path);
  close(fd);

  if (!result)
    return OpticalDriveStatus::NotReadyOther;
  if (result->first)
    return OpticalDriveStatus::Ready;
  return GetStatusFromSense(result->second);
}

std::optional<OpticalDriveStatus> SCSIDevice::GetStatus() const
{
  if (!IsOpen())
    return std::nullopt;

  const std::optional<std::pair<bool, SCSISense>> result = SendTestUnitReady(m_fd, m_path);
  if (!result)
    return std::nullopt;
  if (result->first)
    return OpticalDriveStatus::Ready;
  return GetStatusFromSense(result->second);
}

std::vector<std::string> SCSIDevice::ListOpticalDrives()
{
  // Only sysfs is read and no device is opened, so this returns at once
  std::vector<std::string> drives;
  std::error_code error;
  for (auto it = std::filesystem::directory_iterator("/sys/block", error);
       it != std::filesystem::directory_iterator(); it.increment(error))
  {
    const std::string name = it->path().filename().string();
    if (IsOpticalDeviceName(name) && File::Exists("/dev/" + name))
      drives.push_back("/dev/" + name);
  }
  std::ranges::sort(drives);
  return drives;
}

std::optional<OpticalDriveInfo> SCSIDevice::GetDriveInfo(const std::string& path)
{
  const std::string name = GetBlockDeviceName(path);
  if (name.empty())
    return std::nullopt;

  // The serial number is only known once ReadSerialNumber has successfully asked the drive for it
  OpticalDriveInfo info{ReadDeviceAttribute(name, "vendor"), ReadDeviceAttribute(name, "model"),
                        ReadDeviceAttribute(name, "rev"), GetRememberedSerialNumber(name)};
  if (info.vendor.empty() && info.product.empty())
    return std::nullopt;
  return info;
}

std::optional<std::string> SCSIDevice::ReadSerialNumber(const std::string& path)
{
  const std::string name = GetBlockDeviceName(path);
  if (name.empty())
    return std::nullopt;
  return GetSerialNumber(name);
}

bool SCSIDevice::IsOpticalDrive(const std::string& path)
{
  return !GetBlockDeviceName(path).empty();
}

std::string SCSIDevice::GetCanonicalPath(const std::string& path)
{
  // Symlinks such as /dev/cdrom resolve to the block device they point to
  const std::string name = GetBlockDeviceName(path);
  return name.empty() ? path : "/dev/" + name;
}
}  // namespace Common
