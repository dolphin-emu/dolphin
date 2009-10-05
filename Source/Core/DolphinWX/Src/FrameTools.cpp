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


// Why doesn't it work on windows?
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


// Create menu items
// ---------------------
void CFrame::CreateMenu()
{
	if (GetMenuBar()) GetMenuBar()->Destroy();

	m_MenuBar = new wxMenuBar(wxMB_DOCKABLE);

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
	//	fileMenu->Append(IDM_RESTART, g_pCodeWindow ? _T("Restart in regular &mode") : _T("&Restart in debugging &mode"));
	fileMenu->Append(wxID_EXIT, _T("E&xit\tAlt+F4"));
	m_MenuBar->Append(fileMenu, _T("&File"));

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
	m_MenuBar->Append(emulationMenu, _T("&Emulation"));

	// Options menu
	wxMenu* pOptionsMenu = new wxMenu;
	pOptionsMenu->Append(IDM_CONFIG_MAIN, _T("Co&nfigure..."));
	pOptionsMenu->AppendSeparator();
	pOptionsMenu->Append(IDM_CONFIG_GFX_PLUGIN, _T("&Graphics Settings"));
	pOptionsMenu->Append(IDM_CONFIG_DSP_PLUGIN, _T("&DSP Settings"));
	pOptionsMenu->Append(IDM_CONFIG_PAD_PLUGIN, _T("&Pad Settings"));
	pOptionsMenu->Append(IDM_CONFIG_WIIMOTE_PLUGIN, _T("&Wiimote Settings"));
	pOptionsMenu->AppendSeparator();
	pOptionsMenu->Append(IDM_TOGGLE_FULLSCREEN, _T("&Fullscreen\tAlt+Enter"));	
	if (g_pCodeWindow)
	{
		pOptionsMenu->AppendSeparator();
		g_pCodeWindow->CreateMenuOptions(NULL, pOptionsMenu);	
	}
	m_MenuBar->Append(pOptionsMenu, _T("&Options"));

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

	m_MenuBar->Append(toolsMenu, _T("&Tools"));

	wxMenu* viewMenu = new wxMenu;
	viewMenu->AppendCheckItem(IDM_TOGGLE_TOOLBAR, _T("Show &Toolbar"));
	viewMenu->Check(IDM_TOGGLE_TOOLBAR, SConfig::GetInstance().m_InterfaceToolbar);
	viewMenu->AppendCheckItem(IDM_TOGGLE_STATUSBAR, _T("Show &Statusbar"));
	viewMenu->Check(IDM_TOGGLE_STATUSBAR, SConfig::GetInstance().m_InterfaceStatusbar);
	viewMenu->AppendSeparator();
	viewMenu->AppendCheckItem(IDM_LOGWINDOW, _T("Show &Logwindow"));
	viewMenu->Check(IDM_LOGWINDOW, SConfig::GetInstance().m_InterfaceLogWindow);
	viewMenu->AppendCheckItem(IDM_CONSOLEWINDOW, _T("Show &Console"));
	viewMenu->Check(IDM_CONSOLEWINDOW, SConfig::GetInstance().m_InterfaceConsole);
	viewMenu->AppendSeparator();

	if (g_pCodeWindow)
	{
		g_pCodeWindow->CreateMenuView(NULL, viewMenu);
		viewMenu->AppendSeparator();
	}

	wxMenu *platformMenu = new wxMenu;
	viewMenu->AppendSubMenu(platformMenu, _T("Show Platforms"));
	platformMenu->AppendCheckItem(IDM_LISTWII, _T("Show Wii"));
	platformMenu->Check(IDM_LISTWII, SConfig::GetInstance().m_ListWii);
	platformMenu->AppendCheckItem(IDM_LISTGC, _T("Show GameCube"));
	platformMenu->Check(IDM_LISTGC, SConfig::GetInstance().m_ListGC);
	platformMenu->AppendCheckItem(IDM_LISTWAD, _T("Show Wad"));
	platformMenu->Check(IDM_LISTWAD, SConfig::GetInstance().m_ListWad);

	wxMenu *regionMenu = new wxMenu;
	viewMenu->AppendSubMenu(regionMenu, _T("Show Regions"));
	regionMenu->AppendCheckItem(IDM_LISTJAP, _T("Show JAP"));
	regionMenu->Check(IDM_LISTJAP, SConfig::GetInstance().m_ListJap);
	regionMenu->AppendCheckItem(IDM_LISTPAL, _T("Show PAL"));
	regionMenu->Check(IDM_LISTPAL, SConfig::GetInstance().m_ListPal);
	regionMenu->AppendCheckItem(IDM_LISTUSA, _T("Show USA"));
	regionMenu->Check(IDM_LISTUSA, SConfig::GetInstance().m_ListUsa);

	viewMenu->AppendCheckItem(IDM_LISTDRIVES, _T("Show Drives"));
	viewMenu->Check(IDM_LISTDRIVES, SConfig::GetInstance().m_ListDrives);
	viewMenu->AppendSeparator();
	viewMenu->Append(IDM_PURGECACHE, _T("Purge Cache"));
	m_MenuBar->Append(viewMenu, _T("&View"));	

	if (g_pCodeWindow) g_pCodeWindow->CreateMenu(SConfig::GetInstance().m_LocalCoreStartupParameter, m_MenuBar);

	// Help menu
	wxMenu* helpMenu = new wxMenu;
	/*helpMenu->Append(wxID_HELP, _T("&Help"));
	re-enable when there's something useful to display*/
	helpMenu->Append(IDM_HELPWEBSITE, _T("Dolphin &Web Site"));
	helpMenu->Append(IDM_HELPGOOGLECODE, _T("Dolphin at &Google Code"));
	helpMenu->AppendSeparator();
	helpMenu->Append(IDM_HELPABOUT, _T("&About..."));
	m_MenuBar->Append(helpMenu, _T("&Help"));

	// Associate the menu bar with the frame
	SetMenuBar(m_MenuBar);
}




