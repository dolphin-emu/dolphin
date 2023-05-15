// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/MenuBar.h"

#include <cinttypes>
#include <future>

#include <QAction>
#include <QActionGroup>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFontDialog>
#include <QInputDialog>
#include <QMap>
#include <QUrl>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/Boot/Boot.h"
#include "Core/CommonTitles.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Debugger/RSO.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/AddressSpace.h"
#include "Core/HW/Memmap.h"
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
#include "Core/System.h"
#include "Core/TitleDatabase.h"
#include "Core/WiiUtils.h"

#include "DiscIO/Enums.h"
#include "DiscIO/NANDImporter.h"
#include "DiscIO/WiiSaveBanner.h"

#include "DolphinQt/AboutDialog.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/NANDRepairDialog.h"
#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/ParallelProgressDialog.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/Updater.h"

#include "UICommon/AutoUpdate.h"
#include "UICommon/GameFile.h"

QPointer<MenuBar> MenuBar::s_menu_bar;

QString MenuBar::GetSignatureSelector() const
{
  return QStringLiteral("%1 (*.dsy);; %2 (*.csv);; %3 (*.mega)")
      .arg(tr("Dolphin Signature File"), tr("Dolphin Signature CSV File"),
           tr("WiiTools Signature MEGA File"));
}

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
          [=, this](Core::State state) { OnEmulationStateChanged(state); });
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
  m_recording_play->setEnabled(m_game_selected && !running);
  m_recording_start->setEnabled((m_game_selected || running) && !Movie::IsPlayingInput());

  // JIT
  m_jit_interpreter_core->setEnabled(running);
  m_jit_block_linking->setEnabled(!running);
  m_jit_disable_cache->setEnabled(!running);
  m_jit_disable_fastmem->setEnabled(!running);
  m_jit_clear_cache->setEnabled(running);
  m_jit_log_coverage->setEnabled(!running);
  m_jit_search_instruction->setEnabled(running);

  for (QAction* action :
       {m_jit_off, m_jit_loadstore_off, m_jit_loadstore_lbzx_off, m_jit_loadstore_lxz_off,
        m_jit_loadstore_lwz_off, m_jit_loadstore_floating_off, m_jit_loadstore_paired_off,
        m_jit_floatingpoint_off, m_jit_integer_off, m_jit_paired_off, m_jit_systemregisters_off,
        m_jit_branch_off, m_jit_register_cache_off})
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
  m_reset_ignore_panic_handler->setVisible(enabled);
  m_change_font->setVisible(enabled);

  // View
  m_show_code->setVisible(enabled);
  m_show_registers->setVisible(enabled);
  m_show_threads->setVisible(enabled);
  m_show_watch->setVisible(enabled);
  m_show_breakpoints->setVisible(enabled);
  m_show_memory->setVisible(enabled);
  m_show_network->setVisible(enabled);
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

