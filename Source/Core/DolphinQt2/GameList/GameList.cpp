// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QDesktopServices>
#include <QDir>
#include <QErrorMessage>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSettings>
#include <QUrl>

#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"

#include "DolphinQt2/Config/PropertiesDialog.h"
#include "DolphinQt2/GameList/GameList.h"
#include "DolphinQt2/GameList/GridProxyModel.h"
#include "DolphinQt2/GameList/ListProxyModel.h"
#include "DolphinQt2/QtUtils/DoubleClickEventFilter.h"
#include "DolphinQt2/Settings.h"
#include "DolphinQt2/WiiUpdate.h"

static bool CompressCB(const std::string&, float, void*);

GameList::GameList(QWidget* parent) : QStackedWidget(parent)
{
  m_model = Settings::Instance().GetGameListModel();
  m_list_proxy = new ListProxyModel(this);
  m_list_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
  m_list_proxy->setSortRole(Qt::InitialSortOrderRole);
  m_list_proxy->setSourceModel(m_model);
  m_grid_proxy = new GridProxyModel(this);
  m_grid_proxy->setSourceModel(m_model);

  MakeListView();
  MakeGridView();
  MakeEmptyView();

  connect(m_list, &QTableView::doubleClicked, this, &GameList::GameSelected);
  connect(m_grid, &QListView::doubleClicked, this, &GameList::GameSelected);
  connect(m_model, &QAbstractItemModel::rowsInserted, this, &GameList::ConsiderViewChange);
  connect(m_model, &QAbstractItemModel::rowsRemoved, this, &GameList::ConsiderViewChange);

  addWidget(m_list);
  addWidget(m_grid);
  addWidget(m_empty);
  m_prefer_list = Settings::Instance().GetPreferredView();
  ConsiderViewChange();
}

void GameList::MakeListView()
{
  m_list = new QTableView(this);
  m_list->setModel(m_list_proxy);

  m_list->setSelectionMode(QAbstractItemView::SingleSelection);
  m_list->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_list->setAlternatingRowColors(true);
  m_list->setShowGrid(false);
  m_list->setSortingEnabled(true);
  m_list->setCurrentIndex(QModelIndex());
  m_list->setContextMenuPolicy(Qt::CustomContextMenu);
  m_list->setWordWrap(false);

  connect(m_list, &QTableView::customContextMenuRequested, this, &GameList::ShowContextMenu);

  m_list->setColumnHidden(GameListModel::COL_PLATFORM, !SConfig::GetInstance().m_showSystemColumn);
  m_list->setColumnHidden(GameListModel::COL_ID, !SConfig::GetInstance().m_showIDColumn);
  m_list->setColumnHidden(GameListModel::COL_BANNER, !SConfig::GetInstance().m_showBannerColumn);
  m_list->setColumnHidden(GameListModel::COL_TITLE, !SConfig::GetInstance().m_showTitleColumn);
  m_list->setColumnHidden(GameListModel::COL_DESCRIPTION,
                          !SConfig::GetInstance().m_showDescriptionColumn);
  m_list->setColumnHidden(GameListModel::COL_MAKER, !SConfig::GetInstance().m_showMakerColumn);
  m_list->setColumnHidden(GameListModel::COL_SIZE, !SConfig::GetInstance().m_showSizeColumn);
  m_list->setColumnHidden(GameListModel::COL_COUNTRY, !SConfig::GetInstance().m_showRegionColumn);
  m_list->setColumnHidden(GameListModel::COL_RATING, !SConfig::GetInstance().m_showStateColumn);

  QHeaderView* hor_header = m_list->horizontalHeader();

  connect(hor_header, &QHeaderView::sortIndicatorChanged, this, &GameList::OnHeaderViewChanged);
  connect(hor_header, &QHeaderView::sectionResized, this, &GameList::OnHeaderViewChanged);
  connect(hor_header, &QHeaderView::sectionCountChanged, this, &GameList::OnHeaderViewChanged);
  connect(hor_header, &QHeaderView::sectionMoved, this, &GameList::OnHeaderViewChanged);

  hor_header->setSectionsMovable(true);
  hor_header->restoreState(QSettings().value(QStringLiteral("tableheader/state")).toByteArray());

  hor_header->setSectionResizeMode(GameListModel::COL_PLATFORM, QHeaderView::ResizeToContents);
  hor_header->setSectionResizeMode(GameListModel::COL_COUNTRY, QHeaderView::ResizeToContents);
  hor_header->setSectionResizeMode(GameListModel::COL_ID, QHeaderView::ResizeToContents);
  hor_header->setSectionResizeMode(GameListModel::COL_BANNER, QHeaderView::ResizeToContents);
  hor_header->setSectionResizeMode(GameListModel::COL_TITLE, QHeaderView::Stretch);
  hor_header->setSectionResizeMode(GameListModel::COL_MAKER, QHeaderView::Stretch);
  hor_header->setSectionResizeMode(GameListModel::COL_SIZE, QHeaderView::ResizeToContents);
  hor_header->setSectionResizeMode(GameListModel::COL_DESCRIPTION, QHeaderView::Stretch);
  hor_header->setSectionResizeMode(GameListModel::COL_RATING, QHeaderView::ResizeToContents);

  m_list->verticalHeader()->hide();
  m_list->setFrameStyle(QFrame::NoFrame);
}

