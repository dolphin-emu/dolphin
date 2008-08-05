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
	EVT_BUTTON(ID_CLOSE,ConfigDialog::OKClick)
	EVT_BUTTON(ID_APPLY,ConfigDialog::OKClick)
	EVT_BUTTON(ID_OK,ConfigDialog::OKClick)
	EVT_CHECKBOX(ID_FULLSCREEN,ConfigDialog::FullScreenCheck)
	EVT_CHECKBOX(ID_RENDERTOMAINWINDOW,ConfigDialog::RenderMainCheck)
	EVT_COMBOBOX(ID_FULLSCREENCB,ConfigDialog::FSCB)
	EVT_COMBOBOX(ID_WINDOWRESOLUTIONCB,ConfigDialog::WMCB)
	EVT_CHECKBOX(ID_FORCEFILTERING,ConfigDialog::ForceFilteringCheck)
	EVT_CHECKBOX(ID_FORCEANISOTROPY,ConfigDialog::ForceAnisotropyCheck)
	EVT_CHECKBOX(ID_WIREFRAME,ConfigDialog::WireframeCheck)
	EVT_CHECKBOX(ID_STATISTICS,ConfigDialog::OverlayCheck)
	EVT_CHECKBOX(ID_SHADERERRORS,ConfigDialog::ShowShaderErrorsCheck)
	EVT_CHECKBOX(ID_DUMPTEXTURES,ConfigDialog::DumpTexturesChange)
	EVT_DIRPICKER_CHANGED(ID_TEXTUREPATH,ConfigDialog::TexturePathChange)
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

	// page1
	m_Fullscreen = new wxCheckBox(m_PageVideo, ID_FULLSCREEN, wxT("Fullscreen"), wxPoint(12,16), wxSize(137,25), 0, wxDefaultValidator, wxT("Fullscreen"));
	m_Fullscreen->SetValue(g_Config.bFullscreen);

	m_RenderToMainWindow = new wxCheckBox(m_PageVideo, ID_RENDERTOMAINWINDOW, wxT("Render to main window"), wxPoint(12,40), wxSize(137,25), 0, wxDefaultValidator, wxT("RenderToMainWindow"));
	m_RenderToMainWindow->SetValue(g_Config.renderToMainframe);

	wxStaticText *WxStaticText2 = new wxStaticText(m_PageVideo, ID_WXSTATICTEXT2, wxT("Fullscreen video mode:"), wxPoint(12,72), wxDefaultSize, 0, wxT("WxStaticText2"));
	wxArrayString arrayStringFor_FullscreenCB;
	m_FullscreenCB = new wxComboBox(m_PageVideo, ID_FULLSCREENCB, wxT(""), wxPoint(132,72), wxSize(121,21), arrayStringFor_FullscreenCB, 0, wxDefaultValidator, wxT("FullscreenCB"));

	wxStaticText *WxStaticText1 = new wxStaticText(m_PageVideo, ID_WXSTATICTEXT1, wxT("Windowed resolution:"), wxPoint(12,104), wxDefaultSize, 0, wxT("WxStaticText1"));
	wxArrayString arrayStringFor_WindowResolutionCB;
	m_WindowResolutionCB = new wxComboBox(m_PageVideo, ID_WINDOWRESOLUTIONCB, wxT(""), wxPoint(132,104), wxSize(121,21), arrayStringFor_WindowResolutionCB, 0, wxDefaultValidator, wxT("WindowResolutionCB"));

	// page2
	m_ForceFiltering = new wxCheckBox(m_PageEnhancements, ID_FORCEFILTERING, wxT("Force bi/trlinear (May cause small glitches)"), wxPoint(12,16), wxSize(233,25), 0, wxDefaultValidator, wxT("ForceFiltering"));
	m_ForceFiltering->SetValue(g_Config.bForceFiltering);

	// page3
	m_Statistics = new wxCheckBox(m_PageAdvanced, ID_STATISTICS, wxT("Overlay some statistics"), wxPoint(12,40), wxSize(233,25), 0, wxDefaultValidator, wxT("Statistics"));
	m_Statistics->SetValue(g_Config.bOverlayStats);

	m_DumpTextures = new wxCheckBox(m_PageAdvanced, ID_DUMPTEXTURES, wxT("Dump textures to:"), wxPoint(12,88), wxSize(233,25), 0, wxDefaultValidator, wxT("DumpTextures"));
	m_DumpTextures->SetValue(g_Config.bDumpTextures);
	m_TexturePath = new wxDirPickerCtrl(m_PageAdvanced, ID_TEXTUREPATH, wxEmptyString, wxT("Choose a directory to store texture dumps:"), wxPoint(12,120), wxSize(233,25), 0);//wxDIRP_USE_TEXTCTRL);
	m_TexturePath->SetPath(wxString::FromAscii(g_Config.texDumpPath));
	m_TexturePath->Enable(m_DumpTextures->IsChecked());

	// TODO: make the following work ^^
	wxStaticText *WxStaticText3 = new wxStaticText(m_PageVideo, ID_WXSTATICTEXT3, wxT("Anti-alias mode:"), wxPoint(12,136), wxDefaultSize, 0, wxT("AA"));
	wxArrayString arrayStringFor_AliasModeCB;
	m_AliasModeCB = new wxComboBox(m_PageVideo, ID_ALIASMODECB, wxT(""), wxPoint(132,136), wxSize(121,21), arrayStringFor_AliasModeCB, 0, wxDefaultValidator, wxT("AliasModeCB"));
	m_AliasModeCB->Enable(false);

	m_ForceAnisotropy = new wxCheckBox(m_PageEnhancements, ID_FORCEANISOTROPY, wxT("Force maximum ansitropy filtering"), wxPoint(12,48), wxSize(233,25), 0, wxDefaultValidator, wxT("ForceAnisotropy"));
	//m_ForceAnisotropy->SetValue(g_Config.bForceMaxAniso);
	m_ForceAnisotropy->Enable(false);

	m_Wireframe = new wxCheckBox(m_PageAdvanced, ID_WIREFRAME, wxT("Wireframe"), wxPoint(12,16), wxSize(233,25), 0, wxDefaultValidator, wxT("Wireframe"));
	//m_Wireframe->SetValue(g_Config.bWireFrame);
	m_Wireframe->Enable(false);

	m_ShaderErrors = new wxCheckBox(m_PageAdvanced, ID_SHADERERRORS, wxT("Show shader compilation issues"), wxPoint(12,64), wxSize(233,25), 0, wxDefaultValidator, wxT("ShaderErrors"));
	//m_ShaderErrors->SetValue(g_Config.bShowShaderErrors);
	m_ShaderErrors->Enable(false);

	SetIcon(wxNullIcon);
	SetSize(8,8,492,273);
	Center();
}

