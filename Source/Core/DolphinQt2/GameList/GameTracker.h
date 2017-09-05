// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QFileSystemWatcher>
#include <QMap>
#include <QSet>
#include <QSharedPointer>
#include <QString>

#include "Common/WorkQueueThread.h"
#include "DolphinQt2/GameList/GameFile.h"
#include "DolphinQt2/GameList/GameFileCache.h"

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
  void GameLoaded(QSharedPointer<GameFile> game);
  void GameRemoved(const QString& path);

private:
  void LoadGame(const QString& path);
  void UpdateDirectory(const QString& dir);
  void UpdateFile(const QString& path);
  QSet<QString> FindMissingFiles(const QString& dir);

  // game path -> directories that track it
  QMap<QString, QSet<QString>> m_tracked_files;
  Common::WorkQueueThread<QString> m_load_thread;
  GameFileCache cache;
};

Q_DECLARE_METATYPE(QSharedPointer<GameFile>)
