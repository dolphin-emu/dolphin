// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/MenuBar.h"

#include <cinttypes>

#include <QAction>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMap>
#include <QMessageBox>
#include <QUrl>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/CommonTitles.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/WiiSaveCrypted.h"
#include "Core/HW/Wiimote.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/IOS.h"
#include "Core/Movie.h"
#include "Core/State.h"
#include "Core/TitleDatabase.h"
#include "Core/WiiUtils.h"

#include "DiscIO/NANDImporter.h"
#include "DiscIO/WiiSaveBanner.h"

#include "DolphinQt2/AboutDialog.h"
#include "DolphinQt2/GameList/GameFile.h"
#include "DolphinQt2/QtUtils/ActionHelper.h"
#include "DolphinQt2/Settings.h"

MenuBar::MenuBar(QWidget* parent) : QMenuBar(parent)
{
  AddFileMenu();
  AddEmulationMenu();
  AddMovieMenu();
  AddOptionsMenu();
  AddToolsMenu();
  AddViewMenu();
  AddHelpMenu();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          [=](Core::State state) { OnEmulationStateChanged(state); });
  OnEmulationStateChanged(Core::GetState());

  connect(this, &MenuBar::SelectionChanged, this, &MenuBar::OnSelectionChanged);
  connect(this, &MenuBar::RecordingStatusChanged, this, &MenuBar::OnRecordingStatusChanged);
  connect(this, &MenuBar::ReadOnlyModeChanged, this, &MenuBar::OnReadOnlyModeChanged);
}

void MenuBar::OnEmulationStateChanged(Core::State state)
{
  bool running = state != Core::State::Uninitialized;
  bool playing = running && state != Core::State::Paused;

  // Emulation
  m_play_action->setEnabled(!playing);
  m_play_action->setVisible(!playing);
  m_pause_action->setEnabled(playing);
  m_pause_action->setVisible(playing);
  m_stop_action->setEnabled(running);
  m_stop_action->setVisible(running);
  m_reset_action->setEnabled(running);
  m_fullscreen_action->setEnabled(running);
  m_frame_advance_action->setEnabled(running);
  m_screenshot_action->setEnabled(running);
  m_state_load_menu->setEnabled(running);
  m_state_save_menu->setEnabled(running);

  // Movie
  m_recording_read_only->setEnabled(running);
  if (!running)
    m_recording_stop->setEnabled(false);
  m_recording_play->setEnabled(!running);

  UpdateStateSlotMenu();
  UpdateToolsMenu(running);
}

void MenuBar::AddFileMenu()
{
  QMenu* file_menu = addMenu(tr("&File"));
  m_open_action = AddAction(file_menu, tr("&Open..."), this, &MenuBar::Open,
                            QKeySequence(QStringLiteral("Ctrl+O")));
  m_exit_action = AddAction(file_menu, tr("E&xit"), this, &MenuBar::Exit,
                            QKeySequence(QStringLiteral("Alt+F4")));
}