void GameList::MakeEmptyView()
{
  m_empty = new QLabel(this);
  m_empty->setText(tr("Dolphin could not find any GameCube/Wii ISOs or WADs.\n"
                      "Double-click here to set a games directory..."));
  m_empty->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

  auto event_filter = new DoubleClickEventFilter{};
  m_empty->installEventFilter(event_filter);
  connect(event_filter, &DoubleClickEventFilter::doubleClicked, [this] {
    auto current_dir = QDir::currentPath();
    auto dir = QFileDialog::getExistingDirectory(this, tr("Select a Directory"), current_dir);
    if (!dir.isEmpty())
      Settings::Instance().AddPath(dir);
  });
}

void GameList::MakeGridView()
{
  m_grid = new QListView(this);
  m_grid->setModel(m_grid_proxy);
  m_grid->setViewMode(QListView::IconMode);
  m_grid->setResizeMode(QListView::Adjust);
  m_grid->setUniformItemSizes(true);
  m_grid->setContextMenuPolicy(Qt::CustomContextMenu);
  m_grid->setFrameStyle(QFrame::NoFrame);
  connect(m_grid, &QTableView::customContextMenuRequested, this, &GameList::ShowContextMenu);
}

void GameList::ShowContextMenu(const QPoint&)
{
  const auto game = GetSelectedGame();
  if (game.isNull())
    return;

  QMenu* menu = new QMenu(this);
  DiscIO::Platform platform = game->GetPlatformID();
  menu->addAction(tr("&Properties"), this, &GameList::OpenProperties);
  menu->addAction(tr("&Wiki"), this, &GameList::OpenWiki);
  menu->addSeparator();

  if (platform == DiscIO::Platform::GAMECUBE_DISC || platform == DiscIO::Platform::WII_DISC)
  {
    menu->addAction(tr("Set as &default ISO"), this, &GameList::SetDefaultISO);
    const auto blob_type = game->GetBlobType();

    if (blob_type == DiscIO::BlobType::GCZ)
      menu->addAction(tr("Decompress ISO..."), this, &GameList::CompressISO);
    else if (blob_type == DiscIO::BlobType::PLAIN)
      menu->addAction(tr("Compress ISO..."), this, &GameList::CompressISO);

    menu->addSeparator();
  }

  if (platform == DiscIO::Platform::WII_DISC)
  {
    menu->addAction(tr("Perform System Update"), [this] {
      WiiUpdate::PerformDiscUpdate(GetSelectedGame()->GetFilePath().toStdString(), this);
    });
    menu->setEnabled(!Core::IsRunning() || !SConfig::GetInstance().bWii);
  }

  if (platform == DiscIO::Platform::WII_WAD)
  {
    QAction* wad_install_action = new QAction(tr("Install to the NAND"), menu);
    QAction* wad_uninstall_action = new QAction(tr("Uninstall from the NAND"), menu);

    connect(wad_install_action, &QAction::triggered, this, &GameList::InstallWAD);
    connect(wad_uninstall_action, &QAction::triggered, this, &GameList::UninstallWAD);

    for (QAction* a : {wad_install_action, wad_uninstall_action})
    {
      connect(this, &GameList::EmulationStarted, a, [a] { a->setEnabled(false); });
      a->setEnabled(!Core::IsRunning());
      menu->addAction(a);
    }

    connect(this, &GameList::EmulationStopped, wad_install_action,
            [wad_install_action] { wad_install_action->setEnabled(true); });
    connect(this, &GameList::EmulationStopped, wad_uninstall_action, [wad_uninstall_action, game] {
      wad_uninstall_action->setEnabled(game->IsInstalled());
    });

    menu->addSeparator();
  }

  if (platform == DiscIO::Platform::WII_WAD || platform == DiscIO::Platform::WII_DISC)
  {
    menu->addAction(tr("Open Wii &save folder"), this, &GameList::OpenSaveFolder);
    menu->addAction(tr("Export Wii save (Experimental)"), this, &GameList::ExportWiiSave);
    menu->addSeparator();
  }

  menu->addAction(tr("Open &containing folder"), this, &GameList::OpenContainingFolder);
  menu->addAction(tr("Delete File..."), this, &GameList::DeleteFile);

  QAction* netplay_host = new QAction(tr("Host with NetPlay"), menu);

  connect(netplay_host, &QAction::triggered,
          [this, game] { emit NetPlayHost(game->GetUniqueID()); });

  connect(this, &GameList::EmulationStarted, netplay_host,
          [netplay_host] { netplay_host->setEnabled(false); });
  connect(this, &GameList::EmulationStopped, netplay_host,
          [netplay_host] { netplay_host->setEnabled(true); });
  netplay_host->setEnabled(!Core::IsRunning());

  menu->addAction(netplay_host);

  menu->exec(QCursor::pos());
}

