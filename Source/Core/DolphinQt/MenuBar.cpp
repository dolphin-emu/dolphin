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

QString MenuBar::GetSignatureSelector() const
{
  return QStringLiteral("%1 (*.dsy);; %2 (*.csv);; %3 (*.mega)")
      .arg(tr("Dolphin Signature File"), tr("Dolphin Signature CSV File"),
           tr("WiiTools Signature MEGA File"));
}

MenuBar::MenuBar(QWidget* parent) : QMenuBar(parent)
{
  s_menu_bar = this;

  // Main Bar Menus
  AddFileMainMenu();
  AddViewMainMenu();
  AddEmulationMainMenu();
  AddToolsMainMenu();
  AddOptionsMainMenu();
  AddDebugJITMainMenu();
  AddDebugSymbolsMainMenu();
  AddHelpMainMenu();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          [=](Core::State state) { OnEmulationStateChanged(state); });
  connect(Host::GetInstance(), &Host::UpdateDisasmDialog, this,
          [this] { OnEmulationStateChanged(Core::GetState()); });

  OnEmulationStateChanged(Core::GetState());
  connect(&Settings::Instance(), &Settings::DebugModeToggled, this, &MenuBar::OnDebugModeToggled);

  connect(this, &MenuBar::GameSelectionChanged, this, &MenuBar::OnGameSelectionChanged);

  connect(this, &MenuBar::INRECSStatusChanged, this, &MenuBar::OnINRECSStatusChanged);
  connect(this, &MenuBar::INRECSReadOnlyModeChanged, this, &MenuBar::OnINRECSReadOnlyModeChanged);
}

void MenuBar::OnEmulationStateChanged(Core::State state)
{
  bool running = state != Core::State::Uninitialized;
  bool playing = running && state != Core::State::Paused;

  // File Menu
  m_eject_disc->setEnabled(running);
  m_change_disc->setEnabled(running);

  // Emulation Menu
  m_emu_play_action->setEnabled(!playing);
  m_emu_play_action->setVisible(!playing);
  m_emu_pause_action->setEnabled(playing);
  m_emu_pause_action->setVisible(playing);
  m_emu_stop_action->setEnabled(running);
  m_emu_stop_action->setVisible(running);
  m_emu_reset_action->setEnabled(running);
  m_emu_fullscreen_action->setEnabled(running);
  m_emu_frame_advance_action->setEnabled(running);
  m_emu_screenshot_action->setEnabled(running);
  m_state_load_submenu->setEnabled(running);
  m_state_save_submenu->setEnabled(running);

  UpdateStateSlotSubMenuAction();

  // Tools Menu
  m_show_cheat_manager->setEnabled(Settings::Instance().GetCheatsEnabled() && running);

  UpdateMenu_Tools(running);

  // Input Recording System Sub Menu
  m_inrecs_read_only->setEnabled(running);
  if (!running)
  {
    m_inrecs_stop->setEnabled(false);
    m_inrecs_export->setEnabled(false);
  }
  m_inrecs_replay->setEnabled(m_game_selected && !running);
  m_inrecs_start->setEnabled((m_game_selected || running) && !Movie::IsPlayingInput());

  // Options Menu
  m_controllers_action->setEnabled(NetPlay::IsNetPlayRunning() ? !running : true);

  // JIT Menu
  m_debug__jit_interpreter_core->setEnabled(running);
  m_debug__jit_block_linking->setEnabled(!running);
  m_debug__jit_disable_cache->setEnabled(!running);
  m_debug__jit_disable_fastmem->setEnabled(!running);
  m_debug__jit_clear_cache->setEnabled(running);
  m_debug__jit_log_coverage->setEnabled(!running);
  m_debug__jit_search_instruction->setEnabled(running);

  for (QAction* action :
       {m_debug__jit_off, m_debug__jit_loadstore_off, m_debug__jit_loadstore_lbzx_off,
        m_debug__jit_loadstore_lxz_off, m_debug__jit_loadstore_lwz_off,
        m_debug__jit_loadstore_floating_off, m_debug__jit_loadstore_paired_off,
        m_debug__jit_floatingpoint_off, m_debug__jit_integer_off, m_debug__jit_paired_off,
        m_debug__jit_systemregisters_off, m_debug__jit_branch_off, m_debug__jit_register_cache_off})
  {
    action->setEnabled(running && !playing);
  }

  // Symbols Menu
  m_debug__symbols_mainmenu->setEnabled(running);

  // Other
  OnDebugModeToggled(Settings::Instance().IsDebugModeEnabled());
}

