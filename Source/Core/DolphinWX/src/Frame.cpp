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

#include "Globals.h"
#include "Frame.h"
#include "FileUtil.h"

#include "GameListCtrl.h"
#include "BootManager.h"

#include "Common.h"
#include "Config.h"
#include "Core.h"
#include "PluginOptions.h"
#include "PluginManager.h"
#include "MemcardManager.h"

#include <wx/mstream.h>

// ----------------------------------------------------------------------------
// resources
// ----------------------------------------------------------------------------

extern "C" {
#include "../resources/Dolphin.c"
#include "../resources/toolbar_browse.c"
#include "../resources/toolbar_file_open.c"
#include "../resources/toolbar_fullscreen.c"
#include "../resources/toolbar_help.c"
#include "../resources/toolbar_pause.c"
#include "../resources/toolbar_play.c"
#include "../resources/toolbar_play_dis.c"
#include "../resources/toolbar_plugin_dsp.c"
#include "../resources/toolbar_plugin_gfx.c"
#include "../resources/toolbar_plugin_options.c"
#include "../resources/toolbar_plugin_options_dis.c"
#include "../resources/toolbar_plugin_pad.c"
#include "../resources/toolbar_refresh.c"
#include "../resources/toolbar_stop.c"
#include "../resources/toolbar_stop_dis.c"
};

#define wxGetBitmapFromMemory(name) _wxGetBitmapFromMemory(name, sizeof(name))
inline wxBitmap _wxGetBitmapFromMemory(const unsigned char* data, int length)
{
	wxMemoryInputStream is(data, length);
	return(wxBitmap(wxImage(is, wxBITMAP_TYPE_ANY, -1), -1));
}


// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

static const long TOOLBAR_STYLE = wxTB_FLAT | wxTB_DOCKABLE | wxTB_TEXT;

// ----------------------------------------------------------------------------
// event tables
// ----------------------------------------------------------------------------

// Notice that wxID_HELP will be processed for the 'About' menu and the toolbar
// help button.

const wxEventType wxEVT_HOST_COMMAND = wxNewEventType();

BEGIN_EVENT_TABLE(CFrame, wxFrame)
EVT_MENU(wxID_OPEN, CFrame::OnOpen)
EVT_MENU(wxID_EXIT, CFrame::OnQuit)
EVT_MENU(IDM_HELPWEBSITE, CFrame::OnHelp)
EVT_MENU(IDM_HELPGOOGLECODE, CFrame::OnHelp)
EVT_MENU(IDM_HELPABOUT, CFrame::OnHelp)
EVT_MENU(wxID_REFRESH, CFrame::OnRefresh)
EVT_MENU(IDM_PLAY, CFrame::OnPlay)
EVT_MENU(IDM_STOP, CFrame::OnStop)
EVT_MENU(IDM_PLUGIN_OPTIONS, CFrame::OnPluginOptions)
EVT_MENU(IDM_CONFIG_GFX_PLUGIN, CFrame::OnPluginGFX)
EVT_MENU(IDM_CONFIG_DSP_PLUGIN, CFrame::OnPluginDSP)
EVT_MENU(IDM_CONFIG_PAD_PLUGIN, CFrame::OnPluginPAD)
EVT_MENU(IDM_BROWSE, CFrame::OnBrowse)
EVT_MENU(IDM_MEMCARD, CFrame::OnMemcard)
EVT_MENU(IDM_TOGGLE_FULLSCREEN, CFrame::OnToggleFullscreen)
EVT_MENU(IDM_TOGGLE_DUALCORE, CFrame::OnToggleDualCore)
EVT_MENU(IDM_TOGGLE_TOOLBAR, CFrame::OnToggleToolbar)
EVT_MENU(IDM_LOADSTATE, CFrame::OnLoadState)
EVT_MENU(IDM_SAVESTATE, CFrame::OnSaveState)
EVT_HOST_COMMAND(wxID_ANY, CFrame::OnHostMessage)
END_EVENT_TABLE()

// ----------------------------------------------------------------------------
// implementation
// ----------------------------------------------------------------------------

