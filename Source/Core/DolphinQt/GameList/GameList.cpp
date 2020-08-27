// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/GameList/GameList.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <QDesktopServices>
#include <QDir>
#include <QErrorMessage>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QMap>
#include <QMenu>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QUrl>

#include "Common/FileUtil.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/WiiSave.h"
#include "Core/WiiUtils.h"

#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"

#include "DolphinQt/Config/PropertiesDialog.h"
#include "DolphinQt/ConvertDialog.h"
#include "DolphinQt/GameList/GameListModel.h"
#include "DolphinQt/GameList/GridProxyModel.h"
#include "DolphinQt/GameList/ListProxyModel.h"
#include "DolphinQt/MenuBar.h"
#include "DolphinQt/QtUtils/DoubleClickEventFilter.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/ParallelProgressDialog.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/WiiUpdate.h"

#include "UICommon/GameFile.h"

GameList::GameList(QWidget* parent) : QStackedWidget(parent)
{
  m_model = Settings::Instance().GetGameListModel();
  m_list_proxy = new ListProxyModel(this);
  m_list_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
  m_list_proxy->setSortRole(GameListModel::SORT_ROLE);
  m_list_proxy->setSourceModel(m_model);
  m_grid_proxy = new GridProxyModel(this);
  m_grid_proxy->setSourceModel(m_model);

  MakeListView();
  MakeGridView();
  MakeEmptyView();

  if (Settings::GetQSettings().contains(QStringLiteral("gridview/scale")))
    m_model->SetScale(Settings::GetQSettings().value(QStringLiteral("gridview/scale")).toFloat());

  connect(m_list, &QTableView::doubleClicked, this, &GameList::GameSelected);
  connect(m_grid, &QListView::doubleClicked, this, &GameList::GameSelected);
  connect(m_model, &QAbstractItemModel::rowsInserted, this, &GameList::ConsiderViewChange);
  connect(m_model, &QAbstractItemModel::rowsRemoved, this, &GameList::ConsiderViewChange);

  addWidget(m_list);
  addWidget(m_grid);
  addWidget(m_empty);
  m_prefer_list = Settings::Instance().GetPreferredView();
  ConsiderViewChange();

  auto* zoom_in = new QShortcut(QKeySequence::ZoomIn, this);
  auto* zoom_out = new QShortcut(QKeySequence::ZoomOut, this);

  connect(zoom_in, &QShortcut::activated, this, &GameList::ZoomIn);
  connect(zoom_out, &QShortcut::activated, this, &GameList::ZoomOut);

  connect(&Settings::Instance(), &Settings::MetadataRefreshCompleted, this,
          [this] { m_grid_proxy->invalidate(); });
}

void GameList::PurgeCache()
{
  m_model->PurgeCache();
}

