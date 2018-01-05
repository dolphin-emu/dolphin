// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <unordered_map>

namespace Core
{
// Reader for title database files.
class TitleDatabase final
{
public:
  TitleDatabase();
  ~TitleDatabase();

  enum class TitleType
  {
    Channel,
    Other,
  };

  // Get a user friendly title name for a game ID.
  // This falls back to returning an empty string if none could be found.
  std::string GetTitleName(const std::string& game_id, TitleType = TitleType::Other) const;

  // Get a description for a game ID (title name if available + game ID).
  std::string Describe(const std::string& game_id, TitleType = TitleType::Other) const;

private:
  std::unordered_map<std::string, std::string> m_wii_title_map;
  std::unordered_map<std::string, std::string> m_gc_title_map;
};
}  // namespace Core
