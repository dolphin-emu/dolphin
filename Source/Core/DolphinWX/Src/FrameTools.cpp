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


/*
1.1 Windows

CFrame is the main parent window. Inside CFrame there is m_Panel which is the
parent for the rendering window (when we render to the main window). In Windows
the rendering window is created by giving CreateWindow() m_Panel->GetHandle()
as parent window and creating a new child window to m_Panel. The new child
window handle that is returned by CreateWindow() can be accessed from
Core::GetWindowHandle().
*/


// FIXME: why doesn't it work on windows???
#ifndef _WIN32
#include "Common.h"
#endif

#include "Setup.h" // Common

#if defined(HAVE_SFML) && HAVE_SFML || defined(_WIN32)
#include "NetWindow.h"
#endif

#include "Common.h" // Common
#include "FileUtil.h"
#include "FileSearch.h"
#include "Timer.h"

#include "Globals.h" // Local
#include "Frame.h"
#include "ConfigMain.h"
#include "PluginManager.h"
#include "MemcardManager.h"
#include "CheatsWindow.h"
#include "InfoWindow.h"
#include "AboutDolphin.h"
#include "GameListCtrl.h"
#include "BootManager.h"
#include "LogWindow.h"
#include "WxUtils.h"

#include "ConfigManager.h" // Core
#include "Core.h"
#include "OnFrame.h"
#include "HW/DVDInterface.h"
#include "State.h"
#include "VolumeHandler.h"
#include "NANDContentLoader.h"

#include <wx/datetime.h> // wxWidgets


// Resources
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

