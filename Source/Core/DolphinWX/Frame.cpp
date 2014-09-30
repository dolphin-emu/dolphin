// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifdef __APPLE__
#include <Cocoa/Cocoa.h>
#endif

#include <cstddef>
#include <fstream>
#include <string>
#include <utility>
#include <vector>
#include <wx/chartype.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/filename.h>
#include <wx/frame.h>
#include <wx/gdicmn.h>
#include <wx/icon.h>
#include <wx/listbase.h>
#include <wx/menu.h>
#include <wx/menuitem.h>
#include <wx/mousestate.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/statusbr.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/thread.h>
#include <wx/toplevel.h>
#include <wx/translation.h>
#include <wx/window.h>
#include <wx/windowid.h>
#include <wx/aui/auibook.h>
#include <wx/aui/framemanager.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Thread.h"
#include "Common/Logging/ConsoleListener.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreParameter.h"
#include "Core/Movie.h"
#include "Core/State.h"
#include "Core/HW/DVDInterface.h"

#include "DolphinWX/Frame.h"
#include "DolphinWX/GameListCtrl.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/LogWindow.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/TASInputDlg.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/CodeWindow.h"

#include "InputCommon/GCPadStatus.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

// Resources

extern "C" {
#include "DolphinWX/resources/Dolphin.c" // NOLINT: Dolphin icon
};

int g_saveSlot = 1;

CRenderFrame::CRenderFrame(wxFrame* parent, wxWindowID id, const wxString& title,
		const wxPoint& pos, const wxSize& size, long style)
	: wxFrame(parent, id, title, pos, size, style)
{
	// Give it an icon
	wxIcon IconTemp;
	IconTemp.CopyFromBitmap(wxGetBitmapFromMemory(Dolphin_png));
	SetIcon(IconTemp);

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
			main_frame->GetMenuBar()->FindItem(IDM_RECORDREADONLY)->Check(true);
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
		DVDInterface::ChangeDisc(filepath);
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

	return internal_game_id == SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID();
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
					if (Core::GetState() == Core::CORE_RUN && SConfig::GetInstance().m_LocalCoreStartupParameter.bDisableScreenSaver)
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
				if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor &&
					main_frame->RendererHasFocus() && Core::GetState() == Core::CORE_RUN)
					SetCursor(wxCURSOR_BLANK);
				else
					SetCursor(wxNullCursor);
				break;
			}
			break;

		case WM_CLOSE:
			// Let Core finish initializing before accepting any WM_CLOSE messages
			if (!Core::IsRunning()) break;
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
	if (show)
	{
		// OpenGL requires the pop-up style to activate exclusive mode.
		SetWindowStyle((GetWindowStyle() & ~wxDEFAULT_FRAME_STYLE) | wxPOPUP_WINDOW);
	}

	bool result = wxTopLevelWindow::ShowFullScreen(show, style);

	if (!show)
	{
		// Restore the default style.
		SetWindowStyle((GetWindowStyle() & ~wxPOPUP_WINDOW) | wxDEFAULT_FRAME_STYLE);
	}

	return result;
}

// event tables
// Notice that wxID_HELP will be processed for the 'About' menu and the toolbar
// help button.

const wxEventType wxEVT_HOST_COMMAND = wxNewEventType();

BEGIN_EVENT_TABLE(CFrame, CRenderFrame)

// Menu bar
EVT_MENU(wxID_OPEN, CFrame::OnOpen)
EVT_MENU(wxID_EXIT, CFrame::OnQuit)
EVT_MENU(IDM_HELPWEBSITE, CFrame::OnHelp)
EVT_MENU(IDM_HELPONLINEDOCS, CFrame::OnHelp)
EVT_MENU(IDM_HELPGITHUB, CFrame::OnHelp)
EVT_MENU(wxID_ABOUT, CFrame::OnHelp)
EVT_MENU(wxID_REFRESH, CFrame::OnRefresh)
EVT_MENU(IDM_PLAY, CFrame::OnPlay)
EVT_MENU(IDM_STOP, CFrame::OnStop)
EVT_MENU(IDM_RESET, CFrame::OnReset)
EVT_MENU(IDM_RECORD, CFrame::OnRecord)
EVT_MENU(IDM_PLAYRECORD, CFrame::OnPlayRecording)
EVT_MENU(IDM_RECORDEXPORT, CFrame::OnRecordExport)
EVT_MENU(IDM_RECORDREADONLY, CFrame::OnRecordReadOnly)
EVT_MENU(IDM_TASINPUT, CFrame::OnTASInput)
EVT_MENU(IDM_TOGGLE_PAUSEMOVIE, CFrame::OnTogglePauseMovie)
EVT_MENU(IDM_SHOWLAG, CFrame::OnShowLag)
EVT_MENU(IDM_FRAMESTEP, CFrame::OnFrameStep)
EVT_MENU(IDM_SCREENSHOT, CFrame::OnScreenshot)
EVT_MENU(wxID_PREFERENCES, CFrame::OnConfigMain)
EVT_MENU(IDM_CONFIG_GFX_BACKEND, CFrame::OnConfigGFX)
EVT_MENU(IDM_CONFIG_DSP_EMULATOR, CFrame::OnConfigDSP)
EVT_MENU(IDM_CONFIG_PAD_PLUGIN, CFrame::OnConfigPAD)
EVT_MENU(IDM_CONFIG_WIIMOTE_PLUGIN, CFrame::OnConfigWiimote)
EVT_MENU(IDM_CONFIG_HOTKEYS, CFrame::OnConfigHotkey)

