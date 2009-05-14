// Copyright (C) 2003-2008 Dolphin Project.

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



// Windows

/*
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

#if defined(HAVE_SFML) && HAVE_SFML || defined(_WIN32)
#include "NetWindow.h"
#endif

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

#include "Common.h" // Common
#include "FileUtil.h"
#include "Timer.h"
#include "Setup.h"

#include "ConfigManager.h" // Core
#include "Core.h"
#include "HW/DVDInterface.h"
#include "State.h"
#include "VolumeHandler.h"
#include "NANDContentLoader.h"

#include <wx/datetime.h> // wxWidgets

// ugly that this lib included code from the main
#include "../../DolphinWX/Src/WxUtils.h"

// ----------------------------------------------------------------------------
// Resources
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

// Constants
static const long TOOLBAR_STYLE = wxTB_FLAT | wxTB_DOCKABLE | wxTB_TEXT;

// Other Windows
wxCheatsWindow* CheatsWindow;
wxInfoWindow* InfoWindow;

// Create menu items
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
	fileMenu->Append(wxID_EXIT, _T("E&xit"), _T("Alt+F4"));
	menuBar->Append(fileMenu, _T("&File"));

	// Emulation menu
	wxMenu* emulationMenu = new wxMenu;
	emulationMenu->Append(IDM_PLAY, _T("&Play"));
	emulationMenu->Append(IDM_CHANGEDISC, _T("Change Disc"));
	emulationMenu->Append(IDM_STOP, _T("&Stop"));
	emulationMenu->AppendSeparator();
	wxMenu *saveMenu = new wxMenu;
	wxMenu *loadMenu = new wxMenu;
	m_pSubMenuLoad = emulationMenu->AppendSubMenu(saveMenu, _T("&Load State"));
	m_pSubMenuSave = emulationMenu->AppendSubMenu(loadMenu, _T("Sa&ve State"));
	for (int i = 1; i < 10; i++) {
		saveMenu->Append(IDM_LOADSLOT1 + i - 1, wxString::Format(_T("Slot %i\tF%i"), i, i));
		loadMenu->Append(IDM_SAVESLOT1 + i - 1, wxString::Format(_T("Slot %i\tShift+F%i"), i, i));
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

	toolsMenu->Append(IDM_NETPLAY, _T("Start &NetPlay"));

	// toolsMenu->Append(IDM_SDCARD, _T("Mount &SDCard")); // Disable for now

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
	viewMenu->AppendCheckItem(IDM_TOGGLE_LOGWINDOW, _T("Show &Logwindow"));
	viewMenu->Check(IDM_TOGGLE_LOGWINDOW, m_bLogWindow);
	viewMenu->AppendCheckItem(IDM_TOGGLE_CONSOLE, _T("Show &Console"));
	viewMenu->Check(IDM_TOGGLE_CONSOLE, SConfig::GetInstance().m_InterfaceConsole);
	viewMenu->AppendSeparator();

	viewMenu->AppendCheckItem(IDM_LISTWII, _T("Show Wii"));
	viewMenu->Check(IDM_LISTWII, SConfig::GetInstance().m_ListWii);
	viewMenu->AppendCheckItem(IDM_LISTGC, _T("Show GameCube"));
	viewMenu->Check(IDM_LISTGC, SConfig::GetInstance().m_ListGC);
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

	// Associate the menu bar with the frame
	SetMenuBar(menuBar);
}


// Create toolbar items
void CFrame::PopulateToolbar(wxToolBar* toolBar)
{
	int w = m_Bitmaps[Toolbar_FileOpen].GetWidth(),
	    h = m_Bitmaps[Toolbar_FileOpen].GetHeight();

	toolBar->SetToolBitmapSize(wxSize(w, h));
	toolBar->AddTool(wxID_OPEN,    _T("Open"),    m_Bitmaps[Toolbar_FileOpen], _T("Open file..."));
	toolBar->AddTool(wxID_REFRESH, _T("Refresh"), m_Bitmaps[Toolbar_Refresh], _T("Refresh"));
	toolBar->AddTool(IDM_BROWSE, _T("Browse"),   m_Bitmaps[Toolbar_Browse], _T("Browse for an ISO directory..."));
	toolBar->AddSeparator();

	toolBar->AddTool(IDM_PLAY, _T("Play"),   m_Bitmaps[Toolbar_Play], _T("Play"));


	toolBar->AddTool(IDM_STOP, _T("Stop"),   m_Bitmaps[Toolbar_Stop], _T("Stop"));
#ifdef _WIN32
	toolBar->AddTool(IDM_TOGGLE_FULLSCREEN, _T("Fullscr."),  m_Bitmaps[Toolbar_FullScreen], _T("Toggle Fullscreen"));
#endif
	toolBar->AddTool(IDM_SCREENSHOT, _T("Scr.Shot"),   m_Bitmaps[Toolbar_FullScreen], _T("Take Screenshot"));
	toolBar->AddSeparator();
	toolBar->AddTool(IDM_CONFIG_MAIN, _T("Config"), m_Bitmaps[Toolbar_PluginOptions], _T("Configure..."));
	toolBar->AddTool(IDM_CONFIG_GFX_PLUGIN, _T("Gfx"),  m_Bitmaps[Toolbar_PluginGFX], _T("Graphics settings"));
	toolBar->AddTool(IDM_CONFIG_DSP_PLUGIN, _T("DSP"),  m_Bitmaps[Toolbar_PluginDSP], _T("DSP settings"));
	toolBar->AddTool(IDM_CONFIG_PAD_PLUGIN, _T("Pad"),  m_Bitmaps[Toolbar_PluginPAD], _T("Pad settings"));
	toolBar->AddTool(IDM_CONFIG_WIIMOTE_PLUGIN, _T("Wiimote"),  m_Bitmaps[Toolbar_Wiimote], _T("Wiimote settings"));
	toolBar->AddSeparator();
	toolBar->AddTool(IDM_HELPABOUT, _T("About"), m_Bitmaps[Toolbar_Help], _T("About Dolphin"));

	// after adding the buttons to the toolbar, must call Realize() to reflect
	// the changes
	toolBar->Realize();
}


// Delete and recreate the toolbar
void CFrame::RecreateToolbar()
{

	wxToolBarBase* toolBar = GetToolBar();
	long style = toolBar ? toolBar->GetWindowStyle() : TOOLBAR_STYLE;

	delete toolBar;
	SetToolBar(NULL);

	style &= ~(wxTB_HORIZONTAL | wxTB_VERTICAL | wxTB_BOTTOM | wxTB_RIGHT | wxTB_HORZ_LAYOUT | wxTB_TOP);
	TheToolBar = CreateToolBar(style, ID_TOOLBAR);

	PopulateToolbar(TheToolBar);
	SetToolBar(TheToolBar);
	UpdateGUI();
}
void CFrame::InitBitmaps()
{
	// Get selected theme
	int Theme = SConfig::GetInstance().m_LocalCoreStartupParameter.iTheme;

	/* Save memory by only having one set of bitmaps loaded at any time. I mean, they are still
	   in the exe, which is in memory, but at least we wont make another copy of all of them. */
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
	if (GetToolBar() != NULL)
		RecreateToolbar();
}


