// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef _WIN32
#include <QCoreApplication>
#include <shlobj.h>
#include <wil/com.h>

// This file uses some identifiers which are defined as macros in Windows headers.
// Include and undefine the macros first thing we do to solve build errors.
#ifdef DeleteFile
#undef DeleteFile
#endif
#ifdef interface
#undef interface
#endif
#endif

#include "DolphinQt/GameList/GameList.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <fmt/format.h>

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

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/WiiSave.h"
#include "Core/System.h"
#include "Core/WiiUtils.h"

#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"

#include "DolphinQt/Config/PropertiesDialog.h"
#include "DolphinQt/ConvertDialog.h"
#include "DolphinQt/GameList/GridProxyModel.h"
#include "DolphinQt/GameList/ListProxyModel.h"
#include "DolphinQt/MenuBar.h"
#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/QtUtils/DoubleClickEventFilter.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/ParallelProgressDialog.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/WiiUpdate.h"

#include "UICommon/GameFile.h"

namespace
{
class GameListTableView : public QTableView
{
public:
  explicit GameListTableView(QWidget* parent = nullptr) : QTableView(parent) {}

protected:
  QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override
  {
    // QTableView::moveCursor handles home by moving to the first column and end by moving to the
    // last column, unless control is held in which case it ALSO moves to the first/last row.
    // Columns are irrelevant for the game list, so treat the home/end press as if control were
    // held.
    if (cursorAction == CursorAction::MoveHome || cursorAction == CursorAction::MoveEnd)
      return QTableView::moveCursor(cursorAction, modifiers | Qt::ControlModifier);
    else
      return QTableView::moveCursor(cursorAction, modifiers);
  }
};
}  // namespace

GameList::GameList(QWidget* parent) : QStackedWidget(parent), m_model(this)
{
  m_list_proxy = new ListProxyModel(this);
  m_list_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
  m_list_proxy->setSortRole(GameListModel::SORT_ROLE);
  m_list_proxy->setSourceModel(&m_model);
  m_grid_proxy = new GridProxyModel(this);
  m_grid_proxy->setSourceModel(&m_model);

  MakeListView();
  MakeGridView();
  MakeEmptyView();

  if (Settings::GetQSettings().contains(QStringLiteral("gridview/scale")))
    m_model.SetScale(Settings::GetQSettings().value(QStringLiteral("gridview/scale")).toFloat());

  connect(m_list, &QTableView::doubleClicked, this, &GameList::GameSelected);
  connect(m_grid, &QListView::doubleClicked, this, &GameList::GameSelected);
  connect(&m_model, &QAbstractItemModel::rowsInserted, this, &GameList::ConsiderViewChange);
  connect(&m_model, &QAbstractItemModel::rowsRemoved, this, &GameList::ConsiderViewChange);

  addWidget(m_list);
  addWidget(m_grid);
  addWidget(m_empty);
  m_prefer_list = Settings::Instance().GetPreferredView();
  ConsiderViewChange();

  const auto* zoom_in = new QShortcut(QKeySequence::ZoomIn, this);
  const auto* zoom_out = new QShortcut(QKeySequence::ZoomOut, this);

  connect(zoom_in, &QShortcut::activated, this, &GameList::ZoomIn);
  connect(zoom_out, &QShortcut::activated, this, &GameList::ZoomOut);

  // On most keyboards the key to the left of the primary delete key represents 'plus' when shift is
  // held and 'equal' when it isn't. By common convention, pressing control and that key is treated
  // conceptually as 'control plus' (which is then interpreted as an appropriate zooming action)
  // instead of the technically correct 'control equal'. Qt doesn't account for this convention so
  // an alternate shortcut is needed to avoid counterintuitive behavior.
  const auto* zoom_in_alternate = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal), this);
  connect(zoom_in_alternate, &QShortcut::activated, this, &GameList::ZoomIn);

  // The above correction introduces a different inconsistency: now zooming in can be done using
  // conceptual 'control plus' or 'control shift plus', while zooming out can only be done using
  // 'control minus'. Adding an alternate shortcut representing 'control shift minus' restores
  // consistency.
  const auto* zoom_out_alternate = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Underscore), this);
  connect(zoom_out_alternate, &QShortcut::activated, this, &GameList::ZoomOut);

  connect(&Settings::Instance(), &Settings::MetadataRefreshCompleted, this,
          [this] { m_grid_proxy->invalidate(); });
}

