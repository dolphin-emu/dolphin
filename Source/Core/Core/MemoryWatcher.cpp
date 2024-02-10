// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/MemoryWatcher.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>

#include "Common/FileUtil.h"
#include "Core/HW/SystemTimers.h"
#include "Core/PowerPC/MMU.h"

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
  std::ifstream locations;
  File::OpenFStream(locations, path, std::ios_base::in);
  if (!locations)
    return false;

  std::string line;
  while (std::getline(locations, line))
    ParseLine(line);

  return !m_values.empty();
}

void MemoryWatcher::ParseLine(const std::string& line)
{
  m_values[line] = 0;
  m_addresses[line] = std::vector<u32>();

  std::istringstream offsets(line);
  offsets >> std::hex;
  u32 offset;
  while (offsets >> offset)
    m_addresses[line].push_back(offset);
}

bool MemoryWatcher::OpenSocket(const std::string& path)
{
  m_addr.sun_family = AF_UNIX;
  strncpy(m_addr.sun_path, path.c_str(), sizeof(m_addr.sun_path) - 1);

  m_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
  return m_fd >= 0;
}

u32 MemoryWatcher::ChasePointer(const Core::CPUThreadGuard& guard, const std::string& line)
{
  u32 value = 0;
  for (u32 offset : m_addresses[line])
  {
    value = PowerPC::MMU::HostRead_U32(guard, value + offset);
    if (!PowerPC::MMU::HostIsRAMAddress(guard, value))
      break;
  }
  return value;
}

std::string MemoryWatcher::ComposeMessages(const Core::CPUThreadGuard& guard)
{
  std::ostringstream message_stream;
  message_stream << std::hex;

  for (auto& entry : m_values)
  {
    std::string address = entry.first;
    u32& current_value = entry.second;

    u32 new_value = ChasePointer(guard, address);
    if (new_value != current_value)
    {
      // Update the value
      current_value = new_value;
      message_stream << address << '\n' << new_value << '\n';
    }
  }

  return message_stream.str();
}

void MemoryWatcher::Step(const Core::CPUThreadGuard& guard)
{
  if (!m_running)
    return;

  std::string message = ComposeMessages(guard);
  sendto(m_fd, message.c_str(), message.size() + 1, 0, reinterpret_cast<sockaddr*>(&m_addr),
         sizeof(m_addr));
}
