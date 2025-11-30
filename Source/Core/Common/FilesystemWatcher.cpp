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

void FilesystemWatcher::Watch(const std::string& path, StartedCallback started_callback)
{
  auto& watch_ptr = m_watched_paths[path];
  // Are we already watching this path?
  if (watch_ptr != nullptr)
  {
    // The caller doesn't require notification on start so we're done.
    if (!started_callback)
      return;

    // Remove and recreate the watch object so we can wait for a new "started" event.
    watch_ptr.reset();
  }

  watch_ptr = std::make_unique<wtr::watch>(path, [this, started_callback](wtr::event e) {
    const auto watched_path = PathToString(e.path_name);
    if (e.path_type == wtr::event::path_type::watcher)
    {
      if (watched_path.starts_with('e'))
        ERROR_LOG_FMT(COMMON, "Filesystem watcher: '{}'", watched_path);
      else if (watched_path.starts_with('w'))
        WARN_LOG_FMT(COMMON, "Filesystem watcher: '{}'", watched_path);

      if (e.effect_type == wtr::event::effect_type::create && started_callback)
      {
        const auto unified_path = WithUnifiedPathSeparators(watched_path);
        started_callback(unified_path);
      }
      return;
    }

    if (e.effect_type == wtr::event::effect_type::create)
    {
      const auto unified_path = WithUnifiedPathSeparators(watched_path);
      PathAdded(unified_path);
    }
    else if (e.effect_type == wtr::event::effect_type::modify)
    {
      const auto unified_path = WithUnifiedPathSeparators(watched_path);
      PathModified(unified_path);
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
      const auto unified_path = WithUnifiedPathSeparators(watched_path);
      PathDeleted(unified_path);
    }
  });
}

void FilesystemWatcher::Unwatch(const std::string& path)
{
  m_watched_paths.erase(path);
}
}  // namespace Common
