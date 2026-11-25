// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/SCSIDevice.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <windows.h>
#include <winioctl.h>

#include <ntddscsi.h>
#include <ntddstor.h>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

namespace Common
{
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

// Returns false only if the CDB does not fit or the OS rejects the IOCTL itself
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

// SCSI TEST UNIT READY (0x00). A spinning-up drive answers it at once with a CHECK CONDITION
// (sense 02/04/01), so a short timeout returns the real state instead of waiting for the drive
bool SendTestUnitReady(HANDLE handle, const std::string& path, SCSISense* sense)
{
  constexpr std::array<u8, 6> cdb{};
  DEBUG_LOG_FMT(COMMON, "Sending SCSI opcode {:02X} to {} (0 data bytes)", cdb[0], path);
  SPTDWithSense sptd{};
  if (!SendPassThrough(handle, cdb, {}, SCSIDataDirection::Out,
                       SCSIDevice::STATUS_PROBE_TIMEOUT_SECONDS, &sptd))
  {
    return false;
  }
  DecodeSense(sptd, sense);
  return true;
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
    const DWORD error = GetLastError();
    ERROR_LOG_FMT(COMMON, "SCSI passthrough for opcode {:02X} failed: {}", cdb[0],
                  Common::GetWin32ErrorString(error));
    if (sense != nullptr)
      *sense = SCSISense{.refused = error == ERROR_ACCESS_DENIED};
    return false;
  }

  SCSISense decoded;
  DecodeSense(sptd, &decoded);
  decoded.delivered = true;
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

  SCSISense sense;
  const bool sent = SendTestUnitReady(handle, path, &sense);
  CloseHandle(handle);

  if (!sent)
    return OpticalDriveStatus::NotReadyOther;
  if (sense.status == 0)
    return OpticalDriveStatus::Ready;
  return GetStatusFromSense(sense);
}

std::optional<OpticalDriveStatus> SCSIDevice::GetStatus() const
{
  if (!IsOpen())
    return std::nullopt;

  SCSISense sense;
  if (!SendTestUnitReady(m_handle, m_path, &sense))
    return std::nullopt;
  if (sense.status == 0)
    return OpticalDriveStatus::Ready;
  return GetStatusFromSense(sense);
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

std::optional<std::string> SCSIDevice::ReadSerialNumber(const std::string& path)
{
  // The storage descriptor already holds it, so no command is sent to the drive
  const std::optional<OpticalDriveInfo> info = GetDriveInfo(path);
  if (!info)
    return std::nullopt;
  return info->serial_number;
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
}  // namespace Common
