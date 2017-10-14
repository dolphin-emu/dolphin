// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QFile>

#include <mutex>

#include "DolphinQt2/GameList/GameFile.h"

class GameFileCache
{
public:
  explicit GameFileCache();

  void Update(const GameFile& gamefile);
  void Save();
  void Load();
  bool IsCached(const QString& path);
  GameFile GetFile(const QString& path);
  QList<QString> GetCached();

private:
  QString m_path;

  QMap<QString, GameFile> m_gamefiles;
  std::mutex m_mutex;
};