// Other Windows
wxCheatsWindow* CheatsWindow;
wxInfoWindow* InfoWindow;

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Create menu items
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void CFrame::CreateMenu()
{
	if (GetMenuBar())
		GetMenuBar()->Destroy();

	wxMenuBar* menuBar = new wxMenuBar(wxMB_DOCKABLE);

	// file menu
	wxMenu* fileMenu = new wxMenu;
	fileMenu->Append(wxID_OPEN, _T("&Open...\tCtrl+O"));

	wxMenu *externalDrive = new wxMenu;
	m_pSubMenuDrive = fileMenu->AppendSubMenu(externalDrive, _T("&Boot from DVD Drive..."));
	
	drives = cdio_get_devices();
	for (int i = 0; drives[i] != NULL && i < 24; i++) {
		externalDrive->Append(IDM_DRIVE1 + i, wxString::FromAscii(drives[i]));
	}

	fileMenu->AppendSeparator();
	fileMenu->Append(wxID_REFRESH, _T("&Refresh List"));
	fileMenu->AppendSeparator();
	fileMenu->Append(IDM_BROWSE, _T("&Browse for ISOs..."));

	fileMenu->AppendSeparator();
	fileMenu->Append(wxID_EXIT, _T("E&xit\tAlt+F4"));
	menuBar->Append(fileMenu, _T("&File"));

	// Emulation menu
	wxMenu* emulationMenu = new wxMenu;
	emulationMenu->Append(IDM_PLAY, _T("&Play\tF10"));
	emulationMenu->Append(IDM_STOP, _T("&Stop"));
	emulationMenu->AppendSeparator();
	emulationMenu->Append(IDM_RECORD, _T("Start &Recording..."));
	emulationMenu->Append(IDM_PLAYRECORD, _T("P&lay Recording..."));
	emulationMenu->AppendSeparator();
	emulationMenu->Append(IDM_CHANGEDISC, _T("Change &Disc"));
	
	emulationMenu->Append(IDM_FRAMESTEP, _T("&Frame Stepping"), wxEmptyString, wxITEM_CHECK);

	wxMenu *skippingMenu = new wxMenu;
	m_pSubMenuFrameSkipping = emulationMenu->AppendSubMenu(skippingMenu, _T("Frame S&kipping"));
	for(int i = 0; i < 10; i++)
		skippingMenu->Append(IDM_FRAMESKIP0 + i, wxString::Format(_T("%i"), i), wxEmptyString, wxITEM_RADIO);

	emulationMenu->AppendSeparator();
	emulationMenu->Append(IDM_SCREENSHOT, _T("Take S&creenshot\tF9"));
	emulationMenu->AppendSeparator();
	wxMenu *saveMenu = new wxMenu;
	wxMenu *loadMenu = new wxMenu;
	m_pSubMenuLoad = emulationMenu->AppendSubMenu(loadMenu, _T("&Load State"));
	m_pSubMenuSave = emulationMenu->AppendSubMenu(saveMenu, _T("Sa&ve State"));

	saveMenu->Append(IDM_SAVESTATEFILE, _T("Save State..."));
	loadMenu->Append(IDM_UNDOSAVESTATE, _T("Last Overwritten State\tShift+F12"));
	saveMenu->AppendSeparator();

	loadMenu->Append(IDM_LOADSTATEFILE, _T("Load State..."));
	loadMenu->Append(IDM_LOADLASTSTATE, _T("Last Saved State\tF11"));
	loadMenu->Append(IDM_UNDOLOADSTATE, _T("Undo Load State\tF12"));
	loadMenu->AppendSeparator();

	for (int i = 1; i <= 8; i++) {
		loadMenu->Append(IDM_LOADSLOT1 + i - 1, wxString::Format(_T("Slot %i\tF%i"), i, i));
		saveMenu->Append(IDM_SAVESLOT1 + i - 1, wxString::Format(_T("Slot %i\tShift+F%i"), i, i));
	}
	menuBar->Append(emulationMenu, _T("&Emulation"));

	// Options menu
	wxMenu* pOptionsMenu = new wxMenu;
	pOptionsMenu->Append(IDM_CONFIG_MAIN, _T("Co&nfigure..."));
	pOptionsMenu->AppendSeparator();
	pOptionsMenu->Append(IDM_CONFIG_GFX_PLUGIN, _T("&Graphics Settings"));
	pOptionsMenu->Append(IDM_CONFIG_DSP_PLUGIN, _T("&DSP Settings"));
	pOptionsMenu->Append(IDM_CONFIG_PAD_PLUGIN, _T("&Pad Settings"));
	pOptionsMenu->Append(IDM_CONFIG_WIIMOTE_PLUGIN, _T("&Wiimote Settings"));
	#ifdef _WIN32
		pOptionsMenu->AppendSeparator();
		pOptionsMenu->Append(IDM_TOGGLE_FULLSCREEN, _T("&Fullscreen\tAlt+Enter"));
	#endif			
	menuBar->Append(pOptionsMenu, _T("&Options"));

	// Tools menu
	wxMenu* toolsMenu = new wxMenu;
	toolsMenu->Append(IDM_MEMCARD, _T("&Memcard Manager"));
	toolsMenu->Append(IDM_CHEATS, _T("Action &Replay Manager"));
	toolsMenu->Append(IDM_INFO, _T("System Information"));

#if defined(HAVE_SFML) && HAVE_SFML
	toolsMenu->Append(IDM_NETPLAY, _T("Start &NetPlay"));
#endif

	if (DiscIO::CNANDContentManager::Access().GetNANDLoader(FULL_WII_MENU_DIR).IsValid())
	{
		toolsMenu->Append(IDM_LOAD_WII_MENU, _T("Load Wii Menu"));
	}

	menuBar->Append(toolsMenu, _T("&Tools"));

	wxMenu* viewMenu = new wxMenu;
	viewMenu->AppendCheckItem(IDM_TOGGLE_TOOLBAR, _T("Show &Toolbar"));
	viewMenu->Check(IDM_TOGGLE_TOOLBAR, SConfig::GetInstance().m_InterfaceToolbar);
	viewMenu->AppendCheckItem(IDM_TOGGLE_STATUSBAR, _T("Show &Statusbar"));
	viewMenu->Check(IDM_TOGGLE_STATUSBAR, SConfig::GetInstance().m_InterfaceStatusbar);
	viewMenu->AppendCheckItem(IDM_LOGWINDOW, _T("Show &Logwindow"));
	viewMenu->Check(IDM_LOGWINDOW, m_bLogWindow);
	viewMenu->AppendCheckItem(IDM_CONSOLEWINDOW, _T("Show &Console"));
	viewMenu->Check(IDM_CONSOLEWINDOW, SConfig::GetInstance().m_InterfaceConsole);
	viewMenu->AppendSeparator();

	viewMenu->AppendCheckItem(IDM_LISTWII, _T("Show Wii"));
	viewMenu->Check(IDM_LISTWII, SConfig::GetInstance().m_ListWii);
	viewMenu->AppendCheckItem(IDM_LISTGC, _T("Show GameCube"));
	viewMenu->Check(IDM_LISTGC, SConfig::GetInstance().m_ListGC);
	viewMenu->AppendCheckItem(IDM_LISTWAD, _T("Show Wad"));
	viewMenu->Check(IDM_LISTWAD, SConfig::GetInstance().m_ListWad);
	viewMenu->AppendCheckItem(IDM_LISTJAP, _T("Show JAP"));
	viewMenu->Check(IDM_LISTJAP, SConfig::GetInstance().m_ListJap);
	viewMenu->AppendCheckItem(IDM_LISTPAL, _T("Show PAL"));
	viewMenu->Check(IDM_LISTPAL, SConfig::GetInstance().m_ListPal);
	viewMenu->AppendCheckItem(IDM_LISTUSA, _T("Show USA"));
	viewMenu->Check(IDM_LISTUSA, SConfig::GetInstance().m_ListUsa);
#ifdef _WIN32 //TODO test drive loading on linux, I cannot load from drive on my linux system Dolphin just crashes
	viewMenu->AppendCheckItem(IDM_LISTDRIVES, _T("Show Drives"));
	viewMenu->Check(IDM_LISTDRIVES, SConfig::GetInstance().m_ListDrives);
#endif
	viewMenu->AppendSeparator();
	viewMenu->Append(IDM_PURGECACHE, _T("Purge Cache"));
	menuBar->Append(viewMenu, _T("&View"));	

	// Help menu
	wxMenu* helpMenu = new wxMenu;
	/*helpMenu->Append(wxID_HELP, _T("&Help"));
	re-enable when there's something useful to display*/
	helpMenu->Append(IDM_HELPWEBSITE, _T("Dolphin &Web Site"));
	helpMenu->Append(IDM_HELPGOOGLECODE, _T("Dolphin at &Google Code"));
	helpMenu->AppendSeparator();
	helpMenu->Append(IDM_HELPABOUT, _T("&About..."));
	menuBar->Append(helpMenu, _T("&Help"));

	if (UseDebugger) g_pCodeWindow->CreateMenu(SConfig::GetInstance().m_LocalCoreStartupParameter, menuBar);

	// Associate the menu bar with the frame
	SetMenuBar(menuBar);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Create toolbar items
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void CFrame::PopulateToolbar(wxAuiToolBar* ToolBar)
{
	int w = m_Bitmaps[Toolbar_FileOpen].GetWidth(),
	    h = m_Bitmaps[Toolbar_FileOpen].GetHeight();
	ToolBar->SetToolBitmapSize(wxSize(w, h));

	ToolBar->AddTool(wxID_OPEN,    _T("Open"),    m_Bitmaps[Toolbar_FileOpen], _T("Open file..."));
	ToolBar->AddTool(wxID_REFRESH, _T("Refresh"), m_Bitmaps[Toolbar_Refresh], _T("Refresh"));
	ToolBar->AddTool(IDM_BROWSE, _T("Browse"),   m_Bitmaps[Toolbar_Browse], _T("Browse for an ISO directory..."));
	ToolBar->AddSeparator();
	ToolBar->AddTool(IDM_PLAY, wxT(""),   m_Bitmaps[Toolbar_Play], _T("Play"));
	ToolBar->AddTool(IDM_STOP, _T("Stop"),   m_Bitmaps[Toolbar_Stop], _T("Stop"));
#ifdef _WIN32
	ToolBar->AddTool(IDM_TOGGLE_FULLSCREEN, _T("Fullscr."),  m_Bitmaps[Toolbar_FullScreen], _T("Toggle Fullscreen"));
#endif
	ToolBar->AddTool(IDM_SCREENSHOT, _T("Scr.Shot"),   m_Bitmaps[Toolbar_FullScreen], _T("Take Screenshot"));
	ToolBar->AddSeparator();
	ToolBar->AddTool(IDM_CONFIG_MAIN, _T("Config"), m_Bitmaps[Toolbar_PluginOptions], _T("Configure..."));
	ToolBar->AddTool(IDM_CONFIG_GFX_PLUGIN, _T("Gfx"),  m_Bitmaps[Toolbar_PluginGFX], _T("Graphics settings"));
	ToolBar->AddTool(IDM_CONFIG_DSP_PLUGIN, _T("DSP"),  m_Bitmaps[Toolbar_PluginDSP], _T("DSP settings"));
	ToolBar->AddTool(IDM_CONFIG_PAD_PLUGIN, _T("Pad"),  m_Bitmaps[Toolbar_PluginPAD], _T("Pad settings"));
	ToolBar->AddTool(IDM_CONFIG_WIIMOTE_PLUGIN, _T("Wiimote"),  m_Bitmaps[Toolbar_Wiimote], _T("Wiimote settings"));
	ToolBar->AddSeparator();
	ToolBar->AddTool(IDM_HELPABOUT, _T("About"), m_Bitmaps[Toolbar_Help], _T("About Dolphin"));

	// after adding the buttons to the toolbar, must call Realize() to reflect
	// the changes
	ToolBar->Realize();
}
void CFrame::PopulateToolbarAui(wxAuiToolBar* ToolBar)
{
	int w = m_Bitmaps[Toolbar_FileOpen].GetWidth(),
	    h = m_Bitmaps[Toolbar_FileOpen].GetHeight();
	ToolBar->SetToolBitmapSize(wxSize(w, h));

	ToolBar->AddTool(IDM_SAVE_PERSPECTIVE,	wxT("Save"),	g_pCodeWindow->m_Bitmaps[Toolbar_GotoPC], wxT("Save current perspective"));
	ToolBar->AddTool(IDM_PERSPECTIVES_ADD_PANE,	wxT("Add"),	g_pCodeWindow->m_Bitmaps[Toolbar_GotoPC], wxT("Add new pane"));
	ToolBar->AddTool(IDM_EDIT_PERSPECTIVES,	wxT("Edit"),	g_pCodeWindow->m_Bitmaps[Toolbar_GotoPC], wxT("Edit current perspective"), wxITEM_CHECK);
	ToolBar->AddTool(IDM_TAB_SPLIT,	wxT("Split"),	g_pCodeWindow->m_Bitmaps[Toolbar_GotoPC], wxT("Tab split"), wxITEM_CHECK);

	ToolBar->SetToolDropDown(IDM_SAVE_PERSPECTIVE, true);

	ToolBar->Realize();
}


// Delete and recreate the toolbar
void CFrame::RecreateToolbar()
{
	m_ToolBar = new wxAuiToolBar(this, ID_TOOLBAR, wxDefaultPosition, wxDefaultSize,
				wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_OVERFLOW | wxAUI_TB_TEXT);
	PopulateToolbar(m_ToolBar);
	m_Mgr->AddPane(m_ToolBar, wxAuiPaneInfo().
				Name(wxT("TBMain")).Caption(wxT("TBMain")).
				ToolbarPane().Top().
				LeftDockable(false).RightDockable(false).Floatable(false));

	if (UseDebugger)
	{
		m_ToolBarDebug = new wxAuiToolBar(this, ID_TOOLBAR_DEBUG, wxDefaultPosition, wxDefaultSize,
					wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_OVERFLOW | wxAUI_TB_TEXT);
		g_pCodeWindow->PopulateToolbar(m_ToolBarDebug);
		m_Mgr->AddPane(m_ToolBarDebug, wxAuiPaneInfo().
				Name(wxT("TBDebug")).Caption(wxT("TBDebug")).
				ToolbarPane().Top().
				LeftDockable(false).RightDockable(false).Floatable(false));

		m_ToolBarAui = new wxAuiToolBar(this, ID_TOOLBAR_AUI, wxDefaultPosition, wxDefaultSize,
					wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_OVERFLOW | wxAUI_TB_TEXT);
		PopulateToolbarAui(m_ToolBarAui);
		m_Mgr->AddPane(m_ToolBarAui, wxAuiPaneInfo().
				Name(wxT("TBAui")).Caption(wxT("TBAui")).
				ToolbarPane().Top().
				LeftDockable(false).RightDockable(false).Floatable(false));
	}
	
	//UpdateGUI();

	/*
	wxToolBarBase* ToolBar = GetToolBar();
	long style = ToolBar ? ToolBar->GetWindowStyle() : wxTB_FLAT | wxTB_DOCKABLE | wxTB_TEXT;
	delete ToolBar;
	SetToolBar(NULL);

	style &= ~(wxTB_HORIZONTAL | wxTB_VERTICAL | wxTB_BOTTOM | wxTB_RIGHT | wxTB_HORZ_LAYOUT | wxTB_TOP);
	m_ToolBar = CreateToolBar(style, ID_TOOLBAR);

	PopulateToolbar(m_ToolBar);
	SetToolBar(m_ToolBar);
	UpdateGUI();
	*/
}

void CFrame::InitBitmaps()
{
	// Get selected theme
	int Theme = SConfig::GetInstance().m_LocalCoreStartupParameter.iTheme;

	// Save memory by only having one set of bitmaps loaded at any time. I mean, they are still
	// in the exe, which is in memory, but at least we wont make another copy of all of them. */
	switch (Theme)
	{
	case BOOMY:
	{
		// These are stored as 48x48
		m_Bitmaps[Toolbar_FileOpen]		= wxGetBitmapFromMemory(toolbar_file_open_png);
		m_Bitmaps[Toolbar_Refresh]		= wxGetBitmapFromMemory(toolbar_refresh_png);
		m_Bitmaps[Toolbar_Browse]		= wxGetBitmapFromMemory(toolbar_browse_png);
		m_Bitmaps[Toolbar_Play]			= wxGetBitmapFromMemory(toolbar_play_png);
		m_Bitmaps[Toolbar_Stop]			= wxGetBitmapFromMemory(toolbar_stop_png);
		m_Bitmaps[Toolbar_Pause]		= wxGetBitmapFromMemory(toolbar_pause_png);
		m_Bitmaps[Toolbar_PluginOptions]= wxGetBitmapFromMemory(toolbar_plugin_options_png);
		m_Bitmaps[Toolbar_PluginGFX]	= wxGetBitmapFromMemory(toolbar_plugin_gfx_png);
		m_Bitmaps[Toolbar_PluginDSP]	= wxGetBitmapFromMemory(toolbar_plugin_dsp_png);
		m_Bitmaps[Toolbar_PluginPAD]	= wxGetBitmapFromMemory(toolbar_plugin_pad_png);
		m_Bitmaps[Toolbar_Wiimote]		= wxGetBitmapFromMemory(toolbar_plugin_pad_png);
		m_Bitmaps[Toolbar_Screenshot]	= wxGetBitmapFromMemory(toolbar_fullscreen_png);
		m_Bitmaps[Toolbar_FullScreen]	= wxGetBitmapFromMemory(toolbar_fullscreen_png);
		m_Bitmaps[Toolbar_Help]			= wxGetBitmapFromMemory(toolbar_help_png);

		// Scale the 48x48 bitmaps to 24x24
		for (size_t n = Toolbar_FileOpen; n <= Toolbar_Help; n++)
		{
			m_Bitmaps[n] = wxBitmap(m_Bitmaps[n].ConvertToImage().Scale(24, 24));
		}
	}
	break;

	case VISTA:
	{
		// These are stored as 24x24 and need no scaling
		m_Bitmaps[Toolbar_FileOpen]		= wxGetBitmapFromMemory(Toolbar_Open1_png);
		m_Bitmaps[Toolbar_Refresh]		= wxGetBitmapFromMemory(Toolbar_Refresh1_png);
		m_Bitmaps[Toolbar_Browse]		= wxGetBitmapFromMemory(Toolbar_Browse1_png);
		m_Bitmaps[Toolbar_Play]			= wxGetBitmapFromMemory(Toolbar_Play1_png);
		m_Bitmaps[Toolbar_Stop]			= wxGetBitmapFromMemory(Toolbar_Stop1_png);
		m_Bitmaps[Toolbar_Pause]		= wxGetBitmapFromMemory(Toolbar_Pause1_png);
		m_Bitmaps[Toolbar_PluginOptions]= wxGetBitmapFromMemory(Toolbar_Options1_png);
		m_Bitmaps[Toolbar_PluginGFX]	= wxGetBitmapFromMemory(Toolbar_Gfx1_png);
		m_Bitmaps[Toolbar_PluginDSP]	= wxGetBitmapFromMemory(Toolbar_DSP1_png);
		m_Bitmaps[Toolbar_PluginPAD]	= wxGetBitmapFromMemory(Toolbar_Pad1_png);
		m_Bitmaps[Toolbar_Wiimote]		= wxGetBitmapFromMemory(Toolbar_Wiimote1_png);
		m_Bitmaps[Toolbar_Screenshot]	= wxGetBitmapFromMemory(Toolbar_Fullscreen1_png);
		m_Bitmaps[Toolbar_FullScreen]	= wxGetBitmapFromMemory(Toolbar_Fullscreen1_png);
		m_Bitmaps[Toolbar_Help]			= wxGetBitmapFromMemory(Toolbar_Help1_png);
	}
	break;

	case XPLASTIK:
	{
		m_Bitmaps[Toolbar_FileOpen]		= wxGetBitmapFromMemory(Toolbar_Open2_png);
		m_Bitmaps[Toolbar_Refresh]		= wxGetBitmapFromMemory(Toolbar_Refresh2_png);
		m_Bitmaps[Toolbar_Browse]		= wxGetBitmapFromMemory(Toolbar_Browse2_png);
		m_Bitmaps[Toolbar_Play]			= wxGetBitmapFromMemory(Toolbar_Play2_png);
		m_Bitmaps[Toolbar_Stop]			= wxGetBitmapFromMemory(Toolbar_Stop2_png);
		m_Bitmaps[Toolbar_Pause]		= wxGetBitmapFromMemory(Toolbar_Pause2_png);
		m_Bitmaps[Toolbar_PluginOptions]= wxGetBitmapFromMemory(Toolbar_Options2_png);
		m_Bitmaps[Toolbar_PluginGFX]	= wxGetBitmapFromMemory(Toolbar_Gfx2_png);
		m_Bitmaps[Toolbar_PluginDSP]	= wxGetBitmapFromMemory(Toolbar_DSP2_png);
		m_Bitmaps[Toolbar_PluginPAD]	= wxGetBitmapFromMemory(Toolbar_Pad2_png);
		m_Bitmaps[Toolbar_Wiimote]		= wxGetBitmapFromMemory(Toolbar_Wiimote2_png);
		m_Bitmaps[Toolbar_Screenshot]	= wxGetBitmapFromMemory(Toolbar_Fullscreen2_png);
		m_Bitmaps[Toolbar_FullScreen]	= wxGetBitmapFromMemory(Toolbar_Fullscreen2_png);
		m_Bitmaps[Toolbar_Help]			= wxGetBitmapFromMemory(Toolbar_Help2_png);
	}
	break;

	case KDE:
	{
		m_Bitmaps[Toolbar_FileOpen]		= wxGetBitmapFromMemory(Toolbar_Open3_png);
		m_Bitmaps[Toolbar_Refresh]		= wxGetBitmapFromMemory(Toolbar_Refresh3_png);
		m_Bitmaps[Toolbar_Browse]		= wxGetBitmapFromMemory(Toolbar_Browse3_png);
		m_Bitmaps[Toolbar_Play]			= wxGetBitmapFromMemory(Toolbar_Play3_png);
		m_Bitmaps[Toolbar_Stop]			= wxGetBitmapFromMemory(Toolbar_Stop3_png);
		m_Bitmaps[Toolbar_Pause]		= wxGetBitmapFromMemory(Toolbar_Pause3_png);
		m_Bitmaps[Toolbar_PluginOptions]= wxGetBitmapFromMemory(Toolbar_Options3_png);
		m_Bitmaps[Toolbar_PluginGFX]	= wxGetBitmapFromMemory(Toolbar_Gfx3_png);
		m_Bitmaps[Toolbar_PluginDSP]	= wxGetBitmapFromMemory(Toolbar_DSP3_png);
		m_Bitmaps[Toolbar_PluginPAD]	= wxGetBitmapFromMemory(Toolbar_Pad3_png);
		m_Bitmaps[Toolbar_Wiimote]		= wxGetBitmapFromMemory(Toolbar_Wiimote3_png);
		m_Bitmaps[Toolbar_Screenshot]	= wxGetBitmapFromMemory(Toolbar_Fullscreen3_png);
		m_Bitmaps[Toolbar_FullScreen]	= wxGetBitmapFromMemory(Toolbar_Fullscreen3_png);
		m_Bitmaps[Toolbar_Help]			= wxGetBitmapFromMemory(Toolbar_Help3_png);
	}
	break;

	default: PanicAlert("Theme selection went wrong");
	}

	// Update in case the bitmap has been updated
	//if (GetToolBar() != NULL) RecreateToolbar();

	aNormalFile = wxArtProvider::GetBitmap(wxART_NORMAL_FILE, wxART_OTHER, wxSize(16,16));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Menu items
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

// Start the game or change the disc
void CFrame::BootGame()
{
	// Rerecording
	#ifdef RERECORDING
		Core::RerecordingStart();
	#endif

	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		if (Core::GetState() == Core::CORE_RUN)
		{
			Core::SetState(Core::CORE_PAUSE);
		}
		else
		{
			Core::SetState(Core::CORE_RUN);
		}
		UpdateGUI();
	}
	// Start the selected ISO, return if gamelist is empty
	else if (m_GameListCtrl->GetSelectedISO() == 0) return;
	else if (m_GameListCtrl->GetSelectedISO()->IsValid())
	{
		BootManager::BootCore(m_GameListCtrl->GetSelectedISO()->GetFileName());
	}
	// Start the default ISO, or if we don't have a default ISO, start the last
	// started ISO
	else if (!SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDefaultGCM.empty() &&
		wxFileExists(wxString(SConfig::GetInstance().m_LocalCoreStartupParameter.
		m_strDefaultGCM.c_str(), wxConvUTF8)))
	{
		BootManager::BootCore(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDefaultGCM);
	}
	else if (!SConfig::GetInstance().m_LastFilename.empty() &&
		wxFileExists(wxString(SConfig::GetInstance().m_LastFilename.c_str(), wxConvUTF8)))
	{
		BootManager::BootCore(SConfig::GetInstance().m_LastFilename);
	}
}

// Open file to boot
void CFrame::OnOpen(wxCommandEvent& WXUNUSED (event))
{
	DoOpen(true);
}

void CFrame::DoOpen(bool Boot)
{
    std::string currentDir = File::GetCurrentDir();

	wxString path = wxFileSelector(
			_T("Select the file to load"),
			wxEmptyString, wxEmptyString, wxEmptyString,
			wxString::Format
			(
					_T("All GC/Wii files (elf, dol, gcm, iso, wad)|*.elf;*.dol;*.gcm;*.iso;*.gcz;*.wad|All files (%s)|%s"),
					wxFileSelectorDefaultWildcardStr,
					wxFileSelectorDefaultWildcardStr
			),
			wxFD_OPEN | wxFD_PREVIEW | wxFD_FILE_MUST_EXIST,
			this);

	bool fileChosen = !path.IsEmpty();

    std::string currentDir2 = File::GetCurrentDir();

    if (currentDir != currentDir2)
    {
        PanicAlert("Current dir changed from %s to %s after wxFileSelector!",currentDir.c_str(),currentDir2.c_str());
        File::SetCurrentDir(currentDir.c_str());
    }


	// Should we boot a new game or just change the disc?
	if (Boot)
	{
		if (!fileChosen)
			return;
		BootManager::BootCore(std::string(path.mb_str()));
	}
	else
	{
		if (!fileChosen)
			path = wxT("");
		// temp is deleted by changediscCallback
		char * temp = new char[strlen(path.mb_str())];
		strncpy(temp, path.mb_str(), strlen(path.mb_str()));
		DVDInterface::ChangeDisc(temp);
	}
}

void CFrame::OnFrameStep(wxCommandEvent& event)
{
	Frame::SetFrameStepping(event.IsChecked());
}

void CFrame::OnChangeDisc(wxCommandEvent& WXUNUSED (event))
{
	DoOpen(false);
}

void CFrame::OnRecord(wxCommandEvent& WXUNUSED (event))
{
	wxString path = wxFileSelector(
			_T("Select The Recording File"),
			wxEmptyString, wxEmptyString, wxEmptyString,
			wxString::Format
			(
					_T("Dolphin TAS Movies (*.dtm)|*.dtm|All files (%s)|%s"),
					wxFileSelectorDefaultWildcardStr,
					wxFileSelectorDefaultWildcardStr
			),
			wxFD_SAVE | wxFD_PREVIEW | wxFD_FILE_MUST_EXIST,
			this);

	if(path.IsEmpty())
		return;

	// TODO: Take controller settings from Gamecube Configuration menu
	if(Frame::BeginRecordingInput(path.mb_str(), 1))
		BootGame();
}

void CFrame::OnPlayRecording(wxCommandEvent& WXUNUSED (event))
{
	wxString path = wxFileSelector(
			_T("Select The Recording File"),
			wxEmptyString, wxEmptyString, wxEmptyString,
			wxString::Format
			(
					_T("Dolphin TAS Movies (*.dtm)|*.dtm|All files (%s)|%s"),
					wxFileSelectorDefaultWildcardStr,
					wxFileSelectorDefaultWildcardStr
			),
			wxFD_OPEN | wxFD_PREVIEW | wxFD_FILE_MUST_EXIST,
			this);

	if(path.IsEmpty())
		return;

	if(Frame::PlayInput(path.mb_str()))
		BootGame();
}

void CFrame::OnPlay(wxCommandEvent& WXUNUSED (event))
{
	BootGame();
}

void CFrame::OnBootDrive(wxCommandEvent& event)
{
	BootManager::BootCore(drives[event.GetId()-IDM_DRIVE1]);
}


// Refresh the file list and browse for a favorites directory
void CFrame::OnRefresh(wxCommandEvent& WXUNUSED (event))
{
	if (m_GameListCtrl)
	{
		m_GameListCtrl->Update();
	}
}


void CFrame::OnBrowse(wxCommandEvent& WXUNUSED (event))
{
	m_GameListCtrl->BrowseForDirectory();
}

// Create screenshot
void CFrame::OnScreenshot(wxCommandEvent& WXUNUSED (event))
{
	Core::ScreenShot();
}

// Stop the emulation
void CFrame::DoStop()
{
	// Rerecording
	#ifdef RERECORDING
		Core::RerecordingStop();
	#endif

	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		// Ask for confirmation in case the user accidently clicked Stop / Escape
		if(SConfig::GetInstance().m_LocalCoreStartupParameter.bConfirmStop)
			if(!AskYesNo("Are you sure you want to stop the current emulation?", "Confirm", wxYES_NO))
				return;

		// TODO: Show the author/description dialog here
		if(Frame::IsRecordingInput())
			Frame::EndRecordingInput();
		if(Frame::IsPlayingInput())
			Frame::EndPlayInput();
	
		Core::Stop();
		UpdateGUI();
	}
}

