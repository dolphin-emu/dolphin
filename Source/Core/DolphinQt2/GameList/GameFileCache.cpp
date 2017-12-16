// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "GameFileCache.h"

#include <QDataStream>
#include <QDir>
#include <QFileInfo>

#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "DolphinQt2/Settings.h"

static const int CACHE_VERSION = 4;  // Last changed in PR #6109
static const int DATASTREAM_VERSION = QDataStream::Qt_5_0;

GameFileCache::GameFileCache()
    : m_path(QString::fromStdString(File::GetUserPath(D_CACHE_IDX) + "qt_gamefile.cache"))
{
}

bool GameFileCache::IsCached(const QString& path)
{
  std::lock_guard<std::mutex> guard(m_mutex);

  return m_gamefiles.contains(path);
}

GameFile GameFileCache::GetFile(const QString& path)
{
  std::lock_guard<std::mutex> guard(m_mutex);

  return m_gamefiles[path];
}

void GameFileCache::Load()
{
  std::lock_guard<std::mutex> guard(m_mutex);

  QFile file(m_path);

  if (!file.open(QIODevice::ReadOnly))
    return;

  QDataStream stream(&file);
  stream.setVersion(DATASTREAM_VERSION);

  qint32 cache_version;
  stream >> cache_version;

  // If the cache file is using an older version, ignore it and create it from scratch
  if (cache_version != CACHE_VERSION)
    return;

  stream >> m_gamefiles;
}

void GameFileCache::Save()
{
  std::lock_guard<std::mutex> guard(m_mutex);

  QFile file(m_path);

  if (!file.open(QIODevice::WriteOnly))
    return;

  QDataStream stream(&file);
  stream.setVersion(DATASTREAM_VERSION);

  stream << static_cast<qint32>(CACHE_VERSION);
  stream << m_gamefiles;
}

void GameFileCache::Update(const GameFile& gamefile)
{
  std::lock_guard<std::mutex> guard(m_mutex);

  m_gamefiles[gamefile.GetFilePath()] = gamefile;
}

QList<QString> GameFileCache::GetCached()
{
  std::lock_guard<std::mutex> guard(m_mutex);

  return m_gamefiles.keys();
}
