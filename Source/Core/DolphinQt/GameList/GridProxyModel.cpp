// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/GameList/GridProxyModel.h"

#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QSize>

#include "DolphinQt/GameList/GameListModel.h"

#include "Core/Config/UISettings.h"

#include "UICommon/GameFile.h"

constexpr QSize LARGE_BANNER_SIZE(144, 48);

GridProxyModel::GridProxyModel(QObject* parent) : QSortFilterProxyModel(parent)
{
  setSortCaseSensitivity(Qt::CaseInsensitive);
  sort(static_cast<int>(GameListModel::Column::Title));
}

QVariant GridProxyModel::data(const QModelIndex& i, const int role) const
{
  const QModelIndex source_index = mapToSource(i);
  if (role == Qt::DisplayRole)
  {
    return sourceModel()->data(
        sourceModel()->index(source_index.row(), static_cast<int>(GameListModel::Column::Title)),
        Qt::DisplayRole);
  }
  else if (role == Qt::DecorationRole)
  {
    const auto* model = static_cast<GameListModel*>(sourceModel());

    const auto& buffer = model->GetGameFile(source_index.row())->GetCoverImage().buffer;

    const QSize size = Get(Config::MAIN_USE_GAME_COVERS) ? QSize(160, 224) : LARGE_BANNER_SIZE;
    QPixmap pixmap(size * model->GetScale() * QPixmap().devicePixelRatio());

    if (buffer.empty() || !Get(Config::MAIN_USE_GAME_COVERS))
    {
      QPixmap banner = model
                           ->data(model->index(source_index.row(),
                                               static_cast<int>(GameListModel::Column::Banner)),
                                  Qt::DecorationRole)
                           .value<QPixmap>();

      banner = banner.scaled(pixmap.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

      pixmap.fill();

      QPainter painter(&pixmap);

      painter.drawPixmap(0, pixmap.height() / 2 - banner.height() / 2, banner.width(),
                         banner.height(), banner);

      return pixmap;
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

bool GridProxyModel::filterAcceptsRow(const int source_row, const QModelIndex& source_parent) const
{
  const GameListModel* glm = qobject_cast<GameListModel*>(sourceModel());
  return glm->ShouldDisplayGameListItem(source_row);
}