void MenuBar::AddToolsMenu()
{
  QMenu* tools_menu = addMenu(tr("&Tools"));

  AddAction(tools_menu, tr("Import Wii Save..."), this, &MenuBar::ImportWiiSave);
  AddAction(tools_menu, tr("Export All Wii Saves"), this, &MenuBar::ExportWiiSaves);

  tools_menu->addSeparator();

  m_wad_install_action = AddAction(tools_menu, tr("Install WAD..."), this, &MenuBar::InstallWAD);

  tools_menu->addSeparator();
  QMenu* gc_ipl = tools_menu->addMenu(tr("Load GameCube Main Menu"));

  m_ntscj_ipl = AddAction(gc_ipl, tr("NTSC-J"), this,
                          [this] { emit BootGameCubeIPL(DiscIO::Region::NTSC_J); });
  m_ntscu_ipl = AddAction(gc_ipl, tr("NTSC-U"), this,
                          [this] { emit BootGameCubeIPL(DiscIO::Region::NTSC_U); });
  m_pal_ipl =
      AddAction(gc_ipl, tr("PAL"), this, [this] { emit BootGameCubeIPL(DiscIO::Region::PAL); });

  AddAction(tools_menu, tr("Start &NetPlay..."), this, &MenuBar::StartNetPlay);
  tools_menu->addSeparator();

  // Label will be set by a NANDRefresh later
  m_boot_sysmenu =
      AddAction(tools_menu, QStringLiteral(""), this, [this] { emit BootWiiSystemMenu(); });
  m_import_backup = AddAction(gc_ipl, tr("Import BootMii NAND Backup..."), this,
                              [this] { emit ImportNANDBackup(); });
  m_check_nand = AddAction(tools_menu, tr("Check NAND..."), this, &MenuBar::CheckNAND);
  m_extract_certificates = AddAction(tools_menu, tr("Extract Certificates from NAND"), this,
                                     &MenuBar::NANDExtractCertificates);

  m_boot_sysmenu->setEnabled(false);

  connect(&Settings::Instance(), &Settings::NANDRefresh, [this] { UpdateToolsMenu(false); });

  m_perform_online_update_menu = tools_menu->addMenu(tr("Perform Online System Update"));
  m_perform_online_update_for_current_region =
      AddAction(m_perform_online_update_menu, tr("Current Region"), this,
                [this] { emit PerformOnlineUpdate(""); });
  m_perform_online_update_menu->addSeparator();
  AddAction(m_perform_online_update_menu, tr("Europe"), this,
            [this] { emit PerformOnlineUpdate("EUR"); });
  AddAction(m_perform_online_update_menu, tr("Japan"), this,
            [this] { emit PerformOnlineUpdate("JPN"); });
  AddAction(m_perform_online_update_menu, tr("Korea"), this,
            [this] { emit PerformOnlineUpdate("KOR"); });
  AddAction(m_perform_online_update_menu, tr("United States"), this,
            [this] { emit PerformOnlineUpdate("USA"); });
}

void MenuBar::AddEmulationMenu()
{
  QMenu* emu_menu = addMenu(tr("&Emulation"));
  m_play_action = AddAction(emu_menu, tr("&Play"), this, &MenuBar::Play);
  m_pause_action = AddAction(emu_menu, tr("&Pause"), this, &MenuBar::Pause);
  m_stop_action = AddAction(emu_menu, tr("&Stop"), this, &MenuBar::Stop);
  m_reset_action = AddAction(emu_menu, tr("&Reset"), this, &MenuBar::Reset);
  m_fullscreen_action = AddAction(emu_menu, tr("Toggle &Fullscreen"), this, &MenuBar::Fullscreen);
  m_frame_advance_action = AddAction(emu_menu, tr("&Frame Advance"), this, &MenuBar::FrameAdvance);

  m_screenshot_action = AddAction(emu_menu, tr("Take Screenshot"), this, &MenuBar::Screenshot);

  emu_menu->addSeparator();

  AddStateLoadMenu(emu_menu);
  AddStateSaveMenu(emu_menu);
  AddStateSlotMenu(emu_menu);
  UpdateStateSlotMenu();
}

void MenuBar::AddStateLoadMenu(QMenu* emu_menu)
{
  m_state_load_menu = emu_menu->addMenu(tr("&Load State"));
  AddAction(m_state_load_menu, tr("Load State from File"), this, &MenuBar::StateLoad);
  AddAction(m_state_load_menu, tr("Load State from Selected Slot"), this, &MenuBar::StateLoadSlot);
  m_state_load_slots_menu = m_state_load_menu->addMenu(tr("Load State from Slot"));
  AddAction(m_state_load_menu, tr("Undo Load State"), this, &MenuBar::StateLoadUndo);

  for (int i = 1; i <= 10; i++)
  {
    QAction* action = m_state_load_slots_menu->addAction(QStringLiteral(""));

    connect(action, &QAction::triggered, this, [=]() { emit StateLoadSlotAt(i); });
  }
}

