// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <memory>

#include "Common/CommonTypes.h"

struct libusb_config_descriptor;
struct libusb_context;
struct libusb_device;
struct libusb_device_handle;
struct libusb_transfer;

namespace LibusbUtils
{
template <typename T>
using UniquePtr = std::unique_ptr<T, void (*)(T*)>;

// Return false to stop iterating the device list.
using GetDeviceListCallback = std::function<bool(libusb_device* device)>;

using TransferCallback = std::function<void()>;

class Context
{
public:
  Context();
  ~Context();

  operator libusb_context*() const;
  bool IsValid() const;

  // Only valid if the context is valid.
  bool GetDeviceList(GetDeviceListCallback callback);

  // Submit a transfer asynchronously.
  // The callback may sometimes be invoked on the same thread.
  // Returns a libusb error code.
  int SubmitTransfer(libusb_transfer* transfer, TransferCallback callback);

  // Submit a transfer and wait for it to complete before returning.
  // Returns the transferred length (or a negative error code).
  s64 SubmitTransferSync(libusb_transfer* transfer);

  // Submit a control transfer synchronously.
  s64 ControlTransfer(libusb_device_handle* handle, u8 bmRequestType, u8 bRequest, u16 wValue,
                      u16 wIndex, u8* data, u16 wLength, u32 timeout);
  // Submit an interrupt transfer synchronously.
  s64 InterruptTransfer(libusb_device_handle* handle, u8 endpoint, u8* data, int length,
                        u32 timeout);
  // Submit a bulk transfer synchronously.
  s64 BulkTransfer(libusb_device_handle* handle, u8 endpoint, u8* data, int length, u32 timeout);

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

// Use this to get a libusb context. Do *not* use any other context
// because some libusb backends such as UsbDk only work properly with a single context.
// Additionally, device lists must be retrieved using GetDeviceList for thread safety reasons.
Context& GetContext();

using Transfer = UniquePtr<libusb_transfer>;
Transfer MakeTransfer(int num_isoc_packets = 0);

using ConfigDescriptor = UniquePtr<libusb_config_descriptor>;
ConfigDescriptor MakeConfigDescriptor(libusb_device* device, u8 config_num = 0);
}  // namespace LibusbUtils
