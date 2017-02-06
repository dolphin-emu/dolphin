// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Frame.h"

#include <atomic>
#include <cstddef>
#include <fstream>
#include <string>
#include <utility>
#include <vector>
#include <wx/aui/auibook.h>
#include <wx/aui/framemanager.h>
#include <wx/filename.h>
#include <wx/frame.h>
#include <wx/icon.h>
#include <wx/listbase.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/statusbr.h>
#include <wx/textctrl.h>
#include <wx/thread.h>
#include <wx/toolbar.h>

#include "DolphinWX/Config/ConfigMain.h"
#include "DolphinWX/Debugger/BreakpointDlg.h"
#include "DolphinWX/Debugger/CodeWindow.h"
#include "DolphinWX/Debugger/MemoryCheckDlg.h"
#include "DolphinWX/GameListCtrl.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/LogWindow.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/TASInputDlg.h"
#include "DolphinWX/WxUtils.h"

#if defined(__unix__) || defined(__unix) || defined(__APPLE__)
#include <signal.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

#include "AudioCommon/AudioCommon.h"

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Flag.h"
#include "Common/Logging/ConsoleListener.h"
#include "Common/Thread.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/Wiimote.h"
#include "Core/HotkeyManager.h"
#include "Core/IOS/IPC.h"
#include "Core/IOS/USB/Bluetooth/BTBase.h"
#include "Core/Movie.h"
#include "Core/State.h"

#include "InputCommon/GCPadStatus.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

#if defined(HAVE_X11) && HAVE_X11

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

// X11Utils nastiness that's only used here
namespace X11Utils
{
Window XWindowFromHandle(void* Handle)
{
  return GDK_WINDOW_XID(gtk_widget_get_window(GTK_WIDGET(Handle)));
}

Display* XDisplayFromHandle(void* Handle)
{
  return GDK_WINDOW_XDISPLAY(gtk_widget_get_window(GTK_WIDGET(Handle)));
}
}
#endif

CRenderFrame::CRenderFrame(wxFrame* parent, wxWindowID id, const wxString& title,
                           const wxPoint& pos, const wxSize& size, long style)
    : wxFrame(parent, id, title, pos, size, style)
{
  // Give it an icon
  SetIcons(WxUtils::GetDolphinIconBundle());

  DragAcceptFiles(true);
  Bind(wxEVT_DROP_FILES, &CRenderFrame::OnDropFiles, this);
}

void CRenderFrame::OnDropFiles(wxDropFilesEvent& event)
{
  if (event.GetNumberOfFiles() != 1)
    return;
  if (File::IsDirectory(WxStrToStr(event.GetFiles()[0])))
    return;

  wxFileName file = event.GetFiles()[0];
  const std::string filepath = WxStrToStr(file.GetFullPath());

  if (file.GetExt() == "dtm")
  {
    if (Core::IsRunning())
      return;

    if (!Movie::IsReadOnly())
    {
      // let's make the read-only flag consistent at the start of a movie.
      Movie::SetReadOnly(true);
      main_frame->GetMenuBar()->FindItem(IDM_RECORD_READ_ONLY)->Check(true);
    }

    if (Movie::PlayInput(filepath))
      main_frame->BootGame("");
  }
  else if (!Core::IsRunning())
  {
    main_frame->BootGame(filepath);
  }
  else if (IsValidSavestateDropped(filepath) && Core::IsRunning())
  {
    State::LoadAs(filepath);
  }
  else
  {
    DVDInterface::ChangeDiscAsHost(filepath);
  }
}

bool CRenderFrame::IsValidSavestateDropped(const std::string& filepath)
{
  const int game_id_length = 6;
  std::ifstream file(filepath, std::ios::in | std::ios::binary);

  if (!file)
    return false;

  std::string internal_game_id(game_id_length, ' ');
  file.read(&internal_game_id[0], game_id_length);

  return internal_game_id == SConfig::GetInstance().GetGameID();
}

#ifdef _WIN32
WXLRESULT CRenderFrame::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
  switch (nMsg)
  {
  case WM_SYSCOMMAND:
    switch (wParam)
    {
    case SC_SCREENSAVE:
    case SC_MONITORPOWER:
      if (Core::GetState() == Core::State::Running && SConfig::GetInstance().bDisableScreenSaver)
        break;
    default:
      return wxFrame::MSWWindowProc(nMsg, wParam, lParam);
    }
    break;

  case WM_USER:
    switch (wParam)
    {
    case WM_USER_STOP:
      main_frame->DoStop();
      break;

    case WM_USER_SETCURSOR:
      if (SConfig::GetInstance().bHideCursor && main_frame->RendererHasFocus() &&
          Core::GetState() == Core::State::Running)
        SetCursor(wxCURSOR_BLANK);
      else
        SetCursor(wxNullCursor);
      break;
    }
    break;

  case WM_CLOSE:
    // Let Core finish initializing before accepting any WM_CLOSE messages
    if (!Core::IsRunning())
      break;
  // Use default action otherwise

  default:
    // By default let wxWidgets do what it normally does with this event
    return wxFrame::MSWWindowProc(nMsg, wParam, lParam);
  }
  return 0;
}
#endif

bool CRenderFrame::ShowFullScreen(bool show, long style)
{
#if defined WIN32
  if (show && !g_Config.bBorderlessFullscreen)
  {
    // OpenGL requires the pop-up style to activate exclusive mode.
    SetWindowStyle((GetWindowStyle() & ~wxDEFAULT_FRAME_STYLE) | wxPOPUP_WINDOW);
  }
#endif

  bool result = wxTopLevelWindow::ShowFullScreen(show, style);

#if defined WIN32
  if (!show)
  {
    // Restore the default style.
    SetWindowStyle((GetWindowStyle() & ~wxPOPUP_WINDOW) | wxDEFAULT_FRAME_STYLE);
  }
#endif

  return result;
}

wxDEFINE_EVENT(wxEVT_HOST_COMMAND, wxCommandEvent);
wxDEFINE_EVENT(DOLPHIN_EVT_LOCAL_INI_CHANGED, wxCommandEvent);
wxDEFINE_EVENT(DOLPHIN_EVT_RELOAD_THEME_BITMAPS, wxCommandEvent);
wxDEFINE_EVENT(DOLPHIN_EVT_UPDATE_LOAD_WII_MENU_ITEM, wxCommandEvent);
wxDEFINE_EVENT(DOLPHIN_EVT_BOOT_SOFTWARE, wxCommandEvent);
wxDEFINE_EVENT(DOLPHIN_EVT_STOP_SOFTWARE, wxCommandEvent);

// Event tables
BEGIN_EVENT_TABLE(CFrame, CRenderFrame)

// Debugger pane context menu
EVT_MENU_RANGE(IDM_FLOAT_LOG_WINDOW, IDM_FLOAT_CODE_WINDOW, CFrame::OnFloatWindow)

// Game list context menu
EVT_MENU(IDM_LIST_INSTALL_WAD, CFrame::OnInstallWAD)

// Other
EVT_ACTIVATE(CFrame::OnActive)
EVT_CLOSE(CFrame::OnClose)
EVT_SIZE(CFrame::OnResize)
EVT_MOVE(CFrame::OnMove)
EVT_HOST_COMMAND(wxID_ANY, CFrame::OnHostMessage)

EVT_AUI_PANE_CLOSE(CFrame::OnPaneClose)

