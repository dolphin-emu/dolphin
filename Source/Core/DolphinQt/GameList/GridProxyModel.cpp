// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/GameList/GridProxyModel.h"

#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QSize>

#include "DolphinQt/GameList/GameListModel.h"

#include "Common/Logging/LogManager.h"

#include "Core/Config/UISettings.h"

#include "DolphinQt/GameList/GameTracker.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

#include "UICommon/GameFile.h"

const QSize LARGE_BANNER_SIZE(144, 48);

GridProxyModel::GridProxyModel(QObject* parent) : QSortFilterProxyModel(parent)
{
  setSortCaseSensitivity(Qt::CaseInsensitive);
  sort(GameListModel::COL_TITLE);

  placeholder_cover = Resources::GetScaledPixmap("Game_Cover_Placeholder");
}

void GridProxyModel::ConnectSignals()
{
  connect(&Settings::Instance(), &Settings::MetadataRefreshRequested, this,
          [this] { game_covers.clear(); });
  connect(&Settings::Instance(), &Settings::MetadataRefreshCompleted, this,
          &GridProxyModel::UpdateGameCovers);
  connect(static_cast<GameListModel*>(sourceModel())->GetGameTracker(), &GameTracker::GameLoaded,
          this, &GridProxyModel::UpdateGameCovers);
  connect(static_cast<GameListModel*>(sourceModel())->GetGameTracker(), &GameTracker::GameUpdated,
          this, [this](const std::shared_ptr<const UICommon::GameFile>& game) {
            game_covers.remove(game->GetFilePath());
            UpdateGameCovers();
          });
  connect(static_cast<GameListModel*>(sourceModel())->GetGameTracker(), &GameTracker::GameRemoved,
          this, [this](const std::string& path) { game_covers.remove(path); });
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
    auto game_file = model->GetGameFile(source_index.row());

    QSize size = Config::Get(Config::MAIN_USE_GAME_COVERS) ? QSize(160, 224) : LARGE_BANNER_SIZE;
    QPixmap pixmap(size * model->GetScale() * QPixmap().devicePixelRatio());

    if (!Config::Get(Config::MAIN_USE_GAME_COVERS) ||
        (!game_file->HasCoverImage() && game_file->TriedAllCoverImages()))
    {
      QPixmap banner = model
                           ->data(model->index(source_index.row(), GameListModel::COL_BANNER),
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
      pixmap = game_covers.value(game_file->GetFilePath(), placeholder_cover);

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

void GridProxyModel::UpdateGameCovers()
{
  auto* model = static_cast<GameListModel*>(sourceModel());

  for (int i = 0; i < sourceModel()->rowCount(); i++)
  {
    auto game_file = model->GetGameFile(i);

    if (game_covers.contains(game_file->GetFilePath()))
      continue;

    const auto& buffer = game_file->GetCoverImage().buffer;
    if (buffer.empty())
      continue;

    QPixmap pixmap = QPixmap::fromImage(QImage::fromData(
        reinterpret_cast<const unsigned char*>(&buffer[0]), static_cast<int>(buffer.size())));

    if (game_file->IsCoverImageFull())
    {
      if (pixmap.size() == QSize(1024, 680))
      {
        pixmap = pixmap.copy(539, 0, 485, 680);
      }
      else
      {
        ERROR_LOG(COMMON,
                  "Full cover for %s has incorrect size. Will be displayed without cropping.",
                  game_file->GetGameID().c_str());
      }
    }

    game_covers.insert(game_file->GetFilePath(), pixmap);
  }
}