void GameList::PurgeCache()
{
  m_model.PurgeCache();
}

void GameList::MakeListView()
{
  m_list = new GameListTableView(this);
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
    m_list->sortByColumn(static_cast<int>(GameListModel::Column::Title), Qt::AscendingOrder);

  const auto SetResizeMode = [&hor_header](const GameListModel::Column column,
                                           const QHeaderView::ResizeMode mode) {
    hor_header->setSectionResizeMode(static_cast<int>(column), mode);
  };
  {
    using Column = GameListModel::Column;
    using Mode = QHeaderView::ResizeMode;
    SetResizeMode(Column::Platform, Mode::Fixed);
    SetResizeMode(Column::Banner, Mode::Fixed);
    SetResizeMode(Column::Title, Mode::Interactive);
    SetResizeMode(Column::Description, Mode::Interactive);
    SetResizeMode(Column::Maker, Mode::Interactive);
    SetResizeMode(Column::ID, Mode::Fixed);
    SetResizeMode(Column::Country, Mode::Fixed);
    SetResizeMode(Column::Size, Mode::Fixed);
    SetResizeMode(Column::FileName, Mode::Interactive);
    SetResizeMode(Column::FilePath, Mode::Interactive);
    SetResizeMode(Column::FileFormat, Mode::Fixed);
    SetResizeMode(Column::BlockSize, Mode::Fixed);
    SetResizeMode(Column::Compression, Mode::Fixed);
    SetResizeMode(Column::Tags, Mode::Interactive);

    // Cells have 3 pixels of padding, so the width of these needs to be image width + 6. Banners
    // are 96 pixels wide, platform and country icons are 32 pixels wide.
    m_list->setColumnWidth(static_cast<int>(Column::Banner), 102);
    m_list->setColumnWidth(static_cast<int>(Column::Platform), 38);
    m_list->setColumnWidth(static_cast<int>(Column::Country), 38);
    m_list->setColumnWidth(static_cast<int>(Column::Size), 85);
    m_list->setColumnWidth(static_cast<int>(Column::ID), 70);
  }

  // There's some odd platform-specific behavior with default minimum section size
  hor_header->setMinimumSectionSize(38);

  UpdateColumnVisibility();

  m_list->verticalHeader()->hide();
  m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_list->setFrameStyle(QFrame::NoFrame);

  hor_header->setSectionsMovable(true);
  hor_header->setHighlightSections(false);

  // Work around a Qt bug where clicking in the background (below the last game) as the first
  // action and then pressing a key (e.g. page down or end) selects the first entry in the list
  // instead of performing that key's action.  This workaround does not work if there are no games
  // when the view first appears, but then games are added (e.g. due to no game folders being
  // present, and then the user adding one), but that is an infrequent situation.
  m_list->selectRow(0);
  m_list->clearSelection();

  connect(m_list, &QTableView::customContextMenuRequested, this, &GameList::ShowContextMenu);
  connect(m_list->selectionModel(), &QItemSelectionModel::selectionChanged,
          [this](const QItemSelection&, const QItemSelection&) {
            emit SelectionChanged(GetSelectedGame());
          });
}

GameList::~GameList()
{
  Settings::GetQSettings().setValue(QStringLiteral("tableheader/state"),
                                    m_list->horizontalHeader()->saveState());
  Settings::GetQSettings().setValue(QStringLiteral("gridview/scale"), m_model.GetScale());
}