void MenuBar::UpdateMenu_Tools(bool emulation_started)
{
  m_wii_load_sysmenu_action->setEnabled(!emulation_started);
  m_perform_online_update_submenu->setEnabled(!emulation_started);
  m_gc_load_ipl_ntscj->setEnabled(!emulation_started &&
                                  File::Exists(SConfig::GetInstance().GetBootROMPath(JAP_DIR)));
  m_gc_load_ipl_ntscu->setEnabled(!emulation_started &&
                                  File::Exists(SConfig::GetInstance().GetBootROMPath(USA_DIR)));
  m_gc_load_ipl_pal->setEnabled(!emulation_started &&
                                File::Exists(SConfig::GetInstance().GetBootROMPath(EUR_DIR)));
  m_nand_import_merge_secondary->setEnabled(!emulation_started);
  m_nand_check->setEnabled(!emulation_started);

  if (!emulation_started)
  {
    IOS::HLE::Kernel ios;
    const auto tmd = ios.GetES()->FindInstalledTMD(Titles::SYSTEM_MENU);

    const QString sysmenu_version =
        tmd.IsValid() ?
            QString::fromStdString(DiscIO::GetSysMenuVersionString(tmd.GetTitleVersion())) :
            QString{};
    m_wii_load_sysmenu_action->setText(tr("Load Wii System Menu %1").arg(sysmenu_version));

    m_wii_load_sysmenu_action->setEnabled(tmd.IsValid());

    for (QAction* action : m_perform_online_update_submenu->actions())
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

void MenuBar::OnDebugModeToggled(bool enabled)
{
  // Options
  m_debug__boot_to_pause->setVisible(enabled);
  m_debug__automatic_start->setVisible(enabled);
  m_debug__change_font->setVisible(enabled);

  // View
  m_debug__show_code->setVisible(enabled);
  m_debug__show_registers->setVisible(enabled);
  m_debug__show_threads->setVisible(enabled);
  m_debug__show_watch->setVisible(enabled);
  m_debug__show_breakpoints->setVisible(enabled);
  m_debug__show_memory->setVisible(enabled);
  m_debug__show_network->setVisible(enabled);
  m_debug__show_jit->setVisible(enabled);

  if (enabled)
  {
    addMenu(m_debug__jit_mainmenu);
    addMenu(m_debug__symbols_mainmenu);
  }
  else
  {
    removeAction(m_debug__jit_mainmenu->menuAction());
    removeAction(m_debug__symbols_mainmenu->menuAction());
  }
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

void MenuBar::AddFileMainMenu()
{
  QMenu* const mainmenu_file = addMenu(tr("&File"));
  m_open_action =
      mainmenu_file->addAction(tr("&Open..."), this, &MenuBar::Open, QKeySequence::Open);

  mainmenu_file->addSeparator();

  m_change_disc = mainmenu_file->addAction(tr("Change &Disc..."), this, &MenuBar::ChangeDisc);
  m_eject_disc = mainmenu_file->addAction(tr("&Eject Disc"), this, &MenuBar::EjectDisc);

  AddBootDVDBackupSubMenu(mainmenu_file);

  mainmenu_file->addSeparator();

  m_exit_action = mainmenu_file->addAction(tr("E&xit"), this, &MenuBar::Exit);
  m_exit_action->setShortcuts({QKeySequence::Quit, QKeySequence(Qt::ALT + Qt::Key_F4)});
}

void MenuBar::AddViewMainMenu()
{
  QMenu* const mainmenu_view = addMenu(tr("&View"));

  QAction* const show_log = mainmenu_view->addAction(tr("Show &Log"));
  show_log->setCheckable(true);
  show_log->setChecked(Settings::Instance().IsLogVisible());

  connect(show_log, &QAction::toggled, &Settings::Instance(), &Settings::SetLogVisible);

  QAction* const show_log_config = mainmenu_view->addAction(tr("Show Log &Configuration"));
  show_log_config->setCheckable(true);
  show_log_config->setChecked(Settings::Instance().IsLogConfigVisible());

  connect(show_log_config, &QAction::toggled, &Settings::Instance(),
          &Settings::SetLogConfigVisible);

  QAction* const show_toolbar = mainmenu_view->addAction(tr("Show &Toolbar"));
  show_toolbar->setCheckable(true);
  show_toolbar->setChecked(Settings::Instance().IsToolBarVisible());

  connect(show_toolbar, &QAction::toggled, &Settings::Instance(), &Settings::SetToolBarVisible);

  connect(&Settings::Instance(), &Settings::LogVisibilityChanged, show_log, &QAction::setChecked);
  connect(&Settings::Instance(), &Settings::LogConfigVisibilityChanged, show_log_config,
          &QAction::setChecked);
  connect(&Settings::Instance(), &Settings::ToolBarVisibilityChanged, show_toolbar,
          &QAction::setChecked);

  QAction* const lock_widgets = mainmenu_view->addAction(tr("&Lock Widgets In Place"));
  lock_widgets->setCheckable(true);
  lock_widgets->setChecked(Settings::Instance().AreWidgetsLocked());

  connect(lock_widgets, &QAction::toggled, &Settings::Instance(), &Settings::SetWidgetsLocked);

  mainmenu_view->addSeparator();

  m_debug__show_code = mainmenu_view->addAction(tr("&Code"));
  m_debug__show_code->setCheckable(true);
  m_debug__show_code->setChecked(Settings::Instance().IsCodeVisible());

  connect(m_debug__show_code, &QAction::toggled, &Settings::Instance(), &Settings::SetCodeVisible);
  connect(&Settings::Instance(), &Settings::CodeVisibilityChanged, m_debug__show_code,
          &QAction::setChecked);

  m_debug__show_registers = mainmenu_view->addAction(tr("&Registers"));
  m_debug__show_registers->setCheckable(true);
  m_debug__show_registers->setChecked(Settings::Instance().IsRegistersVisible());

  connect(m_debug__show_registers, &QAction::toggled, &Settings::Instance(),
          &Settings::SetRegistersVisible);
  connect(&Settings::Instance(), &Settings::RegistersVisibilityChanged, m_debug__show_registers,
          &QAction::setChecked);

  m_debug__show_threads = mainmenu_view->addAction(tr("&Threads"));
  m_debug__show_threads->setCheckable(true);
  m_debug__show_threads->setChecked(Settings::Instance().IsThreadsVisible());

  connect(m_debug__show_threads, &QAction::toggled, &Settings::Instance(),
          &Settings::SetThreadsVisible);
  connect(&Settings::Instance(), &Settings::ThreadsVisibilityChanged, m_debug__show_threads,
          &QAction::setChecked);

  // i18n: This kind of "watch" is used for watching emulated memory.
  // It's not related to timekeeping devices.
  m_debug__show_watch = mainmenu_view->addAction(tr("&Watch"));
  m_debug__show_watch->setCheckable(true);
  m_debug__show_watch->setChecked(Settings::Instance().IsWatchVisible());

  connect(m_debug__show_watch, &QAction::toggled, &Settings::Instance(),
          &Settings::SetWatchVisible);
  connect(&Settings::Instance(), &Settings::WatchVisibilityChanged, m_debug__show_watch,
          &QAction::setChecked);

  m_debug__show_breakpoints = mainmenu_view->addAction(tr("&Breakpoints"));
  m_debug__show_breakpoints->setCheckable(true);
  m_debug__show_breakpoints->setChecked(Settings::Instance().IsBreakpointsVisible());

  connect(m_debug__show_breakpoints, &QAction::toggled, &Settings::Instance(),
          &Settings::SetBreakpointsVisible);
  connect(&Settings::Instance(), &Settings::BreakpointsVisibilityChanged, m_debug__show_breakpoints,
          &QAction::setChecked);

  m_debug__show_memory = mainmenu_view->addAction(tr("&Memory"));
  m_debug__show_memory->setCheckable(true);
  m_debug__show_memory->setChecked(Settings::Instance().IsMemoryVisible());

  connect(m_debug__show_memory, &QAction::toggled, &Settings::Instance(),
          &Settings::SetMemoryVisible);
  connect(&Settings::Instance(), &Settings::MemoryVisibilityChanged, m_debug__show_memory,
          &QAction::setChecked);

  m_debug__show_network = mainmenu_view->addAction(tr("&Network"));
  m_debug__show_network->setCheckable(true);
  m_debug__show_network->setChecked(Settings::Instance().IsNetworkVisible());

  connect(m_debug__show_network, &QAction::toggled, &Settings::Instance(),
          &Settings::SetNetworkVisible);
  connect(&Settings::Instance(), &Settings::NetworkVisibilityChanged, m_debug__show_network,
          &QAction::setChecked);

  m_debug__show_jit = mainmenu_view->addAction(tr("&JIT"));
  m_debug__show_jit->setCheckable(true);
  m_debug__show_jit->setChecked(Settings::Instance().IsJITVisible());
  connect(m_debug__show_jit, &QAction::toggled, &Settings::Instance(), &Settings::SetJITVisible);
  connect(&Settings::Instance(), &Settings::JITVisibilityChanged, m_debug__show_jit,
          &QAction::setChecked);

  mainmenu_view->addSeparator();

  AddGameListTypeSection(mainmenu_view);

  mainmenu_view->addSeparator();

  AddListColumnsSubMenu(mainmenu_view);

  mainmenu_view->addSeparator();

  AddShowPlatformsSubMenu(mainmenu_view);
  AddShowRegionsSubMenu(mainmenu_view);

  mainmenu_view->addSeparator();

  QAction* const purge_gamelist_cache =
      mainmenu_view->addAction(tr("Purge Game List Cache"), this, &MenuBar::PurgeGameListCache);
  purge_gamelist_cache->setEnabled(false);

  connect(&Settings::Instance(), &Settings::GameListRefreshRequested, purge_gamelist_cache,
          [purge_gamelist_cache] { purge_gamelist_cache->setEnabled(false); });
  connect(&Settings::Instance(), &Settings::GameListRefreshStarted, purge_gamelist_cache,
          [purge_gamelist_cache] { purge_gamelist_cache->setEnabled(true); });

  mainmenu_view->addSeparator();

  mainmenu_view->addAction(tr("Search"), this, &MenuBar::ShowSearch, QKeySequence::Find);
}

void MenuBar::AddEmulationMainMenu()
{
  QMenu* const mainmenu_emu = addMenu(tr("&Emulation"));
  m_emu_play_action = mainmenu_emu->addAction(tr("&Play"), this, &MenuBar::Play);
  m_emu_pause_action = mainmenu_emu->addAction(tr("&Pause"), this, &MenuBar::Pause);
  m_emu_stop_action = mainmenu_emu->addAction(tr("&Stop"), this, &MenuBar::Stop);
  m_emu_reset_action = mainmenu_emu->addAction(tr("&Reset"), this, &MenuBar::Reset);
  m_emu_fullscreen_action =
      mainmenu_emu->addAction(tr("Toggle &Fullscreen"), this, &MenuBar::Fullscreen);
  m_emu_frame_advance_action =
      mainmenu_emu->addAction(tr("&Frame Advance"), this, &MenuBar::FrameAdvance);

  m_emu_screenshot_action =
      mainmenu_emu->addAction(tr("Take Screenshot"), this, &MenuBar::Screenshot);

  mainmenu_emu->addSeparator();

  QAction* const lag_counter = mainmenu_emu->addAction(tr("Show Lag Counter"));
  lag_counter->setCheckable(true);
  lag_counter->setChecked(SConfig::GetInstance().m_ShowLag);
  connect(lag_counter, &QAction::toggled,
          [](bool value) { SConfig::GetInstance().m_ShowLag = value; });

  QAction* const frame_counter = mainmenu_emu->addAction(tr("Show Frame Counter"));
  frame_counter->setCheckable(true);
  frame_counter->setChecked(SConfig::GetInstance().m_ShowFrameCount);
  connect(frame_counter, &QAction::toggled,
          [](bool value) { SConfig::GetInstance().m_ShowFrameCount = value; });

  QAction* const input_display = mainmenu_emu->addAction(tr("Show Input Display"));
  input_display->setCheckable(true);
  input_display->setChecked(SConfig::GetInstance().m_ShowInputDisplay);
  connect(input_display, &QAction::toggled,
          [](bool value) { SConfig::GetInstance().m_ShowInputDisplay = value; });

  QAction* const system_clock = mainmenu_emu->addAction(tr("Show System Clock"));
  system_clock->setCheckable(true);
  system_clock->setChecked(SConfig::GetInstance().m_ShowRTC);
  connect(system_clock, &QAction::toggled,
          [](bool value) { SConfig::GetInstance().m_ShowRTC = value; });

  mainmenu_emu->addSeparator();

  AddStateLoadSubMenu(mainmenu_emu);
  AddStateSaveSubMenu(mainmenu_emu);
  AddStateSlotSelectSubMenu(mainmenu_emu);
  UpdateStateSlotSubMenuAction();

  for (QMenu* state_menu :
       {m_state_load_submenu, m_state_save_submenu, m_state_slot_select_submenu})
    connect(state_menu, &QMenu::aboutToShow, this, &MenuBar::UpdateStateSlotSubMenuAction);
}

void MenuBar::AddToolsMainMenu()
{
  QMenu* const mainmenu_tools = addMenu(tr("&Tools"));

  mainmenu_tools->addAction(tr("&Resource Pack Manager"), this,
                            [this] { emit ShowResourcePackManager(); });

  m_show_cheat_manager =
      mainmenu_tools->addAction(tr("&Cheats Manager"), this, [this] { emit ShowCheatsManager(); });

  connect(&Settings::Instance(), &Settings::EnableCheatsChanged, this, [this](bool enabled) {
    m_show_cheat_manager->setEnabled(Core::GetState() != Core::State::Uninitialized && enabled);
  });

  mainmenu_tools->addAction(tr("FIFO Player"), this, &MenuBar::ShowFIFOPlayer);

  mainmenu_tools->addSeparator();

  mainmenu_tools->addAction(tr("Start &NetPlay..."), this, &MenuBar::StartNetPlay);
  mainmenu_tools->addAction(tr("Browse &NetPlay Sessions..."), this, &MenuBar::BrowseNetPlay);

  mainmenu_tools->addSeparator();

  AddLoadGameCubeIPLSubMenu(mainmenu_tools);

  mainmenu_tools->addAction(tr("Memory Card Manager"), this, [this] { emit ShowMemcardManager(); });

  mainmenu_tools->addSeparator();

  // Label will be set by a NANDRefresh later
  m_wii_load_sysmenu_action =
      mainmenu_tools->addAction(QString{}, this, [this] { emit BootWiiSystemMenu(); });
  m_wii_load_sysmenu_action->setEnabled(false);

  mainmenu_tools->addAction(tr("Install WAD..."), this, &MenuBar::InstallWAD);

  AddNANDManagementSubMenu(mainmenu_tools);

  AddPerformOnlineSystemUpdateSubMenu(mainmenu_tools);

  mainmenu_tools->addSeparator();

  AddINRECSSubMenu(mainmenu_tools);

  QAction* const dump_frames = mainmenu_tools->addAction(tr("Dump Frames"));
  dump_frames->setCheckable(true);
  dump_frames->setChecked(SConfig::GetInstance().m_DumpFrames);
  connect(dump_frames, &QAction::toggled,
          [](bool value) { SConfig::GetInstance().m_DumpFrames = value; });

  QAction* const dump_audio = mainmenu_tools->addAction(tr("Dump Audio"));
  dump_audio->setCheckable(true);
  dump_audio->setChecked(SConfig::GetInstance().m_DumpAudio);
  connect(dump_audio, &QAction::toggled,
          [](bool value) { SConfig::GetInstance().m_DumpAudio = value; });

  mainmenu_tools->addSeparator();

  mainmenu_tools->addAction(tr("Import Wii Save..."), this, &MenuBar::ImportWiiSave);
  mainmenu_tools->addAction(tr("Export All Wii Saves"), this, &MenuBar::ExportWiiSaves);

  mainmenu_tools->addSeparator();

  AddConnectWiiRemotesSubMenu(mainmenu_tools);
}

void MenuBar::AddOptionsMainMenu()
{
  QMenu* options_menu = addMenu(tr("&Options"));
  options_menu->addAction(tr("Co&nfiguration"), this, &MenuBar::Configure,
                          QKeySequence::Preferences);
  options_menu->addSeparator();
  options_menu->addAction(tr("&Graphics Settings"), this, &MenuBar::ConfigureGraphics);
  options_menu->addAction(tr("&Audio Settings"), this, &MenuBar::ConfigureAudio);
  m_controllers_action =
      options_menu->addAction(tr("&Controller Settings"), this, &MenuBar::ConfigureControllers);
  options_menu->addAction(tr("&Hotkey Settings"), this, &MenuBar::ConfigureHotkeys);

  options_menu->addSeparator();

  // Debugging mode only
  m_debug__boot_to_pause = options_menu->addAction(tr("Boot to Pause"));
  m_debug__boot_to_pause->setCheckable(true);
  m_debug__boot_to_pause->setChecked(SConfig::GetInstance().bBootToPause);

  connect(m_debug__boot_to_pause, &QAction::toggled, this,
          [](bool enable) { SConfig::GetInstance().bBootToPause = enable; });

  m_debug__automatic_start = options_menu->addAction(tr("&Automatic Start"));
  m_debug__automatic_start->setCheckable(true);
  m_debug__automatic_start->setChecked(SConfig::GetInstance().bAutomaticStart);

  connect(m_debug__automatic_start, &QAction::toggled, this,
          [](bool enable) { SConfig::GetInstance().bAutomaticStart = enable; });

  m_debug__change_font = options_menu->addAction(tr("&Font..."), this, &MenuBar::ChangeDebugFont);
}

void MenuBar::AddDebugJITMainMenu()
{
  m_debug__jit_mainmenu = addMenu(tr("JIT"));

  m_debug__jit_interpreter_core = m_debug__jit_mainmenu->addAction(tr("Interpreter Core"));
  m_debug__jit_interpreter_core->setCheckable(true);
  m_debug__jit_interpreter_core->setChecked(SConfig::GetInstance().cpu_core ==
                                            PowerPC::CPUCore::Interpreter);

  connect(m_debug__jit_interpreter_core, &QAction::toggled, [](bool enabled) {
    PowerPC::SetMode(enabled ? PowerPC::CoreMode::Interpreter : PowerPC::CoreMode::JIT);
  });

  m_debug__jit_mainmenu->addSeparator();

  m_debug__jit_block_linking = m_debug__jit_mainmenu->addAction(tr("JIT Block Linking Off"));
  m_debug__jit_block_linking->setCheckable(true);
  m_debug__jit_block_linking->setChecked(SConfig::GetInstance().bJITNoBlockLinking);
  connect(m_debug__jit_block_linking, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITNoBlockLinking = enabled;
    ClearCache();
  });

  m_debug__jit_disable_cache = m_debug__jit_mainmenu->addAction(tr("Disable JIT Cache"));
  m_debug__jit_disable_cache->setCheckable(true);
  m_debug__jit_disable_cache->setChecked(SConfig::GetInstance().bJITNoBlockCache);
  connect(m_debug__jit_disable_cache, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITNoBlockCache = enabled;
    ClearCache();
  });

  m_debug__jit_disable_fastmem = m_debug__jit_mainmenu->addAction(tr("Disable Fastmem"));
  m_debug__jit_disable_fastmem->setCheckable(true);
  m_debug__jit_disable_fastmem->setChecked(!SConfig::GetInstance().bFastmem);
  connect(m_debug__jit_disable_fastmem, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bFastmem = !enabled;
    ClearCache();
  });

  m_debug__jit_clear_cache =
      m_debug__jit_mainmenu->addAction(tr("Clear Cache"), this, &MenuBar::ClearCache);

  m_debug__jit_mainmenu->addSeparator();

  m_debug__jit_log_coverage = m_debug__jit_mainmenu->addAction(tr("Log JIT Instruction Coverage"),
                                                               this, &MenuBar::LogInstructions);
  m_debug__jit_search_instruction = m_debug__jit_mainmenu->addAction(
      tr("Search for an Instruction"), this, &MenuBar::SearchInstruction);

  m_debug__jit_mainmenu->addSeparator();

  m_debug__jit_off = m_debug__jit_mainmenu->addAction(tr("JIT Off (JIT Core)"));
  m_debug__jit_off->setCheckable(true);
  m_debug__jit_off->setChecked(SConfig::GetInstance().bJITOff);
  connect(m_debug__jit_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITOff = enabled;
    ClearCache();
  });

  m_debug__jit_loadstore_off = m_debug__jit_mainmenu->addAction(tr("JIT LoadStore Off"));
  m_debug__jit_loadstore_off->setCheckable(true);
  m_debug__jit_loadstore_off->setChecked(SConfig::GetInstance().bJITLoadStoreOff);
  connect(m_debug__jit_loadstore_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITLoadStoreOff = enabled;
    ClearCache();
  });

  m_debug__jit_loadstore_lbzx_off = m_debug__jit_mainmenu->addAction(tr("JIT LoadStore lbzx Off"));
  m_debug__jit_loadstore_lbzx_off->setCheckable(true);
  m_debug__jit_loadstore_lbzx_off->setChecked(SConfig::GetInstance().bJITLoadStorelbzxOff);
  connect(m_debug__jit_loadstore_lbzx_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITLoadStorelbzxOff = enabled;
    ClearCache();
  });

  m_debug__jit_loadstore_lxz_off = m_debug__jit_mainmenu->addAction(tr("JIT LoadStore lXz Off"));
  m_debug__jit_loadstore_lxz_off->setCheckable(true);
  m_debug__jit_loadstore_lxz_off->setChecked(SConfig::GetInstance().bJITLoadStorelXzOff);
  connect(m_debug__jit_loadstore_lxz_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITLoadStorelXzOff = enabled;
    ClearCache();
  });

  m_debug__jit_loadstore_lwz_off = m_debug__jit_mainmenu->addAction(tr("JIT LoadStore lwz Off"));
  m_debug__jit_loadstore_lwz_off->setCheckable(true);
  m_debug__jit_loadstore_lwz_off->setChecked(SConfig::GetInstance().bJITLoadStorelwzOff);
  connect(m_debug__jit_loadstore_lwz_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITLoadStorelwzOff = enabled;
    ClearCache();
  });

  m_debug__jit_loadstore_floating_off =
      m_debug__jit_mainmenu->addAction(tr("JIT LoadStore Floating Off"));
  m_debug__jit_loadstore_floating_off->setCheckable(true);
  m_debug__jit_loadstore_floating_off->setChecked(SConfig::GetInstance().bJITLoadStoreFloatingOff);
  connect(m_debug__jit_loadstore_floating_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITLoadStoreFloatingOff = enabled;
    ClearCache();
  });

  m_debug__jit_loadstore_paired_off =
      m_debug__jit_mainmenu->addAction(tr("JIT LoadStore Paired Off"));
  m_debug__jit_loadstore_paired_off->setCheckable(true);
  m_debug__jit_loadstore_paired_off->setChecked(SConfig::GetInstance().bJITLoadStorePairedOff);
  connect(m_debug__jit_loadstore_paired_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITLoadStorePairedOff = enabled;
    ClearCache();
  });

  m_debug__jit_floatingpoint_off = m_debug__jit_mainmenu->addAction(tr("JIT FloatingPoint Off"));
  m_debug__jit_floatingpoint_off->setCheckable(true);
  m_debug__jit_floatingpoint_off->setChecked(SConfig::GetInstance().bJITFloatingPointOff);
  connect(m_debug__jit_floatingpoint_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITFloatingPointOff = enabled;
    ClearCache();
  });

  m_debug__jit_integer_off = m_debug__jit_mainmenu->addAction(tr("JIT Integer Off"));
  m_debug__jit_integer_off->setCheckable(true);
  m_debug__jit_integer_off->setChecked(SConfig::GetInstance().bJITIntegerOff);
  connect(m_debug__jit_integer_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITIntegerOff = enabled;
    ClearCache();
  });

  m_debug__jit_paired_off = m_debug__jit_mainmenu->addAction(tr("JIT Paired Off"));
  m_debug__jit_paired_off->setCheckable(true);
  m_debug__jit_paired_off->setChecked(SConfig::GetInstance().bJITPairedOff);
  connect(m_debug__jit_paired_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITPairedOff = enabled;
    ClearCache();
  });

  m_debug__jit_systemregisters_off =
      m_debug__jit_mainmenu->addAction(tr("JIT SystemRegisters Off"));
  m_debug__jit_systemregisters_off->setCheckable(true);
  m_debug__jit_systemregisters_off->setChecked(SConfig::GetInstance().bJITSystemRegistersOff);
  connect(m_debug__jit_systemregisters_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITSystemRegistersOff = enabled;
    ClearCache();
  });

  m_debug__jit_branch_off = m_debug__jit_mainmenu->addAction(tr("JIT Branch Off"));
  m_debug__jit_branch_off->setCheckable(true);
  m_debug__jit_branch_off->setChecked(SConfig::GetInstance().bJITBranchOff);
  connect(m_debug__jit_branch_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITBranchOff = enabled;
    ClearCache();
  });

  m_debug__jit_register_cache_off = m_debug__jit_mainmenu->addAction(tr("JIT Register Cache Off"));
  m_debug__jit_register_cache_off->setCheckable(true);
  m_debug__jit_register_cache_off->setChecked(SConfig::GetInstance().bJITRegisterCacheOff);
  connect(m_debug__jit_register_cache_off, &QAction::toggled, [this](bool enabled) {
    SConfig::GetInstance().bJITRegisterCacheOff = enabled;
    ClearCache();
  });
}