CFrame::CFrame(wxFrame* parent,
		wxWindowID id,
		const wxString& title,
		const wxPoint& pos,
		const wxSize& size,
		long style)
	: wxFrame(parent, id, title, pos, size, style)
	, m_Panel(NULL)
	, m_pStatusBar(NULL)
	, m_pMenuBar(NULL)
	, m_pBootProcessDialog(NULL)
{
	InitBitmaps();

	// Give it an icon
	wxIcon IconTemp;
	IconTemp.CopyFromBitmap(wxGetBitmapFromMemory(dolphin_png));
	SetIcon(IconTemp);

	// Give it a status line
	m_pStatusBar = CreateStatusBar();
	CreateMenu();

	// this panel is the parent for rendering and it holds the gamelistctrl
	{
		m_Panel = new wxPanel(this);

		m_GameListCtrl = new CGameListCtrl(m_Panel, LIST_CTRL,
				wxDefaultPosition, wxDefaultSize,
				wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL | wxLC_SORT_ASCENDING);

		wxBoxSizer* sizerPanel = new wxBoxSizer(wxHORIZONTAL);
		sizerPanel->Add(m_GameListCtrl, 2, wxEXPAND | wxALL);
		m_Panel->SetSizer(sizerPanel);

		sizerPanel->SetSizeHints(m_Panel);
		sizerPanel->Fit(m_Panel);
	}

	// Create the toolbar
	RecreateToolbar();

	Show();

	CPluginManager::GetInstance().ScanForPlugins(this);

	m_GameListCtrl->Update();

	wxTheApp->Connect(wxID_ANY, wxEVT_KEY_DOWN,
			wxKeyEventHandler(CFrame::OnKeyDown),
			(wxObject*)0, this);

	UpdateGUI();
}


void CFrame::CreateMenu()
{
	delete m_pMenuBar;
	m_pMenuBar = new wxMenuBar(wxMB_DOCKABLE);

	// file menu
	wxMenu* fileMenu = new wxMenu;
	fileMenu->Append(wxID_OPEN, _T("&Open..."));
	fileMenu->Append(wxID_REFRESH, _T("&Refresh"));
	fileMenu->Append(IDM_BROWSE, _T("&Browse for ISOs..."));

	fileMenu->AppendSeparator();
	fileMenu->Append(wxID_EXIT, _T("E&xit"), _T(""));
	m_pMenuBar->Append(fileMenu, _T("&File"));

	// emulation menu
	wxMenu* emulationMenu = new wxMenu;
	m_pMenuItemPlay = new wxMenuItem(fileMenu, IDM_PLAY, _T("&Play"));
	emulationMenu->Append(m_pMenuItemPlay);
	m_pMenuItemStop = new wxMenuItem(fileMenu, IDM_STOP, _T("&Stop"));
	emulationMenu->Append(m_pMenuItemStop);
	emulationMenu->AppendSeparator();

	m_pMenuItemLoad = new wxMenuItem(fileMenu, IDM_LOADSTATE, _T("&Load State"));
	emulationMenu->Append(m_pMenuItemLoad);
	m_pMenuItemSave = new wxMenuItem(fileMenu, IDM_SAVESTATE, _T("Sa&ve State"));
	emulationMenu->Append(m_pMenuItemSave);
	m_pMenuBar->Append(emulationMenu, _T("&Emulation"));

	// options menu
	wxMenu* pOptionsMenu = new wxMenu;
	m_pPluginOptions = new wxMenuItem(pOptionsMenu, IDM_PLUGIN_OPTIONS, _T("&Select plugins"));
	pOptionsMenu->Append(m_pPluginOptions);
	pOptionsMenu->AppendSeparator();
	pOptionsMenu->Append(IDM_CONFIG_GFX_PLUGIN, _T("&GFX settings"));
	pOptionsMenu->Append(IDM_CONFIG_DSP_PLUGIN, _T("&DSP settings"));
	pOptionsMenu->Append(IDM_CONFIG_PAD_PLUGIN, _T("&PAD settings"));
	pOptionsMenu->AppendSeparator();
	pOptionsMenu->Append(IDM_TOGGLE_FULLSCREEN, _T("&Fullscreen"));
	pOptionsMenu->AppendCheckItem(IDM_TOGGLE_DUALCORE, _T("&Dual-core (unstable!)"));
	pOptionsMenu->Check(IDM_TOGGLE_DUALCORE, SConfig::GetInstance().m_LocalCoreStartupParameter.bUseDualCore);			
	m_pMenuBar->Append(pOptionsMenu, _T("&Options"));

	// misc menu
	wxMenu* miscMenu = new wxMenu;
	miscMenu->Append(IDM_MEMCARD, _T("&Memcard manager"));
	miscMenu->AppendCheckItem(IDM_TOGGLE_TOOLBAR, _T("&Enable toolbar"));
	miscMenu->Check(IDM_TOGGLE_TOOLBAR, true);
	m_pMenuBar->Append(miscMenu, _T("&Misc"));

	// help menu
	wxMenu* helpMenu = new wxMenu;
	/*helpMenu->Append(wxID_HELP, _T("&Help"));
	re-enable when there's something useful to display*/
	helpMenu->Append(IDM_HELPWEBSITE, _T("&Dolphin web site"));
	helpMenu->Append(IDM_HELPGOOGLECODE, _T("&Dolphin at Google Code"));
	helpMenu->AppendSeparator();
	helpMenu->Append(IDM_HELPABOUT, _T("&About..."));
	m_pMenuBar->Append(helpMenu, _T("&Help"));

	// Associate the menu bar with the frame
	SetMenuBar(m_pMenuBar);
}


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
	toolBar->SetToolDisabledBitmap(IDM_PLAY, m_Bitmaps[Toolbar_Play_Dis]);
	toolBar->AddTool(IDM_STOP, _T("Stop"),   m_Bitmaps[Toolbar_Stop], _T("Stop"));
	toolBar->SetToolDisabledBitmap(IDM_STOP, m_Bitmaps[Toolbar_Stop_Dis]);
	toolBar->AddTool(IDM_TOGGLE_FULLSCREEN, _T("Fullscr."),  m_Bitmaps[Toolbar_FullScreen], _T("Toggle Fullscreen"));
	toolBar->AddSeparator();
	toolBar->AddTool(IDM_PLUGIN_OPTIONS, _T("Plugins"), m_Bitmaps[Toolbar_PluginOptions], _T("Select plugins"));
	toolBar->SetToolDisabledBitmap(IDM_PLUGIN_OPTIONS, m_Bitmaps[Toolbar_PluginOptions_Dis]);
	toolBar->AddTool(IDM_CONFIG_GFX_PLUGIN, _T("GFX"),  m_Bitmaps[Toolbar_PluginGFX], _T("GFX settings"));
	toolBar->AddTool(IDM_CONFIG_DSP_PLUGIN, _T("DSP"),  m_Bitmaps[Toolbar_PluginDSP], _T("DSP settings"));
	toolBar->AddTool(IDM_CONFIG_PAD_PLUGIN, _T("PAD"),  m_Bitmaps[Toolbar_PluginPAD], _T("PAD settings"));
	toolBar->AddSeparator();
	toolBar->AddTool(IDM_HELPABOUT, _T("About"), m_Bitmaps[Toolbar_Help], _T("About Dolphin"));

	// after adding the buttons to the toolbar, must call Realize() to reflect
	// the changes
	toolBar->Realize();
}


