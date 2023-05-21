// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: BSD-3-Clause

#include "SteamHelperCommon/PipeEnd.h"

#include <Windows.h>

namespace Steam
{
void PipeEnd::CloseImpl()
{
  CloseHandle(m_handle);
}

int PipeEnd::ReadImpl(void* buffer, const size_t size)
{
  DWORD bytes_read;
  bool result = ReadFile(m_handle, buffer, static_cast<DWORD>(size), &bytes_read, nullptr);

  return result ? bytes_read : -1;
}

int PipeEnd::WriteImpl(const void* buffer, size_t size)
{
  DWORD bytes_written;
  bool result = WriteFile(m_handle, buffer, static_cast<DWORD>(size), &bytes_written, nullptr);

  return result ? bytes_written : -1;
}
}  // namespace Steam
