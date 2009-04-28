// Copyright (C) 2003-2009 Dolphin Project.

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

#include "Globals.h" // Local
#include "Frame.h"
#include "ConfigMain.h"
#include "PluginManager.h"
#include "MemcardManager.h"
#include "CheatsWindow.h"
#include "AboutDolphin.h"
#include "GameListCtrl.h"
#include "BootManager.h"

#include "Common.h" // Common
#include "FileUtil.h"
#include "Timer.h"
#include "Setup.h"

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



//////////////////////////////////////////////////////////////////////////////////////////
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
/////////////////////////////////////////////////




//////////////////////////////////////////////////////////////////////////////////////////
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
				if (lParam == NULL) main_frame->bRenderToMain = false;
					else main_frame->bRenderToMain = true;
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

			// -----------------------------------------
			#ifdef SETUP_TIMER_WAITING
			// -----------------
				case OPENGL_VIDEO_STOP:
					// The Video thread has been shut down
					Core::VideoThreadEnd();
					//INFO_LOG(CONSOLE, "OPENGL_VIDEO_STOP\n");
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
/////////////////////////////////////////////////



// ----------------------------------------------------------------------------
// event tables
// ----------------------------------------------------------------------------

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
EVT_MENU(IDM_STOP, CFrame::OnStop)
EVT_MENU(IDM_SCREENSHOT, CFrame::OnScreenshot)
EVT_MENU(IDM_CONFIG_MAIN, CFrame::OnConfigMain)
EVT_MENU(IDM_CONFIG_GFX_PLUGIN, CFrame::OnPluginGFX)
EVT_MENU(IDM_CONFIG_DSP_PLUGIN, CFrame::OnPluginDSP)
EVT_MENU(IDM_CONFIG_PAD_PLUGIN, CFrame::OnPluginPAD)
EVT_MENU(IDM_CONFIG_WIIMOTE_PLUGIN, CFrame::OnPluginWiimote)

#ifdef MUSICMOD
EVT_MENU(IDM_MUTE, CFrame::MM_OnMute)
EVT_MENU(IDM_MUSIC_PLAY, CFrame::MM_OnPause)
EVT_COMMAND_SCROLL(IDS_VOLUME, CFrame::MM_OnVolume)
EVT_MENU(IDT_LOG, CFrame::MM_OnLog)
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
EVT_MENU(IDM_TOGGLE_LOGWINDOW, CFrame::OnToggleLogWindow)
EVT_MENU(IDM_TOGGLE_CONSOLE, CFrame::OnToggleConsole)

EVT_MENU(IDM_LISTDRIVES, CFrame::GameListChanged)
EVT_MENU(IDM_LISTWII, CFrame::GameListChanged)
EVT_MENU(IDM_LISTGC,  CFrame::GameListChanged)
EVT_MENU(IDM_LISTJAP, CFrame::GameListChanged)
EVT_MENU(IDM_LISTPAL, CFrame::GameListChanged)
EVT_MENU(IDM_LISTUSA, CFrame::GameListChanged)

EVT_MENU_RANGE(IDM_LOADSLOT1, IDM_LOADSLOT10, CFrame::OnLoadState)
EVT_MENU_RANGE(IDM_SAVESLOT1, IDM_SAVESLOT10, CFrame::OnSaveState)
EVT_MENU_RANGE(IDM_DRIVE1, IDM_DRIVE24, CFrame::OnBootDrive)

EVT_SIZE(CFrame::OnResize)
EVT_LIST_ITEM_ACTIVATED(LIST_CTRL, CFrame::OnGameListCtrl_ItemActivated)
EVT_HOST_COMMAND(wxID_ANY, CFrame::OnHostMessage)
#if wxUSE_TIMER
	EVT_TIMER(wxID_ANY, CFrame::OnTimer)
#endif
END_EVENT_TABLE()
/////////////////////////////////////////////////


// ----------------------------------------------------------------------------
// Creation and close, quit functions
// ----------------------------------------------------------------------------

