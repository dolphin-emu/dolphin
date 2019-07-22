// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Debug/Watches.h"

#include <algorithm>
#include <sstream>

namespace Common::Debug
{
Watch::Watch(u32 address_, std::string name_, State is_enabled_)
    : address(address_), name(std::move(name_)), is_enabled(is_enabled_)
{
}

std::size_t Watches::SetWatch(u32 address, std::string name)
{
  const std::size_t size = m_watches.size();
  for (std::size_t index = 0; index < size; index++)
  {
    if (m_watches.at(index).address == address)
    {
      UpdateWatch(index, address, name);
      return index;
    }
  }
  m_watches.emplace_back(address, std::move(name), Watch::State::Enabled);
  return size;
}

const Watch& Watches::GetWatch(std::size_t index) const
{
  return m_watches.at(index);
}

const std::vector<Watch>& Watches::GetWatches() const
{
  return m_watches;
}

void Watches::UnsetWatch(u32 address)
{
  m_watches.erase(std::remove_if(m_watches.begin(), m_watches.end(),
                                 [address](const auto& watch) { return watch.address == address; }),
                  m_watches.end());
}

void Watches::UpdateWatch(std::size_t index, u32 address, std::string name)
{
  m_watches[index].address = address;
  m_watches[index].name = std::move(name);
}

void Watches::UpdateWatchAddress(std::size_t index, u32 address)
{
  m_watches[index].address = address;
}

void Watches::UpdateWatchName(std::size_t index, std::string name)
{
  m_watches[index].name = std::move(name);
}

void Watches::EnableWatch(std::size_t index)
{
  m_watches[index].is_enabled = Watch::State::Enabled;
}

void Watches::DisableWatch(std::size_t index)
{
  m_watches[index].is_enabled = Watch::State::Disabled;
}

bool Watches::HasEnabledWatch(u32 address) const
{
  return std::any_of(m_watches.begin(), m_watches.end(), [address](const auto& watch) {
    return watch.address == address && watch.is_enabled == Watch::State::Enabled;
  });
}

void Watches::RemoveWatch(std::size_t index)
{
  m_watches.erase(m_watches.begin() + index);
}

void Watches::LoadFromStrings(const std::vector<std::string>& watches)
{
  for (const std::string& watch : watches)
  {
    std::stringstream ss;
    u32 address;
    std::string name;
    ss << std::hex << watch;
    ss >> address;
    ss >> std::ws;
    std::getline(ss, name);
    SetWatch(address, name);
  }
}

std::vector<std::string> Watches::SaveToStrings() const
{
  std::vector<std::string> watches;
  for (const auto& watch : m_watches)
  {
    std::stringstream ss;
    ss << std::hex << watch.address << " " << watch.name;
    watches.push_back(ss.str());
  }
  return watches;
}

void Watches::Clear()
{
  m_watches.clear();
}
}  // namespace Common::Debug
