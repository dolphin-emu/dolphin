// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/MenuBar.h"

#include <cinttypes>

#include <QAction>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFontDialog>
#include <QInputDialog>
#include <QMap>
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
#include "Core/HW/WiiSave.h"
#include "Core/HW/Wiimote.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Bluetooth/BTEmu.h"
#include "Core/Movie.h"
#include "Core/NetPlayProto.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/SignatureDB/SignatureDB.h"
#include "Core/State.h"
#include "Core/TitleDatabase.h"
#include "Core/WiiUtils.h"

#include "DiscIO/Enums.h"
#include "DiscIO/NANDImporter.h"
#include "DiscIO/WiiSaveBanner.h"

#include "DolphinQt/AboutDialog.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/Updater.h"

#include "UICommon/AutoUpdate.h"
#include "UICommon/GameFile.h"

QPointer<MenuBar> MenuBar::s_menu_bar;

MenuBar::MenuBar(QWidget* parent) : QMenuBar(parent)
{
  s_menu_bar = this;

  AddFileMenu();
  AddEmulationMenu();
  AddMovieMenu();
  AddOptionsMenu();
  AddToolsMenu();
  AddViewMenu();
  AddJITMenu();
  AddSymbolsMenu();
  AddHelpMenu();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          [=](Core::State state) { OnEmulationStateChanged(state); });
  connect(Host::GetInstance(), &Host::UpdateDisasmDialog, this,
          [this] { OnEmulationStateChanged(Core::GetState()); });

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
  {
    m_recording_stop->setEnabled(false);
    m_recording_export->setEnabled(false);
  }
  m_recording_play->setEnabled(!running);

  // Options
  m_controllers_action->setEnabled(NetPlay::IsNetPlayRunning() ? !running : true);

  // Tools
  m_show_cheat_manager->setEnabled(Settings::Instance().GetCheatsEnabled() && running);

  // JIT
  m_jit_interpreter_core->setEnabled(running);
  m_jit_block_linking->setEnabled(!running);
  m_jit_disable_cache->setEnabled(!running);
  m_jit_clear_cache->setEnabled(running);
  m_jit_log_coverage->setEnabled(!running);
  m_jit_search_instruction->setEnabled(running);

  for (QAction* action :
       {m_jit_off, m_jit_loadstore_off, m_jit_loadstore_lbzx_off, m_jit_loadstore_lxz_off,
        m_jit_loadstore_lwz_off, m_jit_loadstore_floating_off, m_jit_loadstore_paired_off,
        m_jit_floatingpoint_off, m_jit_integer_off, m_jit_paired_off, m_jit_systemregisters_off,
        m_jit_branch_off})
  {
    action->setEnabled(running && !playing);
  }

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
  m_show_memory->setVisible(enabled);
  m_show_jit->setVisible(enabled);

  if (enabled)
  {
    addMenu(m_jit);
    addMenu(m_symbols);
  }
  else
  {
    removeAction(m_jit->menuAction());
    removeAction(m_symbols->menuAction());
  }
}

void MenuBar::AddDVDBackupMenu(QMenu* file_menu)
{
  m_backup_menu = file_menu->addMenu(tr("&Boot from DVD Backup"));

  const std::vector<std::string> drives = Common::GetCDDevices();
  // Windows Limitation of 24 character drives
  for (size_t i = 0; i < drives.size() && i < 24; i++)
  {
    auto drive = QString::fromStdString(drives[i]);
    m_backup_menu->addAction(drive, this, [this, drive] { emit BootDVDBackup(drive); });
  }
}

void MenuBar::AddFileMenu()
{
  QMenu* file_menu = addMenu(tr("&File"));
  m_open_action = file_menu->addAction(tr("&Open..."), this, &MenuBar::Open,
                                       QKeySequence(QStringLiteral("Ctrl+O")));

  file_menu->addSeparator();

  m_change_disc = file_menu->addAction(tr("Change &Disc..."), this, &MenuBar::ChangeDisc);
  m_eject_disc = file_menu->addAction(tr("&Eject Disc"), this, &MenuBar::EjectDisc);

  AddDVDBackupMenu(file_menu);

  file_menu->addSeparator();

  m_exit_action = file_menu->addAction(tr("E&xit"), this, &MenuBar::Exit,
                                       QKeySequence(QStringLiteral("Alt+F4")));
}