void MenuBar::AddDebugSymbolsMainMenu()
{
  m_debug__symbols_mainmenu = addMenu(tr("Symbols"));

  m_debug__symbols_mainmenu->addAction(tr("&Clear Symbols"), this, &MenuBar::ClearSymbols);

  QMenu* submenu_generate_symbols =
      m_debug__symbols_mainmenu->addMenu(tr("&Generate Symbols From"));
  submenu_generate_symbols->addAction(tr("Address"), this, &MenuBar::GenerateSymbolsFromAddress);
  submenu_generate_symbols->addAction(tr("Signature Database"), this,
                                      &MenuBar::GenerateSymbolsFromSignatureDB);
  submenu_generate_symbols->addAction(tr("RSO Modules"), this, &MenuBar::GenerateSymbolsFromRSO);

  m_debug__symbols_mainmenu->addSeparator();

  m_debug__symbols_mainmenu->addAction(tr("&Load Symbol Map"), this, &MenuBar::LoadSymbolMap);
  m_debug__symbols_mainmenu->addAction(tr("&Save Symbol Map"), this, &MenuBar::SaveSymbolMap);

  m_debug__symbols_mainmenu->addSeparator();

  m_debug__symbols_mainmenu->addAction(tr("Load &Other Map File..."), this,
                                       &MenuBar::LoadOtherSymbolMap);
  m_debug__symbols_mainmenu->addAction(tr("Load &Bad Map File..."), this,
                                       &MenuBar::LoadBadSymbolMap);
  m_debug__symbols_mainmenu->addAction(tr("Save Symbol Map &As..."), this,
                                       &MenuBar::SaveSymbolMapAs);

  m_debug__symbols_mainmenu->addSeparator();

  m_debug__symbols_mainmenu->addAction(tr("Sa&ve Code"), this, &MenuBar::SaveCode);

  m_debug__symbols_mainmenu->addSeparator();

  m_debug__symbols_mainmenu->addAction(tr("C&reate Signature File..."), this,
                                       &MenuBar::CreateSignatureFile);
  m_debug__symbols_mainmenu->addAction(tr("Append to &Existing Signature File..."), this,
                                       &MenuBar::AppendSignatureFile);
  m_debug__symbols_mainmenu->addAction(tr("Combine &Two Signature Files..."), this,
                                       &MenuBar::CombineSignatureFiles);
  m_debug__symbols_mainmenu->addAction(tr("Appl&y Signature File..."), this,
                                       &MenuBar::ApplySignatureFile);

  m_debug__symbols_mainmenu->addSeparator();

  m_debug__symbols_mainmenu->addAction(tr("&Patch HLE Functions"), this,
                                       &MenuBar::PatchHLEFunctions);
}

