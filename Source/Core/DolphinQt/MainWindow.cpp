// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/MainWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QIcon>
#include <QMimeData>
#include <QStackedWidget>
#include <QStyleHints>
#include <QVBoxLayout>
#include <QWindow>

#include <fmt/format.h>

#include <future>
#include <optional>
#include <variant>

#if defined(__unix__) || defined(__unix) || defined(__APPLE__)
#include <signal.h>

#include "QtUtils/SignalDaemon.h"
#endif

#ifndef _WIN32
#include <qpa/qplatformnativeinterface.h>
#endif

#include "Common/ScopeGuard.h"
#include "Common/Version.h"
#include "Common/WindowSystemInfo.h"

#include "Core/AchievementManager.h"
#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/CommonTitles.h"
#include "Core/Config/AchievementSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/NetplaySettings.h"
#include "Core/Config/WiimoteSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/FreeLookManager.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/GBAPad.h"
#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HotkeyManager.h"
#include "Core/IOS/USB/Bluetooth/BTEmu.h"
#include "Core/IOS/USB/Bluetooth/WiimoteDevice.h"
#include "Core/Movie.h"
#include "Core/NetPlayClient.h"
#include "Core/NetPlayProto.h"
#include "Core/NetPlayServer.h"
#include "Core/State.h"
#include "Core/System.h"
#include "Core/WiiUtils.h"

#include "DiscIO/DirectoryBlob.h"
#include "DiscIO/NANDImporter.h"
#include "DiscIO/RiivolutionPatcher.h"

#include "DolphinQt/AboutDialog.h"
#include "DolphinQt/Achievements/AchievementsWindow.h"
#include "DolphinQt/CheatsManager.h"
#include "DolphinQt/Config/ControllersWindow.h"
#include "DolphinQt/Config/FreeLookWindow.h"
#include "DolphinQt/Config/Graphics/GraphicsWindow.h"
#include "DolphinQt/Config/LogConfigWidget.h"
#include "DolphinQt/Config/LogWidget.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/Config/SettingsWindow.h"
#include "DolphinQt/Debugger/BreakpointWidget.h"
#include "DolphinQt/Debugger/CodeViewWidget.h"
#include "DolphinQt/Debugger/CodeWidget.h"
#include "DolphinQt/Debugger/JITWidget.h"
#include "DolphinQt/Debugger/MemoryWidget.h"
#include "DolphinQt/Debugger/NetworkWidget.h"
#include "DolphinQt/Debugger/RegisterWidget.h"
#include "DolphinQt/Debugger/ThreadWidget.h"
#include "DolphinQt/Debugger/WatchWidget.h"
#include "DolphinQt/DiscordHandler.h"
#include "DolphinQt/FIFO/FIFOPlayerWindow.h"
#include "DolphinQt/GCMemcardManager.h"
#include "DolphinQt/GameList/GameList.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/HotkeyScheduler.h"
#include "DolphinQt/InfinityBase/InfinityBaseWindow.h"
#include "DolphinQt/MenuBar.h"
#include "DolphinQt/NKitWarningDialog.h"
#include "DolphinQt/NetPlay/NetPlayBrowser.h"
#include "DolphinQt/NetPlay/NetPlayDialog.h"
#include "DolphinQt/NetPlay/NetPlaySetupDialog.h"
#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/QtUtils/FileOpenEventFilter.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/ParallelProgressDialog.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/QtUtils/RunOnObject.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
#include "DolphinQt/QtUtils/WindowActivationEventFilter.h"
#include "DolphinQt/RenderWidget.h"
#include "DolphinQt/ResourcePackManager.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/RiivolutionBootWidget.h"
#include "DolphinQt/SearchBar.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/SkylanderPortal/SkylanderPortalWindow.h"
#include "DolphinQt/TAS/GBATASInputWindow.h"
#include "DolphinQt/TAS/GCTASInputWindow.h"
#include "DolphinQt/TAS/WiiTASInputWindow.h"
#include "DolphinQt/ToolBar.h"
#include "DolphinQt/WiiUpdate.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/GCAdapter.h"

#include "UICommon/DiscordPresence.h"
#include "UICommon/GameFile.h"
#include "UICommon/ResourcePack/Manager.h"
#include "UICommon/ResourcePack/Manifest.h"
#include "UICommon/ResourcePack/ResourcePack.h"

#include "UICommon/UICommon.h"

#include "VideoCommon/NetPlayChatUI.h"
#include "VideoCommon/VideoConfig.h"

#ifdef HAVE_XRANDR
#include "UICommon/X11Utils.h"
// This #define within X11/X.h conflicts with our WiimoteSource enum.
#undef None
#endif

#if defined(__unix__) || defined(__unix) || defined(__APPLE__)
void MainWindow::OnSignal()
{
  close();
}

static void InstallSignalHandler()
{
  struct sigaction sa;
  sa.sa_handler = &SignalDaemon::HandleInterrupt;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESETHAND;
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
}
#endif

static WindowSystemType GetWindowSystemType()
{
  // Determine WSI type based on Qt platform.
  QString platform_name = QGuiApplication::platformName();
  if (platform_name == QStringLiteral("windows"))
    return WindowSystemType::Windows;
  else if (platform_name == QStringLiteral("cocoa"))
    return WindowSystemType::MacOS;
  else if (platform_name == QStringLiteral("xcb"))
    return WindowSystemType::X11;
  else if (platform_name == QStringLiteral("wayland"))
    return WindowSystemType::Wayland;
  else if (platform_name == QStringLiteral("haiku"))
    return WindowSystemType::Haiku;

  ModalMessageBox::critical(
      nullptr, QStringLiteral("Error"),
      QString::asprintf("Unknown Qt platform: %s", platform_name.toStdString().c_str()));
  return WindowSystemType::Headless;
}

static WindowSystemInfo GetWindowSystemInfo(QWindow* window)
{
  WindowSystemInfo wsi;
  wsi.type = GetWindowSystemType();

  // Our Win32 Qt external doesn't have the private API.
#if defined(WIN32) || defined(__APPLE__) || defined(__HAIKU__)
  wsi.render_window = window ? reinterpret_cast<void*>(window->winId()) : nullptr;
  wsi.render_surface = wsi.render_window;
#else
  QPlatformNativeInterface* pni = QGuiApplication::platformNativeInterface();
  wsi.display_connection = pni->nativeResourceForWindow("display", window);
  if (wsi.type == WindowSystemType::Wayland)
    wsi.render_window = window ? pni->nativeResourceForWindow("surface", window) : nullptr;
  else
    wsi.render_window = window ? reinterpret_cast<void*>(window->winId()) : nullptr;
  wsi.render_surface = wsi.render_window;
#endif
  wsi.render_surface_scale = window ? static_cast<float>(window->devicePixelRatio()) : 1.0f;

  return wsi;
}

static std::vector<std::string> StringListToStdVector(QStringList list)
{
  std::vector<std::string> result;
  result.reserve(list.size());

  for (const QString& s : list)
    result.push_back(s.toStdString());

  return result;
}

MainWindow::MainWindow(std::unique_ptr<BootParameters> boot_parameters,
                       const std::string& movie_path)
    : QMainWindow(nullptr)
{
  setWindowTitle(QString::fromStdString(Common::GetScmRevStr()));
  setWindowIcon(Resources::GetAppIcon());
  setUnifiedTitleAndToolBarOnMac(true);
  setAcceptDrops(true);
  setAttribute(Qt::WA_NativeWindow);

  InitControllers();

  CreateComponents();

  ConnectGameList();
  ConnectHost();
  ConnectToolBar();
  ConnectRenderWidget();
  ConnectStack();
  ConnectMenuBar();
  ConnectHotkeys();

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
          [](Qt::ColorScheme colorScheme) { Settings::Instance().ApplyStyle(); });
#endif

  connect(m_cheats_manager, &CheatsManager::OpenGeneralSettings, this,
          &MainWindow::ShowGeneralWindow);

#ifdef USE_RETRO_ACHIEVEMENTS
  connect(m_cheats_manager, &CheatsManager::OpenAchievementSettings, this,
          &MainWindow::ShowAchievementSettings);
  connect(m_game_list, &GameList::OpenAchievementSettings, this,
          &MainWindow::ShowAchievementSettings);
#endif  // USE_RETRO_ACHIEVEMENTS

  InitCoreCallbacks();

  NetPlayInit();

#ifdef USE_RETRO_ACHIEVEMENTS
  AchievementManager::GetInstance()->Init();
#endif  // USE_RETRO_ACHIEVEMENTS

#if defined(__unix__) || defined(__unix) || defined(__APPLE__)
  auto* daemon = new SignalDaemon(this);

  connect(daemon, &SignalDaemon::InterruptReceived, this, &MainWindow::OnSignal);

  InstallSignalHandler();
#endif

  if (boot_parameters)
  {
    m_pending_boot = std::move(boot_parameters);

    if (!movie_path.empty())
    {
      std::optional<std::string> savestate_path;
      if (Movie::PlayInput(movie_path, &savestate_path))
      {
        m_pending_boot->boot_session_data.SetSavestateData(std::move(savestate_path),
                                                           DeleteSavestateAfterBoot::No);
        emit RecordingStatusChanged(true);
      }
    }
  }

  m_state_slot =
      std::clamp(Settings::Instance().GetStateSlot(), 1, static_cast<int>(State::NUM_STATES));

  QSettings& settings = Settings::GetQSettings();

  restoreState(settings.value(QStringLiteral("mainwindow/state")).toByteArray());
  restoreGeometry(settings.value(QStringLiteral("mainwindow/geometry")).toByteArray());

  m_render_widget_geometry = settings.value(QStringLiteral("renderwidget/geometry")).toByteArray();

  // Restoring of window states can sometimes go wrong, resulting in widgets being visible when they
  // shouldn't be so we have to reapply all our rules afterwards.
  Settings::Instance().RefreshWidgetVisibility();

  if (!ResourcePack::Init())
  {
    ModalMessageBox::critical(this, tr("Error"),
                              tr("Error occurred while loading some texture packs"));
  }

  for (auto& pack : ResourcePack::GetPacks())
  {
    if (!pack.IsValid())
    {
      ModalMessageBox::critical(this, tr("Error"),
                                tr("Invalid Pack %1 provided: %2")
                                    .arg(QString::fromStdString(pack.GetPath()))
                                    .arg(QString::fromStdString(pack.GetError())));
      return;
    }
  }

  Host::GetInstance()->SetMainWindowHandle(reinterpret_cast<void*>(winId()));
}

