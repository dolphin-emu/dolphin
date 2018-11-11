// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

class PointerWrap;

namespace UICommon
{
class GameFile;

std::vector<std::string> FindAllGamePaths(const std::vector<std::string>& directories_to_scan,
                                          bool recursive_scan);

class GameFileCache
{
public:
  enum class DeleteOnDisk
  {
    No = 0,
    Yes = 1,
  };

  GameFileCache();  // Uses the default path

  void ForEach(std::function<void(const std::shared_ptr<const GameFile>&)> f) const;

  size_t GetSize() const;
  void Clear(DeleteOnDisk delete_on_disk);

  // Returns nullptr if the file is invalid.
  std::shared_ptr<const GameFile> AddOrGet(const std::string& path, bool* cache_changed);

  // These functions return true if the call modified the cache.
  bool Update(const std::vector<std::string>& all_game_paths,
              std::function<void(const std::shared_ptr<const GameFile>&)> game_added_to_cache = {},
              std::function<void(const std::string&)> game_removed_from_cache = {});
  bool UpdateAdditionalMetadata(
      std::function<void(const std::shared_ptr<const GameFile>&)> game_updated = {});

  bool Load();
  bool Save();

private:
  bool UpdateAdditionalMetadata(std::shared_ptr<GameFile>* game_file);

  bool SyncCacheFile(bool save);
  void DoState(PointerWrap* p, u64 size = 0);

  std::string m_path;
  std::vector<std::shared_ptr<GameFile>> m_cached_files;
};

}  // namespace UICommon