void MenuBar::AddStateSaveMenu(QMenu* emu_menu)
{
  m_state_save_menu = emu_menu->addMenu(tr("Sa&ve State"));
  AddAction(m_state_save_menu, tr("Save State to File"), this, &MenuBar::StateSave);
  AddAction(m_state_save_menu, tr("Save State to Selected Slot"), this, &MenuBar::StateSaveSlot);
  AddAction(m_state_save_menu, tr("Save State to Oldest Slot"), this, &MenuBar::StateSaveOldest);
  m_state_save_slots_menu = m_state_save_menu->addMenu(tr("Save State to Slot"));
  AddAction(m_state_save_menu, tr("Undo Save State"), this, &MenuBar::StateSaveUndo);

  for (int i = 1; i <= 10; i++)
  {
    QAction* action = m_state_save_slots_menu->addAction(QStringLiteral(""));

    connect(action, &QAction::triggered, this, [=]() { emit StateSaveSlotAt(i); });
  }
}

void MenuBar::AddStateSlotMenu(QMenu* emu_menu)
{
  m_state_slot_menu = emu_menu->addMenu(tr("Select State Slot"));
  m_state_slots = new QActionGroup(this);

  for (int i = 1; i <= 10; i++)
  {
    QAction* action = m_state_slot_menu->addAction(QStringLiteral(""));
    action->setCheckable(true);
    action->setActionGroup(m_state_slots);
    if (Settings::Instance().GetStateSlot() == i)
      action->setChecked(true);

    connect(action, &QAction::triggered, this, [=]() { emit SetStateSlot(i); });
  }
}

void MenuBar::UpdateStateSlotMenu()
{
  QList<QAction*> actions_slot = m_state_slots->actions();
  QList<QAction*> actions_load = m_state_load_slots_menu->actions();
  QList<QAction*> actions_save = m_state_save_slots_menu->actions();
  for (int i = 0; i < actions_slot.length(); i++)
  {
    int slot = i + 1;
    QString info = QString::fromStdString(State::GetInfoStringOfSlot(slot));
    actions_load.at(i)->setText(tr("Load from Slot %1 - %2").arg(slot).arg(info));
    actions_save.at(i)->setText(tr("Save to Slot %1 - %2").arg(slot).arg(info));
    actions_slot.at(i)->setText(tr("Select Slot %1 - %2").arg(slot).arg(info));
  }
}

void MenuBar::AddViewMenu()
{
  QMenu* view_menu = addMenu(tr("&View"));
  QAction* show_log = view_menu->addAction(tr("Show &Log"));
  show_log->setCheckable(true);
  show_log->setChecked(Settings::Instance().IsLogVisible());

  connect(show_log, &QAction::toggled, &Settings::Instance(), &Settings::SetLogVisible);

  QAction* show_log_config = view_menu->addAction(tr("Show Log &Configuration"));
  show_log_config->setCheckable(true);
  show_log_config->setChecked(Settings::Instance().IsLogConfigVisible());

  connect(show_log_config, &QAction::toggled, &Settings::Instance(),
          &Settings::SetLogConfigVisible);

  connect(&Settings::Instance(), &Settings::LogVisibilityChanged, show_log, &QAction::setChecked);
  connect(&Settings::Instance(), &Settings::LogConfigVisibilityChanged, show_log_config,
          &QAction::setChecked);

  view_menu->addSeparator();

  AddGameListTypeSection(view_menu);
  view_menu->addSeparator();
  AddListColumnsMenu(view_menu);
  view_menu->addSeparator();
  AddShowPlatformsMenu(view_menu);
  AddShowRegionsMenu(view_menu);
}