EVT_MENU(IDM_SAVE_PERSPECTIVE, CFrame::OnPerspectiveMenu)
EVT_MENU(IDM_EDIT_PERSPECTIVES, CFrame::OnPerspectiveMenu)
// Drop down
EVT_MENU(IDM_PERSPECTIVES_ADD_PANE, CFrame::OnPerspectiveMenu)
EVT_MENU_RANGE(IDM_PERSPECTIVES_0, IDM_PERSPECTIVES_100, CFrame::OnSelectPerspective)
EVT_MENU(IDM_ADD_PERSPECTIVE, CFrame::OnPerspectiveMenu)
EVT_MENU(IDM_TAB_SPLIT, CFrame::OnPerspectiveMenu)
EVT_MENU(IDM_NO_DOCKING, CFrame::OnPerspectiveMenu)
// Drop down float
EVT_MENU_RANGE(IDM_FLOAT_LOGWINDOW, IDM_FLOAT_CODEWINDOW, CFrame::OnFloatWindow)

EVT_MENU(IDM_NETPLAY, CFrame::OnNetPlay)
EVT_MENU(IDM_BROWSE, CFrame::OnBrowse)
EVT_MENU(IDM_MEMCARD, CFrame::OnMemcard)
EVT_MENU(IDM_IMPORTSAVE, CFrame::OnImportSave)
EVT_MENU(IDM_EXPORTALLSAVE, CFrame::OnExportAllSaves)
EVT_MENU(IDM_CHEATS, CFrame::OnShow_CheatsWindow)
EVT_MENU(IDM_CHANGEDISC, CFrame::OnChangeDisc)
EVT_MENU(IDM_MENU_INSTALLWAD, CFrame::OnInstallWAD)
EVT_MENU(IDM_LIST_INSTALLWAD, CFrame::OnInstallWAD)
EVT_MENU(IDM_LOAD_WII_MENU, CFrame::OnLoadWiiMenu)
EVT_MENU(IDM_FIFOPLAYER, CFrame::OnFifoPlayer)

EVT_MENU(IDM_TOGGLE_FULLSCREEN, CFrame::OnToggleFullscreen)
EVT_MENU(IDM_TOGGLE_DUALCORE, CFrame::OnToggleDualCore)
EVT_MENU(IDM_TOGGLE_SKIPIDLE, CFrame::OnToggleSkipIdle)
EVT_MENU(IDM_TOGGLE_TOOLBAR, CFrame::OnToggleToolbar)
EVT_MENU(IDM_TOGGLE_STATUSBAR, CFrame::OnToggleStatusbar)
EVT_MENU_RANGE(IDM_LOGWINDOW, IDM_VIDEOWINDOW, CFrame::OnToggleWindow)
EVT_MENU_RANGE(IDM_SHOW_SYSTEM, IDM_SHOW_STATE, CFrame::OnChangeColumnsVisible)

EVT_MENU(IDM_PURGECACHE, CFrame::GameListChanged)

EVT_MENU(IDM_SAVEFIRSTSTATE, CFrame::OnSaveFirstState)
EVT_MENU(IDM_UNDOLOADSTATE,     CFrame::OnUndoLoadState)
EVT_MENU(IDM_UNDOSAVESTATE,     CFrame::OnUndoSaveState)
EVT_MENU(IDM_LOADSTATEFILE, CFrame::OnLoadStateFromFile)
EVT_MENU(IDM_SAVESTATEFILE, CFrame::OnSaveStateToFile)
EVT_MENU(IDM_SAVESELECTEDSLOT, CFrame::OnSaveCurrentSlot)
EVT_MENU(IDM_LOADSELECTEDSLOT, CFrame::OnLoadCurrentSlot)

EVT_MENU_RANGE(IDM_LOADSLOT1, IDM_LOADSLOT10, CFrame::OnLoadState)
EVT_MENU_RANGE(IDM_LOADLAST1, IDM_LOADLAST8, CFrame::OnLoadLastState)
EVT_MENU_RANGE(IDM_SAVESLOT1, IDM_SAVESLOT10, CFrame::OnSaveState)
EVT_MENU_RANGE(IDM_SELECTSLOT1, IDM_SELECTSLOT10, CFrame::OnSelectSlot)
EVT_MENU_RANGE(IDM_FRAMESKIP0, IDM_FRAMESKIP9, CFrame::OnFrameSkip)
EVT_MENU_RANGE(IDM_DRIVE1, IDM_DRIVE24, CFrame::OnBootDrive)
EVT_MENU_RANGE(IDM_CONNECT_WIIMOTE1, IDM_CONNECT_BALANCEBOARD, CFrame::OnConnectWiimote)
EVT_MENU_RANGE(IDM_LISTWAD, IDM_LISTDRIVES, CFrame::GameListChanged)

// Other
EVT_ACTIVATE(CFrame::OnActive)
EVT_CLOSE(CFrame::OnClose)
EVT_SIZE(CFrame::OnResize)
EVT_MOVE(CFrame::OnMove)
EVT_LIST_ITEM_ACTIVATED(LIST_CTRL, CFrame::OnGameListCtrl_ItemActivated)
EVT_HOST_COMMAND(wxID_ANY, CFrame::OnHostMessage)

