// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QDebug>

#include "DolphinQt/GameList/GameListModel.h"
#include "DolphinQt/Resources.h"

GameListModel::GameListModel(QObject* parent)
	: QAbstractTableModel(parent)
{
	connect(&m_tracker, &GameTracker::GameLoaded, this, &GameListModel::UpdateGame);
	connect(&m_tracker, &GameTracker::GameRemoved, this, &GameListModel::RemoveGame);
}

QVariant GameListModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
		return QVariant();

	if (index.column() == COL_PLATFORM && role == Qt::DecorationRole)
		return QVariant(Resources::GetPlatform(m_games[index.row()]->GetPlatform()));
	else if (index.column() == COL_PLATFORM && role == Qt::DisplayRole)
		return QVariant(m_games[index.row()]->GetPlatform());

	else if (index.column() == COL_TITLE && role == Qt::DecorationRole)
		return QVariant(m_games[index.row()]->GetBanner());
	else if (index.column() == COL_TITLE && role == Qt::DisplayRole)
		return QVariant(m_games[index.row()]->GetLongName());

	else if (index.column() == COL_DESCRIPTION && role == Qt::DisplayRole)
		return QVariant(m_games[index.row()]->GetDescription());

	else if (index.column() == COL_COUNTRY && role == Qt::DecorationRole)
		return QVariant(Resources::GetCountry(m_games[index.row()]->GetCountry()));
	else if (index.column() == COL_COUNTRY && role == Qt::DisplayRole)
		return QVariant(m_games[index.row()]->GetCountry());

	else if (index.column() == COL_RATING && role == Qt::DecorationRole)
		return QVariant(Resources::GetRating(m_games[index.row()]->GetRating()));
	else if (index.column() == COL_RATING && role == Qt::DisplayRole)
		return QVariant(m_games[index.row()]->GetRating());

	else if (index.column() == COL_LARGE_ICON && role == Qt::DecorationRole)
		return QVariant(m_games[index.row()]->GetBanner().scaled(144, 48));
	else if (index.column() == COL_LARGE_ICON && role == Qt::DisplayRole)
		return QVariant(m_games[index.row()]->GetLongName());

	else
		return QVariant();
}

QVariant GameListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Vertical || role != Qt::DisplayRole)
		return QVariant();
	
	switch (section)
	{
		case COL_TITLE: return QVariant(tr("Title"));
		case COL_DESCRIPTION: return QVariant(tr("Description"));
		case COL_RATING: return QVariant(tr("Quality"));
		default: return QVariant();
	}
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
	if (m_entries.contains(path))
		RemoveGame(path);

	beginInsertRows(QModelIndex(), m_games.size(), m_games.size());
	m_entries[path] = m_games.size();
	m_games.append(game);
	endInsertRows();
}

void GameListModel::RemoveGame(QString path)
{
	int entry = m_entries[path];
	beginRemoveRows(QModelIndex(), entry, entry);
	m_entries.remove(path);
	m_games.removeAt(entry);
	endRemoveRows();
}
