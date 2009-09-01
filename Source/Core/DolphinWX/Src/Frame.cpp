// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/



//////////////////////////////////////////////////////////////////////////////////////////
// Windows
/* ¯¯¯¯¯¯¯¯¯¯¯¯¯¯

CFrame is the main parent window. Inside CFrame there is an m_Panel that is the parent for
the rendering window (when we render to the main window). In Windows the rendering window is
created by giving CreateWindow() m_Panel->GetHandle() as parent window and creating a new
child window to m_Panel. The new child window handle that is returned by CreateWindow() can
be accessed from Core::GetWindowHandle().

///////////////////////////////////////////////*/


// ----------------------------------------------------------------------------
// includes
// ----------------------------------------------------------------------------

#include "Common.h" // Common
#include "FileUtil.h"
#include "Timer.h"
#include "Setup.h"

#include "Globals.h" // Local
#include "Frame.h"
#include "ConfigMain.h"
#include "PluginManager.h"
#include "MemcardManager.h"
#include "CheatsWindow.h"
#include "AboutDolphin.h"
#include "GameListCtrl.h"
#include "BootManager.h"

#include "ConfigManager.h" // Core
#include "Core.h"
#include "HW/DVDInterface.h"
#include "State.h"
#include "VolumeHandler.h"

#include <wx/datetime.h> // wxWidgets

// ----------------------------------------------------------------------------
// resources
// ----------------------------------------------------------------------------

extern "C" {
#include "../resources/Dolphin.c" // Dolphin icon
#include "../resources/toolbar_browse.c"
#include "../resources/toolbar_file_open.c"
#include "../resources/toolbar_fullscreen.c"
#include "../resources/toolbar_help.c"
#include "../resources/toolbar_pause.c"
#include "../resources/toolbar_play.c"
#include "../resources/toolbar_plugin_dsp.c"
#include "../resources/toolbar_plugin_gfx.c"
#include "../resources/toolbar_plugin_options.c"
#include "../resources/toolbar_plugin_pad.c"
#include "../resources/toolbar_refresh.c"
#include "../resources/toolbar_stop.c"
#include "../resources/Boomy.h" // Theme packages
#include "../resources/Vista.h"
#include "../resources/X-Plastik.h"
#include "../resources/KDE.h"
};


/////////////////////////////////////////////////////////////////////////////////////////////////////////
/* Windows functions. Setting the cursor with wxSetCursor() did not work in this instance.
   Probably because it's somehow reset from the WndProc() in the child window */
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#ifdef _WIN32
// Declare a blank icon and one that will be the normal cursor
HCURSOR hCursor = NULL, hCursorBlank = NULL;

// Create the default cursor
void CreateCursor()
{
	hCursor = LoadCursor( NULL, IDC_ARROW );
}

void MSWSetCursor(bool Show)
{
	if(Show)
		SetCursor(hCursor);
	else
	{
		SetCursor(hCursorBlank);
		//wxSetCursor(wxCursor(wxNullCursor));
	}
}

// I could not use FindItemByHWND() instead of this, it crashed on that occation I used it */
HWND MSWGetParent_(HWND Parent)
{
	return GetParent(Parent);
}
#endif
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
/* The CPanel class to receive MSWWindowProc messages from the video plugin. */
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
extern CFrame* main_frame;

class CPanel : public wxPanel
{
	public:
		CPanel(
			wxWindow* parent,
			wxWindowID id = wxID_ANY
			);

	private:
		DECLARE_EVENT_TABLE();

		#ifdef _WIN32
			// Receive WndProc messages
			WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);
		#endif
};

BEGIN_EVENT_TABLE(CPanel, wxPanel)
END_EVENT_TABLE()

CPanel::CPanel(
			wxWindow *parent,
			wxWindowID id
			)
	: wxPanel(parent, id)
{
}
int abc = 0;
#ifdef _WIN32
	WXLRESULT CPanel::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
	{
		switch (nMsg)
		{
		//case WM_LBUTTONDOWN:
		//case WM_LBUTTONUP:
		//case WM_MOUSEMOVE:
		//	break;		

		// This doesn't work, strange
		//case WM_LBUTTONDBLCLK:
			//PanicAlert("Double click");
			//break;

		case WM_USER:
			switch(wParam)
			{
			// Stop
			case OPENGL_WM_USER_STOP:
				main_frame->DoStop();
				return 0; // Don't bother letting wxWidgets process this at all
			
			case OPENGL_WM_USER_CREATE:
				// We don't have a local setting for bRenderToMain but we can detect it this way instead
				//PanicAlert("main call %i  %i  %i  %i", lParam, (HWND)Core::GetWindowHandle(), MSWGetParent_((HWND)Core::GetWindowHandle()), (HWND)this->GetHWND());
				if (lParam == NULL)
					main_frame->bRenderToMain = false;
				else
					main_frame->bRenderToMain = true;
				return 0;

			case NJOY_RELOAD:
				// DirectInput in nJoy has failed
				Core::ReconnectPad();
				return 0;

			case WIIMOTE_RECONNECT:
				// The Wiimote plugin has been shut down, now reconnect the Wiimote
				//INFO_LOG(CONSOLE, "WIIMOTE_RECONNECT\n");
				Core::ReconnectWiimote();
				return 0;

			// -----------------------------------------
			#ifdef RERECORDING
			// -----------------
				case INPUT_FRAME_COUNTER:
					// Wind back the frame counter after a save state has been loaded
					Core::WindBack((int)lParam);
					return 0;
			#endif
			// -----------------------------
			}
			break;

		//default:
		//	return wxPanel::MSWWindowProc(nMsg, wParam, lParam);
		}
		
		// By default let wxWidgets do what it normally does with this event
		return wxPanel::MSWWindowProc(nMsg, wParam, lParam);
	}
