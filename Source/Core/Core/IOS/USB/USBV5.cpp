// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/USBV5.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"

namespace IOS::HLE
{
namespace USB
{
V5CtrlMessage::V5CtrlMessage(EmulationKernel& ios, const IOCtlVRequest& ioctlv)
    : CtrlMessage(ios, ioctlv, ioctlv.GetVector(1)->address)
{
  auto& system = ios.GetSystem();
  auto& memory = system.GetMemory();
  request_type = memory.Read_U8(ioctlv.in_vectors[0].address + 8);
  request = memory.Read_U8(ioctlv.in_vectors[0].address + 9);
  value = memory.Read_U16(ioctlv.in_vectors[0].address + 10);
  index = memory.Read_U16(ioctlv.in_vectors[0].address + 12);
  length = static_cast<u16>(ioctlv.GetVector(1)->size);
}

V5BulkMessage::V5BulkMessage(EmulationKernel& ios, const IOCtlVRequest& ioctlv)
    : BulkMessage(ios, ioctlv, ioctlv.GetVector(1)->address)
{
  auto& system = ios.GetSystem();
  auto& memory = system.GetMemory();
  length = ioctlv.GetVector(1)->size;
  endpoint = memory.Read_U8(ioctlv.in_vectors[0].address + 18);
}

V5IntrMessage::V5IntrMessage(EmulationKernel& ios, const IOCtlVRequest& ioctlv)
    : IntrMessage(ios, ioctlv, ioctlv.GetVector(1)->address)
{
  auto& system = ios.GetSystem();
  auto& memory = system.GetMemory();
  length = ioctlv.GetVector(1)->size;
  endpoint = memory.Read_U8(ioctlv.in_vectors[0].address + 14);
}

V5IsoMessage::V5IsoMessage(EmulationKernel& ios, const IOCtlVRequest& ioctlv)
    : IsoMessage(ios, ioctlv, ioctlv.GetVector(2)->address)
{
  auto& system = ios.GetSystem();
  auto& memory = system.GetMemory();
  num_packets = memory.Read_U8(ioctlv.in_vectors[0].address + 16);
  endpoint = memory.Read_U8(ioctlv.in_vectors[0].address + 17);
  packet_sizes_addr = ioctlv.GetVector(1)->address;
  u32 total_packet_size = 0;
  for (size_t i = 0; i < num_packets; ++i)
  {
    const u32 packet_size = memory.Read_U16(static_cast<u32>(packet_sizes_addr + i * sizeof(u16)));
    packet_sizes.push_back(packet_size);
    total_packet_size += packet_size;
  }
  length = ioctlv.GetVector(2)->size;
  ASSERT_MSG(IOS_USB, length == total_packet_size, "Wrong buffer size ({:#x} != {:#x})", length,
             total_packet_size);
}
}  // namespace USB

namespace
{
#pragma pack(push, 1)
struct DeviceID
{
  u8 reserved;
  u8 index;
  u16 number;
};

struct DeviceEntry
{
  DeviceID id;
  u16 vid;
  u16 pid;
  u16 number;
  u8 interface_number;
  u8 num_altsettings;
};
#pragma pack(pop)
}  // namespace

void USBV5ResourceManager::DoState(PointerWrap& p)
{
  p.Do(m_has_pending_changes);
  p.Do(m_devicechange_hook_request);
  p.Do(m_usbv5_devices);
  USBHost::DoState(p);
}

USBV5ResourceManager::USBV5Device* USBV5ResourceManager::GetUSBV5Device(u32 in_buffer)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  const u8 index = memory.Read_U8(in_buffer + offsetof(DeviceID, index));
  const u16 number = memory.Read_U16(in_buffer + offsetof(DeviceID, number));

  if (index >= m_usbv5_devices.size())
    return nullptr;

  USBV5Device* usbv5_device = &m_usbv5_devices[index];
  if (!usbv5_device->in_use || usbv5_device->number != number)
    return nullptr;

  return usbv5_device;
}

std::optional<IPCReply> USBV5ResourceManager::GetDeviceChange(const IOCtlRequest& request)
{
  if (request.buffer_out_size != 0x180 || m_devicechange_hook_request)
    return IPCReply(IPC_EINVAL);

  std::lock_guard lk{m_devicechange_hook_address_mutex};
  m_devicechange_hook_request = request.address;
  // If there are pending changes, the reply is sent immediately (instead of on device
  // insertion/removal).
  if (m_has_pending_changes)
  {
    TriggerDeviceChangeReply();
    m_has_pending_changes = false;
  }
  return std::nullopt;
}

IPCReply USBV5ResourceManager::SetAlternateSetting(USBV5Device& device, const IOCtlRequest& request)
{
  const auto host_device = GetDeviceById(device.host_id);
  if (!host_device->AttachAndChangeInterface(device.interface_number))
    return IPCReply(-1);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  const u8 alt_setting = memory.Read_U8(request.buffer_in + 2 * sizeof(s32));

  const bool success = host_device->SetAltSetting(alt_setting) == 0;
  return IPCReply(success ? IPC_SUCCESS : IPC_EINVAL);
}

