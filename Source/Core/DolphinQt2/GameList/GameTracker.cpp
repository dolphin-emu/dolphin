// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QDir>
#include <QDirIterator>
#include <QFile>

#include "DiscIO/DirectoryBlob.h"
#include "DolphinQt2/GameList/GameTracker.h"
#include "DolphinQt2/QtUtils/QueueOnObject.h"
#include "DolphinQt2/Settings.h"

static const QStringList game_filters{
    QStringLiteral("*.gcm"),  QStringLiteral("*.iso"), QStringLiteral("*.tgc"),
    QStringLiteral("*.ciso"), QStringLiteral("*.gcz"), QStringLiteral("*.wbfs"),
    QStringLiteral("*.wad"),  QStringLiteral("*.elf"), QStringLiteral("*.dol")};

GameTracker::GameTracker(QObject* parent) : QFileSystemWatcher(parent)
{
  qRegisterMetaType<std::shared_ptr<const UICommon::GameFile>>();
  connect(this, &QFileSystemWatcher::directoryChanged, this, &GameTracker::UpdateDirectory);
  connect(this, &QFileSystemWatcher::fileChanged, this, &GameTracker::UpdateFile);

  m_load_thread.Reset([this](Command command) {
    switch (command.type)
    {
    case CommandType::LoadCache:
      m_cache.Load();
      break;
    case CommandType::AddDirectory:
      AddDirectoryInternal(command.path);
      break;
    case CommandType::RemoveDirectory:
      RemoveDirectoryInternal(command.path);
      break;
    case CommandType::UpdateDirectory:
      UpdateDirectoryInternal(command.path);
      break;
    case CommandType::UpdateFile:
      UpdateFileInternal(command.path);
      break;
    }
  });

  m_load_thread.EmplaceItem(Command{CommandType::LoadCache, {}});

  // TODO: When language changes, reload m_title_database and call m_cache.UpdateAdditionalMetadata
}

void GameTracker::AddDirectory(const QString& dir)
{
  m_load_thread.EmplaceItem(Command{CommandType::AddDirectory, dir});
}

void GameTracker::RemoveDirectory(const QString& dir)
{
  m_load_thread.EmplaceItem(Command{CommandType::RemoveDirectory, dir});
}

void GameTracker::UpdateDirectory(const QString& dir)
{
  m_load_thread.EmplaceItem(Command{CommandType::UpdateDirectory, dir});
}

void GameTracker::UpdateFile(const QString& dir)
{
  m_load_thread.EmplaceItem(Command{CommandType::UpdateFile, dir});
}

void GameTracker::AddDirectoryInternal(const QString& dir)
{
  if (!QFileInfo(dir).exists())
    return;
  addPath(dir);
  UpdateDirectoryInternal(dir);
}

void GameTracker::RemoveDirectoryInternal(const QString& dir)
{
  removePath(dir);
  QDirIterator it(dir, game_filters, QDir::NoFilter, QDirIterator::Subdirectories);
  while (it.hasNext())
  {
    QString path = QFileInfo(it.next()).canonicalFilePath();
    if (m_tracked_files.contains(path))
    {
      m_tracked_files[path].remove(dir);
      if (m_tracked_files[path].empty())
      {
        removePath(path);
        m_tracked_files.remove(path);
        emit GameRemoved(path);
      }
    }
  }
}

void GameTracker::UpdateDirectoryInternal(const QString& dir)
{
  QDirIterator it(dir, game_filters, QDir::NoFilter, QDirIterator::Subdirectories);
  while (it.hasNext())
  {
    QString path = QFileInfo(it.next()).canonicalFilePath();

    if (m_tracked_files.contains(path))
    {
      auto& tracked_file = m_tracked_files[path];
      if (!tracked_file.contains(dir))
        tracked_file.insert(dir);
    }
    else
    {
      addPath(path);
      m_tracked_files[path] = QSet<QString>{dir};
      LoadGame(path);
    }
  }

  for (const auto& missing : FindMissingFiles(dir))
  {
    auto& tracked_file = m_tracked_files[missing];

    tracked_file.remove(dir);
    if (tracked_file.empty())
    {
      m_tracked_files.remove(missing);
      GameRemoved(missing);
    }
  }
}

void GameTracker::UpdateFileInternal(const QString& file)
{
  if (QFileInfo(file).exists())
  {
    GameRemoved(file);
    addPath(file);
    LoadGame(file);
  }
  else if (removePath(file))
  {
    m_tracked_files.remove(file);
    emit GameRemoved(file);
  }
}

QSet<QString> GameTracker::FindMissingFiles(const QString& dir)
{
  QDirIterator it(dir, game_filters, QDir::NoFilter, QDirIterator::Subdirectories);

  QSet<QString> missing_files;

  for (const auto& key : m_tracked_files.keys())
  {
    if (m_tracked_files[key].contains(dir))
      missing_files.insert(key);
  }

  while (it.hasNext())
  {
    QString path = QFileInfo(it.next()).canonicalFilePath();
    if (m_tracked_files.contains(path))
      missing_files.remove(path);
  }

  return missing_files;
}

void GameTracker::LoadGame(const QString& path)
{
  const std::string converted_path = path.toStdString();
  if (!DiscIO::ShouldHideFromGameList(converted_path))
  {
    bool cache_changed = false;
    auto game = m_cache.AddOrGet(converted_path, &cache_changed, m_title_database);
    if (game)
      emit GameLoaded(std::move(game));
    if (cache_changed)
      m_cache.Save();
  }
}
