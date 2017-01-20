// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <list>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IPC.h"
#include "Core/IOS/USB/Common.h"

// Forward declare things which we need from libusb header.
// This prevents users of this file from indirectly pulling in libusb.
#if defined(_WIN32)
#define LIBUSB_CALL WINAPI
#else
#define LIBUSB_CALL
#endif
struct libusb_config_descriptor;
struct libusb_device_descriptor;
struct libusb_device_handle;
struct libusb_endpoint_descriptor;
struct libusb_interface_descriptor;
struct libusb_transfer;

namespace IOS
{
namespace HLE
{
#define HID_ID_MASK 0x0000FFFFFFFFFFFF
#define MAX_HID_INTERFACES 1

namespace Device
{
class USB_HIDv4 : public Device
{
public:
  USB_HIDv4(u32 _DeviceID, const std::string& _rDeviceName);

  virtual ~USB_HIDv4();

  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;
  IPCCommandResult IOCtl(const IOCtlRequest& request) override;

private:
  enum
  {
    IOCTL_HID_GET_ATTACHED = 0x00,
    IOCTL_HID_SET_SUSPEND = 0x01,
    IOCTL_HID_CONTROL = 0x02,
    IOCTL_HID_INTERRUPT_IN = 0x03,
    IOCTL_HID_INTERRUPT_OUT = 0x04,
    IOCTL_HID_GET_US_STRING = 0x05,
    IOCTL_HID_OPEN = 0x06,
    IOCTL_HID_SHUTDOWN = 0x07,
    IOCTL_HID_CANCEL_INTERRUPT = 0x08,
  };

  u32 deviceCommandAddress;
  void FillOutDevices(const IOCtlRequest& request);
  int GetAvailableDevNum(u16 idVendor, u16 idProduct, u8 bus, u8 port, u16 check);
  bool ClaimDevice(libusb_device_handle* dev);

  void ConvertDeviceToWii(USB::DeviceDescriptor* dest, const libusb_device_descriptor* src);
  void ConvertConfigToWii(USB::ConfigDescriptor* dest, const libusb_config_descriptor* src);
  void ConvertInterfaceToWii(USB::InterfaceDescriptor* dest,
                             const libusb_interface_descriptor* src);
  void ConvertEndpointToWii(USB::EndpointDescriptor* dest, const libusb_endpoint_descriptor* src);

  static void checkUsbUpdates(USB_HIDv4* hid);
  static void LIBUSB_CALL handleUsbUpdates(libusb_transfer* transfer);

  libusb_device_handle* GetDeviceByDevNum(u32 devNum);
  std::map<u32, libusb_device_handle*> m_open_devices;
  std::mutex m_open_devices_mutex;
  std::mutex m_device_list_reply_mutex;

  std::thread usb_thread;
  bool usb_thread_running;
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
