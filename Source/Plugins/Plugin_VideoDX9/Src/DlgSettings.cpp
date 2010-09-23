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

#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/filepicker.h>
#include <wx/gbsizer.h>
#include <wx/notebook.h>
#include <wx/mimetype.h>

#include "DlgSettings.h"
#include "FileUtil.h"

#include "D3DBase.h"
#include "D3DUtil.h"

#include "VideoConfig.h"

#include "TextureCache.h"

BEGIN_EVENT_TABLE(GFXConfigDialogDX,wxDialog)

	EVT_CLOSE(GFXConfigDialogDX::OnClose)
	EVT_BUTTON(ID_CLOSE, GFXConfigDialogDX::CloseClick)

	//Direct3D Tab
	EVT_CHECKBOX(ID_VSYNC, GFXConfigDialogDX::DirectXSettingsChanged)
	EVT_CHECKBOX(ID_WIDESCREEN_HACK, GFXConfigDialogDX::DirectXSettingsChanged)
	EVT_CHOICE(ID_ASPECT, GFXConfigDialogDX::DirectXSettingsChanged)
	EVT_CHOICE(ID_ANTIALIASMODE, GFXConfigDialogDX::DirectXSettingsChanged)
	EVT_CHOICE(ID_EFBSCALEMODE, GFXConfigDialogDX::DirectXSettingsChanged)
	EVT_CHECKBOX(ID_EFB_ACCESS_ENABLE, GFXConfigDialogDX::DirectXSettingsChanged)
	EVT_CHECKBOX(ID_SAFETEXTURECACHE, GFXConfigDialogDX::DirectXSettingsChanged)
	EVT_CHECKBOX(ID_DLISTCACHING, GFXConfigDialogDX::DirectXSettingsChanged)
	EVT_RADIOBUTTON(ID_RADIO_SAFETEXTURECACHE_SAFE, GFXConfigDialogDX::DirectXSettingsChanged)
	EVT_RADIOBUTTON(ID_RADIO_SAFETEXTURECACHE_NORMAL, GFXConfigDialogDX::DirectXSettingsChanged)
	EVT_RADIOBUTTON(ID_RADIO_SAFETEXTURECACHE_FAST, GFXConfigDialogDX::DirectXSettingsChanged)

	//Enhancements tab
	EVT_CHECKBOX(ID_FORCEFILTERING, GFXConfigDialogDX::EnhancementsSettingsChanged)
	EVT_CHECKBOX(ID_FORCEANISOTROPY, GFXConfigDialogDX::EnhancementsSettingsChanged)
	EVT_CHECKBOX(ID_LOADHIRESTEXTURES, GFXConfigDialogDX::EnhancementsSettingsChanged)
	EVT_CHECKBOX(ID_EFBSCALEDCOPY, GFXConfigDialogDX::EnhancementsSettingsChanged)
	EVT_CHECKBOX(ID_PIXELLIGHTING, GFXConfigDialogDX::EnhancementsSettingsChanged)
	EVT_CHECKBOX(ID_ANAGLYPH, GFXConfigDialogDX::EnhancementsSettingsChanged)
	EVT_SLIDER(ID_ANAGLYPHSEPARATION, GFXConfigDialogDX::EnhancementsSettingsChanged)
	EVT_SLIDER(ID_ANAGLYPHFOCALANGLE, GFXConfigDialogDX::EnhancementsSettingsChanged)
	

	//Advanced Tab
	EVT_CHECKBOX(ID_DISABLEFOG, GFXConfigDialogDX::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_OVERLAYFPS, GFXConfigDialogDX::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_ENABLEEFBCOPY, GFXConfigDialogDX::AdvancedSettingsChanged)
	EVT_RADIOBUTTON(ID_EFBTORAM, GFXConfigDialogDX::AdvancedSettingsChanged)	
	EVT_RADIOBUTTON(ID_EFBTOTEX, GFXConfigDialogDX::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_ENABLEHOTKEY, GFXConfigDialogDX::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_WIREFRAME, GFXConfigDialogDX::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_ENABLEXFB, GFXConfigDialogDX::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_ENABLEREALXFB, GFXConfigDialogDX::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_USENATIVEMIPS, GFXConfigDialogDX::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_TEXDUMP, GFXConfigDialogDX::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_DUMPFRAMES, GFXConfigDialogDX::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_OVERLAYSTATS, GFXConfigDialogDX::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_PROJSTATS, GFXConfigDialogDX::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_SHADERERRORS, GFXConfigDialogDX::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_TEXFMT_OVERLAY, GFXConfigDialogDX::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_TEXFMT_CENTER, GFXConfigDialogDX::AdvancedSettingsChanged)

END_EVENT_TABLE()