// Post events to child panels
EVT_MENU_RANGE(IDM_INTERPRETER, IDM_ADDRBOX, CFrame::PostEvent)

END_EVENT_TABLE()

// ---------------
// Creation and close, quit functions

bool CFrame::InitControllers()
{
  if (!g_controller_interface.IsInit())
  {
#if defined(HAVE_X11) && HAVE_X11
    void* win = reinterpret_cast<void*>(X11Utils::XWindowFromHandle(GetHandle()));
#else
    void* win = reinterpret_cast<void*>(GetHandle());
#endif
    g_controller_interface.Initialize(win);
    Pad::Initialize();
    Keyboard::Initialize();
    Wiimote::Initialize(Wiimote::InitializeMode::DO_NOT_WAIT_FOR_WIIMOTES);
    HotkeyManagerEmu::Initialize();

    return true;
  }
  return false;
}

static Common::Flag s_shutdown_signal_received;

#ifdef _WIN32
static BOOL WINAPI s_ctrl_handler(DWORD fdwCtrlType)
{
  switch (fdwCtrlType)
  {
  case CTRL_C_EVENT:
  case CTRL_BREAK_EVENT:
  case CTRL_CLOSE_EVENT:
  case CTRL_LOGOFF_EVENT:
  case CTRL_SHUTDOWN_EVENT:
    SetConsoleCtrlHandler(s_ctrl_handler, FALSE);
    s_shutdown_signal_received.Set();
    return TRUE;
  default:
    return FALSE;
  }
}
#endif

#if defined(__unix__) || defined(__unix) || defined(__APPLE__)
static void SignalHandler(int)
{
  const char message[] = "A signal was received. A second signal will force Dolphin to stop.\n";
  if (write(STDERR_FILENO, message, sizeof(message)) < 0)
  {
  }
  s_shutdown_signal_received.Set();
}
#endif

CFrame::CFrame(wxFrame* parent, wxWindowID id, const wxString& title, wxRect geometry,
               bool use_debugger, bool batch_mode, bool show_log_window, long style)
    : CRenderFrame(parent, id, title, wxDefaultPosition, wxSize(800, 600), style),
      UseDebugger(use_debugger), m_bBatchMode(batch_mode)
{
  BindEvents();

  m_main_config_dialog = new CConfigMain(this);

  for (int i = 0; i <= IDM_CODE_WINDOW - IDM_LOG_WINDOW; i++)
    bFloatWindow[i] = false;

  if (show_log_window)
    SConfig::GetInstance().m_InterfaceLogWindow = true;

  // Debugger class
  if (UseDebugger)
  {
    g_pCodeWindow = new CCodeWindow(this, IDM_CODE_WINDOW);
    LoadIniPerspectives();
    g_pCodeWindow->Load();
  }

  wxFrame::CreateToolBar(wxTB_DEFAULT_STYLE | wxTB_TEXT | wxTB_FLAT)->Realize();

  // Give it a status bar
  SetStatusBar(CreateStatusBar(2, wxST_SIZEGRIP, ID_STATUSBAR));
  if (!SConfig::GetInstance().m_InterfaceStatusbar)
    GetStatusBar()->Hide();

  // Give it a menu bar
  SetMenuBar(CreateMenuBar());
  // Create a menubar to service requests while the real menubar is hidden from the screen
  m_menubar_shadow = CreateMenuBar();

  // ---------------
  // Main panel
  // This panel is the parent for rendering and it holds the gamelistctrl
  m_Panel = new wxPanel(this, IDM_MPANEL, wxDefaultPosition, wxDefaultSize, 0);

  m_GameListCtrl = new CGameListCtrl(m_Panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                     wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT);
  m_GameListCtrl->Bind(wxEVT_LIST_ITEM_ACTIVATED, &CFrame::OnGameListCtrlItemActivated, this);

  wxBoxSizer* sizerPanel = new wxBoxSizer(wxHORIZONTAL);
  sizerPanel->Add(m_GameListCtrl, 1, wxEXPAND | wxALL);
  m_Panel->SetSizer(sizerPanel);
  // ---------------

  // Manager
  m_Mgr = new wxAuiManager(this, wxAUI_MGR_DEFAULT | wxAUI_MGR_LIVE_RESIZE);

  m_Mgr->AddPane(m_Panel, wxAuiPaneInfo()
                              .Name("Pane 0")
                              .Caption("Pane 0")
                              .PaneBorder(false)
                              .CaptionVisible(false)
                              .Layer(0)
                              .Center()
                              .Show());
  if (!g_pCodeWindow)
    m_Mgr->AddPane(CreateEmptyNotebook(), wxAuiPaneInfo()
                                              .Name("Pane 1")
                                              .Caption(_("Logging"))
                                              .CaptionVisible(true)
                                              .Layer(0)
                                              .FloatingSize(wxSize(600, 350))
                                              .CloseButton(true)
                                              .Hide());
  AuiFullscreen = m_Mgr->SavePerspective();

  if (!SConfig::GetInstance().m_InterfaceToolbar)
    DoToggleToolbar(false);

  m_LogWindow = new CLogWindow(this, IDM_LOG_WINDOW);
  m_LogWindow->Hide();
  m_LogWindow->Disable();

  for (int i = 0; i < 8; ++i)
    g_TASInputDlg[i] = new TASInputDlg(this);

  Movie::SetGCInputManip(GCTASManipFunction);
  Movie::SetWiiInputManip(WiiTASManipFunction);

  State::SetOnAfterLoadCallback(OnAfterLoadCallback);
  Core::SetOnStoppedCallback(OnStoppedCallback);

  // Setup perspectives
  if (g_pCodeWindow)
  {
    // Load perspective
    DoLoadPerspective();
  }
  else
  {
    if (SConfig::GetInstance().m_InterfaceLogWindow)
      ToggleLogWindow(true);
    if (SConfig::GetInstance().m_InterfaceLogConfigWindow)
      ToggleLogConfigWindow(true);
  }

  // Setup the window size.
  // This has to be done here instead of in Main because the Show() happens here.
  SetMinSize(FromDIP(wxSize(400, 300)));
  WxUtils::SetWindowSizeAndFitToScreen(this, geometry.GetPosition(), geometry.GetSize(),
                                       FromDIP(wxSize(800, 600)));

  // Start debugging maximized (Must be after the window has been positioned)
  if (UseDebugger)
    Maximize(true);

  // Commit
  m_Mgr->Update();

  // The window must be shown for m_XRRConfig to be created (wxGTK will not allocate X11
  // resources until the window is shown for the first time).
  Show();

#ifdef _WIN32
  SetToolTip("");
  GetToolTip()->SetAutoPop(25000);
#endif

#if defined(HAVE_XRANDR) && HAVE_XRANDR
  m_XRRConfig = new X11Utils::XRRConfiguration(X11Utils::XDisplayFromHandle(GetHandle()),
                                               X11Utils::XWindowFromHandle(GetHandle()));
#endif

  // Connect event handlers
  m_Mgr->Bind(wxEVT_AUI_RENDER, &CFrame::OnManagerResize, this);

  // Update controls
  UpdateGUI();

  // check if game is running
  InitControllers();

  m_poll_hotkey_timer.SetOwner(this);
  Bind(wxEVT_TIMER, &CFrame::PollHotkeys, this, m_poll_hotkey_timer.GetId());
  m_poll_hotkey_timer.Start(1000 / 60, wxTIMER_CONTINUOUS);

  // Shut down cleanly on SIGINT, SIGTERM (Unix) and on various signals on Windows
  m_handle_signal_timer.SetOwner(this);
  Bind(wxEVT_TIMER, &CFrame::HandleSignal, this, m_handle_signal_timer.GetId());
  m_handle_signal_timer.Start(100, wxTIMER_CONTINUOUS);

#if defined(__unix__) || defined(__unix) || defined(__APPLE__)
  struct sigaction sa;
  sa.sa_handler = SignalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESETHAND;
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
#endif
#ifdef _WIN32
  SetConsoleCtrlHandler(s_ctrl_handler, TRUE);
#endif
}
// Destructor
CFrame::~CFrame()
{
  Wiimote::Shutdown();
  Keyboard::Shutdown();
  Pad::Shutdown();
  HotkeyManagerEmu::Shutdown();
  g_controller_interface.Shutdown();

#if defined(HAVE_XRANDR) && HAVE_XRANDR
  delete m_XRRConfig;
#endif

  ClosePages();

  delete m_Mgr;

  // This object is owned by us, not wxw
  m_menubar_shadow->Destroy();
  m_menubar_shadow = nullptr;
}