void GameList::OpenProperties()
{
  PropertiesDialog* properties = new PropertiesDialog(this, *GetSelectedGame());
  properties->show();
}

void GameList::ExportWiiSave()
{
  QMessageBox result_dialog(this);

  const bool success = GetSelectedGame()->ExportWiiSave();

  result_dialog.setIcon(success ? QMessageBox::Information : QMessageBox::Critical);
  result_dialog.setText(success ? tr("Successfully exported save files") :
                                  tr("Failed to export save files!"));
  result_dialog.exec();
}

void GameList::OpenWiki()
{
  QString game_id = GetSelectedGame()->GetGameID();
  QString url = QStringLiteral("https://wiki.dolphin-emu.org/index.php?title=").append(game_id);
  QDesktopServices::openUrl(QUrl(url));
}

void GameList::CompressISO()
{
  auto file = GetSelectedGame();
  const auto original_path = file->GetFilePath();

  const bool compressed = (file->GetBlobType() == DiscIO::BlobType::GCZ);

  if (!compressed && file->GetPlatformID() == DiscIO::Platform::WII_DISC)
  {
    QMessageBox wii_warning(this);
    wii_warning.setIcon(QMessageBox::Warning);
    wii_warning.setText(tr("Are you sure?"));
    wii_warning.setInformativeText(
        tr("Compressing a Wii disc image will irreversibly change the compressed copy by removing "
           "padding data. Your disc image will still work. Continue?"));
    wii_warning.setStandardButtons(QMessageBox::Yes | QMessageBox::No);

    if (wii_warning.exec() == QMessageBox::No)
      return;
  }

  QString dst_path = QFileDialog::getSaveFileName(
      this, compressed ? tr("Select where you want to save the decompressed image") :
                         tr("Select where you want to save the compressed image"),
      QFileInfo(GetSelectedGame()->GetFilePath())
          .dir()
          .absoluteFilePath(file->GetGameID())
          .append(compressed ? QStringLiteral(".gcm") : QStringLiteral(".gcz")),
      compressed ? tr("Uncompressed GC/Wii images (*.iso *.gcm)") :
                   tr("Compressed GC/Wii images (*.gcz)"));

  if (dst_path.isEmpty())
    return;

  QProgressDialog progress_dialog(compressed ? tr("Decompressing...") : tr("Compressing..."),
                                  tr("Abort"), 0, 100, this);
  progress_dialog.setWindowModality(Qt::WindowModal);

  bool good;

  if (compressed)
  {
    good = DiscIO::DecompressBlobToFile(original_path.toStdString(), dst_path.toStdString(),
                                        &CompressCB, &progress_dialog);
  }
  else
  {
    good = DiscIO::CompressFileToBlob(original_path.toStdString(), dst_path.toStdString(),
                                      file->GetPlatformID() == DiscIO::Platform::WII_DISC ? 1 : 0,
                                      16384, &CompressCB, &progress_dialog);
  }

  if (good)
  {
    QMessageBox(QMessageBox::Information, tr("Success!"), tr("Successfully compressed image."),
                QMessageBox::Ok, this)
        .exec();
  }
  else
  {
    QErrorMessage(this).showMessage(tr("Dolphin failed to complete the requested action."));
  }
}

void GameList::InstallWAD()
{
  QMessageBox result_dialog(this);

  const bool success = GetSelectedGame()->Install();

  result_dialog.setIcon(success ? QMessageBox::Information : QMessageBox::Critical);
  result_dialog.setText(success ? tr("Successfully installed this title to the NAND.") :
                                  tr("Failed to install this title to the NAND."));
  result_dialog.exec();
}

void GameList::UninstallWAD()
{
  QMessageBox warning_dialog(this);

  warning_dialog.setIcon(QMessageBox::Information);
  warning_dialog.setText(tr("Uninstalling the WAD will remove the currently installed version of "
                            "this title from the NAND without deleting its save data. Continue?"));
  warning_dialog.setStandardButtons(QMessageBox::No | QMessageBox::Yes);

  if (warning_dialog.exec() == QMessageBox::No)
    return;

  QMessageBox result_dialog(this);

  const bool success = GetSelectedGame()->Uninstall();

  result_dialog.setIcon(success ? QMessageBox::Information : QMessageBox::Critical);
  result_dialog.setText(success ? tr("Successfully removed this title from the NAND.") :
                                  tr("Failed to remove this title from the NAND."));
  result_dialog.exec();
}