GFXConfigDialogDX::GFXConfigDialogDX(wxWindow *parent, wxWindowID id, const wxString &title, const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
}
// Close and unload the window
// ---------------
GFXConfigDialogDX::~GFXConfigDialogDX()
{
	INFO_LOG(CONSOLE, "GFXConfigDialogDX closed");
}

void GFXConfigDialogDX::OnClose(wxCloseEvent& event)
{
	//INFO_LOG(CONSOLE, "OnClose");
	CloseWindow();
}

void GFXConfigDialogDX::CloseClick(wxCommandEvent& WXUNUSED (event))
{
	//INFO_LOG(CONSOLE, "CloseClick");
	CloseWindow();
}

void GFXConfigDialogDX::InitializeGUIValues()
{
	// General Display Settings
	m_AdapterCB->SetSelection(g_Config.iAdapter);
	m_VSync->SetValue(g_Config.bVSync);
	m_WidescreenHack->SetValue(g_Config.bWidescreenHack);
	m_KeepAR->SetSelection(g_Config.iAspectRatio);

	m_MSAAModeCB->SetSelection(g_Config.iMultisampleMode);
	m_EFBScaleMode->SetSelection(g_Config.iEFBScale);
	m_EnableEFBAccess->SetValue(g_Config.bEFBAccessEnable);
	m_SafeTextureCache->SetValue(g_Config.bSafeTextureCache);
	m_DlistCaching->SetValue(g_Config.bDlistCachingEnable);
	if(g_Config.iSafeTextureCache_ColorSamples == 0)
		m_Radio_SafeTextureCache_Safe->SetValue(true);
	else
		if(g_Config.iSafeTextureCache_ColorSamples > 128)
			m_Radio_SafeTextureCache_Normal->SetValue(true);
		else
			m_Radio_SafeTextureCache_Fast->SetValue(true);

	// Enhancements
	if(g_Config.iMaxAnisotropy == 1)
		m_MaxAnisotropy->SetValue(false);
	else
	{
		if(g_Config.iMaxAnisotropy == 8)
		m_MaxAnisotropy->SetValue(true);
	}
	m_ForceFiltering->SetValue(g_Config.bForceFiltering);
	m_HiresTextures->SetValue(g_Config.bHiresTextures);
	m_MSAAModeCB->SetSelection(g_Config.iMultisampleMode);
	m_EFBScaledCopy->SetValue(g_Config.bCopyEFBScaled);
	m_Anaglyph->SetValue(g_Config.bAnaglyphStereo);
	m_PixelLighting->SetValue(g_Config.bEnablePixelLigting);
	m_AnaglyphSeparation->SetValue(g_Config.iAnaglyphStereoSeparation);
	m_AnaglyphFocalAngle->SetValue(g_Config.iAnaglyphFocalAngle);
	//Advance
	m_DisableFog->SetValue(g_Config.bDisableFog);
	m_OverlayFPS->SetValue(g_Config.bShowFPS);

	m_CopyEFB->SetValue(!g_Config.bEFBCopyDisable);
	g_Config.bCopyEFBToTexture ? m_Radio_CopyEFBToGL->SetValue(true) : m_Radio_CopyEFBToRAM->SetValue(true);	
	m_EnableHotkeys->SetValue(g_Config.bOSDHotKey);
	m_WireFrame->SetValue(g_Config.bWireFrame);
	m_EnableXFB->SetValue(g_Config.bUseXFB);
	m_EnableRealXFB->SetValue(g_Config.bUseRealXFB);
	m_UseNativeMips->SetValue(g_Config.bUseNativeMips);

	m_DumpTextures->SetValue(g_Config.bDumpTextures);
	m_DumpFrames->SetValue(g_Config.bDumpFrames);
	m_OverlayStats->SetValue(g_Config.bOverlayStats);
	m_ProjStats->SetValue(g_Config.bOverlayProjStats);
	m_ShaderErrors->SetValue(g_Config.bShowShaderErrors);
	m_TexfmtOverlay->SetValue(g_Config.bTexFmtOverlayEnable);
	m_TexfmtCenter->SetValue(g_Config.bTexFmtOverlayCenter);
	m_TexfmtCenter->Enable(m_TexfmtOverlay->IsChecked());
}

