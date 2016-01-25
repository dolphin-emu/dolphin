// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QSize>

#include "DolphinQt2/GameList/GameListModel.h"
#include "DolphinQt2/GameList/ListProxyModel.h"

static QSize LARGE_BANNER_SIZE(144, 48);

ListProxyModel::ListProxyModel(QObject* parent)
	: QSortFilterProxyModel(parent)
{
	setSortCaseSensitivity(Qt::CaseInsensitive);
	sort(GameListModel::COL_TITLE);
}

QVariant ListProxyModel::data(const QModelIndex& i, int role) const
{
	QModelIndex source_index = mapToSource(i);
	if (role == Qt::DisplayRole)
	{
		return sourceModel()->data(
			sourceModel()->index(source_index.row(), GameListModel::COL_TITLE),
			Qt::DisplayRole);
	}
	else if (role == Qt::DecorationRole)
	{
		return sourceModel()->data(
			sourceModel()->index(source_index.row(), GameListModel::COL_BANNER),
			Qt::DisplayRole).value<QPixmap>().scaled(
				LARGE_BANNER_SIZE,
				Qt::KeepAspectRatio,
				Qt::SmoothTransformation);
	}
	return QVariant();
}