MainWindow::~MainWindow()
{
  // Shut down NetPlay first to avoid race condition segfault
  Settings::Instance().ResetNetPlayClient();
  Settings::Instance().ResetNetPlayServer();

#ifdef USE_RETRO_ACHIEVEMENTS
  AchievementManager::GetInstance()->Shutdown();
#endif  // USE_RETRO_ACHIEVEMENTS

  delete m_render_widget;
  delete m_netplay_dialog;

  for (int i = 0; i < 4; i++)
  {
    delete m_gc_tas_input_windows[i];
    delete m_gba_tas_input_windows[i];
    delete m_wii_tas_input_windows[i];
  }

  ShutdownControllers();

  QSettings& settings = Settings::GetQSettings();

  settings.setValue(QStringLiteral("mainwindow/state"), saveState());
  settings.setValue(QStringLiteral("mainwindow/geometry"), saveGeometry());

  settings.setValue(QStringLiteral("renderwidget/geometry"), m_render_widget_geometry);

  Config::Save();
}

WindowSystemInfo MainWindow::GetWindowSystemInfo() const
{
  return ::GetWindowSystemInfo(m_render_widget->windowHandle());
}

void MainWindow::InitControllers()
{
  if (g_controller_interface.IsInit())
    return;

  UICommon::InitControllers(::GetWindowSystemInfo(windowHandle()));

  m_hotkey_scheduler = new HotkeyScheduler();
  m_hotkey_scheduler->Start();

  // Defaults won't work reliably without loading and saving the config first

  Wiimote::LoadConfig();
  Wiimote::GetConfig()->SaveConfig();

  Pad::LoadConfig();
  Pad::GetConfig()->SaveConfig();

  Pad::LoadGBAConfig();
  Pad::GetGBAConfig()->SaveConfig();

  Keyboard::LoadConfig();
  Keyboard::GetConfig()->SaveConfig();

  FreeLook::LoadInputConfig();
  FreeLook::GetInputConfig()->SaveConfig();
}

void MainWindow::ShutdownControllers()
{
  m_hotkey_scheduler->Stop();

  Settings::Instance().UnregisterDevicesChangedCallback();

  UICommon::ShutdownControllers();

  m_hotkey_scheduler->deleteLater();
}

void MainWindow::InitCoreCallbacks()
{
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    if (state == Core::State::Uninitialized)
      OnStopComplete();

    if (state == Core::State::Running && m_fullscreen_requested)
    {
      FullScreen();
      m_fullscreen_requested = false;
    }
  });
  installEventFilter(this);
  m_render_widget->installEventFilter(this);

  // Handle file open events
  auto* filter = new FileOpenEventFilter(QGuiApplication::instance());
  connect(filter, &FileOpenEventFilter::fileOpened, this, [this](const QString& file_name) {
    StartGame(BootParameters::GenerateFromFile(file_name.toStdString()));
  });
}

static void InstallHotkeyFilter(QWidget* dialog)
{
  auto* filter = new WindowActivationEventFilter(dialog);
  dialog->installEventFilter(filter);

  filter->connect(filter, &WindowActivationEventFilter::windowDeactivated,
                  [] { HotkeyManagerEmu::Enable(true); });
  filter->connect(filter, &WindowActivationEventFilter::windowActivated,
                  [] { HotkeyManagerEmu::Enable(false); });
}

void MainWindow::CreateComponents()
{
  m_menu_bar = new MenuBar(this);
  m_tool_bar = new ToolBar(this);
  m_search_bar = new SearchBar(this);
  m_game_list = new GameList(this);
  m_render_widget = new RenderWidget;
  m_stack = new QStackedWidget(this);

  for (int i = 0; i < 4; i++)
  {
    m_gc_tas_input_windows[i] = new GCTASInputWindow(nullptr, i);
    m_gba_tas_input_windows[i] = new GBATASInputWindow(nullptr, i);
    m_wii_tas_input_windows[i] = new WiiTASInputWindow(nullptr, i);
  }

  m_jit_widget = new JITWidget(this);
  m_log_widget = new LogWidget(this);
  m_log_config_widget = new LogConfigWidget(this);
  m_memory_widget = new MemoryWidget(this);
  m_network_widget = new NetworkWidget(this);
  m_register_widget = new RegisterWidget(this);
  m_thread_widget = new ThreadWidget(this);
  m_watch_widget = new WatchWidget(this);
  m_breakpoint_widget = new BreakpointWidget(this);
  m_code_widget = new CodeWidget(this);
  m_cheats_manager = new CheatsManager(this);

  const auto request_watch = [this](QString name, u32 addr) {
    m_watch_widget->AddWatch(name, addr);
  };
  const auto request_breakpoint = [this](u32 addr) { m_breakpoint_widget->AddBP(addr); };
  const auto request_memory_breakpoint = [this](u32 addr) {
    m_breakpoint_widget->AddAddressMBP(addr);
  };
  const auto request_view_in_memory = [this](u32 addr) { m_memory_widget->SetAddress(addr); };
  const auto request_view_in_code = [this](u32 addr) {
    m_code_widget->SetAddress(addr, CodeViewWidget::SetAddressUpdate::WithDetailedUpdate);
  };

  connect(m_watch_widget, &WatchWidget::RequestMemoryBreakpoint, request_memory_breakpoint);
  connect(m_watch_widget, &WatchWidget::ShowMemory, m_memory_widget, &MemoryWidget::SetAddress);
  connect(m_register_widget, &RegisterWidget::RequestMemoryBreakpoint, request_memory_breakpoint);
  connect(m_register_widget, &RegisterWidget::RequestWatch, request_watch);
  connect(m_register_widget, &RegisterWidget::RequestViewInMemory, request_view_in_memory);
  connect(m_register_widget, &RegisterWidget::RequestViewInCode, request_view_in_code);
  connect(m_thread_widget, &ThreadWidget::RequestBreakpoint, request_breakpoint);
  connect(m_thread_widget, &ThreadWidget::RequestMemoryBreakpoint, request_memory_breakpoint);
  connect(m_thread_widget, &ThreadWidget::RequestWatch, request_watch);
  connect(m_thread_widget, &ThreadWidget::RequestViewInMemory, request_view_in_memory);
  connect(m_thread_widget, &ThreadWidget::RequestViewInCode, request_view_in_code);

  connect(m_code_widget, &CodeWidget::BreakpointsChanged, m_breakpoint_widget,
          &BreakpointWidget::Update);
  connect(m_code_widget, &CodeWidget::RequestPPCComparison, m_jit_widget, &JITWidget::Compare);
  connect(m_code_widget, &CodeWidget::ShowMemory, m_memory_widget, &MemoryWidget::SetAddress);
  connect(m_memory_widget, &MemoryWidget::BreakpointsChanged, m_breakpoint_widget,
          &BreakpointWidget::Update);
  connect(m_memory_widget, &MemoryWidget::ShowCode, m_code_widget, [this](u32 address) {
    m_code_widget->SetAddress(address, CodeViewWidget::SetAddressUpdate::WithDetailedUpdate);
  });
  connect(m_memory_widget, &MemoryWidget::RequestWatch, request_watch);

  connect(m_breakpoint_widget, &BreakpointWidget::BreakpointsChanged, m_code_widget,
          &CodeWidget::Update);
  connect(m_breakpoint_widget, &BreakpointWidget::BreakpointsChanged, m_memory_widget,
          &MemoryWidget::Update);
  connect(m_breakpoint_widget, &BreakpointWidget::ShowCode, [this](u32 address) {
    if (Core::GetState() == Core::State::Paused)
      m_code_widget->SetAddress(address, CodeViewWidget::SetAddressUpdate::WithDetailedUpdate);
  });
  connect(m_breakpoint_widget, &BreakpointWidget::ShowMemory, m_memory_widget,
          &MemoryWidget::SetAddress);
  connect(m_cheats_manager, &CheatsManager::ShowMemory, m_memory_widget, &MemoryWidget::SetAddress);
  connect(m_cheats_manager, &CheatsManager::RequestWatch, request_watch);
}