void GameList::MakeListView()
{
  m_list = new QTableView(this);
  m_list->setModel(m_list_proxy);

  m_list->setTabKeyNavigation(false);
  m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_list->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_list->setAlternatingRowColors(true);
  m_list->setShowGrid(false);
  m_list->setSortingEnabled(true);
  m_list->setCurrentIndex(QModelIndex());
  m_list->setContextMenuPolicy(Qt::CustomContextMenu);
  m_list->setWordWrap(false);
  // Have 1 pixel of padding above and below the 32 pixel banners.
  m_list->verticalHeader()->setDefaultSectionSize(32 + 2);

  connect(m_list, &QTableView::customContextMenuRequested, this, &GameList::ShowContextMenu);
  connect(m_list->selectionModel(), &QItemSelectionModel::selectionChanged,
          [this](const QItemSelection&, const QItemSelection&) {
            emit SelectionChanged(GetSelectedGame());
          });

  QHeaderView* hor_header = m_list->horizontalHeader();

  hor_header->restoreState(
      Settings::GetQSettings().value(QStringLiteral("tableheader/state")).toByteArray());

  hor_header->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(hor_header, &QWidget::customContextMenuRequested, this, &GameList::ShowHeaderContextMenu);

  connect(hor_header, &QHeaderView::sortIndicatorChanged, this, &GameList::OnHeaderViewChanged);
  connect(hor_header, &QHeaderView::sectionCountChanged, this, &GameList::OnHeaderViewChanged);
  connect(hor_header, &QHeaderView::sectionMoved, this, &GameList::OnHeaderViewChanged);
  connect(hor_header, &QHeaderView::sectionResized, this, &GameList::OnSectionResized);

  if (!Settings::GetQSettings().contains(QStringLiteral("tableheader/state")))
    m_list->sortByColumn(GameListModel::COL_TITLE, Qt::AscendingOrder);

  hor_header->setSectionResizeMode(GameListModel::COL_PLATFORM, QHeaderView::Fixed);
  hor_header->setSectionResizeMode(GameListModel::COL_BANNER, QHeaderView::Fixed);
  hor_header->setSectionResizeMode(GameListModel::COL_TITLE, QHeaderView::Interactive);
  hor_header->setSectionResizeMode(GameListModel::COL_DESCRIPTION, QHeaderView::Interactive);
  hor_header->setSectionResizeMode(GameListModel::COL_MAKER, QHeaderView::Interactive);
  hor_header->setSectionResizeMode(GameListModel::COL_ID, QHeaderView::Fixed);
  hor_header->setSectionResizeMode(GameListModel::COL_COUNTRY, QHeaderView::Fixed);
  hor_header->setSectionResizeMode(GameListModel::COL_SIZE, QHeaderView::Fixed);
  hor_header->setSectionResizeMode(GameListModel::COL_FILE_NAME, QHeaderView::Interactive);
  hor_header->setSectionResizeMode(GameListModel::COL_FILE_PATH, QHeaderView::Interactive);
  hor_header->setSectionResizeMode(GameListModel::COL_FILE_FORMAT, QHeaderView::Fixed);
  hor_header->setSectionResizeMode(GameListModel::COL_BLOCK_SIZE, QHeaderView::Fixed);
  hor_header->setSectionResizeMode(GameListModel::COL_COMPRESSION, QHeaderView::Fixed);
  hor_header->setSectionResizeMode(GameListModel::COL_TAGS, QHeaderView::Interactive);

  // There's some odd platform-specific behavior with default minimum section size
  hor_header->setMinimumSectionSize(38);

  // Cells have 3 pixels of padding, so the width of these needs to be image width + 6. Banners are
  // 96 pixels wide, platform and country icons are 32 pixels wide.
  m_list->setColumnWidth(GameListModel::COL_BANNER, 102);
  m_list->setColumnWidth(GameListModel::COL_PLATFORM, 38);
  m_list->setColumnWidth(GameListModel::COL_COUNTRY, 38);
  m_list->setColumnWidth(GameListModel::COL_SIZE, 85);
  m_list->setColumnWidth(GameListModel::COL_ID, 70);

  UpdateColumnVisibility();

  m_list->verticalHeader()->hide();
  m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_list->setFrameStyle(QFrame::NoFrame);

  hor_header->setSectionsMovable(true);
  hor_header->setHighlightSections(false);
}

GameList::~GameList()
{
  Settings::GetQSettings().setValue(QStringLiteral("tableheader/state"),
                                    m_list->horizontalHeader()->saveState());
  Settings::GetQSettings().setValue(QStringLiteral("gridview/scale"), m_model->GetScale());
}

