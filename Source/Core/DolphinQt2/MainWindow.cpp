
// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressDialog>
#include <QVBoxLayout>

#include <future>
#include <optional>

#include "Common/Version.h"

#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/CommonTitles.h"
#include "Core/Config/NetplaySettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/ProcessorInterface.h"
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

#include "DiscIO/NANDImporter.h"

#include "DolphinQt2/AboutDialog.h"
#include "DolphinQt2/Config/ControllersWindow.h"
#include "DolphinQt2/Config/Graphics/GraphicsWindow.h"
#include "DolphinQt2/Config/LogConfigWidget.h"
#include "DolphinQt2/Config/LogWidget.h"
#include "DolphinQt2/Config/Mapping/MappingWindow.h"
#include "DolphinQt2/Config/SettingsWindow.h"
#include "DolphinQt2/Debugger/BreakpointWidget.h"
#include "DolphinQt2/Debugger/CodeWidget.h"
#include "DolphinQt2/Debugger/RegisterWidget.h"
#include "DolphinQt2/Debugger/WatchWidget.h"
#include "DolphinQt2/FIFOPlayerWindow.h"
#include "DolphinQt2/GCMemcardManager.h"
#include "DolphinQt2/Host.h"
#include "DolphinQt2/HotkeyScheduler.h"
#include "DolphinQt2/MainWindow.h"
#include "DolphinQt2/NetPlay/NetPlayDialog.h"
#include "DolphinQt2/NetPlay/NetPlaySetupDialog.h"
#include "DolphinQt2/QtUtils/QueueOnObject.h"
#include "DolphinQt2/QtUtils/RunOnObject.h"
#include "DolphinQt2/QtUtils/WindowActivationEventFilter.h"
#include "DolphinQt2/Resources.h"
#include "DolphinQt2/SearchBar.h"
#include "DolphinQt2/Settings.h"
#include "DolphinQt2/TAS/GCTASInputWindow.h"
#include "DolphinQt2/TAS/WiiTASInputWindow.h"
#include "DolphinQt2/WiiUpdate.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"

#include "UICommon/UICommon.h"

#if defined(HAVE_XRANDR) && HAVE_XRANDR
#include <qpa/qplatformnativeinterface.h>
#include "UICommon/X11Utils.h"
#endif

MainWindow::MainWindow(std::unique_ptr<BootParameters> boot_parameters) : QMainWindow(nullptr)
{
  setWindowTitle(QString::fromStdString(Common::scm_rev_str));
  setWindowIcon(Resources::GetAppIcon());
  setUnifiedTitleAndToolBarOnMac(true);
  setAcceptDrops(true);

  InitControllers();

  CreateComponents();

  ConnectGameList();
  ConnectHost();
  ConnectToolBar();
  ConnectRenderWidget();
  ConnectStack();
  ConnectMenuBar();
  ConnectHotkeys();

  InitCoreCallbacks();

  NetPlayInit();

  if (boot_parameters)
    StartGame(std::move(boot_parameters));
}

MainWindow::~MainWindow()
{
  m_render_widget->deleteLater();
  ShutdownControllers();

  Config::Save();
}

void MainWindow::InitControllers()
{
  if (g_controller_interface.IsInit())
    return;

  g_controller_interface.Initialize(reinterpret_cast<void*>(winId()));
  Pad::Initialize();
  Keyboard::Initialize();
  Wiimote::Initialize(Wiimote::InitializeMode::DO_NOT_WAIT_FOR_WIIMOTES);
  m_hotkey_scheduler = new HotkeyScheduler();
  m_hotkey_scheduler->Start();
}

void MainWindow::ShutdownControllers()
{
  m_hotkey_scheduler->Stop();

  g_controller_interface.Shutdown();
  Pad::Shutdown();
  Keyboard::Shutdown();
  Wiimote::Shutdown();
  HotkeyManagerEmu::Shutdown();

  m_hotkey_scheduler->deleteLater();
}

void MainWindow::InitCoreCallbacks()
{
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [=](Core::State state) {
    if (state == Core::State::Uninitialized)
      OnStopComplete();
  });
  installEventFilter(this);
  m_render_widget->installEventFilter(this);
}