EVT_AUI_PANE_CLOSE(CFrame::OnPaneClose)
EVT_AUINOTEBOOK_PAGE_CLOSE(wxID_ANY, CFrame::OnNotebookPageClose)
EVT_AUINOTEBOOK_ALLOW_DND(wxID_ANY, CFrame::OnAllowNotebookDnD)
EVT_AUINOTEBOOK_PAGE_CHANGED(wxID_ANY, CFrame::OnNotebookPageChanged)
EVT_AUINOTEBOOK_TAB_RIGHT_UP(wxID_ANY, CFrame::OnTab)

// Post events to child panels
EVT_MENU_RANGE(IDM_INTERPRETER, IDM_ADDRBOX, CFrame::PostEvent)
EVT_TEXT(IDM_ADDRBOX, CFrame::PostEvent)

END_EVENT_TABLE()

// ---------------
// Creation and close, quit functions

CFrame::CFrame(wxFrame* parent,
		wxWindowID id,
		const wxString& title,
		const wxPoint& pos,
		const wxSize& size,
		bool _UseDebugger,
		bool _BatchMode,
		bool ShowLogWindow,
		long style)
	: CRenderFrame(parent, id, title, pos, size, style)
	, g_pCodeWindow(nullptr), g_NetPlaySetupDiag(nullptr), g_CheatsWindow(nullptr)
	, m_SavedPerspectives(nullptr), m_ToolBar(nullptr)
	, m_GameListCtrl(nullptr), m_Panel(nullptr)
	, m_RenderFrame(nullptr), m_RenderParent(nullptr)
	, m_LogWindow(nullptr), m_LogConfigWindow(nullptr)
	, m_FifoPlayerDlg(nullptr), UseDebugger(_UseDebugger)
	, m_bBatchMode(_BatchMode), m_bEdit(false), m_bTabSplit(false), m_bNoDocking(false)
	, m_bGameLoading(false), m_bClosing(false), m_confirmStop(false), m_menubar_shadow(nullptr)
{
	for (int i = 0; i <= IDM_CODEWINDOW - IDM_LOGWINDOW; i++)
		bFloatWindow[i] = false;

	if (ShowLogWindow)
		SConfig::GetInstance().m_InterfaceLogWindow = true;

	// Start debugging maximized
	if (UseDebugger)
		this->Maximize(true);

	// Debugger class
	if (UseDebugger)
	{
		g_pCodeWindow = new CCodeWindow(SConfig::GetInstance().m_LocalCoreStartupParameter, this, IDM_CODEWINDOW);
		LoadIniPerspectives();
		g_pCodeWindow->Load();
	}

	// Create toolbar bitmaps
	InitBitmaps();

	// Give it a status bar
	SetStatusBar(CreateStatusBar(2, wxST_SIZEGRIP, ID_STATUSBAR));
	if (!SConfig::GetInstance().m_InterfaceStatusbar)
		GetStatusBar()->Hide();

	// Give it a menu bar
	wxMenuBar* menubar_active = CreateMenu();
	SetMenuBar(menubar_active);
	// Create a menubar to service requests while the real menubar is hidden from the screen
	m_menubar_shadow = CreateMenu();

	// ---------------
	// Main panel
	// This panel is the parent for rendering and it holds the gamelistctrl
	m_Panel = new wxPanel(this, IDM_MPANEL, wxDefaultPosition, wxDefaultSize, 0);

	m_GameListCtrl = new CGameListCtrl(m_Panel, LIST_CTRL,
			wxDefaultPosition, wxDefaultSize,
			wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT);

	wxBoxSizer *sizerPanel = new wxBoxSizer(wxHORIZONTAL);
	sizerPanel->Add(m_GameListCtrl, 1, wxEXPAND | wxALL);
	m_Panel->SetSizer(sizerPanel);
	// ---------------

	// Manager
	m_Mgr = new wxAuiManager(this, wxAUI_MGR_DEFAULT | wxAUI_MGR_LIVE_RESIZE);

	m_Mgr->AddPane(m_Panel, wxAuiPaneInfo()
			.Name("Pane 0").Caption("Pane 0").PaneBorder(false)
			.CaptionVisible(false).Layer(0).Center().Show());
	if (!g_pCodeWindow)
		m_Mgr->AddPane(CreateEmptyNotebook(), wxAuiPaneInfo()
				.Name("Pane 1").Caption(_("Logging")).CaptionVisible(true)
				.Layer(0).FloatingSize(wxSize(600, 350)).CloseButton(true).Hide());
	AuiFullscreen = m_Mgr->SavePerspective();

	// Create toolbar
	RecreateToolbar();
	if (!SConfig::GetInstance().m_InterfaceToolbar) DoToggleToolbar(false);

	m_LogWindow = new CLogWindow(this, IDM_LOGWINDOW);
	m_LogWindow->Hide();
	m_LogWindow->Disable();

	g_TASInputDlg[0] = new TASInputDlg(this);
	g_TASInputDlg[1] = new TASInputDlg(this);
	g_TASInputDlg[2] = new TASInputDlg(this);
	g_TASInputDlg[3] = new TASInputDlg(this);

	Movie::SetInputManip(TASManipFunction);

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

	// Show window
	Show();

	// Commit
	m_Mgr->Update();

	#ifdef _WIN32
		SetToolTip("");
		GetToolTip()->SetAutoPop(25000);
	#endif

	#if defined(HAVE_XRANDR) && HAVE_XRANDR
		m_XRRConfig = new X11Utils::XRRConfiguration(X11Utils::XDisplayFromHandle(GetHandle()),
				X11Utils::XWindowFromHandle(GetHandle()));
	#endif

	// -------------------------
	// Connect event handlers

	m_Mgr->Bind(wxEVT_AUI_RENDER, &CFrame::OnManagerResize, this);
	// ----------

	// Update controls
	UpdateGUI();
	if (g_pCodeWindow)
		g_pCodeWindow->UpdateButtonStates();
}
// Destructor
CFrame::~CFrame()
{
	drives.clear();

	#if defined(HAVE_XRANDR) && HAVE_XRANDR
		delete m_XRRConfig;
	#endif

	ClosePages();

	delete m_Mgr;

	// This object is owned by us, not wxw
	m_menubar_shadow->Destroy();
	m_menubar_shadow = nullptr;
}

