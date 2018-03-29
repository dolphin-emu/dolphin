// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

class PointerWrap;

namespace Core
{
class TitleDatabase;
}

namespace UICommon
{
class GameFile;

std::vector<std::string> FindAllGamePaths(const std::vector<std::string>& directories_to_scan,
                                          bool recursive_scan);

class GameFileCache
{
public:
  void ForEach(std::function<void(const std::shared_ptr<const GameFile>&)> f) const;

  void Clear();

  // Returns nullptr if the file is invalid.
  std::shared_ptr<const GameFile> AddOrGet(const std::string& path, bool* cache_changed,
                                           const Core::TitleDatabase& title_database);

  // These functions return true if the call modified the cache.
  bool Update(const std::vector<std::string>& all_game_paths);
  bool UpdateAdditionalMetadata(const Core::TitleDatabase& title_database);

  bool Load();
  bool Save();

private:
  bool UpdateAdditionalMetadata(std::shared_ptr<GameFile>* game_file,
                                const Core::TitleDatabase& title_database);

  bool SyncCacheFile(bool save);
  void DoState(PointerWrap* p, u64 size = 0);

  std::vector<std::shared_ptr<GameFile>> m_cached_files;
};

}  // namespace UICommon
