// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Resources.h"
#include "DolphinQt2/GameList/GameListModel.h"

GameListModel::GameListModel(QObject* parent)
	: QAbstractTableModel(parent)
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
	if (role == Qt::DisplayRole)
	{
		switch (index.column())
		{
		case COL_PLATFORM:    return game->GetPlatform();
		case COL_BANNER:      return game->GetBanner();
		case COL_TITLE:       return game->GetLongName();
		case COL_ID:          return game->GetUniqueID();
		case COL_DESCRIPTION: return game->GetDescription();
		case COL_MAKER:       return game->GetCompany();
		case COL_SIZE:        return game->GetFileSize();
		case COL_COUNTRY:     return game->GetCountry();
		case COL_RATING:      return game->GetRating();
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
	case COL_TITLE:       return tr("Title");
	case COL_ID:          return tr("ID");
	case COL_BANNER:      return tr("Banner");
	case COL_DESCRIPTION: return tr("Description");
	case COL_MAKER:       return tr("Maker");
	case COL_SIZE:        return tr("Size");
	case COL_RATING:      return tr("Quality");
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
	QString path = game->GetPath();

	int entry = FindGame(path);
	if (entry < 0)
		entry = m_games.size();

	beginInsertRows(QModelIndex(), entry, entry);
	m_games.insert(entry, game);
	endInsertRows();
}

void GameListModel::RemoveGame(QString path)
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
		if (m_games[i]->GetPath() == path)
			return i;
	}
	return -1;
}
