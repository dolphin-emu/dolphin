// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QDir>
#include <QDirIterator>
#include <QFile>

#include "DiscIO/DirectoryBlob.h"
#include "DolphinQt2/GameList/GameTracker.h"
#include "DolphinQt2/Settings.h"

static const QStringList game_filters{
    QStringLiteral("*.gcm"),  QStringLiteral("*.iso"), QStringLiteral("*.tgc"),
    QStringLiteral("*.ciso"), QStringLiteral("*.gcz"), QStringLiteral("*.wbfs"),
    QStringLiteral("*.wad"),  QStringLiteral("*.elf"), QStringLiteral("*.dol")};

GameTracker::GameTracker(QObject* parent) : QFileSystemWatcher(parent)
{
  m_loader = new GameLoader;
  m_loader->moveToThread(&m_loader_thread);

  qRegisterMetaType<QSharedPointer<GameFile>>();
  connect(&m_loader_thread, &QThread::finished, m_loader, &QObject::deleteLater);
  connect(this, &QFileSystemWatcher::directoryChanged, this, &GameTracker::UpdateDirectory);
  connect(this, &QFileSystemWatcher::fileChanged, this, &GameTracker::UpdateFile);
  connect(this, &GameTracker::PathChanged, m_loader, &GameLoader::LoadGame);
  connect(m_loader, &GameLoader::GameLoaded, this, &GameTracker::GameLoaded);

  m_loader_thread.start();

  for (QString dir : Settings::Instance().GetPaths())
    AddDirectory(dir);
}

GameTracker::~GameTracker()
{
  m_loader_thread.quit();
  m_loader_thread.wait();
}

void GameTracker::AddDirectory(const QString& dir)
{
  if (!QFileInfo(dir).exists())
    return;
  addPath(dir);
  UpdateDirectory(dir);
}

void GameTracker::RemoveDirectory(const QString& dir)
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

void GameTracker::UpdateDirectory(const QString& dir)
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
      emit PathChanged(path);
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

void GameTracker::UpdateFile(const QString& file)
{
  if (QFileInfo(file).exists())
  {
    GameRemoved(file);
    addPath(file);

    emit PathChanged(file);
  }
  else if (removePath(file))
  {
    m_tracked_files.remove(file);
    emit GameRemoved(file);
  }
}

void GameLoader::LoadGame(const QString& path)
{
  if (!DiscIO::ShouldHideFromGameList(path.toStdString()))
  {
    auto game = QSharedPointer<GameFile>::create(path);
    if (game->IsValid())
      emit GameLoaded(game);
  }
}