void CFrame::BindEvents()
{
  BindMenuBarEvents();

  Bind(DOLPHIN_EVT_RELOAD_THEME_BITMAPS, &CFrame::OnReloadThemeBitmaps, this);
  Bind(DOLPHIN_EVT_RELOAD_GAMELIST, &CFrame::OnReloadGameList, this);
  Bind(DOLPHIN_EVT_UPDATE_LOAD_WII_MENU_ITEM, &CFrame::OnUpdateLoadWiiMenuItem, this);
  Bind(DOLPHIN_EVT_BOOT_SOFTWARE, &CFrame::OnPlay, this);
  Bind(DOLPHIN_EVT_STOP_SOFTWARE, &CFrame::OnStop, this);
}

bool CFrame::RendererIsFullscreen()
{
  bool fullscreen = false;

  if (Core::GetState() == Core::State::Running || Core::GetState() == Core::State::Paused)
  {
    fullscreen = m_RenderFrame->IsFullScreen();
  }

  return fullscreen;
}

void CFrame::OnQuit(wxCommandEvent& WXUNUSED(event))
{
  Close(true);
}

// --------
// Events
void CFrame::OnActive(wxActivateEvent& event)
{
  m_bRendererHasFocus = (event.GetActive() && event.GetEventObject() == m_RenderFrame);
  if (Core::GetState() == Core::State::Running || Core::GetState() == Core::State::Paused)
  {
    if (m_bRendererHasFocus)
    {
      if (SConfig::GetInstance().bRenderToMain)
        m_RenderParent->SetFocus();
      else if (RendererIsFullscreen() && g_ActiveConfig.ExclusiveFullscreenEnabled())
        DoExclusiveFullscreen(true);  // Regain exclusive mode

      if (SConfig::GetInstance().m_PauseOnFocusLost && Core::GetState() == Core::State::Paused)
        DoPause();

      if (SConfig::GetInstance().bHideCursor && Core::GetState() == Core::State::Running)
        m_RenderParent->SetCursor(wxCURSOR_BLANK);
    }
    else
    {
      if (SConfig::GetInstance().m_PauseOnFocusLost && Core::GetState() == Core::State::Running)
        DoPause();

      if (SConfig::GetInstance().bHideCursor)
        m_RenderParent->SetCursor(wxNullCursor);
    }
  }
  event.Skip();
}

void CFrame::OnClose(wxCloseEvent& event)
{
  // Before closing the window we need to shut down the emulation core.
  // We'll try to close this window again once that is done.
  if (Core::GetState() != Core::State::Uninitialized)
  {
    DoStop();
    if (event.CanVeto())
    {
      event.Veto();
    }
    // Tell OnStopped to resubmit the Close event
    m_bClosing = true;
    return;
  }

  // Stop Dolphin from saving the minimized Xpos and Ypos
  if (main_frame->IsIconized())
    main_frame->Iconize(false);

  // Don't forget the skip or the window won't be destroyed
  event.Skip();

  // Save GUI settings
  if (g_pCodeWindow)
  {
    SaveIniPerspectives();
  }
  else
  {
    m_LogWindow->SaveSettings();
  }
  if (m_LogWindow)
    m_LogWindow->RemoveAllListeners();

  // Uninit
  m_Mgr->UnInit();
}

// Post events

// Warning: This may cause an endless loop if the event is propagated back to its parent
void CFrame::PostEvent(wxCommandEvent& event)
{
  if (g_pCodeWindow && event.GetId() >= IDM_INTERPRETER && event.GetId() <= IDM_ADDRBOX)
  {
    event.StopPropagation();
    g_pCodeWindow->GetEventHandler()->AddPendingEvent(event);
  }
  else
  {
    event.Skip();
  }
}

void CFrame::OnMove(wxMoveEvent& event)
{
  event.Skip();

  if (!IsMaximized() && !(SConfig::GetInstance().bRenderToMain && RendererIsFullscreen()))
  {
    SConfig::GetInstance().iPosX = GetPosition().x;
    SConfig::GetInstance().iPosY = GetPosition().y;
  }
}

void CFrame::OnResize(wxSizeEvent& event)
{
  event.Skip();

  if (!IsMaximized() && !IsIconized() &&
      !(SConfig::GetInstance().bRenderToMain && RendererIsFullscreen()) &&
      !(Core::GetState() != Core::State::Uninitialized && SConfig::GetInstance().bRenderToMain &&
        SConfig::GetInstance().bRenderWindowAutoSize))
  {
    SConfig::GetInstance().iWidth = GetSize().GetWidth();
    SConfig::GetInstance().iHeight = GetSize().GetHeight();
  }

  // Make sure the logger pane is a sane size
  if (!g_pCodeWindow && m_LogWindow && m_Mgr->GetPane("Pane 1").IsShown() &&
      !m_Mgr->GetPane("Pane 1").IsFloating() &&
      (m_LogWindow->x > GetClientRect().GetWidth() || m_LogWindow->y > GetClientRect().GetHeight()))
    ShowResizePane();
}

// Host messages

#ifdef _WIN32
WXLRESULT CFrame::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
  if (WM_SYSCOMMAND == nMsg && (SC_SCREENSAVE == wParam || SC_MONITORPOWER == wParam))
  {
    return 0;
  }
  else if (nMsg == WM_QUERYENDSESSION)
  {
    // Indicate that the application will be able to close
    return 1;
  }
  else if (nMsg == WM_ENDSESSION)
  {
    // Actually trigger the close now
    Close(true);
    return 0;
  }
  else
  {
    return wxFrame::MSWWindowProc(nMsg, wParam, lParam);
  }
}
#endif

void CFrame::UpdateTitle(const std::string& str)
{
  if (SConfig::GetInstance().bRenderToMain && SConfig::GetInstance().m_InterfaceStatusbar)
  {
    GetStatusBar()->SetStatusText(str, 0);
    m_RenderFrame->SetTitle(scm_rev_str);
  }
  else
  {
    std::string titleStr = StringFromFormat("%s | %s", scm_rev_str.c_str(), str.c_str());
    m_RenderFrame->SetTitle(titleStr);
  }
}

