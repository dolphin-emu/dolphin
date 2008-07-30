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


#include "ConfigDlg.h"
#include "../Globals.h"

BEGIN_EVENT_TABLE(ConfigDialog,wxDialog)
	EVT_CLOSE(ConfigDialog::OnClose)
	EVT_ACTIVATE(ConfigDialog::ConfigDialogActivate)
	EVT_BUTTON(ID_CLOSE,ConfigDialog::OKClick)
	EVT_BUTTON(ID_APPLY,ConfigDialog::OKClick)
	EVT_BUTTON(ID_OK,ConfigDialog::OKClick)
	EVT_BUTTON(ID_BROWSE,ConfigDialog::BrowseClick)
END_EVENT_TABLE()


ConfigDialog::ConfigDialog(wxWindow *parent, wxWindowID id, const wxString &title, const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
	g_Config.Load();

	CreateGUIControls();
}

ConfigDialog::~ConfigDialog()
{
} 

void ConfigDialog::CreateGUIControls()
{
	// buttons
	m_OK = new wxButton(this, ID_OK, wxT("OK"), wxPoint(404,208), wxSize(73,25), 0, wxDefaultValidator, wxT("OK"));
	m_Apply = new wxButton(this, ID_APPLY, wxT("Apply"), wxPoint(324,208), wxSize(73,25), 0, wxDefaultValidator, wxT("Apply"));
	m_Close = new wxButton(this, ID_CLOSE, wxT("Close"), wxPoint(244,208), wxSize(73,25), 0, wxDefaultValidator, wxT("Close"));

	// notebook
	m_Notebook = new wxNotebook(this, ID_PAGEENHANCEMENTS, wxPoint(0,0),wxSize(484,198));
	m_PageVideo = new wxPanel(m_Notebook, ID_PAGEVIDEO, wxPoint(4,24), wxSize(476,170));
	m_Notebook->AddPage(m_PageVideo, wxT("Video"));

	m_PageEnhancements = new wxPanel(m_Notebook, ID_WXNOTEBOOKPAGE3, wxPoint(4,24), wxSize(476,170));
	m_Notebook->AddPage(m_PageEnhancements, wxT("Enhancements"));

	m_PageAdvanced = new wxPanel(m_Notebook, ID_PAGEADVANCED, wxPoint(4,24), wxSize(476,170));
	m_Notebook->AddPage(m_PageAdvanced, wxT("Advanced"));

	m_Fullscreen = new wxCheckBox(m_PageVideo, ID_FULLSCREEN, wxT("Fullscreen"), wxPoint(12,16), wxSize(137,25), 0, wxDefaultValidator, wxT("Fullscreen"));
	m_Fullscreen->SetValue(g_Config.bFullscreen);

	// page1
	m_RenderToMainWindow = new wxCheckBox(m_PageVideo, ID_RENDERTOMAINWINDOW, wxT("Render to Main Window"), wxPoint(12,40), wxSize(137,25), 0, wxDefaultValidator, wxT("RenderToMainWindow"));
	m_RenderToMainWindow->SetValue(g_Config.renderToMainframe);

	// all other options doesnt to anything so lets disable them
	wxArrayString arrayStringFor_FullscreenCB;
	m_FullscreenCB = new wxComboBox(m_PageVideo, ID_FULLSCREENCB, wxT(""), wxPoint(132,72), wxSize(121,21), arrayStringFor_FullscreenCB, 0, wxDefaultValidator, wxT("FullscreenCB"));
	m_FullscreenCB->Enable(true);

	wxArrayString arrayStringFor_WindowResolutionCB;
	m_WindowResolutionCB = new wxComboBox(m_PageVideo, ID_WINDOWRESOLUTIONCB, wxT(""), wxPoint(132,104), wxSize(121,21), arrayStringFor_WindowResolutionCB, 0, wxDefaultValidator, wxT("WindowResolutionCB"));
	m_WindowResolutionCB->Enable(true);

	wxStaticText *WxStaticText1 = new wxStaticText(m_PageVideo, ID_WXSTATICTEXT1, wxT("Windowed Resolution:"), wxPoint(12,104), wxDefaultSize, 0, wxT("WxStaticText1"));
	wxStaticText *WxStaticText2 = new wxStaticText(m_PageVideo, ID_WXSTATICTEXT2, wxT("Fullscreen Video Mode:"), wxPoint(12,72), wxDefaultSize, 0, wxT("WxStaticText2"));

	wxArrayString arrayStringFor_AliasModeCB;
	m_AliasModeCB = new wxComboBox(m_PageVideo, ID_ALIASMODECB, wxT("WxComboBox1"), wxPoint(132,136), wxSize(121,21), arrayStringFor_AliasModeCB, 0, wxDefaultValidator, wxT("AliasModeCB"));
	m_AliasModeCB->Enable(false);

	wxStaticText *WxStaticText3 = new wxStaticText(m_PageVideo, ID_WXSTATICTEXT3, wxT("Alias Mode:"), wxPoint(12,136), wxDefaultSize, 0, wxT("WxStaticText3"));

	m_ForceFiltering = new wxCheckBox(m_PageEnhancements, ID_FORCEFILTERING, wxT("Force bi/trlinear (May cause small glitches)"), wxPoint(12,16), wxSize(233,25), 0, wxDefaultValidator, wxT("ForceFiltering"));
	m_ForceFiltering->Enable(false);

	m_ForceAnsitropy = new wxCheckBox(m_PageEnhancements, ID_FORCEANSITROPY, wxT("Force maximum ansitropy filtering"), wxPoint(12,48), wxSize(233,25), 0, wxDefaultValidator, wxT("ForceAnsitropy"));
	m_ForceAnsitropy->Enable(false);

	m_Wireframe = new wxCheckBox(m_PageAdvanced, ID_WIREFRAME, wxT("Wireframe"), wxPoint(12,16), wxSize(233,25), 0, wxDefaultValidator, wxT("Wireframe"));
	m_Wireframe->Enable(false);

	m_DumpTextures = new wxCheckBox(m_PageAdvanced, ID_DUMPTEXTURES, wxT("Dump texture to:"), wxPoint(12,88), wxSize(233,25), 0, wxDefaultValidator, wxT("DumpTextures"));
	m_DumpTextures->Enable(false);

	m_Statistics = new wxCheckBox(m_PageAdvanced, ID_STATISTICS, wxT("Overlay some statistics"), wxPoint(12,40), wxSize(233,25), 0, wxDefaultValidator, wxT("Statistics"));
	m_Statistics->SetValue(g_Config.bOverlayStats);
	m_Statistics->Enable(true);

	m_ShaderErrors = new wxCheckBox(m_PageAdvanced, ID_SHADERERRORS, wxT("Show shader compilation issues"), wxPoint(12,64), wxSize(233,25), 0, wxDefaultValidator, wxT("ShaderErrors"));
	m_ShaderErrors->Enable(false);

	m_Browse = new wxButton(m_PageAdvanced, ID_BROWSE, wxT("Browse"), wxPoint(156,136), wxSize(65,25), 0, wxDefaultValidator, wxT("Browse"));
	m_Browse->Enable(false);

	m_TexturePath = new wxTextCtrl(m_PageAdvanced, ID_TEXTUREPATH, wxT("TexturePath"), wxPoint(20,136), wxSize(129,25), 0, wxDefaultValidator, wxT("TexturePath"));
	m_TexturePath->Enable(false);

	SetTitle(wxT("Opengl Plugin Configuration"));
	SetIcon(wxNullIcon);
	SetSize(8,8,492,273);
	Center();
	m_Fullscreen->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDialog::FullScreenCheck ), NULL, this );
	m_RenderToMainWindow->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDialog::RenderMainCheck ), NULL, this );
	m_Statistics->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDialog::OverlayCheck ), NULL, this );
	m_FullscreenCB->Connect( wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler( ConfigDialog::FSCB ), NULL, this );
	m_WindowResolutionCB->Connect( wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler( ConfigDialog::WMCB ), NULL, this );
}

