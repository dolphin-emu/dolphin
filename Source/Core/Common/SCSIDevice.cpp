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
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <fmt/format.h>

#if defined(_WIN32)
#include <windows.h>
#include <winioctl.h>

#include <ntddscsi.h>
#include <ntddstor.h>
#elif defined(__linux__) && !defined(__ANDROID__)
#include <fcntl.h>
#include <scsi/sg.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

namespace Common
{
SCSIDevice::~SCSIDevice()
{
  Close();
}

#if defined(_WIN32)

namespace
{
// Timeout for the TEST UNIT READY that reads drive status. Short, so a spinning-up drive is
// reported as such at once instead of blocking until it is ready
constexpr u32 STATUS_PROBE_TIMEOUT_SECONDS = 1;
}  // namespace

namespace
{
// SCSI_PASS_THROUGH_DIRECT and its sense buffer in one contiguous block, so the kernel can write
// sense data at SenseInfoOffset
struct SPTDWithSense
{
  SCSI_PASS_THROUGH_DIRECT sptd;
  std::array<u8, 24> sense;
};

std::wstring ToDevicePath(const std::string& path)
{
  return UTF8ToWString(R"(\\.\)" + path);
}

HANDLE OpenDeviceHandle(const std::string& path, DWORD access)
{
  return CreateFileW(ToDevicePath(path).c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE,
                     nullptr, OPEN_EXISTING, 0, nullptr);
}

void DecodeSense(const SPTDWithSense& sptd, SCSISense* sense)
{
  sense->status = sptd.sptd.ScsiStatus;
  sense->sense_key = sptd.sense[2] & 0x0F;
  sense->asc = sptd.sense[12];
  sense->ascq = sptd.sense[13];
}

// Returns false only if the OS rejects the IOCTL itself
bool SendPassThrough(HANDLE handle, std::span<const u8> cdb, std::span<u8> buffer,
                     SCSIDataDirection direction, u32 timeout_seconds, SPTDWithSense* sptd)
{
  if (cdb.size() > sizeof(SCSI_PASS_THROUGH_DIRECT::Cdb))
    return false;

  *sptd = {};
  sptd->sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
  sptd->sptd.CdbLength = static_cast<UCHAR>(cdb.size());
  sptd->sptd.SenseInfoLength = static_cast<UCHAR>(sptd->sense.size());
  sptd->sptd.DataIn = static_cast<UCHAR>(direction);
  sptd->sptd.DataTransferLength = static_cast<ULONG>(buffer.size());
  sptd->sptd.TimeOutValue = timeout_seconds;
  sptd->sptd.DataBuffer = buffer.empty() ? nullptr : buffer.data();
  sptd->sptd.SenseInfoOffset = static_cast<ULONG>(offsetof(SPTDWithSense, sense));
  std::ranges::copy(cdb, sptd->sptd.Cdb);

  DWORD bytes_returned = 0;
  return DeviceIoControl(handle, IOCTL_SCSI_PASS_THROUGH_DIRECT, sptd, sizeof(*sptd), sptd,
                         sizeof(*sptd), &bytes_returned, nullptr) != FALSE;
}

std::string ReadDescriptorString(std::span<const u8> data, u32 offset)
{
  if (offset == 0 || offset >= data.size())
    return {};
  const std::string_view text(reinterpret_cast<const char*>(data.data()) + offset,
                              data.size() - offset);
  return std::string(StripWhitespace(text.substr(0, text.find('\0'))));
}
}  // namespace

bool SCSIDevice::IsSupported()
{
  return true;
}

bool SCSIDevice::Open(const std::string& path, bool read_write)
{
  Close();

  DWORD desired_access = GENERIC_READ;
  if (read_write)
    desired_access |= GENERIC_WRITE;

  HANDLE handle = OpenDeviceHandle(path, desired_access);

  // Standard user accounts can be refused GENERIC_WRITE, so fall back to read-only before failing
  if (handle == INVALID_HANDLE_VALUE && read_write)
    handle = OpenDeviceHandle(path, GENERIC_READ);

  if (handle == INVALID_HANDLE_VALUE)
  {
    ERROR_LOG_FMT(COMMON, "Failed to open optical drive {}: {}", path, GetLastErrorString());
    return false;
  }

  m_handle = handle;
  m_path = path;
  return true;
}

void SCSIDevice::Close()
{
  if (m_handle != nullptr)
  {
    CloseHandle(m_handle);
    m_handle = nullptr;
  }
}

bool SCSIDevice::IsOpen() const
{
  return m_handle != nullptr;
}

bool SCSIDevice::SendCommand(std::span<const u8> cdb, std::span<u8> buffer,
                             SCSIDataDirection direction, u32 timeout_seconds,
                             SCSISense* sense) const
{
  if (!IsOpen() || cdb.empty())
    return false;

  DEBUG_LOG_FMT(COMMON, "Sending SCSI opcode {:02X} to {} ({} data bytes)", cdb[0], m_path,
                buffer.size());

  SPTDWithSense sptd{};
  if (!SendPassThrough(m_handle, cdb, buffer, direction, timeout_seconds, &sptd))
  {
    ERROR_LOG_FMT(COMMON, "SCSI passthrough for opcode {:02X} failed: {}", cdb[0],
                  GetLastErrorString());
    return false;
  }

  SCSISense decoded;
  DecodeSense(sptd, &decoded);
  if (sense != nullptr)
    *sense = decoded;

  // Any status other than 0x00 (GOOD) is a device-side failure
  if (decoded.status != 0)
  {
    ERROR_LOG_FMT(COMMON,
                  "SCSI opcode {:02X} failed. Status: {:02X}, Sense Key: {:02X}, ASC: {:02X}, "
                  "ASCQ: {:02X}",
                  cdb[0], decoded.status, decoded.sense_key, decoded.asc, decoded.ascq);
    return false;
  }
  return true;
}

OpticalDriveStatus SCSIDevice::GetDriveStatus(const std::string& path)
{
  HANDLE handle = OpenDeviceHandle(path, GENERIC_READ | GENERIC_WRITE);
  if (handle == INVALID_HANDLE_VALUE)
    handle = OpenDeviceHandle(path, GENERIC_READ);

  if (handle == INVALID_HANDLE_VALUE)
  {
    const DWORD error = GetLastError();
    if (error == ERROR_NOT_READY || error == ERROR_MEDIA_CHANGED)
      return OpticalDriveStatus::NoDisc;
    return OpticalDriveStatus::NotReadyOther;
  }

  // TEST UNIT READY (opcode 0x00). A spinning-up drive answers it at once with a CHECK CONDITION
  // (sense 02/04/01), so a short timeout returns the real state instead of waiting for the drive
  constexpr std::array<u8, 6> cdb{};
  DEBUG_LOG_FMT(COMMON, "Sending SCSI opcode {:02X} to {} (0 data bytes)", cdb[0], path);
  SPTDWithSense sptd{};
  const bool sent =
      SendPassThrough(handle, cdb, {}, SCSIDataDirection::In, STATUS_PROBE_TIMEOUT_SECONDS, &sptd);
  CloseHandle(handle);

  if (sent && sptd.sptd.ScsiStatus == 0)
    return OpticalDriveStatus::Ready;

  SCSISense sense;
  DecodeSense(sptd, &sense);
  if (sense.sense_key == 0x02)
  {
    // ASC 0x3A: no medium present (ASCQ 0x02: tray open)
    // ASC 0x04, ASCQ 0x01: spin-up in progress
    if (sense.asc == 0x3A)
      return sense.ascq == 0x02 ? OpticalDriveStatus::TrayOpen : OpticalDriveStatus::NoDisc;
    if (sense.asc == 0x04 && sense.ascq == 0x01)
      return OpticalDriveStatus::BecomingReady;
  }
  return OpticalDriveStatus::NotReadyOther;
}

std::vector<std::string> SCSIDevice::ListOpticalDrives()
{
  // Only the drive type is checked, without opening any device, so this returns at once
  std::vector<std::string> drives;
  for (char letter = 'A'; letter <= 'Z'; ++letter)
  {
    std::string path{letter, ':'};
    if (IsOpticalDrive(path))
      drives.push_back(std::move(path));
  }
  return drives;
}

std::optional<OpticalDriveInfo> SCSIDevice::GetDriveInfo(const std::string& path)
{
  // Zero access rights so the open does not block on spin-up delays
  const HANDLE handle = OpenDeviceHandle(path, 0);
  if (handle == INVALID_HANDLE_VALUE)
    return std::nullopt;

  STORAGE_PROPERTY_QUERY query{};
  query.PropertyId = StorageDeviceProperty;
  query.QueryType = PropertyStandardQuery;

  std::array<u8, 1024> out{};
  DWORD bytes = 0;
  const bool ok =
      DeviceIoControl(handle, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), out.data(),
                      static_cast<DWORD>(out.size()), &bytes, nullptr) != FALSE;
  CloseHandle(handle);
  if (!ok)
    return std::nullopt;