void MainWindow::ConnectMenuBar()
{
  setMenuBar(m_menu_bar);
  // File
  connect(m_menu_bar, &MenuBar::Open, this, &MainWindow::Open);
  connect(m_menu_bar, &MenuBar::Exit, this, &MainWindow::close);
  connect(m_menu_bar, &MenuBar::EjectDisc, this, &MainWindow::EjectDisc);
  connect(m_menu_bar, &MenuBar::ChangeDisc, this, &MainWindow::ChangeDisc);
  connect(m_menu_bar, &MenuBar::OpenUserFolder, this, &MainWindow::OpenUserFolder);

  // Emulation
  connect(m_menu_bar, &MenuBar::Pause, this, &MainWindow::Pause);
  connect(m_menu_bar, &MenuBar::Play, this, [this]() { Play(); });
  connect(m_menu_bar, &MenuBar::Stop, this, &MainWindow::RequestStop);
  connect(m_menu_bar, &MenuBar::Reset, this, &MainWindow::Reset);
  connect(m_menu_bar, &MenuBar::Fullscreen, this, &MainWindow::FullScreen);
  connect(m_menu_bar, &MenuBar::FrameAdvance, this, &MainWindow::FrameAdvance);
  connect(m_menu_bar, &MenuBar::Screenshot, this, &MainWindow::ScreenShot);
  connect(m_menu_bar, &MenuBar::StateLoad, this, &MainWindow::StateLoad);
  connect(m_menu_bar, &MenuBar::StateSave, this, &MainWindow::StateSave);
  connect(m_menu_bar, &MenuBar::StateLoadSlot, this, &MainWindow::StateLoadSlot);
  connect(m_menu_bar, &MenuBar::StateSaveSlot, this, &MainWindow::StateSaveSlot);
  connect(m_menu_bar, &MenuBar::StateLoadSlotAt, this, &MainWindow::StateLoadSlotAt);
  connect(m_menu_bar, &MenuBar::StateSaveSlotAt, this, &MainWindow::StateSaveSlotAt);
  connect(m_menu_bar, &MenuBar::StateLoadUndo, this, &MainWindow::StateLoadUndo);
  connect(m_menu_bar, &MenuBar::StateSaveUndo, this, &MainWindow::StateSaveUndo);
  connect(m_menu_bar, &MenuBar::StateSaveOldest, this, &MainWindow::StateSaveOldest);
  connect(m_menu_bar, &MenuBar::SetStateSlot, this, &MainWindow::SetStateSlot);

  // Options
  connect(m_menu_bar, &MenuBar::Configure, this, &MainWindow::ShowSettingsWindow);
  connect(m_menu_bar, &MenuBar::ConfigureGraphics, this, &MainWindow::ShowGraphicsWindow);
  connect(m_menu_bar, &MenuBar::ConfigureAudio, this, &MainWindow::ShowAudioWindow);
  connect(m_menu_bar, &MenuBar::ConfigureControllers, this, &MainWindow::ShowControllersWindow);
  connect(m_menu_bar, &MenuBar::ConfigureHotkeys, this, &MainWindow::ShowHotkeyDialog);
  connect(m_menu_bar, &MenuBar::ConfigureFreelook, this, &MainWindow::ShowFreeLookWindow);

  // Tools
  connect(m_menu_bar, &MenuBar::ShowMemcardManager, this, &MainWindow::ShowMemcardManager);
  connect(m_menu_bar, &MenuBar::ShowResourcePackManager, this,
          &MainWindow::ShowResourcePackManager);
  connect(m_menu_bar, &MenuBar::ShowCheatsManager, this, &MainWindow::ShowCheatsManager);
  connect(m_menu_bar, &MenuBar::BootGameCubeIPL, this, &MainWindow::OnBootGameCubeIPL);
  connect(m_menu_bar, &MenuBar::ImportNANDBackup, this, &MainWindow::OnImportNANDBackup);
  connect(m_menu_bar, &MenuBar::PerformOnlineUpdate, this, &MainWindow::PerformOnlineUpdate);
  connect(m_menu_bar, &MenuBar::BootWiiSystemMenu, this, &MainWindow::BootWiiSystemMenu);
  connect(m_menu_bar, &MenuBar::StartNetPlay, this, &MainWindow::ShowNetPlaySetupDialog);
  connect(m_menu_bar, &MenuBar::BrowseNetPlay, this, &MainWindow::ShowNetPlayBrowser);
  connect(m_menu_bar, &MenuBar::ShowFIFOPlayer, this, &MainWindow::ShowFIFOPlayer);
  connect(m_menu_bar, &MenuBar::ShowSkylanderPortal, this, &MainWindow::ShowSkylanderPortal);
  connect(m_menu_bar, &MenuBar::ShowInfinityBase, this, &MainWindow::ShowInfinityBase);
  connect(m_menu_bar, &MenuBar::ConnectWiiRemote, this, &MainWindow::OnConnectWiiRemote);

#ifdef USE_RETRO_ACHIEVEMENTS
  connect(m_menu_bar, &MenuBar::ShowAchievementsWindow, this, &MainWindow::ShowAchievementsWindow);
#endif  // USE_RETRO_ACHIEVEMENTS

  // Movie
  connect(m_menu_bar, &MenuBar::PlayRecording, this, &MainWindow::OnPlayRecording);
  connect(m_menu_bar, &MenuBar::StartRecording, this, &MainWindow::OnStartRecording);
  connect(m_menu_bar, &MenuBar::StopRecording, this, &MainWindow::OnStopRecording);
  connect(m_menu_bar, &MenuBar::ExportRecording, this, &MainWindow::OnExportRecording);
  connect(m_menu_bar, &MenuBar::ShowTASInput, this, &MainWindow::ShowTASInput);

  // View
  connect(m_menu_bar, &MenuBar::ShowList, m_game_list, &GameList::SetListView);
  connect(m_menu_bar, &MenuBar::ShowGrid, m_game_list, &GameList::SetGridView);
  connect(m_menu_bar, &MenuBar::PurgeGameListCache, m_game_list, &GameList::PurgeCache);
  connect(m_menu_bar, &MenuBar::ShowSearch, m_search_bar, &SearchBar::Show);

  connect(m_menu_bar, &MenuBar::ColumnVisibilityToggled, m_game_list,
          &GameList::OnColumnVisibilityToggled);

  connect(m_menu_bar, &MenuBar::GameListPlatformVisibilityToggled, m_game_list,
          &GameList::OnGameListVisibilityChanged);
  connect(m_menu_bar, &MenuBar::GameListRegionVisibilityToggled, m_game_list,
          &GameList::OnGameListVisibilityChanged);

  connect(m_menu_bar, &MenuBar::ShowAboutDialog, this, &MainWindow::ShowAboutDialog);

  connect(m_game_list, &GameList::SelectionChanged, m_menu_bar, &MenuBar::SelectionChanged);
  connect(this, &MainWindow::ReadOnlyModeChanged, m_menu_bar, &MenuBar::ReadOnlyModeChanged);
  connect(this, &MainWindow::RecordingStatusChanged, m_menu_bar, &MenuBar::RecordingStatusChanged);

  // Symbols
  connect(m_menu_bar, &MenuBar::NotifySymbolsUpdated, [this] {
    m_code_widget->UpdateSymbols();
    m_code_widget->Update();
  });
}

void MainWindow::ConnectHotkeys()
{
  connect(m_hotkey_scheduler, &HotkeyScheduler::Open, this, &MainWindow::Open);
  connect(m_hotkey_scheduler, &HotkeyScheduler::ChangeDisc, this, &MainWindow::ChangeDisc);
  connect(m_hotkey_scheduler, &HotkeyScheduler::EjectDisc, this, &MainWindow::EjectDisc);
  connect(m_hotkey_scheduler, &HotkeyScheduler::ExitHotkey, this, &MainWindow::close);
  connect(m_hotkey_scheduler, &HotkeyScheduler::UnlockCursor, this, &MainWindow::UnlockCursor);
  connect(m_hotkey_scheduler, &HotkeyScheduler::TogglePauseHotkey, this, &MainWindow::TogglePause);
  connect(m_hotkey_scheduler, &HotkeyScheduler::ActivateChat, this, &MainWindow::OnActivateChat);
  connect(m_hotkey_scheduler, &HotkeyScheduler::RequestGolfControl, this,
          &MainWindow::OnRequestGolfControl);
  connect(m_hotkey_scheduler, &HotkeyScheduler::RefreshGameListHotkey, this,
          &MainWindow::RefreshGameList);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StopHotkey, this, &MainWindow::RequestStop);
  connect(m_hotkey_scheduler, &HotkeyScheduler::ResetHotkey, this, &MainWindow::Reset);
  connect(m_hotkey_scheduler, &HotkeyScheduler::ScreenShotHotkey, this, &MainWindow::ScreenShot);
  connect(m_hotkey_scheduler, &HotkeyScheduler::FullScreenHotkey, this, &MainWindow::FullScreen);

  connect(m_hotkey_scheduler, &HotkeyScheduler::StateLoadSlot, this, &MainWindow::StateLoadSlotAt);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StateSaveSlot, this, &MainWindow::StateSaveSlotAt);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StateLoadLastSaved, this,
          &MainWindow::StateLoadLastSavedAt);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StateLoadUndo, this, &MainWindow::StateLoadUndo);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StateSaveUndo, this, &MainWindow::StateSaveUndo);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StateSaveOldest, this,
          &MainWindow::StateSaveOldest);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StateSaveFile, this, &MainWindow::StateSave);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StateLoadFile, this, &MainWindow::StateLoad);

  connect(m_hotkey_scheduler, &HotkeyScheduler::StateLoadSlotHotkey, this,
          &MainWindow::StateLoadSlot);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StateSaveSlotHotkey, this,
          &MainWindow::StateSaveSlot);
  connect(m_hotkey_scheduler, &HotkeyScheduler::SetStateSlotHotkey, this,
          &MainWindow::SetStateSlot);
  connect(m_hotkey_scheduler, &HotkeyScheduler::IncrementSelectedStateSlotHotkey, this,
          &MainWindow::IncrementSelectedStateSlot);
  connect(m_hotkey_scheduler, &HotkeyScheduler::DecrementSelectedStateSlotHotkey, this,
          &MainWindow::DecrementSelectedStateSlot);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StartRecording, this,
          &MainWindow::OnStartRecording);
  connect(m_hotkey_scheduler, &HotkeyScheduler::PlayRecording, this, &MainWindow::OnPlayRecording);
  connect(m_hotkey_scheduler, &HotkeyScheduler::ExportRecording, this,
          &MainWindow::OnExportRecording);
  connect(m_hotkey_scheduler, &HotkeyScheduler::ConnectWiiRemote, this,
          &MainWindow::OnConnectWiiRemote);
  connect(m_hotkey_scheduler, &HotkeyScheduler::ToggleReadOnlyMode, [this] {
    bool read_only = !Movie::IsReadOnly();
    Movie::SetReadOnly(read_only);
    emit ReadOnlyModeChanged(read_only);
  });

  connect(m_hotkey_scheduler, &HotkeyScheduler::Step, m_code_widget, &CodeWidget::Step);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StepOver, m_code_widget, &CodeWidget::StepOver);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StepOut, m_code_widget, &CodeWidget::StepOut);
  connect(m_hotkey_scheduler, &HotkeyScheduler::Skip, m_code_widget, &CodeWidget::Skip);

  connect(m_hotkey_scheduler, &HotkeyScheduler::ShowPC, m_code_widget, &CodeWidget::ShowPC);
  connect(m_hotkey_scheduler, &HotkeyScheduler::SetPC, m_code_widget, &CodeWidget::SetPC);

  connect(m_hotkey_scheduler, &HotkeyScheduler::ToggleBreakpoint, m_code_widget,
          &CodeWidget::ToggleBreakpoint);
  connect(m_hotkey_scheduler, &HotkeyScheduler::AddBreakpoint, m_code_widget,
          &CodeWidget::AddBreakpoint);

  connect(m_hotkey_scheduler, &HotkeyScheduler::SkylandersPortalHotkey, this,
          &MainWindow::ShowSkylanderPortal);
  connect(m_hotkey_scheduler, &HotkeyScheduler::InfinityBaseHotkey, this,
          &MainWindow::ShowInfinityBase);
}