bool CFrame::RendererIsFullscreen()
{
	bool fullscreen = false;

	if (Core::GetState() == Core::CORE_RUN || Core::GetState() == Core::CORE_PAUSE)
	{
		fullscreen = m_RenderFrame->IsFullScreen();
	}

#if defined(__APPLE__)
	if (m_RenderFrame != nullptr)
	{
		NSView *view = (NSView *) m_RenderFrame->GetHandle();
		NSWindow *window = [view window];

		fullscreen = (([window styleMask] & NSFullScreenWindowMask) == NSFullScreenWindowMask);
	}
#endif

	return fullscreen;
}

void CFrame::OnQuit(wxCommandEvent& WXUNUSED (event))
{
	Close(true);
}

// --------
// Events
void CFrame::OnActive(wxActivateEvent& event)
{
	if (Core::GetState() == Core::CORE_RUN || Core::GetState() == Core::CORE_PAUSE)
	{
		if (event.GetActive() && event.GetEventObject() == m_RenderFrame)
		{
			if (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
			{
#ifdef __WXMSW__
				::SetFocus((HWND)m_RenderParent->GetHandle());
#else
				m_RenderParent->SetFocus();
#endif
			}

			if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor &&
					Core::GetState() == Core::CORE_RUN)
				m_RenderParent->SetCursor(wxCURSOR_BLANK);
		}
		else
		{
			if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor)
				m_RenderParent->SetCursor(wxNullCursor);
		}
	}
	event.Skip();
}

void CFrame::OnClose(wxCloseEvent& event)
{
	m_bClosing = true;

	// Before closing the window we need to shut down the emulation core.
	// We'll try to close this window again once that is done.
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		DoStop();
		if (event.CanVeto())
		{
			event.Veto();
		}
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
		// Close the log window now so that its settings are saved
		m_LogWindow->Close();
		m_LogWindow = nullptr;
	}


	// Uninit
	m_Mgr->UnInit();
}

// Post events

// Warning: This may cause an endless loop if the event is propagated back to its parent
void CFrame::PostEvent(wxCommandEvent& event)
{
	if (g_pCodeWindow &&
		event.GetId() >= IDM_INTERPRETER &&
		event.GetId() <= IDM_ADDRBOX)
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

	if (!IsMaximized() &&
		!(SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain && RendererIsFullscreen()))
	{
		SConfig::GetInstance().m_LocalCoreStartupParameter.iPosX = GetPosition().x;
		SConfig::GetInstance().m_LocalCoreStartupParameter.iPosY = GetPosition().y;
	}
}

void CFrame::OnResize(wxSizeEvent& event)
{
	event.Skip();

	if (!IsMaximized() &&
		!(SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain && RendererIsFullscreen()) &&
		!(Core::GetState() != Core::CORE_UNINITIALIZED &&
			SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain &&
			SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderWindowAutoSize))
	{
		SConfig::GetInstance().m_LocalCoreStartupParameter.iWidth = GetSize().GetWidth();
		SConfig::GetInstance().m_LocalCoreStartupParameter.iHeight = GetSize().GetHeight();
	}

	// Make sure the logger pane is a sane size
	if (!g_pCodeWindow && m_LogWindow && m_Mgr->GetPane("Pane 1").IsShown() &&
			!m_Mgr->GetPane("Pane 1").IsFloating() &&
			(m_LogWindow->x > GetClientRect().GetWidth() ||
			 m_LogWindow->y > GetClientRect().GetHeight()))
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

void CFrame::UpdateTitle(const std::string &str)
{
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain &&
	    SConfig::GetInstance().m_InterfaceStatusbar)
	{
		GetStatusBar()->SetStatusText(str, 0);
		m_RenderFrame->SetTitle(scm_rev_str);
	}
	else
	{
		std::string titleStr = StringFromFormat("%s | %s", scm_rev_str, str.c_str());
		m_RenderFrame->SetTitle(titleStr);
	}
}

void CFrame::OnHostMessage(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case IDM_UPDATEGUI:
		UpdateGUI();
		break;

	case IDM_UPDATESTATUSBAR:
		if (GetStatusBar() != nullptr)
			GetStatusBar()->SetStatusText(event.GetString(), event.GetInt());
		break;

	case IDM_UPDATETITLE:
		UpdateTitle(WxStrToStr(event.GetString()));
		break;

	case IDM_WINDOWSIZEREQUEST:
		{
			std::pair<int, int> *win_size = (std::pair<int, int> *)(event.GetClientData());
			OnRenderWindowSizeRequest(win_size->first, win_size->second);
			delete win_size;
		}
		break;

	case IDM_FULLSCREENREQUEST:
		{
			bool enable_fullscreen = event.GetInt() == 0 ? false : true;
			ToggleDisplayMode(enable_fullscreen);
			if (m_RenderFrame != nullptr)
				m_RenderFrame->ShowFullScreen(enable_fullscreen);

			// If the stop dialog initiated this fullscreen switch then we need
			// to pause the emulator after we've completed the switch.
			// TODO: Allow the renderer to switch fullscreen modes while paused.
			if (m_confirmStop)
				Core::SetState(Core::CORE_PAUSE);
		}
		break;

	case WM_USER_CREATE:
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor)
			m_RenderParent->SetCursor(wxCURSOR_BLANK);
		break;

