// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/GameList/ListProxyModel.h"
#include "DolphinQt/GameList/GameListModel.h"

ListProxyModel::ListProxyModel(QObject* parent) : QSortFilterProxyModel(parent)
{
  setDynamicSortFilter(true);
}

bool ListProxyModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
  GameListModel* glm = qobject_cast<GameListModel*>(sourceModel());
  return glm->ShouldDisplayGameListItem(source_row);
}

bool ListProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
  if (left.data(Qt::InitialSortOrderRole) != right.data(Qt::InitialSortOrderRole))
    return QSortFilterProxyModel::lessThan(right, left);

  // If two items are otherwise equal, compare them by their title
  const auto right_title =
      sourceModel()->index(right.row(), GameListModel::COL_TITLE).data().toString();
  const auto left_title =
      sourceModel()->index(left.row(), GameListModel::COL_TITLE).data().toString();

  if (sortOrder() == Qt::AscendingOrder)
    return left_title < right_title;

  return right_title < left_title;
}
