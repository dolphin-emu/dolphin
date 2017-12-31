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
  void LoadGame(const QString& path);
  void UpdateDirectory(const QString& dir);
  void UpdateFile(const QString& path);
  QSet<QString> FindMissingFiles(const QString& dir);

  // game path -> directories that track it
  QMap<QString, QSet<QString>> m_tracked_files;
  Common::WorkQueueThread<QString> m_load_thread;
  UICommon::GameFileCache m_cache;
  Core::TitleDatabase m_title_database;
};

Q_DECLARE_METATYPE(std::shared_ptr<const UICommon::GameFile>)