#ifdef __WXGTK__
	case IDM_PANIC:
		{
			wxString caption = event.GetString().BeforeFirst(':');
			wxString text = event.GetString().AfterFirst(':');
			bPanicResult = (wxYES == wxMessageBox(text,
						caption, event.GetInt() ? wxYES_NO : wxOK, wxWindow::FindFocus()));
			panic_event.Set();
		}
		break;
#endif

	case WM_USER_STOP:
		DoStop();
		break;

	case IDM_STOPPED:
		OnStopped();
		break;
	}
}

void CFrame::GetRenderWindowSize(int& x, int& y, int& width, int& height)
{
#ifdef __WXGTK__
	if (!wxIsMainThread())
		wxMutexGuiEnter();
#endif
	wxRect client_rect = m_RenderParent->GetClientRect();
	width = client_rect.width;
	height = client_rect.height;
	x = client_rect.x;
	y = client_rect.y;
#ifdef __WXGTK__
	if (!wxIsMainThread())
		wxMutexGuiLeave();
#endif
}

void CFrame::OnRenderWindowSizeRequest(int width, int height)
{
	if (!Core::IsRunning() ||
			!SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderWindowAutoSize ||
			RendererIsFullscreen() || m_RenderFrame->IsMaximized())
		return;

	int old_width, old_height, log_width = 0, log_height = 0;
	m_RenderFrame->GetClientSize(&old_width, &old_height);

	// Add space for the log/console/debugger window
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain &&
			(SConfig::GetInstance().m_InterfaceLogWindow ||
			 SConfig::GetInstance().m_InterfaceLogConfigWindow) &&
			!m_Mgr->GetPane("Pane 1").IsFloating())
	{
		switch (m_Mgr->GetPane("Pane 1").dock_direction)
		{
			case wxAUI_DOCK_LEFT:
			case wxAUI_DOCK_RIGHT:
				log_width = m_Mgr->GetPane("Pane 1").rect.GetWidth();
				break;
			case wxAUI_DOCK_TOP:
			case wxAUI_DOCK_BOTTOM:
				log_height = m_Mgr->GetPane("Pane 1").rect.GetHeight();
				break;
		}
	}

	if (old_width != width + log_width || old_height != height + log_height)
		m_RenderFrame->SetClientSize(width + log_width, height + log_height);
}

bool CFrame::RendererHasFocus()
{
	if (m_RenderParent == nullptr)
		return false;
#ifdef _WIN32
	HWND window = GetForegroundWindow();
	if (window == nullptr)
		return false;

	if (m_RenderFrame->GetHWND() == window)
		return true;
#else
	wxWindow *window = wxWindow::FindFocus();
	if (window == nullptr)
		return false;
	// Why these different cases?
	if (m_RenderParent == window ||
	    m_RenderParent == window->GetParent() ||
	    m_RenderParent->GetParent() == window->GetParent())
	{
		return true;
	}
#endif
	return false;
}

bool CFrame::UIHasFocus()
{
	// UIHasFocus should return true any time any one of our UI
	// windows has the focus, including any dialogs or other windows.
	//
	// wxWindow::FindFocus() returns the current wxWindow which has
	// focus. If it's not one of our windows, then it will return
	// null.

	wxWindow *focusWindow = wxWindow::FindFocus();
	return (focusWindow != nullptr);
}

