// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace Common::Debug
{
struct Watch
{
  enum class State : bool
  {
    Enabled = true,
    Disabled = false
  };

  u32 address;
  std::string name;
  State is_enabled;

  Watch(u32 address, std::string name, State is_enabled);
};

class Watches
{
public:
  std::size_t SetWatch(u32 address, std::string name);
  const Watch& GetWatch(std::size_t index) const;
  const std::vector<Watch>& GetWatches() const;
  void UnsetWatch(u32 address);
  void UpdateWatch(std::size_t index, u32 address, std::string name);
  void UpdateWatchAddress(std::size_t index, u32 address);
  void UpdateWatchName(std::size_t index, std::string name);
  void EnableWatch(std::size_t index);
  void DisableWatch(std::size_t index);
  bool HasEnabledWatch(u32 address) const;
  void RemoveWatch(std::size_t index);
  void LoadFromStrings(const std::vector<std::string>& watches);
  std::vector<std::string> SaveToStrings() const;
  void Clear();

private:
  std::vector<Watch> m_watches;
};
}  // namespace Common::Debug
