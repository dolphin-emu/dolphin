// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/GameList/GridProxyModel.h"

#include <QImage>
#include <QPixmap>
#include <QSize>

#include "DolphinQt/GameList/GameListModel.h"

#include "Core/Config/UISettings.h"

#include "UICommon/GameFile.h"

const QSize LARGE_BANNER_SIZE(144, 48);

GridProxyModel::GridProxyModel(QObject* parent) : QSortFilterProxyModel(parent)
{
  setSortCaseSensitivity(Qt::CaseInsensitive);
  sort(GameListModel::COL_TITLE);
}

QVariant GridProxyModel::data(const QModelIndex& i, int role) const
{
  QModelIndex source_index = mapToSource(i);
  if (role == Qt::DisplayRole)
  {
    return sourceModel()->data(sourceModel()->index(source_index.row(), GameListModel::COL_TITLE),
                               Qt::DisplayRole);
  }
  else if (role == Qt::DecorationRole)
  {
    auto* model = static_cast<GameListModel*>(sourceModel());

    const auto& buffer = model->GetGameFile(source_index.row())->GetCoverImage().buffer;

    QPixmap pixmap;

    if (buffer.empty() || !Config::Get(Config::MAIN_USE_GAME_COVERS))
    {
      pixmap = model
                   ->data(model->index(source_index.row(), GameListModel::COL_BANNER),
                          Qt::DecorationRole)
                   .value<QPixmap>();

      return pixmap.scaled(LARGE_BANNER_SIZE * model->GetScale() * pixmap.devicePixelRatio(),
                           Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    else
    {
      pixmap = QPixmap::fromImage(QImage::fromData(
          reinterpret_cast<const unsigned char*>(&buffer[0]), static_cast<int>(buffer.size())));

      return pixmap.scaled(QSize(160, 224) * model->GetScale() * pixmap.devicePixelRatio(),
                           Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
  }
  return QVariant();
}

bool GridProxyModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
  GameListModel* glm = qobject_cast<GameListModel*>(sourceModel());
  return glm->ShouldDisplayGameListItem(source_row);
}