void GameList::UpdateColumnVisibility()
{
  m_list->setColumnHidden(GameListModel::COL_PLATFORM, !SConfig::GetInstance().m_showSystemColumn);
  m_list->setColumnHidden(GameListModel::COL_BANNER, !SConfig::GetInstance().m_showBannerColumn);
  m_list->setColumnHidden(GameListModel::COL_TITLE, !SConfig::GetInstance().m_showTitleColumn);
  m_list->setColumnHidden(GameListModel::COL_DESCRIPTION,
                          !SConfig::GetInstance().m_showDescriptionColumn);
  m_list->setColumnHidden(GameListModel::COL_MAKER, !SConfig::GetInstance().m_showMakerColumn);
  m_list->setColumnHidden(GameListModel::COL_ID, !SConfig::GetInstance().m_showIDColumn);
  m_list->setColumnHidden(GameListModel::COL_COUNTRY, !SConfig::GetInstance().m_showRegionColumn);
  m_list->setColumnHidden(GameListModel::COL_SIZE, !SConfig::GetInstance().m_showSizeColumn);
  m_list->setColumnHidden(GameListModel::COL_FILE_NAME,
                          !SConfig::GetInstance().m_showFileNameColumn);
  m_list->setColumnHidden(GameListModel::COL_FILE_PATH,
                          !SConfig::GetInstance().m_showFilePathColumn);
  m_list->setColumnHidden(GameListModel::COL_FILE_FORMAT,
                          !SConfig::GetInstance().m_showFileFormatColumn);
  m_list->setColumnHidden(GameListModel::COL_BLOCK_SIZE,
                          !SConfig::GetInstance().m_showBlockSizeColumn);
  m_list->setColumnHidden(GameListModel::COL_COMPRESSION,
                          !SConfig::GetInstance().m_showCompressionColumn);
  m_list->setColumnHidden(GameListModel::COL_TAGS, !SConfig::GetInstance().m_showTagsColumn);
}

void GameList::MakeEmptyView()
{
  m_empty = new QLabel(this);
  m_empty->setText(tr("Dolphin could not find any GameCube/Wii ISOs or WADs.\n"
                      "Double-click here to set a games directory..."));
  m_empty->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

  auto event_filter = new DoubleClickEventFilter{m_empty};
  m_empty->installEventFilter(event_filter);
  connect(event_filter, &DoubleClickEventFilter::doubleClicked, [this] {
    auto current_dir = QDir::currentPath();
    auto dir = QFileDialog::getExistingDirectory(this, tr("Select a Directory"), current_dir);
    if (!dir.isEmpty())
      Settings::Instance().AddPath(dir);
  });
}

void GameList::resizeEvent(QResizeEvent* event)
{
  OnHeaderViewChanged();
}

void GameList::MakeGridView()
{
  m_grid = new QListView(this);
  m_grid->setModel(m_grid_proxy);
  m_grid->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_grid->setViewMode(QListView::IconMode);
  m_grid->setResizeMode(QListView::Adjust);
  m_grid->setUniformItemSizes(true);
  m_grid->setContextMenuPolicy(Qt::CustomContextMenu);
  m_grid->setFrameStyle(QFrame::NoFrame);
  connect(m_grid, &QTableView::customContextMenuRequested, this, &GameList::ShowContextMenu);
  connect(m_grid->selectionModel(), &QItemSelectionModel::selectionChanged,
          [this](const QItemSelection&, const QItemSelection&) {
            emit SelectionChanged(GetSelectedGame());
          });
}

void GameList::ShowHeaderContextMenu(const QPoint& pos)
{
  const MenuBar* const menu_bar = MenuBar::GetMenuBar();
  if (!menu_bar)
    return;

  QMenu* const list_columns_menu = menu_bar->GetListColumnsMenu();
  if (!list_columns_menu)
    return;

  const QWidget* const widget = qobject_cast<QWidget*>(sender());
  list_columns_menu->exec(widget ? widget->mapToGlobal(pos) : pos);
}