void CFrame::OnStop(wxCommandEvent& WXUNUSED (event))
{
	DoStop();
}

void CFrame::OnConfigMain(wxCommandEvent& WXUNUSED (event))
{
	CConfigMain ConfigMain(this);
	if (ConfigMain.ShowModal() == wxID_OK)
		m_GameListCtrl->Update();
}

void CFrame::OnPluginGFX(wxCommandEvent& WXUNUSED (event))
{
	CPluginManager::GetInstance().OpenConfig(
			GetHandle(),
			SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoPlugin.c_str(),
			PLUGIN_TYPE_VIDEO
			);
}


void CFrame::OnPluginDSP(wxCommandEvent& WXUNUSED (event))
{
	CPluginManager::GetInstance().OpenConfig(
			GetHandle(),
			SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDSPPlugin.c_str(),
			PLUGIN_TYPE_DSP
			);
}

void CFrame::OnPluginPAD(wxCommandEvent& WXUNUSED (event))
{
	CPluginManager::GetInstance().OpenConfig(
			m_Panel->GetHandle(),
			SConfig::GetInstance().m_LocalCoreStartupParameter.m_strPadPlugin[0].c_str(),
			PLUGIN_TYPE_PAD
			);
}
void CFrame::OnPluginWiimote(wxCommandEvent& WXUNUSED (event))
{
	CPluginManager::GetInstance().OpenConfig(
			GetHandle(),
			SConfig::GetInstance().m_LocalCoreStartupParameter.m_strWiimotePlugin[0].c_str(),
			PLUGIN_TYPE_WIIMOTE
			);
}