#endif
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// event tables
// ----------------------------

// Notice that wxID_HELP will be processed for the 'About' menu and the toolbar
// help button.

const wxEventType wxEVT_HOST_COMMAND = wxNewEventType();

BEGIN_EVENT_TABLE(CFrame, wxFrame)
EVT_CLOSE(CFrame::OnClose)
EVT_MENU(wxID_OPEN, CFrame::OnOpen)
EVT_MENU(wxID_EXIT, CFrame::OnQuit)
EVT_MENU(IDM_HELPWEBSITE, CFrame::OnHelp)
EVT_MENU(IDM_HELPGOOGLECODE, CFrame::OnHelp)
EVT_MENU(IDM_HELPABOUT, CFrame::OnHelp)
EVT_MENU(wxID_REFRESH, CFrame::OnRefresh)
EVT_MENU(IDM_PLAY, CFrame::OnPlay)
EVT_MENU(IDM_RECORD, CFrame::OnRecord)
EVT_MENU(IDM_PLAYRECORD, CFrame::OnPlayRecording)
EVT_MENU(IDM_STOP, CFrame::OnStop)
EVT_MENU(IDM_FRAMESTEP, CFrame::OnFrameStep)
EVT_MENU(IDM_SCREENSHOT, CFrame::OnScreenshot)
EVT_MENU(IDM_CONFIG_MAIN, CFrame::OnConfigMain)
EVT_MENU(IDM_CONFIG_GFX_PLUGIN, CFrame::OnPluginGFX)
EVT_MENU(IDM_CONFIG_DSP_PLUGIN, CFrame::OnPluginDSP)
EVT_MENU(IDM_CONFIG_PAD_PLUGIN, CFrame::OnPluginPAD)
EVT_MENU(IDM_CONFIG_WIIMOTE_PLUGIN, CFrame::OnPluginWiimote)

EVT_AUITOOLBAR_TOOL_DROPDOWN(IDM_SAVE_PERSPECTIVE, CFrame::OnDropDownToolbarItem)
EVT_MENU(IDM_SAVE_PERSPECTIVE, CFrame::OnToolBar)
EVT_MENU(IDM_ADD_PERSPECTIVE, CFrame::OnCreatePerspective)
EVT_MENU(IDM_PERSPECTIVES_ADD_PANE, CFrame::OnToolBar)
EVT_MENU(IDM_EDIT_PERSPECTIVES, CFrame::OnToolBar)
EVT_MENU(IDM_TAB_SPLIT, CFrame::OnToolBar)
EVT_MENU_RANGE(IDM_PERSPECTIVES_0, IDM_PERSPECTIVES_100, CFrame::OnSelectPerspective)

#if defined(HAVE_SFML) && HAVE_SFML
EVT_MENU(IDM_NETPLAY, CFrame::OnNetPlay)
#endif

EVT_MENU(IDM_BROWSE, CFrame::OnBrowse)
EVT_MENU(IDM_MEMCARD, CFrame::OnMemcard)
EVT_MENU(IDM_CHEATS, CFrame::OnShow_CheatsWindow)
EVT_MENU(IDM_INFO, CFrame::OnShow_InfoWindow)
EVT_MENU(IDM_CHANGEDISC, CFrame::OnChangeDisc)
EVT_MENU(IDM_LOAD_WII_MENU, CFrame::OnLoadWiiMenu)
EVT_MENU(IDM_TOGGLE_FULLSCREEN, CFrame::OnToggleFullscreen)
EVT_MENU(IDM_TOGGLE_DUALCORE, CFrame::OnToggleDualCore)
EVT_MENU(IDM_TOGGLE_SKIPIDLE, CFrame::OnToggleSkipIdle)
EVT_MENU(IDM_TOGGLE_TOOLBAR, CFrame::OnToggleToolbar)
EVT_MENU(IDM_TOGGLE_STATUSBAR, CFrame::OnToggleStatusbar)
EVT_MENU(IDM_LOGWINDOW, CFrame::OnToggleLogWindow)
EVT_MENU(IDM_CONSOLEWINDOW, CFrame::OnToggleConsole)

EVT_MENU(IDM_LISTDRIVES, CFrame::GameListChanged)
EVT_MENU(IDM_LISTWII,	 CFrame::GameListChanged)
EVT_MENU(IDM_LISTGC,	 CFrame::GameListChanged)
EVT_MENU(IDM_LISTWAD,	 CFrame::GameListChanged)
EVT_MENU(IDM_LISTJAP,	 CFrame::GameListChanged)
EVT_MENU(IDM_LISTPAL,	 CFrame::GameListChanged)
EVT_MENU(IDM_LISTUSA,	 CFrame::GameListChanged)
EVT_MENU(IDM_PURGECACHE, CFrame::GameListChanged)

EVT_MENU(IDM_LOADLASTSTATE, CFrame::OnLoadLastState)
EVT_MENU(IDM_UNDOLOADSTATE,     CFrame::OnUndoLoadState)
EVT_MENU(IDM_UNDOSAVESTATE,     CFrame::OnUndoSaveState)
EVT_MENU(IDM_LOADSTATEFILE, CFrame::OnLoadStateFromFile)
EVT_MENU(IDM_SAVESTATEFILE, CFrame::OnSaveStateToFile)

EVT_MENU_RANGE(IDM_LOADSLOT1, IDM_LOADSLOT8, CFrame::OnLoadState)
EVT_MENU_RANGE(IDM_SAVESLOT1, IDM_SAVESLOT8, CFrame::OnSaveState)
EVT_MENU_RANGE(IDM_FRAMESKIP0, IDM_FRAMESKIP9, CFrame::OnFrameSkip)
EVT_MENU_RANGE(IDM_DRIVE1, IDM_DRIVE24, CFrame::OnBootDrive)

