// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef _WIN32

#include <iterator>
#include <memory>
#include <optional>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <SetupAPI.h>
#include <cfgmgr32.h>
#include <devpropdef.h>

namespace Common
{
std::optional<std::wstring> GetDevNodeStringProperty(DEVINST device,
                                                     const DEVPROPKEY* requested_property);

std::optional<std::wstring> GetDeviceInterfaceStringProperty(LPCWSTR iface,
                                                             const DEVPROPKEY* requested_property);

// Allows iterating null-separated/terminated string lists returned by the Windows API.
template <typename T>
struct NullTerminatedStringList
{
  class Iterator
  {
    friend NullTerminatedStringList;

  public:
    constexpr T* operator*() { return m_ptr; }

    constexpr Iterator& operator++()
    {
      m_ptr += std::basic_string_view(m_ptr).size() + 1;
      return *this;
    }

    constexpr Iterator operator++(int)
    {
      const auto result{*this};
      ++*this;
      return result;
    }

    constexpr bool operator==(std::default_sentinel_t) const { return *m_ptr == T{}; }

  private:
    constexpr Iterator(T* ptr) : m_ptr{ptr} {}

    T* m_ptr;
  };

  constexpr auto begin() const { return Iterator{list.get()}; }
  constexpr auto end() const { return std::default_sentinel; }

  std::unique_ptr<T[]> list;
};

NullTerminatedStringList<WCHAR> GetDeviceInterfaceList(LPGUID iface_class_guid, DEVINSTID device_id,
                                                       ULONG flags);

}  // namespace Common

#endif