void MenuBar::AddToolsMenu()
{
  QMenu* tools_menu = addMenu(tr("&Tools"));

  tools_menu->addAction(tr("&Resource Pack Manager"), this,
                        [this] { emit ShowResourcePackManager(); });

  m_show_cheat_manager =
      tools_menu->addAction(tr("&Cheats Manager"), this, [this] { emit ShowCheatsManager(); });

  connect(&Settings::Instance(), &Settings::EnableCheatsChanged, [this](bool enabled) {
    m_show_cheat_manager->setEnabled(Core::GetState() != Core::State::Uninitialized && enabled);
  });

  tools_menu->addAction(tr("FIFO Player"), this, &MenuBar::ShowFIFOPlayer);

  tools_menu->addSeparator();

  tools_menu->addAction(tr("Start &NetPlay..."), this, &MenuBar::StartNetPlay);
  tools_menu->addAction(tr("Browse &NetPlay Sessions...."), this, &MenuBar::BrowseNetPlay);

  tools_menu->addSeparator();

  QMenu* gc_ipl = tools_menu->addMenu(tr("Load GameCube Main Menu"));

  m_ntscj_ipl = gc_ipl->addAction(tr("NTSC-J"), this,
                                  [this] { emit BootGameCubeIPL(DiscIO::Region::NTSC_J); });
  m_ntscu_ipl = gc_ipl->addAction(tr("NTSC-U"), this,
                                  [this] { emit BootGameCubeIPL(DiscIO::Region::NTSC_U); });
  m_pal_ipl =
      gc_ipl->addAction(tr("PAL"), this, [this] { emit BootGameCubeIPL(DiscIO::Region::PAL); });

  tools_menu->addAction(tr("Memory Card Manager"), this, [this] { emit ShowMemcardManager(); });

  tools_menu->addSeparator();

  // Label will be set by a NANDRefresh later
  m_boot_sysmenu =
      tools_menu->addAction(QStringLiteral(""), this, [this] { emit BootWiiSystemMenu(); });
  m_wad_install_action = tools_menu->addAction(tr("Install WAD..."), this, &MenuBar::InstallWAD);
  m_manage_nand_menu = tools_menu->addMenu(tr("Manage NAND"));
  m_import_backup = m_manage_nand_menu->addAction(tr("Import BootMii NAND Backup..."), this,
                                                  [this] { emit ImportNANDBackup(); });
  m_check_nand = m_manage_nand_menu->addAction(tr("Check NAND..."), this, &MenuBar::CheckNAND);
  m_extract_certificates = m_manage_nand_menu->addAction(tr("Extract Certificates from NAND"), this,
                                                         &MenuBar::NANDExtractCertificates);

  m_boot_sysmenu->setEnabled(false);

  connect(&Settings::Instance(), &Settings::NANDRefresh, [this] { UpdateToolsMenu(false); });

  m_perform_online_update_menu = tools_menu->addMenu(tr("Perform Online System Update"));
  m_perform_online_update_for_current_region = m_perform_online_update_menu->addAction(
      tr("Current Region"), this, [this] { emit PerformOnlineUpdate(""); });
  m_perform_online_update_menu->addSeparator();
  m_perform_online_update_menu->addAction(tr("Europe"), this,
                                          [this] { emit PerformOnlineUpdate("EUR"); });
  m_perform_online_update_menu->addAction(tr("Japan"), this,
                                          [this] { emit PerformOnlineUpdate("JPN"); });
  m_perform_online_update_menu->addAction(tr("Korea"), this,
                                          [this] { emit PerformOnlineUpdate("KOR"); });
  m_perform_online_update_menu->addAction(tr("United States"), this,
                                          [this] { emit PerformOnlineUpdate("USA"); });

  tools_menu->addSeparator();

  tools_menu->addAction(tr("Import Wii Save..."), this, &MenuBar::ImportWiiSave);
  tools_menu->addAction(tr("Export All Wii Saves"), this, &MenuBar::ExportWiiSaves);

  QMenu* menu = new QMenu(tr("Connect Wii Remotes"), tools_menu);

  tools_menu->addSeparator();
  tools_menu->addMenu(menu);

  for (int i = 0; i < 4; i++)
  {
    m_wii_remotes[i] = menu->addAction(tr("Connect Wii Remote %1").arg(i + 1), this,
                                       [this, i] { emit ConnectWiiRemote(i); });
    m_wii_remotes[i]->setCheckable(true);
  }

  menu->addSeparator();

  m_wii_remotes[4] =
      menu->addAction(tr("Connect Balance Board"), this, [this] { emit ConnectWiiRemote(4); });
  m_wii_remotes[4]->setCheckable(true);
}

void MenuBar::AddEmulationMenu()
{
  QMenu* emu_menu = addMenu(tr("&Emulation"));
  m_play_action = emu_menu->addAction(tr("&Play"), this, &MenuBar::Play);
  m_pause_action = emu_menu->addAction(tr("&Pause"), this, &MenuBar::Pause);
  m_stop_action = emu_menu->addAction(tr("&Stop"), this, &MenuBar::Stop);
  m_reset_action = emu_menu->addAction(tr("&Reset"), this, &MenuBar::Reset);
  m_fullscreen_action = emu_menu->addAction(tr("Toggle &Fullscreen"), this, &MenuBar::Fullscreen);
  m_frame_advance_action = emu_menu->addAction(tr("&Frame Advance"), this, &MenuBar::FrameAdvance);

  m_screenshot_action = emu_menu->addAction(tr("Take Screenshot"), this, &MenuBar::Screenshot);

  emu_menu->addSeparator();

  AddStateLoadMenu(emu_menu);
  AddStateSaveMenu(emu_menu);
  AddStateSlotMenu(emu_menu);
  UpdateStateSlotMenu();
}