void GameList::ShowContextMenu(const QPoint&)
{
  if (!GetSelectedGame())
    return;

  QMenu* menu = new QMenu(this);

  const auto can_convert = [](const std::shared_ptr<const UICommon::GameFile>& game) {
    // Converting from TGC is temporarily disabled because PR #8738 was merged prematurely.
    // The TGC check will be removed by PR #8644.
    return DiscIO::IsDisc(game->GetPlatform()) && game->IsVolumeSizeAccurate() &&
           game->GetBlobType() != DiscIO::BlobType::TGC;
  };

  if (HasMultipleSelected())
  {
    const auto selected_games = GetSelectedGames();

    if (std::all_of(selected_games.begin(), selected_games.end(), can_convert))
    {
      menu->addAction(tr("Convert Selected Files..."), this, &GameList::ConvertFile);
      menu->addSeparator();
    }

    if (std::all_of(selected_games.begin(), selected_games.end(),
                    [](const auto& game) { return DiscIO::IsWii(game->GetPlatform()); }))
    {
      menu->addAction(tr("Export Wii Saves"), this, &GameList::ExportWiiSave);
      menu->addSeparator();
    }

    menu->addAction(tr("Delete Selected Files..."), this, &GameList::DeleteFile);
  }
  else
  {
    const auto game = GetSelectedGame();
    DiscIO::Platform platform = game->GetPlatform();

    if (platform != DiscIO::Platform::ELFOrDOL)
    {
      menu->addAction(tr("&Properties"), this, &GameList::OpenProperties);
      menu->addAction(tr("&Wiki"), this, &GameList::OpenWiki);

      menu->addSeparator();
    }

    if (DiscIO::IsDisc(platform))
    {
      menu->addAction(tr("Set as &Default ISO"), this, &GameList::SetDefaultISO);

      if (can_convert(game))
        menu->addAction(tr("Convert File..."), this, &GameList::ConvertFile);

      QAction* change_disc = menu->addAction(tr("Change &Disc"), this, &GameList::ChangeDisc);

      connect(&Settings::Instance(), &Settings::EmulationStateChanged, change_disc,
              [change_disc] { change_disc->setEnabled(Core::IsRunning()); });
      change_disc->setEnabled(Core::IsRunning());

      menu->addSeparator();
    }

    if (platform == DiscIO::Platform::WiiDisc)
    {
      auto* perform_disc_update = menu->addAction(tr("Perform System Update"), this,
                                                  [this, file_path = game->GetFilePath()] {
                                                    WiiUpdate::PerformDiscUpdate(file_path, this);
                                                    // Since the update may have installed a newer
                                                    // system menu, trigger a refresh.
                                                    Settings::Instance().NANDRefresh();
                                                  });
      perform_disc_update->setEnabled(!Core::IsRunning() || !SConfig::GetInstance().bWii);
    }

    if (platform == DiscIO::Platform::WiiWAD)
    {
      QAction* wad_install_action = new QAction(tr("Install to the NAND"), menu);
      QAction* wad_uninstall_action = new QAction(tr("Uninstall from the NAND"), menu);

      connect(wad_install_action, &QAction::triggered, this, &GameList::InstallWAD);
      connect(wad_uninstall_action, &QAction::triggered, this, &GameList::UninstallWAD);

      for (QAction* a : {wad_install_action, wad_uninstall_action})
      {
        a->setEnabled(!Core::IsRunning());
        menu->addAction(a);
      }
      if (!Core::IsRunning())
        wad_uninstall_action->setEnabled(WiiUtils::IsTitleInstalled(game->GetTitleID()));

      connect(&Settings::Instance(), &Settings::EmulationStateChanged, menu,
              [=](Core::State state) {
                wad_install_action->setEnabled(state == Core::State::Uninitialized);
                wad_uninstall_action->setEnabled(state == Core::State::Uninitialized &&
                                                 WiiUtils::IsTitleInstalled(game->GetTitleID()));
              });

      menu->addSeparator();
    }

    if (platform == DiscIO::Platform::WiiWAD || platform == DiscIO::Platform::WiiDisc)
    {
      menu->addAction(tr("Open Wii &Save Folder"), this, &GameList::OpenWiiSaveFolder);
      menu->addAction(tr("Export Wii Save"), this, &GameList::ExportWiiSave);
      menu->addSeparator();
    }

    if (platform == DiscIO::Platform::GameCubeDisc)
    {
      menu->addAction(tr("Open GameCube &Save Folder"), this, &GameList::OpenGCSaveFolder);
      menu->addSeparator();
    }

    menu->addAction(tr("Open &Containing Folder"), this, &GameList::OpenContainingFolder);
    menu->addAction(tr("Delete File..."), this, &GameList::DeleteFile);

    menu->addSeparator();

    auto* model = Settings::Instance().GetGameListModel();

    auto* tags_menu = menu->addMenu(tr("Tags"));

    auto path = game->GetFilePath();
    auto game_tags = model->GetGameTags(path);

    for (const auto& tag : model->GetAllTags())
    {
      auto* tag_action = tags_menu->addAction(tag);

      tag_action->setCheckable(true);
      tag_action->setChecked(game_tags.contains(tag));

      connect(tag_action, &QAction::toggled, [path, tag, model](bool checked) {
        if (!checked)
          model->RemoveGameTag(path, tag);
        else
          model->AddGameTag(path, tag);
      });
    }

    menu->addAction(tr("New Tag..."), this, &GameList::NewTag);
    menu->addAction(tr("Remove Tag..."), this, &GameList::DeleteTag);

    menu->addSeparator();

    QAction* netplay_host = new QAction(tr("Host with NetPlay"), menu);

    connect(netplay_host, &QAction::triggered, [this, game] {
      emit NetPlayHost(QString::fromStdString(game->GetUniqueIdentifier()));
    });

    connect(&Settings::Instance(), &Settings::EmulationStateChanged, menu, [=](Core::State state) {
      netplay_host->setEnabled(state == Core::State::Uninitialized);
    });
    netplay_host->setEnabled(!Core::IsRunning());

    menu->addAction(netplay_host);
  }

  menu->exec(QCursor::pos());
}

