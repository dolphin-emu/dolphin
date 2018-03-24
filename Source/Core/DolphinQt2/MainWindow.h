// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QString>
#include <QToolBar>

#include <memory>
#include <optional>
#include <string>

#include "DolphinQt2/GameList/GameList.h"
#include "DolphinQt2/MenuBar.h"
#include "DolphinQt2/RenderWidget.h"
#include "DolphinQt2/ToolBar.h"

class QProgressDialog;

class BreakpointWidget;
struct BootParameters;
class CodeWidget;
class ControllersWindow;
class DragEnterEvent;
class FIFOPlayerWindow;
class GCTASInputWindow;
class GraphicsWindow;
class HotkeyScheduler;
class LogConfigWidget;
class LogWidget;
class MappingWindow;
class NetPlayClient;
class NetPlayDialog;
class NetPlayServer;
class NetPlaySetupDialog;
class RegisterWidget;
class SearchBar;
class SettingsWindow;
class WatchWidget;
class WiiTASInputWindow;

class MainWindow final : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(std::unique_ptr<BootParameters> boot_parameters);
  ~MainWindow();

  bool eventFilter(QObject* object, QEvent* event) override;

signals:
  void ReadOnlyModeChanged(bool read_only);
  void RecordingStatusChanged(bool recording);

private:
  void Open();
  void Play(const std::optional<std::string>& savestate_path = {});
  void Pause();
  void TogglePause();

  // May ask for confirmation. Returns whether or not it actually stopped.
  bool RequestStop();
  void ForceStop();
  void Reset();
  void FrameAdvance();
  void StateLoad();
  void StateSave();
  void StateLoadSlot();
  void StateSaveSlot();
  void StateLoadSlotAt(int slot);
  void StateSaveSlotAt(int slot);
  void StateLoadUndo();
  void StateSaveUndo();
  void StateSaveOldest();
  void SetStateSlot(int slot);
  void BootWiiSystemMenu();

  void PerformOnlineUpdate(const std::string& region);

  void FullScreen();
  void ScreenShot();

  void CreateComponents();

  void ConnectGameList();
  void ConnectHost();
  void ConnectHotkeys();
  void ConnectMenuBar();
  void ConnectRenderWidget();
  void ConnectStack();
  void ConnectToolBar();

  void InitControllers();
  void ShutdownControllers();

  void InitCoreCallbacks();

  void StartGame(const QString& path, const std::optional<std::string>& savestate_path = {});
  void StartGame(const std::string& path, const std::optional<std::string>& savestate_path = {});
  void StartGame(std::unique_ptr<BootParameters>&& parameters);
  void ShowRenderWidget();
  void HideRenderWidget(bool reinit = true);

  void ShowSettingsWindow();
  void ShowGeneralWindow();
  void ShowAudioWindow();
  void ShowControllersWindow();
  void ShowGraphicsWindow();
  void ShowAboutDialog();
  void ShowHotkeyDialog();
  void ShowNetPlaySetupDialog();
  void ShowFIFOPlayer();
  void ShowMemcardManager();

  void NetPlayInit();
  bool NetPlayJoin();
  bool NetPlayHost(const QString& game_id);
  void NetPlayQuit();

  void OnBootGameCubeIPL(DiscIO::Region region);
  void OnImportNANDBackup();
  void OnConnectWiiRemote(int id);

  void OnUpdateProgressDialog(QString label, int progress, int total);

  void OnPlayRecording();
  void OnStartRecording();
  void OnStopRecording();
  void OnExportRecording();
  void ShowTASInput();

  void ChangeDisc();
  void EjectDisc();

  QString PromptFileName();

  void EnableScreenSaver(bool enable);

  void OnStopComplete();
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  QSize sizeHint() const override;

  QProgressDialog* m_progress_dialog = nullptr;
  QStackedWidget* m_stack;
  ToolBar* m_tool_bar;
  MenuBar* m_menu_bar;
  SearchBar* m_search_bar;
  GameList* m_game_list;
  RenderWidget* m_render_widget;
  bool m_rendering_to_main;
  bool m_stop_requested = false;
  bool m_exit_requested = false;
  int m_state_slot = 1;
  std::unique_ptr<BootParameters> m_pending_boot;

  HotkeyScheduler* m_hotkey_scheduler;
  ControllersWindow* m_controllers_window;
  SettingsWindow* m_settings_window;
  MappingWindow* m_hotkey_window;
  NetPlayDialog* m_netplay_dialog;
  NetPlaySetupDialog* m_netplay_setup_dialog;
  GraphicsWindow* m_graphics_window;
  static constexpr int num_gc_controllers = 4;
  std::array<GCTASInputWindow*, num_gc_controllers> m_gc_tas_input_windows{};
  static constexpr int num_wii_controllers = 4;
  std::array<WiiTASInputWindow*, num_wii_controllers> m_wii_tas_input_windows{};

  BreakpointWidget* m_breakpoint_widget;
  CodeWidget* m_code_widget;
  LogWidget* m_log_widget;
  LogConfigWidget* m_log_config_widget;
  FIFOPlayerWindow* m_fifo_window;
  RegisterWidget* m_register_widget;
  WatchWidget* m_watch_widget;
};
