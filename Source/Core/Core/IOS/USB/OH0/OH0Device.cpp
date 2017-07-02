// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/USB/OH0/OH0Device.h"

#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "Common/ChunkFile.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/OH0/OH0.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
static void GetVidPidFromDevicePath(const std::string& device_path, u16& vid, u16& pid)
{
  std::stringstream stream{device_path};
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

OH0Device::OH0Device(Kernel& ios, const std::string& name) : Device(ios, name, DeviceType::OH0)
{
  if (!name.empty())
    GetVidPidFromDevicePath(name, m_vid, m_pid);
}

void OH0Device::DoState(PointerWrap& p)
{
  m_oh0 = std::static_pointer_cast<OH0>(GetIOS()->GetDeviceByName("/dev/usb/oh0"));
  p.Do(m_name);
  p.Do(m_vid);
  p.Do(m_pid);
  p.Do(m_device_id);
}

ReturnCode OH0Device::Open(const OpenRequest& request)
{
  const u32 ios_major_version = m_ios.GetVersion();
  if (ios_major_version == 57 || ios_major_version == 58 || ios_major_version == 59)
    return IPC_ENOENT;

  if (m_vid == 0 && m_pid == 0)
    return IPC_ENOENT;

  m_oh0 = std::static_pointer_cast<OH0>(GetIOS()->GetDeviceByName("/dev/usb/oh0"));

  ReturnCode return_code;
  std::tie(return_code, m_device_id) = m_oh0->DeviceOpen(m_vid, m_pid);
  return return_code;
}

ReturnCode OH0Device::Close(u32 fd)
{
  m_oh0->DeviceClose(m_device_id);
  return Device::Close(fd);
}

IPCCommandResult OH0Device::IOCtl(const IOCtlRequest& request)
{
  return m_oh0->DeviceIOCtl(m_device_id, request);
}

IPCCommandResult OH0Device::IOCtlV(const IOCtlVRequest& request)
{
  return m_oh0->DeviceIOCtlV(m_device_id, request);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