void MainWindow::ConnectToolBar()
{
  addToolBar(m_tool_bar);

  connect(m_tool_bar, &ToolBar::OpenPressed, this, &MainWindow::Open);
  connect(m_tool_bar, &ToolBar::RefreshPressed, this, &MainWindow::RefreshGameList);

  connect(m_tool_bar, &ToolBar::PlayPressed, this, [this]() { Play(); });
  connect(m_tool_bar, &ToolBar::PausePressed, this, &MainWindow::Pause);
  connect(m_tool_bar, &ToolBar::StopPressed, this, &MainWindow::RequestStop);
  connect(m_tool_bar, &ToolBar::FullScreenPressed, this, &MainWindow::FullScreen);
  connect(m_tool_bar, &ToolBar::ScreenShotPressed, this, &MainWindow::ScreenShot);
  connect(m_tool_bar, &ToolBar::SettingsPressed, this, &MainWindow::ShowSettingsWindow);
  connect(m_tool_bar, &ToolBar::ControllersPressed, this, &MainWindow::ShowControllersWindow);
  connect(m_tool_bar, &ToolBar::GraphicsPressed, this, &MainWindow::ShowGraphicsWindow);

  connect(m_tool_bar, &ToolBar::StepPressed, m_code_widget, &CodeWidget::Step);
  connect(m_tool_bar, &ToolBar::StepOverPressed, m_code_widget, &CodeWidget::StepOver);
  connect(m_tool_bar, &ToolBar::StepOutPressed, m_code_widget, &CodeWidget::StepOut);
  connect(m_tool_bar, &ToolBar::SkipPressed, m_code_widget, &CodeWidget::Skip);
  connect(m_tool_bar, &ToolBar::ShowPCPressed, m_code_widget, &CodeWidget::ShowPC);
  connect(m_tool_bar, &ToolBar::SetPCPressed, m_code_widget, &CodeWidget::SetPC);
}

void MainWindow::ConnectGameList()
{
  connect(m_game_list, &GameList::GameSelected, this, [this]() { Play(); });
  connect(m_game_list, &GameList::NetPlayHost, this, &MainWindow::NetPlayHost);
  connect(m_game_list, &GameList::OnStartWithRiivolution, this,
          &MainWindow::ShowRiivolutionBootWidget);

  connect(m_game_list, &GameList::OpenGeneralSettings, this, &MainWindow::ShowGeneralWindow);
  connect(m_game_list, &GameList::OpenGraphicsSettings, this, &MainWindow::ShowGraphicsWindow);
}

void MainWindow::ConnectRenderWidget()
{
  m_rendering_to_main = false;
  m_render_widget->hide();
  connect(m_render_widget, &RenderWidget::Closed, this, &MainWindow::ForceStop);
  connect(m_render_widget, &RenderWidget::FocusChanged, this, [this](bool focus) {
    if (m_render_widget->isFullScreen())
      SetFullScreenResolution(focus);
  });
}

void MainWindow::ConnectHost()
{
  connect(Host::GetInstance(), &Host::RequestStop, this, &MainWindow::RequestStop);
}

void MainWindow::ConnectStack()
{
  auto* widget = new QWidget;
  auto* layout = new QVBoxLayout;
  widget->setLayout(layout);

  layout->addWidget(m_game_list);
  layout->addWidget(m_search_bar);
  layout->setContentsMargins(0, 0, 0, 0);

  connect(m_search_bar, &SearchBar::Search, m_game_list, &GameList::SetSearchTerm);

  m_stack->addWidget(widget);

  setCentralWidget(m_stack);

  setDockOptions(DockOption::AllowNestedDocks | DockOption::AllowTabbedDocks);
  setTabPosition(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea, QTabWidget::North);
  addDockWidget(Qt::LeftDockWidgetArea, m_log_widget);
  addDockWidget(Qt::LeftDockWidgetArea, m_log_config_widget);
  addDockWidget(Qt::LeftDockWidgetArea, m_code_widget);
  addDockWidget(Qt::LeftDockWidgetArea, m_register_widget);
  addDockWidget(Qt::LeftDockWidgetArea, m_thread_widget);
  addDockWidget(Qt::LeftDockWidgetArea, m_watch_widget);
  addDockWidget(Qt::LeftDockWidgetArea, m_breakpoint_widget);
  addDockWidget(Qt::LeftDockWidgetArea, m_memory_widget);
  addDockWidget(Qt::LeftDockWidgetArea, m_network_widget);
  addDockWidget(Qt::LeftDockWidgetArea, m_jit_widget);

  tabifyDockWidget(m_log_widget, m_log_config_widget);
  tabifyDockWidget(m_log_widget, m_code_widget);
  tabifyDockWidget(m_log_widget, m_register_widget);
  tabifyDockWidget(m_log_widget, m_thread_widget);
  tabifyDockWidget(m_log_widget, m_watch_widget);
  tabifyDockWidget(m_log_widget, m_breakpoint_widget);
  tabifyDockWidget(m_log_widget, m_memory_widget);
  tabifyDockWidget(m_log_widget, m_network_widget);
  tabifyDockWidget(m_log_widget, m_jit_widget);
}

void MainWindow::RefreshGameList()
{
  Settings::Instance().ReloadTitleDB();
  Settings::Instance().RefreshGameList();
}

QStringList MainWindow::PromptFileNames()
{
  auto& settings = Settings::Instance().GetQSettings();
  QStringList paths = DolphinFileDialog::getOpenFileNames(
      this, tr("Select a File"),
      settings.value(QStringLiteral("mainwindow/lastdir"), QString{}).toString(),
      QStringLiteral("%1 (*.elf *.dol *.gcm *.iso *.tgc *.wbfs *.ciso *.gcz *.wia *.rvz "
                     "hif_000000.nfs *.wad *.dff *.m3u *.json);;%2 (*)")
          .arg(tr("All GC/Wii files"))
          .arg(tr("All Files")));

  if (!paths.isEmpty())
  {
    settings.setValue(QStringLiteral("mainwindow/lastdir"),
                      QFileInfo(paths.front()).absoluteDir().absolutePath());
  }

  return paths;
}

void MainWindow::ChangeDisc()
{
  std::vector<std::string> paths = StringListToStdVector(PromptFileNames());

  if (!paths.empty())
    Core::RunAsCPUThread(
        [&paths] { Core::System::GetInstance().GetDVDInterface().ChangeDisc(paths); });
}

void MainWindow::EjectDisc()
{
  Core::RunAsCPUThread(
      [] { Core::System::GetInstance().GetDVDInterface().EjectDisc(DVD::EjectCause::User); });
}

void MainWindow::OpenUserFolder()
{
  std::string path = File::GetUserPath(D_USER_IDX);

  QUrl url = QUrl::fromLocalFile(QString::fromStdString(path));
  QDesktopServices::openUrl(url);
}

void MainWindow::Open()
{
  QStringList files = PromptFileNames();
  if (!files.isEmpty())
    StartGame(StringListToStdVector(files));
}

void MainWindow::Play(const std::optional<std::string>& savestate_path)
{
  // If we're in a paused game, start it up again.
  // Otherwise, play the selected game, if there is one.
  // Otherwise, play the default game.
  // Otherwise, play the last played game, if there is one.
  // Otherwise, prompt for a new game.
  if (Core::GetState() == Core::State::Paused)
  {
    Core::SetState(Core::State::Running);
  }
  else
  {
    std::shared_ptr<const UICommon::GameFile> selection = m_game_list->GetSelectedGame();
    if (selection)
    {
      StartGame(selection->GetFilePath(), ScanForSecondDisc::Yes,
                std::make_unique<BootSessionData>(savestate_path, DeleteSavestateAfterBoot::No));
    }
    else
    {
      const QString default_path = QString::fromStdString(Config::Get(Config::MAIN_DEFAULT_ISO));
      if (!default_path.isEmpty() && QFile::exists(default_path))
      {
        StartGame(default_path, ScanForSecondDisc::Yes,
                  std::make_unique<BootSessionData>(savestate_path, DeleteSavestateAfterBoot::No));
      }
      else
      {
        Open();
      }
    }
  }
}

