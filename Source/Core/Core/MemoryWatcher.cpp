// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iterator>
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
  std::ifstream locations;
  File::OpenFStream(locations, path, std::ios_base::in);
  if (!locations)
    return false;

  std::string line;
  while (std::getline(locations, line))
    ParseLine(line);

  return m_values.size() > 0;
}

void MemoryWatcher::ParseLine(const std::string& line)
{
  // Ignore lines that start with a #
  if (line.front() == '#')
  {
    return;
  }

  if (std::count(line.begin(), line.end(), ' ') == 3)
  {
    // This is a linked list line
    //  First, split the string into its parts
    std::istringstream iss(line);
    std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},
                                    std::istream_iterator<std::string>{}};

    m_linked_lists[tokens[0]] = LinkedList();

    std::stringstream ss;
    u32 converted;
    ss << std::hex << tokens[1];
    ss >> converted;
    m_linked_lists[tokens[0]].m_pointer_offset = converted;

    std::stringstream ss1;
    ss1 << std::hex << tokens[2];
    ss1 >> converted;
    m_linked_lists[tokens[0]].m_data_pointer_offset = converted;

    std::stringstream ss2;
    ss2 << std::hex << tokens[3];
    ss2 >> converted;
    m_linked_lists[tokens[0]].m_data_struct_len = converted;
  }
  else
  {
    // This is a uint32 line
    m_values[line] = 0;
    m_addresses[line] = std::vector<u32>();

    std::stringstream offsets(line);
    offsets >> std::hex;
    u32 offset;
    while (offsets >> offset)
      m_addresses[line].push_back(offset);
  }
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
  {
    value = Common::swap32(value);
    if (!Memory::CopyFromEmu(&value, value + offset, sizeof(value), false))
      return 0;
  }
  return value;
}

bool MemoryWatcher::ChaseLinkedList(const std::string& address, LinkedList& llist)
{
  u32 pointer;
  std::stringstream ss;
  ss << std::hex << address;
  ss >> pointer;

  // Follow the first pointer over
  pointer = Memory::Read_U32(pointer);
  LinkedList::ListData data;

  bool changed = false;
  size_t i = 0;
  while (true)
  {
    // Quit on NULL pointer
    if (pointer == 0)
    {
      if (i != llist.m_data.size())
      {
        changed = true;
      }
      break;
    }

    // Grab the data pointer from the current element
    u32 data_pointer = 0;
    if (!Memory::CopyFromEmu(&data_pointer, pointer + llist.m_data_pointer_offset,
                             sizeof(data_pointer), false))
    {
      break;
    }

    data_pointer = Common::swap32(data_pointer);

    // Read a blob at the data pointer address. This is the actual data we care about
    // Read the next element in the list
    LinkedList::Blob chunk(llist.m_data_struct_len);
    if (!Memory::CopyFromEmu(chunk.data(), data_pointer, llist.m_data_struct_len, false))
    {
      // If we get an error here, interpret it as a NULL pointer and just quit
      // When we first boot up, this memory will be uninitialized and contain
      // garbage. We have to gracefully handle that case.
      break;
    }

    if (i < llist.m_data.size())
    {
      if (chunk != llist.m_data[i])
      {
        changed = true;
      }
    }
    else
    {
      // This list has a different number of entries, so it's definitely changed
      changed = true;
    }

    data.push_back(std::move(chunk));

    // Follow the pointer to the location of the next object
    if (!Memory::CopyFromEmu(&pointer, pointer + llist.m_pointer_offset, sizeof(data_pointer)),
        false)
    {
      break;
    }
    pointer = Common::swap32(pointer);
    i++;
  }

  if (changed)
  {
    llist.m_data = std::move(data);
  }
  return changed;
}

std::string MemoryWatcher::ComposeMessage(const std::string& line, u32 value)
{
  std::stringstream message_stream;
  message_stream << line << '\n' << std::hex << value;
  return message_stream.str();
}

std::string MemoryWatcher::ComposeMessage(const std::string& line, const LinkedList::Blob& data)
{
  std::string message = line + '\n';

  std::stringstream ss;
  for (auto entry : data)
  {
    ss << std::setw(2) << std::setfill('0') << std::hex << static_cast<unsigned int>(entry);
  }
  return message + ss.str();
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

  // Process linked lists
  for (auto& entry : m_linked_lists)
  {
    std::string address = entry.first;
    LinkedList& llist = entry.second;
    // Update the linked list entry for this pointer
    if (ChaseLinkedList(address, llist))
    {
      // If the contents changed, then let the listener know
      if (m_linked_lists[address].m_data.empty())
      {
        std::string message = address + "\n\n";
        sendto(m_fd, message.c_str(), message.size() + 1, 0, reinterpret_cast<sockaddr*>(&m_addr),
               sizeof(m_addr));
      }

      for (const auto& datum : m_linked_lists[address].m_data)
      {
        std::string message = ComposeMessage(address, datum);
        sendto(m_fd, message.c_str(), message.size() + 1, 0, reinterpret_cast<sockaddr*>(&m_addr),
               sizeof(m_addr));
      }
    }
  }
}
