// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/GameList/TableProxyModel.h"
#include "DolphinQt2/GameList/GameListModel.h"

TableProxyModel::TableProxyModel(QObject* parent) : QSortFilterProxyModel(parent)
{
}

bool TableProxyModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
  GameListModel* glm = qobject_cast<GameListModel*>(sourceModel());
  return glm->ShouldDisplayGameListItem(source_row);
}