// Create toolbar items
// ---------------------
void CFrame::PopulateToolbar(wxAuiToolBar* ToolBar)
{
	int w = m_Bitmaps[Toolbar_FileOpen].GetWidth(),
	    h = m_Bitmaps[Toolbar_FileOpen].GetHeight();
		ToolBar->SetToolBitmapSize(wxSize(w, h));
		

	ToolBar->AddTool(wxID_OPEN,    _T("Open"),    m_Bitmaps[Toolbar_FileOpen], _T("Open file..."));
	ToolBar->AddTool(wxID_REFRESH, _T("Refresh"), m_Bitmaps[Toolbar_Refresh], _T("Refresh"));
	ToolBar->AddTool(IDM_BROWSE, _T("Browse"),   m_Bitmaps[Toolbar_Browse], _T("Browse for an ISO directory..."));
	ToolBar->AddSeparator();
	ToolBar->AddTool(IDM_PLAY, wxT("Play"),   m_Bitmaps[Toolbar_Play], _T("Play"));
	ToolBar->AddTool(IDM_STOP, _T("Stop"),   m_Bitmaps[Toolbar_Stop], _T("Stop"));
	ToolBar->AddTool(IDM_TOGGLE_FULLSCREEN, _T("Fullscr."),  m_Bitmaps[Toolbar_FullScreen], _T("Toggle Fullscreen"));
	ToolBar->AddTool(IDM_SCREENSHOT, _T("Scr.Shot"),   m_Bitmaps[Toolbar_FullScreen], _T("Take Screenshot"));
	ToolBar->AddSeparator();
	ToolBar->AddTool(IDM_CONFIG_MAIN, _T("Config"), m_Bitmaps[Toolbar_PluginOptions], _T("Configure..."));
	ToolBar->AddTool(IDM_CONFIG_GFX_PLUGIN, _T("Graphics"),  m_Bitmaps[Toolbar_PluginGFX], _T("Graphics settings"));
	ToolBar->AddTool(IDM_CONFIG_DSP_PLUGIN, _T("DSP"),  m_Bitmaps[Toolbar_PluginDSP], _T("DSP settings"));
	ToolBar->AddTool(IDM_CONFIG_PAD_PLUGIN, _T("Pad"),  m_Bitmaps[Toolbar_PluginPAD], _T("Pad settings"));
	ToolBar->AddTool(IDM_CONFIG_WIIMOTE_PLUGIN, _T("Wiimote"),  m_Bitmaps[Toolbar_Wiimote], _T("Wiimote settings"));

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
	ToolBar->AddTool(IDM_EDIT_PERSPECTIVES,	wxT("Edit"),	g_pCodeWindow->m_Bitmaps[Toolbar_GotoPC], wxT("Edit current perspective"));

	ToolBar->SetToolDropDown(IDM_SAVE_PERSPECTIVE, true);
	ToolBar->SetToolDropDown(IDM_EDIT_PERSPECTIVES, true);	

	ToolBar->Realize();
}