void MenuBar::AddHelpMainMenu()
{
  QMenu* mainmenu_help = addMenu(tr("&Help"));

  QAction* website = mainmenu_help->addAction(tr("&Website"));
  connect(website, &QAction::triggered, this,
          []() { QDesktopServices::openUrl(QUrl(QStringLiteral("https://dolphin-emu.org/"))); });
  QAction* documentation = mainmenu_help->addAction(tr("Online &Documentation"));
  connect(documentation, &QAction::triggered, this, []() {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://dolphin-emu.org/docs/guides")));
  });
  QAction* github = mainmenu_help->addAction(tr("&GitHub Repository"));
  connect(github, &QAction::triggered, this, []() {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/dolphin-emu/dolphin")));
  });
  QAction* bugtracker = mainmenu_help->addAction(tr("&Bug Tracker"));
  connect(bugtracker, &QAction::triggered, this, []() {
    QDesktopServices::openUrl(
        QUrl(QStringLiteral("https://bugs.dolphin-emu.org/projects/emulator")));
  });

  if (AutoUpdateChecker::SystemSupportsAutoUpdates())
  {
    mainmenu_help->addSeparator();

    mainmenu_help->addAction(tr("&Check for Updates..."), this, &MenuBar::InstallUpdateManually);
  }

#ifndef __APPLE__
  mainmenu_help->addSeparator();
#endif

  mainmenu_help->addAction(tr("&About"), this, &MenuBar::ShowAboutDialog);
}

