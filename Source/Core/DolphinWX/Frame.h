// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <wx/bitmap.h>
#include <wx/frame.h>
#include <wx/image.h>
#include <wx/panel.h>
#include <wx/string.h>
#include <wx/timer.h>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Core/ConfigManager.h"
#include "DolphinWX/Globals.h"

#if defined(HAVE_X11) && HAVE_X11
#include "UICommon/X11Utils.h"
#endif

#ifdef __APPLE__
#include <IOKit/pwr_mgt/IOPMLib.h>
#endif

struct BootParameters;

// Class declarations
class GameListCtrl;
class CCodeWindow;
class CConfigMain;
class CLogWindow;
class FifoPlayerDlg;
class LogConfigWindow;
class NetPlaySetupFrame;
class TASInputDlg;
class wxCheatsWindow;

class wxAuiManager;
class wxAuiManagerEvent;
class wxAuiNotebook;
class wxAuiNotebookEvent;
class wxListEvent;
class wxMenuItem;
class wxProgressDialog;

class CRenderFrame : public wxFrame
{
public:
  CRenderFrame(wxFrame* parent, wxWindowID id = wxID_ANY, const wxString& title = "Dolphin",
               const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
               long style = wxDEFAULT_FRAME_STYLE);

  bool ShowFullScreen(bool show, long style = wxFULLSCREEN_ALL) override;

private:
  void OnDropFiles(wxDropFilesEvent& event);
  static bool IsValidSavestateDropped(const std::string& filepath);
#ifdef _WIN32
  // Receive WndProc messages
  WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);
#endif
};

wxDECLARE_EVENT(DOLPHIN_EVT_RELOAD_THEME_BITMAPS, wxCommandEvent);
wxDECLARE_EVENT(DOLPHIN_EVT_UPDATE_LOAD_WII_MENU_ITEM, wxCommandEvent);
wxDECLARE_EVENT(DOLPHIN_EVT_BOOT_SOFTWARE, wxCommandEvent);
wxDECLARE_EVENT(DOLPHIN_EVT_STOP_SOFTWARE, wxCommandEvent);

class CFrame : public CRenderFrame
{
public:
  CFrame(wxFrame* parent, wxWindowID id = wxID_ANY, const wxString& title = "Dolphin",
         wxRect geometry = wxDefaultSize, bool use_debugger = false, bool batch_mode = false,
         bool show_log_window = false,
         long style = wxDEFAULT_FRAME_STYLE | wxNO_FULL_REPAINT_ON_RESIZE);

  virtual ~CFrame();

  void* GetRenderHandle()
  {
#if defined(HAVE_X11) && HAVE_X11
    return reinterpret_cast<void*>(X11Utils::XWindowFromHandle(m_render_parent->GetHandle()));
#else
    return reinterpret_cast<void*>(m_render_parent->GetHandle());
#endif
  }

  // These have to be public
  CCodeWindow* m_code_window = nullptr;
  NetPlaySetupFrame* m_netplay_setup_frame = nullptr;

  void DoStop();
  void UpdateGUI();
  void GameListRefresh();
  void GameListRescan(bool purge_cache = false);
  void ToggleLogWindow(bool bShow);
  void ToggleLogConfigWindow(bool bShow);
  void StatusBarMessage(const char* format, ...);
  void ClearStatusBar();
  void BootGame(const std::string& filename);
  bool RendererHasFocus();
  bool RendererIsFullscreen();
  void OpenGeneralConfiguration(wxWindowID tab_id = wxID_ANY);

  wxMenuBar* GetMenuBar() const override;

  Common::Event m_panic_event;
  bool m_panic_result;

#if defined(HAVE_XRANDR) && HAVE_XRANDR
  X11Utils::XRRConfiguration* m_xrr_config;
#endif

  // AUI
  wxAuiManager* m_mgr = nullptr;
  bool m_float_window[IDM_DEBUG_WINDOW_LIST_END - IDM_DEBUG_WINDOW_LIST_START] = {};

  // Perspectives (Should find a way to make all of this private)
  void DoAddPage(wxWindow* Win, int i, bool Float);
  void DoRemovePage(wxWindow*, bool bHide = true);
  struct SPerspectives
  {
    std::string name;
    wxString perspective;
    std::vector<int> width, height;
  };
  std::vector<SPerspectives> m_perspectives;
  u32 m_active_perspective;

private:
  enum
  {
    ADD_PANE_TOP,
    ADD_PANE_BOTTOM,
    ADD_PANE_LEFT,
    ADD_PANE_RIGHT,
    ADD_PANE_CENTER
  };

