// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/OH0/OH0Device.h"

#include <charconv>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/StrStream.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/OH0/OH0.h"

namespace IOS::HLE
{
static void GetVidPidFromDevicePath(const std::string& device_path, u16& vid, u16& pid)
{
  std::istrstream stream{device_path.c_str(), std::ssize(device_path)};
  std::vector<std::string> list;
  for (std::string segment; std::getline(stream, segment, '/');)
    if (!segment.empty())
      list.push_back(segment);

  if (list.size() != 5)
    return;

  const char *const vid_begin = list[3].data(), *const vid_end = vid_begin + list[3].size();
  std::from_chars(vid_begin, vid_end, vid, 16);
  const char *const pid_begin = list[4].data(), *const pid_end = pid_begin + list[4].size();
  std::from_chars(pid_begin, pid_end, pid, 16);
}

OH0Device::OH0Device(EmulationKernel& ios, const std::string& name)
    : EmulationDevice(ios, name, DeviceType::OH0)
{
  if (!name.empty())
    GetVidPidFromDevicePath(name, m_vid, m_pid);
}

void OH0Device::DoState(PointerWrap& p)
{
  m_oh0 = std::static_pointer_cast<OH0>(GetEmulationKernel().GetDeviceByName("/dev/usb/oh0"));
  Device::DoState(p);
  p.Do(m_vid);
  p.Do(m_pid);
  p.Do(m_device_id);
}

std::optional<IPCReply> OH0Device::Open(const OpenRequest& request)
{
  if (m_vid == 0 && m_pid == 0)
    return IPCReply(IPC_ENOENT);

  m_oh0 = std::static_pointer_cast<OH0>(GetEmulationKernel().GetDeviceByName("/dev/usb/oh0"));

  ReturnCode return_code;
  std::tie(return_code, m_device_id) = m_oh0->DeviceOpen(m_vid, m_pid);
  return IPCReply(return_code);
}

std::optional<IPCReply> OH0Device::Close(u32 fd)
{
  m_oh0->DeviceClose(m_device_id);
  return Device::Close(fd);
}

std::optional<IPCReply> OH0Device::IOCtl(const IOCtlRequest& request)
{
  return m_oh0->DeviceIOCtl(m_device_id, request);
}

std::optional<IPCReply> OH0Device::IOCtlV(const IOCtlVRequest& request)
{
  return m_oh0->DeviceIOCtlV(m_device_id, request);
}
}  // namespace IOS::HLE