void CFrame::OnHostMessage(wxCommandEvent& event)
{
  switch (event.GetId())
  {
  case IDM_UPDATE_DISASM_DIALOG:  // For breakpoints causing pausing
    if (!g_pCodeWindow || Core::GetState() != Core::State::Paused)
      return;
  // fallthrough

  case IDM_UPDATE_GUI:
    UpdateGUI();
    break;

  case IDM_UPDATE_STATUS_BAR:
    if (GetStatusBar() != nullptr)
      GetStatusBar()->SetStatusText(event.GetString(), event.GetInt());
    break;

  case IDM_UPDATE_TITLE:
    UpdateTitle(WxStrToStr(event.GetString()));
    break;

  case IDM_WINDOW_SIZE_REQUEST:
  {
    std::pair<int, int>* win_size = (std::pair<int, int>*)(event.GetClientData());
    OnRenderWindowSizeRequest(win_size->first, win_size->second);
    delete win_size;
  }
  break;

  case WM_USER_CREATE:
    if (SConfig::GetInstance().bHideCursor)
      m_RenderParent->SetCursor(wxCURSOR_BLANK);
    break;

  case IDM_PANIC:
  {
    wxString caption = event.GetString().BeforeFirst(':');
    wxString text = event.GetString().AfterFirst(':');
    bPanicResult =
        (wxYES == wxMessageBox(text, caption, wxSTAY_ON_TOP | (event.GetInt() ? wxYES_NO : wxOK),
                               wxWindow::FindFocus()));
    panic_event.Set();
  }
  break;

  case WM_USER_STOP:
    DoStop();
    break;

  case IDM_STOPPED:
    OnStopped();
    break;

  case IDM_FORCE_CONNECT_WIIMOTE1:
  case IDM_FORCE_CONNECT_WIIMOTE2:
  case IDM_FORCE_CONNECT_WIIMOTE3:
  case IDM_FORCE_CONNECT_WIIMOTE4:
  case IDM_FORCE_CONNECT_BALANCEBOARD:
    ConnectWiimote(event.GetId() - IDM_FORCE_CONNECT_WIIMOTE1, true);
    break;

  case IDM_FORCE_DISCONNECT_WIIMOTE1:
  case IDM_FORCE_DISCONNECT_WIIMOTE2:
  case IDM_FORCE_DISCONNECT_WIIMOTE3:
  case IDM_FORCE_DISCONNECT_WIIMOTE4:
  case IDM_FORCE_DISCONNECT_BALANCEBOARD:
    ConnectWiimote(event.GetId() - IDM_FORCE_DISCONNECT_WIIMOTE1, false);
    break;
  }
}

void CFrame::OnRenderWindowSizeRequest(int width, int height)
{
  if (!SConfig::GetInstance().bRenderWindowAutoSize || !Core::IsRunning() ||
      RendererIsFullscreen() || m_RenderFrame->IsMaximized())
    return;

  wxSize requested_size(width, height);
  // Convert to window pixels, since the size is from the backend it will be in framebuffer px.
  requested_size *= 1.0 / m_RenderFrame->GetContentScaleFactor();
  wxSize old_size;

  if (!SConfig::GetInstance().bRenderToMain)
  {
    old_size = m_RenderFrame->GetClientSize();
  }
  else
  {
    // Resize for the render panel only, this implicitly retains space for everything else
    // (i.e. log panel, toolbar, statusbar, etc) without needing to compute for them.
    old_size = m_RenderParent->GetSize();
  }

  wxSize diff = requested_size - old_size;
  if (diff != wxSize())
    m_RenderFrame->SetSize(m_RenderFrame->GetSize() + diff);
}

bool CFrame::RendererHasFocus()
{
  if (m_RenderParent == nullptr)
    return false;
  return m_bRendererHasFocus;
}

void CFrame::OnGameListCtrlItemActivated(wxListEvent& WXUNUSED(event))
{
  // Show all platforms and regions if...
  // 1. All platforms are set to hide
  // 2. All Regions are set to hide
  // Otherwise call BootGame to either...
  // 1. Boot the selected iso
  // 2. Boot the default or last loaded iso.
  // 3. Call BrowseForDirectory if the gamelist is empty
  if (!m_GameListCtrl->GetISO(0) && CGameListCtrl::IsHidingItems())
  {
    SConfig::GetInstance().m_ListGC = SConfig::GetInstance().m_ListWii =
        SConfig::GetInstance().m_ListWad = SConfig::GetInstance().m_ListElfDol =
            SConfig::GetInstance().m_ListJap = SConfig::GetInstance().m_ListUsa =
                SConfig::GetInstance().m_ListPal = SConfig::GetInstance().m_ListAustralia =
                    SConfig::GetInstance().m_ListFrance = SConfig::GetInstance().m_ListGermany =
                        SConfig::GetInstance().m_ListItaly = SConfig::GetInstance().m_ListKorea =
                            SConfig::GetInstance().m_ListNetherlands =
                                SConfig::GetInstance().m_ListRussia =
                                    SConfig::GetInstance().m_ListSpain =
                                        SConfig::GetInstance().m_ListTaiwan =
                                            SConfig::GetInstance().m_ListWorld =
                                                SConfig::GetInstance().m_ListUnknown = true;

    GetMenuBar()->FindItem(IDM_LIST_GC)->Check(true);
    GetMenuBar()->FindItem(IDM_LIST_WII)->Check(true);
    GetMenuBar()->FindItem(IDM_LIST_WAD)->Check(true);
    GetMenuBar()->FindItem(IDM_LIST_JAP)->Check(true);
    GetMenuBar()->FindItem(IDM_LIST_USA)->Check(true);
    GetMenuBar()->FindItem(IDM_LIST_PAL)->Check(true);
    GetMenuBar()->FindItem(IDM_LIST_AUSTRALIA)->Check(true);
    GetMenuBar()->FindItem(IDM_LIST_FRANCE)->Check(true);
    GetMenuBar()->FindItem(IDM_LIST_GERMANY)->Check(true);
    GetMenuBar()->FindItem(IDM_LIST_ITALY)->Check(true);
    GetMenuBar()->FindItem(IDM_LIST_KOREA)->Check(true);
    GetMenuBar()->FindItem(IDM_LIST_NETHERLANDS)->Check(true);
    GetMenuBar()->FindItem(IDM_LIST_RUSSIA)->Check(true);
    GetMenuBar()->FindItem(IDM_LIST_SPAIN)->Check(true);
    GetMenuBar()->FindItem(IDM_LIST_TAIWAN)->Check(true);
    GetMenuBar()->FindItem(IDM_LIST_WORLD)->Check(true);
    GetMenuBar()->FindItem(IDM_LIST_UNKNOWN)->Check(true);

    UpdateGameList();
  }
  else if (!m_GameListCtrl->GetISO(0))
  {
    m_GameListCtrl->BrowseForDirectory();
  }
  else
  {
    // Game started by double click
    BootGame("");
  }
}

static bool IsHotkey(int id, bool held = false)
{
  return HotkeyManagerEmu::IsPressed(id, held);
}