void MenuBar::AddOptionsMenu()
{
  QMenu* options_menu = addMenu(tr("&Options"));
  AddAction(options_menu, tr("Co&nfiguration"), this, &MenuBar::Configure);
  options_menu->addSeparator();
  AddAction(options_menu, tr("&Graphics Settings"), this, &MenuBar::ConfigureGraphics);
  AddAction(options_menu, tr("&Audio Settings"), this, &MenuBar::ConfigureAudio);
  AddAction(options_menu, tr("&Controller Settings"), this, &MenuBar::ConfigureControllers);
  AddAction(options_menu, tr("&Hotkey Settings"), this, &MenuBar::ConfigureHotkeys);
}

void MenuBar::AddHelpMenu()
{
  QMenu* help_menu = addMenu(tr("&Help"));
  QAction* website = help_menu->addAction(tr("&Website"));
  connect(website, &QAction::triggered, this,
          []() { QDesktopServices::openUrl(QUrl(QStringLiteral("https://dolphin-emu.org/"))); });
  QAction* documentation = help_menu->addAction(tr("Online &Documentation"));
  connect(documentation, &QAction::triggered, this, []() {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://dolphin-emu.org/docs/guides")));
  });
  QAction* github = help_menu->addAction(tr("&GitHub Repository"));
  connect(github, &QAction::triggered, this, []() {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/dolphin-emu/dolphin")));
  });

  help_menu->addSeparator();
  AddAction(help_menu, tr("&About"), this, &MenuBar::ShowAboutDialog);
}

void MenuBar::AddGameListTypeSection(QMenu* view_menu)
{
  QAction* list_view = view_menu->addAction(tr("List View"));
  list_view->setCheckable(true);

  QAction* grid_view = view_menu->addAction(tr("Grid View"));
  grid_view->setCheckable(true);

  QActionGroup* list_group = new QActionGroup(this);
  list_group->addAction(list_view);
  list_group->addAction(grid_view);

  bool prefer_list = Settings::Instance().GetPreferredView();
  list_view->setChecked(prefer_list);
  grid_view->setChecked(!prefer_list);

  connect(list_view, &QAction::triggered, this, &MenuBar::ShowList);
  connect(grid_view, &QAction::triggered, this, &MenuBar::ShowGrid);
}

void MenuBar::AddListColumnsMenu(QMenu* view_menu)
{
  static const QMap<QString, bool*> columns{
      {tr("Platform"), &SConfig::GetInstance().m_showSystemColumn},
      {tr("Banner"), &SConfig::GetInstance().m_showBannerColumn},
      {tr("Title"), &SConfig::GetInstance().m_showTitleColumn},
      {tr("Description"), &SConfig::GetInstance().m_showDescriptionColumn},
      {tr("Maker"), &SConfig::GetInstance().m_showMakerColumn},
      {tr("File Name"), &SConfig::GetInstance().m_showFileNameColumn},
      {tr("Game ID"), &SConfig::GetInstance().m_showIDColumn},
      {tr("Region"), &SConfig::GetInstance().m_showRegionColumn},
      {tr("File Size"), &SConfig::GetInstance().m_showSizeColumn},
      {tr("State"), &SConfig::GetInstance().m_showStateColumn}};

  QActionGroup* column_group = new QActionGroup(this);
  QMenu* cols_menu = view_menu->addMenu(tr("List Columns"));
  column_group->setExclusive(false);

  for (const auto& key : columns.keys())
  {
    bool* config = columns[key];
    QAction* action = column_group->addAction(cols_menu->addAction(key));
    action->setCheckable(true);
    action->setChecked(*config);
    connect(action, &QAction::toggled, [this, config, key](bool value) {
      *config = value;
      emit ColumnVisibilityToggled(key, value);
    });
  }
}