void GameList::OpenProperties()
{
  const auto game = GetSelectedGame();
  if (!game)
    return;

  PropertiesDialog* properties = new PropertiesDialog(this, *game);
  // Since the properties dialog locks the game file, it's important to free it as soon as it's
  // closed so that the file can be moved or deleted.
  properties->setAttribute(Qt::WA_DeleteOnClose, true);

  connect(properties, &PropertiesDialog::OpenGeneralSettings, this, &GameList::OpenGeneralSettings);

  properties->show();
}

void GameList::ExportWiiSave()
{
  const QString export_dir = QFileDialog::getExistingDirectory(
      this, tr("Select Export Directory"), QString::fromStdString(File::GetUserPath(D_USER_IDX)),
      QFileDialog::ShowDirsOnly);
  if (export_dir.isEmpty())
    return;

  QList<std::string> failed;
  for (const auto& game : GetSelectedGames())
  {
    if (!WiiSave::Export(game->GetTitleID(), export_dir.toStdString()))
      failed.push_back(game->GetName(UICommon::GameFile::Variant::LongAndPossiblyCustom));
  }

  if (!failed.isEmpty())
  {
    QString failed_str;
    for (const std::string& str : failed)
      failed_str.append(QLatin1Char{'\n'}).append(QString::fromStdString(str));
    ModalMessageBox::critical(this, tr("Save Export"),
                              tr("Failed to export the following save files:") + failed_str);
  }
  else
  {
    ModalMessageBox::information(this, tr("Save Export"), tr("Successfully exported save files"));
  }
}

void GameList::OpenWiki()
{
  const auto game = GetSelectedGame();
  if (!game)
    return;

  QString game_id = QString::fromStdString(game->GetGameID());
  QString url = QStringLiteral("https://wiki.dolphin-emu.org/index.php?title=").append(game_id);
  QDesktopServices::openUrl(QUrl(url));
}

void GameList::ConvertFile()
{
  auto games = GetSelectedGames();
  if (games.empty())
    return;

  ConvertDialog dialog{std::move(games), this};
  dialog.exec();
}

void GameList::InstallWAD()
{
  const auto game = GetSelectedGame();
  if (!game)
    return;

  ModalMessageBox result_dialog(this);

  const bool success = WiiUtils::InstallWAD(game->GetFilePath());

  result_dialog.setIcon(success ? QMessageBox::Information : QMessageBox::Critical);
  result_dialog.setWindowTitle(success ? tr("Success") : tr("Failure"));
  result_dialog.setText(success ? tr("Successfully installed this title to the NAND.") :
                                  tr("Failed to install this title to the NAND."));
  result_dialog.exec();
}

