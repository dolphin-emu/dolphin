// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/GameList/GameListModel.h"
#include "Core/ConfigManager.h"
#include "DiscIO/Enums.h"
#include "DolphinQt2/Resources.h"
#include "DolphinQt2/Settings.h"

const QSize GAMECUBE_BANNER_SIZE(96, 32);

GameListModel::GameListModel(QObject* parent) : QAbstractTableModel(parent)
{
  connect(&m_tracker, &GameTracker::GameLoaded, this, &GameListModel::UpdateGame);
  connect(&m_tracker, &GameTracker::GameRemoved, this, &GameListModel::RemoveGame);
  connect(&Settings::Instance(), &Settings::PathAdded, &m_tracker, &GameTracker::AddDirectory);
  connect(&Settings::Instance(), &Settings::PathRemoved, &m_tracker, &GameTracker::RemoveDirectory);

  for (const QString& dir : Settings::Instance().GetPaths())
    m_tracker.AddDirectory(dir);

  connect(&Settings::Instance(), &Settings::ThemeChanged, [this] {
    // Tell the view to repaint. The signal 'dataChanged' also seems like it would work here, but
    // unfortunately it won't cause a repaint until the view is focused.
    emit layoutAboutToBeChanged();
    emit layoutChanged();
  });

  // TODO: Reload m_title_database when the language changes
}

QVariant GameListModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid())
    return QVariant();

  QSharedPointer<GameFile> game = m_games[index.row()];

  switch (index.column())
  {
  case COL_PLATFORM:
    if (role == Qt::DecorationRole)
      return Resources::GetPlatform(static_cast<int>(game->GetPlatformID()));
    if (role == Qt::InitialSortOrderRole)
      return static_cast<int>(game->GetPlatformID());
    break;
  case COL_COUNTRY:
    if (role == Qt::DecorationRole)
      return Resources::GetCountry(static_cast<int>(game->GetCountryID()));
    if (role == Qt::InitialSortOrderRole)
      return static_cast<int>(game->GetCountryID());
    break;
  case COL_RATING:
    if (role == Qt::DecorationRole)
      return Resources::GetRating(game->GetRating());
    if (role == Qt::InitialSortOrderRole)
      return game->GetRating();
    break;
  case COL_BANNER:
    if (role == Qt::DecorationRole)
    {
      // GameCube banners are 96x32, but Wii banners are 192x64.
      // TODO: use custom banners from rom directory like DolphinWX?
      QPixmap banner = game->GetBanner();
      if (banner.isNull())
        banner = Resources::GetMisc(Resources::BANNER_MISSING);
      banner.setDevicePixelRatio(std::max(banner.width() / GAMECUBE_BANNER_SIZE.width(),
                                          banner.height() / GAMECUBE_BANNER_SIZE.height()));
      return banner;
    }
    break;
  case COL_TITLE:
    if (role == Qt::DisplayRole || role == Qt::InitialSortOrderRole)
    {
      QString display_name = QString::fromStdString(m_title_database.GetTitleName(
          game->GetGameID().toStdString(), game->GetPlatformID() == DiscIO::Platform::WII_WAD ?
                                               Core::TitleDatabase::TitleType::Channel :
                                               Core::TitleDatabase::TitleType::Other));
      if (display_name.isEmpty())
        display_name = game->GetLongName();

      if (display_name.isEmpty())
        display_name = game->GetFileName();

      return display_name;
    }
    break;
  case COL_ID:
    if (role == Qt::DisplayRole || role == Qt::InitialSortOrderRole)
      return game->GetGameID();
    break;
  case COL_DESCRIPTION:
    if (role == Qt::DisplayRole || role == Qt::InitialSortOrderRole)
      return game->GetDescription();
    break;
  case COL_MAKER:
    if (role == Qt::DisplayRole || role == Qt::InitialSortOrderRole)
      return game->GetMaker();
    break;
  case COL_SIZE:
    if (role == Qt::DisplayRole)
      return FormatSize(game->GetFileSize());
    if (role == Qt::InitialSortOrderRole)
      return game->GetFileSize();
    break;
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
    return tr("State");
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

bool GameListModel::ShouldDisplayGameListItem(int index) const
{
  QSharedPointer<GameFile> game = m_games[index];

  const bool show_platform = [&game] {
    switch (game->GetPlatformID())
    {
    case DiscIO::Platform::GAMECUBE_DISC:
      return SConfig::GetInstance().m_ListGC;
    case DiscIO::Platform::WII_DISC:
      return SConfig::GetInstance().m_ListWii;
    case DiscIO::Platform::WII_WAD:
      return SConfig::GetInstance().m_ListWad;
    case DiscIO::Platform::ELF_DOL:
      return SConfig::GetInstance().m_ListElfDol;
    default:
      return false;
    }
  }();

  if (!show_platform)
    return false;

  switch (game->GetCountryID())
  {
  case DiscIO::Country::COUNTRY_AUSTRALIA:
    return SConfig::GetInstance().m_ListAustralia;
  case DiscIO::Country::COUNTRY_EUROPE:
    return SConfig::GetInstance().m_ListPal;
  case DiscIO::Country::COUNTRY_FRANCE:
    return SConfig::GetInstance().m_ListFrance;
  case DiscIO::Country::COUNTRY_GERMANY:
    return SConfig::GetInstance().m_ListGermany;
  case DiscIO::Country::COUNTRY_ITALY:
    return SConfig::GetInstance().m_ListItaly;
  case DiscIO::Country::COUNTRY_JAPAN:
    return SConfig::GetInstance().m_ListJap;
  case DiscIO::Country::COUNTRY_KOREA:
    return SConfig::GetInstance().m_ListKorea;
  case DiscIO::Country::COUNTRY_NETHERLANDS:
    return SConfig::GetInstance().m_ListNetherlands;
  case DiscIO::Country::COUNTRY_RUSSIA:
    return SConfig::GetInstance().m_ListRussia;
  case DiscIO::Country::COUNTRY_SPAIN:
    return SConfig::GetInstance().m_ListSpain;
  case DiscIO::Country::COUNTRY_TAIWAN:
    return SConfig::GetInstance().m_ListTaiwan;
  case DiscIO::Country::COUNTRY_USA:
    return SConfig::GetInstance().m_ListUsa;
  case DiscIO::Country::COUNTRY_WORLD:
    return SConfig::GetInstance().m_ListWorld;
  case DiscIO::Country::COUNTRY_UNKNOWN:
  default:
    return SConfig::GetInstance().m_ListUnknown;
  }
}

QSharedPointer<GameFile> GameListModel::GetGameFile(int index) const
{
  return m_games[index];
}

void GameListModel::UpdateGame(const QSharedPointer<GameFile>& game)
{
  QString path = game->GetFilePath();

  int index = FindGame(path);
  if (index < 0)
  {
    beginInsertRows(QModelIndex(), m_games.size(), m_games.size());
    m_games.push_back(game);
    endInsertRows();
  }
  else
  {
    m_games[index] = game;
    emit dataChanged(createIndex(index, 0), createIndex(index + 1, columnCount(QModelIndex())));
  }
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
