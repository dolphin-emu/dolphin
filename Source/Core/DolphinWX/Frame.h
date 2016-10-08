// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>
#include <wx/bitmap.h>
#include <wx/frame.h>
#include <wx/image.h>
#include <wx/panel.h>
#include <wx/timer.h>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "DolphinWX/Globals.h"
#include "InputCommon/GCPadStatus.h"

#if defined(HAVE_X11) && HAVE_X11
#include "DolphinWX/X11Utils.h"
#endif

// Class declarations
class CGameListCtrl;
class CCodeWindow;
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
    return reinterpret_cast<void*>(X11Utils::XWindowFromHandle(m_RenderParent->GetHandle()));
#else
    return reinterpret_cast<void*>(m_RenderParent->GetHandle());
#endif
  }

  // These have to be public
  CCodeWindow* g_pCodeWindow = nullptr;
  NetPlaySetupFrame* g_NetPlaySetupDiag = nullptr;
  wxCheatsWindow* g_CheatsWindow = nullptr;
  TASInputDlg* g_TASInputDlg[8];

  void InitBitmaps();
  void DoPause();
  void DoStop();
  void OnStopped();
  void DoRecordingSave();
  void UpdateGUI();
  void UpdateGameList();
  void ToggleLogWindow(bool bShow);
  void ToggleLogConfigWindow(bool bShow);
  void PostEvent(wxCommandEvent& event);
  void StatusBarMessage(const char* Text, ...);
  void ClearStatusBar();
  void OnRenderWindowSizeRequest(int width, int height);
  void BootGame(const std::string& filename);
  void OnRenderParentClose(wxCloseEvent& event);
  void OnRenderParentMove(wxMoveEvent& event);
  bool RendererHasFocus();
  bool UIHasFocus();
  bool RendererIsFullscreen();
  void DoFullscreen(bool bF);
  void ToggleDisplayMode(bool bFullscreen);
  void UpdateWiiMenuChoice(wxMenuItem* WiiMenuItem = nullptr);
  void PopulateSavedPerspectives();
  static void ConnectWiimote(int wm_idx, bool connect);
  void UpdateTitle(const std::string& str);
  void OpenGeneralConfiguration(int tab = -1);

  const CGameListCtrl* GetGameListCtrl() const;
  wxMenuBar* GetMenuBar() const override;
  const wxSize& GetToolbarBitmapSize() const;  // Needed before the toolbar exists

#ifdef __WXGTK__
  Common::Event panic_event;
  bool bPanicResult;
  std::recursive_mutex keystate_lock;
#endif

#if defined(HAVE_XRANDR) && HAVE_XRANDR
  X11Utils::XRRConfiguration* m_XRRConfig;
#endif

  wxMenu* m_SavedPerspectives = nullptr;

  wxToolBar* m_ToolBar = nullptr;
  // AUI
  wxAuiManager* m_Mgr = nullptr;
  bool bFloatWindow[IDM_DEBUG_WINDOW_LIST_END - IDM_DEBUG_WINDOW_LIST_START] = {};

  // Perspectives (Should find a way to make all of this private)
  void DoAddPage(wxWindow* Win, int i, bool Float);
  void DoRemovePage(wxWindow*, bool bHide = true);
  struct SPerspectives
  {
    std::string Name;
    wxString Perspective;
    std::vector<int> Width, Height;
  };
  std::vector<SPerspectives> Perspectives;
  u32 ActivePerspective;