void CFrame::OnGameListCtrl_ItemActivated(wxListEvent& WXUNUSED (event))
{
	// Show all platforms and regions if...
	// 1. All platforms are set to hide
	// 2. All Regions are set to hide
	// Otherwise call BootGame to either...
	// 1. Boot the selected iso
	// 2. Boot the default or last loaded iso.
	// 3. Call BrowseForDirectory if the gamelist is empty
	if (!m_GameListCtrl->GetISO(0) &&
		!((SConfig::GetInstance().m_ListGC &&
		SConfig::GetInstance().m_ListWii &&
		SConfig::GetInstance().m_ListWad) &&
		(SConfig::GetInstance().m_ListJap &&
		SConfig::GetInstance().m_ListUsa  &&
		SConfig::GetInstance().m_ListPal  &&
		SConfig::GetInstance().m_ListFrance &&
		SConfig::GetInstance().m_ListItaly &&
		SConfig::GetInstance().m_ListKorea &&
		SConfig::GetInstance().m_ListTaiwan &&
		SConfig::GetInstance().m_ListUnknown)))
	{
		SConfig::GetInstance().m_ListGC      = SConfig::GetInstance().m_ListWii =
		SConfig::GetInstance().m_ListWad     = SConfig::GetInstance().m_ListJap =
		SConfig::GetInstance().m_ListUsa     = SConfig::GetInstance().m_ListPal =
		SConfig::GetInstance().m_ListFrance  = SConfig::GetInstance().m_ListItaly =
		SConfig::GetInstance().m_ListKorea   = SConfig::GetInstance().m_ListTaiwan =
		SConfig::GetInstance().m_ListUnknown = true;

		GetMenuBar()->FindItem(IDM_LISTGC)->Check(true);
		GetMenuBar()->FindItem(IDM_LISTWII)->Check(true);
		GetMenuBar()->FindItem(IDM_LISTWAD)->Check(true);
		GetMenuBar()->FindItem(IDM_LISTJAP)->Check(true);
		GetMenuBar()->FindItem(IDM_LISTUSA)->Check(true);
		GetMenuBar()->FindItem(IDM_LISTPAL)->Check(true);
		GetMenuBar()->FindItem(IDM_LISTFRANCE)->Check(true);
		GetMenuBar()->FindItem(IDM_LISTITALY)->Check(true);
		GetMenuBar()->FindItem(IDM_LISTKOREA)->Check(true);
		GetMenuBar()->FindItem(IDM_LISTTAIWAN)->Check(true);
		GetMenuBar()->FindItem(IDM_LIST_UNK)->Check(true);

		m_GameListCtrl->Update();
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

static bool IsHotkey(wxKeyEvent &event, int Id)
{
	return (event.GetKeyCode() != WXK_NONE &&
			event.GetKeyCode() == SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkey[Id] &&
			event.GetModifiers() == SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkeyModifier[Id]);
}

int GetCmdForHotkey(unsigned int key)
{
	switch (key)
	{
	case HK_OPEN: return wxID_OPEN;
	case HK_CHANGE_DISC: return IDM_CHANGEDISC;
	case HK_REFRESH_LIST: return wxID_REFRESH;
	case HK_PLAY_PAUSE: return IDM_PLAY;
	case HK_STOP: return IDM_STOP;
	case HK_RESET: return IDM_RESET;
	case HK_FRAME_ADVANCE: return IDM_FRAMESTEP;
	case HK_START_RECORDING: return IDM_RECORD;
	case HK_PLAY_RECORDING: return IDM_PLAYRECORD;
	case HK_EXPORT_RECORDING: return IDM_RECORDEXPORT;
	case HK_READ_ONLY_MODE: return IDM_RECORDREADONLY;
	case HK_FULLSCREEN: return IDM_TOGGLE_FULLSCREEN;
	case HK_SCREENSHOT: return IDM_SCREENSHOT;
	case HK_EXIT: return wxID_EXIT;

	case HK_WIIMOTE1_CONNECT: return IDM_CONNECT_WIIMOTE1;
	case HK_WIIMOTE2_CONNECT: return IDM_CONNECT_WIIMOTE2;
	case HK_WIIMOTE3_CONNECT: return IDM_CONNECT_WIIMOTE3;
	case HK_WIIMOTE4_CONNECT: return IDM_CONNECT_WIIMOTE4;
	case HK_BALANCEBOARD_CONNECT: return IDM_CONNECT_BALANCEBOARD;

	case HK_LOAD_STATE_SLOT_1: return IDM_LOADSLOT1;
	case HK_LOAD_STATE_SLOT_2: return IDM_LOADSLOT2;
	case HK_LOAD_STATE_SLOT_3: return IDM_LOADSLOT3;
	case HK_LOAD_STATE_SLOT_4: return IDM_LOADSLOT4;
	case HK_LOAD_STATE_SLOT_5: return IDM_LOADSLOT5;
	case HK_LOAD_STATE_SLOT_6: return IDM_LOADSLOT6;
	case HK_LOAD_STATE_SLOT_7: return IDM_LOADSLOT7;
	case HK_LOAD_STATE_SLOT_8: return IDM_LOADSLOT8;
	case HK_LOAD_STATE_SLOT_9: return IDM_LOADSLOT9;
	case HK_LOAD_STATE_SLOT_10: return IDM_LOADSLOT10;

	case HK_SAVE_STATE_SLOT_1: return IDM_SAVESLOT1;
	case HK_SAVE_STATE_SLOT_2: return IDM_SAVESLOT2;
	case HK_SAVE_STATE_SLOT_3: return IDM_SAVESLOT3;
	case HK_SAVE_STATE_SLOT_4: return IDM_SAVESLOT4;
	case HK_SAVE_STATE_SLOT_5: return IDM_SAVESLOT5;
	case HK_SAVE_STATE_SLOT_6: return IDM_SAVESLOT6;
	case HK_SAVE_STATE_SLOT_7: return IDM_SAVESLOT7;
	case HK_SAVE_STATE_SLOT_8: return IDM_SAVESLOT8;
	case HK_SAVE_STATE_SLOT_9: return IDM_SAVESLOT9;
	case HK_SAVE_STATE_SLOT_10: return IDM_SAVESLOT10;

	case HK_LOAD_LAST_STATE_1: return IDM_LOADLAST1;
	case HK_LOAD_LAST_STATE_2: return IDM_LOADLAST2;
	case HK_LOAD_LAST_STATE_3: return IDM_LOADLAST3;
	case HK_LOAD_LAST_STATE_4: return IDM_LOADLAST4;
	case HK_LOAD_LAST_STATE_5: return IDM_LOADLAST5;
	case HK_LOAD_LAST_STATE_6: return IDM_LOADLAST6;
	case HK_LOAD_LAST_STATE_7: return IDM_LOADLAST7;
	case HK_LOAD_LAST_STATE_8: return IDM_LOADLAST8;

	case HK_SAVE_FIRST_STATE: return IDM_SAVEFIRSTSTATE;
	case HK_UNDO_LOAD_STATE: return IDM_UNDOLOADSTATE;
	case HK_UNDO_SAVE_STATE: return IDM_UNDOSAVESTATE;
	case HK_LOAD_STATE_FILE: return IDM_LOADSTATEFILE;
	case HK_SAVE_STATE_FILE: return IDM_SAVESTATEFILE;

	case HK_SELECT_STATE_SLOT_1: return IDM_SELECTSLOT1;
	case HK_SELECT_STATE_SLOT_2: return IDM_SELECTSLOT2;
	case HK_SELECT_STATE_SLOT_3: return IDM_SELECTSLOT3;
	case HK_SELECT_STATE_SLOT_4: return IDM_SELECTSLOT4;
	case HK_SELECT_STATE_SLOT_5: return IDM_SELECTSLOT5;
	case HK_SELECT_STATE_SLOT_6: return IDM_SELECTSLOT6;
	case HK_SELECT_STATE_SLOT_7: return IDM_SELECTSLOT7;
	case HK_SELECT_STATE_SLOT_8: return IDM_SELECTSLOT8;
	case HK_SELECT_STATE_SLOT_9: return IDM_SELECTSLOT9;
	case HK_SELECT_STATE_SLOT_10: return IDM_SELECTSLOT10;
	case HK_SAVE_STATE_SLOT_SELECTED: return IDM_SAVESELECTEDSLOT;
	case HK_LOAD_STATE_SLOT_SELECTED: return IDM_LOADSELECTEDSLOT;
	}

	return -1;
}

void OnAfterLoadCallback()
{
	// warning: this gets called from the CPU thread, so we should only queue things to do on the proper thread
	if (main_frame)
	{
		wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATEGUI);
		main_frame->GetEventHandler()->AddPendingEvent(event);
	}
}

void OnStoppedCallback()
{
	// warning: this gets called from the EmuThread, so we should only queue things to do on the proper thread
	if (main_frame)
	{
		wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_STOPPED);
		main_frame->GetEventHandler()->AddPendingEvent(event);
	}
}