void MainWindow::Pause()
{
  Core::SetState(Core::State::Paused);
}

void MainWindow::TogglePause()
{
  if (Core::GetState() == Core::State::Paused)
  {
    Play();
  }
  else
  {
    Pause();
  }
}

void MainWindow::OnStopComplete()
{
  m_stop_requested = false;
  HideRenderWidget(!m_exit_requested, m_exit_requested);
#ifdef USE_DISCORD_PRESENCE
  if (!m_netplay_dialog->isVisible())
    Discord::UpdateDiscordPresence();
#endif

  SetFullScreenResolution(false);

  if (m_exit_requested || Settings::Instance().IsBatchModeEnabled())
    QGuiApplication::exit(0);

  // If the current emulation prevented the booting of another, do that now
  if (m_pending_boot != nullptr)
  {
    StartGame(std::move(m_pending_boot));
    m_pending_boot.reset();
  }
}

bool MainWindow::RequestStop()
{
  if (!Core::IsRunning())
  {
    Core::QueueHostJob([this] { OnStopComplete(); }, true);
    return true;
  }

  const bool rendered_widget_was_active =
      m_render_widget->isActiveWindow() && !m_render_widget->isFullScreen();
  QWidget* confirm_parent = (!m_rendering_to_main && rendered_widget_was_active) ?
                                m_render_widget :
                                static_cast<QWidget*>(this);
  const bool was_cursor_locked = m_render_widget->IsCursorLocked();

  if (!m_render_widget->isFullScreen())
    m_render_widget_geometry = m_render_widget->saveGeometry();
  else
    FullScreen();

  if (Config::Get(Config::MAIN_CONFIRM_ON_STOP))
  {
    if (std::exchange(m_stop_confirm_showing, true))
      return true;

    Common::ScopeGuard confirm_lock([this] { m_stop_confirm_showing = false; });

    const Core::State state = Core::GetState();

    // Only pause the game, if NetPlay is not running
    bool pause = !Settings::Instance().GetNetPlayClient();

    if (pause)
      Core::SetState(Core::State::Paused);

    if (rendered_widget_was_active)
    {
      // We have to do this before creating the message box, otherwise we might receive the window
      // activation event before we know we need to lock the cursor again.
      m_render_widget->SetCursorLockedOnNextActivation(was_cursor_locked);
    }

    // This is to avoid any "race conditions" between the "Window Activate" message and the
    // message box returning, which could break cursor locking depending on the order
    m_render_widget->SetWaitingForMessageBox(true);
    auto confirm = ModalMessageBox::question(
        confirm_parent, tr("Confirm"),
        m_stop_requested ? tr("A shutdown is already in progress. Unsaved data "
                              "may be lost if you stop the current emulation "
                              "before it completes. Force stop?") :
                           tr("Do you want to stop the current emulation?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::NoButton, Qt::ApplicationModal);

    // If a user confirmed stopping the emulation, we do not capture the cursor again,
    // even if the render widget will stay alive for a while.
    // If a used rejected stopping the emulation, we instead capture the cursor again,
    // and let them continue playing as if nothing had happened
    // (assuming cursor locking is on).
    if (confirm != QMessageBox::Yes)
    {
      m_render_widget->SetWaitingForMessageBox(false);

      if (pause)
        Core::SetState(state);

      return false;
    }
    else
    {
      m_render_widget->SetCursorLockedOnNextActivation(false);
      // This needs to be after SetCursorLockedOnNextActivation(false) as it depends on it
      m_render_widget->SetWaitingForMessageBox(false);
    }
  }

  OnStopRecording();
  // TODO: Add Debugger shutdown

  if (!m_stop_requested && UICommon::TriggerSTMPowerEvent())
  {
    m_stop_requested = true;

    // Unpause because gracefully shutting down needs the game to actually request a shutdown.
    // TODO: Do not unpause in debug mode to allow debugging until the complete shutdown.
    if (Core::GetState() == Core::State::Paused)
      Core::SetState(Core::State::Running);

    // Tell NetPlay about the power event
    if (NetPlay::IsNetPlayRunning())
      NetPlay::SendPowerButtonEvent();

    return true;
  }

  ForceStop();
#ifdef Q_OS_WIN
  // Allow windows to idle or turn off display again
  SetThreadExecutionState(ES_CONTINUOUS);
#endif
  return true;
}

void MainWindow::ForceStop()
{
  Core::Stop();
}

void MainWindow::Reset()
{
  if (Movie::IsRecordingInput())
    Movie::SetReset(true);
  auto& system = Core::System::GetInstance();
  system.GetProcessorInterface().ResetButton_Tap();
}

void MainWindow::FrameAdvance()
{
  Core::DoFrameStep();
}

void MainWindow::FullScreen()
{
  // If the render widget is fullscreen we want to reset it to whatever is in
  // settings. If it's set to be fullscreen then it just remakes the window,
  // which probably isn't ideal.
  bool was_fullscreen = m_render_widget->isFullScreen();

  if (!was_fullscreen)
    m_render_widget_geometry = m_render_widget->saveGeometry();

  HideRenderWidget(false);
  SetFullScreenResolution(!was_fullscreen);

  if (was_fullscreen)
  {
    ShowRenderWidget();
  }
  else
  {
    m_render_widget->showFullScreen();
  }
}

void MainWindow::UnlockCursor()
{
  if (!m_render_widget->isFullScreen())
    m_render_widget->SetCursorLocked(false);
}

void MainWindow::ScreenShot()
{
  Core::SaveScreenShot();
}

void MainWindow::ScanForSecondDiscAndStartGame(const UICommon::GameFile& game,
                                               std::unique_ptr<BootSessionData> boot_session_data)
{
  auto second_game = m_game_list->FindSecondDisc(game);

  std::vector<std::string> paths = {game.GetFilePath()};
  if (second_game != nullptr)
    paths.push_back(second_game->GetFilePath());

  StartGame(paths, std::move(boot_session_data));
}

void MainWindow::StartGame(const QString& path, ScanForSecondDisc scan,
                           std::unique_ptr<BootSessionData> boot_session_data)
{
  StartGame(path.toStdString(), scan, std::move(boot_session_data));
}

void MainWindow::StartGame(const std::string& path, ScanForSecondDisc scan,
                           std::unique_ptr<BootSessionData> boot_session_data)
{
  if (scan == ScanForSecondDisc::Yes)
  {
    std::shared_ptr<const UICommon::GameFile> game = m_game_list->FindGame(path);
    if (game != nullptr)
    {
      ScanForSecondDiscAndStartGame(*game, std::move(boot_session_data));
      return;
    }
  }

  StartGame(BootParameters::GenerateFromFile(
      path, boot_session_data ? std::move(*boot_session_data) : BootSessionData()));
}

void MainWindow::StartGame(const std::vector<std::string>& paths,
                           std::unique_ptr<BootSessionData> boot_session_data)
{
  StartGame(BootParameters::GenerateFromFile(
      paths, boot_session_data ? std::move(*boot_session_data) : BootSessionData()));
}

void MainWindow::StartGame(std::unique_ptr<BootParameters>&& parameters)
{
  if (parameters && std::holds_alternative<BootParameters::Disc>(parameters->parameters))
  {
    if (std::get<BootParameters::Disc>(parameters->parameters).volume->IsNKit())
    {
      if (!NKitWarningDialog::ShowUnlessDisabled())
        return;
    }
  }

  // If we're running, only start a new game once we've stopped the last.
  if (Core::GetState() != Core::State::Uninitialized)
  {
    if (!RequestStop())
      return;

    // As long as the shutdown isn't complete, we can't boot, so let's boot later
    m_pending_boot = std::move(parameters);
    return;
  }

  // We need the render widget before booting.
  ShowRenderWidget();

  // Boot up, show an error if it fails to load the game.
  if (!BootManager::BootCore(std::move(parameters),
                             ::GetWindowSystemInfo(m_render_widget->windowHandle())))
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Failed to init core"), QMessageBox::Ok);
    HideRenderWidget();
    return;
  }

#ifdef USE_DISCORD_PRESENCE
  if (!NetPlay::IsNetPlayRunning())
    Discord::UpdateDiscordPresence();
#endif

  if (Config::Get(Config::MAIN_FULLSCREEN))
    m_fullscreen_requested = true;
}

void MainWindow::SetFullScreenResolution(bool fullscreen)
{
  if (Config::Get(Config::MAIN_FULLSCREEN_DISPLAY_RES) == "Auto")
    return;
#ifdef _WIN32

  if (!fullscreen)
  {
    ChangeDisplaySettings(nullptr, CDS_FULLSCREEN);
    return;
  }

  DEVMODE screen_settings;
  memset(&screen_settings, 0, sizeof(screen_settings));
  screen_settings.dmSize = sizeof(screen_settings);
  sscanf(Config::Get(Config::MAIN_FULLSCREEN_DISPLAY_RES).c_str(), "%dx%d",
         &screen_settings.dmPelsWidth, &screen_settings.dmPelsHeight);
  screen_settings.dmBitsPerPel = 32;
  screen_settings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

  // Try To Set Selected Mode And Get Results.  NOTE: CDS_FULLSCREEN Gets Rid Of Start Bar.
  ChangeDisplaySettings(&screen_settings, CDS_FULLSCREEN);
#elif defined(HAVE_XRANDR) && HAVE_XRANDR
  if (m_xrr_config)
    m_xrr_config->ToggleDisplayMode(fullscreen);
#endif
}

void MainWindow::ShowRenderWidget()
{
  SetFullScreenResolution(false);
  Host::GetInstance()->SetRenderFullscreen(false);

  if (Config::Get(Config::MAIN_RENDER_TO_MAIN))
  {
    // If we're rendering to main, add it to the stack and update our title when necessary.
    m_rendering_to_main = true;

    m_stack->setCurrentIndex(m_stack->addWidget(m_render_widget));
    connect(Host::GetInstance(), &Host::RequestTitle, this, &MainWindow::setWindowTitle);
    m_stack->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_stack->repaint();

    Host::GetInstance()->SetRenderFocus(isActiveWindow());
  }
  else
  {
    // Otherwise, just show it.
    m_rendering_to_main = false;

    m_render_widget->showNormal();
    m_render_widget->restoreGeometry(m_render_widget_geometry);
  }
}

void MainWindow::HideRenderWidget(bool reinit, bool is_exit)
{
  if (m_rendering_to_main)
  {
    // Remove the widget from the stack and reparent it to nullptr, so that it can draw
    // itself in a new window if it wants. Disconnect the title updates.
    m_stack->removeWidget(m_render_widget);
    m_render_widget->setParent(nullptr);
    m_rendering_to_main = false;
    m_stack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    disconnect(Host::GetInstance(), &Host::RequestTitle, this, &MainWindow::setWindowTitle);
    setWindowTitle(QString::fromStdString(Common::GetScmRevStr()));
  }

  // The following code works around a driver bug that would lead to Dolphin crashing when changing
  // graphics backends (e.g. OpenGL to Vulkan). To avoid this the render widget is (safely)
  // recreated
  if (reinit)
  {
    m_render_widget->hide();
    disconnect(m_render_widget, &RenderWidget::Closed, this, &MainWindow::ForceStop);

    m_render_widget->removeEventFilter(this);
    m_render_widget->deleteLater();

    m_render_widget = new RenderWidget;

    m_render_widget->installEventFilter(this);
    connect(m_render_widget, &RenderWidget::Closed, this, &MainWindow::ForceStop);
    connect(m_render_widget, &RenderWidget::FocusChanged, this, [this](bool focus) {
      if (m_render_widget->isFullScreen())
        SetFullScreenResolution(focus);
    });

    // The controller interface will still be registered to the old render widget, if the core
    // has booted. Therefore, we should re-bind it to the main window for now. When the core
    // is next started, it will be swapped back to the new render widget.
    g_controller_interface.ChangeWindow(::GetWindowSystemInfo(windowHandle()).render_window,
                                        is_exit ? ControllerInterface::WindowChangeReason::Exit :
                                                  ControllerInterface::WindowChangeReason::Other);
  }
}

void MainWindow::ShowControllersWindow()
{
  if (!m_controllers_window)
  {
    m_controllers_window = new ControllersWindow(this);
    InstallHotkeyFilter(m_controllers_window);
  }

  SetQWidgetWindowDecorations(m_controllers_window);
  m_controllers_window->show();
  m_controllers_window->raise();
  m_controllers_window->activateWindow();
}

void MainWindow::ShowFreeLookWindow()
{
  if (!m_freelook_window)
  {
    m_freelook_window = new FreeLookWindow(this);
    InstallHotkeyFilter(m_freelook_window);

#ifdef USE_RETRO_ACHIEVEMENTS
    connect(m_freelook_window, &FreeLookWindow::OpenAchievementSettings, this,
            &MainWindow::ShowAchievementSettings);
#endif  // USE_RETRO_ACHIEVEMENTS
  }

  SetQWidgetWindowDecorations(m_freelook_window);
  m_freelook_window->show();
  m_freelook_window->raise();
  m_freelook_window->activateWindow();
}

void MainWindow::ShowSettingsWindow()
{
  if (!m_settings_window)
  {
    m_settings_window = new SettingsWindow(this);
    InstallHotkeyFilter(m_settings_window);
  }

  SetQWidgetWindowDecorations(m_settings_window);
  m_settings_window->show();
  m_settings_window->raise();
  m_settings_window->activateWindow();
}

void MainWindow::ShowAudioWindow()
{
  ShowSettingsWindow();
  m_settings_window->SelectAudioPane();
}

void MainWindow::ShowGeneralWindow()
{
  ShowSettingsWindow();
  m_settings_window->SelectGeneralPane();
}

void MainWindow::ShowAboutDialog()
{
  AboutDialog about{this};
  SetQWidgetWindowDecorations(&about);
  about.exec();
}

void MainWindow::ShowHotkeyDialog()
{
  if (!m_hotkey_window)
  {
    m_hotkey_window = new MappingWindow(this, MappingWindow::Type::MAPPING_HOTKEYS, 0);
    InstallHotkeyFilter(m_hotkey_window);
  }

  SetQWidgetWindowDecorations(m_hotkey_window);
  m_hotkey_window->show();
  m_hotkey_window->raise();
  m_hotkey_window->activateWindow();
}

void MainWindow::ShowGraphicsWindow()
{
  if (!m_graphics_window)
  {
#ifdef HAVE_XRANDR
    if (GetWindowSystemType() == WindowSystemType::X11)
    {
      m_xrr_config = std::make_unique<X11Utils::XRRConfiguration>(
          static_cast<Display*>(QGuiApplication::platformNativeInterface()->nativeResourceForWindow(
              "display", windowHandle())),
          winId());
    }
#endif
    m_graphics_window = new GraphicsWindow(this);
    InstallHotkeyFilter(m_graphics_window);
  }

  SetQWidgetWindowDecorations(m_graphics_window);
  m_graphics_window->show();
  m_graphics_window->raise();
  m_graphics_window->activateWindow();
}

void MainWindow::ShowNetPlaySetupDialog()
{
  SetQWidgetWindowDecorations(m_netplay_setup_dialog);
  m_netplay_setup_dialog->show();
  m_netplay_setup_dialog->raise();
  m_netplay_setup_dialog->activateWindow();
}

void MainWindow::ShowNetPlayBrowser()
{
  auto* browser = new NetPlayBrowser(this);
  browser->setAttribute(Qt::WA_DeleteOnClose, true);
  connect(browser, &NetPlayBrowser::Join, this, &MainWindow::NetPlayJoin);
  SetQWidgetWindowDecorations(browser);
  browser->exec();
}

void MainWindow::ShowFIFOPlayer()
{
  if (!m_fifo_window)
  {
    m_fifo_window = new FIFOPlayerWindow;
    connect(m_fifo_window, &FIFOPlayerWindow::LoadFIFORequested, this,
            [this](const QString& path) { StartGame(path, ScanForSecondDisc::No); });
  }

  SetQWidgetWindowDecorations(m_fifo_window);
  m_fifo_window->show();
  m_fifo_window->raise();
  m_fifo_window->activateWindow();
}

void MainWindow::ShowSkylanderPortal()
{
  if (!m_skylander_window)
  {
    m_skylander_window = new SkylanderPortalWindow();
  }

  SetQWidgetWindowDecorations(m_skylander_window);
  m_skylander_window->show();
  m_skylander_window->raise();
  m_skylander_window->activateWindow();
}

void MainWindow::ShowInfinityBase()
{
  if (!m_infinity_window)
  {
    m_infinity_window = new InfinityBaseWindow();
  }

  SetQWidgetWindowDecorations(m_infinity_window);
  m_infinity_window->show();
  m_infinity_window->raise();
  m_infinity_window->activateWindow();
}

void MainWindow::StateLoad()
{
  QString path =
      DolphinFileDialog::getOpenFileName(this, tr("Select a File"), QDir::currentPath(),
                                         tr("All Save States (*.sav *.s##);; All Files (*)"));
  if (!path.isEmpty())
    State::LoadAs(path.toStdString());
}

void MainWindow::StateSave()
{
  QString path =
      DolphinFileDialog::getSaveFileName(this, tr("Select a File"), QDir::currentPath(),
                                         tr("All Save States (*.sav *.s##);; All Files (*)"));
  if (!path.isEmpty())
    State::SaveAs(path.toStdString());
}

void MainWindow::StateLoadSlot()
{
  State::Load(m_state_slot);
}

void MainWindow::StateSaveSlot()
{
  State::Save(m_state_slot);
}

void MainWindow::StateLoadSlotAt(int slot)
{
  State::Load(slot);
}

void MainWindow::StateLoadLastSavedAt(int slot)
{
  State::LoadLastSaved(slot);
}

void MainWindow::StateSaveSlotAt(int slot)
{
  State::Save(slot);
}

void MainWindow::StateLoadUndo()
{
  State::UndoLoadState();
}

void MainWindow::StateSaveUndo()
{
  State::UndoSaveState();
}

void MainWindow::StateSaveOldest()
{
  State::SaveFirstSaved();
}

void MainWindow::SetStateSlot(int slot)
{
  Settings::Instance().SetStateSlot(slot);
  m_state_slot = slot;

  Core::DisplayMessage(fmt::format("Selected slot {} - {}", m_state_slot,
                                   State::GetInfoStringOfSlot(m_state_slot, false)),
                       2500);
}

void MainWindow::IncrementSelectedStateSlot()
{
  u32 state_slot = m_state_slot + 1;
  if (state_slot > State::NUM_STATES)
    state_slot = 1;
  m_menu_bar->SetStateSlot(state_slot);
}

void MainWindow::DecrementSelectedStateSlot()
{
  u32 state_slot = m_state_slot - 1;
  if (state_slot < 1)
    state_slot = State::NUM_STATES;
  m_menu_bar->SetStateSlot(state_slot);
}

void MainWindow::PerformOnlineUpdate(const std::string& region)
{
  WiiUpdate::PerformOnlineUpdate(region, this);
  // Since the update may have installed a newer system menu, trigger a refresh.
  Settings::Instance().NANDRefresh();
}

void MainWindow::BootWiiSystemMenu()
{
  StartGame(std::make_unique<BootParameters>(BootParameters::NANDTitle{Titles::SYSTEM_MENU}));
}

void MainWindow::NetPlayInit()
{
  const auto& game_list_model = m_game_list->GetGameListModel();
  m_netplay_setup_dialog = new NetPlaySetupDialog(game_list_model, this);
  m_netplay_dialog = new NetPlayDialog(
      game_list_model,
      [this](const std::string& path, std::unique_ptr<BootSessionData> boot_session_data) {
        StartGame(path, ScanForSecondDisc::Yes, std::move(boot_session_data));
      });
#ifdef USE_DISCORD_PRESENCE
  m_netplay_discord = new DiscordHandler(this);
#endif

  connect(m_netplay_dialog, &NetPlayDialog::Stop, this, &MainWindow::ForceStop);
  connect(m_netplay_dialog, &NetPlayDialog::rejected, this, &MainWindow::NetPlayQuit);
  connect(m_netplay_setup_dialog, &NetPlaySetupDialog::Join, this, &MainWindow::NetPlayJoin);
  connect(m_netplay_setup_dialog, &NetPlaySetupDialog::Host, this, &MainWindow::NetPlayHost);
#ifdef USE_DISCORD_PRESENCE
  connect(m_netplay_discord, &DiscordHandler::Join, this, &MainWindow::NetPlayJoin);

  Discord::InitNetPlayFunctionality(*m_netplay_discord);
  m_netplay_discord->Start();
#endif
  connect(&Settings::Instance(), &Settings::ConfigChanged, this,
          &MainWindow::UpdateScreenSaverInhibition);
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &MainWindow::UpdateScreenSaverInhibition);
}