static void InstallHotkeyFilter(QWidget* dialog)
{
  auto* filter = new WindowActivationEventFilter();
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
  m_controllers_window = new ControllersWindow(this);
  m_settings_window = new SettingsWindow(this);

  for (int i = 0; i < 4; i++)
  {
    m_gc_tas_input_windows[i] = new GCTASInputWindow(this, i);
    m_wii_tas_input_windows[i] = new WiiTASInputWindow(this, i);
  }

  Movie::SetGCInputManip([this](GCPadStatus* pad_status, int controller_id) {
    m_gc_tas_input_windows[controller_id]->GetValues(pad_status);
  });

  Movie::SetWiiInputManip([this](u8* input_data, WiimoteEmu::ReportFeatures rptf, int controller_id,
                                 int ext, wiimote_key key) {
    m_wii_tas_input_windows[controller_id]->GetValues(input_data, rptf, ext, key);
  });

  m_hotkey_window = new MappingWindow(this, MappingWindow::Type::MAPPING_HOTKEYS, 0);

  m_log_widget = new LogWidget(this);
  m_log_config_widget = new LogConfigWidget(this);
  m_fifo_window = new FIFOPlayerWindow(this);

  connect(m_fifo_window, &FIFOPlayerWindow::LoadFIFORequested, this,
          [this](const QString& path) { StartGame(path); });
  m_register_widget = new RegisterWidget(this);
  m_watch_widget = new WatchWidget(this);
  m_breakpoint_widget = new BreakpointWidget(this);
  m_code_widget = new CodeWidget(this);

  connect(m_watch_widget, &WatchWidget::RequestMemoryBreakpoint,
          [this](u32 addr) { m_breakpoint_widget->AddAddressMBP(addr); });
  connect(m_register_widget, &RegisterWidget::RequestMemoryBreakpoint,
          [this](u32 addr) { m_breakpoint_widget->AddAddressMBP(addr); });
  connect(m_code_widget, &CodeWidget::BreakpointsChanged, m_breakpoint_widget,
          &BreakpointWidget::Update);

#if defined(HAVE_XRANDR) && HAVE_XRANDR
  m_graphics_window = new GraphicsWindow(
      new X11Utils::XRRConfiguration(
          static_cast<Display*>(QGuiApplication::platformNativeInterface()->nativeResourceForWindow(
              "display", windowHandle())),
          winId()),
      this);
#else
  m_graphics_window = new GraphicsWindow(nullptr, this);
#endif

  InstallHotkeyFilter(m_hotkey_window);
  InstallHotkeyFilter(m_controllers_window);
  InstallHotkeyFilter(m_settings_window);
  InstallHotkeyFilter(m_graphics_window);
}

void MainWindow::ConnectMenuBar()
{
  setMenuBar(m_menu_bar);
  // File
  connect(m_menu_bar, &MenuBar::Open, this, &MainWindow::Open);
  connect(m_menu_bar, &MenuBar::Exit, this, &MainWindow::close);
  connect(m_menu_bar, &MenuBar::EjectDisc, this, &MainWindow::EjectDisc);
  connect(m_menu_bar, &MenuBar::ChangeDisc, this, &MainWindow::ChangeDisc);
  connect(m_menu_bar, &MenuBar::BootDVDBackup, this,
          [this](const QString& drive) { StartGame(drive); });

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

  // Tools
  connect(m_menu_bar, &MenuBar::ShowMemcardManager, this, &MainWindow::ShowMemcardManager);
  connect(m_menu_bar, &MenuBar::BootGameCubeIPL, this, &MainWindow::OnBootGameCubeIPL);
  connect(m_menu_bar, &MenuBar::ImportNANDBackup, this, &MainWindow::OnImportNANDBackup);
  connect(m_menu_bar, &MenuBar::PerformOnlineUpdate, this, &MainWindow::PerformOnlineUpdate);
  connect(m_menu_bar, &MenuBar::BootWiiSystemMenu, this, &MainWindow::BootWiiSystemMenu);
  connect(m_menu_bar, &MenuBar::StartNetPlay, this, &MainWindow::ShowNetPlaySetupDialog);
  connect(m_menu_bar, &MenuBar::ShowFIFOPlayer, this, &MainWindow::ShowFIFOPlayer);
  connect(m_menu_bar, &MenuBar::ConnectWiiRemote, this, &MainWindow::OnConnectWiiRemote);

  // Movie
  connect(m_menu_bar, &MenuBar::PlayRecording, this, &MainWindow::OnPlayRecording);
  connect(m_menu_bar, &MenuBar::StartRecording, this, &MainWindow::OnStartRecording);
  connect(m_menu_bar, &MenuBar::StopRecording, this, &MainWindow::OnStopRecording);
  connect(m_menu_bar, &MenuBar::ExportRecording, this, &MainWindow::OnExportRecording);
  connect(m_menu_bar, &MenuBar::ShowTASInput, this, &MainWindow::ShowTASInput);

  // View
  connect(m_menu_bar, &MenuBar::ShowList, m_game_list, &GameList::SetListView);
  connect(m_menu_bar, &MenuBar::ShowGrid, m_game_list, &GameList::SetGridView);
  connect(m_menu_bar, &MenuBar::ToggleSearch, m_search_bar, &SearchBar::Toggle);

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
}

