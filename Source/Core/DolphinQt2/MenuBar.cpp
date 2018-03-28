// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/MenuBar.h"

#include <cinttypes>

#include <QAction>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFontDialog>
#include <QInputDialog>
#include <QMap>
#include <QMessageBox>
#include <QUrl>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Common/CDUtils.h"
#include "Core/Boot/Boot.h"
#include "Core/CommonTitles.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Debugger/RSO.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/WiiSaveCrypted.h"
#include "Core/HW/Wiimote.h"
#include "Core/Host.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Bluetooth/BTEmu.h"
#include "Core/Movie.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/SignatureDB/SignatureDB.h"
#include "Core/State.h"
#include "Core/TitleDatabase.h"
#include "Core/WiiUtils.h"

#include "DiscIO/NANDImporter.h"
#include "DiscIO/WiiSaveBanner.h"

#include "DolphinQt2/AboutDialog.h"
#include "DolphinQt2/QtUtils/ActionHelper.h"
#include "DolphinQt2/Settings.h"

#include "UICommon/GameFile.h"

MenuBar::MenuBar(QWidget* parent) : QMenuBar(parent)
{
  AddFileMenu();
  AddEmulationMenu();
  AddMovieMenu();
  AddOptionsMenu();
  AddToolsMenu();
  AddViewMenu();
  AddSymbolsMenu();
  AddHelpMenu();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          [=](Core::State state) { OnEmulationStateChanged(state); });
  OnEmulationStateChanged(Core::GetState());
  connect(&Settings::Instance(), &Settings::DebugModeToggled, this, &MenuBar::OnDebugModeToggled);

  connect(this, &MenuBar::SelectionChanged, this, &MenuBar::OnSelectionChanged);
  connect(this, &MenuBar::RecordingStatusChanged, this, &MenuBar::OnRecordingStatusChanged);
  connect(this, &MenuBar::ReadOnlyModeChanged, this, &MenuBar::OnReadOnlyModeChanged);
}

void MenuBar::OnEmulationStateChanged(Core::State state)
{
  bool running = state != Core::State::Uninitialized;
  bool playing = running && state != Core::State::Paused;

  // File
  m_eject_disc->setEnabled(running);
  m_change_disc->setEnabled(running);

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

  // Symbols
  m_symbols->setEnabled(running);

  UpdateStateSlotMenu();
  UpdateToolsMenu(running);

  OnDebugModeToggled(Settings::Instance().IsDebugModeEnabled());
}

void MenuBar::OnDebugModeToggled(bool enabled)
{
  // Options
  m_boot_to_pause->setVisible(enabled);
  m_automatic_start->setVisible(enabled);
  m_change_font->setVisible(enabled);

  // View
  m_show_code->setVisible(enabled);
  m_show_registers->setVisible(enabled);
  m_show_watch->setVisible(enabled);
  m_show_breakpoints->setVisible(enabled);

  if (enabled)
    addMenu(m_symbols);
  else
    removeAction(m_symbols->menuAction());
}

void MenuBar::AddDVDBackupMenu(QMenu* file_menu)
{
  m_backup_menu = file_menu->addMenu(tr("Boot from DVD Backup"));

  const std::vector<std::string> drives = cdio_get_devices();
  // Windows Limitation of 24 character drives
  for (size_t i = 0; i < drives.size() && i < 24; i++)
  {
    auto drive = QString::fromStdString(drives[i]);
    AddAction(m_backup_menu, drive, this, [this, drive] { emit BootDVDBackup(drive); });
  }
}

void MenuBar::AddFileMenu()
{
  QMenu* file_menu = addMenu(tr("&File"));
  m_open_action = AddAction(file_menu, tr("&Open..."), this, &MenuBar::Open,
                            QKeySequence(QStringLiteral("Ctrl+O")));

  file_menu->addSeparator();

  m_change_disc = AddAction(file_menu, tr("Change &Disc..."), this, &MenuBar::ChangeDisc);
  m_eject_disc = AddAction(file_menu, tr("&Eject Disc"), this, &MenuBar::EjectDisc);

  AddDVDBackupMenu(file_menu);

  file_menu->addSeparator();

  m_exit_action = AddAction(file_menu, tr("E&xit"), this, &MenuBar::Exit,
                            QKeySequence(QStringLiteral("Alt+F4")));
}