bool MainWindow::NetPlayJoin()
{
  if (Core::IsRunning())
  {
    ModalMessageBox::critical(nullptr, tr("Error"),
                              tr("Can't start a NetPlay Session while a game is still running!"));
    return false;
  }

  if (m_netplay_dialog->isVisible())
  {
    ModalMessageBox::critical(nullptr, tr("Error"),
                              tr("A NetPlay Session is already in progress!"));
    return false;
  }

  auto server = Settings::Instance().GetNetPlayServer();

  // Settings
  const std::string traversal_choice = Config::Get(Config::NETPLAY_TRAVERSAL_CHOICE);
  const bool is_traversal = traversal_choice == "traversal";

  std::string host_ip;
  u16 host_port;
  if (server)
  {
    host_ip = "127.0.0.1";
    host_port = server->GetPort();
  }
  else
  {
    host_ip = is_traversal ? Config::Get(Config::NETPLAY_HOST_CODE) :
                             Config::Get(Config::NETPLAY_ADDRESS);
    host_port = Config::Get(Config::NETPLAY_CONNECT_PORT);
  }

  const std::string traversal_host = Config::Get(Config::NETPLAY_TRAVERSAL_SERVER);
  const u16 traversal_port = Config::Get(Config::NETPLAY_TRAVERSAL_PORT);
  const std::string nickname = Config::Get(Config::NETPLAY_NICKNAME);
  const std::string network_mode = Config::Get(Config::NETPLAY_NETWORK_MODE);
  const bool host_input_authority = network_mode == "hostinputauthority" || network_mode == "golf";

  if (server)
  {
    server->SetHostInputAuthority(host_input_authority);
    server->AdjustPadBufferSize(Config::Get(Config::NETPLAY_BUFFER_SIZE));
  }

  // Create Client
  const bool is_hosting_netplay = server != nullptr;
  Settings::Instance().ResetNetPlayClient(new NetPlay::NetPlayClient(
      host_ip, host_port, m_netplay_dialog, nickname,
      NetPlay::NetTraversalConfig{is_hosting_netplay ? false : is_traversal, traversal_host,
                                  traversal_port}));

  if (!Settings::Instance().GetNetPlayClient()->IsConnected())
  {
    NetPlayQuit();
    return false;
  }

  m_netplay_setup_dialog->close();
  m_netplay_dialog->show(nickname, is_traversal);

  return true;
}