EVT_SIZE(CFrame::OnResize)
EVT_LIST_ITEM_ACTIVATED(LIST_CTRL, CFrame::OnGameListCtrl_ItemActivated)
EVT_HOST_COMMAND(wxID_ANY, CFrame::OnHostMessage)
#if wxUSE_TIMER
	EVT_TIMER(wxID_ANY, CFrame::OnTimer)
#endif

// Debugger Menu Entries
EVT_MENU(wxID_ANY, CFrame::PostEvent)
EVT_TEXT(wxID_ANY, CFrame::PostEvent)

//EVT_MENU_HIGHLIGHT_ALL(CFrame::PostMenuEvent)
//EVT_UPDATE_UI(wxID_ANY, CFrame::PostUpdateUIEvent)

EVT_AUI_PANE_CLOSE(CFrame::OnPaneClose)
EVT_AUINOTEBOOK_PAGE_CLOSE(wxID_ANY, CFrame::OnNotebookPageClose)
EVT_AUINOTEBOOK_ALLOW_DND(wxID_ANY, CFrame::OnAllowNotebookDnD)
EVT_AUINOTEBOOK_PAGE_CHANGED(wxID_ANY, CFrame::OnNotebookPageChanged)

END_EVENT_TABLE()
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Creation and close, quit functions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
CFrame::CFrame(bool showLogWindow,
		wxFrame* parent,
		wxWindowID id,
		const wxString& title,
		const wxPoint& pos,
		const wxSize& size,
		bool _UseDebugger,
		long style)
	: wxFrame(parent, id, title, pos, size, style)
	, UseDebugger(_UseDebugger), m_LogWindow(NULL)
	, m_GameListCtrl(NULL), m_pStatusBar(NULL), bRenderToMain(true), HaveLeds(false)
	, HaveSpeakers(false), m_Panel(NULL), m_ToolBar(NULL), m_ToolBarDebug(NULL)
	, m_bLogWindow(showLogWindow || SConfig::GetInstance().m_InterfaceLogWindow)
	, m_fLastClickTime(0), m_iLastMotionTime(0), LastMouseX(0), LastMouseY(0)
	#if wxUSE_TIMER
		, m_timer(this)
	#endif
          
{
	// Give it a console
	ConsoleListener *Console = LogManager::GetInstance()->getConsoleListener();
	if (SConfig::GetInstance().m_InterfaceConsole) Console->Open();

	// Start debugging mazimized
	if (UseDebugger) this->Maximize(true);
	// Debugger class
	if (UseDebugger)
	{
		g_pCodeWindow = new CCodeWindow(SConfig::GetInstance().m_LocalCoreStartupParameter, this, this);
		g_pCodeWindow->Hide();
		g_pCodeWindow->Load();
	}

	// Create timer
	#if wxUSE_TIMER
		int TimesPerSecond = 10; // We don't need more than this
		m_timer.Start( floor((double)(1000 / TimesPerSecond)) );
	#endif

	// Create toolbar bitmaps	
	InitBitmaps();

	// Give it an icon
	wxIcon IconTemp;
	IconTemp.CopyFromBitmap(wxGetBitmapFromMemory(dolphin_ico32x32));
	SetIcon(IconTemp);

	// Give it a status bar
	m_pStatusBar = CreateStatusBar(1, wxST_SIZEGRIP, ID_STATUSBAR);
	if (!SConfig::GetInstance().m_InterfaceStatusbar)
		m_pStatusBar->Hide();

	// Give it a menu bar
	CreateMenu();

	// -------------------------------------------------------------------------
	// Main panel
	// ¯¯¯¯¯¯¯¯¯¯¯¯¯
	// This panel is the parent for rendering and it holds the gamelistctrl
	m_Panel = new CPanel(this, IDM_MPANEL);

	m_GameListCtrl = new CGameListCtrl(m_Panel, LIST_CTRL,
			wxDefaultPosition, wxDefaultSize,
			wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT);

	sizerPanel = new wxBoxSizer(wxHORIZONTAL);
	sizerPanel->Add(m_GameListCtrl, 1, wxEXPAND | wxALL);
	m_Panel->SetSizer(sizerPanel);
	// -------------------------------------------------------------------------

	m_Mgr = new wxAuiManager();
	m_Mgr->SetManagedWindow(this);


	DefaultNBStyle = wxAUI_NB_TOP | wxAUI_NB_SCROLL_BUTTONS | wxAUI_NB_TAB_EXTERNAL_MOVE | wxNO_BORDER;
	wxBitmap aNormalFile = wxArtProvider::GetBitmap(wxART_NORMAL_FILE, wxART_OTHER, wxSize(16,16));

	if (UseDebugger)
	{
		m_Mgr->AddPane(m_Panel, wxAuiPaneInfo().Name(wxT("Pane 0")).Caption(wxT("Pane 0")).Show());
	}
	else
	{
		m_Mgr->AddPane(m_Panel, wxAuiPaneInfo().Name(wxT("Pane 0")).Caption(wxT("Pane 0")).Hide());
		m_Mgr->AddPane(CreateEmptyNotebook(), wxAuiPaneInfo().Name(wxT("Pane 1")).Caption(wxT("Pane 1")).Hide());
	}

	// Setup perspectives
	if (UseDebugger)
	{		
		m_Mgr->GetPane(wxT("Pane 0")).CenterPane().PaneBorder(false);
		AuiFullscreen = m_Mgr->SavePerspective();
		m_Mgr->GetPane(wxT("Pane 0")).CenterPane().PaneBorder(true);
	}
	else
	{
		m_Mgr->GetPane(wxT("Pane 0")).Show().PaneBorder(false).CaptionVisible(false).Layer(0).Center();
		AuiFullscreen = m_Mgr->SavePerspective();
	}

	// Create toolbar
	RecreateToolbar();
	if (!SConfig::GetInstance().m_InterfaceToolbar) DoToggleToolbar(false);

	// Create list of available plugins for the configuration window
	CPluginManager::GetInstance().ScanForPlugins();

	// Setup perspectives
	if (UseDebugger)
	{		
		// Load perspective
		SaveLocal();
		DoLoadPerspective();
	}
	else
	{
		m_Mgr->GetPane(wxT("Pane 1")).Hide().PaneBorder(false).CaptionVisible(false).Layer(0).Right();
	}

	// Show window
	Show();

	if (!UseDebugger)
	{
		SetSimplePaneSize();
		if (m_bLogWindow) DoToggleWindow(IDM_LOGWINDOW, true);
		if (SConfig::GetInstance().m_InterfaceConsole) DoToggleWindow(IDM_CONSOLEWINDOW, true);
	}

	//sizerPanel->SetSizeHints(m_Panel);

	// Commit 
	m_Mgr->Update();

	// Create cursors
	#ifdef _WIN32
		CreateCursor();
	#endif

	// -------------------------
	// Connect event handlers
	// ----------
	wxTheApp->Connect(wxID_ANY, wxEVT_KEY_DOWN, // Keyboard
		wxKeyEventHandler(CFrame::OnKeyDown),
		(wxObject*)0, this);
	wxTheApp->Connect(wxID_ANY, wxEVT_KEY_UP,
		wxKeyEventHandler(CFrame::OnKeyUp),
		(wxObject*)0, this);

	m_Mgr->Connect(wxID_ANY, wxEVT_AUI_RENDER, // Resize
		wxAuiManagerEventHandler(CFrame::OnManagerResize),
		(wxObject*)0, this);	

		wxTheApp->Connect(wxID_ANY, wxEVT_LEFT_DOWN,
			wxMouseEventHandler(CFrame::OnDoubleClick),
			(wxObject*)0, this);
		wxTheApp->Connect(wxID_ANY, wxEVT_MOTION,
			wxMouseEventHandler(CFrame::OnMotion),
			(wxObject*)0, this);
	// ----------

	// Update controls
	UpdateGUI();

	//if we are ever going back to optional iso caching:
	//m_GameListCtrl->Update(SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableIsoCache);
	if (m_GameListCtrl) m_GameListCtrl->Update();

	// If we are rerecording create the status bar now instead of later when a game starts
	#ifdef RERECORDING
		ModifyStatusBar();
		// It's to early for the OnHostMessage(), we will update the status when Ctrl or Space is pressed
		//Core::WriteStatus();
	#endif
}
// Destructor
CFrame::~CFrame()
{
	cdio_free_device_list(drives);
	/* The statbar sample has this so I add this to, but I guess timer will be deleted after
	   this anyway */
	#if wxUSE_TIMER
		if (m_timer.IsRunning()) m_timer.Stop();
	#endif

	ClosePages();
}