void GameList::SetDefaultISO()
{
  SConfig::GetInstance().m_strDefaultISO = GetSelectedGame()->GetFilePath().toStdString();
}

void GameList::OpenContainingFolder()
{
  QUrl url = QUrl::fromLocalFile(QFileInfo(GetSelectedGame()->GetFilePath()).dir().absolutePath());
  QDesktopServices::openUrl(url);
}

void GameList::OpenSaveFolder()
{
  QUrl url = QUrl::fromLocalFile(GetSelectedGame()->GetWiiFSPath());
  QDesktopServices::openUrl(url);
}

void GameList::DeleteFile()
{
  const auto game = GetSelectedGame()->GetFilePath();
  QMessageBox confirm_dialog(this);

  confirm_dialog.setIcon(QMessageBox::Warning);
  confirm_dialog.setText(tr("Are you sure you want to delete this file?"));
  confirm_dialog.setInformativeText(tr("This cannot be undone!"));
  confirm_dialog.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);

  if (confirm_dialog.exec() == QMessageBox::Yes)
  {
    bool deletion_successful = false;

    while (!deletion_successful)
    {
      deletion_successful = File::Delete(game.toStdString());

      if (deletion_successful)
      {
        m_model->RemoveGame(game);
      }
      else
      {
        QMessageBox error_dialog(this);

        error_dialog.setIcon(QMessageBox::Critical);
        error_dialog.setText(tr("Failed to delete the selected file."));
        error_dialog.setInformativeText(tr("Check whether you have the permissions required to "
                                           "delete the file or whether it's still in use."));
        error_dialog.setStandardButtons(QMessageBox::Retry | QMessageBox::Abort);

        if (error_dialog.exec() == QMessageBox::Abort)
          break;
      }
    }
  }
}

QSharedPointer<GameFile> GameList::GetSelectedGame() const
{
  QAbstractItemView* view;
  QSortFilterProxyModel* proxy;
  if (currentWidget() == m_list)
  {
    view = m_list;
    proxy = m_list_proxy;
  }
  else
  {
    view = m_grid;
    proxy = m_grid_proxy;
  }
  QItemSelectionModel* sel_model = view->selectionModel();
  if (sel_model->hasSelection())
  {
    QModelIndex model_index = proxy->mapToSource(sel_model->selectedIndexes()[0]);
    return m_model->GetGameFile(model_index.row());
  }
  return {};
}

void GameList::SetPreferredView(bool list)
{
  m_prefer_list = list;
  Settings::Instance().SetPreferredView(list);
  ConsiderViewChange();
}

void GameList::ConsiderViewChange()
{
  if (m_model->rowCount(QModelIndex()) > 0)
  {
    if (m_prefer_list)
      setCurrentWidget(m_list);
    else
      setCurrentWidget(m_grid);
  }
  else
  {
    setCurrentWidget(m_empty);
  }
}
void GameList::keyReleaseEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Return)
    emit GameSelected();
  else
    QStackedWidget::keyReleaseEvent(event);
}

void GameList::OnColumnVisibilityToggled(const QString& row, bool visible)
{
  static const QMap<QString, int> rowname_to_col_index = {
      {tr("Banner"), GameListModel::COL_BANNER},
      {tr("Country"), GameListModel::COL_COUNTRY},
      {tr("Description"), GameListModel::COL_DESCRIPTION},
      {tr("ID"), GameListModel::COL_ID},
      {tr("Maker"), GameListModel::COL_MAKER},
      {tr("Platform"), GameListModel::COL_PLATFORM},
      {tr("Size"), GameListModel::COL_SIZE},
      {tr("Title"), GameListModel::COL_TITLE},
      {tr("State"), GameListModel::COL_RATING}};

  m_list->setColumnHidden(rowname_to_col_index[row], !visible);
}

void GameList::OnGameListVisibilityChanged()
{
  m_list_proxy->invalidate();
  m_grid_proxy->invalidate();
}

static bool CompressCB(const std::string& text, float percent, void* ptr)
{
  if (ptr == nullptr)
    return false;
  auto* progress_dialog = static_cast<QProgressDialog*>(ptr);

  progress_dialog->setValue(percent * 100);
  return !progress_dialog->wasCanceled();
}

void GameList::OnHeaderViewChanged()
{
  QSettings().setValue(QStringLiteral("tableheader/state"),
                       m_list->horizontalHeader()->saveState());
}
