// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include <QFileSystemWatcher>
#include <QMap>
#include <QSet>
#include <QString>

#include "Common/WorkQueueThread.h"
#include "Core/TitleDatabase.h"
#include "UICommon/GameFile.h"
#include "UICommon/GameFileCache.h"

// Watches directories and loads GameFiles in a separate thread.
// To use this, just add directories using AddDirectory, and listen for the
// GameLoaded and GameRemoved signals.
class GameTracker final : public QFileSystemWatcher
{
  Q_OBJECT

public:
  explicit GameTracker(QObject* parent = nullptr);

  void AddDirectory(const QString& dir);
  void RemoveDirectory(const QString& dir);

signals:
  void GameLoaded(std::shared_ptr<const UICommon::GameFile> game);
  void GameRemoved(const QString& path);

private:
  void UpdateDirectory(const QString& dir);
  void UpdateFile(const QString& path);
  void AddDirectoryInternal(const QString& dir);
  void RemoveDirectoryInternal(const QString& dir);
  void UpdateDirectoryInternal(const QString& dir);
  void UpdateFileInternal(const QString& path);
  QSet<QString> FindMissingFiles(const QString& dir);
  void LoadGame(const QString& path);

  enum class CommandType
  {
    LoadCache,
    AddDirectory,
    RemoveDirectory,
    UpdateDirectory,
    UpdateFile,
  };

  struct Command
  {
    CommandType type;
    QString path;
  };

  // game path -> directories that track it
  QMap<QString, QSet<QString>> m_tracked_files;
  Common::WorkQueueThread<Command> m_load_thread;
  UICommon::GameFileCache m_cache;
  Core::TitleDatabase m_title_database;
};

Q_DECLARE_METATYPE(std::shared_ptr<const UICommon::GameFile>)
