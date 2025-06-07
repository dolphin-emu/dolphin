// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/FilesystemWatcher.h"

#include <wtr/watcher.hpp>

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

namespace Common
{
FilesystemWatcher::FilesystemWatcher() = default;
FilesystemWatcher::~FilesystemWatcher() = default;

void FilesystemWatcher::Watch(const std::string& path)
{
  const auto [iter, inserted] = m_watched_paths.try_emplace(path, nullptr);
  if (inserted)
  {
    iter->second = std::make_unique<wtr::watch>(path, [this](wtr::event e) {
      const auto watched_path = PathToString(e.path_name);
      if (e.path_type == wtr::event::path_type::watcher)
      {
        if (watched_path.starts_with('e'))
          ERROR_LOG_FMT(COMMON, "Filesystem watcher: '{}'", watched_path);
        else if (watched_path.starts_with('w'))
          WARN_LOG_FMT(COMMON, "Filesystem watcher: '{}'", watched_path);
        return;
      }

      if (e.effect_type == wtr::event::effect_type::create)
      {
        const auto path = WithUnifiedPathSeparators(watched_path);
        PathAdded(path);
      }
      else if (e.effect_type == wtr::event::effect_type::modify)
      {
        const auto path = WithUnifiedPathSeparators(watched_path);
        PathModified(path);
      }
      else if (e.effect_type == wtr::event::effect_type::rename)
      {
        if (!e.associated)
        {
          WARN_LOG_FMT(COMMON, "Rename on path '{}' seen without association!", watched_path);
          return;
        }

        const auto old_path = WithUnifiedPathSeparators(watched_path);
        const auto new_path = WithUnifiedPathSeparators(PathToString(e.associated->path_name));
        PathRenamed(old_path, new_path);
      }
      else if (e.effect_type == wtr::event::effect_type::destroy)
      {
        const auto path = WithUnifiedPathSeparators(watched_path);
        PathDeleted(path);
      }
    });
  }
}

void FilesystemWatcher::Unwatch(const std::string& path)
{
  m_watched_paths.erase(path);
}
}  // namespace Common
