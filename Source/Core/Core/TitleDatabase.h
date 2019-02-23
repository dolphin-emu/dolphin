// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <unordered_map>

#include "Common/CommonTypes.h"

namespace Core
{
// Reader for title database files.
class TitleDatabase final
{
public:
  TitleDatabase();
  ~TitleDatabase();

  // Get a user friendly title name for a GameTDB ID.
  // This falls back to returning an empty string if none could be found.
  const std::string& GetTitleName(const std::string& gametdb_id) const;

  // Same as above, but takes a title ID instead of a GameTDB ID, and only works for channels.
  const std::string& GetChannelName(u64 title_id) const;

  // Get a description for a GameTDB ID (title name if available + GameTDB ID).
  std::string Describe(const std::string& gametdb_id) const;

private:
  std::unordered_map<std::string, std::string> m_wii_title_map;
  std::unordered_map<std::string, std::string> m_gc_title_map;
};
}  // namespace Core
