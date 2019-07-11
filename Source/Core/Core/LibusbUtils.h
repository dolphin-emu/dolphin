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

namespace LibusbUtils
{
template <typename T>
using UniquePtr = std::unique_ptr<T, void (*)(T*)>;

// Return false to stop iterating the device list.
using GetDeviceListCallback = std::function<bool(libusb_device* device)>;

class Context
{
public:
  Context();
  ~Context();

  operator libusb_context*() const;
  bool IsValid() const;

  // Only valid if the context is valid.
  bool GetDeviceList(GetDeviceListCallback callback);

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

using ConfigDescriptor = UniquePtr<libusb_config_descriptor>;
ConfigDescriptor MakeConfigDescriptor(libusb_device* device, u8 config_num = 0);
}  // namespace LibusbUtils