void MenuBar::AddStateLoadMenu(QMenu* emu_menu)
{
  m_state_load_menu = emu_menu->addMenu(tr("&Load State"));
  m_state_load_menu->addAction(tr("Load State from File"), this, &MenuBar::StateLoad);
  m_state_load_menu->addAction(tr("Load State from Selected Slot"), this, &MenuBar::StateLoadSlot);
  m_state_load_slots_menu = m_state_load_menu->addMenu(tr("Load State from Slot"));
  m_state_load_menu->addAction(tr("Undo Load State"), this, &MenuBar::StateLoadUndo);

  for (int i = 1; i <= 10; i++)
  {
    QAction* action = m_state_load_slots_menu->addAction(QStringLiteral(""));

    connect(action, &QAction::triggered, this, [=]() { emit StateLoadSlotAt(i); });
  }
}

void MenuBar::AddStateSaveMenu(QMenu* emu_menu)
{
  m_state_save_menu = emu_menu->addMenu(tr("Sa&ve State"));
  m_state_save_menu->addAction(tr("Save State to File"), this, &MenuBar::StateSave);
  m_state_save_menu->addAction(tr("Save State to Selected Slot"), this, &MenuBar::StateSaveSlot);
  m_state_save_menu->addAction(tr("Save State to Oldest Slot"), this, &MenuBar::StateSaveOldest);
  m_state_save_slots_menu = m_state_save_menu->addMenu(tr("Save State to Slot"));
  m_state_save_menu->addAction(tr("Undo Save State"), this, &MenuBar::StateSaveUndo);

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

  QAction* show_toolbar = view_menu->addAction(tr("Show &Toolbar"));
  show_toolbar->setCheckable(true);
  show_toolbar->setChecked(Settings::Instance().IsToolBarVisible());

  connect(show_toolbar, &QAction::toggled, &Settings::Instance(), &Settings::SetToolBarVisible);

  connect(&Settings::Instance(), &Settings::LogVisibilityChanged, show_log, &QAction::setChecked);
  connect(&Settings::Instance(), &Settings::LogConfigVisibilityChanged, show_log_config,
          &QAction::setChecked);
  connect(&Settings::Instance(), &Settings::ToolBarVisibilityChanged, show_toolbar,
          &QAction::setChecked);

  QAction* lock_widgets = view_menu->addAction(tr("&Lock Widgets In Place"));
  lock_widgets->setCheckable(true);
  lock_widgets->setChecked(Settings::Instance().AreWidgetsLocked());

  connect(lock_widgets, &QAction::toggled, &Settings::Instance(), &Settings::SetWidgetsLocked);

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

  // i18n: This kind of "watch" is used for watching emulated memory.
  // It's not related to timekeeping devices.
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

  m_show_memory = view_menu->addAction(tr("&Memory"));
  m_show_memory->setCheckable(true);
  m_show_memory->setChecked(Settings::Instance().IsMemoryVisible());

  connect(m_show_memory, &QAction::toggled, &Settings::Instance(), &Settings::SetMemoryVisible);
  connect(&Settings::Instance(), &Settings::MemoryVisibilityChanged, m_show_memory,
          &QAction::setChecked);

  m_show_jit = view_menu->addAction(tr("&JIT"));
  m_show_jit->setCheckable(true);
  m_show_jit->setChecked(Settings::Instance().IsJITVisible());
  connect(m_show_jit, &QAction::toggled, &Settings::Instance(), &Settings::SetJITVisible);
  connect(&Settings::Instance(), &Settings::JITVisibilityChanged, m_show_jit, &QAction::setChecked);

  view_menu->addSeparator();

  AddGameListTypeSection(view_menu);
  view_menu->addSeparator();
  AddListColumnsMenu(view_menu);
  view_menu->addSeparator();
  AddShowPlatformsMenu(view_menu);
  AddShowRegionsMenu(view_menu);

  view_menu->addSeparator();
  view_menu->addAction(tr("Purge Game List Cache"), this, &MenuBar::PurgeGameListCache);
  view_menu->addSeparator();
  view_menu->addAction(tr("Search"), this, &MenuBar::ShowSearch,
                       QKeySequence(QStringLiteral("Ctrl+F")));
}

void MenuBar::AddOptionsMenu()
{
  QMenu* options_menu = addMenu(tr("&Options"));
  options_menu->addAction(tr("Co&nfiguration"), this, &MenuBar::Configure);
  options_menu->addSeparator();
  options_menu->addAction(tr("&Graphics Settings"), this, &MenuBar::ConfigureGraphics);
  options_menu->addAction(tr("&Audio Settings"), this, &MenuBar::ConfigureAudio);
  m_controllers_action =
      options_menu->addAction(tr("&Controller Settings"), this, &MenuBar::ConfigureControllers);
  options_menu->addAction(tr("&Hotkey Settings"), this, &MenuBar::ConfigureHotkeys);

  options_menu->addSeparator();

  // Debugging mode only
  m_boot_to_pause = options_menu->addAction(tr("Boot to Pause"));
  m_boot_to_pause->setCheckable(true);
  m_boot_to_pause->setChecked(SConfig::GetInstance().bBootToPause);

  connect(m_boot_to_pause, &QAction::toggled, this,
          [](bool enable) { SConfig::GetInstance().bBootToPause = enable; });

  m_automatic_start = options_menu->addAction(tr("&Automatic Start"));
  m_automatic_start->setCheckable(true);
  m_automatic_start->setChecked(SConfig::GetInstance().bAutomaticStart);

  connect(m_automatic_start, &QAction::toggled, this,
          [](bool enable) { SConfig::GetInstance().bAutomaticStart = enable; });

  m_change_font = options_menu->addAction(tr("&Font..."), this, &MenuBar::ChangeDebugFont);
}

