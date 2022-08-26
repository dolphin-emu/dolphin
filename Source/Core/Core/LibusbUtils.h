// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <fmt/format.h>
#include <functional>
#include <memory>
#include <utility>

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
  int GetDeviceList(GetDeviceListCallback callback) const;

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

using ConfigDescriptor = UniquePtr<libusb_config_descriptor>;
std::pair<int, ConfigDescriptor> MakeConfigDescriptor(libusb_device* device, u8 config_num = 0);

// Wrapper for libusb_error to be used with fmt.  Note that we can't create a fmt::formatter
// directly for libusb_error as it is a plain enum and most libusb functions actually return an
// int instead of a libusb_error.
struct ErrorWrap
{
  constexpr explicit ErrorWrap(int error) : m_error(error) {}
  const int m_error;

  const char* GetStrError() const;
  const char* GetName() const;
};
}  // namespace LibusbUtils

template <>
struct fmt::formatter<LibusbUtils::ErrorWrap>
{
  constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const LibusbUtils::ErrorWrap& wrap, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "{} ({}: {})", wrap.GetStrError(), wrap.m_error,
                          wrap.GetName());
  }
};
