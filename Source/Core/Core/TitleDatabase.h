// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <unordered_map>

#include "Common/CommonTypes.h"
#include "Common/Lazy.h"

namespace DiscIO
{
enum class Language;
}

namespace Core
{
// Reader for title database files.
class TitleDatabase final
{
public:
  TitleDatabase();
  TitleDatabase(const std::string& language);
  ~TitleDatabase();

  // Get a user friendly title name for a GameTDB ID.
  // This falls back to returning an empty string if none could be found.
  const std::string& GetTitleName(const std::string& gametdb_id, DiscIO::Language language) const;

  // Same as above, but takes a title ID instead of a GameTDB ID, and only works for channels.
  const std::string& GetChannelName(u64 title_id, DiscIO::Language language) const;

  // Get a description for a GameTDB ID (title name if available + GameTDB ID).
  std::string Describe(const std::string& gametdb_id, DiscIO::Language language) const;

private:
  void AddLazyMap(DiscIO::Language language, const std::string& language_code);

  std::unordered_map<DiscIO::Language, Common::Lazy<std::unordered_map<std::string, std::string>>>
      m_title_maps;
  std::unordered_map<std::string, std::string> m_base_map;
  std::unordered_map<std::string, std::string> m_user_title_map;
};
}  // namespace Core