void CFrame::OnQuit(wxCommandEvent& WXUNUSED (event))
{
	Close(true);
}

void CFrame::OnClose(wxCloseEvent& event)
{
	// Don't forget the skip or the window won't be destroyed
	event.Skip();
	// Save GUI settings
	if (UseDebugger) g_pCodeWindow->Save();
	if (UseDebugger) Save();
	// Uninit
	m_Mgr->UnInit();

	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		Core::Stop();
		UpdateGUI();
	}
}

wxPanel* CFrame::CreateEmptyPanel()
{	
   wxPanel* Panel = new wxPanel(this, wxID_ANY);
   return Panel;
}
wxAuiNotebook* CFrame::CreateEmptyNotebook()
{	
   wxAuiNotebook* NB = new wxAuiNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, DefaultNBStyle);
   return NB;
}


void CFrame::DoFullscreen(bool _F)
{
	ShowFullScreen(_F);
	if (_F)
	{
		// Save the current mode before going to fullscreen
		AuiCurrent = m_Mgr->SavePerspective();
		m_Mgr->LoadPerspective(AuiFullscreen, true);
	}
	else
	{
		// Restore saved perspective
		m_Mgr->LoadPerspective(AuiCurrent, true);
	}
}
int CFrame::Limit(int i, int Low, int High)
{	
	if (i < Low) return Low;
	if (i > High) return High;
	return i;
}
void CFrame::SetSimplePaneSize()
{
	wxArrayInt Pane, Size;
	Pane.Add(0); Size.Add(50);
	Pane.Add(1); Size.Add(50);

	int iClientSize = this->GetSize().GetX();
	// Fix the pane sizes
	for (int i = 0; i < Pane.size(); i++)
	{
		// Check limits
		Size[i] = Limit(Size[i], 5, 95);
		// Produce pixel width from percentage width
		Size[i] = PercentageToPixels(Size[i], iClientSize);
		// Update size
		m_Mgr->GetPane(wxString::Format(wxT("Pane %i"), Pane[i])).BestSize(Size[i], -1).MinSize(Size[i], -1).MaxSize(Size[i], -1);
	}
	m_Mgr->Update();
	for (int i = 0; i < Pane.size(); i++)
	{
		// Remove the size limits
		m_Mgr->GetPane(wxString::Format(wxT("Pane %i"), Pane[i])).MinSize(-1, -1).MaxSize(-1, -1);
	}
}
void CFrame::SetPaneSize()
{
	if (Perspectives.size() <= ActivePerspective) return;
	int iClientX = this->GetSize().GetX(), iClientY = this->GetSize().GetY();

	for (int i = 0, j = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes().Item(i).window->IsKindOf(CLASSINFO(wxAuiToolBar)))
		{
			if (!m_Mgr->GetAllPanes().Item(i).IsOk()) return;
			if (Perspectives.at(ActivePerspective).Width.size() <= j || Perspectives.at(ActivePerspective).Height.size() <= j) continue;
			int W = Perspectives.at(ActivePerspective).Width.at(j), H = Perspectives.at(ActivePerspective).Height.at(j);
			// Check limits
			W = Limit(W, 5, 95); H = Limit(H, 5, 95);
			// Produce pixel width from percentage width
			W = PercentageToPixels(W, iClientX); H = PercentageToPixels(H, iClientY);
			m_Mgr->GetAllPanes().Item(i).BestSize(W,H).MinSize(W,H).MaxSize(W,H);

			j++;
		}
	}
	m_Mgr->Update();

	for (int i = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes().Item(i).window->IsKindOf(CLASSINFO(wxAuiToolBar)))
		{
			m_Mgr->GetAllPanes().Item(i).MinSize(-1,-1).MaxSize(-1,-1);
		}
	}
}
// Debugging, show loose windows
void CFrame::ListChildren()
{	
	ConsoleListener* Console = LogManager::GetInstance()->getConsoleListener();
	wxAuiNotebook * NB = NULL;

	Console->Log(LogTypes::LCUSTOM, "--------------------------------------------------------------------\n");

	for (int i = 0; i < this->GetChildren().size(); i++)
	{
		wxWindow * Win = this->GetChildren().Item(i)->GetData();
		Console->Log(LogTypes::LCUSTOM, StringFromFormat(
			"%i: %s (%s) :: %s", i,
			Win->GetName().mb_str(), Win->GetLabel().mb_str(), Win->GetParent()->GetName().mb_str()).c_str());
		//if (Win->GetName().IsSameAs(wxT("control")))
		if (Win->IsKindOf(CLASSINFO(wxAuiNotebook)))
		{
			NB = (wxAuiNotebook*)Win;
			Console->Log(LogTypes::LCUSTOM, StringFromFormat(" :: NB", NB->GetName().mb_str()).c_str());
		}
		else
		{
			NB = NULL;
		}
		Console->Log(LogTypes::LCUSTOM, StringFromFormat("\n").c_str());

		Win = this->GetChildren().Item(i)->GetData();
		for (int j = 0; j < Win->GetChildren().size(); j++)
		{
			Console->Log(LogTypes::LCUSTOM, StringFromFormat(
				"     %i.%i: %s (%s) :: %s", i, j,
				Win->GetName().mb_str(), Win->GetLabel().mb_str(), Win->GetParent()->GetName().mb_str()).c_str());
			if (NB)
			{
				if (j < NB->GetPageCount())
					Console->Log(LogTypes::LCUSTOM, StringFromFormat(" :: %s", NB->GetPage(j)->GetName().mb_str()).c_str());
			}
			Console->Log(LogTypes::LCUSTOM, StringFromFormat("\n").c_str());

			/*
			Win = this->GetChildren().Item(j)->GetData();
			for (int k = 0; k < Win->GetChildren().size(); k++)
			{
				Console->Log(LogTypes::LCUSTOM, StringFromFormat(
					"          %i.%i.%i: %s (%s) :: %s\n", i, j, k,
					Win->GetName().mb_str(), Win->GetLabel().mb_str(), Win->GetParent()->GetName().mb_str()).c_str());
			}
			*/
		}
	}	

	Console->Log(LogTypes::LCUSTOM, "--------------------------------------------------------------------\n");

	for (int i = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes().Item(i).window->IsKindOf(CLASSINFO(wxAuiNotebook))) continue;
		wxAuiNotebook * NB = (wxAuiNotebook*)m_Mgr->GetAllPanes().Item(i).window;
		Console->Log(LogTypes::LCUSTOM, StringFromFormat("%i: %s\n", i, m_Mgr->GetAllPanes().Item(i).name.mb_str()).c_str());

		for (int j = 0; j < NB->GetPageCount(); j++)
		{
			Console->Log(LogTypes::LCUSTOM, StringFromFormat("%i.%i: %s\n", i, j, NB->GetPageText(j).mb_str()).c_str());
		}
	}

	Console->Log(LogTypes::LCUSTOM, "--------------------------------------------------------------------\n");
}
void CFrame::ReloadPanes()
{	
	// Keep settings
	bool bConsole = SConfig::GetInstance().m_InterfaceConsole;

	//ListChildren();
	//ConsoleListener* Console = LogManager::GetInstance()->getConsoleListener();
	//Console->Log(LogTypes::LNOTICE, StringFromFormat("ReloadPanes begin: Sound %i\n", FindWindowByName(wxT("Sound"))).c_str());

	if (ActivePerspective >= Perspectives.size()) ActivePerspective = 0;

	// Check that there is a perspective
	if (Perspectives.size() > 0)
	{
		// Check that the perspective was saved once before
		if (Perspectives.at(ActivePerspective).Width.size() == 0) return;

		// Hide to avoid flickering
		HideAllNotebooks(true);
		// Close all pages
		ClosePages();

		CloseAllNotebooks();
		//m_Mgr->Update();

		// Create new panes with notebooks
		for (int i = 0; i < Perspectives.at(ActivePerspective).Width.size() - 1; i++)
		{
			m_Mgr->AddPane(CreateEmptyNotebook(), wxAuiPaneInfo().Hide());
		}
		HideAllNotebooks(true);

		// Names
		NamePanes();
		// Perspectives
		m_Mgr->LoadPerspective(Perspectives.at(ActivePerspective).Perspective, false);
	}
	// Create one pane by default
	else
	{
		m_Mgr->AddPane(CreateEmptyNotebook());
	}

	// Restore settings
	SConfig::GetInstance().m_InterfaceConsole = bConsole;
	// Load GUI settings
	g_pCodeWindow->Load();
	// Open notebook pages
	AddRemoveBlankPage();
	g_pCodeWindow->OpenPages();
	if (m_bLogWindow) DoToggleWindow(IDM_LOGWINDOW, true);
	if (SConfig::GetInstance().m_InterfaceConsole) DoToggleWindow(IDM_CONSOLEWINDOW, true);	

	//Console->Log(LogTypes::LNOTICE, StringFromFormat("ReloadPanes end: Sound %i\n", FindWindowByName(wxT("Sound"))).c_str());
	//ListChildren();
}
void CFrame::DoLoadPerspective()
{
	ReloadPanes();
	// Restore the exact window sizes, which LoadPerspective doesn't always do
	SetPaneSize();
	// Show
	ShowAllNotebooks(true);

	/*	*/
	ConsoleListener* Console = LogManager::GetInstance()->getConsoleListener();
	Console->Log(LogTypes::LCUSTOM, StringFromFormat(
		"Loaded: %s (%i panes, %i NBs)\n",
		Perspectives.at(ActivePerspective).Name.c_str(), m_Mgr->GetAllPanes().GetCount(), GetNotebookCount()).c_str());

}
// Update the local perspectives array
void CFrame::SaveLocal()
{
	Perspectives.clear();
	std::vector<std::string> VPerspectives;
	std::string _Perspectives;

	IniFile ini;
	ini.Load(DEBUGGER_CONFIG_FILE);
	ini.Get("Perspectives", "Perspectives", &_Perspectives, "");
	ini.Get("Perspectives", "Active", &ActivePerspective, 5);
	SplitString(_Perspectives, ",", VPerspectives);

	//
	for (int i = 0; i < VPerspectives.size(); i++)
	{
		SPerspectives Tmp;		
		std::string _Section, _Perspective, _Width, _Height;
		std::vector<std::string> _SWidth, _SHeight;
		Tmp.Name = VPerspectives.at(i);
		_Section = StringFromFormat("P - %s", Tmp.Name.c_str());
		if (!ini.Exists(_Section.c_str(), "Width")) continue;
		
		ini.Get(_Section.c_str(), "Perspective", &_Perspective, "");
		ini.Get(_Section.c_str(), "Width", &_Width, "");
		ini.Get(_Section.c_str(), "Height", &_Height, "");	

		Tmp.Perspective = wxString::FromAscii(_Perspective.c_str());

		SplitString(_Width, ",", _SWidth);
		SplitString(_Height, ",", _SHeight);
		for (int i = 0; i < _SWidth.size(); i++)
		{
			int _Tmp;
			if (TryParseInt(_SWidth.at(0).c_str(), &_Tmp)) Tmp.Width.push_back(_Tmp);				
		}
		for (int i = 0; i < _SHeight.size(); i++)
		{
			int _Tmp;
			if (TryParseInt(_SHeight.at(0).c_str(), &_Tmp)) Tmp.Height.push_back(_Tmp);
		}
		Perspectives.push_back(Tmp);
	}
}
void CFrame::Save()
{
	if (Perspectives.size() == 0)
	{
		wxMessageBox(wxT("Please create a perspective before saving"), wxT("Notice"), wxOK, this);
		return;
	}
	if (ActivePerspective >= Perspectives.size()) ActivePerspective = 0;

	// Turn off edit before saving
	TogglePaneStyle(false);
	// Name panes
	NamePanes();

	// Get client size
	int iClientX = this->GetSize().GetX(), iClientY = this->GetSize().GetY();

	IniFile ini;
	ini.Load(DEBUGGER_CONFIG_FILE);

	std::string _Section = StringFromFormat("P - %s", Perspectives.at(ActivePerspective).Name.c_str());
	ini.Set(_Section.c_str(), "Perspective", m_Mgr->SavePerspective().mb_str());
	
	std::string SWidth = "", SHeight = "";
	for (int i = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes().Item(i).window->IsKindOf(CLASSINFO(wxAuiToolBar)))
		{
			SWidth += StringFromFormat("%i", PixelsToPercentage(m_Mgr->GetAllPanes().Item(i).window->GetClientSize().GetX(), iClientX));
			SHeight += StringFromFormat("%i", PixelsToPercentage(m_Mgr->GetAllPanes().Item(i).window->GetClientSize().GetY(), iClientY));
			SWidth += ","; SHeight += ",";
		}
	}
	// Remove the ending ","
	SWidth = SWidth.substr(0, SWidth.length()-1); SHeight = SHeight.substr(0, SHeight.length()-1);

	ini.Set(_Section.c_str(), "Width", SWidth.c_str());
	ini.Set(_Section.c_str(), "Height", SHeight.c_str());

	// Save perspective names
	std::string STmp = "";
	for (int i = 0; i < Perspectives.size(); i++)
	{
		STmp += Perspectives.at(i).Name + ",";
	}
	STmp = STmp.substr(0, STmp.length()-1);
	ini.Set("Perspectives", "Perspectives", STmp.c_str());
	ini.Set("Perspectives", "Active", ActivePerspective);
	ini.Save(DEBUGGER_CONFIG_FILE);

	// Update the local vector
	SaveLocal();

	/**/
	ConsoleListener* Console = LogManager::GetInstance()->getConsoleListener();
	Console->Log(LogTypes::LCUSTOM, StringFromFormat(
		"Saved: %s (%i panes, %i NBs)\n",
		Perspectives.at(ActivePerspective).Name.c_str(), m_Mgr->GetAllPanes().GetCount(), GetNotebookCount()).c_str());
	
	
	TogglePaneStyle(m_ToolBarAui->GetToolToggled(IDM_EDIT_PERSPECTIVES)); 
}