void CFrame::RecreateToolbar()
{
	// delete and recreate the toolbar
	wxToolBarBase* toolBar = GetToolBar();
	long style = toolBar ? toolBar->GetWindowStyle() : TOOLBAR_STYLE;

	delete toolBar;
	SetToolBar(NULL);

	style &= ~(wxTB_HORIZONTAL | wxTB_VERTICAL | wxTB_BOTTOM | wxTB_RIGHT | wxTB_HORZ_LAYOUT | wxTB_TOP);
	wxToolBar* theToolBar = CreateToolBar(style, ID_TOOLBAR);

	PopulateToolbar(theToolBar);
	SetToolBar(theToolBar);
}


void CFrame::InitBitmaps()
{
	// load orignal size 48x48
	m_Bitmaps[Toolbar_FileOpen] = wxGetBitmapFromMemory(toolbar_file_open_png);
	m_Bitmaps[Toolbar_Refresh] = wxGetBitmapFromMemory(toolbar_refresh_png);
	m_Bitmaps[Toolbar_Browse] = wxGetBitmapFromMemory(toolbar_browse_png);
	m_Bitmaps[Toolbar_Play] = wxGetBitmapFromMemory(toolbar_play_png);
	m_Bitmaps[Toolbar_Play_Dis] = wxGetBitmapFromMemory(toolbar_play_dis_png);
	m_Bitmaps[Toolbar_Stop] = wxGetBitmapFromMemory(toolbar_stop_png);
	m_Bitmaps[Toolbar_Stop_Dis] = wxGetBitmapFromMemory(toolbar_stop_dis_png);
	m_Bitmaps[Toolbar_Pause] = wxGetBitmapFromMemory(toolbar_pause_png);
	m_Bitmaps[Toolbar_PluginOptions] = wxGetBitmapFromMemory(toolbar_plugin_options_png);
	m_Bitmaps[Toolbar_PluginOptions_Dis] = wxGetBitmapFromMemory(toolbar_plugin_options_dis_png);
	m_Bitmaps[Toolbar_PluginGFX]  = wxGetBitmapFromMemory(toolbar_plugin_gfx_png);
	m_Bitmaps[Toolbar_PluginDSP]  = wxGetBitmapFromMemory(toolbar_plugin_dsp_png);
	m_Bitmaps[Toolbar_PluginPAD]  = wxGetBitmapFromMemory(toolbar_plugin_pad_png);
	m_Bitmaps[Toolbar_FullScreen] = wxGetBitmapFromMemory(toolbar_fullscreen_png);
	m_Bitmaps[Toolbar_Help] = wxGetBitmapFromMemory(toolbar_help_png);

	// scale to 24x24 for toolbar
	for (size_t n = Toolbar_FileOpen; n < WXSIZEOF(m_Bitmaps); n++)
	{
		m_Bitmaps[n] = wxBitmap(m_Bitmaps[n].ConvertToImage().Scale(24, 24));
	}
}