void MenuBar::AddBootDVDBackupSubMenu(QMenu* mainmenu_file)
{
  m_boot_dvd_backup_submenu = mainmenu_file->addMenu(tr("&Boot from DVD Backup"));

  const std::vector<std::string> drives = Common::GetCDDevices();
  // Windows Limitation of 24 character drives
  for (size_t i = 0; i < drives.size() && i < 24; i++)
  {
    auto drive = QString::fromStdString(drives[i]);
    m_boot_dvd_backup_submenu->addAction(drive, this, [this, drive] { emit BootDVDBackup(drive); });
  }
}

void MenuBar::AddGameListTypeSection(QMenu* mainmenu_view)
{
  QAction* const list_view = mainmenu_view->addAction(tr("List View"));
  list_view->setCheckable(true);

  QAction* const grid_view = mainmenu_view->addAction(tr("Grid View"));
  grid_view->setCheckable(true);

  QActionGroup* const list_group = new QActionGroup(this);
  list_group->addAction(list_view);
  list_group->addAction(grid_view);

  bool prefer_list = Settings::Instance().GetPreferredView();
  list_view->setChecked(prefer_list);
  grid_view->setChecked(!prefer_list);

  connect(list_view, &QAction::triggered, this, &MenuBar::ShowList);
  connect(grid_view, &QAction::triggered, this, &MenuBar::ShowGrid);
}