// Delete and recreate the toolbar
void CFrame::RecreateToolbar()
{
	if (m_ToolBar)
	{
		m_Mgr->DetachPane(m_ToolBar);
		m_ToolBar->Destroy();
	}

	m_ToolBar = new wxAuiToolBar(this, ID_TOOLBAR, wxDefaultPosition, wxDefaultSize, TOOLBAR_STYLE);
	PopulateToolbar(m_ToolBar);
	
	m_Mgr->AddPane(m_ToolBar, wxAuiPaneInfo().
				Name(wxT("TBMain")).Caption(wxT("TBMain")).
				ToolbarPane().Top().
				LeftDockable(false).RightDockable(false).Floatable(false));

	if (g_pCodeWindow && !m_ToolBarDebug)
	{
		m_ToolBarDebug = new wxAuiToolBar(this, ID_TOOLBAR_DEBUG, wxDefaultPosition, wxDefaultSize, TOOLBAR_STYLE);
		g_pCodeWindow->PopulateToolbar(m_ToolBarDebug);
		
		m_Mgr->AddPane(m_ToolBarDebug, wxAuiPaneInfo().
				Name(wxT("TBDebug")).Caption(wxT("TBDebug")).
				ToolbarPane().Top().
				LeftDockable(false).RightDockable(false).Floatable(false));

		m_ToolBarAui = new wxAuiToolBar(this, ID_TOOLBAR_AUI, wxDefaultPosition, wxDefaultSize, TOOLBAR_STYLE);
		PopulateToolbarAui(m_ToolBarAui);
		m_Mgr->AddPane(m_ToolBarAui, wxAuiPaneInfo().
				Name(wxT("TBAui")).Caption(wxT("TBAui")).
				ToolbarPane().Top().
				LeftDockable(false).RightDockable(false).Floatable(false));
	}

	UpdateGUI();
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
	if (m_ToolBar != NULL) RecreateToolbar();

	aNormalFile = wxArtProvider::GetBitmap(wxART_NORMAL_FILE, wxART_OTHER, wxSize(16,16));
}




// Menu items
// ---------------------

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
			Core::SetState(Core::CORE_PAUSE);
		else
			Core::SetState(Core::CORE_RUN);
		UpdateGUI();
	}
	// Start the selected ISO, or try one of the saved paths.
	// If all that fails, ask to add a dir and don't boot
	else if (m_GameListCtrl->GetSelectedISO() != NULL)
	{
		if (m_GameListCtrl->GetSelectedISO()->IsValid())
			BootManager::BootCore(m_GameListCtrl->GetSelectedISO()->GetFileName());
	}
	else
	{
		SCoreStartupParameter& StartUp = SConfig::GetInstance().m_LocalCoreStartupParameter;

		if (!StartUp.m_strDefaultGCM.empty()
			&&	wxFileExists(wxString(StartUp.m_strDefaultGCM.c_str(), wxConvUTF8)))
		{
			BootManager::BootCore(StartUp.m_strDefaultGCM);
		}
		else if (!SConfig::GetInstance().m_LastFilename.empty()
			&& wxFileExists(wxString(SConfig::GetInstance().m_LastFilename.c_str(), wxConvUTF8)))
		{
			BootManager::BootCore(SConfig::GetInstance().m_LastFilename);
		}
		else
		{
			m_GameListCtrl->BrowseForDirectory();
			return;
		}
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
	if (m_GameListCtrl) m_GameListCtrl->Update();
}


