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
#include "../Config.h"

#include "../TextureMngr.h"

BEGIN_EVENT_TABLE(ConfigDialog,wxDialog)
	EVT_CLOSE(ConfigDialog::OnClose)
	EVT_BUTTON(ID_CANCEL,ConfigDialog::OKClick)
	EVT_BUTTON(ID_OK,ConfigDialog::OKClick)
	EVT_BUTTON(ID_GRAPHIC_ABOUT,ConfigDialog::DllAbout)
	EVT_CHECKBOX(ID_FULLSCREEN,ConfigDialog::FullScreenCheck)
	EVT_CHECKBOX(ID_RENDERTOMAINWINDOW,ConfigDialog::RenderMainCheck)
	EVT_COMBOBOX(ID_FULLSCREENCB,ConfigDialog::FSCB)
	EVT_COMBOBOX(ID_WINDOWRESOLUTIONCB,ConfigDialog::WMCB)
	EVT_COMBOBOX(ID_ALIASMODECB,ConfigDialog::AACB)
	EVT_CHECKBOX(ID_FORCEFILTERING,ConfigDialog::ForceFilteringCheck)
	EVT_CHECKBOX(ID_FORCEANISOTROPY,ConfigDialog::ForceAnisotropyCheck)
	EVT_CHECKBOX(ID_STRETCHTOFIT,ConfigDialog::StretchToFitCheck)
	EVT_CHECKBOX(ID_KEEPAR,ConfigDialog::KeepARCheck)
	EVT_CHECKBOX(ID_WIREFRAME,ConfigDialog::WireframeCheck)
	EVT_CHECKBOX(ID_SHOWFPS,ConfigDialog::ShowFPSCheck)
	EVT_CHECKBOX(ID_STATISTICS,ConfigDialog::OverlayCheck)
	EVT_CHECKBOX(ID_SHADERERRORS,ConfigDialog::ShowShaderErrorsCheck)
	EVT_CHECKBOX(ID_TEXFMTOVERLAY,ConfigDialog::TexFmtOverlayChange)
	EVT_CHECKBOX(ID_TEXFMTCENTER,ConfigDialog::TexFmtOverlayChange)
	EVT_CHECKBOX(ID_USEXFB,ConfigDialog::UseXFBChange)
	EVT_CHECKBOX(ID_DUMPTEXTURES,ConfigDialog::DumpTexturesChange)
	EVT_CHECKBOX(ID_INVERTDEPTH,ConfigDialog::InvertDepth)
	EVT_CHECKBOX(ID_PROJECTIONHAX1,ConfigDialog::ProjectionHax1)
	EVT_CHECKBOX(ID_PROJECTIONHAX2,ConfigDialog::ProjectionHax2)
	EVT_CHECKBOX(ID_DISABLELIGHTING,ConfigDialog::DisableLighting)
	EVT_CHECKBOX(ID_DISABLETEXTURING,ConfigDialog::DisableTexturing)
	EVT_CHECKBOX(ID_EFBTOTEXTUREDISABLE,ConfigDialog::EFBToTextureDisable)
	EVT_CHECKBOX(ID_DISABLECULLING,ConfigDialog::DisableCulling)
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
	m_Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);

	m_PageGeneral = new wxPanel(m_Notebook, ID_PAGEGENERAL, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_PageGeneral, wxT("General"));

	m_PageEnhancements = new wxPanel(m_Notebook, ID_PAGEENHANCEMENTS, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_PageEnhancements, wxT("Enhancements"));

	m_PageInformation = new wxPanel(m_Notebook, ID_PAGEINFORMATION, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_PageInformation, wxT("Info"));

	m_PageRender = new wxPanel(m_Notebook, ID_PAGERENDER, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_PageRender, wxT("Render"));

	m_PageUtility = new wxPanel(m_Notebook, ID_PAGEUTILITY, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_PageUtility, wxT("Util"));

	m_PageHacks = new wxPanel(m_Notebook, ID_PAGEHACKS, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_PageHacks, wxT("Hacks"));
	
	//buttons
	m_About = new wxButton(this, ID_GRAPHIC_ABOUT, wxT("About"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_OK = new wxButton(this, ID_OK, wxT("OK"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Cancel = new wxButton(this, ID_CANCEL, wxT("Cancel"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);	

	//put notebook and buttons in sizers
	wxBoxSizer* sButtons;
	sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->Add(m_About, 0, wxALL, 5);
	sButtons->Add(0, 0, 1, wxEXPAND, 5);
	sButtons->Add(m_OK, 0, wxALL, 5);
	sButtons->Add(m_Cancel, 0, wxALL, 5);
	
	wxBoxSizer* sMain;
	sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(m_Notebook, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxEXPAND, 5);
	
	this->SetSizer(sMain);
	this->Layout();

	// Page 1 "General"
	m_Fullscreen = new wxCheckBox(m_PageGeneral, ID_FULLSCREEN, wxT("Fullscreen"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Fullscreen->SetValue(g_Config.bFullscreen);

	m_RenderToMainWindow = new wxCheckBox(m_PageGeneral, ID_RENDERTOMAINWINDOW, wxT("Render to main window"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_RenderToMainWindow->SetValue(g_Config.renderToMainframe);

	m_StretchToFit = new wxCheckBox(m_PageGeneral, ID_STRETCHTOFIT, wxT("Stretch to fit (instead of changing res.)"),
	wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_StretchToFit->SetValue(g_Config.bStretchToFit);

	m_KeepAR = new wxCheckBox(m_PageGeneral, ID_KEEPAR, wxT("Keep 4:3 aspect ratio"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_KeepAR->SetValue(g_Config.bKeepAR);

	wxStaticText *FSText = new wxStaticText(m_PageGeneral, ID_FSTEXT, wxT("Fullscreen video mode:"), wxDefaultPosition, wxDefaultSize, 0);
	wxArrayString arrayStringFor_FullscreenCB;
	m_FullscreenCB = new wxComboBox(m_PageGeneral, ID_FULLSCREENCB, wxEmptyString, wxDefaultPosition, wxDefaultSize, arrayStringFor_FullscreenCB, 0, wxDefaultValidator);
	m_FullscreenCB->SetValue(wxString::FromAscii(g_Config.iFSResolution));

	wxStaticText *WMText = new wxStaticText(m_PageGeneral, ID_WMTEXT, wxT("Windowed resolution:"), wxDefaultPosition, wxDefaultSize, 0);
	wxArrayString arrayStringFor_WindowResolutionCB;
	m_WindowResolutionCB = new wxComboBox(m_PageGeneral, ID_WINDOWRESOLUTIONCB, wxEmptyString, wxDefaultPosition, wxDefaultSize, arrayStringFor_WindowResolutionCB, 0, wxDefaultValidator);
	m_WindowResolutionCB->SetValue(wxString::FromAscii(g_Config.iWindowedRes));

	// Page 2 "Enhancements"
	m_ForceFiltering = new wxCheckBox(m_PageEnhancements, ID_FORCEFILTERING, wxT("Force bi/trilinear filtering (May cause small glitches)"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_ForceFiltering->SetValue(g_Config.bForceFiltering);

	//TODO: make the following work ^^
	m_ForceAnisotropy = new wxCheckBox(m_PageEnhancements, ID_FORCEANISOTROPY, wxT("Force maximum anisotropy filtering"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	//m_ForceAnisotropy->SetValue(g_Config.bForceMaxAniso);
	m_ForceAnisotropy->Enable(false);

	wxStaticText *AAText = new wxStaticText(m_PageEnhancements, ID_AATEXT, wxT("Anti-alias mode:"),  wxDefaultPosition, wxDefaultSize, 0);
	wxArrayString arrayStringFor_AliasModeCB;
	m_AliasModeCB = new wxComboBox(m_PageEnhancements, ID_ALIASMODECB, wxEmptyString, wxDefaultPosition, wxDefaultSize, arrayStringFor_AliasModeCB, 0, wxDefaultValidator);
	wxString tmp;
	tmp<<g_Config.iMultisampleMode;
	m_AliasModeCB->SetValue(tmp);

	// Page 3 "Information"
	m_ShowFPS = new wxCheckBox(m_PageInformation, ID_SHOWFPS, wxT("Overlay FPS"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_ShowFPS->SetValue(g_Config.bShowFPS);
	
	m_Statistics = new wxCheckBox(m_PageInformation, ID_STATISTICS, wxT("Overlay some statistics"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Statistics->SetValue(g_Config.bOverlayStats);

	m_ShaderErrors = new wxCheckBox(m_PageInformation, ID_SHADERERRORS, wxT("Show shader compilation issues"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	//m_ShaderErrors->SetValue(g_Config.bShowShaderErrors);
	m_ShaderErrors->Enable(false);

	m_TexFmtOverlay = new wxCheckBox(m_PageInformation, ID_TEXFMTOVERLAY, wxT("Overlay texture format"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_TexFmtOverlay->SetValue(g_Config.bTexFmtOverlayEnable);
	m_TexFmtCenter = new wxCheckBox(m_PageInformation, ID_TEXFMTCENTER, wxT("centered"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_TexFmtCenter->SetValue(g_Config.bTexFmtOverlayCenter);
	m_TexFmtCenter->Enable(m_TexFmtOverlay->IsChecked());

	// Page 4 "Render"
	m_UseXFB = new wxCheckBox(m_PageRender, ID_USEXFB, wxT("Use External Framebuffer (XFB)"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_UseXFB->SetValue(g_Config.bUseXFB);

	m_Wireframe = new wxCheckBox(m_PageRender, ID_WIREFRAME, wxT("Enable Wireframe"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Wireframe->SetValue(g_Config.bWireFrame);
	m_Wireframe->Enable(true);

	m_DisableLighting = new wxCheckBox(m_PageRender, ID_DISABLELIGHTING, wxT("Disable Material Lighting"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_DisableLighting->SetValue(g_Config.bDisableLighting);
	m_DisableLighting->Enable(true);

	m_DisableTexturing = new wxCheckBox(m_PageRender, ID_DISABLETEXTURING, wxT("Disable Texturing"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_DisableTexturing->SetValue(g_Config.bDisableTexturing);
	m_DisableTexturing->Enable(true);

	m_DisableCulling = new wxCheckBox(m_PageRender, ID_DISABLECULLING, wxT("Disable Culling"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_DisableCulling->SetValue(g_Config.bDisableCulling);
	m_DisableCulling->Enable(true);

	// Page 5 "Utility"
	m_DumpTextures = new wxCheckBox(m_PageUtility, ID_DUMPTEXTURES, wxT("Dump textures to:"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_DumpTextures->SetValue(g_Config.bDumpTextures);
	m_TexturePath = new wxDirPickerCtrl(m_PageUtility, ID_TEXTUREPATH, wxEmptyString, wxT("Choose a directory to store texture dumps:"), wxDefaultPosition, wxDefaultSize, wxDIRP_USE_TEXTCTRL);
	m_TexturePath->SetPath(wxString::FromAscii(g_Config.texDumpPath));
	m_TexturePath->Enable(m_DumpTextures->IsChecked());

	// Page 6 "Hacks"
	m_InvertDepth = new wxCheckBox(m_PageHacks, ID_INVERTDEPTH, wxT("Invert Depth"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_InvertDepth->Enable(true);
	m_InvertDepth->SetValue(g_Config.bInvertDepth);

	m_EFBToTextureDisable = new wxCheckBox(m_PageHacks, ID_EFBTOTEXTUREDISABLE, wxT("Disable copy EFB to texture"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_EFBToTextureDisable->Enable(true);
	m_EFBToTextureDisable->SetValue(g_Config.bEBFToTextureDisable);

	m_ProjectionHax1 = new wxCheckBox(m_PageHacks, ID_PROJECTIONHAX1, wxT("Projection before R945"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_ProjectionHax1->Enable(true);
	m_ProjectionHax1->SetValue(g_Config.bProjectionHax1);

	m_ProjectionHax2 = new wxCheckBox(m_PageHacks, ID_PROJECTIONHAX2, wxT("Projection hack of R844"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_ProjectionHax2->Enable(true);
	m_ProjectionHax2->SetValue(g_Config.bProjectionHax2);

	//Put options in sizers within the notebook
	wxGridBagSizer* sPage1;
	sPage1 = new wxGridBagSizer(0, 0);
	sPage1->SetFlexibleDirection(wxBOTH);
	sPage1->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
	sPage1->Add(m_Fullscreen, wxGBPosition(0, 0), wxGBSpan(1, 2), wxALL, 5);
	sPage1->Add(m_RenderToMainWindow, wxGBPosition(1, 0), wxGBSpan(1, 2), wxALL, 5);
	sPage1->Add(m_StretchToFit, wxGBPosition(2, 0), wxGBSpan(1, 2), wxALL, 5);
	sPage1->Add(m_KeepAR, wxGBPosition(3, 0), wxGBSpan(1, 2), wxALL, 5);
	sPage1->Add(FSText, wxGBPosition(4, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sPage1->Add(m_FullscreenCB, wxGBPosition(4, 1), wxGBSpan(1, 1), wxALL, 5);
	sPage1->Add(WMText, wxGBPosition(5, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sPage1->Add(m_WindowResolutionCB, wxGBPosition(5, 1), wxGBSpan(1, 1), wxALL, 5);
	m_PageGeneral->SetSizer(sPage1);
	sPage1->Layout();

    wxGridBagSizer* sPage2;
	sPage2 = new wxGridBagSizer(0, 0);
	sPage2->SetFlexibleDirection(wxBOTH);
	sPage2->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
	sPage2->Add(m_ForceFiltering, wxGBPosition(0, 0), wxGBSpan(1, 2), wxALL, 5);
	sPage2->Add(m_ForceAnisotropy, wxGBPosition(1, 0), wxGBSpan(1, 2), wxALL, 5);
    sPage2->Add(AAText, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sPage2->Add(m_AliasModeCB, wxGBPosition(2, 1), wxGBSpan(1, 2), wxALL, 5);
	m_PageEnhancements->SetSizer(sPage2);
	sPage2->Layout();

	wxGridBagSizer* sPage3;
	sPage3 = new wxGridBagSizer(0, 0);
	sPage3->SetFlexibleDirection(wxBOTH);
	sPage3->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
	sPage3->Add(m_ShowFPS, wxGBPosition(0, 0), wxGBSpan(1, 2), wxALL, 5);
	sPage3->Add(m_ShaderErrors, wxGBPosition(1, 0), wxGBSpan(1, 2), wxALL, 5);
	sPage3->Add(m_Statistics, wxGBPosition(2, 0), wxGBSpan(1, 2), wxALL, 5);
	sPage3->Add(m_TexFmtOverlay, wxGBPosition(3, 0), wxGBSpan(1, 1), wxALL, 5);
	sPage3->Add(m_TexFmtCenter, wxGBPosition(3, 1), wxGBSpan(1, 1), wxALL, 5);
	m_PageInformation->SetSizer(sPage3);
	sPage3->Layout();

	wxGridBagSizer* sPage4;
	sPage4 = new wxGridBagSizer(0, 0);
	sPage4->SetFlexibleDirection(wxBOTH);
	sPage4->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
	sPage4->Add(m_UseXFB, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALL, 5);
	sPage4->Add(m_Wireframe, wxGBPosition(1, 0), wxGBSpan(1, 2), wxALL, 5);
	sPage4->Add(m_DisableLighting, wxGBPosition(2, 0), wxGBSpan(1, 2), wxALL, 5);
	sPage4->Add(m_DisableTexturing, wxGBPosition(3, 0), wxGBSpan(1, 2), wxALL, 5);
	sPage4->Add(m_DisableCulling, wxGBPosition(4, 0), wxGBSpan(1, 2), wxALL, 5);
	m_PageRender->SetSizer(sPage4);
	sPage4->Layout();

	wxGridBagSizer* sPage5;
	sPage5 = new wxGridBagSizer(0, 0);
	sPage5->SetFlexibleDirection(wxBOTH);
	sPage5->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
	sPage5->Add(m_DumpTextures, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sPage5->Add(m_TexturePath, wxGBPosition(0, 1), wxGBSpan(1, 1), wxALL, 5);
	m_PageUtility->SetSizer(sPage5);
	sPage5->Layout();

	wxGridBagSizer* sPage6;
	sPage6 = new wxGridBagSizer(0, 0);
	sPage6->SetFlexibleDirection(wxBOTH);
	sPage6->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
	sPage6->Add(m_InvertDepth, wxGBPosition(0, 0), wxGBSpan(1, 2), wxALL, 5);
	sPage6->Add(m_EFBToTextureDisable, wxGBPosition(1, 0), wxGBSpan(1, 2), wxALL, 5);
	sPage6->Add(m_ProjectionHax1, wxGBPosition(2, 0), wxGBSpan(1, 2), wxALL, 5);
	sPage6->Add(m_ProjectionHax2, wxGBPosition(3, 0), wxGBSpan(1, 2), wxALL, 5);
	m_PageHacks->SetSizer(sPage6);
	sPage6->Layout();

	SetIcon(wxNullIcon);
	Fit();
}

void ConfigDialog::OnClose(wxCloseEvent& event)
{
	EndModal(0);
}

void ConfigDialog::OKClick(wxCommandEvent& event)
{
	switch(event.GetId())
	{
	case ID_CANCEL:
		Close();
		break;
	case ID_OK:
		g_Config.Save();
		Close();
		break;
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

void ConfigDialog::StretchToFitCheck(wxCommandEvent& event) // stretch
{
	g_Config.bStretchToFit = m_StretchToFit->IsChecked();
}

void ConfigDialog::KeepARCheck(wxCommandEvent& event) // keep AR
{
	g_Config.bKeepAR = m_KeepAR->IsChecked();
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

void ConfigDialog::AddAAMode(int mode)
{
	wxString tmp;
	tmp<<mode;
	m_AliasModeCB->Append(tmp);
}

void ConfigDialog::AACB(wxCommandEvent& event)
{
	g_Config.iMultisampleMode = atoi(m_AliasModeCB->GetValue().mb_str());
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

void ConfigDialog::ShowShaderErrorsCheck(wxCommandEvent& event)
{
	g_Config.bShowShaderErrors = m_ShaderErrors->IsChecked();
}

void ConfigDialog::ShowFPSCheck(wxCommandEvent& event)
{
	g_Config.bShowFPS = m_ShowFPS->IsChecked();
}

void ConfigDialog::OverlayCheck(wxCommandEvent& event)
{
	g_Config.bOverlayStats = m_Statistics->IsChecked();
}

void ConfigDialog::TexFmtOverlayChange(wxCommandEvent& event)
{
	switch(event.GetId())
	{
	case ID_TEXFMTOVERLAY:
		g_Config.bTexFmtOverlayEnable = m_TexFmtOverlay->IsChecked();
		m_TexFmtCenter->Enable(m_TexFmtOverlay->IsChecked());
		break;
	case ID_TEXFMTCENTER:
		g_Config.bTexFmtOverlayCenter = m_TexFmtCenter->IsChecked();
		break;
	}
	TextureMngr::Invalidate();
}

void ConfigDialog::UseXFBChange(wxCommandEvent& event)
{
	g_Config.bUseXFB = m_UseXFB->IsChecked();
}

void ConfigDialog::DumpTexturesChange(wxCommandEvent& event)
{	
	m_TexturePath->Enable(m_DumpTextures->IsChecked());
	g_Config.bDumpTextures = m_DumpTextures->IsChecked();
}

void ConfigDialog::TexturePathChange(wxFileDirPickerEvent& event)
{
	//note: if a user inputs an incorrect path(by typing, not by choosing from
	// the combobox, this event wil not be fired.
	strcpy(g_Config.texDumpPath,event.GetPath().mb_str());
}
void ConfigDialog::DllAbout(wxCommandEvent& event) 
{
	wxAboutDialogInfo info;
	info.AddDeveloper(_T("zerofrog(@gmail.com)"));
	info.SetDescription(_T("Vertex/Pixel Shader 2.0 or higher, framebuffer objects, multiple render targets"));
	wxAboutBox(info);
}
void ConfigDialog::InvertDepth(wxCommandEvent& event)
{
	g_Config.bInvertDepth = m_InvertDepth->IsChecked();
}
void ConfigDialog::ProjectionHax1(wxCommandEvent& event)
{	
	g_Config.bProjectionHax1 = m_ProjectionHax1->IsChecked();
}
void ConfigDialog::ProjectionHax2(wxCommandEvent& event)
{
	g_Config.bProjectionHax2 = m_ProjectionHax2->IsChecked();
}

void ConfigDialog::DisableLighting(wxCommandEvent &event)
{
	g_Config.bDisableLighting = m_DisableLighting->IsChecked();
}
void ConfigDialog::DisableTexturing(wxCommandEvent &event)
{
	g_Config.bDisableTexturing = m_DisableTexturing->IsChecked();
}
void ConfigDialog::EFBToTextureDisable(wxCommandEvent &event)
{
	g_Config.bEBFToTextureDisable = m_EFBToTextureDisable->IsChecked();
}
void ConfigDialog::DisableCulling(wxCommandEvent &event)
{
	g_Config.bDisableCulling = m_DisableCulling->IsChecked();
}