void MenuBar::AddListColumnsSubMenu(QMenu* mainmenu_view)
{
  m_gamelist_columns_submenu = mainmenu_view->addMenu(tr("List Columns"));

  static const QMap<QString, bool*> columns{
      {tr("Platform"), &SConfig::GetInstance().m_showSystemColumn},
      {tr("Banner"), &SConfig::GetInstance().m_showBannerColumn},
      {tr("Title"), &SConfig::GetInstance().m_showTitleColumn},
      {tr("Description"), &SConfig::GetInstance().m_showDescriptionColumn},
      {tr("Maker"), &SConfig::GetInstance().m_showMakerColumn},
      {tr("File Name"), &SConfig::GetInstance().m_showFileNameColumn},
      {tr("File Path"), &SConfig::GetInstance().m_showFilePathColumn},
      {tr("Game ID"), &SConfig::GetInstance().m_showIDColumn},
      {tr("Region"), &SConfig::GetInstance().m_showRegionColumn},
      {tr("File Size"), &SConfig::GetInstance().m_showSizeColumn},
      {tr("File Format"), &SConfig::GetInstance().m_showFileFormatColumn},
      {tr("Block Size"), &SConfig::GetInstance().m_showBlockSizeColumn},
      {tr("Compression"), &SConfig::GetInstance().m_showCompressionColumn},
      {tr("Tags"), &SConfig::GetInstance().m_showTagsColumn}};

  QActionGroup* const column_group = new QActionGroup(this);
  column_group->setExclusive(false);

  for (const auto& key : columns.keys())
  {
    bool* config = columns[key];
    QAction* action = column_group->addAction(m_gamelist_columns_submenu->addAction(key));
    action->setCheckable(true);
    action->setChecked(*config);
    connect(action, &QAction::toggled, [this, config, key](bool value) {
      *config = value;
      emit ColumnVisibilityToggled(key, value);
    });
  }
}

void MenuBar::AddShowPlatformsSubMenu(QMenu* mainmenu_view)
{
  QMenu* const submenu_platforms = mainmenu_view->addMenu(tr("Show Platforms"));

  static const QMap<QString, bool*> platform_map{
      {tr("Show Wii"), &SConfig::GetInstance().m_ListWii},
      {tr("Show GameCube"), &SConfig::GetInstance().m_ListGC},
      {tr("Show WAD"), &SConfig::GetInstance().m_ListWad},
      {tr("Show ELF/DOL"), &SConfig::GetInstance().m_ListElfDol}};

  QActionGroup* const platform_group = new QActionGroup(this);
  platform_group->setExclusive(false);

  for (const auto& key : platform_map.keys())
  {
    bool* const config = platform_map[key];
    QAction* const action = platform_group->addAction(submenu_platforms->addAction(key));
    action->setCheckable(true);
    action->setChecked(*config);
    connect(action, &QAction::toggled, [this, config, key](bool value) {
      *config = value;
      emit GameListPlatformVisibilityToggled(key, value);
    });
  }
}

void MenuBar::AddShowRegionsSubMenu(QMenu* mainmenu_view)
{
  QMenu* const submenu_regions = mainmenu_view->addMenu(tr("Show Regions"));

  const QAction* const show_all_regions = submenu_regions->addAction(tr("Show All"));
  const QAction* const hide_all_regions = submenu_regions->addAction(tr("Hide All"));

  submenu_regions->addSeparator();

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

  for (const auto& key : region_map.keys())
  {
    bool* const config = region_map[key];
    QAction* const menu_item = submenu_regions->addAction(key);
    menu_item->setCheckable(true);
    menu_item->setChecked(*config);

    const auto set_visibility = [this, config, key, menu_item](bool visibility) {
      menu_item->setChecked(visibility);
      *config = visibility;
      emit GameListRegionVisibilityToggled(key, visibility);
    };
    const auto set_visible = std::bind(set_visibility, true);
    const auto set_hidden = std::bind(set_visibility, false);

    connect(menu_item, &QAction::toggled, set_visibility);
    connect(show_all_regions, &QAction::triggered, menu_item, set_visible);
    connect(hide_all_regions, &QAction::triggered, menu_item, set_hidden);
  }
}

void MenuBar::AddStateLoadSubMenu(QMenu* mainmenu_emu)
{
  m_state_load_submenu = mainmenu_emu->addMenu(tr("&Load State"));

  m_state_load_submenu->addAction(tr("Load State from File"), this, &MenuBar::StateLoadFile);
  m_state_load_submenu->addAction(tr("Load State from Selected Slot"), this,
                                  &MenuBar::StateLoadSlot);
  m_state_load_slots_submenu = m_state_load_submenu->addMenu(tr("Load State from Slot"));
  m_state_load_submenu->addAction(tr("Undo Load State"), this, &MenuBar::StateLoadUndo);

  for (int i = 1; i <= 10; i++)
  {
    QAction* slot = m_state_load_slots_submenu->addAction(QString{});

    connect(slot, &QAction::triggered, this, [=]() { emit StateLoadSlotAt(i); });
  }
}

