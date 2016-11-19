// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <unistd.h>

#include "Common/FileUtil.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/MemoryWatcher.h"

static std::unique_ptr<MemoryWatcher> s_memory_watcher;
static CoreTiming::EventType* s_event;
static const int MW_RATE = 600;  // Steps per second

static void MWCallback(u64 userdata, s64 cyclesLate)
{
  s_memory_watcher->Step();
  CoreTiming::ScheduleEvent(SystemTimers::GetTicksPerSecond() / MW_RATE - cyclesLate, s_event);
}

void MemoryWatcher::Init()
{
  s_memory_watcher = std::make_unique<MemoryWatcher>();
  s_event = CoreTiming::RegisterEvent("MemoryWatcher", MWCallback);
  CoreTiming::ScheduleEvent(0, s_event);
}

void MemoryWatcher::Shutdown()
{
  CoreTiming::RemoveEvent(s_event);
  s_memory_watcher.reset();
}

MemoryWatcher::MemoryWatcher()
{
  m_running = false;
  if (!LoadAddresses(File::GetUserPath(F_MEMORYWATCHERLOCATIONS_IDX)))
    return;
  if (!OpenSocket(File::GetUserPath(F_MEMORYWATCHERSOCKET_IDX)))
    return;
  m_running = true;
}

MemoryWatcher::~MemoryWatcher()
{
  if (!m_running)
    return;

  m_running = false;
  close(m_fd);
}

bool MemoryWatcher::LoadAddresses(const std::string& path)
{
  std::ifstream locations(path);
  if (!locations)
    return false;

  std::string line;
  while (std::getline(locations, line))
    ParseLine(line);

  return m_values.size() > 0;
}

void MemoryWatcher::ParseLine(const std::string& line)
{
  m_values[line] = 0;
  m_addresses[line] = std::vector<u32>();

  std::stringstream offsets(line);
  offsets >> std::hex;
  u32 offset;
  while (offsets >> offset)
    m_addresses[line].push_back(offset);
}

bool MemoryWatcher::OpenSocket(const std::string& path)
{
  memset(&m_addr, 0, sizeof(m_addr));
  m_addr.sun_family = AF_UNIX;
  strncpy(m_addr.sun_path, path.c_str(), sizeof(m_addr.sun_path) - 1);

  m_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
  return m_fd >= 0;
}

u32 MemoryWatcher::ChasePointer(const std::string& line)
{
  u32 value = 0;
  for (u32 offset : m_addresses[line])
    value = Memory::Read_U32(value + offset);
  return value;
}

std::string MemoryWatcher::ComposeMessage(const std::string& line, u32 value)
{
  std::stringstream message_stream;
  message_stream << line << '\n' << std::hex << value;
  return message_stream.str();
}

void MemoryWatcher::Step()
{
  if (!m_running)
    return;

  for (auto& entry : m_values)
  {
    std::string address = entry.first;
    u32& current_value = entry.second;

    u32 new_value = ChasePointer(address);
    if (new_value != current_value)
    {
      // Update the value
      current_value = new_value;
      std::string message = ComposeMessage(address, new_value);
      sendto(m_fd, message.c_str(), message.size() + 1, 0, reinterpret_cast<sockaddr*>(&m_addr),
             sizeof(m_addr));
    }
  }
}