void CFrame::OnOpen(wxCommandEvent& WXUNUSED (event))
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
		return;
	wxString path = wxFileSelector(
			_T("Select the file to load"),
			wxEmptyString, wxEmptyString, wxEmptyString,
			wxString::Format
			(
					_T("All GC/Wii files (elf, dol, gcm, iso)|*.elf;*.dol;*.gcm;*.iso|All files (%s)|%s"),
					wxFileSelectorDefaultWildcardStr,
					wxFileSelectorDefaultWildcardStr
			),
			wxFD_OPEN | wxFD_PREVIEW | wxFD_FILE_MUST_EXIST,
			this);
	if (!path)
	{
		return;
	}
	BootManager::BootCore(std::string(path.ToAscii()));
}


void CFrame::OnQuit(wxCommandEvent& WXUNUSED (event))
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		Core::Stop();
		UpdateGUI();
	}

	Close(true);
}


void CFrame::OnHelp(wxCommandEvent& event)
{
	switch (event.GetId()) {
	case IDM_HELPABOUT:
		{
		wxAboutDialogInfo info;
		info.AddDeveloper(_T("ector"));
		info.AddDeveloper(_T("F|RES"));
		info.AddDeveloper(_T("yaz0r"));
		info.AddDeveloper(_T("zerofrog"));
	/*	info.SetLicence(wxString::FromAscii(
		"Dolphin Licence 1.0"
		"#include GPL.TXT"));
	 */

		info.AddArtist(_T("miloszwl@miloszwl.com (miloszwl.deviantart.com)"));

		wxAboutBox(info);
		break;
		}
	case IDM_HELPWEBSITE:
		File::Launch("http://www.dolphin-emu.com/");
		break;
	case IDM_HELPGOOGLECODE:
		File::Launch("http://code.google.com/p/dolphin-emu/");
		break;
	}
}


void CFrame::OnPlay(wxCommandEvent& WXUNUSED (event))
{
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
}


void CFrame::OnStop(wxCommandEvent& WXUNUSED (event))
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		Core::Stop();
		UpdateGUI();
	}
}


void CFrame::OnRefresh(wxCommandEvent& WXUNUSED (event))
{
	if (m_GameListCtrl)
	{
		m_GameListCtrl->Update();
	}
}


void CFrame::OnPluginOptions(wxCommandEvent& WXUNUSED (event))
{
	CPluginOptions PluginOptions(this);
	PluginOptions.ShowModal();
}


void CFrame::OnPluginGFX(wxCommandEvent& WXUNUSED (event))
{
	CPluginManager::GetInstance().OpenConfig(
			GetHandle(),
			SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoPlugin
			);
}


void CFrame::OnPluginDSP(wxCommandEvent& WXUNUSED (event))
{
	CPluginManager::GetInstance().OpenConfig(
			GetHandle(),
			SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDSPPlugin
			);
}


void CFrame::OnPluginPAD(wxCommandEvent& WXUNUSED (event))
{
	CPluginManager::GetInstance().OpenConfig(
			GetHandle(),
			SConfig::GetInstance().m_LocalCoreStartupParameter.m_strPadPlugin
			);
}

void CFrame::OnBrowse(wxCommandEvent& WXUNUSED (event))
{
	m_GameListCtrl->BrowseForDirectory();
}