//////////////////////////////////////////////////////////////////////////////////////
// Start the game or change the disc
// -------------
void CFrame::BootGame()
{
	// Rerecording
	#ifdef RERECORDING
		Core::RerecordingStart();
	#endif

	// shuffle2: wxBusyInfo is meant to be created on the stack
	// and only stay around for the life of the scope it's in.
	// If that is not what we want, find another solution. I don't
	// think such a dialog is needed anyways, so maybe kill it?
	wxBusyInfo bootingDialog(wxString::FromAscii("Booting..."), this);

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
	// Start the selected ISO
	else if (m_GameListCtrl->GetSelectedISO() != 0)
	{
		BootManager::BootCore(m_GameListCtrl->GetSelectedISO()->GetFileName());
	}
	/* Start the default ISO, or if we don't have a default ISO, start the last
	started ISO */
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

// Open file to boot or for changing disc
void CFrame::OnOpen(wxCommandEvent& WXUNUSED (event))
{
	// Don't allow this for an initialized core
	//if (Core::GetState() != Core::CORE_UNINITIALIZED)
	//	return;

	DoOpen(true);
}

void CFrame::DoOpen(bool Boot)
{
    std::string currentDir = File::GetCurrentDirectory();

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
	if (!path)
	{
		return;
	}

    std::string currentDir2 = File::GetCurrentDirectory();

    if (currentDir != currentDir2)
    {
        PanicAlert("Current dir changed has been changeg from %s to %s after wxFileSelector!",currentDir.c_str(),currentDir2.c_str());
        File::SetCurrentDirectory(currentDir.c_str());
    }


	// Should we boot a new game or just change the disc?
	if(Boot)
	{
		BootManager::BootCore(std::string(path.ToAscii()));
	}
	else
	{
		// Get the current ISO name
		std::string OldName = SConfig::GetInstance().m_LocalCoreStartupParameter.m_strFilename;

		// Change the iso and make sure it's a valid file
		if(!VolumeHandler::SetVolumeName(std::string(path.ToAscii())))
		{
			PanicAlert("The file you selected is not a valid ISO file. Please try again.");

			// Put back the old one
			VolumeHandler::SetVolumeName(OldName);
		}
		// Yes it is a valid ISO file
		else
		{
			// Save the new ISO file name
			SConfig::GetInstance().m_LocalCoreStartupParameter.m_strFilename = std::string(path.ToAscii());
		}
	}
}

void CFrame::OnChangeDisc(wxCommandEvent& WXUNUSED (event))
{
	DVDInterface::SetLidOpen(true);
	DoOpen(false);
	DVDInterface::SetLidOpen(false);
	DVDInterface::SetDiscInside(true);
}

void CFrame::OnPlay(wxCommandEvent& WXUNUSED (event))
{
	BootGame();
}

void CFrame::OnBootDrive(wxCommandEvent& event)
{
	BootManager::BootCore(drives[event.GetId()-IDM_DRIVE1]);
}

////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////
// Refresh the file list and browse for a favorites directory
// -------------
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

static inline void GenerateScreenshotName(std::string& name)
{
	int index = 1;
	std::string tempname;
	tempname = FULL_SCREENSHOTS_DIR;
	tempname += Core::GetStartupParameter().GetUniqueID();

	name = StringFromFormat("%s-%d.bmp", tempname.c_str(), index);

	while(File::Exists(name.c_str()))
		name = StringFromFormat("%s-%d.bmp", tempname.c_str(), ++index);
}

void CFrame::OnScreenshot(wxCommandEvent& WXUNUSED (event))
{
	std::string name;
	GenerateScreenshotName(name);
	bool bPaused = (Core::GetState() == Core::CORE_PAUSE);

    Core::SetState(Core::CORE_PAUSE);
    CPluginManager::GetInstance().GetVideo()->Video_Screenshot(name.c_str());
	if(!bPaused)
		Core::SetState(Core::CORE_RUN);
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
	
		Core::Stop();

		/* This is needed together with the option to not "delete g_EmuThread" in Core.cpp, because then
		   we have to wait a moment before GetState() == CORE_UNINITIALIZED so that UpdateGUI() works.
		   It's also needed when a WaitForSingleObject() has timed out so that the ShutDown() process
		   has continued without all threads having come to a rest. It's not compatible with the
		   SETUP_TIMER_WAITING option (because the Stop returns before it's finished).
		   
		   Without this we just see the gray CPanel background because the ISO list will not be displayed.
		   */
		#ifndef SETUP_TIMER_WAITING
			if (bRenderToMain)
				while(Core::GetState() != Core::CORE_UNINITIALIZED) SLEEP(10);
		#endif
		
		UpdateGUI();
	}
}

