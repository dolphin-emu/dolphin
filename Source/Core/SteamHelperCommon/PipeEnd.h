// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <mutex>

#ifdef _WIN32
#include <Windows.h>
typedef HANDLE PipeHandle;
#define INVALID_PIPE_HANDLE INVALID_HANDLE_VALUE
#else
typedef int PipeHandle;
#define INVALID_PIPE_HANDLE -1
#endif

namespace Steam
{
class PipeEnd
{
public:
  PipeEnd(PipeHandle handle) : m_handle(handle) {}
  ~PipeEnd() { Close(); }

  void Close();
  int Read(void* buffer, const size_t bufSize);
  int Write(const void* buffer, size_t size);

  // These call their respective platform-specific functions.
  void CloseImpl();
  int ReadImpl(void* buffer, const size_t bufSize);
  int WriteImpl(const void* buffer, size_t size);

private:
  PipeHandle m_handle;

  std::mutex m_mutex;
  bool m_is_open = true;
};
}  // namespace Steam