CFrame::CFrame(bool showLogWindow,
		wxFrame* parent,
		wxWindowID id,
		const wxString& title,
		const wxPoint& pos,
		const wxSize& size,
		long style)
	: wxFrame(parent, id, title, pos, size, style)
	, m_bLogWindow(showLogWindow || SConfig::GetInstance().m_InterfaceLogWindow)
	, m_pStatusBar(NULL), bRenderToMain(true)
	, HaveLeds(false), HaveSpeakers(false)
    , m_Panel(NULL)
	, m_fLastClickTime(0), m_iLastMotionTime(0), LastMouseX(0), LastMouseY(0)
	#if wxUSE_TIMER
		, m_timer(this)
	#endif
          
{
	// Create timer
	#if wxUSE_TIMER
		int TimesPerSecond = 10; // We don't need more than this
		m_timer.Start( floor((double)(1000 / TimesPerSecond)) );
	#endif

	// Create toolbar bitmaps
	InitBitmaps();

	// Give it an icon
	wxIcon IconTemp;
	IconTemp.CopyFromBitmap(wxGetBitmapFromMemory(dolphin_png));
	SetIcon(IconTemp);

	// Give it a status bar
	m_pStatusBar = CreateStatusBar(1, wxST_SIZEGRIP, ID_STATUSBAR);
	if (!SConfig::GetInstance().m_InterfaceStatusbar)
		m_pStatusBar->Hide();

	// Give it a menu bar
	CreateMenu();

	// Give it a console
	ConsoleListener *console = LogManager::GetInstance()->getConsoleListener();
	if (SConfig::GetInstance().m_InterfaceConsole)
		console->Open();

	// This panel is the parent for rendering and it holds the gamelistctrl
	//m_Panel = new wxPanel(this, IDM_MPANEL);
	m_Panel = new CPanel(this, IDM_MPANEL);

	m_LogWindow = new CLogWindow(this);
	if (m_bLogWindow)
		m_LogWindow->Show();

	m_GameListCtrl = new CGameListCtrl(m_Panel, LIST_CTRL,
			wxDefaultPosition, wxDefaultSize,
			wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT);

	sizerPanel = new wxBoxSizer(wxHORIZONTAL);
	sizerPanel->Add(m_GameListCtrl, 1, wxEXPAND | wxALL);
	m_Panel->SetSizer(sizerPanel);

	sizerFrame = new wxBoxSizer(wxHORIZONTAL);
	sizerFrame->Add(m_Panel, 1, wxEXPAND | wxALL);
	this->SetSizer(sizerFrame);

	// Create the toolbar
	RecreateToolbar();
	if (!SConfig::GetInstance().m_InterfaceToolbar)
		TheToolBar->Hide();

	FitInside();

	Show(); // Show the window

	// Create list of available plugins for the configuration window
	CPluginManager::GetInstance().ScanForPlugins();

	//if we are ever going back to optional iso caching:
	//m_GameListCtrl->Update(SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableIsoCache);
	m_GameListCtrl->Update();
	//sizerPanel->SetSizeHints(m_Panel);

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

	#ifdef _WIN32 // The functions are only tested in Windows so far
		wxTheApp->Connect(wxID_ANY, wxEVT_LEFT_DOWN, // Mouse
			wxMouseEventHandler(CFrame::OnDoubleClick),
			(wxObject*)0, this);
		wxTheApp->Connect(wxID_ANY, wxEVT_MOTION,
			wxMouseEventHandler(CFrame::OnMotion),
			(wxObject*)0, this);
	#endif
	// ----------

	UpdateGUI();

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
}

void CFrame::OnQuit(wxCommandEvent& WXUNUSED (event))
{
	Close(true);
}