void GFXConfigDialogDX::CreateGUIControls()
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* sMain;
	sMain = new wxBoxSizer( wxVERTICAL );
	
	m_Notebook = new wxNotebook( this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize, 0 );
	m_PageDirect3D = new wxPanel( m_Notebook, ID_DIRERCT3D, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_PageEnhancements = new wxPanel( m_Notebook, ID_PAGEENHANCEMENTS, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_PageAdvanced = new wxPanel( m_Notebook, ID_PAGEADVANCED, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );

	//D3D Tab
	wxStaticBoxSizer* sbBasic;
	sbBasic = new wxStaticBoxSizer( new wxStaticBox( m_PageDirect3D, wxID_ANY, wxT("Basic") ), wxVERTICAL );
	m_AdapterText = new wxStaticText( m_PageDirect3D, wxID_ANY, wxT("Adapter:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_AdapterText->Wrap( -1 );

	wxArrayString arrayStringFor_AdapterCB;
	for (int i = 0; i < D3D::GetNumAdapters(); ++i)
	{
		const D3D::Adapter &adapter = D3D::GetAdapter(i);
		arrayStringFor_AdapterCB.Add(wxString::FromAscii(adapter.ident.Description));
	}
	const D3D::Adapter &adapter = D3D::GetAdapter(g_Config.iAdapter);

	m_AdapterCB = new wxChoice( m_PageDirect3D, ID_ADAPTER, wxDefaultPosition, wxDefaultSize, arrayStringFor_AdapterCB, 0);
	m_VSync = new wxCheckBox( m_PageDirect3D, ID_VSYNC, wxT("V-sync"), wxPoint( -1,-1 ), wxDefaultSize, 0 );
	m_WidescreenHack = new wxCheckBox( m_PageDirect3D, ID_WIDESCREEN_HACK, wxT("Widescreen hack"), wxPoint( -1,-1 ), wxDefaultSize, 0 );

	m_staticARText = new wxStaticText( m_PageDirect3D, wxID_ANY, wxT("Aspect ratio:"), wxPoint( -1,-1 ), wxDefaultSize, 0 );
	m_staticARText->Wrap( -1 );
	wxString m_KeepARChoices[] = { wxT("Auto"), wxT("Force 16:9 (widescreen)"), wxT("Force 4:3 (standard)"), wxT("Stretch to window") };
	int m_KeepARNChoices = sizeof( m_KeepARChoices ) / sizeof( wxString );
	m_KeepAR = new wxChoice( m_PageDirect3D, ID_ASPECT, wxPoint( -1,-1 ), wxDefaultSize, m_KeepARNChoices, m_KeepARChoices, 0 );
	m_KeepAR->SetSelection( 0 );

	m_staticMSAAText = new wxStaticText( m_PageDirect3D, wxID_ANY, wxT("SSAA mode:"), wxPoint( -1,-1 ), wxDefaultSize, 0 );
	m_staticMSAAText->Wrap( -1 );
	wxArrayString arrayStringFor_MSAAModeCB;
	for (int i = 0; i < (int)adapter.aa_levels.size(); i++)
	{
		arrayStringFor_MSAAModeCB.Add(wxString::FromAscii(adapter.aa_levels[i].name));
	}
	m_MSAAModeCB = new wxChoice( m_PageDirect3D, ID_ANTIALIASMODE, wxPoint( -1,-1 ), wxDefaultSize, arrayStringFor_MSAAModeCB, 0);
	m_EFBScaleText = new wxStaticText( m_PageDirect3D, wxID_ANY, wxT("EFB scale:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_EFBScaleText->Wrap( -1 );
	wxString m_EFBScaleModeChoices[] = { wxT("Auto (fractional)"), wxT("Auto (integral)"), wxT("1x"), wxT("2x"), wxT("3x") };
	int m_EFBScaleModeNChoices = sizeof( m_EFBScaleModeChoices ) / sizeof( wxString );
	m_EFBScaleMode = new wxChoice( m_PageDirect3D, ID_EFBSCALEMODE, wxDefaultPosition, wxDefaultSize, m_EFBScaleModeNChoices, m_EFBScaleModeChoices, 0 );

	m_EnableEFBAccess = new wxCheckBox( m_PageDirect3D, ID_EFB_ACCESS_ENABLE, wxT("Enable CPU->EFB access"), wxDefaultPosition, wxDefaultSize, 0 );

	wxStaticBoxSizer* sbSTC;
	sbSTC = new wxStaticBoxSizer( new wxStaticBox( m_PageDirect3D, wxID_ANY, wxT("Safe texture cache") ), wxVERTICAL );
	m_SafeTextureCache = new wxCheckBox( m_PageDirect3D, ID_SAFETEXTURECACHE, wxT("Use safe texture cache"), wxDefaultPosition, wxDefaultSize, 0 );
	m_Radio_SafeTextureCache_Safe = new wxRadioButton( m_PageDirect3D, ID_RADIO_SAFETEXTURECACHE_SAFE, wxT("safe"), wxDefaultPosition, wxDefaultSize, 0 );
	m_Radio_SafeTextureCache_Normal = new wxRadioButton( m_PageDirect3D, ID_RADIO_SAFETEXTURECACHE_NORMAL, wxT("normal"), wxDefaultPosition, wxDefaultSize, 0 );
	m_Radio_SafeTextureCache_Fast = new wxRadioButton( m_PageDirect3D, ID_RADIO_SAFETEXTURECACHE_FAST, wxT("fast"), wxDefaultPosition, wxDefaultSize, 0 );
	m_DlistCaching = new wxCheckBox( m_PageDirect3D, ID_DLISTCACHING, wxT("Use DList Caching"), wxDefaultPosition, wxDefaultSize, 0 );
	// Sizers
	wxGridBagSizer* sBasic;
	wxBoxSizer* sGeneral;

	sGeneral = new wxBoxSizer( wxVERTICAL );
	sBasic = new wxGridBagSizer( 0, 0 );
	sBasic->SetFlexibleDirection( wxBOTH );
	sBasic->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	sBasic->Add( m_AdapterText, wxGBPosition( 0, 0 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sBasic->Add( m_AdapterCB, wxGBPosition( 0, 1 ), wxGBSpan( 1, 2 ), wxALL|wxEXPAND, 5 );
	sBasic->Add( m_VSync, wxGBPosition( 1, 1 ), wxGBSpan( 1, 1 ), wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	sBasic->Add( m_WidescreenHack, wxGBPosition( 1, 2 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sBasic->Add( m_staticARText, wxGBPosition( 2, 0 ), wxGBSpan( 1, 1 ), wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	sBasic->Add( m_KeepAR, wxGBPosition( 2, 1 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sBasic->Add( m_staticMSAAText, wxGBPosition( 3, 0 ), wxGBSpan( 1, 1 ), wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	sBasic->Add( m_MSAAModeCB, wxGBPosition( 3, 1 ), wxGBSpan( 1, 2 ), wxALL, 5 );
	sBasic->Add( m_EFBScaleText, wxGBPosition( 4, 0 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sBasic->Add( m_EFBScaleMode, wxGBPosition( 4, 1 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sBasic->Add( m_EnableEFBAccess, wxGBPosition( 5, 1 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sBasic->Add( m_DlistCaching, wxGBPosition( 6, 1 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sbBasic->Add( sBasic, 0, 0, 5 );
	sGeneral->Add( sbBasic, 0, wxEXPAND|wxALL, 5 );
	
	wxGridBagSizer* sSTC;
	sSTC = new wxGridBagSizer( 0, 0 );
	sSTC->SetFlexibleDirection( wxBOTH );
	sSTC->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	sSTC->Add( m_SafeTextureCache, wxGBPosition( 0, 0 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sSTC->Add( 0, 0, wxGBPosition( 0, 1 ), wxGBSpan( 1, 1 ), wxEXPAND, 5 );
	sSTC->Add( m_Radio_SafeTextureCache_Safe, wxGBPosition( 0, 2 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sSTC->Add( m_Radio_SafeTextureCache_Normal, wxGBPosition( 0, 3 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sSTC->Add( m_Radio_SafeTextureCache_Fast, wxGBPosition( 0, 4 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sbSTC->Add( sSTC, 0, wxEXPAND, 5 );
	sGeneral->Add( sbSTC, 0, wxEXPAND|wxALL, 5 );
	
	m_PageDirect3D->SetSizer( sGeneral );
	m_PageDirect3D->Layout();
	sGeneral->Fit( m_PageDirect3D );
	m_Notebook->AddPage( m_PageDirect3D, wxT("General"), true );

	//Enhancements Tab
	wxStaticBoxSizer* sbTextureFilter;
	sbTextureFilter = new wxStaticBoxSizer( new wxStaticBox( m_PageEnhancements, wxID_ANY, wxT("Texture filtering") ), wxVERTICAL );
	m_ForceFiltering = new wxCheckBox( m_PageEnhancements, ID_FORCEFILTERING, wxT("Force bi/trilinear filtering  (breaks video in several Wii games)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_MaxAnisotropy = new wxCheckBox( m_PageEnhancements, ID_FORCEANISOTROPY, wxT("Enable 16x anisotropic filtering"), wxDefaultPosition, wxDefaultSize, 0 );
	m_HiresTextures = new wxCheckBox( m_PageEnhancements, ID_LOADHIRESTEXTURES, wxT("Enable hires texture loading"), wxDefaultPosition, wxDefaultSize, 0 );

	wxStaticBoxSizer* sbEFBHacks;
	sbEFBHacks = new wxStaticBoxSizer( new wxStaticBox( m_PageEnhancements, wxID_ANY, wxT("EFB hacks") ), wxVERTICAL );
	m_EFBScaledCopy = new wxCheckBox( m_PageEnhancements, ID_EFBSCALEDCOPY, wxT("EFB scaled copy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_Anaglyph = new wxCheckBox( m_PageEnhancements, ID_ANAGLYPH, wxT("Enable Anaglyph Stereo"), wxDefaultPosition, wxDefaultSize, 0 );
	m_PixelLighting = new wxCheckBox( m_PageEnhancements, ID_PIXELLIGHTING, wxT("Enable Pixel Lighting"), wxDefaultPosition, wxDefaultSize, 0 );
	m_AnaglyphSeparation  = new wxSlider( m_PageEnhancements, ID_ANAGLYPHSEPARATION,2000,1,10000, wxDefaultPosition, wxDefaultSize, wxHORIZONTAL,wxDefaultValidator, wxT("Stereo separation") );
	m_AnaglyphFocalAngle = new wxSlider( m_PageEnhancements, ID_ANAGLYPHFOCALANGLE,0,-1000,1000, wxDefaultPosition, wxDefaultSize, wxHORIZONTAL,wxDefaultValidator, wxT("Stereo Focal Angle") );
	m_AnaglyphSeparationText = new wxStaticText( m_PageEnhancements, wxID_ANY, wxT("Stereo Separation:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_AnaglyphFocalAngleText= new wxStaticText( m_PageEnhancements, wxID_ANY, wxT("Focal Angle:"), wxDefaultPosition, wxDefaultSize, 0 );

	// Sizers
	wxBoxSizer* sEnhancements;
	wxGridBagSizer* sTextureFilter;
	sEnhancements = new wxBoxSizer( wxVERTICAL );
	sTextureFilter = new wxGridBagSizer( 0, 0 );
	sTextureFilter->SetFlexibleDirection( wxBOTH );
	sTextureFilter->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	sTextureFilter->Add( m_ForceFiltering, wxGBPosition( 0, 0 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sTextureFilter->Add( m_MaxAnisotropy, wxGBPosition( 1, 0 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sTextureFilter->Add( m_HiresTextures, wxGBPosition( 2, 0 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sbTextureFilter->Add( sTextureFilter, 0, wxEXPAND, 5 );
	sEnhancements->Add( sbTextureFilter, 0, wxEXPAND|wxALL, 5 );
	
	wxGridBagSizer* sEFBHacks;
	sEFBHacks = new wxGridBagSizer( 0, 0 );
	sEFBHacks->SetFlexibleDirection( wxBOTH );
	sEFBHacks->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	sEFBHacks->Add( m_EFBScaledCopy, wxGBPosition( 0, 0 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sbEFBHacks->Add( sEFBHacks, 1, wxEXPAND, 5 );
	sEnhancements->Add( sbEFBHacks, 0, wxEXPAND|wxALL, 5 );
	
	wxStaticBoxSizer* sbImprovements;
	sbImprovements = new wxStaticBoxSizer( new wxStaticBox( m_PageEnhancements, wxID_ANY, wxT("Improvements") ), wxVERTICAL );
	wxGridBagSizer* sImprovements;
	sImprovements = new wxGridBagSizer(  0, 0  );
	sImprovements->SetFlexibleDirection( wxBOTH );
	sImprovements->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	sImprovements->Add( m_Anaglyph, wxGBPosition( 0, 0 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sImprovements->Add( m_AnaglyphSeparationText, wxGBPosition( 1, 0 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sImprovements->Add( m_AnaglyphFocalAngleText, wxGBPosition( 1, 1 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sImprovements->Add( m_AnaglyphSeparation, wxGBPosition( 2, 0 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sImprovements->Add( m_AnaglyphFocalAngle, wxGBPosition( 2, 1 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sImprovements->Add( m_PixelLighting, wxGBPosition( 3, 0 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sbImprovements->Add( sImprovements, 1, wxEXPAND, 5 );
	sEnhancements->Add( sbImprovements, 0, wxEXPAND|wxALL, 5 );

	m_PageEnhancements->SetSizer( sEnhancements );
	m_PageEnhancements->Layout();
	sEnhancements->Fit( m_PageEnhancements );
	m_Notebook->AddPage( m_PageEnhancements, wxT("Enhancements"), false );

	//Advanced Tab
	wxStaticBoxSizer* sbSettings;
	sbSettings = new wxStaticBoxSizer( new wxStaticBox( m_PageAdvanced, wxID_ANY, wxT("Settings") ), wxVERTICAL );
	m_DisableFog = new wxCheckBox( m_PageAdvanced, ID_DISABLEFOG, wxT("Disable fog"), wxDefaultPosition, wxDefaultSize, 0 );
	m_OverlayFPS = new wxCheckBox( m_PageAdvanced, ID_OVERLAYFPS, wxT("Overlay FPS counter"), wxDefaultPosition, wxDefaultSize, 0 );
	m_CopyEFB = new wxCheckBox( m_PageAdvanced, ID_ENABLEEFBCOPY, wxT("Enable EFB copy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_EnableHotkeys = new wxCheckBox( m_PageAdvanced, ID_ENABLEHOTKEY, wxT("Enable hotkey"), wxDefaultPosition, wxDefaultSize, 0 );
	m_Radio_CopyEFBToRAM = new wxRadioButton( m_PageAdvanced, ID_EFBTORAM, wxT("To RAM (accuracy)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_Radio_CopyEFBToGL = new wxRadioButton( m_PageAdvanced, ID_EFBTOTEX, wxT("To texture (performance, resolution)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_WireFrame = new wxCheckBox( m_PageAdvanced, ID_WIREFRAME, wxT("Enable wireframe"), wxDefaultPosition, wxDefaultSize, 0 );
	m_EnableRealXFB = new wxCheckBox( m_PageAdvanced, ID_ENABLEREALXFB, wxT("Enable real XFB"), wxDefaultPosition, wxDefaultSize, 0 );
	m_EnableXFB = new wxCheckBox( m_PageAdvanced, ID_ENABLEXFB, wxT("Enable XFB"), wxDefaultPosition, wxDefaultSize, 0 );
	m_UseNativeMips = new wxCheckBox( m_PageAdvanced, ID_USENATIVEMIPS, wxT("Use native mipmaps"), wxDefaultPosition, wxDefaultSize, 0 );

	wxStaticBoxSizer* sbDataDumping;
	sbDataDumping = new wxStaticBoxSizer( new wxStaticBox( m_PageAdvanced, wxID_ANY, wxT("Data dumping") ), wxVERTICAL );
	m_DumpTextures = new wxCheckBox( m_PageAdvanced, ID_TEXDUMP, wxT("Dump textures"), wxDefaultPosition, wxDefaultSize, 0 );
	m_DumpFrames = new wxCheckBox( m_PageAdvanced, ID_DUMPFRAMES, wxT("Dump frames To User/Dump/Frames"), wxDefaultPosition, wxDefaultSize, 0 );

	wxStaticBoxSizer* sbDebuggingTools;
	sbDebuggingTools = new wxStaticBoxSizer( new wxStaticBox( m_PageAdvanced, wxID_ANY, wxT("Debugging tools") ), wxVERTICAL );
	m_OverlayStats = new wxCheckBox( m_PageAdvanced, ID_OVERLAYSTATS, wxT("Overlay some statistics"), wxDefaultPosition, wxDefaultSize, 0 );
	m_ShaderErrors = new wxCheckBox( m_PageAdvanced, ID_SHADERERRORS, wxT("Show shader compilation errors"), wxDefaultPosition, wxDefaultSize, 0 );	
	m_TexfmtOverlay = new wxCheckBox( m_PageAdvanced, ID_TEXFMT_OVERLAY, wxT("Enable texture format overlay"), wxDefaultPosition, wxDefaultSize, 0 );
	m_TexfmtCenter = new wxCheckBox( m_PageAdvanced, ID_TEXFMT_CENTER, wxT("Centered"), wxDefaultPosition, wxDefaultSize, 0 );
	m_ProjStats = new wxCheckBox( m_PageAdvanced, wxID_ANY, wxT("Overlay projection stats"), wxDefaultPosition, wxDefaultSize, 0 );

	// Sizers
	wxBoxSizer* sAdvanced;
	sAdvanced = new wxBoxSizer( wxVERTICAL );

	wxGridBagSizer* sSettings;
	sSettings = new wxGridBagSizer( 0, 0 );
	sSettings->SetFlexibleDirection( wxBOTH );
	sSettings->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	sSettings->Add( m_DisableFog, wxGBPosition( 0, 0 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sSettings->Add( m_OverlayFPS, wxGBPosition( 0, 1 ), wxGBSpan( 1, 1 ), wxEXPAND|wxLEFT, 20 );
	sSettings->Add( m_CopyEFB, wxGBPosition( 1, 0 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sSettings->Add( m_EnableHotkeys, wxGBPosition( 1, 1 ), wxGBSpan( 1, 1 ), wxEXPAND|wxLEFT, 20 );
	sSettings->Add( m_Radio_CopyEFBToRAM, wxGBPosition( 2, 0 ), wxGBSpan( 1, 1 ), wxALIGN_CENTER_VERTICAL|wxLEFT, 10 );
	sSettings->Add( m_Radio_CopyEFBToGL, wxGBPosition( 3, 0 ), wxGBSpan( 1, 1 ), wxALIGN_CENTER_VERTICAL|wxLEFT|wxTOP, 10 );
	sSettings->Add( m_WireFrame, wxGBPosition( 2, 1 ), wxGBSpan( 1, 1 ), wxEXPAND|wxLEFT, 20 );
	sSettings->Add( m_EnableRealXFB, wxGBPosition( 4, 1 ), wxGBSpan( 1, 1 ), wxEXPAND|wxLEFT, 20 );
	sSettings->Add( m_EnableXFB, wxGBPosition( 4, 0 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sSettings->Add( m_UseNativeMips, wxGBPosition( 5, 0 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sbSettings->Add( sSettings, 0, wxEXPAND, 5 );
	sAdvanced->Add( sbSettings, 0, wxEXPAND|wxALL, 5 );
	
	wxGridBagSizer* sDataDumping;
	sDataDumping = new wxGridBagSizer( 0, 0 );
	sDataDumping->SetFlexibleDirection( wxBOTH );
	sDataDumping->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	sDataDumping->Add( m_DumpTextures, wxGBPosition( 0, 0 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sDataDumping->Add( m_DumpFrames, wxGBPosition( 1, 0 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sbDataDumping->Add( sDataDumping, 0, wxEXPAND, 5 );
	sAdvanced->Add( sbDataDumping, 0, wxEXPAND|wxALL, 5 );
	
	wxGridBagSizer* sDebuggingTools;
	sDebuggingTools = new wxGridBagSizer( 0, 0 );
	sDebuggingTools->SetFlexibleDirection( wxBOTH );
	sDebuggingTools->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	sDebuggingTools->Add( m_OverlayStats, wxGBPosition( 0, 0 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sDebuggingTools->Add( m_ShaderErrors, wxGBPosition( 1, 0 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sDebuggingTools->Add( m_TexfmtOverlay, wxGBPosition( 2, 0 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sDebuggingTools->Add( m_TexfmtCenter, wxGBPosition( 2, 1 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sDebuggingTools->Add( m_ProjStats, wxGBPosition( 0, 1 ), wxGBSpan( 1, 1 ), wxALL, 5 );
	sbDebuggingTools->Add( sDebuggingTools, 0, wxEXPAND, 5 );
	sAdvanced->Add( sbDebuggingTools, 0, wxEXPAND|wxALL, 5 );
	
	m_PageAdvanced->SetSizer( sAdvanced );
	m_PageAdvanced->Layout();
	sAdvanced->Fit( m_PageAdvanced );
	m_Notebook->AddPage( m_PageAdvanced, wxT("Advanced"), false );
	
	sMain->Add( m_Notebook, 1, wxALL|wxEXPAND, 5 );
	
	//Buttons
	wxBoxSizer* sButtons;
	sButtons = new wxBoxSizer( wxVERTICAL );
	m_Close = new wxButton( this, ID_CLOSE, wxT("Close"), wxDefaultPosition, wxDefaultSize, 0 );
	sButtons->Add( m_Close, 0, wxALL|wxEXPAND, 5 );
	sMain->Add( sButtons, 0, wxALIGN_RIGHT, 5 );
	
	this->SetSizer( sMain );
	this->Layout();

	InitializeGUIValues();

	Fit();
	Center();
	UpdateGUI();
}

void GFXConfigDialogDX::DirectXSettingsChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_ADAPTER:
		g_Config.iAdapter = m_AdapterCB->GetSelection();
		break;
	case ID_VSYNC:
		g_Config.bVSync = m_VSync->IsChecked();
		break;
	case ID_WIDESCREEN_HACK:
		g_Config.bWidescreenHack = m_WidescreenHack->IsChecked();
		break;
	case ID_ASPECT:
		g_Config.iAspectRatio = m_KeepAR->GetSelection();
		break;
	case ID_ANTIALIASMODE:
		g_Config.iMultisampleMode = m_MSAAModeCB->GetSelection();
		break;
	case ID_EFBSCALEMODE:
		g_Config.iEFBScale = m_EFBScaleMode->GetSelection();
		break;
	case ID_EFB_ACCESS_ENABLE:
		g_Config.bEFBAccessEnable = m_EnableEFBAccess->IsChecked();
		break;
	case ID_SAFETEXTURECACHE:
		g_Config.bSafeTextureCache = m_SafeTextureCache->IsChecked();
		break;
	case ID_DLISTCACHING:
		g_Config.bDlistCachingEnable = m_DlistCaching->IsChecked();
		break;
	case ID_RADIO_SAFETEXTURECACHE_SAFE:
		g_Config.iSafeTextureCache_ColorSamples = 0;
		break;
	case ID_RADIO_SAFETEXTURECACHE_NORMAL:
		if(g_Config.iSafeTextureCache_ColorSamples < 512)
			g_Config.iSafeTextureCache_ColorSamples = 512;
		break;
	case ID_RADIO_SAFETEXTURECACHE_FAST:
		if(g_Config.iSafeTextureCache_ColorSamples > 128 || g_Config.iSafeTextureCache_ColorSamples == 0)
			g_Config.iSafeTextureCache_ColorSamples = 128;
		break;
	}
	UpdateGUI();
}

void GFXConfigDialogDX::EnhancementsSettingsChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_FORCEFILTERING:
			g_Config.bForceFiltering = m_ForceFiltering->IsChecked();
		break;
	case ID_FORCEANISOTROPY:
		g_Config.iMaxAnisotropy = m_MaxAnisotropy->IsChecked() ? 8 : 1;
		break;
	case ID_LOADHIRESTEXTURES:
			g_Config.bHiresTextures = m_HiresTextures->IsChecked();
		break;
	case ID_EFBSCALEDCOPY:
		g_Config.bCopyEFBScaled = m_EFBScaledCopy->IsChecked();
		break;
	case ID_PIXELLIGHTING:
		g_Config.bEnablePixelLigting = m_PixelLighting->IsChecked();
		break;
	case ID_ANAGLYPH:
		g_Config.bAnaglyphStereo = m_Anaglyph->IsChecked();
		break;
	case ID_ANAGLYPHSEPARATION:
		g_Config.iAnaglyphStereoSeparation = m_AnaglyphSeparation->GetValue();
		break;
	case ID_ANAGLYPHFOCALANGLE:
		g_Config.iAnaglyphFocalAngle = m_AnaglyphFocalAngle->GetValue();
		break;
	}
	UpdateGUI();
}

void GFXConfigDialogDX::AdvancedSettingsChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_DISABLEFOG:
		g_Config.bDisableFog = m_DisableFog->IsChecked();
		break;
	case ID_OVERLAYFPS:
		g_Config.bShowFPS = m_OverlayFPS->IsChecked();
		break;
	case ID_ENABLEEFBCOPY:
		g_Config.bEFBCopyDisable = !m_CopyEFB->IsChecked();
		break;
	case ID_EFBTORAM:
		g_Config.bCopyEFBToTexture = false;
		break;
	case ID_EFBTOTEX:
		g_Config.bCopyEFBToTexture = true;
		break;
	case ID_ENABLEHOTKEY:
		g_Config.bOSDHotKey = m_EnableHotkeys->IsChecked();
		break;
	case ID_WIREFRAME:
		g_Config.bWireFrame = m_WireFrame->IsChecked();
		break;
	case ID_ENABLEXFB:
		g_Config.bUseXFB = m_EnableXFB->IsChecked();
		break;
	case ID_ENABLEREALXFB:
		g_Config.bUseRealXFB = m_EnableRealXFB->IsChecked();
		break;
	case ID_USENATIVEMIPS:
		g_Config.bUseNativeMips = m_UseNativeMips->IsChecked();
		break;
	case ID_TEXDUMP:
		g_Config.bDumpTextures = m_DumpTextures->IsChecked();
		break;
	case ID_DUMPFRAMES:
		g_Config.bDumpFrames = m_DumpFrames->IsChecked();
		break;
	case ID_OVERLAYSTATS:
		g_Config.bOverlayStats = m_OverlayStats->IsChecked();
		break;
	case ID_PROJSTATS:
		g_Config.bOverlayProjStats = m_ProjStats->IsChecked();
		break;
	case ID_SHADERERRORS:
		g_Config.bShowShaderErrors = m_ShaderErrors->IsChecked();
		break;
	case ID_TEXFMT_OVERLAY:
		g_Config.bTexFmtOverlayEnable = m_TexfmtOverlay->IsChecked();
		break;
	case ID_TEXFMT_CENTER:
		g_Config.bTexFmtOverlayCenter = m_TexfmtCenter->IsChecked();
		break;
	}
	UpdateGUI();
}

void GFXConfigDialogDX::CloseWindow()
{
	// Save the config to INI
	g_Config.Save((std::string(File::GetUserPath(D_CONFIG_IDX)) + "gfx_dx9.ini").c_str());
	EndModal(1);
}

void GFXConfigDialogDX::UpdateGUI()
{
	if (g_Config.bUseRealXFB)
	{
		// must use XFB to use real XFB
		g_Config.bUseXFB = true;
		m_EnableXFB->SetValue(true);
	}
	m_EnableXFB->Enable(!g_Config.bUseRealXFB);
	m_TexfmtCenter->Enable(g_Config.bTexFmtOverlayEnable);

	// Disable the Copy to options when EFBCopy is disabled
	m_Radio_CopyEFBToRAM->Enable(!g_Config.bEFBCopyDisable);
	m_Radio_CopyEFBToGL->Enable(!g_Config.bEFBCopyDisable);

	// Disable/Enable Safe Texture Cache options
	m_Radio_SafeTextureCache_Safe->Enable(g_Config.bSafeTextureCache);
	m_Radio_SafeTextureCache_Normal->Enable(g_Config.bSafeTextureCache);
	m_Radio_SafeTextureCache_Fast->Enable(g_Config.bSafeTextureCache);
}