void CFrame::OnHelp(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case IDM_HELPABOUT:
		{
		AboutDolphin frame(this);
		frame.ShowModal();
		break;
		}
	case IDM_HELPWEBSITE:
		WxUtils::Launch("http://www.dolphin-emu.com/");
		break;
	case IDM_HELPGOOGLECODE:
		WxUtils::Launch("http://code.google.com/p/dolphin-emu/");
		break;
	}
}

void CFrame::OnToolBar(wxCommandEvent& event)
{
	switch (event.GetId())
	{
		case IDM_SAVE_PERSPECTIVE:
			Save();
			break;
		case IDM_PERSPECTIVES_ADD_PANE:
			AddPane();
			break;
			/*
		case IDM_PERSPECTIVE_1:
			m_ToolBarAui->ToggleTool(IDM_PERSPECTIVE_0, false);
			DoLoadPerspective(1);
			break;
			*/
		case IDM_EDIT_PERSPECTIVES:
			TogglePaneStyle(m_ToolBarAui->GetToolToggled(IDM_EDIT_PERSPECTIVES));
			break;
		case IDM_TAB_SPLIT:
			ToggleNotebookStyle(wxAUI_NB_TAB_SPLIT);
			break;
	}
}
void CFrame::OnSelectPerspective(wxCommandEvent& event)
{
	int _Selection = event.GetId() - IDM_PERSPECTIVES_0;
	if (Perspectives.size() <= _Selection) return;
	ActivePerspective = _Selection;
	DoLoadPerspective(Perspectives.at(ActivePerspective).Perspective);

	ConsoleListener* Console = LogManager::GetInstance()->getConsoleListener();
	Console->Log(LogTypes::LCUSTOM, StringFromFormat("OnSelectPerspective: %i %s\n", _Selection, Perspectives.at(ActivePerspective).Name.c_str()).c_str());
}
void CFrame::OnDropDownToolbarItem(wxAuiToolBarEvent& event)
{
	event.Skip();

    if (event.IsDropDownClicked())
    {
        wxAuiToolBar* tb = static_cast<wxAuiToolBar*>(event.GetEventObject());

        tb->SetToolSticky(event.GetId(), true);

        // create the popup menu
        wxMenu menuPopup;
        wxMenuItem* m1 =  new wxMenuItem(&menuPopup, IDM_ADD_PERSPECTIVE, wxT("Create new perspective"));
        menuPopup.Append(m1);

		if (Perspectives.size() > 0)
		{
			menuPopup.Append(new wxMenuItem(&menuPopup));
			for (int i = 0; i < Perspectives.size(); i++)
			{
				wxMenuItem* Item = new wxMenuItem(&menuPopup, IDM_PERSPECTIVES_0 + i, wxString::FromAscii(Perspectives.at(i).Name.c_str()), wxT(""), wxITEM_CHECK);
				menuPopup.Append(Item);
				if (i == ActivePerspective) Item->Check(true);
			}
		}

        // line up our menu with the button
        wxRect rect = tb->GetToolRect(event.GetId());
        wxPoint pt = tb->ClientToScreen(rect.GetBottomLeft());
        pt = ScreenToClient(pt);

        PopupMenu(&menuPopup, pt);

        // make sure the button is "un-stuck"
        tb->SetToolSticky(event.GetId(), false);
    }
}
void CFrame::OnCreatePerspective(wxCommandEvent& event)
{
    wxTextEntryDialog dlg(this, wxT("Enter a name for the new perspective:"), wxT("Create new perspective"));

    dlg.SetValue(wxString::Format(wxT("Perspective %u"), unsigned(Perspectives.size() + 1)));
    if (dlg.ShowModal() != wxID_OK) return;

	SPerspectives Tmp;
	Tmp.Name = dlg.GetValue().mb_str();
	Perspectives.push_back(Tmp);

    //if (m_perspectives.GetCount() == 0) m_perspectives_menu->AppendSeparator();
   // m_perspectives_menu->Append(ID_FirstPerspective + m_perspectives.GetCount(), dlg.GetValue());
    //m_perspectives.Add(m_mgr.SavePerspective());
}