static int GetMenuIDFromHotkey(unsigned int key)
{
  switch (key)
  {
  case HK_OPEN:
    return wxID_OPEN;
  case HK_CHANGE_DISC:
    return IDM_CHANGE_DISC;
  case HK_REFRESH_LIST:
    return wxID_REFRESH;
  case HK_PLAY_PAUSE:
    return IDM_PLAY;
  case HK_STOP:
    return IDM_STOP;
  case HK_RESET:
    return IDM_RESET;
  case HK_FRAME_ADVANCE:
    return IDM_FRAMESTEP;
  case HK_START_RECORDING:
    return IDM_RECORD;
  case HK_PLAY_RECORDING:
    return IDM_PLAY_RECORD;
  case HK_EXPORT_RECORDING:
    return IDM_RECORD_EXPORT;
  case HK_READ_ONLY_MODE:
    return IDM_RECORD_READ_ONLY;
  case HK_FULLSCREEN:
    return IDM_TOGGLE_FULLSCREEN;
  case HK_SCREENSHOT:
    return IDM_SCREENSHOT;
  case HK_EXIT:
    return wxID_EXIT;

  case HK_WIIMOTE1_CONNECT:
    return IDM_CONNECT_WIIMOTE1;
  case HK_WIIMOTE2_CONNECT:
    return IDM_CONNECT_WIIMOTE2;
  case HK_WIIMOTE3_CONNECT:
    return IDM_CONNECT_WIIMOTE3;
  case HK_WIIMOTE4_CONNECT:
    return IDM_CONNECT_WIIMOTE4;
  case HK_BALANCEBOARD_CONNECT:
    return IDM_CONNECT_BALANCEBOARD;

  case HK_LOAD_STATE_SLOT_1:
    return IDM_LOAD_SLOT_1;
  case HK_LOAD_STATE_SLOT_2:
    return IDM_LOAD_SLOT_2;
  case HK_LOAD_STATE_SLOT_3:
    return IDM_LOAD_SLOT_3;
  case HK_LOAD_STATE_SLOT_4:
    return IDM_LOAD_SLOT_4;
  case HK_LOAD_STATE_SLOT_5:
    return IDM_LOAD_SLOT_5;
  case HK_LOAD_STATE_SLOT_6:
    return IDM_LOAD_SLOT_6;
  case HK_LOAD_STATE_SLOT_7:
    return IDM_LOAD_SLOT_7;
  case HK_LOAD_STATE_SLOT_8:
    return IDM_LOAD_SLOT_8;
  case HK_LOAD_STATE_SLOT_9:
    return IDM_LOAD_SLOT_9;
  case HK_LOAD_STATE_SLOT_10:
    return IDM_LOAD_SLOT_10;

  case HK_SAVE_STATE_SLOT_1:
    return IDM_SAVE_SLOT_1;
  case HK_SAVE_STATE_SLOT_2:
    return IDM_SAVE_SLOT_2;
  case HK_SAVE_STATE_SLOT_3:
    return IDM_SAVE_SLOT_3;
  case HK_SAVE_STATE_SLOT_4:
    return IDM_SAVE_SLOT_4;
  case HK_SAVE_STATE_SLOT_5:
    return IDM_SAVE_SLOT_5;
  case HK_SAVE_STATE_SLOT_6:
    return IDM_SAVE_SLOT_6;
  case HK_SAVE_STATE_SLOT_7:
    return IDM_SAVE_SLOT_7;
  case HK_SAVE_STATE_SLOT_8:
    return IDM_SAVE_SLOT_8;
  case HK_SAVE_STATE_SLOT_9:
    return IDM_SAVE_SLOT_9;
  case HK_SAVE_STATE_SLOT_10:
    return IDM_SAVE_SLOT_10;

  case HK_LOAD_LAST_STATE_1:
    return IDM_LOAD_LAST_1;
  case HK_LOAD_LAST_STATE_2:
    return IDM_LOAD_LAST_2;
  case HK_LOAD_LAST_STATE_3:
    return IDM_LOAD_LAST_3;
  case HK_LOAD_LAST_STATE_4:
    return IDM_LOAD_LAST_4;
  case HK_LOAD_LAST_STATE_5:
    return IDM_LOAD_LAST_5;
  case HK_LOAD_LAST_STATE_6:
    return IDM_LOAD_LAST_6;
  case HK_LOAD_LAST_STATE_7:
    return IDM_LOAD_LAST_7;
  case HK_LOAD_LAST_STATE_8:
    return IDM_LOAD_LAST_8;
  case HK_LOAD_LAST_STATE_9:
    return IDM_LOAD_LAST_9;
  case HK_LOAD_LAST_STATE_10:
    return IDM_LOAD_LAST_10;

  case HK_SAVE_FIRST_STATE:
    return IDM_SAVE_FIRST_STATE;
  case HK_UNDO_LOAD_STATE:
    return IDM_UNDO_LOAD_STATE;
  case HK_UNDO_SAVE_STATE:
    return IDM_UNDO_SAVE_STATE;
  case HK_LOAD_STATE_FILE:
    return IDM_LOAD_STATE_FILE;
  case HK_SAVE_STATE_FILE:
    return IDM_SAVE_STATE_FILE;

  case HK_SELECT_STATE_SLOT_1:
    return IDM_SELECT_SLOT_1;
  case HK_SELECT_STATE_SLOT_2:
    return IDM_SELECT_SLOT_2;
  case HK_SELECT_STATE_SLOT_3:
    return IDM_SELECT_SLOT_3;
  case HK_SELECT_STATE_SLOT_4:
    return IDM_SELECT_SLOT_4;
  case HK_SELECT_STATE_SLOT_5:
    return IDM_SELECT_SLOT_5;
  case HK_SELECT_STATE_SLOT_6:
    return IDM_SELECT_SLOT_6;
  case HK_SELECT_STATE_SLOT_7:
    return IDM_SELECT_SLOT_7;
  case HK_SELECT_STATE_SLOT_8:
    return IDM_SELECT_SLOT_8;
  case HK_SELECT_STATE_SLOT_9:
    return IDM_SELECT_SLOT_9;
  case HK_SELECT_STATE_SLOT_10:
    return IDM_SELECT_SLOT_10;
  case HK_SAVE_STATE_SLOT_SELECTED:
    return IDM_SAVE_SELECTED_SLOT;
  case HK_LOAD_STATE_SLOT_SELECTED:
    return IDM_LOAD_SELECTED_SLOT;

  case HK_FREELOOK_DECREASE_SPEED:
    return IDM_FREELOOK_DECREASE_SPEED;
  case HK_FREELOOK_INCREASE_SPEED:
    return IDM_FREELOOK_INCREASE_SPEED;
  case HK_FREELOOK_RESET_SPEED:
    return IDM_FREELOOK_RESET_SPEED;
  case HK_FREELOOK_LEFT:
    return IDM_FREELOOK_LEFT;
  case HK_FREELOOK_RIGHT:
    return IDM_FREELOOK_RIGHT;
  case HK_FREELOOK_UP:
    return IDM_FREELOOK_UP;
  case HK_FREELOOK_DOWN:
    return IDM_FREELOOK_DOWN;
  case HK_FREELOOK_ZOOM_IN:
    return IDM_FREELOOK_ZOOM_IN;
  case HK_FREELOOK_ZOOM_OUT:
    return IDM_FREELOOK_ZOOM_OUT;
  case HK_FREELOOK_RESET:
    return IDM_FREELOOK_RESET;
  }

  return -1;
}

void OnAfterLoadCallback()
{
  // warning: this gets called from the CPU thread, so we should only queue things to do on the
  // proper thread
  if (main_frame)
  {
    wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATE_GUI);
    main_frame->GetEventHandler()->AddPendingEvent(event);
  }
}

