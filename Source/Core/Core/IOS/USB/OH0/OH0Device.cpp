// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/OH0/OH0Device.h"

#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "Common/ChunkFile.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/OH0/OH0.h"

namespace IOS::HLE
{
static void GetVidPidFromDevicePath(const std::string& device_path, u16& vid, u16& pid)
{
  std::istringstream stream{device_path};
  std::string segment;
  std::vector<std::string> list;
  while (std::getline(stream, segment, '/'))
    if (!segment.empty())
      list.push_back(segment);

  if (list.size() != 5)
    return;

  std::stringstream ss;
  ss << std::hex << list[3];
  ss >> vid;
  ss.clear();
  ss << std::hex << list[4];
  ss >> pid;
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