void MenuBar::AddToolsMenu()
{
  QMenu* tools_menu = addMenu(tr("&Tools"));

  AddAction(tools_menu, tr("&Memory Card Manager (GC)"), this,
            [this] { emit ShowMemcardManager(); });

  tools_menu->addSeparator();

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
  AddAction(tools_menu, tr("FIFO Player"), this, &MenuBar::ShowFIFOPlayer);

  tools_menu->addSeparator();

  // Label will be set by a NANDRefresh later
  m_boot_sysmenu =
      AddAction(tools_menu, QStringLiteral(""), this, [this] { emit BootWiiSystemMenu(); });
  m_import_backup = AddAction(tools_menu, tr("Import BootMii NAND Backup..."), this,
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

  QMenu* menu = new QMenu(tr("Connect Wii Remotes"));

  tools_menu->addSeparator();
  tools_menu->addMenu(menu);

  for (int i = 0; i < 4; i++)
  {
    m_wii_remotes[i] = AddAction(menu, tr("Connect Wii Remote %1").arg(i + 1), this,
                                 [this, i] { emit ConnectWiiRemote(i); });
    m_wii_remotes[i]->setCheckable(true);
  }

  menu->addSeparator();

  m_wii_remotes[4] =
      AddAction(menu, tr("Connect Balance Board"), this, [this] { emit ConnectWiiRemote(4); });
  m_wii_remotes[4]->setCheckable(true);
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

  m_show_code = view_menu->addAction(tr("&Code"));
  m_show_code->setCheckable(true);
  m_show_code->setChecked(Settings::Instance().IsCodeVisible());

  connect(m_show_code, &QAction::toggled, &Settings::Instance(), &Settings::SetCodeVisible);
  connect(&Settings::Instance(), &Settings::CodeVisibilityChanged, m_show_code,
          &QAction::setChecked);

  m_show_registers = view_menu->addAction(tr("&Registers"));
  m_show_registers->setCheckable(true);
  m_show_registers->setChecked(Settings::Instance().IsRegistersVisible());

  connect(m_show_registers, &QAction::toggled, &Settings::Instance(),
          &Settings::SetRegistersVisible);
  connect(&Settings::Instance(), &Settings::RegistersVisibilityChanged, m_show_registers,
          &QAction::setChecked);

  m_show_watch = view_menu->addAction(tr("&Watch"));
  m_show_watch->setCheckable(true);
  m_show_watch->setChecked(Settings::Instance().IsWatchVisible());

  connect(m_show_watch, &QAction::toggled, &Settings::Instance(), &Settings::SetWatchVisible);
  connect(&Settings::Instance(), &Settings::WatchVisibilityChanged, m_show_watch,
          &QAction::setChecked);

  m_show_breakpoints = view_menu->addAction(tr("&Breakpoints"));
  m_show_breakpoints->setCheckable(true);
  m_show_breakpoints->setChecked(Settings::Instance().IsBreakpointsVisible());

  connect(m_show_breakpoints, &QAction::toggled, &Settings::Instance(),
          &Settings::SetBreakpointsVisible);
  connect(&Settings::Instance(), &Settings::BreakpointsVisibilityChanged, m_show_breakpoints,
          &QAction::setChecked);

  view_menu->addSeparator();

  AddGameListTypeSection(view_menu);
  view_menu->addSeparator();
  AddListColumnsMenu(view_menu);
  view_menu->addSeparator();
  AddShowPlatformsMenu(view_menu);
  AddShowRegionsMenu(view_menu);

  view_menu->addSeparator();
  AddAction(view_menu, tr("Search"), this, &MenuBar::ToggleSearch,
            QKeySequence(QStringLiteral("Ctrl+F")));
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

  options_menu->addSeparator();

  // Debugging mode only
  m_boot_to_pause = options_menu->addAction(tr("Boot to Pause"));
  m_boot_to_pause->setCheckable(true);
  m_boot_to_pause->setChecked(SConfig::GetInstance().bBootToPause);

  connect(m_boot_to_pause, &QAction::toggled, this,
          [this](bool enable) { SConfig::GetInstance().bBootToPause = enable; });

  m_automatic_start = options_menu->addAction(tr("&Automatic Start"));
  m_automatic_start->setCheckable(true);
  m_automatic_start->setChecked(SConfig::GetInstance().bAutomaticStart);

  connect(m_automatic_start, &QAction::toggled, this,
          [this](bool enable) { SConfig::GetInstance().bAutomaticStart = enable; });

  m_change_font = AddAction(options_menu, tr("&Font..."), this, &MenuBar::ChangeDebugFont);
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

  AddAction(movie_menu, tr("TAS Input"), this, [this] { emit ShowTASInput(); });

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

void MenuBar::AddSymbolsMenu()
{
  m_symbols = addMenu(tr("Symbols"));

  AddAction(m_symbols, tr("&Clear Symbols"), this, &MenuBar::ClearSymbols);

  auto* generate = m_symbols->addMenu(tr("&Generate Symbols From"));
  AddAction(generate, tr("Address"), this, &MenuBar::GenerateSymbolsFromAddress);
  AddAction(generate, tr("Signature Database"), this, &MenuBar::GenerateSymbolsFromSignatureDB);
  AddAction(generate, tr("RSO Modules"), this, &MenuBar::GenerateSymbolsFromRSO);
  m_symbols->addSeparator();

  AddAction(m_symbols, tr("&Load Symbol Map"), this, &MenuBar::LoadSymbolMap);
  AddAction(m_symbols, tr("&Save Symbol Map"), this, &MenuBar::SaveSymbolMap);
  m_symbols->addSeparator();

  AddAction(m_symbols, tr("Load &Other Map File..."), this, &MenuBar::LoadOtherSymbolMap);
  AddAction(m_symbols, tr("Save Symbol Map &As..."), this, &MenuBar::SaveSymbolMapAs);
  m_symbols->addSeparator();

  AddAction(m_symbols, tr("Save Code"), this, &MenuBar::SaveCode);
  m_symbols->addSeparator();

  AddAction(m_symbols, tr("&Create Signature File..."), this, &MenuBar::CreateSignatureFile);
  m_symbols->addSeparator();

  AddAction(m_symbols, tr("&Patch HLE Functions"), this, &MenuBar::PatchHLEFunctions);
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

  const auto ios = IOS::HLE::GetIOS();
  const auto bt = ios ? std::static_pointer_cast<IOS::HLE::Device::BluetoothEmu>(
                            ios->GetDeviceByName("/dev/usb/oh1/57e/305")) :
                        nullptr;
  bool enable_wiimotes =
      emulation_started && bt && !SConfig::GetInstance().m_bt_passthrough_enabled;

  for (int i = 0; i < 5; i++)
  {
    m_wii_remotes[i]->setEnabled(enable_wiimotes);
    if (enable_wiimotes)
      m_wii_remotes[i]->setChecked(bt->AccessWiiMote(0x0100 + i)->IsConnected());
  }
}

void MenuBar::InstallWAD()
{
  QString wad_file = QFileDialog::getOpenFileName(this, tr("Select a title to install to NAND"),
                                                  QString(), tr("WAD files (*.wad)"));

  if (wad_file.isEmpty())
    return;

  QMessageBox result_dialog(this);

  if (WiiUtils::InstallWAD(wad_file.toStdString()))
  {
    Settings::Instance().NANDRefresh();
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

void MenuBar::OnSelectionChanged(std::shared_ptr<const UICommon::GameFile> game_file)
{
  const bool game_selected = !!game_file;

  m_recording_play->setEnabled(game_selected && !Core::IsRunning());
  m_recording_start->setEnabled(game_selected && !Movie::IsPlayingInput());
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

void MenuBar::ChangeDebugFont()
{
  bool okay;
  QFont font = QFontDialog::getFont(&okay, Settings::Instance().GetDebugFont(), this,
                                    tr("Pick a debug font"));

  if (okay)
    Settings::Instance().SetDebugFont(font);
}

void MenuBar::ClearSymbols()
{
  auto result = QMessageBox::warning(this, tr("Confirmation"),
                                     tr("Do you want to clear the list of symbol names?"),
                                     QMessageBox::Yes | QMessageBox::Cancel);

  if (result == QMessageBox::Cancel)
    return;

  g_symbolDB.Clear();
  Host_NotifyMapLoaded();
}

void MenuBar::GenerateSymbolsFromAddress()
{
  PPCAnalyst::FindFunctions(0x80000000, 0x81800000, &g_symbolDB);
  Host_NotifyMapLoaded();
}

void MenuBar::GenerateSymbolsFromSignatureDB()
{
  PPCAnalyst::FindFunctions(0x80000000, 0x81800000, &g_symbolDB);
  SignatureDB db(SignatureDB::HandlerType::DSY);
  if (db.Load(File::GetSysDirectory() + TOTALDB))
  {
    db.Apply(&g_symbolDB);
    QMessageBox::information(
        this, tr("Information"),
        tr("Generated symbol names from '%1'").arg(QString::fromStdString(TOTALDB)));
    db.List();
  }
  else
  {
    QMessageBox::critical(
        this, tr("Error"),
        tr("'%1' not found, no symbol names generated").arg(QString::fromStdString(TOTALDB)));
  }

  Host_NotifyMapLoaded();
}

void MenuBar::GenerateSymbolsFromRSO()
{
  QString text = QInputDialog::getText(this, tr("Input"), tr("Enter the RSO module address:"));
  bool good;
  uint address = text.toUInt(&good, 16);

  if (!good)
  {
    QMessageBox::warning(this, tr("Error"), tr("Invalid RSO module address: %1").arg(text));
    return;
  }

  RSOChainView rso_chain;
  if (rso_chain.Load(static_cast<u32>(address)))
  {
    rso_chain.Apply(&g_symbolDB);
    Host_NotifyMapLoaded();
  }
  else
  {
    QMessageBox::warning(this, tr("Error"), tr("Failed to load RSO module at %1").arg(text));
  }
}

void MenuBar::LoadSymbolMap()
{
  std::string existing_map_file, writable_map_file;
  bool map_exists = CBoot::FindMapFile(&existing_map_file, &writable_map_file);

  if (!map_exists)
  {
    g_symbolDB.Clear();
    PPCAnalyst::FindFunctions(0x81300000, 0x81800000, &g_symbolDB);
    SignatureDB db(SignatureDB::HandlerType::DSY);
    if (db.Load(File::GetSysDirectory() + TOTALDB))
      db.Apply(&g_symbolDB);

    QMessageBox::warning(this, tr("Warning"),
                         tr("'%1' not found, scanning for common functions instead")
                             .arg(QString::fromStdString(writable_map_file)));
  }
  else
  {
    g_symbolDB.LoadMap(existing_map_file);
    QMessageBox::information(
        this, tr("Information"),
        tr("Loaded symbols from '%1'").arg(QString::fromStdString(existing_map_file)));
  }

  HLE::PatchFunctions();
  Host_NotifyMapLoaded();
}

void MenuBar::SaveSymbolMap()
{
  std::string existing_map_file, writable_map_file;
  CBoot::FindMapFile(&existing_map_file, &writable_map_file);

  g_symbolDB.SaveSymbolMap(writable_map_file);
}

void MenuBar::LoadOtherSymbolMap()
{
  QString file = QFileDialog::getOpenFileName(this, tr("Load map file"),
                                              QString::fromStdString(File::GetUserPath(D_MAPS_IDX)),
                                              tr("Dolphin Map File (*.map)"));

  if (file.isEmpty())
    return;

  g_symbolDB.LoadMap(file.toStdString());
  HLE::PatchFunctions();
  Host_NotifyMapLoaded();
}

void MenuBar::SaveSymbolMapAs()
{
  const std::string& title_id_str = SConfig::GetInstance().m_debugger_game_id;
  QString file = QFileDialog::getSaveFileName(
      this, tr("Save map file"),
      QString::fromStdString(File::GetUserPath(D_MAPS_IDX) + "/" + title_id_str + ".map"),
      tr("Dolphin Map File (*.map)"));

  if (file.isEmpty())
    return;

  g_symbolDB.SaveSymbolMap(file.toStdString());
}

void MenuBar::SaveCode()
{
  std::string existing_map_file, writable_map_file;
  CBoot::FindMapFile(&existing_map_file, &writable_map_file);

  const std::string path =
      writable_map_file.substr(0, writable_map_file.find_last_of('.')) + "_code.map";

  g_symbolDB.SaveCodeMap(path);
}

void MenuBar::CreateSignatureFile()
{
  QString text = QInputDialog::getText(
      this, tr("Input"), tr("Only export symbols with prefix:\n(Blank for all symbols)"));

  if (text.isEmpty())
    return;

  std::string prefix = text.toStdString();

  QString file = QFileDialog::getSaveFileName(this, tr("Save signature file"));

  if (file.isEmpty())
    return;

  std::string save_path = file.toStdString();
  SignatureDB db(save_path);
  db.Populate(&g_symbolDB, prefix);
  db.Save(save_path);
  db.List();
}

void MenuBar::PatchHLEFunctions()
{
  HLE::PatchFunctions();
}