void CFrame::TogglePaneStyle(bool On)
{
    wxAuiPaneInfoArray& AllPanes = m_Mgr->GetAllPanes();
    for (int i = 0; i < AllPanes.GetCount(); ++i)
    {
        wxAuiPaneInfo& Pane = AllPanes.Item(i);
		if (Pane.window->IsKindOf(CLASSINFO(wxAuiNotebook)))
		{
			Pane.CaptionVisible(On);
			Pane.Show();
		}
    }
	m_Mgr->Update();
}
void CFrame::ToggleNotebookStyle(long Style)
{
    wxAuiPaneInfoArray& AllPanes = m_Mgr->GetAllPanes();
    for (int i = 0, Count = AllPanes.GetCount(); i < Count; ++i)
    {
        wxAuiPaneInfo& Pane = AllPanes.Item(i);
        if (Pane.window->IsKindOf(CLASSINFO(wxAuiNotebook)))
        {
            wxAuiNotebook* NB = (wxAuiNotebook*)Pane.window;
            NB->SetWindowStyleFlag(NB->GetWindowStyleFlag() ^ Style);
            NB->Refresh();
        }
    }
}
void CFrame::ResizeConsole()
{
	for (int i = 0; i < m_NB.size(); i++)
	{
		if (!m_NB[i]) continue;
		for(u32 j = 0; j <= m_NB[i]->GetPageCount(); j++)
		{
			if (m_NB[i]->GetPageText(j).IsSameAs(wxT("Console")))
			{
				#ifdef _WIN32
				// ----------------------------------------------------------
				// Get OS version
				// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
				int wxBorder, Border, LowerBorder, MenuBar, ScrollBar, WidthReduction;
				OSVERSIONINFO osvi;
				ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
				osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
				GetVersionEx(&osvi);
				if (osvi.dwMajorVersion == 6) // Vista (same as 7?)
				{
					wxBorder = 2;
					Border = 4;
					LowerBorder = 6;
					MenuBar = 30; // Including upper border				
					ScrollBar = 19;
				}
				else // XP
				{
					wxBorder = 2;
					Border = 4;
					LowerBorder = 6;
					MenuBar = 30;					
					ScrollBar = 19;
				}
				WidthReduction = 30 - Border;
				// --------------------------------
				// Get the client size
				int X = m_NB[i]->GetClientSize().GetX();
				int Y = m_NB[i]->GetClientSize().GetY();
				int InternalWidth = X - wxBorder*2 - ScrollBar;
				int InternalHeight = Y - wxBorder*2;
				int WindowWidth = InternalWidth + Border*2;
				int WindowHeight = InternalHeight;
				// Resize buffer
				ConsoleListener* Console = LogManager::GetInstance()->getConsoleListener();
				//Console->Log(LogTypes::LNOTICE, StringFromFormat("Window WxH:%i %i\n", X, Y).c_str());
				Console->PixelSpace(0,0, InternalWidth,InternalHeight, false);
				// Move the window to hide the border
				MoveWindow(GetConsoleWindow(), -Border-wxBorder,-MenuBar-wxBorder, WindowWidth + 100,WindowHeight, true);
				// Move it to the bottom of the view order so that it doesn't hide the notebook tabs
				// ...
				#endif
			}
		}	
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Miscellaneous menus
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
// NetPlay stuff
void CFrame::OnNetPlay(wxCommandEvent& WXUNUSED (event))
{
#if defined(HAVE_SFML) && HAVE_SFML
	new NetPlay(this, m_GameListCtrl->GetGamePaths(), m_GameListCtrl->GetGameNames());
#endif
}

void CFrame::OnMemcard(wxCommandEvent& WXUNUSED (event))
{
	CMemcardManager MemcardManager(this);
	MemcardManager.ShowModal();
}

void CFrame::OnShow_CheatsWindow(wxCommandEvent& WXUNUSED (event))
{
	CheatsWindow = new wxCheatsWindow(this, wxDefaultPosition, wxSize(600, 390));
}

void CFrame::OnShow_InfoWindow(wxCommandEvent& WXUNUSED (event))
{
	InfoWindow = new wxInfoWindow(this, wxDefaultPosition, wxSize(600, 390));
}


void CFrame::OnLoadWiiMenu(wxCommandEvent& WXUNUSED (event))
{
	BootManager::BootCore(FULL_WII_MENU_DIR);
}

// Toogle fullscreen. In Windows the fullscreen mode is accomplished by expanding the m_Panel to cover
// the entire screen (when we render to the main window).
void CFrame::OnToggleFullscreen(wxCommandEvent& WXUNUSED (event))
{
	DoFullscreen(true);
	UpdateGUI();
}

void CFrame::OnToggleDualCore(wxCommandEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bUseDualCore = !SConfig::GetInstance().m_LocalCoreStartupParameter.bUseDualCore;
	SConfig::GetInstance().SaveSettings();
}

void CFrame::OnToggleSkipIdle(wxCommandEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle = !SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle;
	SConfig::GetInstance().SaveSettings();
}

void CFrame::OnLoadStateFromFile(wxCommandEvent& WXUNUSED (event))
{
	wxString path = wxFileSelector(
		_T("Select the state to load"),
		wxEmptyString, wxEmptyString, wxEmptyString,
		wxString::Format
		(
		_T("All Save States (sav, s##)|*.sav;*.s??|All files (%s)|%s"),
		wxFileSelectorDefaultWildcardStr,
		wxFileSelectorDefaultWildcardStr
		),
		wxFD_OPEN | wxFD_PREVIEW | wxFD_FILE_MUST_EXIST,
		this);

	if(path)
		State_LoadAs((const char*)path.mb_str());
}

void CFrame::OnSaveStateToFile(wxCommandEvent& WXUNUSED (event))
{
	wxString path = wxFileSelector(
		_T("Select the state to save"),
		wxEmptyString, wxEmptyString, wxEmptyString,
		wxString::Format
		(
		_T("All Save States (sav, s##)|*.sav;*.s??|All files (%s)|%s"),
		wxFileSelectorDefaultWildcardStr,
		wxFileSelectorDefaultWildcardStr
		),
		wxFD_SAVE,
		this);

	if(path)
		State_SaveAs((const char*)path.mb_str());
}

void CFrame::OnLoadLastState(wxCommandEvent& WXUNUSED (event))
{
	State_LoadLastSaved();
}

void CFrame::OnUndoLoadState(wxCommandEvent& WXUNUSED (event))
{
	State_UndoLoadState();
}

void CFrame::OnUndoSaveState(wxCommandEvent& WXUNUSED (event))
{
	State_UndoSaveState();
}


void CFrame::OnLoadState(wxCommandEvent& event)
{
	int id = event.GetId();
	int slot = id - IDM_LOADSLOT1 + 1;
	State_Load(slot);
}

void CFrame::OnSaveState(wxCommandEvent& event)
{
	int id = event.GetId();
	int slot = id - IDM_SAVESLOT1 + 1;
	State_Save(slot);
}

void CFrame::OnFrameSkip(wxCommandEvent& event)
{
	int amount = event.GetId() - IDM_FRAMESKIP0;

	Frame::SetFrameSkipping((unsigned int)amount);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Notebooks
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#ifdef _WIN32
wxWindow * CFrame::GetWxWindowHwnd(HWND hWnd)
{
	wxWindow * Win = new wxWindow();
	Win->SetHWND((WXHWND)hWnd);
	Win->AdoptAttributesFromHWND();
	return Win;
}
#endif
wxWindow * CFrame::GetWxWindow(wxString Name)
{
	#ifdef _WIN32
	HWND hWnd = ::FindWindow(NULL, Name.c_str());
	if (hWnd)
	{
		wxWindow * Win = new wxWindow();
		Win->SetHWND((WXHWND)hWnd);
		Win->AdoptAttributesFromHWND();
		return Win;
	}
	else
	#endif
	if (FindWindowByName(Name))
	{
		return FindWindowByName(Name);
	}
	else if (FindWindowByLabel(Name))
	{
		return FindWindowByLabel(Name);
	}
	else if (GetNootebookPage(Name))
	{
		return GetNootebookPage(Name);
	}
	else
		return NULL;
}
wxWindow * CFrame::GetNootebookPage(wxString Name)
{
	for (int i = 0; i < m_NB.size(); i++)
	{
		if (!m_NB[i]) continue;
		for(u32 j = 0; j < m_NB[i]->GetPageCount(); j++)
		{
			if (m_NB[i]->GetPageText(j).IsSameAs(Name)) return m_NB[i]->GetPage(j);
		}	
	}
	return NULL;
}
void CFrame::AddRemoveBlankPage()
{
	for (int i = 0; i < m_NB.size(); i++)
	{
		if (!m_NB[i]) continue;
		for(u32 j = 0; j < m_NB[i]->GetPageCount(); j++)
		{
			if (m_NB[i]->GetPageText(j).IsSameAs(wxT("<>")) && m_NB[i]->GetPageCount() > 1) m_NB[i]->DeletePage(j);
		}	
		if (m_NB[i]->GetPageCount() == 0) m_NB[i]->AddPage(CreateEmptyPanel(), wxT("<>"), true);
	}
}
int CFrame::GetNootebookAffiliation(wxString Name)
{
	for (int i = 0; i < m_NB.size(); i++)
	{
		if (!m_NB[i]) continue;
		for(u32 j = 0; j < m_NB[i]->GetPageCount(); j++)
		{
			if (m_NB[i]->GetPageText(j).IsSameAs(Name)) return i;
		}	
	}
	return -1;
}
void CFrame::DoToggleWindow(int Id, bool Show)
{			
	switch (Id)
	{
		case IDM_LOGWINDOW: ToggleLogWindow(Show, UseDebugger ? g_pCodeWindow->iLogWindow : 0); break;
		case IDM_CONSOLEWINDOW: ToggleConsole(Show, UseDebugger ? g_pCodeWindow->iConsoleWindow : 0); break;
		case IDM_CODEWINDOW: g_pCodeWindow->OnToggleCodeWindow(Show, g_pCodeWindow->iCodeWindow); break;
		case IDM_REGISTERWINDOW: g_pCodeWindow->OnToggleRegisterWindow(Show, g_pCodeWindow->iRegisterWindow); break;
		case IDM_BREAKPOINTWINDOW: g_pCodeWindow->OnToggleBreakPointWindow(Show, g_pCodeWindow->iBreakpointWindow); break;
		case IDM_MEMORYWINDOW: g_pCodeWindow->OnToggleMemoryWindow(Show, g_pCodeWindow->iMemoryWindow); break;
		case IDM_JITWINDOW: g_pCodeWindow->OnToggleJitWindow(Show, g_pCodeWindow->iJitWindow); break;
		case IDM_SOUNDWINDOW: g_pCodeWindow->OnToggleSoundWindow(Show, g_pCodeWindow->iSoundWindow); break;
		case IDM_VIDEOWINDOW: g_pCodeWindow->OnToggleVideoWindow(Show, g_pCodeWindow->iVideoWindow); break;
	}
}
void CFrame::OnNotebookPageChanged(wxAuiNotebookEvent& event)
{
	event.Skip();
	if (!UseDebugger) return;

	// Remove the blank page if any
	AddRemoveBlankPage();

	// Update the notebook affiliation
	if(GetNootebookAffiliation(wxT("Log")) >= 0) g_pCodeWindow->iLogWindow = GetNootebookAffiliation(wxT("Log"));
	if(GetNootebookAffiliation(wxT("Console")) >= 0) g_pCodeWindow->iConsoleWindow = GetNootebookAffiliation(wxT("Console"));
	if(GetNootebookAffiliation(wxT("Code")) >= 0) g_pCodeWindow->iCodeWindow = GetNootebookAffiliation(wxT("Code"));
	if(GetNootebookAffiliation(wxT("Registers")) >= 0) g_pCodeWindow->iRegisterWindow = GetNootebookAffiliation(wxT("Registers"));
	if(GetNootebookAffiliation(wxT("Breakpoints")) >= 0) g_pCodeWindow->iBreakpointWindow = GetNootebookAffiliation(wxT("Breakpoints"));
	if(GetNootebookAffiliation(wxT("JIT")) >= 0) g_pCodeWindow->iJitWindow = GetNootebookAffiliation(wxT("JIT"));
	if(GetNootebookAffiliation(wxT("Memory")) >= 0) g_pCodeWindow->iMemoryWindow = GetNootebookAffiliation(wxT("Memory"));
	if(GetNootebookAffiliation(wxT("Sound")) >= 0) g_pCodeWindow->iSoundWindow = GetNootebookAffiliation(wxT("Sound"));
	if(GetNootebookAffiliation(wxT("Video")) >= 0) g_pCodeWindow->iVideoWindow = GetNootebookAffiliation(wxT("Video"));
}
void CFrame::OnNotebookPageClose(wxAuiNotebookEvent& event)
{
	// Override event
	event.Veto();

    wxAuiNotebook* Ctrl = (wxAuiNotebook*)event.GetEventObject();

	if (Ctrl->GetPageText(event.GetSelection()).IsSameAs(wxT("Log"))) { GetMenuBar()->FindItem(IDM_LOGWINDOW)->Check(false); DoToggleWindow(IDM_LOGWINDOW, false); }
	if (Ctrl->GetPageText(event.GetSelection()).IsSameAs(wxT("Console"))) { GetMenuBar()->FindItem(IDM_CONSOLEWINDOW)->Check(false); DoToggleWindow(IDM_CONSOLEWINDOW, false); }
	if (Ctrl->GetPageText(event.GetSelection()).IsSameAs(wxT("Registers"))) { GetMenuBar()->FindItem(IDM_REGISTERWINDOW)->Check(false); DoToggleWindow(IDM_REGISTERWINDOW, false); }
	if (Ctrl->GetPageText(event.GetSelection()).IsSameAs(wxT("Breakpoints"))) { GetMenuBar()->FindItem(IDM_BREAKPOINTWINDOW)->Check(false); DoToggleWindow(IDM_BREAKPOINTWINDOW, false); }
	if (Ctrl->GetPageText(event.GetSelection()).IsSameAs(wxT("JIT"))) { GetMenuBar()->FindItem(IDM_JITWINDOW)->Check(false); DoToggleWindow(IDM_JITWINDOW, false); }
	if (Ctrl->GetPageText(event.GetSelection()).IsSameAs(wxT("Memory"))) { GetMenuBar()->FindItem(IDM_MEMORYWINDOW)->Check(false); DoToggleWindow(IDM_MEMORYWINDOW, false); }
	if (Ctrl->GetPageText(event.GetSelection()).IsSameAs(wxT("Sound"))) { GetMenuBar()->FindItem(IDM_SOUNDWINDOW)->Check(false); DoToggleWindow(IDM_SOUNDWINDOW, false); }
	if (Ctrl->GetPageText(event.GetSelection()).IsSameAs(wxT("Video"))) { GetMenuBar()->FindItem(IDM_VIDEOWINDOW)->Check(false); DoToggleWindow(IDM_VIDEOWINDOW, false); }
}
void CFrame::OnAllowNotebookDnD(wxAuiNotebookEvent& event)
{
	event.Skip();
	event.Allow();
	wxAuiNotebook* Ctrl = (wxAuiNotebook*)event.GetEventObject();
	// If we drag away the last one the tab bar goes away and we can't add any panes to it
	//if (Ctrl->GetPageCount() == 1) Ctrl->AddPage(CreateEmptyPanel(), wxT("<>"), true);
}
void CFrame::HidePane()
{
	if (m_NB[0]->GetPageCount() == 0)
		m_Mgr->GetPane(wxT("Pane1")).Hide();
	else
		m_Mgr->GetPane(wxT("Pane1")).Show();
	 m_Mgr->Update();

	SetSimplePaneSize();
}
void CFrame::DoRemovePageString(wxString Str, bool Hide)
{
	for (int i = 0; i < m_NB.size(); i++)
	{
		if (m_NB[i])
		{
			for (int j = 0; j < m_NB[i]->GetPageCount(); j++)
			{
				if (m_NB[i]->GetPageText(j).IsSameAs(Str)) { m_NB[i]->RemovePage(j); break; }	
				ConsoleListener* Console = LogManager::GetInstance()->getConsoleListener();
			}
		}
	}
	//if (Hide) Win->Hide();
}
void CFrame::DoAddPage(wxWindow * Win, int i, std::string Name)
{
	if (!Win) return;
	if (m_NB.size() == 0) return;
	if (i < 0 || i > m_NB.size()-1) i = 0;
	if (Win && m_NB[i]->GetPageIndex(Win) != wxNOT_FOUND) return;
	m_NB[i]->AddPage(Win, wxString::FromAscii(Name.c_str()), true, aNormalFile );

	ConsoleListener* Console = LogManager::GetInstance()->getConsoleListener();
	Console->Log(LogTypes::LCUSTOM, StringFromFormat("Add: %s\n", Name.c_str()).c_str());
}
void CFrame::DoRemovePage(wxWindow * Win, bool Hide)
{
	// If m_dialog is NULL, then possibly the system didn't report the checked menu item status correctly.
	// It should be true just after the menu item was selected, if there was no modeless dialog yet.
	wxASSERT(Win != NULL);

	if (Win)
	{
		for (int i = 0; i < m_NB.size(); i++)
		{
			if (m_NB[i])
			{
				if (m_NB[i]->GetPageIndex(Win) != wxNOT_FOUND) m_NB[i]->RemovePage(m_NB[i]->GetPageIndex(Win));
			}
		}
		if (Hide) Win->Hide();
	}
}

// Enable and disable the toolbar
void CFrame::OnToggleToolbar(wxCommandEvent& event)
{
	SConfig::GetInstance().m_InterfaceToolbar = event.IsChecked();
	DoToggleToolbar(event.IsChecked());
}
void CFrame::DoToggleToolbar(bool Show)
{
	if (Show)
	{
		m_Mgr->GetPane(wxT("TBMain")).Show();
		if (UseDebugger) { m_Mgr->GetPane(wxT("TBDebug")).Show(); m_Mgr->GetPane(wxT("TBAui")).Show(); }
		m_Mgr->Update();
	}
	else
	{
		m_Mgr->GetPane(wxT("TBMain")).Hide();
		if (UseDebugger) { m_Mgr->GetPane(wxT("TBDebug")).Hide(); m_Mgr->GetPane(wxT("TBAui")).Hide(); }
		m_Mgr->Update();
	}
}

// Enable and disable the status bar
void CFrame::OnToggleStatusbar(wxCommandEvent& event)
{
	SConfig::GetInstance().m_InterfaceStatusbar = event.IsChecked();
	if (SConfig::GetInstance().m_InterfaceStatusbar == true)
		m_pStatusBar->Show();
	else
		m_pStatusBar->Hide();

	this->SendSizeEvent();
}

// Enable and disable the log window
void CFrame::OnToggleLogWindow(wxCommandEvent& event)
{
	DoToggleWindow(event.GetId(), event.IsChecked());
}
void CFrame::ToggleLogWindow(bool Show, int i)
{
	SConfig::GetInstance().m_InterfaceLogWindow = Show;
	if (Show)
	{
		if (!m_LogWindow) m_LogWindow = new CLogWindow(this);
		DoAddPage(m_LogWindow, i, "Log");
	}
	else
	{
		DoRemovePage(m_LogWindow);
	}

	// Hide pane
	if (!UseDebugger) HidePane();

	// Make sure the check is updated (if wxw isn't calling this func)
	//GetMenuBar()->FindItem(IDM_LOGWINDOW)->Check(Show);
}
// Enable and disable the console
void CFrame::OnToggleConsole(wxCommandEvent& event)
{
	DoToggleWindow(event.GetId(), event.IsChecked());
}
void CFrame::ToggleConsole(bool Show, int i)
{
	ConsoleListener *Console = LogManager::GetInstance()->getConsoleListener();
	SConfig::GetInstance().m_InterfaceConsole = Show;

	if (Show)
	{
		if (m_NB.size() == 0) return;
		if (i < 0 || i > m_NB.size()-1) i = 0;
		#ifdef _WIN32
		wxWindow *Win = GetWxWindowHwnd(GetConsoleWindow());
		if (Win && m_NB[i]->GetPageIndex(Win) != wxNOT_FOUND) return;
		{
			#else
			Console->Open();
			#endif

			#ifdef _WIN32
			if(!GetConsoleWindow()) Console->Open(); else ShowWindow(GetConsoleWindow(),SW_SHOW);
		}
		Win = GetWxWindowHwnd(GetConsoleWindow());
		// Can we remove the border?
		//Win->SetWindowStyleFlag(wxNO_BORDER);
		//SetWindowLong(GetConsoleWindow(), GWL_STYLE, WS_VISIBLE);
		// Create parent window
		wxPanel * ConsoleParent = CreateEmptyPanel();
		::SetParent(GetConsoleWindow(), (HWND)ConsoleParent->GetHWND());
		//Win->SetParent(ConsoleParent);
		//if (Win) m_NB[i]->AddPage(Win, wxT("Console"), true, aNormalFile );
		if (Win) m_NB[i]->AddPage(ConsoleParent, wxT("Console"), true, aNormalFile );
		#endif
	}
	else // hide
	{
		#ifdef _WIN32
		//wxWindow *Win = GetWxWindowHwnd(GetConsoleWindow());
		//DoRemovePage (Win, true);
		DoRemovePageString(wxT("Console"), true);
		#else
		Console->Close();
		#endif
		#ifdef _WIN32
		if(GetConsoleWindow()) ShowWindow(GetConsoleWindow(),SW_HIDE);
		#endif
	}

	// Hide pane
	if (!UseDebugger) HidePane();

	// Make sure the check is updated (if wxw isn't calling this func)
	//GetMenuBar()->FindItem(IDM_CONSOLEWINDOW)->Check(Show);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
// GUI
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void CFrame::OnManagerResize(wxAuiManagerEvent& event)
{
	event.Skip();
	ResizeConsole();
}
void CFrame::OnResize(wxSizeEvent& event)
{
	event.Skip();
	// fit frame content, not needed right now
	//FitInside();
	DoMoveIcons();  // In FrameWiimote.cpp
}

// Update the enabled/disabled status
void CFrame::UpdateGUI()
{
	// Save status
	bool initialized = Core::isRunning();
	bool running = Core::GetState() == Core::CORE_RUN;
	bool paused = Core::GetState() == Core::CORE_PAUSE;

	// Make sure that we have a toolbar
	if (m_ToolBar != NULL)
	{
		// Enable/disable the Config and Stop buttons
		//GetToolBar()->EnableTool(IDM_CONFIG_MAIN, !initialized);
		m_ToolBar->EnableTool(wxID_OPEN, !initialized);
		m_ToolBar->EnableTool(wxID_REFRESH, !initialized); // Don't allow refresh when we don't show the list
		m_ToolBar->EnableTool(IDM_STOP, running || paused);
		m_ToolBar->EnableTool(IDM_SCREENSHOT, running || paused);
	}

	// File
	GetMenuBar()->FindItem(wxID_OPEN)->Enable(!initialized);
	m_pSubMenuDrive->Enable(!initialized);
	GetMenuBar()->FindItem(wxID_REFRESH)->Enable(!initialized);
	GetMenuBar()->FindItem(IDM_BROWSE)->Enable(!initialized);

	// Emulation
	GetMenuBar()->FindItem(IDM_STOP)->Enable(running || paused);
	GetMenuBar()->FindItem(IDM_RECORD)->Enable(!initialized);
	GetMenuBar()->FindItem(IDM_PLAYRECORD)->Enable(!initialized);
	GetMenuBar()->FindItem(IDM_FRAMESTEP)->Enable(running || paused);
	GetMenuBar()->FindItem(IDM_SCREENSHOT)->Enable(running || paused);
	m_pSubMenuLoad->Enable(initialized);
	m_pSubMenuSave->Enable(initialized);

	// Let's enable it by default.
	//m_pSubMenuFrameSkipping->Enable(initialized);

	// Misc
	GetMenuBar()->FindItem(IDM_CHANGEDISC)->Enable(initialized);
	if (DiscIO::CNANDContentManager::Access().GetNANDLoader(FULL_WII_MENU_DIR).IsValid())
		GetMenuBar()->FindItem(IDM_LOAD_WII_MENU)->Enable(!initialized);

	if (running)
	{
		if (m_ToolBar != NULL)
		{
			m_ToolBar->SetToolBitmap(IDM_PLAY, m_Bitmaps[Toolbar_Pause]);
			m_ToolBar->SetToolShortHelp(IDM_PLAY, _("Pause"));
			m_ToolBar->SetToolLabel(IDM_PLAY, _("Pause"));
		}
		GetMenuBar()->FindItem(IDM_PLAY)->SetText(_("&Pause\tF10"));
	}
	else
	{
		if (m_ToolBar != NULL)
		{
			m_ToolBar->SetToolBitmap(IDM_PLAY, m_Bitmaps[Toolbar_Play]);
			m_ToolBar->SetToolShortHelp(IDM_PLAY, _("Play"));
			m_ToolBar->SetToolLabel(IDM_PLAY, wxT(" Play "));
		}
		GetMenuBar()->FindItem(IDM_PLAY)->SetText(_("&Play\tF10"));
		
	}

	if (!initialized)
	{
		if (m_GameListCtrl && !m_GameListCtrl->IsShown())
		{
			m_GameListCtrl->Enable();
			m_GameListCtrl->Show();
			sizerPanel->FitInside(m_Panel);
		}
	}
	else
	{
		if (m_GameListCtrl && m_GameListCtrl->IsShown())
		{
			m_GameListCtrl->Disable();
			m_GameListCtrl->Hide();
		}
	}

	// Commit changes to toolbar
	if (m_ToolBar != NULL) m_ToolBar->Realize();
	if (UseDebugger) g_pCodeWindow->Update();
	// Commit changes to manager
	m_Mgr->Update();
}

void CFrame::GameListChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case IDM_LISTWII:
		SConfig::GetInstance().m_ListWii = event.IsChecked();
		break;
	case IDM_LISTGC:
		SConfig::GetInstance().m_ListGC = event.IsChecked();
		break;
	case IDM_LISTWAD:
		SConfig::GetInstance().m_ListWad = event.IsChecked();
		break;
	case IDM_LISTJAP:
		SConfig::GetInstance().m_ListJap = event.IsChecked();
		break;
	case IDM_LISTPAL:
		SConfig::GetInstance().m_ListPal = event.IsChecked();
		break;
	case IDM_LISTUSA:
		SConfig::GetInstance().m_ListUsa = event.IsChecked();
		break;
	case IDM_LISTDRIVES:
		SConfig::GetInstance().m_ListDrives = event.IsChecked();
		break;
	case IDM_PURGECACHE:
		CFileSearch::XStringVector Directories;
		Directories.push_back(FULL_CACHE_DIR);
		CFileSearch::XStringVector Extensions;
		Extensions.push_back("*.cache");
		
		CFileSearch FileSearch(Extensions, Directories);
		const CFileSearch::XStringVector& rFilenames = FileSearch.GetFileNames();
		
		for (u32 i = 0; i < rFilenames.size(); i++)
		{
			File::Delete(rFilenames[i].c_str());
		}
		break;
	}
	
	if (m_GameListCtrl)
	{
		m_GameListCtrl->Update();
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////