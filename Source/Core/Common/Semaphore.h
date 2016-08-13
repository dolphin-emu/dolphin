// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
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

#else  // _WIN32

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