  const std::span<const u8> returned(out.data(), bytes);
  const auto* descriptor = reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR*>(out.data());
  return OpticalDriveInfo{ReadDescriptorString(returned, descriptor->VendorIdOffset),
                          ReadDescriptorString(returned, descriptor->ProductIdOffset),
                          ReadDescriptorString(returned, descriptor->ProductRevisionOffset),
                          ReadDescriptorString(returned, descriptor->SerialNumberOffset)};
}

bool SCSIDevice::IsOpticalDrive(const std::string& path)
{
  // Only bare drive letters ("D:") qualify, which keeps this cheap for game list scans
  if (path.size() != 2 || path[1] != ':' || !std::isalpha(static_cast<unsigned char>(path[0])))
    return false;
  return GetDriveTypeW(UTF8ToWString(path + '\\').c_str()) == DRIVE_CDROM;
}

std::string SCSIDevice::GetCanonicalPath(const std::string& path)
{
  // Drive letters are case-insensitive
  if (!IsOpticalDrive(path))
    return path;
  return std::string{static_cast<char>(std::toupper(static_cast<unsigned char>(path[0]))), ':'};
}

#elif defined(__linux__) && !defined(__ANDROID__)

namespace
{
// Timeout for the TEST UNIT READY that reads drive status. Short, so a spinning-up drive is
// reported as such at once instead of blocking until it is ready
constexpr u32 STATUS_PROBE_TIMEOUT_SECONDS = 1;

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

std::mutex s_feature_serial_numbers_mutex;
std::map<std::string, std::string> s_feature_serial_numbers;

std::optional<std::string> QueryFeatureSerialNumber(const std::string& name)
{
  // SCSI GET CONFIGURATION is on the kernel's list of safe commands, so the block device works too
  const std::string sg_node = GetSCSIGenericNode(name);
  int fd = sg_node.empty() ? -1 : open(sg_node.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
  if (fd == -1)
    fd = open(("/dev/" + name).c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd == -1)
    return std::nullopt;

  // SCSI GET CONFIGURATION (0x46), starting at feature 0108h, RT 0
  std::array<u8, 64> buffer{};
  const std::array<u8, 10> cdb{
      0x46, 0x00, 0x01, 0x08, 0x00, 0x00, 0x00, 0x00, static_cast<u8>(buffer.size()), 0x00};
  sg_io_hdr_t header{};
  std::array<u8, SENSE_BUFFER_SIZE> sense_buffer{};
  const bool sent = SendSGIO(fd, cdb, buffer, SCSIDataDirection::In, STATUS_PROBE_TIMEOUT_SECONDS,
                             &header, &sense_buffer);
  close(fd);
  if (!sent || !CommandSucceeded(header))
    return std::nullopt;

  // A drive without the feature returns the next feature it has, or none
  const u32 data_length = Common::swap32(buffer.data()) + 4;
  if (data_length < 12 || Common::swap16(&buffer[8]) != 0x0108)
    return std::string();

  const std::size_t length =
      std::min<std::size_t>({buffer[11], buffer.size() - 12, std::size_t{data_length} - 12});
  return std::string(
      StripWhitespace(std::string_view(reinterpret_cast<const char*>(&buffer[12]), length)));
}

// The drive's serial number, retrieved from SCSI GET CONFIGURATION. Each attached drive is asked
// once, and only once it has settled, so known and busy drives receive no command
std::string GetSerialNumber(const std::string& name)
{
  std::error_code error;
  const std::string device =
      std::filesystem::canonical(fmt::format("/sys/block/{}/device", name), error).string();
  if (error)
    return {};

  {
    std::lock_guard lock(s_feature_serial_numbers_mutex);
    const auto it = s_feature_serial_numbers.find(device);
    if (it != s_feature_serial_numbers.end())
      return it->second;
  }

  // A busy drive may hold the command until it settles. Try again on a later scan instead
  const OpticalDriveStatus status = SCSIDevice::GetDriveStatus("/dev/" + name);
  if (status == OpticalDriveStatus::BecomingReady || status == OpticalDriveStatus::NotReadyOther)
    return {};

  const std::optional<std::string> serial = QueryFeatureSerialNumber(name);
  if (!serial)
    return {};

  std::lock_guard lock(s_feature_serial_numbers_mutex);
  s_feature_serial_numbers[device] = *serial;
  return *serial;
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

  // Users with read-only access fall back to read-only, which still allows every read command
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

  const SCSISense decoded = DecodeSense(header, sense_buffer);
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

  // TEST UNIT READY (opcode 0x00). A spinning-up drive answers it at once with a CHECK CONDITION
  // (sense 02/04/01), so a short timeout returns the real state instead of waiting for the drive
  constexpr std::array<u8, 6> cdb{};
  DEBUG_LOG_FMT(COMMON, "Sending SCSI opcode {:02X} to {} (0 data bytes)", cdb[0], path);
  sg_io_hdr_t header{};
  std::array<u8, SENSE_BUFFER_SIZE> sense_buffer{};
  const bool sent = SendSGIO(fd, cdb, {}, SCSIDataDirection::In, STATUS_PROBE_TIMEOUT_SECONDS,
                             &header, &sense_buffer);
  close(fd);

  if (!sent)
    return OpticalDriveStatus::NotReadyOther;
  if (CommandSucceeded(header))
    return OpticalDriveStatus::Ready;

  const SCSISense sense = DecodeSense(header, sense_buffer);
  if (sense.sense_key == 0x02)
  {
    // ASC 0x3A: no medium present (ASCQ 0x02: tray open)
    // ASC 0x04, ASCQ 0x01: spin-up in progress
    if (sense.asc == 0x3A)
      return sense.ascq == 0x02 ? OpticalDriveStatus::TrayOpen : OpticalDriveStatus::NoDisc;
    if (sense.asc == 0x04 && sense.ascq == 0x01)
      return OpticalDriveStatus::BecomingReady;
  }
  return OpticalDriveStatus::NotReadyOther;
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

  OpticalDriveInfo info{ReadDeviceAttribute(name, "vendor"), ReadDeviceAttribute(name, "model"),
                        ReadDeviceAttribute(name, "rev"), GetSerialNumber(name)};
  if (info.vendor.empty() && info.product.empty())
    return std::nullopt;
  return info;
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

#else

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
  m_fd = -1;
}

bool SCSIDevice::IsOpen() const
{
  return m_fd != -1;
}

bool SCSIDevice::SendCommand(std::span<const u8>, std::span<u8>, SCSIDataDirection, u32,
                             SCSISense*) const
{
  return false;
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
