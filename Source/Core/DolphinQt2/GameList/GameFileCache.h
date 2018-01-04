// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QList>
#include <QMap>
#include <QString>

#include "DolphinQt2/GameList/GameFile.h"

class GameFileCache
{
public:
  explicit GameFileCache();

  void Update(const GameFile& gamefile);
  void Save() const;
  void Load();
  bool IsCached(const QString& path) const;
  GameFile GetFile(const QString& path) const;
  QList<QString> GetCached() const;

private:
  QString m_path;

  QMap<QString, GameFile> m_gamefiles;
};
