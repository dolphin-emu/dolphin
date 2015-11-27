// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Resources.h"
#include "DolphinQt2/GameList/GameListModel.h"

static QString FormatSize(qint64 size)
{
	QStringList units{
		QStringLiteral("KB"),
		QStringLiteral("MB"),
		QStringLiteral("GB"),
		QStringLiteral("TB")
	};
	QStringListIterator i(units);
	QString unit = QStringLiteral("B");
	double num = (double) size;
	while (num > 1024.0 && i.hasNext())
	{
		unit = i.next();
		num /= 1024.0;
	}
	return QStringLiteral("%1 %2").arg(QString::number(num, 'f', 1)).arg(unit);
}

GameListModel::GameListModel(QObject* parent)
	: QAbstractTableModel(parent)
{
	connect(&m_tracker, &GameTracker::GameLoaded, this, &GameListModel::UpdateGame);
	connect(&m_tracker, &GameTracker::GameRemoved, this, &GameListModel::RemoveGame);
	connect(this, &GameListModel::DirectoryAdded, &m_tracker, &GameTracker::AddDirectory);
}

QVariant GameListModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
		return QVariant();

	QSharedPointer<GameFile> game = m_games[index.row()];
	if (index.column() == COL_PLATFORM && role == Qt::DecorationRole)
		return QVariant(Resources::GetPlatform(game->GetPlatform()));
	else if (index.column() == COL_PLATFORM && role == Qt::DisplayRole)
		return QVariant(game->GetPlatform());

	else if (index.column() == COL_TITLE && role == Qt::DecorationRole)
		return QVariant(game->GetBanner());
	else if (index.column() == COL_TITLE && role == Qt::DisplayRole)
		return QVariant(game->GetLongName());

	else if (index.column() == COL_ID && role == Qt::DisplayRole)
		return QVariant(game->GetUniqueID());

	else if (index.column() == COL_DESCRIPTION && role == Qt::DisplayRole)
		return QVariant(game->GetDescription());

	else if (index.column() == COL_MAKER && role == Qt::DisplayRole)
		return QVariant(game->GetCompany());

	// FIXME this sorts lexicographically, not by size.
	else if (index.column() == COL_SIZE && role == Qt::DisplayRole)
		return QVariant(FormatSize(game->GetFileSize()));

	else if (index.column() == COL_COUNTRY && role == Qt::DecorationRole)
		return QVariant(Resources::GetCountry(game->GetCountry()));
	else if (index.column() == COL_COUNTRY && role == Qt::DisplayRole)
		return QVariant(game->GetCountry());

	else if (index.column() == COL_RATING && role == Qt::DecorationRole)
		return QVariant(Resources::GetRating(game->GetRating()));
	else if (index.column() == COL_RATING && role == Qt::DisplayRole)
		return QVariant(game->GetRating());

	else if (index.column() == COL_LARGE_ICON && role == Qt::DecorationRole)
		return QVariant(game->GetBanner().scaled(144, 48));
	else if (index.column() == COL_LARGE_ICON && role == Qt::DisplayRole)
		return QVariant(game->GetLongName());

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
		case COL_ID: return QVariant(tr("ID"));
		case COL_DESCRIPTION: return QVariant(tr("Description"));
		case COL_MAKER: return QVariant(tr("Maker"));
		case COL_SIZE: return QVariant(tr("Size"));
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