void MenuBar::InstallUpdateManually()
{
  auto& track = SConfig::GetInstance().m_auto_update_track;
  auto previous_value = track;

  track = "dev";

  auto* updater = new Updater(this->parentWidget());

  if (!updater->CheckForUpdate())
  {
    ModalMessageBox::information(
        this, tr("Update"),
        tr("You are running the latest version available on this update track."));
  }

  track = previous_value;
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

  if (AutoUpdateChecker::SystemSupportsAutoUpdates())
  {
    help_menu->addSeparator();

    help_menu->addAction(tr("&Check for Updates..."), this, &MenuBar::InstallUpdateManually);
  }

#ifndef __APPLE__
  help_menu->addSeparator();
#endif

  help_menu->addAction(tr("&About"), this, &MenuBar::ShowAboutDialog);
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
      {tr("Tags"), &SConfig::GetInstance().m_showTagsColumn}};

  QActionGroup* column_group = new QActionGroup(this);
  m_cols_menu = view_menu->addMenu(tr("List Columns"));
  column_group->setExclusive(false);

  for (const auto& key : columns.keys())
  {
    bool* config = columns[key];
    QAction* action = column_group->addAction(m_cols_menu->addAction(key));
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
      movie_menu->addAction(tr("Start Re&cording Input"), this, [this] { emit StartRecording(); });
  m_recording_play =
      movie_menu->addAction(tr("P&lay Input Recording..."), this, [this] { emit PlayRecording(); });
  m_recording_stop = movie_menu->addAction(tr("Stop Playing/Recording Input"), this,
                                           [this] { emit StopRecording(); });
  m_recording_export =
      movie_menu->addAction(tr("Export Recording..."), this, [this] { emit ExportRecording(); });

  m_recording_start->setEnabled(false);
  m_recording_play->setEnabled(false);
  m_recording_stop->setEnabled(false);
  m_recording_export->setEnabled(false);

  m_recording_read_only = movie_menu->addAction(tr("&Read-Only Mode"));
  m_recording_read_only->setCheckable(true);
  m_recording_read_only->setChecked(Movie::IsReadOnly());
  connect(m_recording_read_only, &QAction::toggled, [](bool value) { Movie::SetReadOnly(value); });

  movie_menu->addAction(tr("TAS Input"), this, [this] { emit ShowTASInput(); });

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
  connect(input_display, &QAction::toggled,
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

void MenuBar::AddJITMenu()
{
  m_jit = addMenu(tr("JIT"));

  m_jit_interpreter_core = m_jit->addAction(tr("Interpreter Core"));
  m_jit_interpreter_core->setCheckable(true);
  m_jit_interpreter_core->setChecked(SConfig::GetInstance().cpu_core ==
                                     PowerPC::CPUCore::Interpreter);

  connect(m_jit_interpreter_core, &QAction::toggled, [](bool enabled) {
    PowerPC::SetMode(enabled ? PowerPC::CoreMode::Interpreter : PowerPC::CoreMode::JIT);
  });

  m_jit->addSeparator();

  m_jit_block_linking = m_jit->addAction(tr("JIT Block Linking Off"));
  m_jit_block_linking->setCheckable(true);
  m_jit_block_linking->setChecked(SConfig::GetInstance().bJITNoBlockLinking);
  connect(m_jit_block_linking, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITNoBlockLinking = enabled;
    ClearCache();
  });

  m_jit_disable_cache = m_jit->addAction(tr("Disable JIT Cache"));
  m_jit_disable_cache->setCheckable(true);
  m_jit_disable_cache->setChecked(SConfig::GetInstance().bJITNoBlockCache);
  connect(m_jit_disable_cache, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITNoBlockCache = enabled;
    ClearCache();
  });

  m_jit_clear_cache = m_jit->addAction(tr("Clear Cache"), this, &MenuBar::ClearCache);

  m_jit->addSeparator();

  m_jit_log_coverage =
      m_jit->addAction(tr("Log JIT Instruction Coverage"), this, &MenuBar::LogInstructions);
  m_jit_search_instruction =
      m_jit->addAction(tr("Search for an Instruction"), this, &MenuBar::SearchInstruction);

  m_jit->addSeparator();

  m_jit_off = m_jit->addAction(tr("JIT Off (JIT Core)"));
  m_jit_off->setCheckable(true);
  m_jit_off->setChecked(SConfig::GetInstance().bJITOff);
  connect(m_jit_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITOff = enabled;
    ClearCache();
  });

  m_jit_loadstore_off = m_jit->addAction(tr("JIT LoadStore Off"));
  m_jit_loadstore_off->setCheckable(true);
  m_jit_loadstore_off->setChecked(SConfig::GetInstance().bJITLoadStoreOff);
  connect(m_jit_loadstore_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITLoadStoreOff = enabled;
    ClearCache();
  });

  m_jit_loadstore_lbzx_off = m_jit->addAction(tr("JIT LoadStore lbzx Off"));
  m_jit_loadstore_lbzx_off->setCheckable(true);
  m_jit_loadstore_lbzx_off->setChecked(SConfig::GetInstance().bJITLoadStorelbzxOff);
  connect(m_jit_loadstore_lbzx_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITLoadStorelbzxOff = enabled;
    ClearCache();
  });

  m_jit_loadstore_lxz_off = m_jit->addAction(tr("JIT LoadStore lXz Off"));
  m_jit_loadstore_lxz_off->setCheckable(true);
  m_jit_loadstore_lxz_off->setChecked(SConfig::GetInstance().bJITLoadStorelXzOff);
  connect(m_jit_loadstore_lxz_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITLoadStorelXzOff = enabled;
    ClearCache();
  });

  m_jit_loadstore_lwz_off = m_jit->addAction(tr("JIT LoadStore lwz Off"));
  m_jit_loadstore_lwz_off->setCheckable(true);
  m_jit_loadstore_lwz_off->setChecked(SConfig::GetInstance().bJITLoadStorelwzOff);
  connect(m_jit_loadstore_lwz_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITLoadStorelwzOff = enabled;
    ClearCache();
  });

  m_jit_loadstore_floating_off = m_jit->addAction(tr("JIT LoadStore Floating Off"));
  m_jit_loadstore_floating_off->setCheckable(true);
  m_jit_loadstore_floating_off->setChecked(SConfig::GetInstance().bJITLoadStoreFloatingOff);
  connect(m_jit_loadstore_floating_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITLoadStoreFloatingOff = enabled;
    ClearCache();
  });

  m_jit_loadstore_paired_off = m_jit->addAction(tr("JIT LoadStore Paired Off"));
  m_jit_loadstore_paired_off->setCheckable(true);
  m_jit_loadstore_paired_off->setChecked(SConfig::GetInstance().bJITLoadStorePairedOff);
  connect(m_jit_loadstore_paired_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITLoadStorePairedOff = enabled;
    ClearCache();
  });

  m_jit_floatingpoint_off = m_jit->addAction(tr("JIT FloatingPoint Off"));
  m_jit_floatingpoint_off->setCheckable(true);
  m_jit_floatingpoint_off->setChecked(SConfig::GetInstance().bJITFloatingPointOff);
  connect(m_jit_floatingpoint_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITFloatingPointOff = enabled;
    ClearCache();
  });

  m_jit_integer_off = m_jit->addAction(tr("JIT Integer Off"));
  m_jit_integer_off->setCheckable(true);
  m_jit_integer_off->setChecked(SConfig::GetInstance().bJITIntegerOff);
  connect(m_jit_integer_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITIntegerOff = enabled;
    ClearCache();
  });

  m_jit_paired_off = m_jit->addAction(tr("JIT Paired Off"));
  m_jit_paired_off->setCheckable(true);
  m_jit_paired_off->setChecked(SConfig::GetInstance().bJITPairedOff);
  connect(m_jit_paired_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITPairedOff = enabled;
    ClearCache();
  });

  m_jit_systemregisters_off = m_jit->addAction(tr("JIT SystemRegisters Off"));
  m_jit_systemregisters_off->setCheckable(true);
  m_jit_systemregisters_off->setChecked(SConfig::GetInstance().bJITSystemRegistersOff);
  connect(m_jit_systemregisters_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITSystemRegistersOff = enabled;
    ClearCache();
  });

  m_jit_branch_off = m_jit->addAction(tr("JIT Branch Off"));
  m_jit_branch_off->setCheckable(true);
  m_jit_branch_off->setChecked(SConfig::GetInstance().bJITBranchOff);
  connect(m_jit_branch_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITBranchOff = enabled;
    ClearCache();
  });
}