void MenuBar::AddStateSaveSubMenu(QMenu* mainmenu_emu)
{
  m_state_save_submenu = mainmenu_emu->addMenu(tr("Sa&ve State"));

  m_state_save_submenu->addAction(tr("Save State to File"), this, &MenuBar::StateSaveFile);
  m_state_save_submenu->addAction(tr("Save State to Selected Slot"), this, &MenuBar::StateSaveSlot);
  m_state_save_submenu->addAction(tr("Save State to Oldest Slot"), this,
                                  &MenuBar::StateSaveSlotOldest);
  m_state_save_slots_submenu = m_state_save_submenu->addMenu(tr("Save State to Slot"));
  m_state_save_submenu->addAction(tr("Undo Save State"), this, &MenuBar::StateSaveUndo);

  for (int i = 1; i <= 10; i++)
  {
    QAction* slot = m_state_save_slots_submenu->addAction(QString{});

    connect(slot, &QAction::triggered, this, [=]() { emit StateSaveSlotAt(i); });
  }
}

void MenuBar::AddStateSlotSelectSubMenu(QMenu* mainmenu_emu)
{
  m_state_slot_select_submenu = mainmenu_emu->addMenu(tr("Select State Slot"));
  m_state_slots = new QActionGroup(this);

  for (int i = 1; i <= 10; i++)
  {
    QAction* slot = m_state_slot_select_submenu->addAction(QString{});
    slot->setCheckable(true);
    slot->setActionGroup(m_state_slots);
    if (Settings::Instance().GetStateSlot() == i)
      slot->setChecked(true);

    connect(slot, &QAction::triggered, this, [=]() { emit SetStateSlot(i); });
  }
}

void MenuBar::UpdateStateSlotSubMenuAction()
{
  QList<QAction*> actions_slot = m_state_slots->actions();
  QList<QAction*> actions_load = m_state_load_slots_submenu->actions();
  QList<QAction*> actions_save = m_state_save_slots_submenu->actions();

  for (int i = 0; i < actions_slot.length(); i++)
  {
    int slot = i + 1;
    QString info = QString::fromStdString(State::GetInfoStringOfSlot(slot));
    actions_load.at(i)->setText(tr("Load from Slot %1 - %2").arg(slot).arg(info));
    actions_save.at(i)->setText(tr("Save to Slot %1 - %2").arg(slot).arg(info));
    actions_slot.at(i)->setText(tr("Select Slot %1 - %2").arg(slot).arg(info));
  }
}

void MenuBar::AddLoadGameCubeIPLSubMenu(QMenu* mainmenu_tools)
{
  QMenu* const submenu_gc_load_ipl = mainmenu_tools->addMenu(tr("Load GameCube Main Menu (IPL)"));

  m_gc_load_ipl_ntscj = submenu_gc_load_ipl->addAction(
      tr("NTSC-J"), this, [this] { emit BootGameCubeIPL(DiscIO::Region::NTSC_J); });
  m_gc_load_ipl_ntscu = submenu_gc_load_ipl->addAction(
      tr("NTSC-U"), this, [this] { emit BootGameCubeIPL(DiscIO::Region::NTSC_U); });
  m_gc_load_ipl_pal = submenu_gc_load_ipl->addAction(
      tr("PAL"), this, [this] { emit BootGameCubeIPL(DiscIO::Region::PAL); });
}

void MenuBar::AddNANDManagementSubMenu(QMenu* mainmenu_tools)
{
  QMenu* const submenu_nand_management = mainmenu_tools->addMenu(tr("NAND Management"));

  m_nand_import_merge_secondary = submenu_nand_management->addAction(
      tr("Import and Merge secondary NAND..."), this, [this] { emit ImportMergeSecondaryNAND(); });
  m_nand_check =
      submenu_nand_management->addAction(tr("Check NAND..."), this, &MenuBar::NAND_Check);
  m_nand_extract_certificates = submenu_nand_management->addAction(
      tr("Extract Certificates from NAND"), this, &MenuBar::NAND_ExtractCertificates);

  connect(&Settings::Instance(), &Settings::NANDRefresh, this, [this] { UpdateMenu_Tools(false); });
}

void MenuBar::AddPerformOnlineSystemUpdateSubMenu(QMenu* mainmenu_tools)
{
  m_perform_online_update_submenu = mainmenu_tools->addMenu(tr("Perform Online System Update"));

  m_perform_online_update_for_current_region = m_perform_online_update_submenu->addAction(
      tr("Current Region"), this, [this] { emit PerformOnlineUpdate(""); });

  m_perform_online_update_submenu->addSeparator();

  m_perform_online_update_submenu->addAction(tr("Europe"), this,
                                             [this] { emit PerformOnlineUpdate("EUR"); });
  m_perform_online_update_submenu->addAction(tr("Japan"), this,
                                             [this] { emit PerformOnlineUpdate("JPN"); });
  m_perform_online_update_submenu->addAction(tr("Korea"), this,
                                             [this] { emit PerformOnlineUpdate("KOR"); });
  m_perform_online_update_submenu->addAction(tr("United States"), this,
                                             [this] { emit PerformOnlineUpdate("USA"); });
}

void MenuBar::AddINRECSSubMenu(QMenu* mainmenu_tools)
{
  QMenu* const submenu_inrecs = mainmenu_tools->addMenu(tr("&Input Recording System"));

  m_inrecs_start = submenu_inrecs->addAction(tr("Start Re&cording Input"), this,
                                             [this] { emit INRECSStartRecording(); });
  m_inrecs_replay = submenu_inrecs->addAction(tr("P&lay Recorded Input Track..."), this,
                                              [this] { emit INRECSPlayRecordedInputTrack(); });
  m_inrecs_stop = submenu_inrecs->addAction(tr("Stop Playing/Recording Input"), this,
                                            [this] { emit INRECSStopRecording(); });
  m_inrecs_export = submenu_inrecs->addAction(tr("Export Recording..."), this,
                                              [this] { emit INRECSExportRecording(); });

  m_inrecs_start->setEnabled(false);
  m_inrecs_replay->setEnabled(false);
  m_inrecs_stop->setEnabled(false);
  m_inrecs_export->setEnabled(false);

  submenu_inrecs->addAction(tr("TAS Input Configurator"), this,
                            [this] { emit INRECSShowTASInputConfig(); });

  submenu_inrecs->addSeparator();

  m_inrecs_read_only = submenu_inrecs->addAction(tr("&Read-Only Mode"));
  m_inrecs_read_only->setCheckable(true);
  m_inrecs_read_only->setChecked(Movie::IsReadOnly());
  connect(m_inrecs_read_only, &QAction::toggled, [](bool value) { Movie::SetReadOnly(value); });

  QAction* const pause_at_end = submenu_inrecs->addAction(tr("Pause at End of Track"));
  pause_at_end->setCheckable(true);
  pause_at_end->setChecked(SConfig::GetInstance().m_PauseMovie);
  connect(pause_at_end, &QAction::toggled,
          [](bool value) { SConfig::GetInstance().m_PauseMovie = value; });
}

