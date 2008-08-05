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
	//notebook
	m_Notebook = new wxNotebook(this, ID_PAGEENHANCEMENTS, wxDefaultPosition, wxDefaultSize);
	m_PageVideo = new wxPanel(m_Notebook, ID_PAGEVIDEO, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_PageVideo, wxT("Video"));

	m_PageEnhancements = new wxPanel(m_Notebook, ID_WXNOTEBOOKPAGE3, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_PageEnhancements, wxT("Enhancements"));

	m_PageAdvanced = new wxPanel(m_Notebook, ID_PAGEADVANCED, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_PageAdvanced, wxT("Advanced"));
	
	//buttons
	m_Close = new wxButton(this, ID_CLOSE, wxT("Close"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Apply = new wxButton(this, ID_APPLY, wxT("Apply"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_OK = new wxButton(this, ID_OK, wxT("OK"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);	
	
	//put notebook and buttons in sizers
	wxBoxSizer* sButtons;
	sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->Add(0, 0, 1, wxEXPAND, 5);
	sButtons->Add(m_Close, 0, wxALL, 5);
	sButtons->Add(m_Apply, 0, wxALL, 5);
	sButtons->Add(m_OK, 0, wxALL, 5);
	
	wxBoxSizer* sMain;
	sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(m_Notebook, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxEXPAND, 5);
	
	this->SetSizer(sMain);
	this->Layout();

	//page1
	m_Fullscreen = new wxCheckBox(m_PageVideo, ID_FULLSCREEN, wxT("Fullscreen"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Fullscreen->SetValue(g_Config.bFullscreen);

	m_RenderToMainWindow = new wxCheckBox(m_PageVideo, ID_RENDERTOMAINWINDOW, wxT("Render to main window"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_RenderToMainWindow->SetValue(g_Config.renderToMainframe);

	wxStaticText *FSText = new wxStaticText(m_PageVideo, ID_FSTEXT, wxT("Fullscreen video mode:"), wxDefaultPosition, wxDefaultSize, 0);
	wxArrayString arrayStringFor_FullscreenCB;
	m_FullscreenCB = new wxComboBox(m_PageVideo, ID_FULLSCREENCB, wxT(""), wxDefaultPosition, wxDefaultSize, arrayStringFor_FullscreenCB, 0, wxDefaultValidator);

	wxStaticText *WMText = new wxStaticText(m_PageVideo, ID_WMTEXT, wxT("Windowed resolution:"), wxDefaultPosition, wxDefaultSize, 0);
	wxArrayString arrayStringFor_WindowResolutionCB;
	m_WindowResolutionCB = new wxComboBox(m_PageVideo, ID_WINDOWRESOLUTIONCB, wxT(""), wxDefaultPosition, wxDefaultSize, arrayStringFor_WindowResolutionCB, 0, wxDefaultValidator);

	//page2
	m_ForceFiltering = new wxCheckBox(m_PageEnhancements, ID_FORCEFILTERING, wxT("Force bi/trilinear filtering (May cause small glitches)"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_ForceFiltering->SetValue(g_Config.bForceFiltering);

	//page3
	m_Statistics = new wxCheckBox(m_PageAdvanced, ID_STATISTICS, wxT("Overlay some statistics"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Statistics->SetValue(g_Config.bOverlayStats);

	m_DumpTextures = new wxCheckBox(m_PageAdvanced, ID_DUMPTEXTURES, wxT("Dump textures to:"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_DumpTextures->SetValue(g_Config.bDumpTextures);
	m_TexturePath = new wxDirPickerCtrl(m_PageAdvanced, ID_TEXTUREPATH, wxEmptyString, wxT("Choose a directory to store texture dumps:"), wxDefaultPosition, wxDefaultSize, wxDIRP_USE_TEXTCTRL);
	m_TexturePath->SetPath(wxString::FromAscii(g_Config.texDumpPath));
	m_TexturePath->Enable(m_DumpTextures->IsChecked());

	//TODO: make the following work ^^
	wxStaticText *AAText = new wxStaticText(m_PageVideo, ID_AATEXT, wxT("Anti-alias mode:"),  wxDefaultPosition, wxDefaultSize, 0);
	wxArrayString arrayStringFor_AliasModeCB;
	m_AliasModeCB = new wxComboBox(m_PageVideo, ID_ALIASMODECB, wxT(""), wxDefaultPosition, wxDefaultSize, arrayStringFor_AliasModeCB, 0, wxDefaultValidator);
	m_AliasModeCB->Enable(false);

	m_ForceAnisotropy = new wxCheckBox(m_PageEnhancements, ID_FORCEANISOTROPY, wxT("Force maximum ansitropy filtering"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	//m_ForceAnisotropy->SetValue(g_Config.bForceMaxAniso);
	m_ForceAnisotropy->Enable(false);

	m_Wireframe = new wxCheckBox(m_PageAdvanced, ID_WIREFRAME, wxT("Wireframe"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	//m_Wireframe->SetValue(g_Config.bWireFrame);
	m_Wireframe->Enable(false);

	m_ShaderErrors = new wxCheckBox(m_PageAdvanced, ID_SHADERERRORS, wxT("Show shader compilation issues"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	//m_ShaderErrors->SetValue(g_Config.bShowShaderErrors);
	m_ShaderErrors->Enable(false);

	//Put options in sizers in the notebook
	wxGridBagSizer* sPage1;
	sPage1 = new wxGridBagSizer(0, 0);
	sPage1->SetFlexibleDirection(wxBOTH);
	sPage1->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
	sPage1->Add(m_Fullscreen, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALL, 5);
	sPage1->Add(m_RenderToMainWindow, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALL, 5);
	sPage1->Add(FSText, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sPage1->Add(m_FullscreenCB, wxGBPosition(2, 1), wxGBSpan(1, 1), wxALL, 5);
	sPage1->Add(WMText, wxGBPosition(3, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sPage1->Add(m_WindowResolutionCB, wxGBPosition(3, 1), wxGBSpan(1, 1), wxALL, 5);
	sPage1->Add(AAText, wxGBPosition(4, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sPage1->Add(m_AliasModeCB, wxGBPosition(4, 1), wxGBSpan(1, 1), wxALL, 5);
	m_PageVideo->SetSizer(sPage1);
	sPage1->Layout();

	wxBoxSizer* sPage2;
	sPage2 = new wxBoxSizer(wxVERTICAL);
	sPage2->Add(m_ForceFiltering, 0, wxALL, 5);
	sPage2->Add(m_ForceAnisotropy, 0, wxALL, 5);
	m_PageEnhancements->SetSizer(sPage2);
	sPage2->Layout();

	wxBoxSizer* sPage3;
	sPage3 = new wxBoxSizer(wxVERTICAL);
	sPage3->Add(m_Wireframe, 0, wxALL, 5);
	sPage3->Add(m_Statistics, 0, wxALL, 5);
	sPage3->Add(m_ShaderErrors, 0, wxALL, 5);
	sPage3->Add(m_DumpTextures, 0, wxALL, 5);
	sPage3->Add(m_TexturePath, 0, wxALL, 5);
	m_PageAdvanced->SetSizer(sPage3);
	sPage3->Layout();

	SetIcon(wxNullIcon);
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
