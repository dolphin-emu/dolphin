// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <span>
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

  using ForEachFn = std::function<void(const std::shared_ptr<const GameFile>&)>;
  using GameAddedToCacheFn = std::function<void(const std::shared_ptr<const GameFile>&)>;
  using GameRemovedFromCacheFn = std::function<void(const std::string&)>;
  using GameUpdatedFn = std::function<void(const std::shared_ptr<const GameFile>&)>;

  GameFileCache();

  void ForEach(const ForEachFn& f) const;

  size_t GetSize() const;
  void Clear(DeleteOnDisk delete_on_disk);

  // Returns nullptr if the file is invalid.
  std::shared_ptr<const GameFile> AddOrGet(const std::string& path, bool* cache_changed);

  // These functions return true if the call modified the cache.
  bool Update(std::span<const std::string> all_game_paths,
              const GameAddedToCacheFn& game_added_to_cache = {},
              const GameRemovedFromCacheFn& game_removed_from_cache = {},
              const std::atomic_bool& processing_halted = false);
  bool UpdateAdditionalMetadata(const GameUpdatedFn& game_updated = {},
                                const std::atomic_bool& processing_halted = false);

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