void MenuBar::AddConnectWiiRemotesSubMenu(QMenu* const mainmenu_tools)
{
  QMenu* const submenu_connect_wiimotes = mainmenu_tools->addMenu(tr("Connect Wii Remotes"));

  for (int i = 0; i < 4; i++)
  {
    m_wii_remotes[i] = submenu_connect_wiimotes->addAction(
        tr("Connect Wii Remote %1").arg(i + 1), this, [this, i] { emit ConnectWiiRemote(i); });
    m_wii_remotes[i]->setCheckable(true);
  }

  submenu_connect_wiimotes->addSeparator();

  m_wii_remotes[4] = submenu_connect_wiimotes->addAction(tr("Connect Balance Board"), this,
                                                         [this] { emit ConnectWiiRemote(4); });
  m_wii_remotes[4]->setCheckable(true);
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

void MenuBar::NAND_Check()
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

void MenuBar::NAND_ExtractCertificates()
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

void MenuBar::OnGameSelectionChanged(std::shared_ptr<const UICommon::GameFile> game_file)
{
  m_game_selected = !!game_file;

  m_inrecs_replay->setEnabled(m_game_selected && !Core::IsRunning());
  m_inrecs_start->setEnabled((m_game_selected || Core::IsRunning()) && !Movie::IsPlayingInput());
}

void MenuBar::OnINRECSStatusChanged(bool recording)
{
  m_inrecs_start->setEnabled(!recording && (m_game_selected || Core::IsRunning()));
  m_inrecs_stop->setEnabled(recording);
  m_inrecs_export->setEnabled(recording);
}

void MenuBar::OnINRECSReadOnlyModeChanged(bool read_only)
{
  m_inrecs_read_only->setChecked(read_only);
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
  PPCAnalyst::FindFunctions(Memory::MEM1_BASE_ADDR,
                            Memory::MEM1_BASE_ADDR + Memory::GetRamSizeReal(), &g_symbolDB);
  emit NotifySymbolsUpdated();
}

void MenuBar::GenerateSymbolsFromSignatureDB()
{
  PPCAnalyst::FindFunctions(Memory::MEM1_BASE_ADDR,
                            Memory::MEM1_BASE_ADDR + Memory::GetRamSizeReal(), &g_symbolDB);
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
  // i18n: RSO refers to a proprietary format for shared objects (like DLL files).
  const int ret =
      ModalMessageBox::question(this, tr("RSO auto-detection"), tr("Auto-detect RSO modules?"));
  if (ret == QMessageBox::Yes)
    return GenerateSymbolsFromRSOAuto();

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

void MenuBar::GenerateSymbolsFromRSOAuto()
{
  constexpr std::array<std::string_view, 2> search_for = {".elf", ".plf"};
  const AddressSpace::Accessors* accessors =
      AddressSpace::GetAccessors(AddressSpace::Type::Effective);
  std::vector<std::pair<u32, std::string>> matches;

  // Find filepath to elf/plf commonly used by RSO modules
  for (const auto& str : search_for)
  {
    u32 next = 0;
    while (true)
    {
      auto found_addr =
          accessors->Search(next, reinterpret_cast<const u8*>(str.data()), str.size() + 1, true);

      if (!found_addr.has_value())
        break;
      next = *found_addr + 1;

      // Get the beginning of the string
      found_addr = accessors->Search(*found_addr, reinterpret_cast<const u8*>(""), 1, false);
      if (!found_addr.has_value())
        continue;

      // Get the string reference
      const u32 ref_addr = *found_addr + 1;
      const std::array<u8, 4> ref = {static_cast<u8>(ref_addr >> 24),
                                     static_cast<u8>(ref_addr >> 16),
                                     static_cast<u8>(ref_addr >> 8), static_cast<u8>(ref_addr)};
      found_addr = accessors->Search(ref_addr, ref.data(), ref.size(), false);
      if (!found_addr.has_value() || *found_addr < 16)
        continue;

      // Go to the beginning of the RSO header
      matches.emplace_back(*found_addr - 16, PowerPC::HostGetString(ref_addr, 128));
    }
  }

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
  const QString item = QInputDialog::getItem(
      this, tr("Input"), tr("Select the RSO module address:"), items, 0, false, &ok);

  if (!ok)
    return;

  RSOChainView rso_chain;
  const u32 address = item.mid(0, item.indexOf(QLatin1Char(' '))).toUInt(nullptr, 16);
  if (rso_chain.Load(address))
  {
    rso_chain.Apply(&g_symbolDB);
    emit NotifySymbolsUpdated();
  }
  else
  {
    ModalMessageBox::warning(this, tr("Error"), tr("Failed to load RSO module at %1").arg(address));
  }
}

void MenuBar::LoadSymbolMap()
{
  std::string existing_map_file, writable_map_file;
  bool map_exists = CBoot::FindMapFile(&existing_map_file, &writable_map_file);

  if (!map_exists)
  {
    g_symbolDB.Clear();
    PPCAnalyst::FindFunctions(Memory::MEM1_BASE_ADDR + 0x1300000,
                              Memory::MEM1_BASE_ADDR + Memory::GetRamSizeReal(), &g_symbolDB);
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

  const QString file = QFileDialog::getSaveFileName(this, tr("Save signature file"),
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
      this, tr("Input"), tr("Only append symbols with prefix:\n(Blank for all symbols)"));

  const QString file = QFileDialog::getSaveFileName(this, tr("Append signature to"),
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
  const QString file = QFileDialog::getOpenFileName(this, tr("Apply signature file"),
                                                    QDir::homePath(), GetSignatureSelector());

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
  const QString priorityFile = QFileDialog::getOpenFileName(
      this, tr("Choose priority input file"), QDir::homePath(), GetSignatureSelector());
  if (priorityFile.isEmpty())
    return;

  const QString secondaryFile = QFileDialog::getOpenFileName(
      this, tr("Choose secondary input file"), QDir::homePath(), GetSignatureSelector());
  if (secondaryFile.isEmpty())
    return;

  const QString saveFile = QFileDialog::getSaveFileName(this, tr("Save combined output file as"),
                                                        QDir::homePath(), GetSignatureSelector());
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
  const QString op = QInputDialog::getText(this, tr("Search instruction"), tr("Instruction:"),
                                           QLineEdit::Normal, QString{}, &good);

  if (!good)
    return;

  bool found = false;
  for (u32 addr = Memory::MEM1_BASE_ADDR; addr < Memory::MEM1_BASE_ADDR + Memory::GetRamSizeReal();
       addr += 4)
  {
    auto ins_name =
        QString::fromStdString(PPCTables::GetInstructionName(PowerPC::HostRead_U32(addr)));
    if (op == ins_name)
    {
      NOTICE_LOG_FMT(POWERPC, "Found {} at {:08x}", op.toStdString(), addr);
      found = true;
    }
  }
  if (!found)
    NOTICE_LOG_FMT(POWERPC, "Opcode {} not found", op.toStdString());
}