IPCReply USBV5ResourceManager::Shutdown(const IOCtlRequest& request)
{
  if (request.buffer_in != 0 || request.buffer_in_size != 0 || request.buffer_out != 0 ||
      request.buffer_out_size != 0)
  {
    return IPCReply(IPC_EINVAL);
  }

  std::lock_guard lk{m_devicechange_hook_address_mutex};
  if (m_devicechange_hook_request)
  {
    IOCtlRequest change_request{GetSystem(), m_devicechange_hook_request.value()};
    GetEmulationKernel().EnqueueIPCReply(change_request, IPC_SUCCESS);
    m_devicechange_hook_request.reset();
  }
  return IPCReply(IPC_SUCCESS);
}

IPCReply USBV5ResourceManager::SuspendResume(USBV5Device& device, const IOCtlRequest& request)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const auto host_device = GetDeviceById(device.host_id);
  const s32 resumed = memory.Read_U32(request.buffer_in + 8);

  // Note: this is unimplemented because there's no easy way to do this in a
  // platform-independant way (libusb does not support power management).
  INFO_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Received {} command", host_device->GetVid(),
               host_device->GetPid(), device.interface_number, resumed == 0 ? "suspend" : "resume");
  return IPCReply(IPC_SUCCESS);
}

std::optional<IPCReply> USBV5ResourceManager::HandleDeviceIOCtl(const IOCtlRequest& request,
                                                                Handler handler)
{
  if (request.buffer_in == 0 || request.buffer_in_size != 0x20)
    return IPCReply(IPC_EINVAL);

  std::lock_guard lock{m_usbv5_devices_mutex};
  USBV5Device* device = GetUSBV5Device(request.buffer_in);
  if (!device)
    return IPCReply(IPC_EINVAL);
  return handler(*device);
}

void USBV5ResourceManager::OnDeviceChange(const ChangeEvent event,
                                          std::shared_ptr<USB::Device> device)
{
  std::lock_guard lock{m_usbv5_devices_mutex};
  const u64 host_device_id = device->GetId();
  if (event == ChangeEvent::Inserted)
  {
    for (const auto& interface : device->GetInterfaces(0))
    {
      if (interface.bAlternateSetting != 0)
        continue;

      auto it = std::find_if(m_usbv5_devices.rbegin(), m_usbv5_devices.rend(),
                             [](const USBV5Device& entry) { return !entry.in_use; });
      if (it == m_usbv5_devices.rend())
        return;

      it->in_use = true;
      it->interface_number = interface.bInterfaceNumber;
      it->number = m_current_device_number;
      it->host_id = host_device_id;
    }
  }
  else if (event == ChangeEvent::Removed)
  {
    for (USBV5Device& entry : m_usbv5_devices)
    {
      if (entry.host_id == host_device_id)
        entry.in_use = false;
    }
  }
}

void USBV5ResourceManager::OnDeviceChangeEnd()
{
  std::lock_guard lk{m_devicechange_hook_address_mutex};
  TriggerDeviceChangeReply();
  ++m_current_device_number;
}

// Must be called with m_devicechange_hook_address_mutex locked
void USBV5ResourceManager::TriggerDeviceChangeReply()
{
  if (!m_devicechange_hook_request)
  {
    m_has_pending_changes = true;
    return;
  }

  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  IOCtlRequest request{GetSystem(), m_devicechange_hook_request.value()};

  std::lock_guard lock{m_usbv5_devices_mutex};
  u8 num_devices = 0;
  for (auto it = m_usbv5_devices.crbegin(); it != m_usbv5_devices.crend(); ++it)
  {
    const USBV5Device& usbv5_device = *it;
    if (!usbv5_device.in_use)
      continue;

    const auto device = GetDeviceById(usbv5_device.host_id);
    if (!device)
      continue;

    DeviceEntry entry;
    if (HasInterfaceNumberInIDs())
    {
      entry.id.reserved = usbv5_device.interface_number;
    }
    else
    {
      // The actual value is static_cast<u8>(hook_internal_ipc_request >> 8).
      // Since we don't actually emulate the IOS kernel and internal IPC,
      // just pretend the value is 0xe7 (most common value according to hwtests).
      entry.id.reserved = 0xe7;
    }
    entry.id.index = static_cast<u8>(std::distance(m_usbv5_devices.cbegin(), it.base()) - 1);
    entry.id.number = Common::swap16(usbv5_device.number);
    entry.vid = Common::swap16(device->GetVid());
    entry.pid = Common::swap16(device->GetPid());
    entry.number = Common::swap16(usbv5_device.number);
    entry.interface_number = usbv5_device.interface_number;
    entry.num_altsettings = device->GetNumberOfAltSettings(entry.interface_number);

    memory.CopyToEmu(request.buffer_out + sizeof(entry) * num_devices, &entry, sizeof(entry));
    ++num_devices;
  }

  GetEmulationKernel().EnqueueIPCReply(request, num_devices, 0, CoreTiming::FromThread::ANY);
  m_devicechange_hook_request.reset();
  INFO_LOG_FMT(IOS_USB, "{} USBv5 device(s), including interfaces", num_devices);
}
}  // namespace IOS::HLE
