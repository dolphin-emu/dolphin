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
	EVT_BUTTON(ID_CLOSE, ConfigDialog::CloseClick)
	EVT_BUTTON(ID_ABOUTOGL, ConfigDialog::AboutClick)
	EVT_CHECKBOX(ID_FULLSCREEN, ConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_RENDERTOMAINWINDOW, ConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(ID_FULLSCREENCB, ConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(ID_WINDOWRESOLUTIONCB, ConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(ID_RENDERBACKEND, ConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(ID_ALIASMODECB, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(ID_MAXANISOTROPY, ConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_FORCEFILTERING, ConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_STRETCHTOFIT, ConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_KEEPAR, ConfigDialog::GeneralSettingsChanged)
	#ifndef _WIN32
		EVT_CHECKBOX(ID_HIDECURSOR, ConfigDialog::GeneralSettingsChanged)
	#endif
	EVT_CHECKBOX(ID_WIREFRAME, ConfigDialog::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_SHOWFPS, ConfigDialog::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_STATISTICS, ConfigDialog::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_BLENDSTATS, ConfigDialog::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_PROJSTATS, ConfigDialog::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_SHADERERRORS, ConfigDialog::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_TEXFMTOVERLAY, ConfigDialog::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_TEXFMTCENTER, ConfigDialog::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_USEXFB, ConfigDialog::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_DUMPTEXTURES, ConfigDialog::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_DISABLELIGHTING, ConfigDialog::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_DISABLETEXTURING, ConfigDialog::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_EFBCOPYDISABLEHOTKEY, ConfigDialog::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_PROJECTIONHACK1,ConfigDialog::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_PROJECTIONHACK2,ConfigDialog::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_SAFETEXTURECACHE,ConfigDialog::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_CHECKBOX_DISABLECOPYEFB, ConfigDialog::AdvancedSettingsChanged)
	EVT_DIRPICKER_CHANGED(ID_TEXTUREPATH, ConfigDialog::TexturePathChange)
	EVT_RADIOBUTTON(ID_RADIO_COPYEFBTORAM, ConfigDialog::AdvancedSettingsChanged)
	EVT_RADIOBUTTON(ID_RADIO_COPYEFBTOGL, ConfigDialog::AdvancedSettingsChanged)
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
	// Notebook
	m_Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);
	m_PageGeneral = new wxPanel(m_Notebook, ID_PAGEGENERAL, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_PageGeneral, wxT("General"));
	m_PageAdvanced = new wxPanel(m_Notebook, ID_PAGEADVANCED, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_PageAdvanced, wxT("Advanced"));

	// Buttons
	m_About = new wxButton(this, ID_ABOUTOGL, wxT("About"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Close = new wxButton(this, ID_CLOSE, wxT("Close"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	// Put notebook and buttons in sizers
	wxBoxSizer* sButtons;
	sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->Add(m_About, 0, wxALL, 5);
	sButtons->AddStretchSpacer();
	sButtons->Add(m_Close, 0, wxALL, 5);

	wxBoxSizer* sMain;
	sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(m_Notebook, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxEXPAND, 5);

	this->SetSizer(sMain);
	this->Layout();

	// General
	sbBasic = new wxStaticBoxSizer(wxVERTICAL, m_PageGeneral, wxT("Basic Settings"));
	m_Fullscreen = new wxCheckBox(m_PageGeneral, ID_FULLSCREEN, wxT("Fullscreen"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Fullscreen->SetValue(g_Config.bFullscreen);
	m_RenderToMainWindow = new wxCheckBox(m_PageGeneral, ID_RENDERTOMAINWINDOW, wxT("Render to main window"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_RenderToMainWindow->SetValue(g_Config.renderToMainframe);
	m_StretchToFit = new wxCheckBox(m_PageGeneral, ID_STRETCHTOFIT, wxT("Stretch to fit"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_StretchToFit->SetToolTip
		(wxT("This will use the game's native resolution and stretch it to fill the"
		"\nwindow instead of changing the internal display resolution. It"
		"\nmay result in a slightly blurrier image, but it may also give a higher"
		"\nFPS if you have a slow graphics card."));
	m_StretchToFit->SetValue(g_Config.bStretchToFit);
	m_KeepAR = new wxCheckBox(m_PageGeneral, ID_KEEPAR, wxT("Keep 4:3 aspect ratio"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_KeepAR->SetValue(g_Config.bKeepAR);

	#ifndef _WIN32
		m_HideCursor = new wxCheckBox(m_PageGeneral, ID_HIDECURSOR, wxT("Hide mouse cursor"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
		m_HideCursor->SetValue(g_Config.bHideCursor); 
	#endif

	wxStaticText *FSText = new wxStaticText(m_PageGeneral, ID_FSTEXT, wxT("Fullscreen video mode:"), wxDefaultPosition, wxDefaultSize, 0);
	m_FullscreenCB = new wxComboBox(m_PageGeneral, ID_FULLSCREENCB, wxEmptyString, wxDefaultPosition, wxDefaultSize, arrayStringFor_FullscreenCB, 0, wxDefaultValidator);
	m_FullscreenCB->SetValue(wxString::FromAscii(g_Config.iFSResolution));

	wxStaticText *WMText = new wxStaticText(m_PageGeneral, ID_WMTEXT, wxT("Windowed resolution:"), wxDefaultPosition, wxDefaultSize, 0);
	m_WindowResolutionCB = new wxComboBox(m_PageGeneral, ID_WINDOWRESOLUTIONCB, wxEmptyString, wxDefaultPosition, wxDefaultSize, arrayStringFor_WindowResolutionCB, 0, wxDefaultValidator);
	m_WindowResolutionCB->SetValue(wxString::FromAscii(g_Config.iWindowedRes));

	wxStaticText *BEText = new wxStaticText(m_PageGeneral, ID_BETEXT, wxT("Rendering backend:"), wxDefaultPosition, wxDefaultSize, 0);
	m_RenderBackend = new wxComboBox(m_PageGeneral, ID_RENDERBACKEND, wxEmptyString, wxDefaultPosition, wxDefaultSize, arrayStringFor_RenderBackend, 0, wxDefaultValidator);
	m_RenderBackend->SetValue(wxString::FromAscii(g_Config.iBackend));

	
	// Enhancements
	sbEnhancements = new wxStaticBoxSizer(wxVERTICAL, m_PageGeneral, wxT("Enhancements"));
	m_ForceFiltering = new wxCheckBox(m_PageGeneral, ID_FORCEFILTERING, wxT("Force bi/trilinear filtering (May cause small glitches)"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_ForceFiltering->SetValue(g_Config.bForceFiltering);
	wxStaticText *AnisoText = new wxStaticText(m_PageGeneral, ID_WMTEXT, wxT("Anisotropic filter:"), wxDefaultPosition, wxDefaultSize, 0);
	m_MaxAnisotropyCB = new wxChoice(m_PageGeneral, ID_MAXANISOTROPY, wxDefaultPosition, wxDefaultSize, arrayStringFor_MaxAnisotropyCB, 0, wxDefaultValidator);
	m_MaxAnisotropyCB->Append(wxT("1x"));
	m_MaxAnisotropyCB->Append(wxT("2x"));
	m_MaxAnisotropyCB->Append(wxT("4x"));
	m_MaxAnisotropyCB->Append(wxT("8x"));
	m_MaxAnisotropyCB->Append(wxT("16x"));
	m_MaxAnisotropyCB->SetSelection(g_Config.iMaxAnisotropy - 1);

	wxStaticText *AAText = new wxStaticText(m_PageGeneral, ID_AATEXT, wxT("Anti-alias mode:"),  wxDefaultPosition, wxDefaultSize, 0);
	wxArrayString arrayStringFor_AliasModeCB;
	m_AliasModeCB = new wxComboBox(m_PageGeneral, ID_ALIASMODECB, wxEmptyString, wxDefaultPosition, wxDefaultSize, arrayStringFor_AliasModeCB, 0, wxDefaultValidator);
	wxString tmp;
	tmp << g_Config.iMultisampleMode;
	m_AliasModeCB->SetValue(tmp);

	// Usage: The wxGBPosition() must have a column and row
	sGeneral = new wxBoxSizer(wxVERTICAL);
	sBasic = new wxGridBagSizer(0, 0);
	sBasic->Add(m_Fullscreen, wxGBPosition(0, 0), wxGBSpan(1, 2), wxALL, 5);
	sBasic->Add(m_RenderToMainWindow, wxGBPosition(1, 0), wxGBSpan(1, 2), wxALL, 5);
	sBasic->Add(m_StretchToFit, wxGBPosition(2, 0), wxGBSpan(1, 2), wxALL, 5);
	sBasic->Add(m_KeepAR, wxGBPosition(3, 0), wxGBSpan(1, 2), wxALL, 5);
	#ifndef _WIN32
		sBasic->Add(m_HideCursor, wxGBPosition(4, 0), wxGBSpan(1, 2), wxALL, 5);
		sBasic->Add(FSText, wxGBPosition(5, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sBasic->Add(m_FullscreenCB, wxGBPosition(5, 1), wxGBSpan(1, 1), wxALL, 5);
		sBasic->Add(WMText, wxGBPosition(6, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sBasic->Add(m_WindowResolutionCB, wxGBPosition(6, 1), wxGBSpan(1, 1), wxALL, 5);
		sBasic->Add(BEText, wxGBPosition(7, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sBasic->Add(m_RenderBackend, wxGBPosition(7, 1), wxGBSpan(1, 1), wxALL, 5);
	#else
		sBasic->Add(FSText, wxGBPosition(4, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sBasic->Add(m_FullscreenCB, wxGBPosition(4, 1), wxGBSpan(1, 1), wxALL, 5);
		sBasic->Add(WMText, wxGBPosition(5, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sBasic->Add(m_WindowResolutionCB, wxGBPosition(5, 1), wxGBSpan(1, 1), wxALL, 5);
		sBasic->Add(BEText, wxGBPosition(6, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sBasic->Add(m_RenderBackend, wxGBPosition(6, 1), wxGBSpan(1, 1), wxALL, 5);
	#endif
	sbBasic->Add(sBasic);
	sGeneral->Add(sbBasic, 0, wxEXPAND|wxALL, 5);

	sEnhancements = new wxGridBagSizer(0, 0);
	sEnhancements->Add(m_ForceFiltering, wxGBPosition(0, 0), wxGBSpan(1, 2), wxALL, 5);
	sEnhancements->Add(AnisoText, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sEnhancements->Add(m_MaxAnisotropyCB, wxGBPosition(1, 1), wxGBSpan(1, 2), wxALL, 5);
    sEnhancements->Add(AAText, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sEnhancements->Add(m_AliasModeCB, wxGBPosition(2, 1), wxGBSpan(1, 2), wxALL, 5);
	sbEnhancements->Add(sEnhancements);
	sGeneral->Add(sbEnhancements, 0, wxEXPAND|wxALL, 5);
	m_PageGeneral->SetSizer(sGeneral);
	sGeneral->Layout();

	// Information
	sbInfo = new wxStaticBoxSizer(wxVERTICAL, m_PageAdvanced, wxT("Info"));
	m_ShowFPS = new wxCheckBox(m_PageAdvanced, ID_SHOWFPS, wxT("Overlay FPS"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_ShowFPS->SetValue(g_Config.bShowFPS);
	m_Statistics = new wxCheckBox(m_PageAdvanced, ID_STATISTICS, wxT("Overlay some statistics"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Statistics->SetValue(g_Config.bOverlayStats);
	m_BlendStats = new wxCheckBox(m_PageAdvanced, ID_BLENDSTATS, wxT("Overlay Blend Stats"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_BlendStats->SetValue(g_Config.bOverlayBlendStats);
	m_ProjStats = new wxCheckBox(m_PageAdvanced, ID_PROJSTATS, wxT("Overlay Projection Stats"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_ProjStats->SetValue(g_Config.bOverlayProjStats);
	m_ShaderErrors = new wxCheckBox(m_PageAdvanced, ID_SHADERERRORS, wxT("Show shader compilation issues"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	//m_ShaderErrors->SetValue(g_Config.bShowShaderErrors);
	m_ShaderErrors->Enable(false);
	m_TexFmtOverlay = new wxCheckBox(m_PageAdvanced, ID_TEXFMTOVERLAY, wxT("Overlay texture format"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_TexFmtOverlay->SetValue(g_Config.bTexFmtOverlayEnable);
	m_TexFmtCenter = new wxCheckBox(m_PageAdvanced, ID_TEXFMTCENTER, wxT("centered"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_TexFmtCenter->SetValue(g_Config.bTexFmtOverlayCenter);
	m_TexFmtCenter->Enable(m_TexFmtOverlay->IsChecked());

	// Render
	sbRendering = new wxStaticBoxSizer(wxVERTICAL, m_PageAdvanced, wxT("Rendering"));
	m_UseXFB = new wxCheckBox(m_PageAdvanced, ID_USEXFB, wxT("Use External Framebuffer (XFB)"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_UseXFB->SetValue(g_Config.bUseXFB);
	m_Wireframe = new wxCheckBox(m_PageAdvanced, ID_WIREFRAME, wxT("Enable Wireframe"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Wireframe->SetValue(g_Config.bWireFrame);
	m_Wireframe->Enable(true);
	m_DisableLighting = new wxCheckBox(m_PageAdvanced, ID_DISABLELIGHTING, wxT("Disable Material Lighting"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_DisableLighting->SetValue(g_Config.bDisableLighting);
	m_DisableLighting->Enable(true);
	m_DisableTexturing = new wxCheckBox(m_PageAdvanced, ID_DISABLETEXTURING, wxT("Disable Texturing"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_DisableTexturing->SetValue(g_Config.bDisableTexturing);
	m_DisableTexturing->Enable(true);

	m_StaticBox_EFB = new wxStaticBox(m_PageAdvanced, ID_STATICBOX_EFB, wxT("EFB Copy"));
	m_CheckBox_DisableCopyEFB = new wxCheckBox(m_PageAdvanced, ID_CHECKBOX_DISABLECOPYEFB, wxT("Disable"));
	m_CheckBox_DisableCopyEFB->SetValue(g_Config.bEFBCopyDisable);
	m_CheckBox_DisableCopyEFB->SetToolTip(wxT("This may lead to a higher FPS in for example Zelda - TP."
		" But it may also cause graphical errors and missing graphics."));

	m_Radio_CopyEFBToRAM = new wxRadioButton(m_PageAdvanced, ID_RADIO_COPYEFBTORAM, wxT("Copy EFB to system RAM (real)"));
	m_Radio_CopyEFBToRAM->SetToolTip(wxT("[This option will apply immediately and does not require a restart to take effect.]"));
	m_Radio_CopyEFBToGL = new wxRadioButton(m_PageAdvanced, ID_RADIO_COPYEFBTOGL, wxT("Copy EFB to GL texture (hack)"));
	m_Radio_CopyEFBToGL->SetToolTip(wxT("[This option will apply immediately and does not require a restart to take effect.]"));
	g_Config.bCopyEFBToRAM ? m_Radio_CopyEFBToRAM->SetValue(true) : m_Radio_CopyEFBToGL->SetValue(true);
	
	m_EFBCopyDisableHotKey = new wxCheckBox(m_PageAdvanced, ID_EFBCOPYDISABLEHOTKEY, wxT("With hotkey E"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_EFBCopyDisableHotKey->SetToolTip(wxT("Use the E key to turn this option on and off"));
	#ifndef _WIN32
	// JPeterson set the hot key to be Win32-specific
	m_EFBCopyDisableHotKey->Enable(false);
	#endif
	m_EFBCopyDisableHotKey->SetValue(g_Config.bEFBCopyDisableHotKey);

	// Utility
	sbUtilities = new wxStaticBoxSizer(wxVERTICAL, m_PageAdvanced, wxT("Utilities"));
	m_DumpTextures = new wxCheckBox(m_PageAdvanced, ID_DUMPTEXTURES, wxT("Dump textures to:"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_DumpTextures->SetValue(g_Config.bDumpTextures);
	m_TexturePath = new wxDirPickerCtrl(m_PageAdvanced, ID_TEXTUREPATH, wxEmptyString, wxT("Choose a directory to store texture dumps:"), wxDefaultPosition, wxDefaultSize, wxDIRP_USE_TEXTCTRL);
	m_TexturePath->SetPath(wxString::FromAscii(g_Config.texDumpPath));
	m_TexturePath->Enable(m_DumpTextures->IsChecked());

	// Hacks
	sbHacks = new wxStaticBoxSizer(wxVERTICAL, m_PageAdvanced, wxT("Hacks"));

	m_SafeTextureCache = new wxCheckBox(m_PageAdvanced, ID_SAFETEXTURECACHE, wxT("Use Safe texture cache"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_SafeTextureCache->SetToolTip(wxT("This is useful to prevent Metroid Prime from crashing, but can cause problems in other games."
		" [This option will apply immediately and does not require a restart. However it may not"
		" be entirely safe to change it midgames.]"));
	m_SafeTextureCache->Enable(true);
	m_SafeTextureCache->SetValue(g_Config.bSafeTextureCache);

	m_ProjectionHax1 = new wxCheckBox(m_PageAdvanced, ID_PROJECTIONHACK1, wxT("Projection before R945"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_ProjectionHax1->SetToolTip(wxT("This may reveal otherwise invisible graphics"
		" in\ngames like Mario Galaxy or Ikaruga."));
	m_ProjectionHax1->Enable(true);
	m_ProjectionHax1->SetValue(g_Config.bProjectionHax1);

	m_ProjectionHax2 = new wxCheckBox(m_PageAdvanced, ID_PROJECTIONHACK2, wxT("Projection hack of R844"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_ProjectionHax2->Enable(true);
	m_ProjectionHax2->SetValue(g_Config.bProjectionHax2);

	sAdvanced = new wxBoxSizer(wxVERTICAL);
	sInfo = new wxGridBagSizer(0, 0);
	sInfo->Add(m_ShowFPS, wxGBPosition(0, 0), wxGBSpan(1, 2), wxALL, 5);
	sInfo->Add(m_ShaderErrors, wxGBPosition(1, 0), wxGBSpan(1, 2), wxALL, 5);
	sInfo->Add(m_Statistics, wxGBPosition(2, 0), wxGBSpan(1, 2), wxALL, 5);
	sInfo->Add(m_BlendStats, wxGBPosition(3, 0), wxGBSpan(1, 2), wxALL, 5);
	sInfo->Add(m_ProjStats, wxGBPosition(4, 0), wxGBSpan(1, 2), wxALL, 5);
	sInfo->Add(m_TexFmtOverlay, wxGBPosition(5, 0), wxGBSpan(1, 1), wxALL, 5);
	sInfo->Add(m_TexFmtCenter, wxGBPosition(5, 1), wxGBSpan(1, 1), wxALL, 5);
	sbInfo->Add(sInfo);
	sAdvanced->Add(sbInfo, 0, wxEXPAND|wxALL, 5);

	wxBoxSizer *sRenderBoxRow1 = new wxBoxSizer(wxHORIZONTAL);
	sRendering = new wxGridBagSizer(0, 0);
	sRendering->Add(m_UseXFB, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALL, 5);
	sRendering->Add(m_Wireframe, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALL, 5);
	sRendering->Add(m_DisableLighting, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALL, 5);
	sRendering->Add(m_DisableTexturing, wxGBPosition(3, 0), wxGBSpan(1, 1), wxALL, 5);
	sRenderBoxRow1->Add(sRendering, 0, wxALL|wxEXPAND, 5);
				wxStaticBoxSizer *sSBox = new wxStaticBoxSizer(m_StaticBox_EFB, wxVERTICAL);
					wxBoxSizer *sStrip1 = new wxBoxSizer(wxHORIZONTAL);
					sStrip1->Add(m_CheckBox_DisableCopyEFB, 0, wxALL|wxEXPAND, 5);
					sStrip1->Add(m_EFBCopyDisableHotKey, 0, wxALL|wxEXPAND, 5);
				sSBox->Add(sStrip1, 0, wxALL|wxEXPAND, 0);
				sSBox->Add(m_Radio_CopyEFBToRAM, 0, wxALL|wxEXPAND, 5);
				sSBox->Add(m_Radio_CopyEFBToGL, 0, wxALL|wxEXPAND, 5);
	sRenderBoxRow1->Add(sSBox, 0, wxALL|wxEXPAND, 5);
	sbRendering->Add(sRenderBoxRow1);
	sAdvanced->Add(sbRendering, 0, wxEXPAND|wxALL, 5);

	sUtilities = new wxBoxSizer(wxHORIZONTAL);
	sUtilities->Add(m_DumpTextures, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sUtilities->Add(m_TexturePath, 1, wxALL|wxEXPAND, 5);
	sbUtilities->Add(sUtilities, 1, wxEXPAND);
	sAdvanced->Add(sbUtilities, 1, wxEXPAND|wxALL, 5);

	sHacks = new wxGridBagSizer(0, 0);
	sHacks->Add(m_ProjectionHax1, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALL, 5);
	sHacks->Add(m_ProjectionHax2, wxGBPosition(1, 0), wxGBSpan(1, 2), wxALL, 5);
	sHacks->Add(m_SafeTextureCache, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALL, 5);
	sbHacks->Add(sHacks);
	sAdvanced->Add(sbHacks, 0, wxEXPAND|wxALL, 5);
	m_PageAdvanced->SetSizer(sAdvanced);
	sAdvanced->Layout();

	Fit();
	Center();
}

void ConfigDialog::OnClose(wxCloseEvent& WXUNUSED (event))
{
	/* notice that we don't run wxEntryCleanup(); here so the dll will 
	still be loaded */
	g_Config.Save();
	EndModal(0);
}

void ConfigDialog::CloseClick(wxCommandEvent& WXUNUSED (event))
{
	Close();
}

void ConfigDialog::AddFSReso(char *reso)
{
	m_FullscreenCB->Append(wxString::FromAscii(reso));
}

void ConfigDialog::AddWindowReso(char *reso)
{
	m_WindowResolutionCB->Append(wxString::FromAscii(reso));
}

void ConfigDialog::AddRenderBackend(const char *backend)
{
	m_RenderBackend->Append(wxString::FromAscii(backend));
}


void ConfigDialog::AddAAMode(int mode)
{
	wxString tmp;
	tmp<<mode;
	m_AliasModeCB->Append(tmp);
}

void ConfigDialog::AboutClick(wxCommandEvent& WXUNUSED (event))
{
	wxMessageBox(_T("Dolphin OpenGL Plugin\nBy zerofrog(@gmail.com)\n\n"
		"A card supporting Vertex/Pixel Shader 2.0 or higher, framebuffer objects, "
		"and multiple render targets is required in order to use this plugin."),
		_T("Dolphin OGL"), wxOK, this);
}

void ConfigDialog::GeneralSettingsChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_FULLSCREEN:
		g_Config.bFullscreen = m_Fullscreen->IsChecked();
		break;
	case ID_RENDERTOMAINWINDOW:
		g_Config.renderToMainframe = m_RenderToMainWindow->IsChecked();
		break;
	case ID_STRETCHTOFIT:
		g_Config.bStretchToFit = m_StretchToFit->IsChecked();
		break;
	case ID_KEEPAR:		
		g_Config.bKeepAR = m_KeepAR->IsChecked();
		break;
	#ifndef _WIN32
		case ID_HIDECURSOR:
			g_Config.bHideCursor = m_HideCursor->IsChecked();
			break; 
	#endif
	case ID_FULLSCREENCB:
		strcpy(g_Config.iFSResolution, m_FullscreenCB->GetValue().mb_str() );
		break;
	case ID_WINDOWRESOLUTIONCB:
		strcpy(g_Config.iWindowedRes, m_WindowResolutionCB->GetValue().mb_str() );
		break;
	case ID_RENDERBACKEND:
	  	strcpy(g_Config.iBackend, m_RenderBackend->GetValue().mb_str());
		break;
	case ID_FORCEFILTERING:
		g_Config.bForceFiltering = m_ForceFiltering->IsChecked();
		break;
	case ID_MAXANISOTROPY:
		g_Config.iMaxAnisotropy = m_MaxAnisotropyCB->GetSelection() + 1;
		break;
	case ID_ALIASMODECB:
		g_Config.iMultisampleMode = atoi(m_AliasModeCB->GetValue().mb_str());
		break;
	}
}

void ConfigDialog::AdvancedSettingsChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_SHOWFPS:
		g_Config.bShowFPS = m_ShowFPS->IsChecked();
		break;
	case ID_SHADERERRORS:
		g_Config.bShowShaderErrors = m_ShaderErrors->IsChecked();
		break;
	case ID_STATISTICS:
		g_Config.bOverlayStats = m_Statistics->IsChecked();
		break;
	case ID_TEXFMTOVERLAY:
		g_Config.bTexFmtOverlayEnable = m_TexFmtOverlay->IsChecked();
		m_TexFmtCenter->Enable(m_TexFmtOverlay->IsChecked());
		TextureMngr::Invalidate();
		break;
	case ID_TEXFMTCENTER:
		g_Config.bTexFmtOverlayCenter = m_TexFmtCenter->IsChecked();
		TextureMngr::Invalidate();
		break;
	case ID_USEXFB:
		g_Config.bUseXFB = m_UseXFB->IsChecked();
		break;
	case ID_WIREFRAME:
		g_Config.bWireFrame = m_Wireframe->IsChecked();
		break;
	case ID_DISABLELIGHTING:
		g_Config.bDisableLighting = m_DisableLighting->IsChecked();
		break;
	case ID_DISABLETEXTURING:
		g_Config.bDisableTexturing = m_DisableTexturing->IsChecked();
		break;
	case ID_DUMPTEXTURES:
		m_TexturePath->Enable(m_DumpTextures->IsChecked());
		g_Config.bDumpTextures = m_DumpTextures->IsChecked();
		break;
	case ID_TEXTUREPATH:
		break;
	case ID_CHECKBOX_DISABLECOPYEFB:
		g_Config.bEFBCopyDisable = m_CheckBox_DisableCopyEFB->IsChecked();
		break;
	case ID_EFBCOPYDISABLEHOTKEY:
		g_Config.bEFBCopyDisableHotKey = m_EFBCopyDisableHotKey->IsChecked();
		break;
	case ID_PROJECTIONHACK1:
		g_Config.bProjectionHax1 = m_ProjectionHax1->IsChecked();
		break;
	case ID_PROJECTIONHACK2:
		g_Config.bProjectionHax2 = m_ProjectionHax2->IsChecked();
		break;
	case ID_SAFETEXTURECACHE:
		g_Config.bSafeTextureCache = m_SafeTextureCache->IsChecked();
		break;
	case ID_RADIO_COPYEFBTORAM:
		TextureMngr::ClearRenderTargets();
		g_Config.bCopyEFBToRAM = true;
		break;
	case ID_RADIO_COPYEFBTOGL:
		g_Config.bCopyEFBToRAM = false;
		break;
	case ID_BLENDSTATS:
		g_Config.bOverlayBlendStats = m_BlendStats->IsChecked();
		break;
	case ID_PROJSTATS:
		g_Config.bOverlayProjStats = m_ProjStats->IsChecked();
		break;

	default:
		break;
	}
}

void ConfigDialog::TexturePathChange(wxFileDirPickerEvent& event)
{
	// Note: if a user inputs an incorrect path(by typing, not by choosing from
	// the combobox) this event wil not be fired.
	strcpy(g_Config.texDumpPath, event.GetPath().mb_str());
}
