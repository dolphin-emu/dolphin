// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: BSD-3-Clause

#include "SteamHelperCommon/PipeEnd.h"

#include <unistd.h>

namespace Steam
{
void PipeEnd::CloseImpl()
{
  close(m_handle);
}

int PipeEnd::ReadImpl(void* buffer, size_t size)
{
  return read(m_handle, buffer, size);
}

int PipeEnd::WriteImpl(const void* buffer, size_t size)
{
  return write(m_handle, buffer, size);
}
}  // namespace Steam