void OnStoppedCallback()
{
  // warning: this gets called from the EmuThread, so we should only queue things to do on the
  // proper thread
  if (main_frame)
  {
    wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_STOPPED);
    main_frame->GetEventHandler()->AddPendingEvent(event);
  }
}

void GCTASManipFunction(GCPadStatus* PadStatus, int controllerID)
{
  if (main_frame)
    main_frame->g_TASInputDlg[controllerID]->GetValues(PadStatus);
}

void WiiTASManipFunction(u8* data, WiimoteEmu::ReportFeatures rptf, int controllerID, int ext,
                         const wiimote_key key)
{
  if (main_frame)
  {
    main_frame->g_TASInputDlg[controllerID + 4]->GetValues(data, rptf, ext, key);
  }
}

void CFrame::OnKeyDown(wxKeyEvent& event)
{
// On OS X, we claim all keyboard events while
// emulation is running to avoid wxWidgets sounding
// the system beep for unhandled key events when
// receiving pad/Wiimote keypresses which take an
// entirely different path through the HID subsystem.
#ifndef __APPLE__
  // On other platforms, we leave the key event alone
  // so it can be passed on to the windowing system.
  event.Skip();
#endif
}

void CFrame::OnMouse(wxMouseEvent& event)
{
  // next handlers are all for FreeLook, so we don't need to check them if disabled
  if (!g_Config.bFreeLook)
  {
    event.Skip();
    return;
  }

  // Free look variables
  static bool mouseLookEnabled = false;
  static bool mouseMoveEnabled = false;
  static float lastMouse[2];

  if (event.MiddleDown())
  {
    lastMouse[0] = event.GetX();
    lastMouse[1] = event.GetY();
    mouseMoveEnabled = true;
  }
  else if (event.RightDown())
  {
    lastMouse[0] = event.GetX();
    lastMouse[1] = event.GetY();
    mouseLookEnabled = true;
  }
  else if (event.MiddleUp())
  {
    mouseMoveEnabled = false;
  }
  else if (event.RightUp())
  {
    mouseLookEnabled = false;
  }
  // no button, so it's a move event
  else if (event.GetButton() == wxMOUSE_BTN_NONE)
  {
    if (mouseLookEnabled)
    {
      VertexShaderManager::RotateView((event.GetX() - lastMouse[0]) / 200.0f,
                                      (event.GetY() - lastMouse[1]) / 200.0f);
      lastMouse[0] = event.GetX();
      lastMouse[1] = event.GetY();
    }

    if (mouseMoveEnabled)
    {
      VertexShaderManager::TranslateView((event.GetX() - lastMouse[0]) / 50.0f,
                                         (event.GetY() - lastMouse[1]) / 50.0f);
      lastMouse[0] = event.GetX();
      lastMouse[1] = event.GetY();
    }
  }

  event.Skip();
}

void CFrame::DoFullscreen(bool enable_fullscreen)
{
  ToggleDisplayMode(enable_fullscreen);

  if (SConfig::GetInstance().bRenderToMain)
  {
    m_RenderFrame->ShowFullScreen(enable_fullscreen, wxFULLSCREEN_ALL);

    if (enable_fullscreen)
    {
      // Save the current mode before going to fullscreen
      AuiCurrent = m_Mgr->SavePerspective();
      m_Mgr->LoadPerspective(AuiFullscreen, true);

      // Hide toolbar
      DoToggleToolbar(false);

      // Hide menubar (by having wxwidgets delete it)
      SetMenuBar(nullptr);

      // Hide the statusbar if enabled
      if (GetStatusBar()->IsShown())
      {
        GetStatusBar()->Hide();
        this->SendSizeEvent();
      }
    }
    else
    {
      // Restore saved perspective
      m_Mgr->LoadPerspective(AuiCurrent, true);

      // Restore toolbar to the status it was at before going fullscreen.
      DoToggleToolbar(SConfig::GetInstance().m_InterfaceToolbar);

      // Recreate the menubar if needed.
      if (wxFrame::GetMenuBar() == nullptr)
      {
        SetMenuBar(CreateMenuBar());
      }

      // Show statusbar if enabled
      if (SConfig::GetInstance().m_InterfaceStatusbar)
      {
        GetStatusBar()->Show();
        this->SendSizeEvent();
      }
    }
  }
  else if (g_ActiveConfig.ExclusiveFullscreenEnabled())
  {
    if (!enable_fullscreen)
      DoExclusiveFullscreen(false);

    m_RenderFrame->ShowFullScreen(enable_fullscreen, wxFULLSCREEN_ALL);
    m_RenderFrame->Raise();

    if (enable_fullscreen)
      DoExclusiveFullscreen(true);
  }
  else
  {
    m_RenderFrame->ShowFullScreen(enable_fullscreen, wxFULLSCREEN_ALL);
    m_RenderFrame->Raise();
  }
}

void CFrame::DoExclusiveFullscreen(bool enable_fullscreen)
{
  if (!g_renderer || g_renderer->IsFullscreen() == enable_fullscreen)
    return;

  bool was_unpaused = Core::PauseAndLock(true);
  g_renderer->SetFullscreen(enable_fullscreen);
  Core::PauseAndLock(false, was_unpaused);
}

const CGameListCtrl* CFrame::GetGameListCtrl() const
{
  return m_GameListCtrl;
}

void CFrame::PollHotkeys(wxTimerEvent& event)
{
  if (!HotkeyManagerEmu::IsEnabled())
    return;

  if (Core::GetState() == Core::State::Uninitialized || Core::GetState() == Core::State::Paused)
    g_controller_interface.UpdateInput();

  if (Core::GetState() != Core::State::Stopping)
  {
    HotkeyManagerEmu::GetStatus();
    ParseHotkeys();
  }
}

