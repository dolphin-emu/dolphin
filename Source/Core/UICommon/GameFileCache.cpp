// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
#include "Common/File.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"

#include "DiscIO/DirectoryBlob.h"

#include "UICommon/GameFile.h"

namespace UICommon
{
static constexpr u32 CACHE_REVISION = 15;  // Last changed in PR 7816

std::vector<std::string> FindAllGamePaths(const std::vector<std::string>& directories_to_scan,
                                          bool recursive_scan)
{
  static const std::vector<std::string> search_extensions = {
      ".gcm", ".tgc", ".iso", ".ciso", ".gcz", ".wbfs", ".wad", ".dol", ".elf"};

  // TODO: We could process paths iteratively as they are found
  return Common::DoFileSearch(directories_to_scan, search_extensions, recursive_scan);
}

GameFileCache::GameFileCache() : m_path(File::GetUserPath(D_CACHE_IDX) + "gamelist.cache")
{
}

void GameFileCache::ForEach(std::function<void(const std::shared_ptr<const GameFile>&)> f) const
{
  for (const std::shared_ptr<const GameFile>& item : m_cached_files)
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

bool GameFileCache::Update(
    const std::vector<std::string>& all_game_paths,
    std::function<void(const std::shared_ptr<const GameFile>&)> game_added_to_cache,
    std::function<void(const std::string&)> game_removed_from_cache)
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

bool GameFileCache::UpdateAdditionalMetadata(
    std::function<void(const std::shared_ptr<const GameFile>&)> game_updated)
{
  bool cache_changed = false;

  for (std::shared_ptr<GameFile>& file : m_cached_files)
  {
    const bool updated = UpdateAdditionalMetadata(&file);
    cache_changed |= updated;
    if (game_updated && updated)
      game_updated(file);
  }

  return cache_changed;
}

bool GameFileCache::UpdateAdditionalMetadata(std::shared_ptr<GameFile>* game_file)
{
#ifndef ANDROID
  const bool wii_banner_changed = (*game_file)->WiiBannerChanged();
  const bool custom_banner_changed = (*game_file)->CustomBannerChanged();

  (*game_file)->DownloadDefaultCover();

  const bool default_cover_changed = (*game_file)->DefaultCoverChanged();
  const bool custom_cover_changed = (*game_file)->CustomCoverChanged();

  if (!wii_banner_changed && !custom_banner_changed && !default_cover_changed &&
      !custom_cover_changed)
    return false;

  // If a cached file needs an update, apply the updates to a copy and delete the original.
  // This makes the usage of cached files in other threads safe.

  std::shared_ptr<GameFile> copy = std::make_shared<GameFile>(**game_file);
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
#else
  return false;
#endif
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
    PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);
    DoState(&p);
    const size_t buffer_size = reinterpret_cast<size_t>(ptr);

    // Then actually do the write.
    std::vector<u8> buffer(buffer_size);
    ptr = &buffer[0];
    p.SetMode(PointerWrap::MODE_WRITE);
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
      PointerWrap p(&ptr, PointerWrap::MODE_READ);
      DoState(&p, buffer.size());
      if (p.GetMode() == PointerWrap::MODE_READ)
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
  if (p->GetMode() == PointerWrap::MODE_READ)
  {
    if (header.revision != CACHE_REVISION || header.expected_size != size)
    {
      p->SetMode(PointerWrap::MODE_MEASURE);
      return;
    }
  }
  p->DoEachElement(m_cached_files, [](PointerWrap& state, std::shared_ptr<GameFile>& elem) {
    if (state.GetMode() == PointerWrap::MODE_READ)
      elem = std::make_shared<GameFile>();
    elem->DoState(state);
  });
}

}  // namespace UICommon