void CFrame::OnMemcard(wxCommandEvent& WXUNUSED (event))
{
	CMemcardManager MemcardManager(this);
	MemcardManager.ShowModal();
}

void CFrame::OnHostMessage(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	    case IDM_UPDATEGUI:
		    UpdateGUI();
		    break;

	    case IDM_BOOTING_STARTED:
		    if (m_pBootProcessDialog == NULL)
		    {
			    /*		m_pBootProcessDialog = new wxProgressDialog
			       	    (_T("Booting the core"),
			       	    _T("Booting..."),
			       	    1,    // range
			       	    this,
			       	    wxPD_APP_MODAL |
			       	    // wxPD_AUTO_HIDE | -- try this as well
			       	    wxPD_ELAPSED_TIME |
			       	    wxPD_SMOOTH // - makes indeterminate mode bar on WinXP very small
			       	    );*/

			    m_pBootProcessDialog = new wxBusyInfo(wxString::FromAscii("Booting..."), this);
		    }
		    break;

	    case IDM_BOOTING_ENDED:
		    if (m_pBootProcessDialog != NULL)
		    {
			    // m_pBootProcessDialog->Destroy();
			    delete m_pBootProcessDialog;
			    m_pBootProcessDialog = NULL;
		    }
		    break;

		case IDM_UPDATESTATUSBAR:
			if (m_pStatusBar != NULL)
			{
				m_pStatusBar->SetStatusText(event.GetString());
			}
			break;
	}
}

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

void CFrame::OnLoadState(wxCommandEvent& WXUNUSED (event))
{
	Core::LoadState();
}

void CFrame::OnSaveState(wxCommandEvent& WXUNUSED (event))
{
	Core::SaveState();
}

void CFrame::OnToggleToolbar(wxCommandEvent& event)
{
	wxToolBarBase* toolBar = GetToolBar();
	
	if (event.IsChecked())
	{
		CFrame::RecreateToolbar();
	}
	else
	{
		delete toolBar;
		SetToolBar(NULL);
	}
}

void CFrame::OnKeyDown(wxKeyEvent& event)
{
	if (((event.GetKeyCode() == WXK_RETURN) && (event.GetModifiers() == wxMOD_ALT)) ||
	    (event.GetKeyCode() == WXK_ESCAPE))
	{
		ShowFullScreen(!IsFullScreen());
		UpdateGUI();
	}
	else
	{
		event.Skip();
	}
}

void CFrame::UpdateGUI()
{
	// buttons
	{
		if (Core::GetState() == Core::CORE_UNINITIALIZED)
		{
			GetToolBar()->EnableTool(IDM_PLUGIN_OPTIONS, true);
			m_pPluginOptions->Enable(true);

			GetToolBar()->EnableTool(IDM_STOP, false);
			GetToolBar()->EnableTool(IDM_PLAY, false);

			m_pMenuItemPlay->Enable(false);
			m_pMenuItemStop->Enable(false);
			m_pMenuItemLoad->Enable(false);
			m_pMenuItemSave->Enable(false);
		}
		else
		{
			GetToolBar()->EnableTool(IDM_PLUGIN_OPTIONS, false);
			m_pPluginOptions->Enable(false);

			GetToolBar()->EnableTool(IDM_STOP, true);
			GetToolBar()->EnableTool(IDM_PLAY, true);

			m_pMenuItemPlay->Enable(true);
			m_pMenuItemStop->Enable(true);
			m_pMenuItemLoad->Enable(true);
			m_pMenuItemSave->Enable(true);

			if (Core::GetState() == Core::CORE_RUN)
			{
				GetToolBar()->SetToolNormalBitmap(IDM_PLAY, m_Bitmaps[Toolbar_Pause]);
				GetToolBar()->SetToolShortHelp(IDM_PLAY, _T("Pause"));

				m_pMenuItemPlay->SetText(_T("Pause"));
			}
			else
			{
				GetToolBar()->SetToolNormalBitmap(IDM_PLAY, m_Bitmaps[Toolbar_Play]);
				GetToolBar()->SetToolShortHelp(IDM_PLAY, _T("Play"));

				m_pMenuItemPlay->SetText(_T("Play"));
			}
		}
	}

	// gamelistctrl
	{
		if (Core::GetState() == Core::CORE_UNINITIALIZED)
		{
			if (m_GameListCtrl && !m_GameListCtrl->IsShown())
			{
				m_GameListCtrl->Enable();
				m_GameListCtrl->Show();
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
	}
}
