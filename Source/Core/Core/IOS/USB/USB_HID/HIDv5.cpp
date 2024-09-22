// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/USB_HID/HIDv5.h"

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

USB_HIDv5::~USB_HIDv5()
{
  m_scan_thread.Stop();
}

std::optional<IPCReply> USB_HIDv5::IOCtl(const IOCtlRequest& request)
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

std::optional<IPCReply> USB_HIDv5::IOCtlV(const IOCtlVRequest& request)
{
  switch (request.request)
  {
  // TODO: HIDv5 seems to be able to queue transfers depending on the transfer length (unlike VEN).
  case USB::IOCTLV_USBV5_CTRLMSG:
  case USB::IOCTLV_USBV5_INTRMSG:
  {
    // IOS does not check the number of vectors, but let's do that to avoid out-of-bounds reads.
    if (request.in_vectors.size() + request.io_vectors.size() != 2)
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
                          [&, this]() { return SubmitTransfer(*device, *host_device, request); });
  }
  default:
    request.DumpUnknown(GetSystem(), GetDeviceName(), Common::Log::LogType::IOS_USB);
    return IPCReply(IPC_EINVAL);
  }
}

s32 USB_HIDv5::SubmitTransfer(USBV5Device& device, USB::Device& host_device,
                              const IOCtlVRequest& ioctlv)
{
  switch (ioctlv.request)
  {
  case USB::IOCTLV_USBV5_CTRLMSG:
    return host_device.SubmitTransfer(
        std::make_unique<USB::V5CtrlMessage>(GetEmulationKernel(), ioctlv));
  case USB::IOCTLV_USBV5_INTRMSG:
  {
    auto message = std::make_unique<USB::V5IntrMessage>(GetEmulationKernel(), ioctlv);

    auto& system = GetSystem();
    auto& memory = system.GetMemory();

    // Unlike VEN, the endpoint is determined by the value at 8-12.
    // If it's non-zero, HID submits the request to the interrupt OUT endpoint.
    // Otherwise, the request is submitted to the IN endpoint.
    AdditionalDeviceData* data = &m_additional_device_data[&device - m_usbv5_devices.data()];
    if (memory.Read_U32(ioctlv.in_vectors[0].address + 8) != 0)
      message->endpoint = data->interrupt_out_endpoint;
    else
      message->endpoint = data->interrupt_in_endpoint;

    return host_device.SubmitTransfer(std::move(message));
  }
  default:
    return IPC_EINVAL;
  }
}

IPCReply USB_HIDv5::CancelEndpoint(USBV5Device& device, const IOCtlRequest& request)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const u8 value = memory.Read_U8(request.buffer_in + 8);
  u8 endpoint = 0;
  switch (value)
  {
  case 0:
    // TODO: cancel all queued control transfers with return code -7022.
    endpoint = 0;
    break;
  case 1:
    // TODO: cancel all queued interrupt transfers with return code -7022.
    endpoint = m_additional_device_data[&device - m_usbv5_devices.data()].interrupt_in_endpoint;
    break;
  case 2:
    // TODO: cancel all queued interrupt transfers with return code -7022.
    endpoint = m_additional_device_data[&device - m_usbv5_devices.data()].interrupt_out_endpoint;
    break;
  }

  GetDeviceById(device.host_id)->CancelTransfer(endpoint);
  return IPCReply(IPC_SUCCESS);
}

IPCReply USB_HIDv5::GetDeviceInfo(USBV5Device& device, const IOCtlRequest& request)
{
  if (request.buffer_out == 0 || request.buffer_out_size != 0x60)
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
  memory.CopyToEmu(request.buffer_out + 36, &device_descriptor, sizeof(device_descriptor));

  // Just like VEN, HIDv5 only cares about the first configuration.
  USB::ConfigDescriptor config_descriptor = host_device->GetConfigurations()[0];
  config_descriptor.Swap();
  memory.CopyToEmu(request.buffer_out + 56, &config_descriptor, sizeof(config_descriptor));

  std::vector<USB::InterfaceDescriptor> interfaces = host_device->GetInterfaces(0);
  auto it = std::ranges::find_if(interfaces, [&](const USB::InterfaceDescriptor& interface) {
    return interface.bInterfaceNumber == device.interface_number &&
           interface.bAlternateSetting == alt_setting;
  });
  if (it == interfaces.end())
    return IPCReply(IPC_EINVAL);
  it->Swap();
  memory.CopyToEmu(request.buffer_out + 68, &*it, sizeof(*it));

  auto endpoints = host_device->GetEndpoints(0, it->bInterfaceNumber, it->bAlternateSetting);
  for (auto& endpoint : endpoints)
  {
    constexpr u8 ENDPOINT_INTERRUPT = 0b11;
    constexpr u8 ENDPOINT_IN = 0x80;
    if (endpoint.bmAttributes == ENDPOINT_INTERRUPT)
    {
      const bool is_in_endpoint = (endpoint.bEndpointAddress & ENDPOINT_IN) != 0;

      AdditionalDeviceData* data = &m_additional_device_data[&device - m_usbv5_devices.data()];
      if (is_in_endpoint)
        data->interrupt_in_endpoint = endpoint.bEndpointAddress;
      else
        data->interrupt_out_endpoint = endpoint.bEndpointAddress;

      const u32 offset = is_in_endpoint ? 80 : 88;
      endpoint.Swap();
      memory.CopyToEmu(request.buffer_out + offset, &endpoint, sizeof(endpoint));
    }
  }

  return IPCReply(IPC_SUCCESS);
}

bool USB_HIDv5::ShouldAddDevice(const USB::Device& device) const
{
  // XXX: HIDv5 opens /dev/usb/usb with mode 3 (which is likely HID_CLASS),
  //      unlike VEN (which opens it with mode 0xff). But is this really correct?
  constexpr u8 HID_CLASS = 0x03;
  return device.HasClass(HID_CLASS);
}
}  // namespace IOS::HLE