void GameList::UninstallWAD()
{
  const auto game = GetSelectedGame();
  if (!game)
    return;

  ModalMessageBox warning_dialog(this);

  warning_dialog.setIcon(QMessageBox::Information);
  warning_dialog.setWindowTitle(tr("Confirm"));
  warning_dialog.setText(tr("Uninstalling the WAD will remove the currently installed version of "
                            "this title from the NAND without deleting its save data. Continue?"));
  warning_dialog.setStandardButtons(QMessageBox::No | QMessageBox::Yes);

  if (warning_dialog.exec() == QMessageBox::No)
    return;

  ModalMessageBox result_dialog(this);

  const bool success = WiiUtils::UninstallTitle(game->GetTitleID());

  result_dialog.setIcon(success ? QMessageBox::Information : QMessageBox::Critical);
  result_dialog.setWindowTitle(success ? tr("Success") : tr("Failure"));
  result_dialog.setText(success ? tr("Successfully removed this title from the NAND.") :
                                  tr("Failed to remove this title from the NAND."));
  result_dialog.exec();
}

void GameList::SetDefaultISO()
{
  const auto game = GetSelectedGame();
  if (!game)
    return;

  Settings::Instance().SetDefaultGame(
      QDir::toNativeSeparators(QString::fromStdString(game->GetFilePath())));
}

void GameList::OpenContainingFolder()
{
  const auto game = GetSelectedGame();
  if (!game)
    return;

  QUrl url = QUrl::fromLocalFile(
      QFileInfo(QString::fromStdString(game->GetFilePath())).dir().absolutePath());
  QDesktopServices::openUrl(url);
}

void GameList::OpenWiiSaveFolder()
{
  const auto game = GetSelectedGame();
  if (!game)
    return;

  QUrl url = QUrl::fromLocalFile(QString::fromStdString(game->GetWiiFSPath()));
  QDesktopServices::openUrl(url);
}

void GameList::OpenGCSaveFolder()
{
  const auto game = GetSelectedGame();
  if (!game)
    return;

  bool found = false;

  for (int i = 0; i < 2; i++)
  {
    QUrl url;
    switch (SConfig::GetInstance().m_EXIDevice[i])
    {
    case ExpansionInterface::EXIDEVICE_MEMORYCARDFOLDER:
    {
      std::string path = StringFromFormat("%s/%s/%s", File::GetUserPath(D_GCUSER_IDX).c_str(),
                                          SConfig::GetDirectoryForRegion(game->GetRegion()),
                                          i == 0 ? "Card A" : "Card B");

      std::string override_path = i == 0 ? Config::Get(Config::MAIN_GCI_FOLDER_A_PATH_OVERRIDE) :
                                           Config::Get(Config::MAIN_GCI_FOLDER_B_PATH_OVERRIDE);

      if (!override_path.empty())
        path = override_path;

      QDir dir(QString::fromStdString(path));

      if (!dir.entryList({QStringLiteral("%1-%2-*.gci")
                              .arg(QString::fromStdString(game->GetMakerID()))
                              .arg(QString::fromStdString(game->GetGameID().substr(0, 4)))})
               .empty())
      {
        url = QUrl::fromLocalFile(dir.absolutePath());
      }
      break;
    }
    case ExpansionInterface::EXIDEVICE_MEMORYCARD:
    {
      std::string memcard_path = i == 0 ? Config::Get(Config::MAIN_MEMCARD_A_PATH) :
                                          Config::Get(Config::MAIN_MEMCARD_B_PATH);

      std::string memcard_dir;

      SplitPath(memcard_path, &memcard_dir, nullptr, nullptr);
      url = QUrl::fromLocalFile(QString::fromStdString(memcard_dir));
      break;
    }
    default:
      break;
    }

    found |= !url.isEmpty();

    if (!url.isEmpty())
      QDesktopServices::openUrl(url);
  }

  if (!found)
    ModalMessageBox::information(this, tr("Information"), tr("No save data found."));
}

