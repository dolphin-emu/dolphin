// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef _WIN32

#include <Windows.h>

namespace Common
{
class Semaphore
{
public:
  Semaphore(int initial_count, int maximum_count)
  {
    m_handle = CreateSemaphoreA(nullptr, initial_count, maximum_count, nullptr);
  }

  ~Semaphore() { CloseHandle(m_handle); }
  void Wait() { WaitForSingleObject(m_handle, INFINITE); }
  void Post() { ReleaseSemaphore(m_handle, 1, nullptr); }

private:
  HANDLE m_handle;
};
}  // namespace Common

#elif defined(__APPLE__)

#include <dispatch/dispatch.h>

namespace Common
{
class Semaphore
{
public:
  Semaphore(int initial_count, int maximum_count)
  {
    m_handle = dispatch_semaphore_create(0);
    for (int i = 0; i < initial_count; i++)
      dispatch_semaphore_signal(m_handle);
  }
  ~Semaphore() { dispatch_release(m_handle); }
  void Wait() { dispatch_semaphore_wait(m_handle, DISPATCH_TIME_FOREVER); }
  void Post() { dispatch_semaphore_signal(m_handle); }

private:
  dispatch_semaphore_t m_handle;
};
}  // namespace Common

#else

#include <semaphore.h>

namespace Common
{
class Semaphore
{
public:
  Semaphore(int initial_count, int maximum_count) { sem_init(&m_handle, 0, initial_count); }
  ~Semaphore() { sem_destroy(&m_handle); }
  void Wait() { sem_wait(&m_handle); }
  void Post() { sem_post(&m_handle); }

private:
  sem_t m_handle;
};
}  // namespace Common

#endif  // _WIN32