void MenuBar::AddShowPlatformsMenu(QMenu* view_menu)
{
  static const QMap<QString, bool*> platform_map{
      {tr("Show Wii"), &SConfig::GetInstance().m_ListWii},
      {tr("Show GameCube"), &SConfig::GetInstance().m_ListGC},
      {tr("Show WAD"), &SConfig::GetInstance().m_ListWad},
      {tr("Show ELF/DOL"), &SConfig::GetInstance().m_ListElfDol}};

  QActionGroup* platform_group = new QActionGroup(this);
  QMenu* plat_menu = view_menu->addMenu(tr("Show Platforms"));
  platform_group->setExclusive(false);

  for (const auto& key : platform_map.keys())
  {
    bool* config = platform_map[key];
    QAction* action = platform_group->addAction(plat_menu->addAction(key));
    action->setCheckable(true);
    action->setChecked(*config);
    connect(action, &QAction::toggled, [this, config, key](bool value) {
      *config = value;
      emit GameListPlatformVisibilityToggled(key, value);
    });
  }
}

void MenuBar::AddShowRegionsMenu(QMenu* view_menu)
{
  static const QMap<QString, bool*> region_map{
      {tr("Show JAP"), &SConfig::GetInstance().m_ListJap},
      {tr("Show PAL"), &SConfig::GetInstance().m_ListPal},
      {tr("Show USA"), &SConfig::GetInstance().m_ListUsa},
      {tr("Show Australia"), &SConfig::GetInstance().m_ListAustralia},
      {tr("Show France"), &SConfig::GetInstance().m_ListFrance},
      {tr("Show Germany"), &SConfig::GetInstance().m_ListGermany},
      {tr("Show Italy"), &SConfig::GetInstance().m_ListItaly},
      {tr("Show Korea"), &SConfig::GetInstance().m_ListKorea},
      {tr("Show Netherlands"), &SConfig::GetInstance().m_ListNetherlands},
      {tr("Show Russia"), &SConfig::GetInstance().m_ListRussia},
      {tr("Show Spain"), &SConfig::GetInstance().m_ListSpain},
      {tr("Show Taiwan"), &SConfig::GetInstance().m_ListTaiwan},
      {tr("Show World"), &SConfig::GetInstance().m_ListWorld},
      {tr("Show Unknown"), &SConfig::GetInstance().m_ListUnknown}};

  QActionGroup* region_group = new QActionGroup(this);
  QMenu* region_menu = view_menu->addMenu(tr("Show Regions"));
  region_group->setExclusive(false);

  for (const auto& key : region_map.keys())
  {
    bool* config = region_map[key];
    QAction* action = region_group->addAction(region_menu->addAction(key));
    action->setCheckable(true);
    action->setChecked(*config);
    connect(action, &QAction::toggled, [this, config, key](bool value) {
      *config = value;
      emit GameListRegionVisibilityToggled(key, value);
    });
  }
}