void MenuBar::AddSymbolsMenu()
{
  m_symbols = addMenu(tr("Symbols"));

  m_symbols->addAction(tr("&Clear Symbols"), this, &MenuBar::ClearSymbols);

  auto* generate = m_symbols->addMenu(tr("&Generate Symbols From"));
  generate->addAction(tr("Address"), this, &MenuBar::GenerateSymbolsFromAddress);
  generate->addAction(tr("Signature Database"), this, &MenuBar::GenerateSymbolsFromSignatureDB);
  generate->addAction(tr("RSO Modules"), this, &MenuBar::GenerateSymbolsFromRSO);
  m_symbols->addSeparator();

  m_symbols->addAction(tr("&Load Symbol Map"), this, &MenuBar::LoadSymbolMap);
  m_symbols->addAction(tr("&Save Symbol Map"), this, &MenuBar::SaveSymbolMap);
  m_symbols->addSeparator();

  m_symbols->addAction(tr("Load &Other Map File..."), this, &MenuBar::LoadOtherSymbolMap);
  m_symbols->addAction(tr("Load &Bad Map File..."), this, &MenuBar::LoadBadSymbolMap);
  m_symbols->addAction(tr("Save Symbol Map &As..."), this, &MenuBar::SaveSymbolMapAs);
  m_symbols->addSeparator();

  m_symbols->addAction(tr("Sa&ve Code"), this, &MenuBar::SaveCode);
  m_symbols->addSeparator();

  m_symbols->addAction(tr("C&reate Signature File..."), this, &MenuBar::CreateSignatureFile);
  m_symbols->addAction(tr("Append to &Existing Signature File..."), this,
                       &MenuBar::AppendSignatureFile);
  m_symbols->addAction(tr("Combine &Two Signature Files..."), this,
                       &MenuBar::CombineSignatureFiles);
  m_symbols->addAction(tr("Appl&y Signature File..."), this, &MenuBar::ApplySignatureFile);
  m_symbols->addSeparator();

  m_symbols->addAction(tr("&Patch HLE Functions"), this, &MenuBar::PatchHLEFunctions);
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
  const bool enable_wiimotes =
      emulation_started && bt && !SConfig::GetInstance().m_bt_passthrough_enabled;

  for (std::size_t i = 0; i < m_wii_remotes.size(); i++)
  {
    QAction* const wii_remote = m_wii_remotes[i];

    wii_remote->setEnabled(enable_wiimotes);
    if (enable_wiimotes)
      wii_remote->setChecked(bt->AccessWiimoteByIndex(i)->IsConnected());
  }
}