void ConfigDialog::OnClose(wxCloseEvent& event)
{
	g_Config.Save();
	EndModal(0);

}

void ConfigDialog::OKClick(wxCommandEvent& event)
{
	if ((event.GetId() == ID_APPLY) ||
		(event.GetId() == ID_OK))
	{
		g_Config.Save();
	}

	if ((event.GetId() == ID_CLOSE) ||
		(event.GetId() == ID_OK))
	{
		Close(); 
	}
}

void ConfigDialog::FullScreenCheck(wxCommandEvent& event)
{
	g_Config.bFullscreen = m_Fullscreen->IsChecked();
}

void ConfigDialog::RenderMainCheck(wxCommandEvent& event)
{
	g_Config.renderToMainframe = m_RenderToMainWindow->IsChecked();
}

void ConfigDialog::AddFSReso(char *reso)
{
	m_FullscreenCB->Append(wxString::FromAscii(reso));
}


void ConfigDialog::FSCB(wxCommandEvent& event)
{
	strcpy(g_Config.iFSResolution, m_FullscreenCB->GetValue().mb_str() );
}
void ConfigDialog::AddWindowReso(char *reso)
{
	m_WindowResolutionCB->Append(wxString::FromAscii(reso));
}
void ConfigDialog::WMCB(wxCommandEvent& event)
{
	strcpy(g_Config.iWindowedRes, m_WindowResolutionCB->GetValue().mb_str() );
}

void ConfigDialog::ForceFilteringCheck(wxCommandEvent& event)
{
	g_Config.bForceFiltering = m_ForceFiltering->IsChecked();
}

void ConfigDialog::ForceAnisotropyCheck(wxCommandEvent& event)
{
	g_Config.bForceMaxAniso = m_ForceAnisotropy->IsChecked();
}

void ConfigDialog::WireframeCheck(wxCommandEvent& event)
{
	g_Config.bWireFrame = m_Wireframe->IsChecked();
}
void ConfigDialog::OverlayCheck(wxCommandEvent& event)
{
	g_Config.bOverlayStats = m_Statistics->IsChecked();
}

void ConfigDialog::ShowShaderErrorsCheck(wxCommandEvent& event)
{
	g_Config.bShowShaderErrors = m_ShaderErrors->IsChecked();
}

void ConfigDialog::DumpTexturesChange(wxCommandEvent& event)
{
	m_TexturePath->Enable(m_DumpTextures->IsChecked());
	g_Config.bDumpTextures = m_DumpTextures->IsChecked();
}

void ConfigDialog::TexturePathChange(wxFileDirPickerEvent& event)
{
	//note: if a user inputs an incorrect path, this event wil not be fired.
	strcpy(g_Config.texDumpPath,event.GetPath().mb_str());
}