void TASManipFunction(GCPadStatus* PadStatus, int controllerID)
{
	if (main_frame)
		main_frame->g_TASInputDlg[controllerID]->GetValues(PadStatus, controllerID);
}

bool TASInputHasFocus()
{
	for (int i = 0; i < 4; i++)
	{
		if (main_frame->g_TASInputDlg[i]->TASHasFocus())
			return true;
	}
	return false;
}


void CFrame::OnKeyDown(wxKeyEvent& event)
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED &&
	    (RendererHasFocus() || TASInputHasFocus()))
	{
		int WiimoteId = -1;
		// Toggle fullscreen
		if (IsHotkey(event, HK_FULLSCREEN))
			DoFullscreen(!RendererIsFullscreen());
		// Send Debugger keys to CodeWindow
		else if (g_pCodeWindow && (event.GetKeyCode() >= WXK_F9 && event.GetKeyCode() <= WXK_F11))
			event.Skip();
		// Pause and Unpause
		else if (IsHotkey(event, HK_PLAY_PAUSE))
			DoPause();
		// Stop
		else if (IsHotkey(event, HK_STOP))
			DoStop();
		// Screenshot hotkey
		else if (IsHotkey(event, HK_SCREENSHOT))
			Core::SaveScreenShot();
		else if (IsHotkey(event, HK_EXIT))
			wxPostEvent(this, wxCommandEvent(wxID_EXIT));
		// Wiimote connect and disconnect hotkeys
		else if (IsHotkey(event, HK_WIIMOTE1_CONNECT))
			WiimoteId = 0;
		else if (IsHotkey(event, HK_WIIMOTE2_CONNECT))
			WiimoteId = 1;
		else if (IsHotkey(event, HK_WIIMOTE3_CONNECT))
			WiimoteId = 2;
		else if (IsHotkey(event, HK_WIIMOTE4_CONNECT))
			WiimoteId = 3;
		else if (IsHotkey(event, HK_BALANCEBOARD_CONNECT))
			WiimoteId = 4;
		else if (IsHotkey(event, HK_TOGGLE_IR))
		{
			OSDChoice = 1;
			// Toggle native resolution
			if (++g_Config.iEFBScale > SCALE_4X)
				g_Config.iEFBScale = SCALE_AUTO;
		}
		else if (IsHotkey(event, HK_TOGGLE_AR))
		{
			OSDChoice = 2;
			// Toggle aspect ratio
			g_Config.iAspectRatio = (g_Config.iAspectRatio + 1) & 3;
		}
		else if (IsHotkey(event, HK_TOGGLE_EFBCOPIES))
		{
			OSDChoice = 3;
			// Toggle EFB copies between EFB2RAM and EFB2Texture
			if (!g_Config.bEFBCopyEnable)
			{
				OSD::AddMessage("EFB Copies are disabled, enable them in Graphics settings for toggling", 6000);
			}
			else
			{
				g_Config.bCopyEFBToTexture = !g_Config.bCopyEFBToTexture;
			}
		}
		else if (IsHotkey(event, HK_TOGGLE_FOG))
		{
			OSDChoice = 4;
			g_Config.bDisableFog = !g_Config.bDisableFog;
		}
		else if (IsHotkey(event, HK_TOGGLE_THROTTLE))
		{
			Core::SetIsFramelimiterTempDisabled(true);
		}
		else if (IsHotkey(event, HK_INCREASE_FRAME_LIMIT))
		{
			if (++SConfig::GetInstance().m_Framelimit > 0x19)
				SConfig::GetInstance().m_Framelimit = 0;
		}
		else if (IsHotkey(event, HK_DECREASE_FRAME_LIMIT))
		{
			if (--SConfig::GetInstance().m_Framelimit > 0x19)
				SConfig::GetInstance().m_Framelimit = 0x19;
		}
		else if (IsHotkey(event, HK_SAVE_STATE_SLOT_SELECTED))
		{
			State::Save(g_saveSlot);
		}
		else if (IsHotkey(event, HK_LOAD_STATE_SLOT_SELECTED))
		{
			State::Load(g_saveSlot);
		}

		else
		{
			for (int i = HK_SELECT_STATE_SLOT_1; i < HK_SELECT_STATE_SLOT_10; ++i)
			{
				if (IsHotkey (event, i))
				{
					wxCommandEvent slot_event;
					slot_event.SetId(i + IDM_SELECTSLOT1 - HK_SELECT_STATE_SLOT_1);
					CFrame::OnSelectSlot(slot_event);
				}
			}

			unsigned int i = NUM_HOTKEYS;
			if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain || TASInputHasFocus())
			{
				for (i = 0; i < NUM_HOTKEYS; i++)
				{
					if (IsHotkey(event, i))
					{
						int cmd = GetCmdForHotkey(i);
						if (cmd >= 0)
						{
							wxCommandEvent evt(wxEVT_MENU, cmd);
							wxMenuItem *item = GetMenuBar()->FindItem(cmd);
							if (item && item->IsCheckable())
							{
								item->wxMenuItemBase::Toggle();
								evt.SetInt(item->IsChecked());
							}
							GetEventHandler()->AddPendingEvent(evt);
							break;
						}
					}
				}
			}
			// On OS X, we claim all keyboard events while
			// emulation is running to avoid wxWidgets sounding
			// the system beep for unhandled key events when
			// receiving pad/wiimote keypresses which take an
			// entirely different path through the HID subsystem.
#ifndef __APPLE__
			// On other platforms, we leave the key event alone
			// so it can be passed on to the windowing system.
			if (i == NUM_HOTKEYS)
				event.Skip();
#endif
		}

		// Actually perform the wiimote connection or disconnection
		if (WiimoteId >= 0)
		{
			bool connect = !GetMenuBar()->IsChecked(IDM_CONNECT_WIIMOTE1 + WiimoteId);
			ConnectWiimote(WiimoteId, connect);
		}

		if (g_Config.bFreeLook && event.GetModifiers() == wxMOD_SHIFT)
		{
			static float debugSpeed = 1.0f;
			switch (event.GetKeyCode())
			{
			case '9':
				debugSpeed /= 2.0f;
				break;
			case '0':
				debugSpeed *= 2.0f;
				break;
			case 'W':
				VertexShaderManager::TranslateView(0.0f, debugSpeed);
				break;
			case 'S':
				VertexShaderManager::TranslateView(0.0f, -debugSpeed);
				break;
			case 'A':
				VertexShaderManager::TranslateView(debugSpeed, 0.0f);
				break;
			case 'D':
				VertexShaderManager::TranslateView(-debugSpeed, 0.0f);
				break;
			case 'Q':
				VertexShaderManager::TranslateView(0.0f, 0.0f, debugSpeed);
				break;
			case 'E':
				VertexShaderManager::TranslateView(0.0f, 0.0f, -debugSpeed);
				break;
			case 'R':
				VertexShaderManager::ResetView();
				break;
			default:
				break;
			}
		}
	}
	else
	{
		event.Skip();
	}
}