void MenuBar::InstallWAD()
{
  QString wad_file = QFileDialog::getOpenFileName(this, tr("Select a title to install to NAND"),
                                                  QString(), tr("WAD files (*.wad)"));

  if (wad_file.isEmpty())
    return;

  if (WiiUtils::InstallWAD(wad_file.toStdString()))
  {
    Settings::Instance().NANDRefresh();
    ModalMessageBox::information(this, tr("Success"),
                                 tr("Successfully installed this title to the NAND."));
  }
  else
  {
    ModalMessageBox::critical(this, tr("Failure"), tr("Failed to install this title to the NAND."));
  }
}

void MenuBar::ImportWiiSave()
{
  QString file = QFileDialog::getOpenFileName(this, tr("Select the save file"), QDir::currentPath(),
                                              tr("Wii save files (*.bin);;"
                                                 "All Files (*)"));

  if (file.isEmpty())
    return;

  bool cancelled = false;
  auto can_overwrite = [&] {
    bool yes = ModalMessageBox::question(
                   this, tr("Save Import"),
                   tr("Save data for this title already exists in the NAND. Consider backing up "
                      "the current data before overwriting.\nOverwrite now?")) == QMessageBox::Yes;
    cancelled = !yes;
    return yes;
  };
  if (WiiSave::Import(file.toStdString(), can_overwrite))
    ModalMessageBox::information(this, tr("Save Import"), tr("Successfully imported save files."));
  else if (!cancelled)
    ModalMessageBox::critical(this, tr("Save Import"), tr("Failed to import save files."));
}

void MenuBar::ExportWiiSaves()
{
  const QString export_dir = QFileDialog::getExistingDirectory(
      this, tr("Select Export Directory"), QString::fromStdString(File::GetUserPath(D_USER_IDX)),
      QFileDialog::ShowDirsOnly);
  if (export_dir.isEmpty())
    return;

  const size_t count = WiiSave::ExportAll(export_dir.toStdString());
  ModalMessageBox::information(this, tr("Save Export"),
                               tr("Exported %n save(s)", "", static_cast<int>(count)));
}