void CFrame::OnBrowse(wxCommandEvent& WXUNUSED (event))
{
	if (m_GameListCtrl) m_GameListCtrl->BrowseForDirectory();
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
			if(!AskYesNo("Do want to stop the current emulation?", "Confirm", wxYES_NO))
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

void CFrame::ClearStatusBar()
{
	if (this->GetStatusBar()->IsEnabled()) this->GetStatusBar()->SetStatusText(wxT(""),0);
}

void CFrame::StatusBarMessage(const char * Text, ...)
{
	const int MAX_BYTES = 1024*10;
	char Str[MAX_BYTES];
	va_list ArgPtr;
	int Cnt;
	va_start(ArgPtr, Text);
	Cnt = vsnprintf(Str, MAX_BYTES, Text, ArgPtr);
	va_end(ArgPtr);

	if (this->GetStatusBar()->IsEnabled()) this->GetStatusBar()->SetStatusText(wxString::FromAscii(Str),0);
}


// Miscellaneous menus
// ---------------------
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
	DoFullscreen(!IsFullScreen());
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

	if(!path.IsEmpty())
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

	if(! path.IsEmpty())
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




// GUI
// ---------------------

// Update the enabled/disabled status
void CFrame::UpdateGUI()
{
	// Save status
	bool Initialized = Core::isRunning();
	bool Running = Core::GetState() == Core::CORE_RUN;
	bool Paused = Core::GetState() == Core::CORE_PAUSE;

	// Make sure that we have a toolbar
	if (m_ToolBar)
	{
		// Enable/disable the Config and Stop buttons
		m_ToolBar->EnableTool(wxID_OPEN, !Initialized);
		m_ToolBar->EnableTool(wxID_REFRESH, !Initialized); // Don't allow refresh when we don't show the list
		m_ToolBar->EnableTool(IDM_STOP, Running || Paused);
		m_ToolBar->EnableTool(IDM_SCREENSHOT, Running || Paused);
	}

	// File
	GetMenuBar()->FindItem(wxID_OPEN)->Enable(!Initialized);
	m_pSubMenuDrive->Enable(!Initialized);
	GetMenuBar()->FindItem(wxID_REFRESH)->Enable(!Initialized);
	GetMenuBar()->FindItem(IDM_BROWSE)->Enable(!Initialized);

	// Emulation
	GetMenuBar()->FindItem(IDM_STOP)->Enable(Running || Paused);
	GetMenuBar()->FindItem(IDM_RECORD)->Enable(!Initialized);
	GetMenuBar()->FindItem(IDM_PLAYRECORD)->Enable(!Initialized);
	GetMenuBar()->FindItem(IDM_FRAMESTEP)->Enable(Running || Paused);
	GetMenuBar()->FindItem(IDM_SCREENSHOT)->Enable(Running || Paused);
	m_pSubMenuLoad->Enable(Initialized);
	m_pSubMenuSave->Enable(Initialized);

	// Misc
	GetMenuBar()->FindItem(IDM_CHANGEDISC)->Enable(Initialized);
	if (DiscIO::CNANDContentManager::Access().GetNANDLoader(FULL_WII_MENU_DIR).IsValid())
		GetMenuBar()->FindItem(IDM_LOAD_WII_MENU)->Enable(!Initialized);

	if (Running)
	{
		if (m_ToolBar)
		{
			m_ToolBar->SetToolBitmap(IDM_PLAY, m_Bitmaps[Toolbar_Pause]);
			m_ToolBar->SetToolShortHelp(IDM_PLAY, _("Pause"));
			m_ToolBar->SetToolLabel(IDM_PLAY, _("Pause"));
		}
		GetMenuBar()->FindItem(IDM_PLAY)->SetText(_("&Pause\tF10"));
	}
	else
	{
		if (m_ToolBar)
		{
			m_ToolBar->SetToolBitmap(IDM_PLAY, m_Bitmaps[Toolbar_Play]);
			m_ToolBar->SetToolShortHelp(IDM_PLAY, _("Play"));
			m_ToolBar->SetToolLabel(IDM_PLAY, wxT(" Play "));
		}
		GetMenuBar()->FindItem(IDM_PLAY)->SetText(_("&Play\tF10"));
		
	}

	if (!Initialized)
	{
		if (m_GameListCtrl)
		{
			if (!m_GameListCtrl->IsShown())
			{
				m_GameListCtrl->Reparent(m_Panel);
				m_GameListCtrl->Enable();
				m_GameListCtrl->Show();
				sizerPanel->FitInside(m_Panel);
			}
		}
	}
	else
	{
		if (m_GameListCtrl)
		{
			if (m_GameListCtrl->IsShown())
			{
				m_GameListCtrl->Disable();
				m_GameListCtrl->Hide();
			}
		}
	}

	if (m_ToolBar) m_ToolBar->Refresh();
	if (g_pCodeWindow) g_pCodeWindow->Update();

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
	
	if (m_GameListCtrl) m_GameListCtrl->Update();
}

// Enable and disable the toolbar
void CFrame::OnToggleToolbar(wxCommandEvent& event)
{
	SConfig::GetInstance().m_InterfaceToolbar = event.IsChecked();
	DoToggleToolbar(event.IsChecked());
}
void CFrame::DoToggleToolbar(bool _show)
{
	if (_show)
	{
		m_Mgr->GetPane(wxT("TBMain")).Show();
		if (g_pCodeWindow) { m_Mgr->GetPane(wxT("TBDebug")).Show(); m_Mgr->GetPane(wxT("TBAui")).Show(); }
		m_Mgr->Update();
	}
	else
	{
		m_Mgr->GetPane(wxT("TBMain")).Hide();
		if (g_pCodeWindow) { m_Mgr->GetPane(wxT("TBDebug")).Hide(); m_Mgr->GetPane(wxT("TBAui")).Hide(); }
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