void CFrame::OnKeyUp(wxKeyEvent& event)
{
	if(Core::IsRunning() && (RendererHasFocus() || TASInputHasFocus()))
	{
		if (IsHotkey(event, HK_TOGGLE_THROTTLE))
		{
			Core::SetIsFramelimiterTempDisabled(false);
		}
	}
	else
	{
		event.Skip();
	}
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
	if (!g_Config.BorderlessFullscreenEnabled() &&
		!SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain &&
		Core::GetState() == Core::CORE_PAUSE)
	{
		// A responsive renderer is required for exclusive fullscreen, but the
		// renderer can only respond in the running state. Therefore we ignore
		// fullscreen switches if we support exclusive fullscreen, but the
		// renderer is not running.
		// TODO: Allow the renderer to switch fullscreen modes while paused.
		return;
	}

	ToggleDisplayMode(enable_fullscreen);

#if defined(__APPLE__)
	NSView *view = (NSView *) m_RenderFrame->GetHandle();
	NSWindow *window = [view window];

	if (enable_fullscreen != RendererIsFullscreen())
	{
		[window toggleFullScreen:nil];
	}
#else
	if (enable_fullscreen)
	{
		m_RenderFrame->ShowFullScreen(true, wxFULLSCREEN_ALL);
	}
	else if (g_Config.BorderlessFullscreenEnabled() ||
		SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
	{
		// Exiting exclusive fullscreen should be done from a Renderer callback.
		// Therefore we don't exit fullscreen from here if we support exclusive mode.
		m_RenderFrame->ShowFullScreen(false, wxFULLSCREEN_ALL);
	}
#endif

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
	{
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
				SetMenuBar(CreateMenu());
			}

			// Show statusbar if enabled
			if (SConfig::GetInstance().m_InterfaceStatusbar)
			{
				GetStatusBar()->Show();
				this->SendSizeEvent();
			}
		}
	}
	else
	{
		m_RenderFrame->Raise();
	}

	g_Config.bFullscreen = (g_Config.BorderlessFullscreenEnabled() ||
		SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain) ? false : enable_fullscreen;
}

const CGameListCtrl *CFrame::GetGameListCtrl() const
{
	return m_GameListCtrl;
}