void GameList::DeleteFile()
{
  ModalMessageBox confirm_dialog(this);

  confirm_dialog.setIcon(QMessageBox::Warning);
  confirm_dialog.setWindowTitle(tr("Confirm"));
  confirm_dialog.setText(tr("Are you sure you want to delete this file?"));
  confirm_dialog.setInformativeText(tr("This cannot be undone!"));
  confirm_dialog.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);

  if (confirm_dialog.exec() == QMessageBox::Yes)
  {
    for (const auto& game : GetSelectedGames())
    {
      bool deletion_successful = false;

      while (!deletion_successful)
      {
        deletion_successful = File::Delete(game->GetFilePath());

        if (deletion_successful)
        {
          m_model->RemoveGame(game->GetFilePath());
        }
        else
        {
          ModalMessageBox error_dialog(this);

          error_dialog.setIcon(QMessageBox::Critical);
          error_dialog.setWindowTitle(tr("Failure"));
          error_dialog.setText(tr("Failed to delete the selected file."));
          error_dialog.setInformativeText(tr("Check whether you have the permissions required to "
                                             "delete the file or whether it's still in use."));
          error_dialog.setStandardButtons(QMessageBox::Retry | QMessageBox::Abort);

          if (error_dialog.exec() == QMessageBox::Abort)
            break;
        }
      }

      if (!deletion_successful)
        break;  // Something is wrong, so we should abort the whole thing
    }
  }
}

void GameList::ChangeDisc()
{
  const auto game = GetSelectedGame();
  if (!game)
    return;

  Core::RunAsCPUThread([file_path = game->GetFilePath()] { DVDInterface::ChangeDisc(file_path); });
}

std::shared_ptr<const UICommon::GameFile> GameList::GetSelectedGame() const
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

QList<std::shared_ptr<const UICommon::GameFile>> GameList::GetSelectedGames() const
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
  QList<std::shared_ptr<const UICommon::GameFile>> selected_list;
  QItemSelectionModel* sel_model = view->selectionModel();
  if (sel_model->hasSelection())
  {
    QModelIndexList index_list =
        currentWidget() == m_list ? sel_model->selectedRows() : sel_model->selectedIndexes();
    for (const auto& index : index_list)
    {
      QModelIndex model_index = proxy->mapToSource(index);
      selected_list.push_back(m_model->GetGameFile(model_index.row()));
    }
  }
  return selected_list;
}

bool GameList::HasMultipleSelected() const
{
  return currentWidget() == m_list ? m_list->selectionModel()->selectedRows().size() > 1 :
                                     m_grid->selectionModel()->selectedIndexes().size() > 1;
}

std::shared_ptr<const UICommon::GameFile> GameList::FindGame(const std::string& path) const
{
  return m_model->FindGame(path);
}

std::shared_ptr<const UICommon::GameFile>
GameList::FindSecondDisc(const UICommon::GameFile& game) const
{
  return m_model->FindSecondDisc(game);
}

void GameList::SetViewColumn(int col, bool view)
{
  m_list->setColumnHidden(col, !view);
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
void GameList::keyPressEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Return && GetSelectedGame() != nullptr)
    emit GameSelected();
  else
    QStackedWidget::keyPressEvent(event);
}

void GameList::OnColumnVisibilityToggled(const QString& row, bool visible)
{
  static const QMap<QString, int> rowname_to_col_index = {
      {tr("Platform"), GameListModel::COL_PLATFORM},
      {tr("Banner"), GameListModel::COL_BANNER},
      {tr("Title"), GameListModel::COL_TITLE},
      {tr("Description"), GameListModel::COL_DESCRIPTION},
      {tr("Maker"), GameListModel::COL_MAKER},
      {tr("File Name"), GameListModel::COL_FILE_NAME},
      {tr("File Path"), GameListModel::COL_FILE_PATH},
      {tr("Game ID"), GameListModel::COL_ID},
      {tr("Region"), GameListModel::COL_COUNTRY},
      {tr("File Size"), GameListModel::COL_SIZE},
      {tr("File Format"), GameListModel::COL_FILE_FORMAT},
      {tr("Block Size"), GameListModel::COL_BLOCK_SIZE},
      {tr("Compression"), GameListModel::COL_COMPRESSION},
      {tr("Tags"), GameListModel::COL_TAGS}};

  m_list->setColumnHidden(rowname_to_col_index[row], !visible);
}