  static constexpr int MOUSE_HIDE_DELAY = 3000;
  GameListCtrl* m_game_list_ctrl = nullptr;
  CConfigMain* m_main_config_dialog = nullptr;
  wxPanel* m_panel = nullptr;
  CRenderFrame* m_render_frame = nullptr;
  wxWindow* m_render_parent = nullptr;
  CLogWindow* m_log_window = nullptr;
  LogConfigWindow* m_log_config_window = nullptr;
  FifoPlayerDlg* m_fifo_player_dialog = nullptr;
  std::array<TASInputDlg*, 8> m_tas_input_dialogs{};
  wxCheatsWindow* m_cheats_window = nullptr;
  wxProgressDialog* m_progress_dialog = nullptr;
  bool m_use_debugger = false;
  bool m_batch_mode = false;
  bool m_editing_perspectives = false;
  bool m_is_split_tab_notebook = false;
  bool m_no_panel_docking = false;
  bool m_is_game_loading = false;
  bool m_is_closing = false;
  bool m_renderer_has_focus = false;
  bool m_confirm_stop = false;
  bool m_tried_graceful_shutdown = false;
  int m_save_slot = 1;

  wxTimer m_poll_hotkey_timer;
  wxTimer m_cursor_timer;
  wxTimer m_handle_signal_timer;

  wxMenuBar* m_menubar_shadow = nullptr;

  wxString m_aui_fullscreen_perspective;
  wxString m_aui_current_perspective;

#ifdef __WXGTK__
  std::recursive_mutex m_keystate_lock;
#endif

  void BindEvents();
  void BindMenuBarEvents();
  void BindDebuggerMenuBarEvents();
  void BindDebuggerMenuBarUpdateEvents();

  wxToolBar* OnCreateToolBar(long style, wxWindowID id, const wxString& name) override;
  wxMenuBar* CreateMenuBar() const;

  void InitializeTASDialogs();
  void InitializeCoreCallbacks();

  void StartGame(std::unique_ptr<BootParameters> boot);
  void SetDebuggerStartupParameters() const;

  // Utility
  wxWindow* GetNotebookPageFromId(wxWindowID Id);
  wxAuiNotebook* GetNotebookFromId(u32 NBId);
  int GetNotebookCount();
  wxAuiNotebook* CreateEmptyNotebook();
  void HandleFrameSkipHotkeys();

  // Perspectives
  void AddRemoveBlankPage();
  void OnNotebookAllowDnD(wxAuiNotebookEvent& event);
  void OnNotebookPageChanged(wxAuiNotebookEvent& event);
  void OnNotebookPageClose(wxAuiNotebookEvent& event);
  void OnNotebookTabRightUp(wxAuiNotebookEvent& event);
  void OnFloatWindow(wxCommandEvent& event);
  void ToggleFloatWindow(int Id);
  int GetNotebookAffiliation(wxWindowID Id);
  void ClosePages();
  void CloseAllNotebooks();
  void ShowResizePane();
  void TogglePane();
  void SetPaneSize();
  void TogglePaneStyle(bool On, int EventId);
  void ToggleNotebookStyle(bool On, long Style);
  void PopulateSavedPerspectives();

  // Float window
  void DoUnfloatPage(int Id);
  void OnFloatingPageClosed(wxCloseEvent& event);
  void DoFloatNotebookPage(wxWindowID Id);
  wxFrame* CreateParentFrame(wxWindowID Id = wxID_ANY, const wxString& title = "",
                             wxWindow* = nullptr);

  void AddPane(int dir);
  void UpdateCurrentPerspective();
  void SaveIniPerspectives();
  void LoadIniPerspectives();
  void OnPaneClose(wxAuiManagerEvent& evt);
  void ReloadPanes();
  void DoLoadPerspective();
  void OnPerspectiveMenu(wxCommandEvent& event);
  void OnSelectPerspective(wxCommandEvent& event);

#ifdef _WIN32
  // Override window proc for tricks like screensaver disabling
  WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);
#endif

// Screensaver
#ifdef __APPLE__
  IOPMAssertionID m_power_assertion = kIOPMNullAssertionID;