void MainWindow::ConnectHotkeys()
{
  connect(m_hotkey_scheduler, &HotkeyScheduler::ExitHotkey, this, &MainWindow::close);
  connect(m_hotkey_scheduler, &HotkeyScheduler::TogglePauseHotkey, this, &MainWindow::TogglePause);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StopHotkey, this, &MainWindow::RequestStop);
  connect(m_hotkey_scheduler, &HotkeyScheduler::ScreenShotHotkey, this, &MainWindow::ScreenShot);
  connect(m_hotkey_scheduler, &HotkeyScheduler::FullScreenHotkey, this, &MainWindow::FullScreen);

  connect(m_hotkey_scheduler, &HotkeyScheduler::StateLoadSlotHotkey, this,
          &MainWindow::StateLoadSlot);
  connect(m_hotkey_scheduler, &HotkeyScheduler::StateSaveSlotHotkey, this,
          &MainWindow::StateSaveSlot);
  connect(m_hotkey_scheduler, &HotkeyScheduler::SetStateSlotHotkey, this,
          &MainWindow::SetStateSlot);

  connect(m_hotkey_scheduler, &HotkeyScheduler::StartRecording, this,
          &MainWindow::OnStartRecording);
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
}

void MainWindow::ConnectToolBar()
{
  addToolBar(m_tool_bar);

  connect(m_tool_bar, &ToolBar::OpenPressed, this, &MainWindow::Open);

  connect(m_tool_bar, &ToolBar::OpenPressed, this, &MainWindow::Open);
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

  connect(m_game_list, &GameList::OpenGeneralSettings, this, &MainWindow::ShowGeneralWindow);
}

void MainWindow::ConnectRenderWidget()
{
  m_rendering_to_main = false;
  m_render_widget->hide();
  connect(m_render_widget, &RenderWidget::Closed, this, &MainWindow::ForceStop);
}

void MainWindow::ConnectHost()
{
  connect(Host::GetInstance(), &Host::UpdateProgressDialog, this,
          &MainWindow::OnUpdateProgressDialog);
}

void MainWindow::ConnectStack()
{
  auto* widget = new QWidget;
  auto* layout = new QVBoxLayout;
  widget->setLayout(layout);

  layout->addWidget(m_game_list);
  layout->addWidget(m_search_bar);
  layout->setMargin(0);

  connect(m_search_bar, &SearchBar::Search, m_game_list, &GameList::SetSearchTerm);

  m_stack->addWidget(widget);

  setCentralWidget(m_stack);

  setTabPosition(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea, QTabWidget::North);
  addDockWidget(Qt::RightDockWidgetArea, m_log_widget);
  addDockWidget(Qt::RightDockWidgetArea, m_log_config_widget);
  addDockWidget(Qt::RightDockWidgetArea, m_code_widget);
  addDockWidget(Qt::RightDockWidgetArea, m_register_widget);
  addDockWidget(Qt::RightDockWidgetArea, m_watch_widget);
  addDockWidget(Qt::RightDockWidgetArea, m_breakpoint_widget);

  tabifyDockWidget(m_log_widget, m_log_config_widget);
  tabifyDockWidget(m_log_widget, m_code_widget);
  tabifyDockWidget(m_log_widget, m_register_widget);
  tabifyDockWidget(m_log_widget, m_watch_widget);
  tabifyDockWidget(m_log_widget, m_breakpoint_widget);
}

