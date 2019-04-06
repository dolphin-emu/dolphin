// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QMainWindow>
#include <QStringList>

#include <memory>
#include <optional>
#include <string>

class QProgressDialog;
class QStackedWidget;
class QString;

class BreakpointWidget;
struct BootParameters;
class CheatsManager;
class CodeWidget;
class ControllersWindow;
class DiscordHandler;
class DragEnterEvent;
class FIFOPlayerWindow;
class GameList;
class GCTASInputWindow;
class GraphicsWindow;
class HotkeyScheduler;
class JITWidget;
class LogConfigWidget;
class LogWidget;
class MappingWindow;
class MemoryWidget;
class MenuBar;
class NetPlayDialog;
class NetPlaySetupDialog;
class RegisterWidget;
class RenderWidget;
class SearchBar;
class SettingsWindow;
class ToolBar;
class WatchWidget;
class WiiTASInputWindow;

namespace DiscIO
{
enum class Region;
}

namespace UICommon
{
class GameFile;
}

namespace X11Utils
{
class XRRConfiguration;
}

class MainWindow final : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(std::unique_ptr<BootParameters> boot_parameters,
                      const std::string& movie_path);
  ~MainWindow();

  void Show();

  bool eventFilter(QObject* object, QEvent* event) override;

signals:
  void ReadOnlyModeChanged(bool read_only);
  void RecordingStatusChanged(bool recording);

private:
  void Open();
  void RefreshGameList();
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
  void StateLoadLastSavedAt(int slot);
  void StateLoadUndo();
  void StateSaveUndo();
  void StateSaveOldest();
  void SetStateSlot(int slot);
  void BootWiiSystemMenu();

  void PerformOnlineUpdate(const std::string& region);

  void SetFullScreenResolution(bool fullscreen);

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

  enum class ScanForSecondDisc
  {
    Yes,
    No,
  };

  void ScanForSecondDiscAndStartGame(const UICommon::GameFile& game,
                                     const std::optional<std::string>& savestate_path = {});
  void StartGame(const QString& path, ScanForSecondDisc scan,
                 const std::optional<std::string>& savestate_path = {});
  void StartGame(const std::string& path, ScanForSecondDisc scan,
                 const std::optional<std::string>& savestate_path = {});
  void StartGame(const std::vector<std::string>& paths,
                 const std::optional<std::string>& savestate_path = {});
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
  void ShowNetPlayBrowser();
  void ShowFIFOPlayer();
  void ShowMemcardManager();
  void ShowResourcePackManager();
  void ShowCheatsManager();

  void NetPlayInit();
  bool NetPlayJoin();
  bool NetPlayHost(const QString& game_id);
  void NetPlayQuit();

  void OnBootGameCubeIPL(DiscIO::Region region);
  void OnImportNANDBackup();
  void OnConnectWiiRemote(int id);
  void OnSignal();

  void OnUpdateProgressDialog(QString label, int progress, int total);

  void OnPlayRecording();
  void OnStartRecording();
  void OnStopRecording();
  void OnExportRecording();
  void OnActivateChat();
  void OnRequestGolfControl();
  void ShowTASInput();

  void ChangeDisc();
  void EjectDisc();

  QStringList PromptFileNames();

  void EnableScreenSaver(bool enable);

  void OnStopComplete();
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  QSize sizeHint() const override;

#if defined(HAVE_XRANDR) && HAVE_XRANDR
  std::unique_ptr<X11Utils::XRRConfiguration> m_xrr_config;
#endif

  QProgressDialog* m_progress_dialog = nullptr;
  QStackedWidget* m_stack;
  ToolBar* m_tool_bar;
  MenuBar* m_menu_bar;
  SearchBar* m_search_bar;
  GameList* m_game_list;
  RenderWidget* m_render_widget = nullptr;
  bool m_rendering_to_main;
  bool m_stop_requested = false;
  bool m_exit_requested = false;
  bool m_fullscreen_requested = false;
  int m_state_slot = 1;
  std::unique_ptr<BootParameters> m_pending_boot;

  ControllersWindow* m_controllers_window = nullptr;
  SettingsWindow* m_settings_window = nullptr;
  GraphicsWindow* m_graphics_window = nullptr;
  FIFOPlayerWindow* m_fifo_window = nullptr;
  MappingWindow* m_hotkey_window = nullptr;

  HotkeyScheduler* m_hotkey_scheduler;
  NetPlayDialog* m_netplay_dialog;
  DiscordHandler* m_netplay_discord;
  NetPlaySetupDialog* m_netplay_setup_dialog;
  static constexpr int num_gc_controllers = 4;
  std::array<GCTASInputWindow*, num_gc_controllers> m_gc_tas_input_windows{};
  static constexpr int num_wii_controllers = 4;
  std::array<WiiTASInputWindow*, num_wii_controllers> m_wii_tas_input_windows{};

  BreakpointWidget* m_breakpoint_widget;
  CodeWidget* m_code_widget;
  JITWidget* m_jit_widget;
  LogWidget* m_log_widget;
  LogConfigWidget* m_log_config_widget;
  MemoryWidget* m_memory_widget;
  RegisterWidget* m_register_widget;
  WatchWidget* m_watch_widget;
  CheatsManager* m_cheats_manager;
  QByteArray m_render_widget_geometry;
};