void MenuBar::AddFileMenu()
{
  QMenu* file_menu = addMenu(tr("&File"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  m_open_action = file_menu->addAction(tr("&Open..."), QKeySequence::Open, this, &MenuBar::Open);
#else
  m_open_action = file_menu->addAction(tr("&Open..."), this, &MenuBar::Open, QKeySequence::Open);
#endif

  file_menu->addSeparator();

  m_change_disc = file_menu->addAction(tr("Change &Disc..."), this, &MenuBar::ChangeDisc);
  m_eject_disc = file_menu->addAction(tr("&Eject Disc"), this, &MenuBar::EjectDisc);

  file_menu->addSeparator();

  m_open_user_folder =
      file_menu->addAction(tr("Open &User Folder"), this, &MenuBar::OpenUserFolder);

  file_menu->addSeparator();

  m_exit_action = file_menu->addAction(tr("E&xit"), this, &MenuBar::Exit);
  m_exit_action->setShortcuts({QKeySequence::Quit, QKeySequence(Qt::ALT | Qt::Key_F4)});
}

void MenuBar::AddToolsMenu()
{
  QMenu* tools_menu = addMenu(tr("&Tools"));

  tools_menu->addAction(tr("&Resource Pack Manager"), this,
                        [this] { emit ShowResourcePackManager(); });

  tools_menu->addAction(tr("&Cheats Manager"), this, [this] { emit ShowCheatsManager(); });

  tools_menu->addAction(tr("FIFO Player"), this, &MenuBar::ShowFIFOPlayer);

  auto* usb_device_menu = new QMenu(tr("Emulated USB Devices"), tools_menu);
  usb_device_menu->addAction(tr("&Skylanders Portal"), this, &MenuBar::ShowSkylanderPortal);
  usb_device_menu->addAction(tr("&Infinity Base"), this, &MenuBar::ShowInfinityBase);
  tools_menu->addMenu(usb_device_menu);

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
  m_boot_sysmenu = tools_menu->addAction(QString{}, this, [this] { emit BootWiiSystemMenu(); });
  m_wad_install_action = tools_menu->addAction(tr("Install WAD..."), this, &MenuBar::InstallWAD);
  m_manage_nand_menu = tools_menu->addMenu(tr("Manage NAND"));
  m_import_backup = m_manage_nand_menu->addAction(tr("Import BootMii NAND Backup..."), this,
                                                  [this] { emit ImportNANDBackup(); });
  m_check_nand = m_manage_nand_menu->addAction(tr("Check NAND..."), this, &MenuBar::CheckNAND);
  m_extract_certificates = m_manage_nand_menu->addAction(tr("Extract Certificates from NAND"), this,
                                                         &MenuBar::NANDExtractCertificates);

  m_boot_sysmenu->setEnabled(false);

  connect(&Settings::Instance(), &Settings::NANDRefresh, this, [this] { UpdateToolsMenu(false); });

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

  for (QMenu* menu : {m_state_load_menu, m_state_save_menu, m_state_slot_menu})
    connect(menu, &QMenu::aboutToShow, this, &MenuBar::UpdateStateSlotMenu);
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
    QAction* action = m_state_load_slots_menu->addAction(QString{});

    connect(action, &QAction::triggered, this, [=, this]() { emit StateLoadSlotAt(i); });
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
    QAction* action = m_state_save_slots_menu->addAction(QString{});

    connect(action, &QAction::triggered, this, [=, this]() { emit StateSaveSlotAt(i); });
  }
}

void MenuBar::AddStateSlotMenu(QMenu* emu_menu)
{
  m_state_slot_menu = emu_menu->addMenu(tr("Select State Slot"));
  m_state_slots = new QActionGroup(this);

  for (int i = 1; i <= 10; i++)
  {
    QAction* action = m_state_slot_menu->addAction(QString{});
    action->setCheckable(true);
    action->setActionGroup(m_state_slots);
    if (Settings::Instance().GetStateSlot() == i)
      action->setChecked(true);

    connect(action, &QAction::triggered, this, [=, this]() { emit SetStateSlot(i); });
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

  m_show_threads = view_menu->addAction(tr("&Threads"));
  m_show_threads->setCheckable(true);
  m_show_threads->setChecked(Settings::Instance().IsThreadsVisible());

  connect(m_show_threads, &QAction::toggled, &Settings::Instance(), &Settings::SetThreadsVisible);
  connect(&Settings::Instance(), &Settings::ThreadsVisibilityChanged, m_show_threads,
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

  m_show_network = view_menu->addAction(tr("&Network"));
  m_show_network->setCheckable(true);
  m_show_network->setChecked(Settings::Instance().IsNetworkVisible());

  connect(m_show_network, &QAction::toggled, &Settings::Instance(), &Settings::SetNetworkVisible);
  connect(&Settings::Instance(), &Settings::NetworkVisibilityChanged, m_show_network,
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
  QAction* const purge_action =
      view_menu->addAction(tr("Purge Game List Cache"), this, &MenuBar::PurgeGameListCache);
  purge_action->setEnabled(false);
  connect(&Settings::Instance(), &Settings::GameListRefreshRequested, purge_action,
          [purge_action] { purge_action->setEnabled(false); });
  connect(&Settings::Instance(), &Settings::GameListRefreshStarted, purge_action,
          [purge_action] { purge_action->setEnabled(true); });
  view_menu->addSeparator();
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  view_menu->addAction(tr("Search"), QKeySequence::Find, this, &MenuBar::ShowSearch);
#else
  view_menu->addAction(tr("Search"), this, &MenuBar::ShowSearch, QKeySequence::Find);
#endif
}

void MenuBar::AddOptionsMenu()
{
  QMenu* options_menu = addMenu(tr("&Options"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  options_menu->addAction(tr("Co&nfiguration"), QKeySequence::Preferences, this,
                          &MenuBar::Configure);
#else
  options_menu->addAction(tr("Co&nfiguration"), this, &MenuBar::Configure,
                          QKeySequence::Preferences);
#endif
  options_menu->addSeparator();
  options_menu->addAction(tr("&Graphics Settings"), this, &MenuBar::ConfigureGraphics);
  options_menu->addAction(tr("&Audio Settings"), this, &MenuBar::ConfigureAudio);
  m_controllers_action =
      options_menu->addAction(tr("&Controller Settings"), this, &MenuBar::ConfigureControllers);
  options_menu->addAction(tr("&Hotkey Settings"), this, &MenuBar::ConfigureHotkeys);
  options_menu->addAction(tr("&Free Look Settings"), this, &MenuBar::ConfigureFreelook);

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

  m_reset_ignore_panic_handler = options_menu->addAction(tr("Reset Ignore Panic Handler"));

  connect(m_reset_ignore_panic_handler, &QAction::triggered, this, []() {
    Config::DeleteKey(Config::LayerType::CurrentRun, Config::MAIN_USE_PANIC_HANDLERS);
  });

  m_change_font = options_menu->addAction(tr("&Font..."), this, &MenuBar::ChangeDebugFont);
}

void MenuBar::InstallUpdateManually()
{
  const std::string autoupdate_track = Config::Get(Config::MAIN_AUTOUPDATE_UPDATE_TRACK);
  const std::string manual_track = autoupdate_track.empty() ? "dev" : autoupdate_track;
  auto* const updater = new Updater(this->parentWidget(), manual_track,
                                    Config::Get(Config::MAIN_AUTOUPDATE_HASH_OVERRIDE));

  updater->CheckForUpdate();
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
  QAction* bugtracker = help_menu->addAction(tr("&Bug Tracker"));
  connect(bugtracker, &QAction::triggered, this, []() {
    QDesktopServices::openUrl(
        QUrl(QStringLiteral("https://bugs.dolphin-emu.org/projects/emulator")));
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
  static const QMap<QString, const Config::Info<bool>*> columns{
      {tr("Platform"), &Config::MAIN_GAMELIST_COLUMN_PLATFORM},
      {tr("Banner"), &Config::MAIN_GAMELIST_COLUMN_BANNER},
      {tr("Title"), &Config::MAIN_GAMELIST_COLUMN_TITLE},
      {tr("Description"), &Config::MAIN_GAMELIST_COLUMN_DESCRIPTION},
      {tr("Maker"), &Config::MAIN_GAMELIST_COLUMN_MAKER},
      {tr("File Name"), &Config::MAIN_GAMELIST_COLUMN_FILE_NAME},
      {tr("File Path"), &Config::MAIN_GAMELIST_COLUMN_FILE_PATH},
      {tr("Game ID"), &Config::MAIN_GAMELIST_COLUMN_GAME_ID},
      {tr("Region"), &Config::MAIN_GAMELIST_COLUMN_REGION},
      {tr("File Size"), &Config::MAIN_GAMELIST_COLUMN_FILE_SIZE},
      {tr("File Format"), &Config::MAIN_GAMELIST_COLUMN_FILE_FORMAT},
      {tr("Block Size"), &Config::MAIN_GAMELIST_COLUMN_BLOCK_SIZE},
      {tr("Compression"), &Config::MAIN_GAMELIST_COLUMN_COMPRESSION},
      {tr("Tags"), &Config::MAIN_GAMELIST_COLUMN_TAGS}};

  QActionGroup* column_group = new QActionGroup(this);
  m_cols_menu = view_menu->addMenu(tr("List Columns"));
  column_group->setExclusive(false);

  for (const auto& key : columns.keys())
  {
    const Config::Info<bool>* const config = columns[key];
    QAction* action = column_group->addAction(m_cols_menu->addAction(key));
    action->setCheckable(true);
    action->setChecked(Config::Get(*config));
    connect(action, &QAction::toggled, [this, config, key](bool value) {
      Config::SetBase(*config, value);
      emit ColumnVisibilityToggled(key, value);
    });
  }
}

void MenuBar::AddShowPlatformsMenu(QMenu* view_menu)
{
  static const QMap<QString, const Config::Info<bool>*> platform_map{
      {tr("Show Wii"), &Config::MAIN_GAMELIST_LIST_WII},
      {tr("Show GameCube"), &Config::MAIN_GAMELIST_LIST_GC},
      {tr("Show WAD"), &Config::MAIN_GAMELIST_LIST_WAD},
      {tr("Show ELF/DOL"), &Config::MAIN_GAMELIST_LIST_ELF_DOL}};

  QActionGroup* platform_group = new QActionGroup(this);
  QMenu* plat_menu = view_menu->addMenu(tr("Show Platforms"));
  platform_group->setExclusive(false);

  for (const auto& key : platform_map.keys())
  {
    const Config::Info<bool>* const config = platform_map[key];
    QAction* action = platform_group->addAction(plat_menu->addAction(key));
    action->setCheckable(true);
    action->setChecked(Config::Get(*config));
    connect(action, &QAction::toggled, [this, config, key](bool value) {
      Config::SetBase(*config, value);
      emit GameListPlatformVisibilityToggled(key, value);
    });
  }
}

void MenuBar::AddShowRegionsMenu(QMenu* view_menu)
{
  static const QMap<QString, const Config::Info<bool>*> region_map{
      {tr("Show JPN"), &Config::MAIN_GAMELIST_LIST_JPN},
      {tr("Show PAL"), &Config::MAIN_GAMELIST_LIST_PAL},
      {tr("Show USA"), &Config::MAIN_GAMELIST_LIST_USA},
      {tr("Show Australia"), &Config::MAIN_GAMELIST_LIST_AUSTRALIA},
      {tr("Show France"), &Config::MAIN_GAMELIST_LIST_FRANCE},
      {tr("Show Germany"), &Config::MAIN_GAMELIST_LIST_GERMANY},
      {tr("Show Italy"), &Config::MAIN_GAMELIST_LIST_ITALY},
      {tr("Show Korea"), &Config::MAIN_GAMELIST_LIST_KOREA},
      {tr("Show Netherlands"), &Config::MAIN_GAMELIST_LIST_NETHERLANDS},
      {tr("Show Russia"), &Config::MAIN_GAMELIST_LIST_RUSSIA},
      {tr("Show Spain"), &Config::MAIN_GAMELIST_LIST_SPAIN},
      {tr("Show Taiwan"), &Config::MAIN_GAMELIST_LIST_TAIWAN},
      {tr("Show World"), &Config::MAIN_GAMELIST_LIST_WORLD},
      {tr("Show Unknown"), &Config::MAIN_GAMELIST_LIST_UNKNOWN}};

  QMenu* const region_menu = view_menu->addMenu(tr("Show Regions"));
  const QAction* const show_all_regions = region_menu->addAction(tr("Show All"));
  const QAction* const hide_all_regions = region_menu->addAction(tr("Hide All"));
  region_menu->addSeparator();

  for (const auto& key : region_map.keys())
  {
    const Config::Info<bool>* const config = region_map[key];
    QAction* const menu_item = region_menu->addAction(key);
    menu_item->setCheckable(true);
    menu_item->setChecked(Config::Get(*config));

    const auto set_visibility = [this, config, key, menu_item](bool visibility) {
      menu_item->setChecked(visibility);
      Config::SetBase(*config, visibility);
      emit GameListRegionVisibilityToggled(key, visibility);
    };
    const auto set_visible = std::bind(set_visibility, true);
    const auto set_hidden = std::bind(set_visibility, false);

    connect(menu_item, &QAction::toggled, set_visibility);
    connect(show_all_regions, &QAction::triggered, menu_item, set_visible);
    connect(hide_all_regions, &QAction::triggered, menu_item, set_hidden);
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
  pause_at_end->setChecked(Config::Get(Config::MAIN_MOVIE_PAUSE_MOVIE));
  connect(pause_at_end, &QAction::toggled,
          [](bool value) { Config::SetBaseOrCurrent(Config::MAIN_MOVIE_PAUSE_MOVIE, value); });

  auto* rerecord_counter = movie_menu->addAction(tr("Show Rerecord Counter"));
  rerecord_counter->setCheckable(true);
  rerecord_counter->setChecked(Config::Get(Config::MAIN_MOVIE_SHOW_RERECORD));
  connect(rerecord_counter, &QAction::toggled,
          [](bool value) { Config::SetBaseOrCurrent(Config::MAIN_MOVIE_SHOW_RERECORD, value); });

  auto* lag_counter = movie_menu->addAction(tr("Show Lag Counter"));
  lag_counter->setCheckable(true);
  lag_counter->setChecked(Config::Get(Config::MAIN_SHOW_LAG));
  connect(lag_counter, &QAction::toggled,
          [](bool value) { Config::SetBaseOrCurrent(Config::MAIN_SHOW_LAG, value); });

  auto* frame_counter = movie_menu->addAction(tr("Show Frame Counter"));
  frame_counter->setCheckable(true);
  frame_counter->setChecked(Config::Get(Config::MAIN_SHOW_FRAME_COUNT));
  connect(frame_counter, &QAction::toggled,
          [](bool value) { Config::SetBaseOrCurrent(Config::MAIN_SHOW_FRAME_COUNT, value); });

  auto* input_display = movie_menu->addAction(tr("Show Input Display"));
  input_display->setCheckable(true);
  input_display->setChecked(Config::Get(Config::MAIN_MOVIE_SHOW_INPUT_DISPLAY));
  connect(input_display, &QAction::toggled, [](bool value) {
    Config::SetBaseOrCurrent(Config::MAIN_MOVIE_SHOW_INPUT_DISPLAY, value);
  });

  auto* system_clock = movie_menu->addAction(tr("Show System Clock"));
  system_clock->setCheckable(true);
  system_clock->setChecked(Config::Get(Config::MAIN_MOVIE_SHOW_RTC));
  connect(system_clock, &QAction::toggled,
          [](bool value) { Config::SetBaseOrCurrent(Config::MAIN_MOVIE_SHOW_RTC, value); });

  movie_menu->addSeparator();

  auto* dump_frames = movie_menu->addAction(tr("Dump Frames"));
  dump_frames->setCheckable(true);
  dump_frames->setChecked(Config::Get(Config::MAIN_MOVIE_DUMP_FRAMES));
  connect(dump_frames, &QAction::toggled,
          [](bool value) { Config::SetBaseOrCurrent(Config::MAIN_MOVIE_DUMP_FRAMES, value); });

  auto* dump_audio = movie_menu->addAction(tr("Dump Audio"));
  dump_audio->setCheckable(true);
  dump_audio->setChecked(Config::Get(Config::MAIN_DUMP_AUDIO));
  connect(dump_audio, &QAction::toggled,
          [](bool value) { Config::SetBaseOrCurrent(Config::MAIN_DUMP_AUDIO, value); });
}

void MenuBar::AddJITMenu()
{
  m_jit = addMenu(tr("JIT"));

  m_jit_interpreter_core = m_jit->addAction(tr("Interpreter Core"));
  m_jit_interpreter_core->setCheckable(true);
  m_jit_interpreter_core->setChecked(Config::Get(Config::MAIN_CPU_CORE) ==
                                     PowerPC::CPUCore::Interpreter);

  connect(m_jit_interpreter_core, &QAction::toggled, [](bool enabled) {
    Core::System::GetInstance().GetPowerPC().SetMode(enabled ? PowerPC::CoreMode::Interpreter :
                                                               PowerPC::CoreMode::JIT);
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

  m_jit_disable_fastmem = m_jit->addAction(tr("Disable Fastmem"));
  m_jit_disable_fastmem->setCheckable(true);
  m_jit_disable_fastmem->setChecked(!Config::Get(Config::MAIN_FASTMEM));
  connect(m_jit_disable_fastmem, &QAction::toggled, [this](bool enabled) {
    Config::SetBaseOrCurrent(Config::MAIN_FASTMEM, !enabled);
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
  m_jit_off->setChecked(Config::Get(Config::MAIN_DEBUG_JIT_OFF));
  connect(m_jit_off, &QAction::toggled, [this](bool enabled) {
    Config::SetBaseOrCurrent(Config::MAIN_DEBUG_JIT_OFF, enabled);
    ClearCache();
  });

  m_jit_loadstore_off = m_jit->addAction(tr("JIT LoadStore Off"));
  m_jit_loadstore_off->setCheckable(true);
  m_jit_loadstore_off->setChecked(Config::Get(Config::MAIN_DEBUG_JIT_LOAD_STORE_OFF));
  connect(m_jit_loadstore_off, &QAction::toggled, [this](bool enabled) {
    Config::SetBaseOrCurrent(Config::MAIN_DEBUG_JIT_LOAD_STORE_OFF, enabled);
    ClearCache();
  });

  m_jit_loadstore_lbzx_off = m_jit->addAction(tr("JIT LoadStore lbzx Off"));
  m_jit_loadstore_lbzx_off->setCheckable(true);
  m_jit_loadstore_lbzx_off->setChecked(Config::Get(Config::MAIN_DEBUG_JIT_LOAD_STORE_LBZX_OFF));
  connect(m_jit_loadstore_lbzx_off, &QAction::toggled, [this](bool enabled) {
    Config::SetBaseOrCurrent(Config::MAIN_DEBUG_JIT_LOAD_STORE_LBZX_OFF, enabled);
    ClearCache();
  });

  m_jit_loadstore_lxz_off = m_jit->addAction(tr("JIT LoadStore lXz Off"));
  m_jit_loadstore_lxz_off->setCheckable(true);
  m_jit_loadstore_lxz_off->setChecked(Config::Get(Config::MAIN_DEBUG_JIT_LOAD_STORE_LXZ_OFF));
  connect(m_jit_loadstore_lxz_off, &QAction::toggled, [this](bool enabled) {
    Config::SetBaseOrCurrent(Config::MAIN_DEBUG_JIT_LOAD_STORE_LXZ_OFF, enabled);
    ClearCache();
  });

  m_jit_loadstore_lwz_off = m_jit->addAction(tr("JIT LoadStore lwz Off"));
  m_jit_loadstore_lwz_off->setCheckable(true);
  m_jit_loadstore_lwz_off->setChecked(Config::Get(Config::MAIN_DEBUG_JIT_LOAD_STORE_LWZ_OFF));
  connect(m_jit_loadstore_lwz_off, &QAction::toggled, [this](bool enabled) {
    Config::SetBaseOrCurrent(Config::MAIN_DEBUG_JIT_LOAD_STORE_LWZ_OFF, enabled);
    ClearCache();
  });

  m_jit_loadstore_floating_off = m_jit->addAction(tr("JIT LoadStore Floating Off"));
  m_jit_loadstore_floating_off->setCheckable(true);
  m_jit_loadstore_floating_off->setChecked(
      Config::Get(Config::MAIN_DEBUG_JIT_LOAD_STORE_FLOATING_OFF));
  connect(m_jit_loadstore_floating_off, &QAction::toggled, [this](bool enabled) {
    Config::SetBaseOrCurrent(Config::MAIN_DEBUG_JIT_LOAD_STORE_FLOATING_OFF, enabled);
    ClearCache();
  });

  m_jit_loadstore_paired_off = m_jit->addAction(tr("JIT LoadStore Paired Off"));
  m_jit_loadstore_paired_off->setCheckable(true);
  m_jit_loadstore_paired_off->setChecked(Config::Get(Config::MAIN_DEBUG_JIT_LOAD_STORE_PAIRED_OFF));
  connect(m_jit_loadstore_paired_off, &QAction::toggled, [this](bool enabled) {
    Config::SetBaseOrCurrent(Config::MAIN_DEBUG_JIT_LOAD_STORE_PAIRED_OFF, enabled);
    ClearCache();
  });

  m_jit_floatingpoint_off = m_jit->addAction(tr("JIT FloatingPoint Off"));
  m_jit_floatingpoint_off->setCheckable(true);
  m_jit_floatingpoint_off->setChecked(Config::Get(Config::MAIN_DEBUG_JIT_FLOATING_POINT_OFF));
  connect(m_jit_floatingpoint_off, &QAction::toggled, [this](bool enabled) {
    Config::SetBaseOrCurrent(Config::MAIN_DEBUG_JIT_FLOATING_POINT_OFF, enabled);
    ClearCache();
  });

  m_jit_integer_off = m_jit->addAction(tr("JIT Integer Off"));
  m_jit_integer_off->setCheckable(true);
  m_jit_integer_off->setChecked(Config::Get(Config::MAIN_DEBUG_JIT_INTEGER_OFF));
  connect(m_jit_integer_off, &QAction::toggled, [this](bool enabled) {
    Config::SetBaseOrCurrent(Config::MAIN_DEBUG_JIT_INTEGER_OFF, enabled);
    ClearCache();
  });

  m_jit_paired_off = m_jit->addAction(tr("JIT Paired Off"));
  m_jit_paired_off->setCheckable(true);
  m_jit_paired_off->setChecked(Config::Get(Config::MAIN_DEBUG_JIT_PAIRED_OFF));
  connect(m_jit_paired_off, &QAction::toggled, [this](bool enabled) {
    Config::SetBaseOrCurrent(Config::MAIN_DEBUG_JIT_PAIRED_OFF, enabled);
    ClearCache();
  });

  m_jit_systemregisters_off = m_jit->addAction(tr("JIT SystemRegisters Off"));
  m_jit_systemregisters_off->setCheckable(true);
  m_jit_systemregisters_off->setChecked(Config::Get(Config::MAIN_DEBUG_JIT_SYSTEM_REGISTERS_OFF));
  connect(m_jit_systemregisters_off, &QAction::toggled, [this](bool enabled) {
    Config::SetBaseOrCurrent(Config::MAIN_DEBUG_JIT_SYSTEM_REGISTERS_OFF, enabled);
    ClearCache();
  });

  m_jit_branch_off = m_jit->addAction(tr("JIT Branch Off"));
  m_jit_branch_off->setCheckable(true);
  m_jit_branch_off->setChecked(Config::Get(Config::MAIN_DEBUG_JIT_BRANCH_OFF));
  connect(m_jit_branch_off, &QAction::toggled, [this](bool enabled) {
    Config::SetBaseOrCurrent(Config::MAIN_DEBUG_JIT_BRANCH_OFF, enabled);
    ClearCache();
  });

  m_jit_register_cache_off = m_jit->addAction(tr("JIT Register Cache Off"));
  m_jit_register_cache_off->setCheckable(true);
  m_jit_register_cache_off->setChecked(Config::Get(Config::MAIN_DEBUG_JIT_REGISTER_CACHE_OFF));
  connect(m_jit_register_cache_off, &QAction::toggled, [this](bool enabled) {
    Config::SetBaseOrCurrent(Config::MAIN_DEBUG_JIT_REGISTER_CACHE_OFF, enabled);
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
  m_ntscj_ipl->setEnabled(!emulation_started && File::Exists(Config::GetBootROMPath(JAP_DIR)));
  m_ntscu_ipl->setEnabled(!emulation_started && File::Exists(Config::GetBootROMPath(USA_DIR)));
  m_pal_ipl->setEnabled(!emulation_started && File::Exists(Config::GetBootROMPath(EUR_DIR)));
  m_import_backup->setEnabled(!emulation_started);
  m_check_nand->setEnabled(!emulation_started);

  if (!emulation_started)
  {
    IOS::HLE::Kernel ios;
    const auto tmd = ios.GetESCore().FindInstalledTMD(Titles::SYSTEM_MENU);

    const QString sysmenu_version =
        tmd.IsValid() ? QString::fromStdString(
                            DiscIO::GetSysMenuVersionString(tmd.GetTitleVersion(), tmd.IsvWii())) :
                        QString{};

    const QString sysmenu_text = (tmd.IsValid() && tmd.IsvWii()) ? tr("Load vWii System Menu %1") :
                                                                   tr("Load Wii System Menu %1");

    m_boot_sysmenu->setText(sysmenu_text.arg(sysmenu_version));

    m_boot_sysmenu->setEnabled(tmd.IsValid());

    for (QAction* action : m_perform_online_update_menu->actions())
      action->setEnabled(!tmd.IsValid());
    m_perform_online_update_for_current_region->setEnabled(tmd.IsValid());
  }

  const auto bt = WiiUtils::GetBluetoothEmuDevice();
  const bool enable_wiimotes = emulation_started && bt != nullptr;

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
  QString wad_file = DolphinFileDialog::getOpenFileName(
      this, tr("Select a title to install to NAND"), QString(), tr("WAD files (*.wad)"));

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
  QString file =
      DolphinFileDialog::getOpenFileName(this, tr("Select the save file"), QDir::currentPath(),
                                         tr("Wii save files (*.bin);;"
                                            "All Files (*)"));

  if (file.isEmpty())
    return;

  auto can_overwrite = [&] {
    return ModalMessageBox::question(
               this, tr("Save Import"),
               tr("Save data for this title already exists in the NAND. Consider backing up "
                  "the current data before overwriting.\nOverwrite now?")) == QMessageBox::Yes;
  };

  const auto result = WiiSave::Import(file.toStdString(), can_overwrite);
  switch (result)
  {
  case WiiSave::CopyResult::Success:
    ModalMessageBox::information(this, tr("Save Import"), tr("Successfully imported save file."));
    break;
  case WiiSave::CopyResult::CorruptedSource:
    ModalMessageBox::critical(this, tr("Save Import"),
                              tr("Failed to import save file. The given file appears to be "
                                 "corrupted or is not a valid Wii save."));
    break;
  case WiiSave::CopyResult::TitleMissing:
    ModalMessageBox::critical(
        this, tr("Save Import"),
        tr("Failed to import save file. Please launch the game once, then try again."));
    break;
  case WiiSave::CopyResult::Cancelled:
    break;
  default:
    ModalMessageBox::critical(
        this, tr("Save Import"),
        tr("Failed to import save file. Your NAND may be corrupt, or something is preventing "
           "access to files within it. Try repairing your NAND (Tools -> Manage NAND -> Check "
           "NAND...), then import the save again."));
    break;
  }
}

void MenuBar::ExportWiiSaves()
{
  const QString export_dir = DolphinFileDialog::getExistingDirectory(
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

  if (NANDRepairDialog(result, this).exec() != QDialog::Accepted)
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
  if (DiscIO::NANDImporter().ExtractCertificates())
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
  m_game_selected = !!game_file;

  m_recording_play->setEnabled(m_game_selected && !Core::IsRunning());
  m_recording_start->setEnabled((m_game_selected || Core::IsRunning()) && !Movie::IsPlayingInput());
}

void MenuBar::OnRecordingStatusChanged(bool recording)
{
  m_recording_start->setEnabled(!recording && (m_game_selected || Core::IsRunning()));
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
  Core::CPUThreadGuard guard(Core::System::GetInstance());

  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  PPCAnalyst::FindFunctions(guard, Memory::MEM1_BASE_ADDR,
                            Memory::MEM1_BASE_ADDR + memory.GetRamSizeReal(), &g_symbolDB);
  emit NotifySymbolsUpdated();
}

void MenuBar::GenerateSymbolsFromSignatureDB()
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());

  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  PPCAnalyst::FindFunctions(guard, Memory::MEM1_BASE_ADDR,
                            Memory::MEM1_BASE_ADDR + memory.GetRamSizeReal(), &g_symbolDB);
  SignatureDB db(SignatureDB::HandlerType::DSY);
  if (db.Load(File::GetSysDirectory() + TOTALDB))
  {
    db.Apply(guard, &g_symbolDB);
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
  // i18n: RSO refers to a proprietary format for shared objects (like DLL files).
  const int ret =
      ModalMessageBox::question(this, tr("RSO auto-detection"), tr("Auto-detect RSO modules?"));
  if (ret == QMessageBox::Yes)
    return GenerateSymbolsFromRSOAuto();

  const QString text =
      QInputDialog::getText(this, tr("Input"), tr("Enter the RSO module address:"),
                            QLineEdit::Normal, QString{}, nullptr, Qt::WindowCloseButtonHint);
  bool good;
  const uint address = text.toUInt(&good, 16);

  if (!good)
  {
    ModalMessageBox::warning(this, tr("Error"), tr("Invalid RSO module address: %1").arg(text));
    return;
  }

  Core::CPUThreadGuard guard(Core::System::GetInstance());

  RSOChainView rso_chain;
  if (rso_chain.Load(guard, static_cast<u32>(address)))
  {
    rso_chain.Apply(guard, &g_symbolDB);
    emit NotifySymbolsUpdated();
  }
  else
  {
    ModalMessageBox::warning(this, tr("Error"), tr("Failed to load RSO module at %1").arg(text));
  }
}

void MenuBar::GenerateSymbolsFromRSOAuto()
{
  ParallelProgressDialog progress(tr("Modules found: %1").arg(0), tr("Cancel"), 0, 0, this);
  progress.GetRaw()->setWindowTitle(tr("Detecting RSO Modules"));
  progress.GetRaw()->setMinimumDuration(1000 * 10);
  progress.GetRaw()->setWindowModality(Qt::WindowModal);

  auto future = std::async(std::launch::async, [&progress, this]() -> RSOVector {
    progress.SetValue(0);
    auto matches = DetectRSOModules(progress);
    progress.Reset();

    return matches;
  });
  progress.GetRaw()->exec();

  const auto matches = future.get();

  QStringList items;
  for (const auto& match : matches)
  {
    const QString item = QLatin1String("%1 %2");
    items << item.arg(QString::number(match.first, 16), QString::fromStdString(match.second));
  }

  if (items.empty())
  {
    ModalMessageBox::warning(this, tr("Error"), tr("Unable to auto-detect RSO module"));
    return;
  }

  bool ok;
  const QString item =
      QInputDialog::getItem(this, tr("Input"), tr("Select the RSO module address:"), items, 0,
                            false, &ok, Qt::WindowCloseButtonHint);

  if (!ok)
    return;

  RSOChainView rso_chain;
  const u32 address = item.mid(0, item.indexOf(QLatin1Char(' '))).toUInt(nullptr, 16);

  Core::CPUThreadGuard guard(Core::System::GetInstance());

  if (rso_chain.Load(guard, address))
  {
    rso_chain.Apply(guard, &g_symbolDB);
    emit NotifySymbolsUpdated();
  }
  else
  {
    ModalMessageBox::warning(this, tr("Error"), tr("Failed to load RSO module at %1").arg(address));
  }
}

RSOVector MenuBar::DetectRSOModules(ParallelProgressDialog& progress)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());

  constexpr std::array<std::string_view, 2> search_for = {".elf", ".plf"};

  const AddressSpace::Accessors* accessors =
      AddressSpace::GetAccessors(AddressSpace::Type::Effective);

  RSOVector matches;

  // Find filepath to elf/plf commonly used by RSO modules
  for (const auto& str : search_for)
  {
    u32 next = 0;
    while (true)
    {
      if (progress.WasCanceled())
      {
        return matches;
      }

      auto found_addr = accessors->Search(guard, next, reinterpret_cast<const u8*>(str.data()),
                                          str.size() + 1, true);

      if (!found_addr.has_value())
        break;

      next = *found_addr + 1;

      // Non-null data can precede the module name.
      // Get the maximum name length that a module could have.
      auto get_max_module_name_len = [&guard, found_addr] {
        constexpr u32 MODULE_NAME_MAX_LENGTH = 260;
        u32 len = 0;

        for (; len < MODULE_NAME_MAX_LENGTH; ++len)
        {
          const auto res = PowerPC::MMU::HostRead_U8(guard, *found_addr - (len + 1));
          if (!std::isprint(res))
          {
            break;
          }
        }

        return len;
      };

      if (progress.WasCanceled())
      {
        return matches;
      }

      const auto max_name_length = get_max_module_name_len();
      auto found = false;
      u32 module_name_length = 0;

      // Look for the Module Name Offset Field based on each possible length
      for (u32 i = 0; i < max_name_length; ++i)
      {
        if (progress.WasCanceled())
        {
          return matches;
        }

        const auto lookup_addr = (*found_addr - max_name_length) + i;

        const std::array<u8, 4> ref = {
            static_cast<u8>(lookup_addr >> 24), static_cast<u8>(lookup_addr >> 16),
            static_cast<u8>(lookup_addr >> 8), static_cast<u8>(lookup_addr)};

        // Get the field (Module Name Offset) that point to the string
        const auto module_name_offset_addr =
            accessors->Search(guard, lookup_addr, ref.data(), ref.size(), false);
        if (!module_name_offset_addr.has_value())
          continue;

        // The next 4 bytes should be the module name length
        module_name_length = accessors->ReadU32(guard, *module_name_offset_addr + 4);
        if (module_name_length == max_name_length - i + str.length())
        {
          found_addr = module_name_offset_addr;
          found = true;
          break;
        }
      }

      if (!found)
        continue;

      const auto module_name_offset = accessors->ReadU32(guard, *found_addr);

      // Go to the beginning of the RSO header
      matches.emplace_back(*found_addr - 16, PowerPC::MMU::HostGetString(guard, module_name_offset,
                                                                         module_name_length));

      progress.SetLabelText(tr("Modules found: %1").arg(matches.size()));
    }
  }

  return matches;
}

void MenuBar::LoadSymbolMap()
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  std::string existing_map_file, writable_map_file;
  bool map_exists = CBoot::FindMapFile(&existing_map_file, &writable_map_file);

  if (!map_exists)
  {
    g_symbolDB.Clear();

    {
      Core::CPUThreadGuard guard(Core::System::GetInstance());

      PPCAnalyst::FindFunctions(guard, Memory::MEM1_BASE_ADDR + 0x1300000,
                                Memory::MEM1_BASE_ADDR + memory.GetRamSizeReal(), &g_symbolDB);
      SignatureDB db(SignatureDB::HandlerType::DSY);
      if (db.Load(File::GetSysDirectory() + TOTALDB))
        db.Apply(guard, &g_symbolDB);
    }

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

  HLE::PatchFunctions(system);
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
  const QString file = DolphinFileDialog::getOpenFileName(
      this, tr("Load map file"), QString::fromStdString(File::GetUserPath(D_MAPS_IDX)),
      tr("Dolphin Map File (*.map)"));

  if (file.isEmpty())
    return;

  if (!TryLoadMapFile(file))
    return;

  auto& system = Core::System::GetInstance();
  HLE::PatchFunctions(system);
  emit NotifySymbolsUpdated();
}

void MenuBar::LoadBadSymbolMap()
{
  const QString file = DolphinFileDialog::getOpenFileName(
      this, tr("Load map file"), QString::fromStdString(File::GetUserPath(D_MAPS_IDX)),
      tr("Dolphin Map File (*.map)"));

  if (file.isEmpty())
    return;

  if (!TryLoadMapFile(file, true))
    return;

  auto& system = Core::System::GetInstance();
  HLE::PatchFunctions(system);
  emit NotifySymbolsUpdated();
}

void MenuBar::SaveSymbolMapAs()
{
  const std::string& title_id_str = SConfig::GetInstance().m_debugger_game_id;
  const QString file = DolphinFileDialog::getSaveFileName(
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

  bool success;
  {
    Core::CPUThreadGuard guard(Core::System::GetInstance());
    success = g_symbolDB.SaveCodeMap(guard, path);
  }

  if (!success)
  {
    ModalMessageBox::warning(
        this, tr("Error"),
        tr("Failed to save code map to path '%1'").arg(QString::fromStdString(path)));
  }
}

bool MenuBar::TryLoadMapFile(const QString& path, const bool bad)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());

  if (!g_symbolDB.LoadMap(guard, path.toStdString(), bad))
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
      this, tr("Input"), tr("Only export symbols with prefix:\n(Blank for all symbols)"),
      QLineEdit::Normal, QString{}, nullptr, Qt::WindowCloseButtonHint);

  const QString file = DolphinFileDialog::getSaveFileName(this, tr("Save signature file"),
                                                          QDir::homePath(), GetSignatureSelector());
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
      this, tr("Input"), tr("Only append symbols with prefix:\n(Blank for all symbols)"),
      QLineEdit::Normal, QString{}, nullptr, Qt::WindowCloseButtonHint);

  const QString file = DolphinFileDialog::getSaveFileName(this, tr("Append signature to"),
                                                          QDir::homePath(), GetSignatureSelector());
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
  const QString file = DolphinFileDialog::getOpenFileName(this, tr("Apply signature file"),
                                                          QDir::homePath(), GetSignatureSelector());

  if (file.isEmpty())
    return;

  const std::string load_path = file.toStdString();
  SignatureDB db(load_path);
  db.Load(load_path);
  {
    Core::CPUThreadGuard guard(Core::System::GetInstance());
    db.Apply(guard, &g_symbolDB);
  }
  db.List();
  auto& system = Core::System::GetInstance();
  HLE::PatchFunctions(system);
  emit NotifySymbolsUpdated();
}

void MenuBar::CombineSignatureFiles()
{
  const QString priorityFile = DolphinFileDialog::getOpenFileName(
      this, tr("Choose priority input file"), QDir::homePath(), GetSignatureSelector());
  if (priorityFile.isEmpty())
    return;

  const QString secondaryFile = DolphinFileDialog::getOpenFileName(
      this, tr("Choose secondary input file"), QDir::homePath(), GetSignatureSelector());
  if (secondaryFile.isEmpty())
    return;

  const QString saveFile = DolphinFileDialog::getSaveFileName(
      this, tr("Save combined output file as"), QDir::homePath(), GetSignatureSelector());
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
  auto& system = Core::System::GetInstance();
  HLE::PatchFunctions(system);
}

void MenuBar::ClearCache()
{
  Core::RunAsCPUThread([] { Core::System::GetInstance().GetJitInterface().ClearCache(); });
}

void MenuBar::LogInstructions()
{
  PPCTables::LogCompiledInstructions();
}

void MenuBar::SearchInstruction()
{
  bool good;
  const QString op =
      QInputDialog::getText(this, tr("Search instruction"), tr("Instruction:"), QLineEdit::Normal,
                            QString{}, &good, Qt::WindowCloseButtonHint);

  if (!good)
    return;

  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  Core::CPUThreadGuard guard(system);

  bool found = false;
  for (u32 addr = Memory::MEM1_BASE_ADDR; addr < Memory::MEM1_BASE_ADDR + memory.GetRamSizeReal();
       addr += 4)
  {
    const auto ins_name = QString::fromStdString(
        PPCTables::GetInstructionName(PowerPC::MMU::HostRead_U32(guard, addr), addr));
    if (op == ins_name)
    {
      NOTICE_LOG_FMT(POWERPC, "Found {} at {:08x}", op.toStdString(), addr);
      found = true;
    }
  }
  if (!found)
    NOTICE_LOG_FMT(POWERPC, "Opcode {} not found", op.toStdString());
}