void CFrame::ParseHotkeys()
{
  for (int i = 0; i < NUM_HOTKEYS; i++)
  {
    switch (i)
    {
    case HK_OPEN:
    case HK_CHANGE_DISC:
    case HK_REFRESH_LIST:
    case HK_RESET:
    case HK_START_RECORDING:
    case HK_PLAY_RECORDING:
    case HK_EXPORT_RECORDING:
    case HK_READ_ONLY_MODE:

    case HK_LOAD_STATE_FILE:
    case HK_SAVE_STATE_FILE:
    case HK_LOAD_STATE_SLOT_SELECTED:

      if (IsHotkey(i))
      {
        const int id = GetMenuIDFromHotkey(i);
        if (id >= 0)
        {
          wxCommandEvent evt(wxEVT_MENU, id);
          wxMenuItem* item = GetMenuBar()->FindItem(id);
          if (item && item->IsCheckable())
          {
            item->wxMenuItemBase::Toggle();
            evt.SetInt(item->IsChecked());
          }
          GetEventHandler()->AddPendingEvent(evt);
        }
      }
    default:
      break;
      // do nothing
    }
  }

  if (!Core::IsRunningAndStarted())
  {
    return;
  }

  // Toggle fullscreen
  if (IsHotkey(HK_FULLSCREEN))
    DoFullscreen(!RendererIsFullscreen());
  // Pause and Unpause
  if (IsHotkey(HK_PLAY_PAUSE))
    DoPause();
  // Frame advance
  HandleFrameSkipHotkeys();
  // Stop
  if (IsHotkey(HK_STOP))
    DoStop();
  // Screenshot hotkey
  if (IsHotkey(HK_SCREENSHOT))
    Core::SaveScreenShot();
  if (IsHotkey(HK_EXIT))
    wxPostEvent(this, wxCommandEvent(wxEVT_MENU, wxID_EXIT));
  if (IsHotkey(HK_VOLUME_DOWN))
    AudioCommon::DecreaseVolume(3);
  if (IsHotkey(HK_VOLUME_UP))
    AudioCommon::IncreaseVolume(3);
  if (IsHotkey(HK_VOLUME_TOGGLE_MUTE))
    AudioCommon::ToggleMuteVolume();

  if (SConfig::GetInstance().m_bt_passthrough_enabled)
  {
    auto device = IOS::HLE::GetDeviceByName("/dev/usb/oh1/57e/305");
    if (device != nullptr)
      std::static_pointer_cast<IOS::HLE::Device::BluetoothBase>(device)->UpdateSyncButtonState(
          IsHotkey(HK_TRIGGER_SYNC_BUTTON, true));
  }

  if (UseDebugger)
  {
    if (IsHotkey(HK_STEP))
    {
      wxCommandEvent evt(wxEVT_MENU, IDM_STEP);
      GetEventHandler()->AddPendingEvent(evt);
    }
    if (IsHotkey(HK_STEP_OVER))
    {
      wxCommandEvent evt(wxEVT_MENU, IDM_STEPOVER);
      GetEventHandler()->AddPendingEvent(evt);
    }
    if (IsHotkey(HK_STEP_OUT))
    {
      wxCommandEvent evt(wxEVT_MENU, IDM_STEPOUT);
      GetEventHandler()->AddPendingEvent(evt);
    }
    if (IsHotkey(HK_SKIP))
    {
      wxCommandEvent evt(wxEVT_MENU, IDM_SKIP);
      GetEventHandler()->AddPendingEvent(evt);
    }
    if (IsHotkey(HK_SHOW_PC))
    {
      wxCommandEvent evt(wxEVT_MENU, IDM_GOTOPC);
      GetEventHandler()->AddPendingEvent(evt);
    }
    if (IsHotkey(HK_SET_PC))
    {
      wxCommandEvent evt(wxEVT_MENU, IDM_SETPC);
      GetEventHandler()->AddPendingEvent(evt);
    }
    if (IsHotkey(HK_BP_TOGGLE))
    {
      wxCommandEvent evt(wxEVT_MENU, IDM_TOGGLE_BREAKPOINT);
      GetEventHandler()->AddPendingEvent(evt);
    }
    if (IsHotkey(HK_BP_ADD))
    {
      BreakPointDlg bpDlg(this);
      if (bpDlg.ShowModal() == wxID_OK)
      {
        wxCommandEvent evt(wxEVT_HOST_COMMAND, IDM_UPDATE_BREAKPOINTS);
        g_pCodeWindow->GetEventHandler()->AddPendingEvent(evt);
      }
    }
    if (IsHotkey(HK_MBP_ADD))
    {
      MemoryCheckDlg memDlg(this);
      if (memDlg.ShowModal() == wxID_OK)
      {
        wxCommandEvent evt(wxEVT_HOST_COMMAND, IDM_UPDATE_BREAKPOINTS);
        g_pCodeWindow->GetEventHandler()->AddPendingEvent(evt);
      }
    }
  }

  // Wiimote connect and disconnect hotkeys
  int WiimoteId = -1;
  if (IsHotkey(HK_WIIMOTE1_CONNECT))
    WiimoteId = 0;
  if (IsHotkey(HK_WIIMOTE2_CONNECT))
    WiimoteId = 1;
  if (IsHotkey(HK_WIIMOTE3_CONNECT))
    WiimoteId = 2;
  if (IsHotkey(HK_WIIMOTE4_CONNECT))
    WiimoteId = 3;
  if (IsHotkey(HK_BALANCEBOARD_CONNECT))
    WiimoteId = 4;

  // Actually perform the Wiimote connection or disconnection
  if (WiimoteId >= 0 && SConfig::GetInstance().bWii)
  {
    wxCommandEvent evt;
    evt.SetId(IDM_CONNECT_WIIMOTE1 + WiimoteId);
    OnConnectWiimote(evt);
  }

  if (IsHotkey(HK_INCREASE_IR))
  {
    OSDChoice = 1;
    ++g_Config.iEFBScale;
  }
  if (IsHotkey(HK_DECREASE_IR))
  {
    OSDChoice = 1;
    if (--g_Config.iEFBScale < SCALE_AUTO)
      g_Config.iEFBScale = SCALE_AUTO;
  }
  if (IsHotkey(HK_TOGGLE_CROP))
  {
    g_Config.bCrop = !g_Config.bCrop;
  }
  if (IsHotkey(HK_TOGGLE_AR))
  {
    OSDChoice = 2;
    // Toggle aspect ratio
    g_Config.iAspectRatio = (g_Config.iAspectRatio + 1) & 3;
  }
  if (IsHotkey(HK_TOGGLE_EFBCOPIES))
  {
    OSDChoice = 3;
    // Toggle EFB copies between EFB2RAM and EFB2Texture
    g_Config.bSkipEFBCopyToRam = !g_Config.bSkipEFBCopyToRam;
  }
  if (IsHotkey(HK_TOGGLE_FOG))
  {
    OSDChoice = 4;
    g_Config.bDisableFog = !g_Config.bDisableFog;
  }
  if (IsHotkey(HK_TOGGLE_DUMPTEXTURES))
  {
    g_Config.bDumpTextures = !g_Config.bDumpTextures;
  }
  if (IsHotkey(HK_TOGGLE_TEXTURES))
    g_Config.bHiresTextures = !g_Config.bHiresTextures;
  Core::SetIsThrottlerTempDisabled(IsHotkey(HK_TOGGLE_THROTTLE, true));
  if (IsHotkey(HK_DECREASE_EMULATION_SPEED))
  {
    OSDChoice = 5;

    if (SConfig::GetInstance().m_EmulationSpeed <= 0.0f)
      SConfig::GetInstance().m_EmulationSpeed = 1.0f;
    else if (SConfig::GetInstance().m_EmulationSpeed >= 0.2f)
      SConfig::GetInstance().m_EmulationSpeed -= 0.1f;
    else
      SConfig::GetInstance().m_EmulationSpeed = 0.1f;

    if (SConfig::GetInstance().m_EmulationSpeed >= 0.95f &&
        SConfig::GetInstance().m_EmulationSpeed <= 1.05f)
      SConfig::GetInstance().m_EmulationSpeed = 1.0f;
  }
  if (IsHotkey(HK_INCREASE_EMULATION_SPEED))
  {
    OSDChoice = 5;

    if (SConfig::GetInstance().m_EmulationSpeed > 0.0f)
      SConfig::GetInstance().m_EmulationSpeed += 0.1f;

    if (SConfig::GetInstance().m_EmulationSpeed >= 0.95f &&
        SConfig::GetInstance().m_EmulationSpeed <= 1.05f)
      SConfig::GetInstance().m_EmulationSpeed = 1.0f;
  }
  if (IsHotkey(HK_SAVE_STATE_SLOT_SELECTED))
  {
    State::Save(m_saveSlot);
  }
  if (IsHotkey(HK_LOAD_STATE_SLOT_SELECTED))
  {
    State::Load(m_saveSlot);
  }

  if (IsHotkey(HK_TOGGLE_STEREO_SBS))
  {
    if (g_Config.iStereoMode != STEREO_SBS)
    {
      // Current implementation of anaglyph stereoscopy uses a
      // post-processing shader. Thus the shader needs to be to be
      // turned off when selecting other stereoscopy modes.
      if (g_Config.sPostProcessingShader == "dubois")
      {
        g_Config.sPostProcessingShader = "";
      }
      g_Config.iStereoMode = STEREO_SBS;
    }
    else
    {
      g_Config.iStereoMode = STEREO_OFF;
    }
  }
  if (IsHotkey(HK_TOGGLE_STEREO_TAB))
  {
    if (g_Config.iStereoMode != STEREO_TAB)
    {
      if (g_Config.sPostProcessingShader == "dubois")
      {
        g_Config.sPostProcessingShader = "";
      }
      g_Config.iStereoMode = STEREO_TAB;
    }
    else
    {
      g_Config.iStereoMode = STEREO_OFF;
    }
  }
  if (IsHotkey(HK_TOGGLE_STEREO_ANAGLYPH))
  {
    if (g_Config.iStereoMode != STEREO_ANAGLYPH)
    {
      // Setting the anaglyph mode also requires a specific
      // post-processing shader to be activated.
      g_Config.iStereoMode = STEREO_ANAGLYPH;
      g_Config.sPostProcessingShader = "dubois";
    }
    else
    {
      g_Config.iStereoMode = STEREO_OFF;
      g_Config.sPostProcessingShader = "";
    }
  }
  if (IsHotkey(HK_TOGGLE_STEREO_3DVISION))
  {
    if (g_Config.iStereoMode != STEREO_3DVISION)
    {
      if (g_Config.sPostProcessingShader == "dubois")
      {
        g_Config.sPostProcessingShader = "";
      }
      g_Config.iStereoMode = STEREO_3DVISION;
    }
    else
    {
      g_Config.iStereoMode = STEREO_OFF;
    }
  }

  if (IsHotkey(HK_DECREASE_DEPTH, true))
  {
    if (--g_Config.iStereoDepth < 0)
      g_Config.iStereoDepth = 0;
  }
  if (IsHotkey(HK_INCREASE_DEPTH, true))
  {
    if (++g_Config.iStereoDepth > 100)
      g_Config.iStereoDepth = 100;
  }
  if (IsHotkey(HK_DECREASE_CONVERGENCE, true))
  {
    g_Config.iStereoConvergence -= 5;
    if (g_Config.iStereoConvergence < 0)
      g_Config.iStereoConvergence = 0;
  }
  if (IsHotkey(HK_INCREASE_CONVERGENCE, true))
  {
    g_Config.iStereoConvergence += 5;
    if (g_Config.iStereoConvergence > 500)
      g_Config.iStereoConvergence = 500;
  }

  static float debugSpeed = 1.0f;
  if (IsHotkey(HK_FREELOOK_DECREASE_SPEED, true))
    debugSpeed /= 1.1f;
  if (IsHotkey(HK_FREELOOK_INCREASE_SPEED, true))
    debugSpeed *= 1.1f;
  if (IsHotkey(HK_FREELOOK_RESET_SPEED, true))
    debugSpeed = 1.0f;
  if (IsHotkey(HK_FREELOOK_UP, true))
    VertexShaderManager::TranslateView(0.0f, 0.0f, -debugSpeed);
  if (IsHotkey(HK_FREELOOK_DOWN, true))
    VertexShaderManager::TranslateView(0.0f, 0.0f, debugSpeed);
  if (IsHotkey(HK_FREELOOK_LEFT, true))
    VertexShaderManager::TranslateView(debugSpeed, 0.0f);
  if (IsHotkey(HK_FREELOOK_RIGHT, true))
    VertexShaderManager::TranslateView(-debugSpeed, 0.0f);
  if (IsHotkey(HK_FREELOOK_ZOOM_IN, true))
    VertexShaderManager::TranslateView(0.0f, debugSpeed);
  if (IsHotkey(HK_FREELOOK_ZOOM_OUT, true))
    VertexShaderManager::TranslateView(0.0f, -debugSpeed);
  if (IsHotkey(HK_FREELOOK_RESET, true))
    VertexShaderManager::ResetView();

  // Savestates
  for (u32 i = 0; i < State::NUM_STATES; i++)
  {
    if (IsHotkey(HK_LOAD_STATE_SLOT_1 + i))
      State::Load(1 + i);

    if (IsHotkey(HK_SAVE_STATE_SLOT_1 + i))
      State::Save(1 + i);

    if (IsHotkey(HK_LOAD_LAST_STATE_1 + i))
      State::LoadLastSaved(1 + i);

    if (IsHotkey(HK_SELECT_STATE_SLOT_1 + i))
    {
      wxCommandEvent slot_event;
      slot_event.SetId(IDM_SELECT_SLOT_1 + i);
      CFrame::OnSelectSlot(slot_event);
    }
  }
  if (IsHotkey(HK_SAVE_FIRST_STATE))
    State::SaveFirstSaved();
  if (IsHotkey(HK_UNDO_LOAD_STATE))
    State::UndoLoadState();
  if (IsHotkey(HK_UNDO_SAVE_STATE))
    State::UndoSaveState();
}