bool MainWindow::NetPlayHost(const UICommon::GameFile& game)
{
  if (Core::IsRunning())
  {
    ModalMessageBox::critical(nullptr, tr("Error"),
                              tr("Can't start a NetPlay Session while a game is still running!"));
    return false;
  }

  if (m_netplay_dialog->isVisible())
  {
    ModalMessageBox::critical(nullptr, tr("Error"),
                              tr("A NetPlay Session is already in progress!"));
    return false;
  }

  // Settings
  u16 host_port = Config::Get(Config::NETPLAY_HOST_PORT);
  const std::string traversal_choice = Config::Get(Config::NETPLAY_TRAVERSAL_CHOICE);
  const bool is_traversal = traversal_choice == "traversal";
  const bool use_upnp = Config::Get(Config::NETPLAY_USE_UPNP);

  const std::string traversal_host = Config::Get(Config::NETPLAY_TRAVERSAL_SERVER);
  const u16 traversal_port = Config::Get(Config::NETPLAY_TRAVERSAL_PORT);
  const u16 traversal_port_alt = Config::Get(Config::NETPLAY_TRAVERSAL_PORT_ALT);

  if (is_traversal)
    host_port = Config::Get(Config::NETPLAY_LISTEN_PORT);

  // Create Server
  Settings::Instance().ResetNetPlayServer(
      new NetPlay::NetPlayServer(host_port, use_upnp, m_netplay_dialog,
                                 NetPlay::NetTraversalConfig{is_traversal, traversal_host,
                                                             traversal_port, traversal_port_alt}));

  if (!Settings::Instance().GetNetPlayServer()->is_connected)
  {
    ModalMessageBox::critical(
        nullptr, tr("Failed to open server"),
        tr("Failed to listen on port %1. Is another instance of the NetPlay server running?")
            .arg(host_port));
    NetPlayQuit();
    return false;
  }

  Settings::Instance().GetNetPlayServer()->ChangeGame(game.GetSyncIdentifier(),
                                                      m_game_list->GetNetPlayName(game));

  // Join our local server
  return NetPlayJoin();
}

void MainWindow::NetPlayQuit()
{
  Settings::Instance().ResetNetPlayClient();
  Settings::Instance().ResetNetPlayServer();
#ifdef USE_DISCORD_PRESENCE
  Discord::UpdateDiscordPresence();
#endif
}

void MainWindow::UpdateScreenSaverInhibition()
{
  const bool inhibit =
      Config::Get(Config::MAIN_DISABLE_SCREENSAVER) && (Core::GetState() == Core::State::Running);

  if (inhibit == m_is_screensaver_inhibited)
    return;

  m_is_screensaver_inhibited = inhibit;

#ifdef HAVE_X11
  if (GetWindowSystemType() == WindowSystemType::X11)
    UICommon::InhibitScreenSaver(winId(), inhibit);
#else
  UICommon::InhibitScreenSaver(inhibit);
#endif
}

bool MainWindow::eventFilter(QObject* object, QEvent* event)
{
  if (event->type() == QEvent::Close)
  {
    if (RequestStop() && object == this)
      m_exit_requested = true;

    static_cast<QCloseEvent*>(event)->ignore();
    return true;
  }

  return false;
}

QMenu* MainWindow::createPopupMenu()
{
  // Disable the default popup menu as it exposes the debugger UI even when the debugger UI is
  // disabled, which can lead to user confusion (see e.g. https://bugs.dolphin-emu.org/issues/13306)
  return nullptr;
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
  if (event->mimeData()->hasUrls() && event->mimeData()->urls().size() == 1)
    event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
  const QList<QUrl>& urls = event->mimeData()->urls();
  if (urls.empty())
    return;

  QStringList files;
  QStringList folders;

  for (const QUrl& url : urls)
  {
    QFileInfo file_info(url.toLocalFile());
    QString path = file_info.filePath();

    if (!file_info.exists() || !file_info.isReadable())
    {
      ModalMessageBox::critical(this, tr("Error"), tr("Failed to open '%1'").arg(path));
      return;
    }

    (file_info.isFile() ? files : folders).append(path);
  }

  if (!files.isEmpty())
  {
    StartGame(StringListToStdVector(files));
  }
  else
  {
    Settings& settings = Settings::Instance();
    const bool show_confirm = !settings.GetPaths().empty();

    for (const QString& folder : folders)
    {
      if (show_confirm)
      {
        if (ModalMessageBox::question(
                this, tr("Confirm"),
                tr("Do you want to add \"%1\" to the list of Game Paths?").arg(folder)) !=
            QMessageBox::Yes)
          return;
      }
      settings.AddPath(folder);
    }
  }
}

QSize MainWindow::sizeHint() const
{
  return QSize(800, 600);
}

#ifdef _WIN32
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
  auto* msg = reinterpret_cast<MSG*>(message);
  if (msg && msg->message == WM_SETTINGCHANGE && msg->lParam != NULL &&
      std::wstring_view(L"ImmersiveColorSet")
              .compare(reinterpret_cast<const wchar_t*>(msg->lParam)) == 0)
  {
    // Windows light/dark theme has changed. Update our flag and refresh the theme.
    auto& settings = Settings::Instance();
    const bool was_dark_before = settings.IsSystemDark();
    settings.UpdateSystemDark();
    if (settings.IsSystemDark() != was_dark_before)
    {
      settings.ApplyStyle();

      // force the colors in the Skylander window to update
      if (m_skylander_window)
        m_skylander_window->RefreshList();
    }

    // TODO: When switching from light to dark, the window decorations remain light. Qt seems very
    // convinced that it needs to change these in response to this message, so even if we set them
    // to dark here, Qt sets them back to light afterwards.
  }

  return false;
}
#endif