void GameList::UpdateColumnVisibility()
{
  const auto SetVisiblity = [this](const GameListModel::Column column, const bool is_visible) {
    m_list->setColumnHidden(static_cast<int>(column), !is_visible);
  };

  using Column = GameListModel::Column;
  SetVisiblity(Column::Platform, Config::Get(Config::MAIN_GAMELIST_COLUMN_PLATFORM));
  SetVisiblity(Column::Banner, Config::Get(Config::MAIN_GAMELIST_COLUMN_BANNER));
  SetVisiblity(Column::Title, Config::Get(Config::MAIN_GAMELIST_COLUMN_TITLE));
  SetVisiblity(Column::Description, Config::Get(Config::MAIN_GAMELIST_COLUMN_DESCRIPTION));
  SetVisiblity(Column::Maker, Config::Get(Config::MAIN_GAMELIST_COLUMN_MAKER));
  SetVisiblity(Column::ID, Config::Get(Config::MAIN_GAMELIST_COLUMN_GAME_ID));
  SetVisiblity(Column::Country, Config::Get(Config::MAIN_GAMELIST_COLUMN_REGION));
  SetVisiblity(Column::Size, Config::Get(Config::MAIN_GAMELIST_COLUMN_FILE_SIZE));
  SetVisiblity(Column::FileName, Config::Get(Config::MAIN_GAMELIST_COLUMN_FILE_NAME));
  SetVisiblity(Column::FilePath, Config::Get(Config::MAIN_GAMELIST_COLUMN_FILE_PATH));
  SetVisiblity(Column::FileFormat, Config::Get(Config::MAIN_GAMELIST_COLUMN_FILE_FORMAT));
  SetVisiblity(Column::BlockSize, Config::Get(Config::MAIN_GAMELIST_COLUMN_BLOCK_SIZE));
  SetVisiblity(Column::Compression, Config::Get(Config::MAIN_GAMELIST_COLUMN_COMPRESSION));
  SetVisiblity(Column::Tags, Config::Get(Config::MAIN_GAMELIST_COLUMN_TAGS));
}