void MenuBar::CheckNAND()
{
  IOS::HLE::Kernel ios;
  WiiUtils::NANDCheckResult result = WiiUtils::CheckNAND(ios);
  if (!result.bad)
  {
    ModalMessageBox::information(this, tr("NAND Check"), tr("No issues have been detected."));
    return;
  }

  QString message = tr("The emulated NAND is damaged. System titles such as the Wii Menu and "
                       "the Wii Shop Channel may not work correctly.\n\n"
                       "Do you want to try to repair the NAND?");
  if (!result.titles_to_remove.empty())
  {
    std::string title_listings;
    Core::TitleDatabase title_db;
    const DiscIO::Language language = SConfig::GetInstance().GetCurrentLanguage(true);
    for (const u64 title_id : result.titles_to_remove)
    {
      title_listings += StringFromFormat("%016" PRIx64, title_id);

      const std::string database_name = title_db.GetChannelName(title_id, language);
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

  if (ModalMessageBox::question(this, tr("NAND Check"), message) != QMessageBox::Yes)
    return;

  if (WiiUtils::RepairNAND(ios))
  {
    ModalMessageBox::information(this, tr("NAND Check"), tr("The NAND has been repaired."));
    return;
  }

  ModalMessageBox::critical(this, tr("NAND Check"),
                            tr("The NAND could not be repaired. It is recommended to back up "
                               "your current data and start over with a fresh NAND."));
}

void MenuBar::NANDExtractCertificates()
{
  if (DiscIO::NANDImporter().ExtractCertificates(File::GetUserPath(D_WIIROOT_IDX)))
  {
    ModalMessageBox::information(this, tr("Success"),
                                 tr("Successfully extracted certificates from NAND"));
  }
  else
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Failed to extract certificates from NAND"));
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
  m_recording_export->setEnabled(recording);
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
  auto result = ModalMessageBox::warning(this, tr("Confirmation"),
                                         tr("Do you want to clear the list of symbol names?"),
                                         QMessageBox::Yes | QMessageBox::Cancel);

  if (result == QMessageBox::Cancel)
    return;

  g_symbolDB.Clear();
  emit NotifySymbolsUpdated();
}

void MenuBar::GenerateSymbolsFromAddress()
{
  PPCAnalyst::FindFunctions(0x80000000, 0x81800000, &g_symbolDB);
  emit NotifySymbolsUpdated();
}

void MenuBar::GenerateSymbolsFromSignatureDB()
{
  PPCAnalyst::FindFunctions(0x80000000, 0x81800000, &g_symbolDB);
  SignatureDB db(SignatureDB::HandlerType::DSY);
  if (db.Load(File::GetSysDirectory() + TOTALDB))
  {
    db.Apply(&g_symbolDB);
    ModalMessageBox::information(
        this, tr("Information"),
        tr("Generated symbol names from '%1'").arg(QString::fromStdString(TOTALDB)));
    db.List();
  }
  else
  {
    ModalMessageBox::critical(
        this, tr("Error"),
        tr("'%1' not found, no symbol names generated").arg(QString::fromStdString(TOTALDB)));
  }

  emit NotifySymbolsUpdated();
}

void MenuBar::GenerateSymbolsFromRSO()
{
  QString text = QInputDialog::getText(this, tr("Input"), tr("Enter the RSO module address:"));
  bool good;
  uint address = text.toUInt(&good, 16);

  if (!good)
  {
    ModalMessageBox::warning(this, tr("Error"), tr("Invalid RSO module address: %1").arg(text));
    return;
  }

  RSOChainView rso_chain;
  if (rso_chain.Load(static_cast<u32>(address)))
  {
    rso_chain.Apply(&g_symbolDB);
    emit NotifySymbolsUpdated();
  }
  else
  {
    ModalMessageBox::warning(this, tr("Error"), tr("Failed to load RSO module at %1").arg(text));
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

    ModalMessageBox::warning(this, tr("Warning"),
                             tr("'%1' not found, scanning for common functions instead")
                                 .arg(QString::fromStdString(writable_map_file)));
  }
  else
  {
    const QString existing_map_file_path = QString::fromStdString(existing_map_file);

    if (!TryLoadMapFile(existing_map_file_path))
      return;

    ModalMessageBox::information(this, tr("Information"),
                                 tr("Loaded symbols from '%1'").arg(existing_map_file_path));
  }

  HLE::PatchFunctions();
  emit NotifySymbolsUpdated();
}

void MenuBar::SaveSymbolMap()
{
  std::string existing_map_file, writable_map_file;
  CBoot::FindMapFile(&existing_map_file, &writable_map_file);

  TrySaveSymbolMap(QString::fromStdString(writable_map_file));
}

void MenuBar::LoadOtherSymbolMap()
{
  const QString file = QFileDialog::getOpenFileName(
      this, tr("Load map file"), QString::fromStdString(File::GetUserPath(D_MAPS_IDX)),
      tr("Dolphin Map File (*.map)"));

  if (file.isEmpty())
    return;

  if (!TryLoadMapFile(file))
    return;

  HLE::PatchFunctions();
  emit NotifySymbolsUpdated();
}

void MenuBar::LoadBadSymbolMap()
{
  const QString file = QFileDialog::getOpenFileName(
      this, tr("Load map file"), QString::fromStdString(File::GetUserPath(D_MAPS_IDX)),
      tr("Dolphin Map File (*.map)"));

  if (file.isEmpty())
    return;

  if (!TryLoadMapFile(file, true))
    return;

  HLE::PatchFunctions();
  emit NotifySymbolsUpdated();
}

void MenuBar::SaveSymbolMapAs()
{
  const std::string& title_id_str = SConfig::GetInstance().m_debugger_game_id;
  const QString file = QFileDialog::getSaveFileName(
      this, tr("Save map file"),
      QString::fromStdString(File::GetUserPath(D_MAPS_IDX) + "/" + title_id_str + ".map"),
      tr("Dolphin Map File (*.map)"));

  if (file.isEmpty())
    return;

  TrySaveSymbolMap(file);
}

void MenuBar::SaveCode()
{
  std::string existing_map_file, writable_map_file;
  CBoot::FindMapFile(&existing_map_file, &writable_map_file);

  const std::string path =
      writable_map_file.substr(0, writable_map_file.find_last_of('.')) + "_code.map";

  if (!g_symbolDB.SaveCodeMap(path))
  {
    ModalMessageBox::warning(
        this, tr("Error"),
        tr("Failed to save code map to path '%1'").arg(QString::fromStdString(path)));
  }
}

bool MenuBar::TryLoadMapFile(const QString& path, const bool bad)
{
  if (!g_symbolDB.LoadMap(path.toStdString(), bad))
  {
    ModalMessageBox::warning(this, tr("Error"), tr("Failed to load map file '%1'").arg(path));
    return false;
  }

  return true;
}

void MenuBar::TrySaveSymbolMap(const QString& path)
{
  if (g_symbolDB.SaveSymbolMap(path.toStdString()))
    return;

  ModalMessageBox::warning(this, tr("Error"),
                           tr("Failed to save symbol map to path '%1'").arg(path));
}

void MenuBar::CreateSignatureFile()
{
  const QString text = QInputDialog::getText(
      this, tr("Input"), tr("Only export symbols with prefix:\n(Blank for all symbols)"));

  const QString file = QFileDialog::getSaveFileName(
      this, tr("Save signature file"), QDir::homePath(), tr("Function signature file (*.dsy)"));
  if (file.isEmpty())
    return;

  const std::string prefix = text.toStdString();
  const std::string save_path = file.toStdString();
  SignatureDB db(save_path);
  db.Populate(&g_symbolDB, prefix);

  if (!db.Save(save_path))
  {
    ModalMessageBox::warning(this, tr("Error"), tr("Failed to save signature file '%1'").arg(file));
    return;
  }

  db.List();
}

void MenuBar::AppendSignatureFile()
{
  const QString text = QInputDialog::getText(
      this, tr("Input"), tr("Only append symbols with prefix:\n(Blank for all symbols)"));

  const QString file = QFileDialog::getSaveFileName(
      this, tr("Append signature to"), QDir::homePath(), tr("Function signature file (*.dsy)"));
  if (file.isEmpty())
    return;

  const std::string prefix = text.toStdString();
  const std::string signature_path = file.toStdString();
  SignatureDB db(signature_path);
  db.Populate(&g_symbolDB, prefix);
  db.List();
  db.Load(signature_path);
  if (!db.Save(signature_path))
  {
    ModalMessageBox::warning(this, tr("Error"),
                             tr("Failed to append to signature file '%1'").arg(file));
    return;
  }

  db.List();
}

void MenuBar::ApplySignatureFile()
{
  const QString file = QFileDialog::getOpenFileName(
      this, tr("Apply signature file"), QDir::homePath(), tr("Function signature file (*.dsy)"));

  if (file.isEmpty())
    return;

  const std::string load_path = file.toStdString();
  SignatureDB db(load_path);
  db.Load(load_path);
  db.Apply(&g_symbolDB);
  db.List();
  HLE::PatchFunctions();
  emit NotifySymbolsUpdated();
}

void MenuBar::CombineSignatureFiles()
{
  const QString priorityFile =
      QFileDialog::getOpenFileName(this, tr("Choose priority input file"), QDir::homePath(),
                                   tr("Function signature file (*.dsy)"));
  if (priorityFile.isEmpty())
    return;

  const QString secondaryFile =
      QFileDialog::getOpenFileName(this, tr("Choose secondary input file"), QDir::homePath(),
                                   tr("Function signature file (*.dsy)"));
  if (secondaryFile.isEmpty())
    return;

  const QString saveFile =
      QFileDialog::getSaveFileName(this, tr("Save combined output file as"), QDir::homePath(),
                                   tr("Function signature file (*.dsy)"));
  if (saveFile.isEmpty())
    return;

  const std::string load_pathPriorityFile = priorityFile.toStdString();
  const std::string load_pathSecondaryFile = secondaryFile.toStdString();
  const std::string save_path = saveFile.toStdString();
  SignatureDB db(load_pathPriorityFile);
  db.Load(load_pathPriorityFile);
  db.Load(load_pathSecondaryFile);
  if (!db.Save(save_path))
  {
    ModalMessageBox::warning(this, tr("Error"),
                             tr("Failed to save to signature file '%1'").arg(saveFile));
    return;
  }

  db.List();
}

void MenuBar::PatchHLEFunctions()
{
  HLE::PatchFunctions();
}

void MenuBar::ClearCache()
{
  Core::RunAsCPUThread(JitInterface::ClearCache);
}

void MenuBar::LogInstructions()
{
  PPCTables::LogCompiledInstructions();
}

void MenuBar::SearchInstruction()
{
  bool good;
  QString op = QInputDialog::getText(this, tr("Search instruction"), tr("Instruction:"),
                                     QLineEdit::Normal, QStringLiteral(""), &good);

  if (!good)
    return;

  bool found = false;
  for (u32 addr = 0x80000000; addr < 0x81800000; addr += 4)
  {
    auto ins_name =
        QString::fromStdString(PPCTables::GetInstructionName(PowerPC::HostRead_U32(addr)));
    if (op == ins_name)
    {
      NOTICE_LOG(POWERPC, "Found %s at %08x", op.toStdString().c_str(), addr);
      found = true;
    }
  }
  if (!found)
    NOTICE_LOG(POWERPC, "Opcode %s not found", op.toStdString().c_str());
}