void CFrame::OnClose(wxCloseEvent& event)
{

	// Don't forget the skip or the window won't be destroyed
	event.Skip();

	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		Core::Stop();
		UpdateGUI();
	}
}
/////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Input and host messages
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#ifdef _WIN32
	WXLRESULT CFrame::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
	{
		switch (nMsg)
		{
		case WM_SYSCOMMAND:
			switch (wParam)
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


void CFrame::OnGameListCtrl_ItemActivated(wxListEvent& WXUNUSED (event))
{
	BootGame();
}

void CFrame::OnKeyDown(wxKeyEvent& event)
{
	// Toggle fullscreen from Alt + Enter or Esc
	if (((event.GetKeyCode() == WXK_RETURN) && (event.GetModifiers() == wxMOD_ALT)) ||
	    (event.GetKeyCode() == WXK_ESCAPE))
	{
		ShowFullScreen(!IsFullScreen());
		UpdateGUI();
	}
#ifdef _WIN32
	if(event.GetKeyCode() == 'E','M') // Send this to the video plugin WndProc
	{
		PostMessage((HWND)Core::GetWindowHandle(), WM_KEYDOWN, event.GetKeyCode(), 0);
		event.Skip(); // Don't block the E key
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

	// Don't block other events
	event.Skip();
}

void CFrame::OnKeyUp(wxKeyEvent& event)
{
	if(Core::GetState() != Core::CORE_UNINITIALIZED)
		CPluginManager::GetInstance().GetPad(0)->PAD_Input(event.GetKeyCode(), 0); // 0 = Up
	event.Skip();
}


// Returns a timestamp with decimals for precise time comparisons
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯?
double GetDoubleTime()
{
	wxDateTime datetime = wxDateTime::UNow(); // Get timestamp
	u64 TmpSeconds = Common::Timer::GetTimeSinceJan1970(); // Get continous timestamp

	/* Remove a few years. We only really want enough seconds to make sure that we are
	   detecting actual actions, perhaps 60 seconds is enough really, but I leave a
	   year of seconds anyway, in case the user's clock is incorrect or something like that */
	TmpSeconds = TmpSeconds - (38 * 365 * 24 * 60 * 60);

	//if (TmpSeconds < 0) return 0; // Check the the user's clock is working somewhat

	u32 Seconds = (u32)TmpSeconds; // Make a smaller integer that fits in the double
	double ms = datetime.GetMillisecond() / 1000.0;
	double TmpTime = Seconds + ms;
	return TmpTime;
}


// Detect double click. Kind of, for some reason we have to manually create the double click for now.
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯?
void CFrame::OnDoubleClick(wxMouseEvent& event)
{
	 // Don't block the mouse click
	event.Skip();

	// Don't use this in Wii mode since we use the mouse as input to the game there
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii) return;

	// Only detect double clicks in the rendering window, and only use this when a game is running
	if(Core::GetState() == Core::CORE_UNINITIALIZED || event.GetId() != IDM_MPANEL) return;

	// For first click just save the time
	if(m_fLastClickTime == 0) { m_fLastClickTime = GetDoubleTime(); return; }

	// -------------------------------------------
	/* Manually detect double clicks since both wxEVT_LEFT_DCLICK and WM_LBUTTONDBLCLK stops
	   working after the child window is created by the plugin */
	// ----------------------
	double TmpTime = GetDoubleTime();
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
		ShowFullScreen(!IsFullScreen());
		#ifdef _WIN32
			MSWSetCursor(true); // Show the cursor again, in case it was hidden
		#endif
		m_fLastClickTime -= 10; // Don't treat repeated clicks as double clicks
	}			
	// --------------------------
}


// Check for mouse motion. Here we process the bHideCursor setting.
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯?
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
		m_iLastMotionTime = GetDoubleTime();
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
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯?
#if wxUSE_TIMER
void CFrame::Update()
{
	// Check if auto hide is on, or if we are already hiding the cursor all the time
	if(!SConfig::GetInstance().m_LocalCoreStartupParameter.bAutoHideCursor
		|| SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor) return;

	if(IsFullScreen())
	{
		int HideDelay = 1; // Wait 1 second to hide the cursor, just like Windows Media Player
		double TmpSeconds = GetDoubleTime(); // Get timestamp
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

