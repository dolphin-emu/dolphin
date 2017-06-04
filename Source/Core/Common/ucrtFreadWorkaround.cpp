// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#if defined(_WIN32)

#include <string>
#include <vector>
#include <Windows.h>
#include "CommonTypes.h"
#include "StringUtil.h"
#include "MsgHandler.h"

#pragma comment(lib, "mincore.lib")

// Old versions of ucrtbase implements caching between fseek/fread
// such that some reads return incorrect data. This causes noticable bugs
// in dolphin since we use these APIs for reading game images. Dolphin
// used to patch ucrtbase to hack around the issue, but now we just advise the
// user to install the fixed version.

static const u16 fixed_build = 0xffff;// 10548;

struct Version {
  u16 major;
  u16 minor;
  u16 build;
  u16 qfe;
  Version &operator=(u64 &&rhs) {
    major = static_cast<u16>(rhs >> 48);
    minor = static_cast<u16>(rhs >> 32);
    build = static_cast<u16>(rhs >> 16);
    qfe = static_cast<u16>(rhs);
    return *this;
  }
};

static bool GetModulePath(const wchar_t *name, std::wstring *path) {
  auto module = GetModuleHandleW(name);
  if (module == nullptr)
  {
    return false;
  }
  DWORD path_len = MAX_PATH;
retry:
  path->resize(path_len);
  path_len = GetModuleFileNameW(module, const_cast<wchar_t *>(path->data()),
    static_cast<DWORD>(path->size()));
  if (!path_len) {
    return false;
  }
  auto error = GetLastError();
  if (error == ERROR_SUCCESS) {
    return true;
  }
  if (error == ERROR_INSUFFICIENT_BUFFER) {
    goto retry;
  }
  return false;
}

static bool GetModuleVersion(const wchar_t *name, Version *version) {
  std::wstring path;
  if (!GetModulePath(name, &path)) {
    return false;
  }

  bool rv = false;
  DWORD handle;
  DWORD data_len = GetFileVersionInfoSizeW(path.c_str(), &handle);
  if (!data_len) {
    return false;
  }
  std::vector<u8> block(data_len);
  if (!GetFileVersionInfoW(path.c_str(), handle, data_len, block.data())) {
    return false;
  }
  void *buf;
  UINT buf_len;
  if (!VerQueryValueW(block.data(), LR"(\)", &buf, &buf_len)) {
    return false;
  }
  auto info = static_cast<VS_FIXEDFILEINFO *>(buf);
  *version = (static_cast<u64>(info->dwFileVersionMS) << 32) | info->dwFileVersionLS;
  return true;
}

int __cdecl EnableucrtFreadWorkaround()
{
  const wchar_t *const module_names[] = { L"ucrtbase.dll", L"ucrtbased.dll" };
  for (const auto &name : module_names) {
    Version version;
    if (GetModuleVersion(name, &version)) {
      if (version.build < fixed_build) {
        auto msg = StringFromFormat("You are running %S version %d.%d.%d.%d.\n"
          "An important fix affecting dolphin was introduced in build %d.\n"
          "You can use dolphin, but there will be known bugs.\n"
          "Please update this file by installing the latest UCRT:\n"
          "<put_url_here>\n",
          name, version.major, version.minor, version.build,
          version.qfe, fixed_build);
        // Use MessageBox for maximal user annoyance
        MessageBoxA(nullptr, msg.c_str(), "WARNING: BUGGY UCRT VERSION",
          MB_ICONEXCLAMATION);
      }
    }
  }

  return 0;
}

// Create a segment which is recognized by the linker to be part of the CRT
// initialization. XI* = C startup, XC* = C++ startup. "A" placement is reserved
// for system use. C startup is before C++.
// Use last C++ slot in hopes that makes using C++ from this code safe.
#pragma section(".CRT$XCZ", read)

// Place a symbol in the special segment, make it have C linkage so that
// referencing it doesn't require ugly decorated names.
// Use /include:EnableucrtFreadWorkaround linker flag to enable this.
extern "C" {
  __declspec(allocate(".CRT$XCZ")) decltype(&EnableucrtFreadWorkaround)
    ucrtFreadWorkaround = EnableucrtFreadWorkaround;
};

#endif
