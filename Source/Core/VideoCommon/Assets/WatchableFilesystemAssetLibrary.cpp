// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/WatchableFilesystemAssetLibrary.h"

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

namespace VideoCommon
{
void WatchableFilesystemAssetLibrary::Watch(const std::string& path)
{
  const auto [iter, inserted] = m_watched_paths.try_emplace(path, nullptr);
  if (inserted)
  {
    iter->second = std::make_unique<wtr::watch>(path, [this](wtr::event e) {
      if (e.path_type == wtr::event::path_type::watcher)
      {
        return;
      }

      if (e.effect_type == wtr::event::effect_type::create)
      {
        const auto path = WithUnifiedPathSeparators(PathToString(e.path_name));
        PathAdded(path);
      }
      else if (e.effect_type == wtr::event::effect_type::modify)
      {
        const auto path = WithUnifiedPathSeparators(PathToString(e.path_name));
        PathModified(path);
      }
      else if (e.effect_type == wtr::event::effect_type::rename)
      {
        if (!e.associated)
        {
          WARN_LOG_FMT(VIDEO, "Rename on path seen without association!");
          return;
        }

        const auto old_path = WithUnifiedPathSeparators(PathToString(e.path_name));
        const auto new_path = WithUnifiedPathSeparators(PathToString(e.associated->path_name));
        PathRenamed(old_path, new_path);
      }
      else if (e.effect_type == wtr::event::effect_type::destroy)
      {
        const auto path = WithUnifiedPathSeparators(PathToString(e.path_name));
        PathDeleted(path);
      }
    });
  }
}

void WatchableFilesystemAssetLibrary::Unwatch(const std::string& path)
{
  m_watched_paths.erase(path);
}
}  // namespace VideoCommon
