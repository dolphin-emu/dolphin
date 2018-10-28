// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include <QFileSystemWatcher>
#include <QMap>
#include <QSet>
#include <QString>
#include <QVector>

#include "Common/Event.h"
#include "Common/WorkQueueThread.h"
#include "UICommon/GameFileCache.h"

namespace UICommon
{
class GameFile;
}

// Watches directories and loads GameFiles in a separate thread.
// To use this, just add directories using AddDirectory, and listen for the
// GameLoaded and GameRemoved signals.
class GameTracker final : public QFileSystemWatcher
{
  Q_OBJECT

public:
  explicit GameTracker(QObject* parent = nullptr);

  // A GameTracker won't emit any signals until this function has been called.
  // Before you call this function, make sure to call AddDirectory for all
  // directories you want to track, otherwise games will briefly disappear
  // until you call AddDirectory and the GameTracker finishes scanning the file system.
  void Start();

  void AddDirectory(const QString& dir);
  void RemoveDirectory(const QString& dir);
  void RefreshAll();
  void PurgeCache();

signals:
  void GameLoaded(const std::shared_ptr<const UICommon::GameFile>& game);
  void GameUpdated(const std::shared_ptr<const UICommon::GameFile>& game);
  void GameRemoved(const std::string& path);

private:
  void LoadCache();
  void StartInternal();
  void UpdateDirectory(const QString& dir);
  void UpdateFile(const QString& path);
  void AddDirectoryInternal(const QString& dir);
  void RemoveDirectoryInternal(const QString& dir);
  void UpdateDirectoryInternal(const QString& dir);
  void UpdateFileInternal(const QString& path);
  QSet<QString> FindMissingFiles(const QString& dir);
  void LoadGame(const QString& path);

  bool AddPath(const QString& path);
  bool RemovePath(const QString& path);

  enum class CommandType
  {
    LoadCache,
    Start,
    AddDirectory,
    RemoveDirectory,
    UpdateDirectory,
    UpdateFile,
    UpdateMetadata,
    PurgeCache
  };

  struct Command
  {
    CommandType type;
    QString path;
  };

  // game path -> directories that track it
  QMap<QString, QSet<QString>> m_tracked_files;
  QVector<QString> m_tracked_paths;
  Common::WorkQueueThread<Command> m_load_thread;
  UICommon::GameFileCache m_cache;
  Common::Event m_cache_loaded_event;
  Common::Event m_initial_games_emitted_event;
  bool m_initial_games_emitted = false;
  bool m_started = false;
};

Q_DECLARE_METATYPE(std::shared_ptr<const UICommon::GameFile>)
Q_DECLARE_METATYPE(std::string)