void CFrame::OnStop(wxCommandEvent& WXUNUSED (event))
{
	DoStop();
}
////////////////////////////////////////////////////


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

// NetPlay stuff
void CFrame::OnNetPlay(wxCommandEvent& WXUNUSED (event))
{
#if defined(HAVE_SFML) && HAVE_SFML

	new NetPlay(this, m_GameListCtrl->GetGamePaths(), m_GameListCtrl->GetGameNames());
#endif
}


// Miscellaneous menu
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

/* Toogle fullscreen. In Windows the fullscreen mode is accomplished by expanding the m_Panel to cover
   the entire screen (when we render to the main window). */
void CFrame::OnToggleFullscreen(wxCommandEvent& WXUNUSED (event))
{
	ShowFullScreen(true);
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

void CFrame::OnLoadState(wxCommandEvent& event)
{
	int id = event.GetId();
	int slot = id - IDM_LOADSLOT1 + 1;
	State_Load(slot);
}

void CFrame::OnResize(wxSizeEvent& event)
{
	DoMoveIcons();  // In FrameWiimote.cpp
	event.Skip();
}

void CFrame::OnSaveState(wxCommandEvent& event)
{
	int id = event.GetId();
	int slot = id - IDM_SAVESLOT1 + 1;
	State_Save(slot);
}


// Enable and disable the toolbar
void CFrame::OnToggleToolbar(wxCommandEvent& event)
{
	wxToolBarBase* toolBar = GetToolBar();
	SConfig::GetInstance().m_InterfaceToolbar = event.IsChecked();
	if (SConfig::GetInstance().m_InterfaceToolbar == true)
	{
		CFrame::RecreateToolbar();
	}
	else
	{
		delete toolBar;
		SetToolBar(NULL);
	}

	this->SendSizeEvent();
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
	ToggleLogWindow(event.IsChecked());
}

void CFrame::ToggleLogWindow(bool check)
{
	SConfig::GetInstance().m_InterfaceLogWindow = check;
	if (SConfig::GetInstance().m_InterfaceLogWindow)
		m_LogWindow->Show();
	else
		m_LogWindow->Hide();

	// Make sure the check is updated (if wxw isn't calling this func)
	GetMenuBar()->FindItem(IDM_TOGGLE_LOGWINDOW)->Check(check);
}

// Enable and disable the console
void CFrame::OnToggleConsole(wxCommandEvent& event)
{
	ToggleConsole(event.IsChecked());
}

void CFrame::ToggleConsole(bool check)
{
	ConsoleListener *console = LogManager::GetInstance()->getConsoleListener();
	SConfig::GetInstance().m_InterfaceConsole = check;
	if (SConfig::GetInstance().m_InterfaceConsole)
		console->Open();
	else
		console->Close();

	// Make sure the check is updated (if wxw isn't calling this func)
	GetMenuBar()->FindItem(IDM_TOGGLE_CONSOLE)->Check(check);
}

// Update the enabled/disabled status
void CFrame::UpdateGUI()
{
	// Save status
	bool initialized = Core::GetState() != Core::CORE_UNINITIALIZED;
	bool running = Core::GetState() == Core::CORE_RUN;
	bool paused = Core::GetState() == Core::CORE_PAUSE;

	// Make sure that we have a toolbar
	if (GetToolBar() != NULL)
	{
		// Enable/disable the Config and Stop buttons
		//GetToolBar()->EnableTool(IDM_CONFIG_MAIN, !initialized);
		GetToolBar()->EnableTool(wxID_OPEN, !initialized);
		GetToolBar()->EnableTool(wxID_REFRESH, !initialized); // Don't allow refresh when we don't show the list
		GetToolBar()->EnableTool(IDM_STOP, running || paused);
		GetToolBar()->EnableTool(IDM_SCREENSHOT, running || paused);
	}

	// File
	GetMenuBar()->FindItem(wxID_OPEN)->Enable(!initialized);
	m_pSubMenuDrive->Enable(!initialized);
	GetMenuBar()->FindItem(wxID_REFRESH)->Enable(!initialized);
	GetMenuBar()->FindItem(IDM_BROWSE)->Enable(!initialized);

	// Emulation
	GetMenuBar()->FindItem(IDM_STOP)->Enable(running || paused);
	m_pSubMenuLoad->Enable(initialized);
	m_pSubMenuSave->Enable(initialized);

	// Misc
	GetMenuBar()->FindItem(IDM_CHANGEDISC)->Enable(initialized);
	if (DiscIO::CNANDContentManager::Access().GetNANDLoader(FULL_WII_MENU_DIR).IsValid())
		GetMenuBar()->FindItem(IDM_LOAD_WII_MENU)->Enable(!initialized);

	if (running)
	{
		if (GetToolBar() != NULL)
		{
			GetToolBar()->FindById(IDM_PLAY)->SetNormalBitmap(m_Bitmaps[Toolbar_Pause]);
			GetToolBar()->FindById(IDM_PLAY)->SetShortHelp(_("Pause"));
			GetToolBar()->FindById(IDM_PLAY)->SetLabel(_("Pause"));
		}
		GetMenuBar()->FindItem(IDM_PLAY)->SetText(_("&Pause"));
		
	}
	else
	{
		if (GetToolBar() != NULL)
		{
			GetToolBar()->FindById(IDM_PLAY)->SetNormalBitmap(m_Bitmaps[Toolbar_Play]);
			GetToolBar()->FindById(IDM_PLAY)->SetShortHelp(_("Play"));
			GetToolBar()->FindById(IDM_PLAY)->SetLabel(_("Play"));
		}
		GetMenuBar()->FindItem(IDM_PLAY)->SetText(_("&Play"));
		
	}
	if (GetToolBar() != NULL)
		GetToolBar()->Realize();


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

	TheToolBar->Realize();
	FitInside();
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
	}
	
	if (m_GameListCtrl)
	{
		m_GameListCtrl->Update();
	}
}