void MainWindow::OnBootGameCubeIPL(DiscIO::Region region)
{
  StartGame(std::make_unique<BootParameters>(BootParameters::IPL{region}));
}

void MainWindow::OnImportNANDBackup()
{
  auto response = ModalMessageBox::question(
      this, tr("Question"),
      tr("Merging a new NAND over your currently selected NAND will overwrite any channels "
         "and savegames that already exist. This process is not reversible, so it is "
         "recommended that you keep backups of both NANDs. Are you sure you want to "
         "continue?"));

  if (response == QMessageBox::No)
    return;

  QString file =
      DolphinFileDialog::getOpenFileName(this, tr("Select the save file"), QDir::currentPath(),
                                         tr("BootMii NAND backup file (*.bin);;"
                                            "All Files (*)"));

  if (file.isEmpty())
    return;

  ParallelProgressDialog dialog(this);
  dialog.GetRaw()->setMinimum(0);
  dialog.GetRaw()->setMaximum(0);
  dialog.GetRaw()->setLabelText(tr("Importing NAND backup"));
  dialog.GetRaw()->setCancelButton(nullptr);

  auto beginning = QDateTime::currentDateTime().toMSecsSinceEpoch();

  std::future<void> result = std::async(std::launch::async, [&] {
    DiscIO::NANDImporter().ImportNANDBin(
        file.toStdString(),
        [&dialog, beginning] {
          dialog.SetLabelText(
              tr("Importing NAND backup\n Time elapsed: %1s")
                  .arg((QDateTime::currentDateTime().toMSecsSinceEpoch() - beginning) / 1000));
        },
        [this] {
          std::optional<std::string> keys_file = RunOnObject(this, [this] {
            return DolphinFileDialog::getOpenFileName(
                       this, tr("Select the keys file (OTP/SEEPROM dump)"), QDir::currentPath(),
                       tr("BootMii keys file (*.bin);;"
                          "All Files (*)"))
                .toStdString();
          });
          if (keys_file)
            return *keys_file;
          return std::string("");
        });
    dialog.Reset();
  });

  SetQWidgetWindowDecorations(dialog.GetRaw());
  dialog.GetRaw()->exec();

  result.wait();

  m_menu_bar->UpdateToolsMenu(Core::IsRunning());
}

void MainWindow::OnPlayRecording()
{
  QString dtm_file = DolphinFileDialog::getOpenFileName(
      this, tr("Select the Recording File to Play"), QString(), tr("Dolphin TAS Movies (*.dtm)"));

  if (dtm_file.isEmpty())
    return;

  if (!Movie::IsReadOnly())
  {
    // let's make the read-only flag consistent at the start of a movie.
    Movie::SetReadOnly(true);
    emit ReadOnlyModeChanged(true);
  }

  std::optional<std::string> savestate_path;
  if (Movie::PlayInput(dtm_file.toStdString(), &savestate_path))
  {
    emit RecordingStatusChanged(true);

    Play(savestate_path);
  }
}

void MainWindow::OnStartRecording()
{
  if ((!Core::IsRunningAndStarted() && Core::IsRunning()) || Movie::IsRecordingInput() ||
      Movie::IsPlayingInput())
    return;

  if (Movie::IsReadOnly())
  {
    // The user just chose to record a movie, so that should take precedence
    Movie::SetReadOnly(false);
    emit ReadOnlyModeChanged(true);
  }

  Movie::ControllerTypeArray controllers{};
  Movie::WiimoteEnabledArray wiimotes{};

  for (int i = 0; i < 4; i++)
  {
    const SerialInterface::SIDevices si_device = Config::Get(Config::GetInfoForSIDevice(i));
    if (si_device == SerialInterface::SIDEVICE_GC_GBA_EMULATED)
      controllers[i] = Movie::ControllerType::GBA;
    else if (SerialInterface::SIDevice_IsGCController(si_device))
      controllers[i] = Movie::ControllerType::GC;
    else
      controllers[i] = Movie::ControllerType::None;
    wiimotes[i] = Config::Get(Config::GetInfoForWiimoteSource(i)) != WiimoteSource::None;
  }

  if (Movie::BeginRecordingInput(controllers, wiimotes))
  {
    emit RecordingStatusChanged(true);

    if (!Core::IsRunning())
      Play();
  }
}

void MainWindow::OnStopRecording()
{
  if (Movie::IsRecordingInput())
    OnExportRecording();
  if (Movie::IsMovieActive())
    Movie::EndPlayInput(false);
  emit RecordingStatusChanged(false);
}

void MainWindow::OnExportRecording()
{
  Core::RunAsCPUThread([this] {
    QString dtm_file = DolphinFileDialog::getSaveFileName(
        this, tr("Save Recording File As"), QString(), tr("Dolphin TAS Movies (*.dtm)"));
    if (!dtm_file.isEmpty())
      Movie::SaveRecording(dtm_file.toStdString());
  });
}

void MainWindow::OnActivateChat()
{
  if (g_netplay_chat_ui)
    g_netplay_chat_ui->Activate();
}

void MainWindow::OnRequestGolfControl()
{
  auto client = Settings::Instance().GetNetPlayClient();
  if (client)
    client->RequestGolfControl();
}

void MainWindow::ShowTASInput()
{
  for (int i = 0; i < num_gc_controllers; i++)
  {
    const auto si_device = Config::Get(Config::GetInfoForSIDevice(i));
    if (si_device == SerialInterface::SIDEVICE_GC_GBA_EMULATED)
    {
      SetQWidgetWindowDecorations(m_gba_tas_input_windows[i]);
      m_gba_tas_input_windows[i]->show();
      m_gba_tas_input_windows[i]->raise();
      m_gba_tas_input_windows[i]->activateWindow();
    }
    else if (si_device != SerialInterface::SIDEVICE_NONE &&
             si_device != SerialInterface::SIDEVICE_GC_GBA)
    {
      SetQWidgetWindowDecorations(m_gc_tas_input_windows[i]);
      m_gc_tas_input_windows[i]->show();
      m_gc_tas_input_windows[i]->raise();
      m_gc_tas_input_windows[i]->activateWindow();
    }
  }

  for (int i = 0; i < num_wii_controllers; i++)
  {
    if (Config::Get(Config::GetInfoForWiimoteSource(i)) == WiimoteSource::Emulated &&
        (!Core::IsRunning() || SConfig::GetInstance().bWii))
    {
      SetQWidgetWindowDecorations(m_wii_tas_input_windows[i]);
      m_wii_tas_input_windows[i]->show();
      m_wii_tas_input_windows[i]->raise();
      m_wii_tas_input_windows[i]->activateWindow();
    }
  }
}

void MainWindow::OnConnectWiiRemote(int id)
{
  Core::RunAsCPUThread([&] {
    if (const auto bt = WiiUtils::GetBluetoothEmuDevice())
    {
      const auto wm = bt->AccessWiimoteByIndex(id);
      wm->Activate(!wm->IsConnected());
    }
  });
}

#ifdef USE_RETRO_ACHIEVEMENTS
void MainWindow::ShowAchievementsWindow()
{
  if (!m_achievements_window)
  {
    m_achievements_window = new AchievementsWindow(this);
  }

  SetQWidgetWindowDecorations(m_achievements_window);
  m_achievements_window->show();
  m_achievements_window->raise();
  m_achievements_window->activateWindow();
}

void MainWindow::ShowAchievementSettings()
{
  ShowAchievementsWindow();
  m_achievements_window->ForceSettingsTab();
}
#endif  // USE_RETRO_ACHIEVEMENTS

void MainWindow::ShowMemcardManager()
{
  GCMemcardManager manager(this);

  SetQWidgetWindowDecorations(&manager);
  manager.exec();
}

void MainWindow::ShowResourcePackManager()
{
  ResourcePackManager manager(this);

  SetQWidgetWindowDecorations(&manager);
  manager.exec();
}

void MainWindow::ShowCheatsManager()
{
  SetQWidgetWindowDecorations(m_cheats_manager);
  m_cheats_manager->show();
}

void MainWindow::ShowRiivolutionBootWidget(const UICommon::GameFile& game)
{
  auto second_game = m_game_list->FindSecondDisc(game);
  std::vector<std::string> paths = {game.GetFilePath()};
  if (second_game != nullptr)
    paths.push_back(second_game->GetFilePath());
  std::unique_ptr<BootParameters> boot_params = BootParameters::GenerateFromFile(paths);
  if (!boot_params)
    return;
  if (!std::holds_alternative<BootParameters::Disc>(boot_params->parameters))
    return;

  auto& disc = std::get<BootParameters::Disc>(boot_params->parameters);
  RiivolutionBootWidget w(disc.volume->GetGameID(), disc.volume->GetRevision(),
                          disc.volume->GetDiscNumber(), game.GetFilePath(), this);
  SetQWidgetWindowDecorations(&w);

#ifdef USE_RETRO_ACHIEVEMENTS
  connect(&w, &RiivolutionBootWidget::OpenAchievementSettings, this,
          &MainWindow::ShowAchievementSettings);
#endif  // USE_RETRO_ACHIEVEMENTS

  w.exec();
  if (!w.ShouldBoot())
    return;

  AddRiivolutionPatches(boot_params.get(), std::move(w.GetPatches()));
  StartGame(std::move(boot_params));
}

void MainWindow::Show()
{
  if (!Settings::Instance().IsBatchModeEnabled())
  {
    SetQWidgetWindowDecorations(this);
    QWidget::show();
  }

  // If the booting of a game was requested on start up, do that now
  if (m_pending_boot != nullptr)
  {
    StartGame(std::move(m_pending_boot));
    m_pending_boot.reset();
  }
}