void GameList::OnGameListVisibilityChanged()
{
  m_list_proxy->invalidate();
  m_grid_proxy->invalidate();
}

void GameList::OnSectionResized(int index, int, int)
{
  auto* hor_header = m_list->horizontalHeader();

  std::vector<int> sections;

  const int vis_index = hor_header->visualIndex(index);
  const int col_count = hor_header->count() - hor_header->hiddenSectionCount();

  bool last = true;

  for (int i = vis_index + 1; i < col_count; i++)
  {
    const int logical_index = hor_header->logicalIndex(i);
    if (hor_header->sectionResizeMode(logical_index) != QHeaderView::Interactive)
      continue;

    last = false;
    break;
  }

  if (!last)
  {
    for (int i = 0; i < vis_index; i++)
    {
      const int logical_index = hor_header->logicalIndex(i);
      if (hor_header->sectionResizeMode(logical_index) != QHeaderView::Interactive)
        continue;

      hor_header->setSectionResizeMode(logical_index, QHeaderView::Fixed);
      sections.push_back(i);
    }

    OnHeaderViewChanged();

    for (int i : sections)
    {
      hor_header->setSectionResizeMode(hor_header->logicalIndex(i), QHeaderView::Interactive);
    }
  }
  else
  {
    OnHeaderViewChanged();
  }
}

void GameList::OnHeaderViewChanged()
{
  static bool block = false;

  if (block)
    return;

  block = true;

  UpdateColumnVisibility();

  // So here's the deal: Qt's way of resizing stuff around stretched columns sucks ass
  // That's why instead of using Stretch, we'll just make resizable columns take all the available
  // space ourselves!

  int available_width = width() - style()->pixelMetric(QStyle::PM_ScrollBarExtent);
  int previous_width = 0;

  std::vector<int> candidate_columns;

  // Iterate through all columns
  for (int i = 0; i < GameListModel::NUM_COLS; i++)
  {
    if (m_list->isColumnHidden(i))
      continue;

    if (m_list->horizontalHeader()->sectionResizeMode(i) == QHeaderView::Fixed)
    {
      available_width -= m_list->columnWidth(i);
    }
    else
    {
      candidate_columns.push_back(i);
      previous_width += m_list->columnWidth(i);
    }
  }

  for (int column : candidate_columns)
  {
    int column_width = static_cast<int>(
        std::max(5.f, std::ceil(available_width * (static_cast<float>(m_list->columnWidth(column)) /
                                                   previous_width))));

    m_list->setColumnWidth(column, column_width);
  }

  block = false;
}

void GameList::NewTag()
{
  auto tag = QInputDialog::getText(this, tr("New tag"), tr("Name for a new tag:"));

  if (tag.isEmpty())
    return;

  Settings::Instance().GetGameListModel()->NewTag(tag);
}

void GameList::DeleteTag()
{
  auto tag = QInputDialog::getText(this, tr("Remove tag"), tr("Name of the tag to remove:"));

  if (tag.isEmpty())
    return;

  Settings::Instance().GetGameListModel()->DeleteTag(tag);
}

void GameList::SetSearchTerm(const QString& term)
{
  m_model->SetSearchTerm(term);

  m_list_proxy->invalidate();
  m_grid_proxy->invalidate();

  UpdateColumnVisibility();
}

void GameList::ZoomIn()
{
  m_model->SetScale(m_model->GetScale() + 0.1);

  m_list_proxy->invalidate();
  m_grid_proxy->invalidate();

  UpdateFont();
}

void GameList::ZoomOut()
{
  if (m_model->GetScale() <= 0.1)
    return;

  m_model->SetScale(m_model->GetScale() - 0.1);

  m_list_proxy->invalidate();
  m_grid_proxy->invalidate();

  UpdateFont();
}

void GameList::UpdateFont()
{
  QFont f;

  f.setPointSizeF(m_model->GetScale() * f.pointSize());

  m_grid->setFont(f);
}