private:
  CGameListCtrl* m_GameListCtrl = nullptr;
  wxPanel* m_Panel = nullptr;
  CRenderFrame* m_RenderFrame = nullptr;
  wxWindow* m_RenderParent = nullptr;
  CLogWindow* m_LogWindow = nullptr;
  LogConfigWindow* m_LogConfigWindow = nullptr;
  FifoPlayerDlg* m_FifoPlayerDlg = nullptr;
  bool UseDebugger = false;
  bool m_bBatchMode = false;
  bool m_bEdit = false;
  bool m_bTabSplit = false;
  bool m_bNoDocking = false;
  bool m_bGameLoading = false;
  bool m_bClosing = false;
  bool m_confirmStop = false;
  bool m_tried_graceful_shutdown = false;
  int m_saveSlot = 1;

  std::vector<std::string> drives;

  enum EToolbar
  {
    Toolbar_FileOpen,
    Toolbar_Refresh,
    Toolbar_Play,
    Toolbar_Stop,
    Toolbar_Pause,
    Toolbar_Screenshot,
    Toolbar_FullScreen,
    Toolbar_ConfigMain,
    Toolbar_ConfigGFX,
    Toolbar_Controller,
    EToolbar_Max
  };

  enum
  {
    ADD_PANE_TOP,
    ADD_PANE_BOTTOM,
    ADD_PANE_LEFT,
    ADD_PANE_RIGHT,
    ADD_PANE_CENTER
  };

  wxTimer m_poll_hotkey_timer;
  wxTimer m_handle_signal_timer;

  wxSize m_toolbar_bitmap_size;
  wxBitmap m_Bitmaps[EToolbar_Max];

  wxMenuBar* m_menubar_shadow = nullptr;

  void PopulateToolbar(wxToolBar* toolBar);
  void RecreateToolbar();
  wxMenuBar* CreateMenu();

  // Utility
  wxString GetMenuLabel(int Id);
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
  // Float window
  void DoUnfloatPage(int Id);
  void OnFloatingPageClosed(wxCloseEvent& event);
  void DoFloatNotebookPage(wxWindowID Id);
  wxFrame* CreateParentFrame(wxWindowID Id = wxID_ANY, const wxString& title = "",
                             wxWindow* = nullptr);
  wxString AuiFullscreen, AuiCurrent;
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
  // Event functions
  void OnQuit(wxCommandEvent& event);
  void OnHelp(wxCommandEvent& event);

  void OnOpen(wxCommandEvent& event);  // File menu
  void DoOpen(bool Boot);
  void OnRefresh(wxCommandEvent& event);
  void OnBootDrive(wxCommandEvent& event);

  void OnPlay(wxCommandEvent& event);  // Emulation
  void OnStop(wxCommandEvent& event);
  void OnReset(wxCommandEvent& event);
  void OnRecord(wxCommandEvent& event);
  void OnPlayRecording(wxCommandEvent& event);
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
  void OnToggleDualCore(wxCommandEvent& event);
  void OnManagerResize(wxAuiManagerEvent& event);
  void OnMove(wxMoveEvent& event);
  void OnResize(wxSizeEvent& event);
  void OnToggleToolbar(wxCommandEvent& event);
  void DoToggleToolbar(bool);
  void OnToggleStatusbar(wxCommandEvent& event);
  void OnToggleWindow(wxCommandEvent& event);

  void OnKeyDown(wxKeyEvent& event);  // Keyboard
  void OnMouse(wxMouseEvent& event);  // Mouse

  void OnFocusChange(wxFocusEvent& event);

  void OnHostMessage(wxCommandEvent& event);

  void OnMemcard(wxCommandEvent& event);  // Misc
  void OnImportSave(wxCommandEvent& event);
  void OnExportAllSaves(wxCommandEvent& event);

  void OnNetPlay(wxCommandEvent& event);

  void OnShowCheatsWindow(wxCommandEvent& event);
  void OnLoadWiiMenu(wxCommandEvent& event);
  void OnInstallWAD(wxCommandEvent& event);
  void OnFifoPlayer(wxCommandEvent& event);
  void OnConnectWiimote(wxCommandEvent& event);
  void GameListChanged(wxCommandEvent& event);

  void OnGameListCtrlItemActivated(wxListEvent& event);
  void OnRenderParentResize(wxSizeEvent& event);
  void StartGame(const std::string& filename);
  void OnChangeColumnsVisible(wxCommandEvent& event);

  void OnSelectSlot(wxCommandEvent& event);
  void OnSaveCurrentSlot(wxCommandEvent& event);
  void OnLoadCurrentSlot(wxCommandEvent& event);

  void PollHotkeys(wxTimerEvent&);
  void ParseHotkeys();
  void HandleSignal(wxTimerEvent&);

  bool InitControllers();

  // OSD
  void OSDPrintInternalResolution();
  void OSDPrintAspectRatio();
  void OSDPrintEFB();
  void OSDPrintFog();
  void OSDPrintEmulationSpeed();

  // Event table
  DECLARE_EVENT_TABLE();
};

int GetCmdForHotkey(unsigned int key);

void OnAfterLoadCallback();
void OnStoppedCallback();

// For TASInputDlg
void GCTASManipFunction(GCPadStatus* PadStatus, int controllerID);
void WiiTASManipFunction(u8* data, WiimoteEmu::ReportFeatures rptf, int controllerID, int ext,
                         const wiimote_key key);