int CFrame::PercentageToPixels(int Percentage, int Total)
{
	int Pixels = (int)((float)Total * ((float)Percentage / 100.0));
	return Pixels;
}
int CFrame::PixelsToPercentage(int Pixels, int Total)
{
	int Percentage = (int)(((float)Pixels / (float)Total) * 100.0);
	return Percentage;
}

void CFrame::NamePanes()
{
	for (int i = 0, j = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes().Item(i).window->IsKindOf(CLASSINFO(wxAuiToolBar)))
		{
			m_Mgr->GetAllPanes().Item(i).Name(wxString::Format(wxT("Pane %i"), j));
			m_Mgr->GetAllPanes().Item(i).Caption(wxString::Format(wxT("Pane %i"), j));
			j++;
		}
	}
}
void CFrame::AddPane()
{
	m_Mgr->AddPane(CreateEmptyNotebook());
	NamePanes();
	AddRemoveBlankPage();
	m_Mgr->Update();
}	

void CFrame::OnPaneClose(wxAuiManagerEvent& event)
{
	event.Veto();

	wxAuiNotebook * nb = (wxAuiNotebook*)event.pane->window;
	if (!nb) return;
	if (! (nb->GetPageCount() == 0 || (nb->GetPageCount() == 1 && nb->GetPageText(0).IsSameAs(wxT("<>")))))
	{
		wxMessageBox(wxT("You can't close panes that have pages in them."), wxT("Notice"), wxOK, this);		
	}
	else
	{
		/*
		ConsoleListener* Console = LogManager::GetInstance()->getConsoleListener();
		Console->Log(LogTypes::LCUSTOM, StringFromFormat("GetNotebookCount before: %i\n", GetNotebookCount()).c_str());
		*/

		// Detach and delete the empty notebook
		event.pane->DestroyOnClose(true);
		m_Mgr->ClosePane(*event.pane);

		//Console->Log(LogTypes::LCUSTOM, StringFromFormat("GetNotebookCount after: %i\n", GetNotebookCount()).c_str());
	}

	m_Mgr->Update();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Host messages
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#ifdef _WIN32
WXLRESULT CFrame::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
	switch (nMsg)
	{
	case WM_SYSCOMMAND:
		switch (wParam & 0xFFF0)
		{
		case SC_SCREENSAVE:
		case SC_MONITORPOWER:
			return 0;
		}
	default:
		// Let wxWidgets process it as normal
		return wxFrame::MSWWindowProc(nMsg, wParam, lParam);
	}
}
#endif