void MenuBar::AddMovieMenu()
{
  auto* movie_menu = addMenu(tr("&Movie"));
  m_recording_start =
      AddAction(movie_menu, tr("Start Re&cording Input"), this, [this] { emit StartRecording(); });
  m_recording_play =
      AddAction(movie_menu, tr("P&lay Input Recording..."), this, [this] { emit PlayRecording(); });
  m_recording_stop = AddAction(movie_menu, tr("Stop Playing/Recording Input"), this,
                               [this] { emit StopRecording(); });
  m_recording_export =
      AddAction(movie_menu, tr("Export Recording..."), this, [this] { emit ExportRecording(); });

  m_recording_start->setEnabled(false);
  m_recording_play->setEnabled(false);
  m_recording_stop->setEnabled(false);
  m_recording_export->setEnabled(false);

  m_recording_read_only = movie_menu->addAction(tr("&Read-Only Mode"));
  m_recording_read_only->setCheckable(true);
  m_recording_read_only->setChecked(Movie::IsReadOnly());
  connect(m_recording_read_only, &QAction::toggled, [](bool value) { Movie::SetReadOnly(value); });

  movie_menu->addSeparator();

  auto* pause_at_end = movie_menu->addAction(tr("Pause at End of Movie"));
  pause_at_end->setCheckable(true);
  pause_at_end->setChecked(SConfig::GetInstance().m_PauseMovie);
  connect(pause_at_end, &QAction::toggled,
          [](bool value) { SConfig::GetInstance().m_PauseMovie = value; });

  auto* lag_counter = movie_menu->addAction(tr("Show Lag Counter"));
  lag_counter->setCheckable(true);
  lag_counter->setChecked(SConfig::GetInstance().m_ShowLag);
  connect(lag_counter, &QAction::toggled,
          [](bool value) { SConfig::GetInstance().m_ShowLag = value; });

  auto* frame_counter = movie_menu->addAction(tr("Show Frame Counter"));
  frame_counter->setCheckable(true);
  frame_counter->setChecked(SConfig::GetInstance().m_ShowFrameCount);
  connect(frame_counter, &QAction::toggled,
          [](bool value) { SConfig::GetInstance().m_ShowFrameCount = value; });

  auto* input_display = movie_menu->addAction(tr("Show Input Display"));
  input_display->setCheckable(true);
  input_display->setChecked(SConfig::GetInstance().m_ShowInputDisplay);
  connect(frame_counter, &QAction::toggled,
          [](bool value) { SConfig::GetInstance().m_ShowInputDisplay = value; });

  auto* system_clock = movie_menu->addAction(tr("Show System Clock"));
  system_clock->setCheckable(true);
  system_clock->setChecked(SConfig::GetInstance().m_ShowRTC);
  connect(system_clock, &QAction::toggled,
          [](bool value) { SConfig::GetInstance().m_ShowRTC = value; });

  movie_menu->addSeparator();

  auto* dump_frames = movie_menu->addAction(tr("Dump Frames"));
  dump_frames->setCheckable(true);
  dump_frames->setChecked(SConfig::GetInstance().m_DumpFrames);
  connect(dump_frames, &QAction::toggled,
          [](bool value) { SConfig::GetInstance().m_DumpFrames = value; });

  auto* dump_audio = movie_menu->addAction(tr("Dump Audio"));
  dump_audio->setCheckable(true);
  dump_audio->setChecked(SConfig::GetInstance().m_DumpAudio);
  connect(dump_audio, &QAction::toggled,
          [](bool value) { SConfig::GetInstance().m_DumpAudio = value; });
}

void MenuBar::UpdateToolsMenu(bool emulation_started)
{
  m_boot_sysmenu->setEnabled(!emulation_started);
  m_perform_online_update_menu->setEnabled(!emulation_started);
  m_ntscj_ipl->setEnabled(!emulation_started &&
                          File::Exists(SConfig::GetInstance().GetBootROMPath(JAP_DIR)));
  m_ntscu_ipl->setEnabled(!emulation_started &&
                          File::Exists(SConfig::GetInstance().GetBootROMPath(USA_DIR)));
  m_pal_ipl->setEnabled(!emulation_started &&
                        File::Exists(SConfig::GetInstance().GetBootROMPath(EUR_DIR)));
  m_import_backup->setEnabled(!emulation_started);
  m_check_nand->setEnabled(!emulation_started);

  if (!emulation_started)
  {
    IOS::HLE::Kernel ios;
    const auto tmd = ios.GetES()->FindInstalledTMD(Titles::SYSTEM_MENU);

    const QString sysmenu_version =
        tmd.IsValid() ?
            QString::fromStdString(DiscIO::GetSysMenuVersionString(tmd.GetTitleVersion())) :
            QStringLiteral("");
    m_boot_sysmenu->setText(tr("Load Wii System Menu %1").arg(sysmenu_version));

    m_boot_sysmenu->setEnabled(tmd.IsValid());

    for (QAction* action : m_perform_online_update_menu->actions())
      action->setEnabled(!tmd.IsValid());
    m_perform_online_update_for_current_region->setEnabled(tmd.IsValid());
  }
}

