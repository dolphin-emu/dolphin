// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/USB_VEN/VEN.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <mutex>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/USB/Common.h"
#include "Core/System.h"

namespace IOS::HLE
{
constexpr u32 USBV5_VERSION = 0x50001;

USB_VEN::~USB_VEN()
{
  m_scan_thread.Stop();
}

std::optional<IPCReply> USB_VEN::IOCtl(const IOCtlRequest& request)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  request.Log(GetDeviceName(), Common::Log::LogType::IOS_USB);
  switch (request.request)
  {
  case USB::IOCTL_USBV5_GETVERSION:
    memory.Write_U32(USBV5_VERSION, request.buffer_out);
    return IPCReply(IPC_SUCCESS);
  case USB::IOCTL_USBV5_GETDEVICECHANGE:
    return GetDeviceChange(request);
  case USB::IOCTL_USBV5_SHUTDOWN:
    return Shutdown(request);
  case USB::IOCTL_USBV5_GETDEVPARAMS:
    return HandleDeviceIOCtl(request,
                             [&](USBV5Device& device) { return GetDeviceInfo(device, request); });
  case USB::IOCTL_USBV5_ATTACHFINISH:
    return IPCReply(IPC_SUCCESS);
  case USB::IOCTL_USBV5_SETALTERNATE:
    return HandleDeviceIOCtl(
        request, [&](USBV5Device& device) { return SetAlternateSetting(device, request); });
  case USB::IOCTL_USBV5_SUSPEND_RESUME:
    return HandleDeviceIOCtl(request,
                             [&](USBV5Device& device) { return SuspendResume(device, request); });
  case USB::IOCTL_USBV5_CANCELENDPOINT:
    return HandleDeviceIOCtl(request,
                             [&](USBV5Device& device) { return CancelEndpoint(device, request); });
  default:
    request.DumpUnknown(GetSystem(), GetDeviceName(), Common::Log::LogType::IOS_USB,
                        Common::Log::LogLevel::LERROR);
    return IPCReply(IPC_SUCCESS);
  }
}

std::optional<IPCReply> USB_VEN::IOCtlV(const IOCtlVRequest& request)
{
  static const std::map<u32, u32> s_num_vectors = {
      {USB::IOCTLV_USBV5_CTRLMSG, 2},
      {USB::IOCTLV_USBV5_INTRMSG, 2},
      {USB::IOCTLV_USBV5_BULKMSG, 2},
      {USB::IOCTLV_USBV5_ISOMSG, 4},
  };

  switch (request.request)
  {
  case USB::IOCTLV_USBV5_CTRLMSG:
  case USB::IOCTLV_USBV5_INTRMSG:
  case USB::IOCTLV_USBV5_BULKMSG:
  case USB::IOCTLV_USBV5_ISOMSG:
  {
    if (request.in_vectors.size() + request.io_vectors.size() != s_num_vectors.at(request.request))
      return IPCReply(IPC_EINVAL);

    std::lock_guard lock{m_usbv5_devices_mutex};
    USBV5Device* device = GetUSBV5Device(request.in_vectors[0].address);
    if (!device)
      return IPCReply(IPC_EINVAL);
    auto host_device = GetDeviceById(device->host_id);
    if (request.request == USB::IOCTLV_USBV5_CTRLMSG)
      host_device->Attach();
    else
      host_device->AttachAndChangeInterface(device->interface_number);
    return HandleTransfer(host_device, request.request,
                          [&, this]() { return SubmitTransfer(*host_device, request); });
  }
  default:
    return IPCReply(IPC_EINVAL);
  }
}

s32 USB_VEN::SubmitTransfer(USB::Device& device, const IOCtlVRequest& ioctlv)
{
  switch (ioctlv.request)
  {
  case USB::IOCTLV_USBV5_CTRLMSG:
    return device.SubmitTransfer(
        std::make_unique<USB::V5CtrlMessage>(GetEmulationKernel(), ioctlv));
  case USB::IOCTLV_USBV5_INTRMSG:
    return device.SubmitTransfer(
        std::make_unique<USB::V5IntrMessage>(GetEmulationKernel(), ioctlv));
  case USB::IOCTLV_USBV5_BULKMSG:
    return device.SubmitTransfer(
        std::make_unique<USB::V5BulkMessage>(GetEmulationKernel(), ioctlv));
  case USB::IOCTLV_USBV5_ISOMSG:
    return device.SubmitTransfer(std::make_unique<USB::V5IsoMessage>(GetEmulationKernel(), ioctlv));
  default:
    return IPC_EINVAL;
  }
}

IPCReply USB_VEN::CancelEndpoint(USBV5Device& device, const IOCtlRequest& request)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const u8 endpoint = memory.Read_U8(request.buffer_in + 8);
  // IPC_EINVAL (-4) is returned when no transfer was cancelled.
  if (GetDeviceById(device.host_id)->CancelTransfer(endpoint) < 0)
    return IPCReply(IPC_EINVAL);
  return IPCReply(IPC_SUCCESS);
}

IPCReply USB_VEN::GetDeviceInfo(USBV5Device& device, const IOCtlRequest& request)
{
  if (request.buffer_out == 0 || request.buffer_out_size != 0xc0)
    return IPCReply(IPC_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const std::shared_ptr<USB::Device> host_device = GetDeviceById(device.host_id);
  const u8 alt_setting = memory.Read_U8(request.buffer_in + 8);

  memory.Memset(request.buffer_out, 0, request.buffer_out_size);
  memory.Write_U32(memory.Read_U32(request.buffer_in), request.buffer_out);
  memory.Write_U32(1, request.buffer_out + 4);

  USB::DeviceDescriptor device_descriptor = host_device->GetDeviceDescriptor();
  device_descriptor.Swap();
  memory.CopyToEmu(request.buffer_out + 20, &device_descriptor, sizeof(device_descriptor));

  // VEN only cares about the first configuration.
  USB::ConfigDescriptor config_descriptor = host_device->GetConfigurations()[0];
  config_descriptor.Swap();
  memory.CopyToEmu(request.buffer_out + 40, &config_descriptor, sizeof(config_descriptor));

  std::vector<USB::InterfaceDescriptor> interfaces = host_device->GetInterfaces(0);
  auto it = std::find_if(interfaces.begin(), interfaces.end(),
                         [&](const USB::InterfaceDescriptor& interface) {
                           return interface.bInterfaceNumber == device.interface_number &&
                                  interface.bAlternateSetting == alt_setting;
                         });
  if (it == interfaces.end())
    return IPCReply(IPC_EINVAL);
  it->Swap();
  memory.CopyToEmu(request.buffer_out + 52, &*it, sizeof(*it));

  auto endpoints = host_device->GetEndpoints(0, it->bInterfaceNumber, it->bAlternateSetting);
  for (size_t i = 0; i < endpoints.size(); ++i)
  {
    endpoints[i].Swap();
    memory.CopyToEmu(request.buffer_out + 64 + 8 * static_cast<u8>(i), &endpoints[i],
                     sizeof(endpoints[i]));
  }

  return IPCReply(IPC_SUCCESS);
}
}  // namespace IOS::HLE