void ConfigDialog::OnClose(wxCloseEvent& /*event*/)
{
	g_Config.Save();
	EndModal(0);
}

void ConfigDialog::ConfigDialogActivate(wxActivateEvent& event)
{
	// init dialog elements from config
}

void ConfigDialog::BrowseClick(wxCommandEvent& event)
{
	// browse for folder
}

void ConfigDialog::OKClick(wxCommandEvent& event)
{
	if ((event.GetId() == ID_APPLY) ||
		(event.GetId() == ID_OK))
	{
		g_Config.renderToMainframe = m_RenderToMainWindow->GetValue();
		g_Config.bFullscreen = m_Fullscreen->GetValue();
		g_Config.Save();
	}

	if ((event.GetId() == ID_CLOSE) ||
		(event.GetId() == ID_OK))
	{
		Close(); 
	}
}
void ConfigDialog::AddFSReso(char *reso)
{
	m_FullscreenCB->Append(wxString::FromAscii(reso));
}
void ConfigDialog::AddWindowReso(char *reso)
{
	m_WindowResolutionCB->Append(wxString::FromAscii(reso));
}
void ConfigDialog::FullScreenCheck(wxCommandEvent& event)
{
	g_Config.bFullscreen = m_Fullscreen->IsChecked();
}
void ConfigDialog::FSCB(wxCommandEvent& event)
{
	strcpy(g_Config.iFSResolution, m_FullscreenCB->GetValue().mb_str() );
}
void ConfigDialog::WMCB(wxCommandEvent& event)
{
	strcpy(g_Config.iWindowedRes, m_WindowResolutionCB->GetValue().mb_str() );
}
void ConfigDialog::RenderMainCheck(wxCommandEvent& event)
{
	g_Config.renderToMainframe = m_RenderToMainWindow->IsChecked();
}
void ConfigDialog::OverlayCheck(wxCommandEvent& event)
{
	g_Config.bOverlayStats = m_Statistics->IsChecked();
}