void GameList::MakeEmptyView()
{
  const QString refreshing_msg = tr("Refreshing...");
  const QString empty_msg = tr("Dolphin could not find any GameCube/Wii ISOs or WADs.\n"
                               "Double-click here to set a games directory...");

  m_empty = new QLabel(this);
  m_empty->setText(refreshing_msg);
  m_empty->setEnabled(false);
  m_empty->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

  auto event_filter = new DoubleClickEventFilter{m_empty};
  m_empty->installEventFilter(event_filter);
  connect(event_filter, &DoubleClickEventFilter::doubleClicked, [this] {
    auto current_dir = QDir::currentPath();
    auto dir = DolphinFileDialog::getExistingDirectory(this, tr("Select a Directory"), current_dir);
    if (!dir.isEmpty())
      Settings::Instance().AddPath(dir);
  });

  QSizePolicy size_policy{m_empty->sizePolicy()};
  size_policy.setRetainSizeWhenHidden(true);
  m_empty->setSizePolicy(size_policy);

  connect(&Settings::Instance(), &Settings::GameListRefreshRequested, this,
          [this, refreshing_msg = refreshing_msg] {
            m_empty->setText(refreshing_msg);
            m_empty->setEnabled(false);
          });
  connect(&Settings::Instance(), &Settings::GameListRefreshCompleted, this,
          [this, empty_msg = empty_msg] {
            m_empty->setText(empty_msg);
            m_empty->setEnabled(true);
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

  // Row in this context refers to the full set of columns (banner, title, region, etc.) associated
  // with a single game rather than multiple games presented visually on the same row. Even though
  // most of these elements are hidden in grid view selecting all of them allows us to use
  // selectedRow() which only returns rows where all elements are selected. In addition to improving
  // consistency with list view this avoids an edge case where clicking a game in grid mode selected
  // only the first element of the row but ctrl+a selected every element of the row, making a bunch
  // of duplicate selections for each game.
  m_grid->setSelectionBehavior(QAbstractItemView::SelectRows);

  m_grid->setViewMode(QListView::IconMode);
  m_grid->setResizeMode(QListView::Adjust);
  m_grid->setUniformItemSizes(true);
  m_grid->setContextMenuPolicy(Qt::CustomContextMenu);
  m_grid->setFrameStyle(QFrame::NoFrame);

  // Work around a Qt bug where clicking in the background (below the last game) as the first action
  // and then pressing a key (e.g. page down or end) selects the first entry in the list instead of
  // performing that key's action.  This workaround does not work if there are no games when the
  // view first appears, but then games are added (e.g. due to no game folders being present,
  // and then the user adding one), but that is an infrequent situation.
  m_grid->setCurrentIndex(m_grid->indexAt(QPoint(0, 0)));
  m_grid->clearSelection();

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

  if (HasMultipleSelected())
  {
    const auto selected_games = GetSelectedGames();

    if (std::all_of(selected_games.begin(), selected_games.end(),
                    [](const auto& game) { return game->ShouldAllowConversion(); }))
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
    const bool is_mod_descriptor = game->IsModDescriptor();
    DiscIO::Platform platform = game->GetPlatform();
    menu->addAction(tr("&Properties"), this, &GameList::OpenProperties);
    if (!is_mod_descriptor && platform != DiscIO::Platform::ELFOrDOL)
    {
      menu->addAction(tr("&Wiki"), this, &GameList::OpenWiki);
    }

    menu->addSeparator();

    if (!is_mod_descriptor && DiscIO::IsDisc(platform))
    {
      menu->addAction(tr("Start with Riivolution Patches..."), this,
                      &GameList::StartWithRiivolution);

      menu->addSeparator();

      menu->addAction(tr("Set as &Default ISO"), this, &GameList::SetDefaultISO);

      if (game->ShouldAllowConversion())
        menu->addAction(tr("Convert File..."), this, &GameList::ConvertFile);

      QAction* change_disc = menu->addAction(tr("Change &Disc"), this, &GameList::ChangeDisc);

      connect(&Settings::Instance(), &Settings::EmulationStateChanged, change_disc,
              [change_disc] { change_disc->setEnabled(Core::IsRunning()); });
      change_disc->setEnabled(Core::IsRunning());

      menu->addSeparator();
    }

    if (!is_mod_descriptor && platform == DiscIO::Platform::WiiDisc)
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

    if (!is_mod_descriptor && platform == DiscIO::Platform::WiiWAD)
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

    if (!is_mod_descriptor &&
        (platform == DiscIO::Platform::WiiWAD || platform == DiscIO::Platform::WiiDisc))
    {
      QAction* open_wii_save_folder =
          menu->addAction(tr("Open Wii &Save Folder"), this, &GameList::OpenWiiSaveFolder);
      QAction* export_wii_save =
          menu->addAction(tr("Export Wii Save"), this, &GameList::ExportWiiSave);

      open_wii_save_folder->setEnabled(!Core::IsRunning());
      export_wii_save->setEnabled(!Core::IsRunning());

      menu->addSeparator();
    }

    if (!is_mod_descriptor && platform == DiscIO::Platform::GameCubeDisc)
    {
      menu->addAction(tr("Open GameCube &Save Folder"), this, &GameList::OpenGCSaveFolder);
      menu->addSeparator();
    }

    menu->addAction(tr("Open &Containing Folder"), this, &GameList::OpenContainingFolder);
    menu->addAction(tr("Delete File..."), this, &GameList::DeleteFile);
#ifdef _WIN32
    menu->addAction(tr("Add Shortcut to Desktop"), this, [this] {
      if (!AddShortcutToDesktop())
      {
        ModalMessageBox::critical(this, tr("Add Shortcut to Desktop"),
                                  tr("There was an issue adding a shortcut to the desktop"));
      }
    });
#endif

    menu->addSeparator();

    auto* tags_menu = menu->addMenu(tr("Tags"));

    auto path = game->GetFilePath();
    auto game_tags = m_model.GetGameTags(path);

    for (const auto& tag : m_model.GetAllTags())
    {
      auto* tag_action = tags_menu->addAction(tag);

      tag_action->setCheckable(true);
      tag_action->setChecked(game_tags.contains(tag));

      connect(tag_action, &QAction::toggled, [path, tag, model = &m_model](bool checked) {
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

    connect(netplay_host, &QAction::triggered, [this, game] { emit NetPlayHost(*game); });

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
  connect(properties, &PropertiesDialog::OpenGraphicsSettings, this,
          &GameList::OpenGraphicsSettings);
#ifdef USE_RETRO_ACHIEVEMENTS
  connect(properties, &PropertiesDialog::OpenAchievementSettings, this,
          &GameList::OpenAchievementSettings);
#endif  // USE_RETRO_ACHIEVEMENTS

  SetQWidgetWindowDecorations(properties);
  properties->show();
}

void GameList::ExportWiiSave()
{
  const QString export_dir = DolphinFileDialog::getExistingDirectory(
      this, tr("Select Export Directory"), QString::fromStdString(File::GetUserPath(D_USER_IDX)),
      QFileDialog::ShowDirsOnly);
  if (export_dir.isEmpty())
    return;

  QList<std::string> failed;
  for (const auto& game : GetSelectedGames())
  {
    if (WiiSave::Export(game->GetTitleID(), export_dir.toStdString()) !=
        WiiSave::CopyResult::Success)
    {
      failed.push_back(game->GetName(UICommon::GameFile::Variant::LongAndPossiblyCustom));
    }
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
  QString url =
      QStringLiteral("https://wiki.dolphin-emu.org/dolphin-redirect.php?gameid=").append(game_id);
  QDesktopServices::openUrl(QUrl(url));
}

void GameList::ConvertFile()
{
  auto games = GetSelectedGames();
  if (games.empty())
    return;

  ConvertDialog dialog{std::move(games), this};
  SetQWidgetWindowDecorations(&dialog);
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
  SetQWidgetWindowDecorations(&result_dialog);
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

  SetQWidgetWindowDecorations(&warning_dialog);
  if (warning_dialog.exec() == QMessageBox::No)
    return;

  ModalMessageBox result_dialog(this);

  const bool success = WiiUtils::UninstallTitle(game->GetTitleID());

  result_dialog.setIcon(success ? QMessageBox::Information : QMessageBox::Critical);
  result_dialog.setWindowTitle(success ? tr("Success") : tr("Failure"));
  result_dialog.setText(success ? tr("Successfully removed this title from the NAND.") :
                                  tr("Failed to remove this title from the NAND."));
  SetQWidgetWindowDecorations(&result_dialog);
  result_dialog.exec();
}

void GameList::StartWithRiivolution()
{
  const auto game = GetSelectedGame();
  if (!game)
    return;

  emit OnStartWithRiivolution(*game);
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

  // Remove everything after the last separator in the game's path, resulting in the parent
  // directory path with a trailing separator. Keeping the trailing separator prevents Windows from
  // mistakenly opening a .bat or .exe file in the grandparent folder when that file has the same
  // base name as the folder (See https://bugs.dolphin-emu.org/issues/12411).
  std::string parent_directory_path;
  SplitPath(game->GetFilePath(), &parent_directory_path, nullptr, nullptr);
  if (parent_directory_path.empty())
  {
    return;
  }

  const QUrl url = QUrl::fromLocalFile(QString::fromStdString(parent_directory_path));
  QDesktopServices::openUrl(url);
}

void GameList::OpenWiiSaveFolder()
{
  const auto game = GetSelectedGame();
  if (!game)
    return;

  const std::string path = game->GetWiiFSPath();
  if (!File::Exists(path))
  {
    ModalMessageBox::information(this, tr("Information"), tr("No save data found."));
    return;
  }

  const QUrl url = QUrl::fromLocalFile(QString::fromStdString(path));
  QDesktopServices::openUrl(url);
}

void GameList::OpenGCSaveFolder()
{
  const auto game = GetSelectedGame();
  if (!game)
    return;

  bool found = false;

  using ExpansionInterface::Slot;

  for (Slot slot : ExpansionInterface::MEMCARD_SLOTS)
  {
    QUrl url;
    const ExpansionInterface::EXIDeviceType current_exi_device =
        Config::Get(Config::GetInfoForEXIDevice(slot));
    switch (current_exi_device)
    {
    case ExpansionInterface::EXIDeviceType::MemoryCardFolder:
    {
      std::string override_path = Config::Get(Config::GetInfoForGCIPathOverride(slot));
      QDir dir(QString::fromStdString(override_path.empty() ?
                                          Config::GetGCIFolderPath(slot, game->GetRegion()) :
                                          override_path));

      if (!dir.entryList({QStringLiteral("%1-%2-*.gci")
                              .arg(QString::fromStdString(game->GetMakerID()))
                              .arg(QString::fromStdString(game->GetGameID().substr(0, 4)))})
               .empty())
      {
        url = QUrl::fromLocalFile(dir.absolutePath());
      }
      break;
    }
    case ExpansionInterface::EXIDeviceType::MemoryCard:
    {
      const std::string memcard_path = Config::GetMemcardPath(slot, game->GetRegion());

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

#ifdef _WIN32
bool GameList::AddShortcutToDesktop()
{
  auto init = wil::CoInitializeEx_failfast(COINIT_APARTMENTTHREADED);
  auto shell_link = wil::CoCreateInstanceNoThrow<ShellLink, IShellLink>();
  if (!shell_link)
    return false;

  std::wstring dolphin_path = QCoreApplication::applicationFilePath().toStdWString();
  if (FAILED(shell_link->SetPath(dolphin_path.c_str())))
    return false;

  const auto game = GetSelectedGame();
  const auto& file_path = game->GetFilePath();
  std::wstring args = UTF8ToTStr("-e \"" + file_path + "\"");
  if (FAILED(shell_link->SetArguments(args.c_str())))
    return false;

  wil::unique_cotaskmem_string desktop;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_Desktop, KF_FLAG_NO_ALIAS, nullptr, &desktop)))
    return false;

  std::string game_name = game->GetName(Core::TitleDatabase());
  // Sanitize the string by removing all characters that cannot be used in NTFS file names
  game_name.erase(std::remove_if(game_name.begin(), game_name.end(),
                                 [](char ch) {
                                   static constexpr char illegal_characters[] = {
                                       '<', '>', ':', '\"', '/', '\\', '|', '?', '*'};
                                   return std::find(std::begin(illegal_characters),
                                                    std::end(illegal_characters),
                                                    ch) != std::end(illegal_characters);
                                 }),
                  game_name.end());

  std::wstring desktop_path = std::wstring(desktop.get()) + UTF8ToTStr("\\" + game_name + ".lnk");
  auto persist_file = shell_link.try_query<IPersistFile>();
  if (!persist_file)
    return false;

  if (FAILED(persist_file->Save(desktop_path.c_str(), TRUE)))
    return false;

  return true;
}
#endif

void GameList::DeleteFile()
{
  ModalMessageBox confirm_dialog(this);

  confirm_dialog.setIcon(QMessageBox::Warning);
  confirm_dialog.setWindowTitle(tr("Confirm"));
  confirm_dialog.setText(tr("Are you sure you want to delete this file?"));
  confirm_dialog.setInformativeText(tr("This cannot be undone!"));
  confirm_dialog.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);

  SetQWidgetWindowDecorations(&confirm_dialog);
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
          m_model.RemoveGame(game->GetFilePath());
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

          SetQWidgetWindowDecorations(&error_dialog);
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

  Core::RunAsCPUThread([file_path = game->GetFilePath()] {
    Core::System::GetInstance().GetDVDInterface().ChangeDisc(file_path);
  });
}

QAbstractItemView* GameList::GetActiveView() const
{
  if (currentWidget() == m_list)
  {
    return m_list;
  }
  return m_grid;
}

QSortFilterProxyModel* GameList::GetActiveProxyModel() const
{
  if (currentWidget() == m_list)
  {
    return m_list_proxy;
  }
  return m_grid_proxy;
}

std::shared_ptr<const UICommon::GameFile> GameList::GetSelectedGame() const
{
  QItemSelectionModel* const sel_model = GetActiveView()->selectionModel();
  if (sel_model->hasSelection())
  {
    QSortFilterProxyModel* const proxy = GetActiveProxyModel();
    QModelIndex model_index = proxy->mapToSource(sel_model->selectedIndexes()[0]);
    return m_model.GetGameFile(model_index.row());
  }
  return {};
}

QList<std::shared_ptr<const UICommon::GameFile>> GameList::GetSelectedGames() const
{
  QList<std::shared_ptr<const UICommon::GameFile>> selected_list;
  QItemSelectionModel* const sel_model = GetActiveView()->selectionModel();

  if (sel_model->hasSelection())
  {
    const QModelIndexList index_list = sel_model->selectedRows();
    for (const auto& index : index_list)
    {
      const QModelIndex model_index = GetActiveProxyModel()->mapToSource(index);
      selected_list.push_back(m_model.GetGameFile(model_index.row()));
    }
  }
  return selected_list;
}

bool GameList::HasMultipleSelected() const
{
  return GetActiveView()->selectionModel()->selectedRows().size() > 1;
}

std::shared_ptr<const UICommon::GameFile> GameList::FindGame(const std::string& path) const
{
  return m_model.FindGame(path);
}

std::shared_ptr<const UICommon::GameFile>
GameList::FindSecondDisc(const UICommon::GameFile& game) const
{
  return m_model.FindSecondDisc(game);
}

std::string GameList::GetNetPlayName(const UICommon::GameFile& game) const
{
  return m_model.GetNetPlayName(game);
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
  if (m_model.rowCount(QModelIndex()) > 0)
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
  {
    if (event->modifiers() == Qt::AltModifier)
      OpenProperties();
    else
      emit GameSelected();
  }
  else
  {
    QStackedWidget::keyPressEvent(event);
  }
}

void GameList::OnColumnVisibilityToggled(const QString& row, bool visible)
{
  using Column = GameListModel::Column;
  static const QMap<QString, Column> rowname_to_column = {
      {tr("Platform"), Column::Platform},
      {tr("Banner"), Column::Banner},
      {tr("Title"), Column::Title},
      {tr("Description"), Column::Description},
      {tr("Maker"), Column::Maker},
      {tr("File Name"), Column::FileName},
      {tr("File Path"), Column::FilePath},
      {tr("Game ID"), Column::ID},
      {tr("Region"), Column::Country},
      {tr("File Size"), Column::Size},
      {tr("File Format"), Column::FileFormat},
      {tr("Block Size"), Column::BlockSize},
      {tr("Compression"), Column::Compression},
      {tr("Tags"), Column::Tags},
  };

  m_list->setColumnHidden(static_cast<int>(rowname_to_column[row]), !visible);
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
  for (int i = 0; i < static_cast<int>(GameListModel::Column::Count); i++)
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
  const auto tag =
      QInputDialog::getText(this, tr("New tag"), tr("Name for a new tag:"), QLineEdit::Normal,
                            QString{}, nullptr, Qt::WindowCloseButtonHint);

  if (tag.isEmpty())
    return;

  m_model.NewTag(tag);
}

void GameList::DeleteTag()
{
  const auto tag =
      QInputDialog::getText(this, tr("Remove tag"), tr("Name of the tag to remove:"),
                            QLineEdit::Normal, QString{}, nullptr, Qt::WindowCloseButtonHint);

  if (tag.isEmpty())
    return;

  m_model.DeleteTag(tag);
}

void GameList::SetSearchTerm(const QString& term)
{
  m_model.SetSearchTerm(term);

  m_list_proxy->invalidate();
  m_grid_proxy->invalidate();

  UpdateColumnVisibility();
}

void GameList::ZoomIn()
{
  m_model.SetScale(m_model.GetScale() + 0.1);

  m_list_proxy->invalidate();
  m_grid_proxy->invalidate();

  UpdateFont();
}

void GameList::ZoomOut()
{
  if (m_model.GetScale() <= 0.1)
    return;

  m_model.SetScale(m_model.GetScale() - 0.1);

  m_list_proxy->invalidate();
  m_grid_proxy->invalidate();

  UpdateFont();
}

void GameList::UpdateFont()
{
  QFont f;

  f.setPointSizeF(m_model.GetScale() * f.pointSize());

  m_grid->setFont(f);
}