void CFrame::OnHostMessage(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case IDM_UPDATEGUI:
		UpdateGUI();
		break;

	case IDM_UPDATESTATUSBAR:
		if (m_pStatusBar != NULL)
		{
			m_pStatusBar->SetStatusText(event.GetString(), event.GetInt());
		}
		break;
	}
}

// Post events
// Warning: This may cause an endless loop if the event is propagated back to its parent
void CFrame::PostEvent(wxCommandEvent& event)
{
	event.Skip();
	event.StopPropagation();

	if (g_pCodeWindow
		&& event.GetId() >= IDM_INTERPRETER && event.GetId() <= IDM_ADDRBOX
		&& event.GetId() != IDM_JITUNLIMITED
		)
		wxPostEvent(g_pCodeWindow, event);
}
void CFrame::PostMenuEvent(wxMenuEvent& event)
{
	if (g_pCodeWindow) wxPostEvent(g_pCodeWindow, event);
}
void CFrame::PostUpdateUIEvent(wxUpdateUIEvent& event)
{
	if (g_pCodeWindow) wxPostEvent(g_pCodeWindow, event);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Input
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void CFrame::OnGameListCtrl_ItemActivated(wxListEvent& WXUNUSED (event))
{
	// Show all platforms and regions if...
	// 1. All platforms are set to hide
	// 2. All Regions are set to hide
	// Otherwise call BootGame to either...
	// 1. Boot the selected iso
	// 2. Boot the default or last loaded iso.
	// 3. Call BrowseForDirectory if the gamelist is empty
	if (!m_GameListCtrl->GetGameNames().size() &&
		!((SConfig::GetInstance().m_ListGC ||
		SConfig::GetInstance().m_ListWii ||
		SConfig::GetInstance().m_ListWad) &&
		(SConfig::GetInstance().m_ListJap ||
		SConfig::GetInstance().m_ListUsa  ||
		SConfig::GetInstance().m_ListPal)))
	{
		SConfig::GetInstance().m_ListGC  =	SConfig::GetInstance().m_ListWii =
		SConfig::GetInstance().m_ListWad =	SConfig::GetInstance().m_ListJap =
		SConfig::GetInstance().m_ListUsa =	SConfig::GetInstance().m_ListPal = true;

		GetMenuBar()->FindItem(IDM_LISTGC)->Check(true);
		GetMenuBar()->FindItem(IDM_LISTWII)->Check(true);
		GetMenuBar()->FindItem(IDM_LISTWAD)->Check(true);
		GetMenuBar()->FindItem(IDM_LISTJAP)->Check(true);
		GetMenuBar()->FindItem(IDM_LISTUSA)->Check(true);
		GetMenuBar()->FindItem(IDM_LISTPAL)->Check(true);

		m_GameListCtrl->Update();
	}			
	else BootGame();
}

void CFrame::OnKeyDown(wxKeyEvent& event)
{
	// Don't block other events
	event.Skip();

	// Escape key turn off fullscreen then Stop emulation in windowed mode
	if (event.GetKeyCode() == WXK_ESCAPE)
	{
		// Temporary solution to double esc keydown. When the OpenGL plugin is running all esc keydowns are duplicated
		// I'm guessing it's coming from the OpenGL plugin but I couldn't find the source of it so I added this until
		// the source of the problem surfaces.
		static double Time = 0;
		if (Common::Timer::GetDoubleTime()-1 < Time) return;
		Time = Common::Timer::GetDoubleTime();

		DoFullscreen(!IsFullScreen());
		if (IsFullScreen())
		{
			#ifdef _WIN32
			MSWSetCursor(true);
			#endif
		}	
		//UpdateGUI();
	}
	if (event.GetKeyCode() == WXK_RETURN && event.GetModifiers() == wxMOD_ALT)
	{
		// For some reasons, wxWidget doesn't proccess the Alt+Enter event there on windows.
		// But still, pressing Alt+Enter make it Fullscreen, So this is for other OS... :P
		DoFullscreen(!IsFullScreen());
	}
#ifdef _WIN32
	if(event.GetKeyCode() == 'M', '3', '4', '5', '6') // Send this to the video plugin WndProc
	{
		PostMessage((HWND)Core::GetWindowHandle(), WM_KEYDOWN, event.GetKeyCode(), 0);
	}
#endif

#ifdef RERECORDING
	// Turn on or off frame advance
	if (event.GetKeyCode() == WXK_CONTROL) Core::FrameStepOnOff();

	// Step forward
	if (event.GetKeyCode() == WXK_SPACE) Core::FrameAdvance();
#endif

	// Send the keyboard status to the Input plugin
	if(Core::GetState() != Core::CORE_UNINITIALIZED)
	    CPluginManager::GetInstance().GetPad(0)->PAD_Input(event.GetKeyCode(), 1); // 1 = Down
}

void CFrame::OnKeyUp(wxKeyEvent& event)
{
	if(Core::GetState() != Core::CORE_UNINITIALIZED)
		CPluginManager::GetInstance().GetPad(0)->PAD_Input(event.GetKeyCode(), 0); // 0 = Up
	event.Skip();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Detect double click. Kind of, for some reason we have to manually create the double click for now.
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void CFrame::OnDoubleClick(wxMouseEvent& event)
{
	 // Don't block the mouse click
	event.Skip();

	// Don't use this in Wii mode since we use the mouse as input to the game there
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii) return;

	// Only detect double clicks in the rendering window, and only use this when a game is running
	if(Core::GetState() == Core::CORE_UNINITIALIZED || event.GetId() != IDM_MPANEL) return;

	// For first click just save the time
	if(m_fLastClickTime == 0) { m_fLastClickTime = Common::Timer::GetDoubleTime(); return; }

	// -------------------------------------------
	/* Manually detect double clicks since both wxEVT_LEFT_DCLICK and WM_LBUTTONDBLCLK stops
	   working after the child window is created by the plugin */
	// ----------------------
	double TmpTime = Common::Timer::GetDoubleTime();
	int Elapsed = (TmpTime - m_fLastClickTime) * 1000;

	// Get the double click time, if avaliable
	int DoubleClickTime;
	#ifdef _WIN32
		DoubleClickTime = GetDoubleClickTime();
	#else
		DoubleClickTime = 500; // The default in Windows
	#endif

	m_fLastClickTime = TmpTime; // Save the click time

	if (Elapsed < DoubleClickTime)
	{
		DoFullscreen(!IsFullScreen());
		#ifdef _WIN32
			MSWSetCursor(true); // Show the cursor again, in case it was hidden
		#endif
		m_fLastClickTime -= 10; // Don't treat repeated clicks as double clicks
	}

	UpdateGUI();
}


// Check for mouse motion. Here we process the bHideCursor setting.
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void CFrame::OnMotion(wxMouseEvent& event)
{
	event.Skip();

	// The following is only interesting when a game is running
	if(Core::GetState() == Core::CORE_UNINITIALIZED) return;

	/* For some reason WM_MOUSEMOVE events are sent from the plugin even when there is no movement
	   so we have to check that the cursor position has actually changed */
	if(bRenderToMain) //
	{
		bool PositionIdentical = false;
		if (event.GetX() == LastMouseX && event.GetY() == LastMouseY) PositionIdentical = true;
		LastMouseX = event.GetX(); LastMouseY = event.GetY();
		if(PositionIdentical) return;
	}

	// Now we know that we have an actual mouse movement event

	// Update motion for the auto hide option and return
	if(IsFullScreen() && SConfig::GetInstance().m_LocalCoreStartupParameter.bAutoHideCursor)
	{
		m_iLastMotionTime = Common::Timer::GetDoubleTime();
		#ifdef _WIN32
				MSWSetCursor(true);
		#endif
		return;
	}

	if(SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor && event.GetId() == IDM_MPANEL)
	{
		#ifdef _WIN32
			if(bRenderToMain) MSWSetCursor(false);

			/* We only need to use this if we are rendering to a separate window. It does work
			   for rendering to the main window to, but in that case our MSWSetCursor() works to
			   so we can use that instead. If we one day determine that the separate window
			   rendering is superfluous we could do without this */
			else PostMessage((HWND)Core::GetWindowHandle(), WM_USER, 10, 0);
		#endif
	}

	// For some reason we need this to, otherwise the cursor can get stuck with the resizing arrows
	else
	{
		#ifdef _WIN32
			if(bRenderToMain) MSWSetCursor(true);
			else PostMessage((HWND)Core::GetWindowHandle(), WM_USER, 10, 1);
		#endif
	}

}

// Check for mouse status a couple of times per second for the auto hide option
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#if wxUSE_TIMER
void CFrame::Update()
{
	// Check if auto hide is on, or if we are already hiding the cursor all the time
	if(!SConfig::GetInstance().m_LocalCoreStartupParameter.bAutoHideCursor
		|| SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor) return;

	if(IsFullScreen())
	{
		int HideDelay = 1; // Wait 1 second to hide the cursor, just like Windows Media Player
		double TmpSeconds = Common::Timer::GetDoubleTime(); // Get timestamp
		double CompareTime = TmpSeconds - HideDelay; // Compare it

		if(m_iLastMotionTime < CompareTime) // Update cursor
		#ifdef _WIN32
			MSWSetCursor(false);
		#else
			{}
		#endif
	}
}
#endif
/////////////////////////////////////////////////////////////////////////////////////////////////////////