void MenuBar::InstallWAD()
{
  QString wad_file = QFileDialog::getOpenFileName(this, tr("Select a title to install to NAND"),
                                                  QString(), tr("WAD files (*.wad)"));

  if (wad_file.isEmpty())
    return;

  QMessageBox result_dialog(this);

  if (GameFile(wad_file).Install())
  {
    result_dialog.setIcon(QMessageBox::Information);
    result_dialog.setText(tr("Successfully installed this title to the NAND."));
  }
  else
  {
    result_dialog.setIcon(QMessageBox::Critical);
    result_dialog.setText(tr("Failed to install this title to the NAND."));
  }

  result_dialog.exec();
}

void MenuBar::ImportWiiSave()
{
  QString file = QFileDialog::getOpenFileName(this, tr("Select the save file"), QDir::currentPath(),
                                              tr("Wii save files (*.bin);;"
                                                 "All Files (*)"));

  if (!file.isEmpty())
    CWiiSaveCrypted::ImportWiiSave(file.toStdString());
}

void MenuBar::ExportWiiSaves()
{
  CWiiSaveCrypted::ExportAllSaves();
}

void MenuBar::CheckNAND()
{
  IOS::HLE::Kernel ios;
  WiiUtils::NANDCheckResult result = WiiUtils::CheckNAND(ios);
  if (!result.bad)
  {
    QMessageBox::information(this, tr("NAND Check"), tr("No issues have been detected."));
    return;
  }

  QString message = tr("The emulated NAND is damaged. System titles such as the Wii Menu and "
                       "the Wii Shop Channel may not work correctly.\n\n"
                       "Do you want to try to repair the NAND?");
  if (!result.titles_to_remove.empty())
  {
    std::string title_listings;
    Core::TitleDatabase title_db;
    for (const u64 title_id : result.titles_to_remove)
    {
      title_listings += StringFromFormat("%016" PRIx64, title_id);

      const std::string database_name = title_db.GetChannelName(title_id);
      if (!database_name.empty())
      {
        title_listings += " - " + database_name;
      }
      else
      {
        DiscIO::WiiSaveBanner banner(title_id);
        if (banner.IsValid())
        {
          title_listings += " - " + banner.GetName();
          const std::string description = banner.GetDescription();
          if (!StripSpaces(description).empty())
            title_listings += " - " + description;
        }
      }

      title_listings += "\n";
    }

    message += tr("\n\nWARNING: Fixing this NAND requires the deletion of titles that have "
                  "incomplete data on the NAND, including all associated save data. "
                  "By continuing, the following title(s) will be removed:\n\n"
                  "%1"
                  "\nLaunching these titles may also fix the issues.")
                   .arg(QString::fromStdString(title_listings));
  }

  if (QMessageBox::question(this, tr("NAND Check"), message) != QMessageBox::Yes)
    return;

  if (WiiUtils::RepairNAND(ios))
  {
    QMessageBox::information(this, tr("NAND Check"), tr("The NAND has been repaired."));
    return;
  }

  QMessageBox::critical(this, tr("NAND Check"),
                        tr("The NAND could not be repaired. It is recommended to back up "
                           "your current data and start over with a fresh NAND."));
}

void MenuBar::NANDExtractCertificates()
{
  if (DiscIO::NANDImporter().ExtractCertificates(File::GetUserPath(D_WIIROOT_IDX)))
  {
    QMessageBox::information(this, tr("Success"),
                             tr("Successfully extracted certificates from NAND"));
  }
  else
  {
    QMessageBox::critical(this, tr("Error"), tr("Failed to extract certificates from NAND"));
  }
}

void MenuBar::OnSelectionChanged(QSharedPointer<GameFile> game_file)
{
  bool is_null = game_file.isNull();

  m_recording_play->setEnabled(!Core::IsRunning() && !is_null);
  m_recording_start->setEnabled(!Movie::IsPlayingInput() && !is_null);
}

void MenuBar::OnRecordingStatusChanged(bool recording)
{
  m_recording_start->setEnabled(!recording);
  m_recording_stop->setEnabled(recording);
}

void MenuBar::OnReadOnlyModeChanged(bool read_only)
{
  m_recording_read_only->setChecked(read_only);
}