QString MainWindow::PromptFileName()
{
  return QFileDialog::getOpenFileName(
      this, tr("Select a File"), QDir::currentPath(),
      tr("All GC/Wii files (*.elf *.dol *.gcm *.iso *.tgc *.wbfs *.ciso *.gcz *.wad);;"
         "All Files (*)"));
}

void MainWindow::ChangeDisc()
{
  QString file = PromptFileName();

  if (!file.isEmpty())
    Core::RunAsCPUThread([&file] { DVDInterface::ChangeDisc(file.toStdString()); });
}

void MainWindow::EjectDisc()
{
  Core::RunAsCPUThread(DVDInterface::EjectDisc);
}

void MainWindow::Open()
{
  QString file = PromptFileName();
  if (!file.isEmpty())
    StartGame(file);
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
    EnableScreenSaver(false);
  }
  else
  {
    std::shared_ptr<const UICommon::GameFile> selection = m_game_list->GetSelectedGame();
    if (selection)
    {
      StartGame(selection->GetFilePath(), savestate_path);
      EnableScreenSaver(false);
    }
    else
    {
      QString default_path = QString::fromStdString(SConfig::GetInstance().m_strDefaultISO);
      if (!default_path.isEmpty() && QFile::exists(default_path))
      {
        StartGame(default_path, savestate_path);
        EnableScreenSaver(false);
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
  EnableScreenSaver(true);
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
  HideRenderWidget();

  if (m_exit_requested)
    QGuiApplication::instance()->quit();

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

  if (SConfig::GetInstance().bConfirmStop)
  {
    const Core::State state = Core::GetState();

    // Only pause the game, if NetPlay is not running
    bool pause = Settings::Instance().GetNetPlayClient() != nullptr;

    if (pause)
      Core::SetState(Core::State::Paused);

    QMessageBox::StandardButton confirm;
    confirm = QMessageBox::question(m_render_widget, tr("Confirm"),
                                    m_stop_requested ?
                                        tr("A shutdown is already in progress. Unsaved data "
                                           "may be lost if you stop the current emulation "
                                           "before it completes. Force stop?") :
                                        tr("Do you want to stop the current emulation?"));

    if (pause)
      Core::SetState(state);

    if (confirm != QMessageBox::Yes)
      return false;
  }

  // TODO: Add Movie shutdown
  // TODO: Add Debugger shutdown

  if (!m_stop_requested && UICommon::TriggerSTMPowerEvent())
  {
    m_stop_requested = true;

    // Unpause because gracefully shutting down needs the game to actually request a shutdown.
    // TODO: Do not unpause in debug mode to allow debugging until the complete shutdown.
    if (Core::GetState() == Core::State::Paused)
      Core::SetState(Core::State::Running);

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
  BootManager::Stop();
  EnableScreenSaver(true);
}

void MainWindow::Reset()
{
  if (Movie::IsRecordingInput())
    Movie::SetReset(true);
  ProcessorInterface::ResetButton_Tap();
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
  HideRenderWidget(false);
  if (was_fullscreen)
    ShowRenderWidget();
  else
    m_render_widget->showFullScreen();
}

void MainWindow::ScreenShot()
{
  Core::SaveScreenShot();
}

void MainWindow::StartGame(const QString& path, const std::optional<std::string>& savestate_path)
{
  StartGame(path.toStdString(), savestate_path);
}

void MainWindow::StartGame(const std::string& path,
                           const std::optional<std::string>& savestate_path)
{
  StartGame(BootParameters::GenerateFromFile(path, savestate_path));
}

void MainWindow::StartGame(std::unique_ptr<BootParameters>&& parameters)
{
  // If we're running, only start a new game once we've stopped the last.
  if (Core::GetState() != Core::State::Uninitialized)
  {
    if (!RequestStop())
      return;

    // As long as the shutdown isn't complete, we can't boot, so let's boot later
    m_pending_boot = std::move(parameters);
    return;
  }
  // Boot up, show an error if it fails to load the game.
  if (!BootManager::BootCore(std::move(parameters)))
  {
    QMessageBox::critical(this, tr("Error"), tr("Failed to init core"), QMessageBox::Ok);
    return;
  }
  ShowRenderWidget();

#ifdef Q_OS_WIN
  // Prevents Windows from sleeping, turning off the display, or idling
  EXECUTION_STATE shouldScreenSave =
      SConfig::GetInstance().bDisableScreenSaver ? ES_DISPLAY_REQUIRED : 0;
  SetThreadExecutionState(ES_CONTINUOUS | shouldScreenSave | ES_SYSTEM_REQUIRED);
#endif
}

void MainWindow::ShowRenderWidget()
{
  if (SConfig::GetInstance().bRenderToMain)
  {
    // If we're rendering to main, add it to the stack and update our title when necessary.
    m_rendering_to_main = true;
    m_stack->setCurrentIndex(m_stack->addWidget(m_render_widget));
    connect(Host::GetInstance(), &Host::RequestTitle, this, &MainWindow::setWindowTitle);
    m_stack->repaint();
  }
  else
  {
    // Otherwise, just show it.
    m_rendering_to_main = false;
    if (SConfig::GetInstance().bFullscreen && !m_render_widget->isFullScreen())
    {
      m_render_widget->showFullScreen();
    }
    else
    {
      m_render_widget->showNormal();
      m_render_widget->resize(640, 480);
    }
  }
}

void MainWindow::HideRenderWidget(bool reinit)
{
  if (m_rendering_to_main)
  {
    // Remove the widget from the stack and reparent it to nullptr, so that it can draw
    // itself in a new window if it wants. Disconnect the title updates.
    m_stack->removeWidget(m_render_widget);
    m_render_widget->setParent(nullptr);
    m_rendering_to_main = false;
    disconnect(Host::GetInstance(), &Host::RequestTitle, this, &MainWindow::setWindowTitle);
    setWindowTitle(QString::fromStdString(Common::scm_rev_str));
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
  }
}

void MainWindow::ShowControllersWindow()
{
  m_controllers_window->show();
  m_controllers_window->raise();
  m_controllers_window->activateWindow();
}

void MainWindow::ShowSettingsWindow()
{
  m_settings_window->show();
  m_settings_window->raise();
  m_settings_window->activateWindow();
}

void MainWindow::ShowAudioWindow()
{
  m_settings_window->SelectAudioPane();
  ShowSettingsWindow();
}

void MainWindow::ShowGeneralWindow()
{
  m_settings_window->SelectGeneralPane();
  ShowSettingsWindow();
}

void MainWindow::ShowAboutDialog()
{
  AboutDialog about{this};
  about.exec();
}

void MainWindow::ShowHotkeyDialog()
{
  m_hotkey_window->show();
  m_hotkey_window->raise();
  m_hotkey_window->activateWindow();
}

void MainWindow::ShowGraphicsWindow()
{
  m_graphics_window->show();
  m_graphics_window->raise();
  m_graphics_window->activateWindow();
}

void MainWindow::ShowNetPlaySetupDialog()
{
  m_netplay_setup_dialog->show();
  m_netplay_setup_dialog->raise();
  m_netplay_setup_dialog->activateWindow();
}

void MainWindow::ShowFIFOPlayer()
{
  m_fifo_window->show();
  m_fifo_window->raise();
  m_fifo_window->activateWindow();
}

void MainWindow::StateLoad()
{
  QString path = QFileDialog::getOpenFileName(this, tr("Select a File"), QDir::currentPath(),
                                              tr("All Save States (*.sav *.s##);; All Files (*)"));
  State::LoadAs(path.toStdString());
}

void MainWindow::StateSave()
{
  QString path = QFileDialog::getSaveFileName(this, tr("Select a File"), QDir::currentPath(),
                                              tr("All Save States (*.sav *.s##);; All Files (*)"));
  State::SaveAs(path.toStdString());
}

void MainWindow::StateLoadSlot()
{
  State::Load(m_state_slot);
}

void MainWindow::StateSaveSlot()
{
  State::Save(m_state_slot, true);
  m_menu_bar->UpdateStateSlotMenu();
}

void MainWindow::StateLoadSlotAt(int slot)
{
  State::Load(slot);
}

void MainWindow::StateSaveSlotAt(int slot)
{
  State::Save(slot, true);
  m_menu_bar->UpdateStateSlotMenu();
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
}

void MainWindow::PerformOnlineUpdate(const std::string& region)
{
  WiiUpdate::PerformOnlineUpdate(region, this);
  // Since the update may have installed a newer system menu, refresh the tools menu.
  m_menu_bar->UpdateToolsMenu(false);
}

void MainWindow::BootWiiSystemMenu()
{
  StartGame(std::make_unique<BootParameters>(BootParameters::NANDTitle{Titles::SYSTEM_MENU}));
}

void MainWindow::NetPlayInit()
{
  m_netplay_setup_dialog = new NetPlaySetupDialog(this);
  m_netplay_dialog = new NetPlayDialog(this);

  connect(m_netplay_dialog, &NetPlayDialog::Boot, this,
          [this](const QString& path) { StartGame(path); });
  connect(m_netplay_dialog, &NetPlayDialog::Stop, this, &MainWindow::RequestStop);
  connect(m_netplay_dialog, &NetPlayDialog::rejected, this, &MainWindow::NetPlayQuit);
  connect(m_netplay_setup_dialog, &NetPlaySetupDialog::Join, this, &MainWindow::NetPlayJoin);
  connect(m_netplay_setup_dialog, &NetPlaySetupDialog::Host, this, &MainWindow::NetPlayHost);
}

bool MainWindow::NetPlayJoin()
{
  if (Core::IsRunning())
  {
    QMessageBox::critical(
        nullptr, QObject::tr("Error"),
        QObject::tr("Can't start a NetPlay Session while a game is still running!"));
    return false;
  }

  if (m_netplay_dialog->isVisible())
  {
    QMessageBox::critical(nullptr, QObject::tr("Error"),
                          QObject::tr("A NetPlay Session is already in progress!"));
    return false;
  }

  // Settings
  std::string host_ip;
  u16 host_port;
  if (Settings::Instance().GetNetPlayServer() != nullptr)
  {
    host_ip = "127.0.0.1";
    host_port = Settings::Instance().GetNetPlayServer()->GetPort();
  }
  else
  {
    host_ip = Config::Get(Config::NETPLAY_HOST_CODE);
    host_port = Config::Get(Config::NETPLAY_HOST_PORT);
  }

  const std::string traversal_choice = Config::Get(Config::NETPLAY_TRAVERSAL_CHOICE);
  const bool is_traversal = traversal_choice == "traversal";

  const std::string traversal_host = Config::Get(Config::NETPLAY_TRAVERSAL_SERVER);
  const u16 traversal_port = Config::Get(Config::NETPLAY_TRAVERSAL_PORT);
  const std::string nickname = Config::Get(Config::NETPLAY_NICKNAME);

  // Create Client
  Settings::Instance().ResetNetPlayClient(new NetPlayClient(
      host_ip, host_port, m_netplay_dialog, nickname,
      NetTraversalConfig{Settings::Instance().GetNetPlayServer() != nullptr ? false : is_traversal,
                         traversal_host, traversal_port}));

  if (!Settings::Instance().GetNetPlayClient()->IsConnected())
  {
    QMessageBox::critical(nullptr, QObject::tr("Error"),
                          QObject::tr("Failed to connect to server"));
    return false;
  }

  m_netplay_setup_dialog->close();
  m_netplay_dialog->show(nickname, is_traversal);

  return true;
}

bool MainWindow::NetPlayHost(const QString& game_id)
{
  if (Core::IsRunning())
  {
    QMessageBox::critical(
        nullptr, QObject::tr("Error"),
        QObject::tr("Can't start a NetPlay Session while a game is still running!"));
    return false;
  }

  if (m_netplay_dialog->isVisible())
  {
    QMessageBox::critical(nullptr, QObject::tr("Error"),
                          QObject::tr("A NetPlay Session is already in progress!"));
    return false;
  }

  // Settings
  u16 host_port = Config::Get(Config::NETPLAY_HOST_PORT);
  const std::string traversal_choice = Config::Get(Config::NETPLAY_TRAVERSAL_CHOICE);
  const bool is_traversal = traversal_choice == "traversal";
  const bool use_upnp = Config::Get(Config::NETPLAY_USE_UPNP);

  const std::string traversal_host = Config::Get(Config::NETPLAY_TRAVERSAL_SERVER);
  const u16 traversal_port = Config::Get(Config::NETPLAY_TRAVERSAL_PORT);

  if (is_traversal)
    host_port = Config::Get(Config::NETPLAY_LISTEN_PORT);

  // Create Server
  Settings::Instance().ResetNetPlayServer(new NetPlayServer(
      host_port, use_upnp, NetTraversalConfig{is_traversal, traversal_host, traversal_port}));

  if (!Settings::Instance().GetNetPlayServer()->is_connected)
  {
    QMessageBox::critical(
        nullptr, QObject::tr("Failed to open server"),
        QObject::tr(
            "Failed to listen on port %1. Is another instance of the NetPlay server running?")
            .arg(host_port));
    return false;
  }

  Settings::Instance().GetNetPlayServer()->ChangeGame(game_id.toStdString());

  // Join our local server
  return NetPlayJoin();
}

void MainWindow::NetPlayQuit()
{
  Settings::Instance().ResetNetPlayClient();
  Settings::Instance().ResetNetPlayServer();
}

void MainWindow::EnableScreenSaver(bool enable)
{
#if defined(HAVE_XRANDR) && HAVE_XRANDR
  UICommon::EnableScreenSaver(
      static_cast<Display*>(QGuiApplication::platformNativeInterface()->nativeResourceForWindow(
          "display", windowHandle())),
      winId(), enable);
#else
  UICommon::EnableScreenSaver(enable);
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

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
  if (event->mimeData()->hasUrls() && event->mimeData()->urls().size() == 1)
    event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
  const auto& urls = event->mimeData()->urls();
  if (urls.empty())
    return;

  const auto& url = urls[0];
  QFileInfo file_info(url.toLocalFile());

  auto path = file_info.filePath();

  if (!file_info.exists() || !file_info.isReadable())
  {
    QMessageBox::critical(this, tr("Error"), tr("Failed to open '%1'").arg(path));
    return;
  }

  if (file_info.isFile())
  {
    StartGame(path);
  }
  else
  {
    auto& settings = Settings::Instance();

    if (settings.GetPaths().size() != 0)
    {
      if (QMessageBox::question(
              this, tr("Confirm"),
              tr("Do you want to add \"%1\" to the list of Game Paths?").arg(path)) !=
          QMessageBox::Yes)
        return;
    }
    settings.AddPath(path);
  }
}

QSize MainWindow::sizeHint() const
{
  return QSize(800, 600);
}

void MainWindow::OnBootGameCubeIPL(DiscIO::Region region)
{
  StartGame(std::make_unique<BootParameters>(BootParameters::IPL{region}));
}

void MainWindow::OnImportNANDBackup()
{
  auto response = QMessageBox::question(
      this, tr("Question"),
      tr("Merging a new NAND over your currently selected NAND will overwrite any channels "
         "and savegames that already exist. This process is not reversible, so it is "
         "recommended that you keep backups of both NANDs. Are you sure you want to "
         "continue?"));

  if (response == QMessageBox::No)
    return;

  QString file = QFileDialog::getOpenFileName(this, tr("Select the save file"), QDir::currentPath(),
                                              tr("BootMii NAND backup file (*.bin);;"
                                                 "All Files (*)"));

  if (file.isEmpty())
    return;

  QProgressDialog* dialog = new QProgressDialog(this);
  dialog->setMinimum(0);
  dialog->setMaximum(0);
  dialog->setLabelText(tr("Importing NAND backup"));
  dialog->setCancelButton(nullptr);

  auto beginning = QDateTime::currentDateTime().toMSecsSinceEpoch();

  auto result = std::async(std::launch::async, [&] {
    DiscIO::NANDImporter().ImportNANDBin(
        file.toStdString(),
        [&dialog, beginning] {
          QueueOnObject(dialog, [&dialog, beginning] {
            dialog->setLabelText(
                tr("Importing NAND backup\n Time elapsed: %1s")
                    .arg((QDateTime::currentDateTime().toMSecsSinceEpoch() - beginning) / 1000));
          });
        },
        [this] {
          return RunOnObject(this, [this] {
            return QFileDialog::getOpenFileName(this, tr("Select the keys file (OTP/SEEPROM dump)"),
                                                QDir::currentPath(),
                                                tr("BootMii keys file (*.bin);;"
                                                   "All Files (*)"))
                .toStdString();
          });
        });
    QueueOnObject(dialog, &QProgressDialog::close);
  });

  dialog->exec();

  result.wait();

  m_menu_bar->UpdateToolsMenu(Core::IsRunning());
}

void MainWindow::OnPlayRecording()
{
  QString dtm_file = QFileDialog::getOpenFileName(this, tr("Select the Recording File"), QString(),
                                                  tr("Dolphin TAS Movies (*.dtm)"));

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

  int controllers = 0;

  for (int i = 0; i < 4; i++)
  {
    if (SerialInterface::SIDevice_IsGCController(SConfig::GetInstance().m_SIDevice[i]))
      controllers |= (1 << i);

    if (g_wiimote_sources[i] != WIIMOTE_SRC_NONE)
      controllers |= (1 << (i + 4));
  }

  if (Movie::BeginRecordingInput(controllers))
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

  Movie::EndPlayInput(false);
  emit RecordingStatusChanged(true);
}

void MainWindow::OnExportRecording()
{
  bool was_paused = Core::GetState() == Core::State::Paused;

  if (was_paused)
    Core::SetState(Core::State::Paused);

  QString dtm_file = QFileDialog::getSaveFileName(this, tr("Select the Recording File"), QString(),
                                                  tr("Dolphin TAS Movies (*.dtm)"));

  if (was_paused)
    Core::SetState(Core::State::Running);

  if (dtm_file.isEmpty())
    return;

  Core::SetState(Core::State::Running);

  Movie::SaveRecording(dtm_file.toStdString());
}

void MainWindow::ShowTASInput()
{
  for (int i = 0; i < num_gc_controllers; i++)
  {
    if (SConfig::GetInstance().m_SIDevice[i] != SerialInterface::SIDEVICE_NONE &&
        SConfig::GetInstance().m_SIDevice[i] != SerialInterface::SIDEVICE_GC_GBA)
    {
      m_gc_tas_input_windows[i]->show();
      m_gc_tas_input_windows[i]->raise();
      m_gc_tas_input_windows[i]->activateWindow();
    }
  }

  for (int i = 0; i < num_wii_controllers; i++)
  {
    if (g_wiimote_sources[i] == WIIMOTE_SRC_EMU &&
        (!Core::IsRunning() || SConfig::GetInstance().bWii))
    {
      m_wii_tas_input_windows[i]->show();
      m_wii_tas_input_windows[i]->raise();
      m_wii_tas_input_windows[i]->activateWindow();
    }
  }
}

void MainWindow::OnConnectWiiRemote(int id)
{
  const auto ios = IOS::HLE::GetIOS();
  if (!ios || SConfig::GetInstance().m_bt_passthrough_enabled)
    return;
  Core::RunAsCPUThread([&] {
    const auto bt = std::static_pointer_cast<IOS::HLE::Device::BluetoothEmu>(
        ios->GetDeviceByName("/dev/usb/oh1/57e/305"));
    const bool is_connected = bt && bt->AccessWiiMote(id | 0x100)->IsConnected();
    Wiimote::Connect(id, !is_connected);
  });
}

void MainWindow::ShowMemcardManager()
{
  GCMemcardManager manager(this);

  manager.exec();
}

void MainWindow::OnUpdateProgressDialog(QString title, int progress, int total)
{
  if (!m_progress_dialog)
  {
    m_progress_dialog = new QProgressDialog(m_render_widget);
    m_progress_dialog->show();
  }

  m_progress_dialog->setValue(progress);
  m_progress_dialog->setLabelText(title);
  m_progress_dialog->setWindowTitle(title);
  m_progress_dialog->setMaximum(total);

  if (total < 0 || progress >= total)
  {
    m_progress_dialog->hide();
    m_progress_dialog->deleteLater();
    m_progress_dialog = nullptr;
  }
}