void CFrame::HandleFrameSkipHotkeys()
{
  static const int MAX_FRAME_SKIP_DELAY = 60;
  static int frameStepCount = 0;
  static const int FRAME_STEP_DELAY = 30;
  static int holdFrameStepDelay = 1;
  static int holdFrameStepDelayCount = 0;
  static bool holdFrameStep = false;

  if (IsHotkey(HK_FRAME_ADVANCE_DECREASE_SPEED))
  {
    ++holdFrameStepDelay;
    if (holdFrameStepDelay > MAX_FRAME_SKIP_DELAY)
      holdFrameStepDelay = MAX_FRAME_SKIP_DELAY;
  }
  else if (IsHotkey(HK_FRAME_ADVANCE_INCREASE_SPEED))
  {
    --holdFrameStepDelay;
    if (holdFrameStepDelay < 0)
      holdFrameStepDelay = 0;
  }
  else if (IsHotkey(HK_FRAME_ADVANCE_RESET_SPEED))
  {
    holdFrameStepDelay = 1;
  }
  else if (IsHotkey(HK_FRAME_ADVANCE, true))
  {
    if (holdFrameStepDelayCount < holdFrameStepDelay && holdFrameStep)
      ++holdFrameStepDelayCount;

    if ((frameStepCount == 0 || frameStepCount == FRAME_STEP_DELAY) && !holdFrameStep)
    {
      wxCommandEvent evt;
      evt.SetId(IDM_FRAMESTEP);
      CFrame::OnFrameStep(evt);
      if (holdFrameStepDelay > 0)
        holdFrameStep = true;
    }

    if (frameStepCount < FRAME_STEP_DELAY)
    {
      ++frameStepCount;
      if (holdFrameStep)
        holdFrameStep = false;
    }

    if (frameStepCount == FRAME_STEP_DELAY && holdFrameStep &&
        holdFrameStepDelayCount >= holdFrameStepDelay)
    {
      holdFrameStep = false;
      holdFrameStepDelayCount = 0;
    }
  }
  else if (frameStepCount > 0)
  {
    // Reset values of frame advance to default
    frameStepCount = 0;
    holdFrameStep = false;
    holdFrameStepDelayCount = 0;
  }
}

void CFrame::HandleSignal(wxTimerEvent& event)
{
  if (!s_shutdown_signal_received.TestAndClear())
    return;
  m_bClosing = true;
  Close();
}
