// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "Common/CommonTypes.h"

namespace File
{
class IOFileByHandle
{
public:
  IOFileByHandle(const std::string& filename);
  ~IOFileByHandle();

  IOFileByHandle(const IOFileByHandle&) = delete;
  IOFileByHandle& operator=(const IOFileByHandle&) = delete;

  bool Write(const u8* data, size_t length);

  bool Rename(const std::string& filename);

  template <typename T>
  bool WriteArray(const T* elements, size_t count)
  {
    return Write(reinterpret_cast<const u8*>(elements), count * sizeof(T));
  }

private:
  struct Impl;
  std::unique_ptr<Impl> m_data;
};

}  // namespace File
