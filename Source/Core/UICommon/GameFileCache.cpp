// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/GameFileCache.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"

#include "DiscIO/DirectoryBlob.h"

#include "UICommon/GameFile.h"

namespace UICommon
{
static constexpr u32 CACHE_REVISION = 25;  // Last changed in PR 12702

std::vector<std::string> FindAllGamePaths(const std::vector<std::string>& directories_to_scan,
                                          bool recursive_scan)
{
  static const std::vector<std::string> search_extensions = {
      ".gcm", ".tgc", ".iso", ".ciso", ".gcz", ".wbfs", ".wia",
      ".rvz", ".nfs", ".wad", ".dol",  ".elf", ".json"};

  // TODO: We could process paths iteratively as they are found
  return Common::DoFileSearch(directories_to_scan, search_extensions, recursive_scan);
}

GameFileCache::GameFileCache() : m_path(File::GetUserPath(D_CACHE_IDX) + "gamelist.cache")
{
}

void GameFileCache::ForEach(const ForEachFn& f) const
{
  for (const std::shared_ptr<GameFile>& item : m_cached_files)
    f(item);
}

size_t GameFileCache::GetSize() const
{
  return m_cached_files.size();
}

void GameFileCache::Clear(DeleteOnDisk delete_on_disk)
{
  if (delete_on_disk != DeleteOnDisk::No)
    File::Delete(m_path);

  m_cached_files.clear();
}

std::shared_ptr<const GameFile> GameFileCache::AddOrGet(const std::string& path,
                                                        bool* cache_changed)
{
  auto it = std::find_if(
      m_cached_files.begin(), m_cached_files.end(),
      [&path](const std::shared_ptr<GameFile>& file) { return file->GetFilePath() == path; });
  const bool found = it != m_cached_files.cend();
  if (!found)
  {
    std::shared_ptr<UICommon::GameFile> game = std::make_shared<GameFile>(path);
    if (!game->IsValid())
      return nullptr;
    m_cached_files.emplace_back(std::move(game));
  }
  std::shared_ptr<GameFile>& result = found ? *it : m_cached_files.back();
  if (UpdateAdditionalMetadata(&result) || !found)
    *cache_changed = true;

  return result;
}

bool GameFileCache::Update(std::span<const std::string> all_game_paths,
                           const GameAddedToCacheFn& game_added_to_cache,
                           const GameRemovedFromCacheFn& game_removed_from_cache,
                           const std::atomic_bool& processing_halted)
{
  // Copy game paths into a set, except ones that match DiscIO::ShouldHideFromGameList.
  // TODO: Prevent DoFileSearch from looking inside /files/ directories of DirectoryBlobs at all?
  // TODO: Make DoFileSearch support filter predicates so we don't have remove things afterwards?
  std::unordered_set<std::string> game_paths;
  game_paths.reserve(all_game_paths.size());
  for (const std::string& path : all_game_paths)
  {
    if (!DiscIO::ShouldHideFromGameList(path))
      game_paths.insert(path);
  }

  bool cache_changed = false;

  // Delete paths that aren't in game_paths from m_cached_files,
  // while simultaneously deleting paths that are in m_cached_files from game_paths.
  // For the sake of speed, we don't care about maintaining the order of m_cached_files.
  {
    auto it = m_cached_files.begin();
    auto end = m_cached_files.end();
    while (it != end)
    {
      if (processing_halted)
        break;

      if (game_paths.erase((*it)->GetFilePath()))
      {
        ++it;
      }
      else
      {
        if (game_removed_from_cache)
          game_removed_from_cache((*it)->GetFilePath());

        cache_changed = true;
        --end;
        *it = std::move(*end);
      }
    }
    m_cached_files.erase(it, m_cached_files.end());
  }

  // Now that the previous loop has run, game_paths only contains paths that
  // aren't in m_cached_files, so we simply add all of them to m_cached_files.
  for (const std::string& path : game_paths)
  {
    if (processing_halted)
      break;

    auto file = std::make_shared<GameFile>(path);
    if (file->IsValid())
    {
      if (game_added_to_cache)
        game_added_to_cache(file);

      cache_changed = true;
      m_cached_files.push_back(std::move(file));
    }
  }

  return cache_changed;
}

bool GameFileCache::UpdateAdditionalMetadata(const GameUpdatedFn& game_updated,
                                             const std::atomic_bool& processing_halted)
{
  bool cache_changed = false;

  for (std::shared_ptr<GameFile>& file : m_cached_files)
  {
    if (processing_halted)
      break;

    const bool updated = UpdateAdditionalMetadata(&file);
    cache_changed |= updated;
    if (game_updated && updated)
      game_updated(file);
  }

  return cache_changed;
}

bool GameFileCache::UpdateAdditionalMetadata(std::shared_ptr<GameFile>* game_file)
{
  const bool xml_metadata_changed = (*game_file)->XMLMetadataChanged();
  const bool wii_banner_changed = (*game_file)->WiiBannerChanged();
  const bool custom_banner_changed = (*game_file)->CustomBannerChanged();

  (*game_file)->DownloadDefaultCover();

  const bool default_cover_changed = (*game_file)->DefaultCoverChanged();
  const bool custom_cover_changed = (*game_file)->CustomCoverChanged();

  if (!xml_metadata_changed && !wii_banner_changed && !custom_banner_changed &&
      !default_cover_changed && !custom_cover_changed)
  {
    return false;
  }

  // If a cached file needs an update, apply the updates to a copy and delete the original.
  // This makes the usage of cached files in other threads safe.

  std::shared_ptr<GameFile> copy = std::make_shared<GameFile>(**game_file);
  if (xml_metadata_changed)
    copy->XMLMetadataCommit();
  if (wii_banner_changed)
    copy->WiiBannerCommit();
  if (custom_banner_changed)
    copy->CustomBannerCommit();
  if (default_cover_changed)
    copy->DefaultCoverCommit();
  if (custom_cover_changed)
    copy->CustomCoverCommit();

  *game_file = std::move(copy);

  return true;
}

bool GameFileCache::Load()
{
  return SyncCacheFile(false);
}

bool GameFileCache::Save()
{
  return SyncCacheFile(true);
}

bool GameFileCache::SyncCacheFile(bool save)
{
  const char* open_mode = save ? "wb" : "rb";
  File::IOFile f(m_path, open_mode);
  if (!f)
    return false;
  bool success = false;
  if (save)
  {
    // Measure the size of the buffer.
    u8* ptr = nullptr;
    PointerWrap p_measure(&ptr, 0, PointerWrap::Mode::Measure);
    DoState(&p_measure);
    const size_t buffer_size = reinterpret_cast<size_t>(ptr);

    // Then actually do the write.
    std::vector<u8> buffer(buffer_size);
    ptr = buffer.data();
    PointerWrap p(&ptr, buffer_size, PointerWrap::Mode::Write);
    DoState(&p, buffer_size);
    if (f.WriteBytes(buffer.data(), buffer.size()))
      success = true;
  }
  else
  {
    std::vector<u8> buffer(f.GetSize());
    if (!buffer.empty() && f.ReadBytes(buffer.data(), buffer.size()))
    {
      u8* ptr = buffer.data();
      PointerWrap p(&ptr, buffer.size(), PointerWrap::Mode::Read);
      DoState(&p, buffer.size());
      if (p.IsReadMode())
        success = true;
    }
  }
  if (!success)
  {
    // If some file operation failed, try to delete the probably-corrupted cache
    f.Close();
    File::Delete(m_path);
  }
  return success;
}

void GameFileCache::DoState(PointerWrap* p, u64 size)
{
  struct
  {
    u32 revision;
    u64 expected_size;
  } header = {CACHE_REVISION, size};
  p->Do(header);
  if (p->IsReadMode())
  {
    if (header.revision != CACHE_REVISION || header.expected_size != size)
    {
      p->SetMeasureMode();
      return;
    }
  }
  p->DoEachElement(m_cached_files, [](PointerWrap& state, std::shared_ptr<GameFile>& elem) {
    if (state.IsReadMode())
      elem = std::make_shared<GameFile>();
    elem->DoState(state);
  });
}

}  // namespace UICommon