#endif
  void InhibitScreensaver();
  void UninhibitScreensaver();

  void DoOpen(bool Boot);
  void DoPause();
  void DoToggleToolbar(bool);
  void DoRecordingSave();
  void DoFullscreen(bool enable_fullscreen);
  void DoExclusiveFullscreen(bool enable_fullscreen);
  void ToggleDisplayMode(bool bFullscreen);
  void OnStopped();
  void OnRenderWindowSizeRequest(int width, int height);
  void UpdateTitle(const wxString& str);

  // Event functions
  void PostEvent(wxCommandEvent& event);
  void OnRenderParentClose(wxCloseEvent& event);
  void OnRenderParentMove(wxMoveEvent& event);

  void OnQuit(wxCommandEvent& event);
  void OnHelp(wxCommandEvent& event);

  void OnReloadThemeBitmaps(wxCommandEvent& event);
  void OnRefreshGameList(wxCommandEvent& event);
  void OnRescanGameList(wxCommandEvent& event);

  void OnUpdateInterpreterMenuItem(wxUpdateUIEvent& event);

  void OnUpdateLoadWiiMenuItem(wxCommandEvent&);
  void UpdateLoadWiiMenuItem() const;

  void OnOpen(wxCommandEvent& event);  // File menu
  void OnRefresh(wxCommandEvent& event);
  void OnBootDrive(wxCommandEvent& event);

  void OnPlay(wxCommandEvent& event);  // Emulation
  void OnStop(wxCommandEvent& event);
  void OnReset(wxCommandEvent& event);
  void OnRecord(wxCommandEvent& event);
  void OnPlayRecording(wxCommandEvent& event);
  void OnStopRecording(wxCommandEvent& event);
  void OnRecordExport(wxCommandEvent& event);
  void OnRecordReadOnly(wxCommandEvent& event);
  void OnTASInput(wxCommandEvent& event);
  void OnTogglePauseMovie(wxCommandEvent& event);
  void OnToggleDumpFrames(wxCommandEvent& event);
  void OnToggleDumpAudio(wxCommandEvent& event);
  void OnShowLag(wxCommandEvent& event);
  void OnShowFrameCount(wxCommandEvent& event);
  void OnShowInputDisplay(wxCommandEvent& event);
  void OnShowRTCDisplay(wxCommandEvent& event);
  void OnChangeDisc(wxCommandEvent& event);
  void OnScreenshot(wxCommandEvent& event);
  void OnActive(wxActivateEvent& event);
  void OnClose(wxCloseEvent& event);
  void OnLoadState(wxCommandEvent& event);
  void OnSaveState(wxCommandEvent& event);
  void OnLoadStateFromFile(wxCommandEvent& event);
  void OnSaveStateToFile(wxCommandEvent& event);
  void OnLoadLastState(wxCommandEvent& event);
  void OnSaveFirstState(wxCommandEvent& event);
  void OnUndoLoadState(wxCommandEvent& event);
  void OnUndoSaveState(wxCommandEvent& event);

  void OnFrameStep(wxCommandEvent& event);

  void OnConfigMain(wxCommandEvent& event);  // Options
  void OnConfigGFX(wxCommandEvent& event);
  void OnConfigAudio(wxCommandEvent& event);
  void OnConfigControllers(wxCommandEvent& event);
  void OnConfigHotkey(wxCommandEvent& event);

  void OnToggleFullscreen(wxCommandEvent& event);
  void OnManagerResize(wxAuiManagerEvent& event);
  void OnMove(wxMoveEvent& event);
  void OnResize(wxSizeEvent& event);
  void OnToggleToolbar(wxCommandEvent& event);
  void OnToggleStatusbar(wxCommandEvent& event);
  void OnToggleWindow(wxCommandEvent& event);

  void OnKeyDown(wxKeyEvent& event);  // Keyboard
  void OnMouse(wxMouseEvent& event);  // Mouse

  void OnHostMessage(wxCommandEvent& event);

  void OnMemcard(wxCommandEvent& event);  // Misc
  void OnImportSave(wxCommandEvent& event);
  void OnExportAllSaves(wxCommandEvent& event);

  void OnLoadGameCubeIPLJAP(wxCommandEvent& event);
  void OnLoadGameCubeIPLUSA(wxCommandEvent& event);
  void OnLoadGameCubeIPLEUR(wxCommandEvent& event);

  void OnNetPlay(wxCommandEvent& event);

  void OnShowCheatsWindow(wxCommandEvent& event);
  void OnLoadWiiMenu(wxCommandEvent& event);
  void OnInstallWAD(wxCommandEvent& event);
  void OnUninstallWAD(wxCommandEvent& event);
  void OnImportBootMiiBackup(wxCommandEvent& event);
  void OnExtractCertificates(wxCommandEvent& event);
  void OnPerformOnlineWiiUpdate(wxCommandEvent& event);
  void OnPerformDiscWiiUpdate(wxCommandEvent& event);
  void OnFifoPlayer(wxCommandEvent& event);
  void OnConnectWiimote(wxCommandEvent& event);
  void GameListChanged(wxCommandEvent& event);

  void OnGameListCtrlItemActivated(wxListEvent& event);
  void OnRenderParentResize(wxSizeEvent& event);
  void OnChangeColumnsVisible(wxCommandEvent& event);

  void OnSelectSlot(wxCommandEvent& event);
  void OnSaveCurrentSlot(wxCommandEvent& event);
  void OnLoadCurrentSlot(wxCommandEvent& event);

  void PollHotkeys(wxTimerEvent&);
  void ParseHotkeys();
  void HandleCursorTimer(wxTimerEvent&);
  void HandleSignal(wxTimerEvent&);

  bool InitControllers();

  // Event table
  DECLARE_EVENT_TABLE()
};
