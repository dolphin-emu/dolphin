// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#ifdef __MINGW32__
#include <windows.h>
#include <objbase.h>
#endif

#include "Common/CommonTypes.h"

struct WindowSystemInfo;

namespace UICommon
{
void Init();
void Shutdown();

void InitControllers(const WindowSystemInfo& wsi);
void ShutdownControllers();

void InhibitScreenSaver(bool inhibit);

// Calls std::locale::global, selecting a fallback locale if the
// requested locale isn't available
void SetLocale(std::string locale_name);

void CreateDirectories();
void SetUserDirectory(std::string custom_path);

bool TriggerSTMPowerEvent();

// Return a pretty file size string from byte count.
// e.g. 1134278 -> "1.08 MiB"
std::string FormatSize(u64 bytes, int decimals = 2);
}  // namespace UICommon
#ifdef __MINGW32__
namespace wil {
  struct unique_cotaskmem_string
  {
    PWSTR ptr{nullptr};

    unique_cotaskmem_string() noexcept = default;

    ~unique_cotaskmem_string()
    {
      if (ptr)
      {
        CoTaskMemFree(ptr);
      }
    }

    unique_cotaskmem_string(const unique_cotaskmem_string&) = delete;
    unique_cotaskmem_string& operator=(const unique_cotaskmem_string&) = delete;

    unique_cotaskmem_string(unique_cotaskmem_string&& other) noexcept : ptr(other.ptr)
    {
      other.ptr = nullptr;
    }

    unique_cotaskmem_string& operator=(unique_cotaskmem_string&& other) noexcept
    {
      if (this != &other)
      {
        if (ptr)
        {
          CoTaskMemFree(ptr);
        }
        ptr = other.ptr;
        other.ptr = nullptr;
      }
      return *this;
    }

    PWSTR* put() { return &ptr; }
    PWSTR get() const { return ptr; }
  };

  struct unique_hkey
  {
    HKEY h{nullptr};

    unique_hkey() noexcept = default;

    ~unique_hkey()
    {
      if (h)
      {
        RegCloseKey(h);
      }
    }

    unique_hkey(const unique_hkey&) = delete;
    unique_hkey& operator=(const unique_hkey&) = delete;

    unique_hkey(unique_hkey&& other) noexcept : h(other.h)
    {
      other.h = nullptr;
    }

    unique_hkey& operator=(unique_hkey&& other) noexcept
    {
      if (this != &other)
      {
        if (h)
        {
          RegCloseKey(h);
        }
        h = other.h;
        other.h = nullptr;
      }
      return *this;
    }

    HKEY* put() { return &h; }
    HKEY get() const { return h; }
  };
}  // namespace wil
#endif
