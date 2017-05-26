// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/GameList/GameListModel.h"
#include "DolphinQt2/Resources.h"

const QSize GAMECUBE_BANNER_SIZE(96, 32);

GameListModel::GameListModel(QObject* parent) : QAbstractTableModel(parent)
{
  connect(&m_tracker, &GameTracker::GameLoaded, this, &GameListModel::UpdateGame);
  connect(&m_tracker, &GameTracker::GameRemoved, this, &GameListModel::RemoveGame);
  connect(this, &GameListModel::DirectoryAdded, &m_tracker, &GameTracker::AddDirectory);
  connect(this, &GameListModel::DirectoryRemoved, &m_tracker, &GameTracker::RemoveDirectory);
}

QVariant GameListModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid())
    return QVariant();

  QSharedPointer<GameFile> game = m_games[index.row()];
  if (role == Qt::DecorationRole)
  {
    switch (index.column())
    {
    case COL_PLATFORM:
      return Resources::GetPlatform(static_cast<int>(game->GetPlatformID()));
    case COL_COUNTRY:
      return Resources::GetCountry(static_cast<int>(game->GetCountryID()));
    case COL_RATING:
      return Resources::GetRating(game->GetRating());
    case COL_BANNER:
      // GameCube banners are 96x32, but Wii banners are 192x64.
      // TODO: use custom banners from rom directory like DolphinWX?
      QPixmap banner = game->GetBanner();
      banner.setDevicePixelRatio(std::max(banner.width() / GAMECUBE_BANNER_SIZE.width(),
                                          banner.height() / GAMECUBE_BANNER_SIZE.height()));
      return banner;
    }
  }
  if (role == Qt::DisplayRole)
  {
    switch (index.column())
    {
    case COL_TITLE:
      return game->GetLongName();
    case COL_ID:
      return game->GetGameID();
    case COL_DESCRIPTION:
      return game->GetDescription();
    case COL_MAKER:
      return game->GetMaker();
    case COL_SIZE:
      return FormatSize(game->GetFileSize());
    }
  }
  return QVariant();
}

QVariant GameListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation == Qt::Vertical || role != Qt::DisplayRole)
    return QVariant();

  switch (section)
  {
  case COL_TITLE:
    return tr("Title");
  case COL_ID:
    return tr("ID");
  case COL_BANNER:
    return tr("Banner");
  case COL_DESCRIPTION:
    return tr("Description");
  case COL_MAKER:
    return tr("Maker");
  case COL_SIZE:
    return tr("Size");
  case COL_RATING:
    return tr("Quality");
  }
  return QVariant();
}

int GameListModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid())
    return 0;
  return m_games.size();
}

int GameListModel::columnCount(const QModelIndex& parent) const
{
  return NUM_COLS;
}

void GameListModel::UpdateGame(QSharedPointer<GameFile> game)
{
  QString path = game->GetFilePath();

  int entry = FindGame(path);
  if (entry < 0)
    entry = m_games.size();
  else
    return;

  beginInsertRows(QModelIndex(), entry, entry);
  m_games.insert(entry, game);
  endInsertRows();
}

void GameListModel::RemoveGame(const QString& path)
{
  int entry = FindGame(path);
  if (entry < 0)
    return;

  beginRemoveRows(QModelIndex(), entry, entry);
  m_games.removeAt(entry);
  endRemoveRows();
}

int GameListModel::FindGame(const QString& path) const
{
  for (int i = 0; i < m_games.size(); i++)
  {
    if (m_games[i]->GetFilePath() == path)
      return i;
  }
  return -1;
}
